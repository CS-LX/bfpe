#include "process.hpp"

#include <windows.h>

#include <iostream>
#include <sstream>

namespace bfpe {

void print_error(const std::string& message) {
    std::cerr << message << std::endl;
}

std::map<std::wstring, std::wstring> current_environment() {
    std::map<std::wstring, std::wstring> env;
    wchar_t* block = GetEnvironmentStringsW();
    if (!block) {
        return env;
    }

    for (const wchar_t* cursor = block; *cursor != L'\0';) {
        const std::wstring entry = cursor;
        const size_t equal = entry.find(L'=');
        if (equal != std::wstring::npos) {
            env.emplace(entry.substr(0, equal), entry.substr(equal + 1));
        }
        cursor += entry.size() + 1;
    }

    FreeEnvironmentStringsW(block);
    return env;
}

std::wstring build_environment_block(const std::map<std::wstring, std::wstring>& env) {
    std::wstring block;
    block.reserve(env.size() * 64);
    for (const auto& [key, value] : env) {
        block += key;
        block += L'=';
        block += value;
        block += L'\0';
    }
    block += L'\0';
    return block;
}

std::wstring quote_argument(const std::wstring& arg) {
    if (arg.empty()) {
        return L"\"\"";
    }
    if (arg.find_first_of(L" \t\"") == std::wstring::npos) {
        return arg;
    }

    std::wstring quoted = L"\"";
    for (size_t i = 0; i < arg.size(); ++i) {
        size_t backslashes = 0;
        while (i < arg.size() && arg[i] == L'\\') {
            ++backslashes;
            ++i;
        }
        if (i == arg.size()) {
            quoted.append(backslashes * 2, L'\\');
            break;
        }
        if (arg[i] == L'"') {
            quoted.append(backslashes * 2 + 1, L'\\');
            quoted += L'"';
        } else {
            quoted.append(backslashes, L'\\');
            quoted += arg[i];
        }
    }
    quoted += L'"';
    return quoted;
}

std::wstring join_command_line(const std::vector<std::wstring>& args) {
    std::wstring line;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) {
            line += L' ';
        }
        line += quote_argument(args[i]);
    }
    return line;
}

int run_process(const std::vector<std::wstring>& args,
                const std::optional<std::map<std::wstring, std::wstring>>& env,
                bool echo) {
    if (args.empty()) {
        print_error("error: empty command");
        return 1;
    }

    if (echo) {
        std::wostringstream display;
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) {
                display << L' ';
            }
            display << quote_argument(args[i]);
        }
        std::wcout << L"> " << display.str() << std::endl;
    }

    std::wstring command_line = join_command_line(args);
    std::vector<wchar_t> mutable_command(command_line.begin(), command_line.end());
    mutable_command.push_back(L'\0');

    STARTUPINFOW startup_info{};
    startup_info.cb = sizeof(startup_info);
    PROCESS_INFORMATION process_info{};

    std::wstring env_block;
    LPVOID environment = nullptr;
    DWORD creation_flags = 0;
    if (env.has_value()) {
        env_block = build_environment_block(*env);
        environment = env_block.data();
        creation_flags = CREATE_UNICODE_ENVIRONMENT;
    }

    const BOOL created = CreateProcessW(
        nullptr,
        mutable_command.data(),
        nullptr,
        nullptr,
        FALSE,
        creation_flags,
        environment,
        nullptr,
        &startup_info,
        &process_info);

    if (!created) {
        print_error("error: CreateProcess failed");
        return 1;
    }

    WaitForSingleObject(process_info.hProcess, INFINITE);
    DWORD exit_code = 1;
    GetExitCodeProcess(process_info.hProcess, &exit_code);
    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);
    return static_cast<int>(exit_code);
}

std::optional<std::filesystem::path> find_executable(const std::wstring& name,
                                                   const std::map<std::wstring, std::wstring>& env) {
    wchar_t buffer[MAX_PATH]{};

    const auto search = [&](const wchar_t* path_list) -> std::optional<std::filesystem::path> {
        const DWORD length = SearchPathW(path_list, name.c_str(), nullptr, MAX_PATH, buffer, nullptr);
        if (length > 0 && length < MAX_PATH) {
            return std::filesystem::path(buffer);
        }
        return std::nullopt;
    };

    if (auto found = search(nullptr)) {
        return found;
    }

    const auto path_it = env.find(L"PATH");
    if (path_it != env.end() && !path_it->second.empty()) {
        if (auto found = search(path_it->second.c_str())) {
            return found;
        }
    }
    return std::nullopt;
}

std::optional<std::filesystem::path> find_python() {
    const std::map<std::wstring, std::wstring> env = current_environment();
    const wchar_t* candidates[] = {L"python.exe", L"python3.exe"};
    for (const wchar_t* candidate : candidates) {
        if (auto found = find_executable(candidate, env)) {
            return found;
        }
    }
    return std::nullopt;
}

}  // namespace bfpe
