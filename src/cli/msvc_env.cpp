#include "msvc_env.hpp"

#include "paths.hpp"
#include "process.hpp"

#include <windows.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

namespace bfpe {

namespace {

std::wstring read_pipe_output(const std::vector<std::wstring>& args) {
    SECURITY_ATTRIBUTES security_attributes{};
    security_attributes.nLength = sizeof(security_attributes);
    security_attributes.bInheritHandle = TRUE;

    HANDLE read_handle = nullptr;
    HANDLE write_handle = nullptr;
    if (!CreatePipe(&read_handle, &write_handle, &security_attributes, 0)) {
        return {};
    }

    SetHandleInformation(read_handle, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW startup_info{};
    startup_info.cb = sizeof(startup_info);
    startup_info.dwFlags = STARTF_USESTDHANDLES;
    startup_info.hStdOutput = write_handle;
    startup_info.hStdError = write_handle;

    PROCESS_INFORMATION process_info{};
    std::wstring command_line = join_command_line(args);
    std::vector<wchar_t> mutable_command(command_line.begin(), command_line.end());
    mutable_command.push_back(L'\0');

    const BOOL created = CreateProcessW(
        nullptr,
        mutable_command.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &startup_info,
        &process_info);

    CloseHandle(write_handle);

    std::string output;
    if (created) {
        char buffer[4096];
        DWORD bytes_read = 0;
        while (ReadFile(read_handle, buffer, sizeof(buffer), &bytes_read, nullptr) && bytes_read > 0) {
            output.append(buffer, buffer + bytes_read);
        }
        WaitForSingleObject(process_info.hProcess, INFINITE);
        CloseHandle(process_info.hThread);
        CloseHandle(process_info.hProcess);
    }

    CloseHandle(read_handle);

    int wide_length = MultiByteToWideChar(CP_UTF8, 0, output.c_str(), static_cast<int>(output.size()), nullptr, 0);
    UINT code_page = CP_UTF8;
    if (wide_length <= 0) {
        wide_length = MultiByteToWideChar(CP_ACP, 0, output.c_str(), static_cast<int>(output.size()), nullptr, 0);
        code_page = CP_ACP;
    }
    if (wide_length <= 0) {
        return {};
    }
    std::wstring wide(static_cast<size_t>(wide_length), L'\0');
    MultiByteToWideChar(code_page, 0, output.c_str(), static_cast<int>(output.size()), wide.data(), wide_length);
    return wide;
}

void merge_env_line(std::map<std::wstring, std::wstring>& env, const std::wstring& line) {
    const size_t equal = line.find(L'=');
    if (equal == std::wstring::npos) {
        return;
    }
    env[line.substr(0, equal)] = line.substr(equal + 1);
}

}  // namespace

std::optional<std::filesystem::path> find_vs_install() {
    wchar_t program_files_x86[MAX_PATH]{};
    DWORD length = GetEnvironmentVariableW(L"ProgramFiles(x86)", program_files_x86, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        wcscpy_s(program_files_x86, MAX_PATH, L"C:\\Program Files (x86)");
    }

    const std::filesystem::path vswhere =
        std::filesystem::path(program_files_x86) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe";
    if (!std::filesystem::exists(vswhere)) {
        return std::nullopt;
    }

    const std::wstring output = read_pipe_output(
        {vswhere.wstring(),
         L"-latest",
         L"-requires",
         L"Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
         L"-property",
         L"installationPath"});

    std::wstring trimmed = output;
    while (!trimmed.empty() && (trimmed.back() == L'\r' || trimmed.back() == L'\n' || trimmed.back() == L' ')) {
        trimmed.pop_back();
    }
    if (trimmed.empty()) {
        return std::nullopt;
    }
    return std::filesystem::path(trimmed);
}

std::optional<std::filesystem::path> find_msvc_bin(const std::filesystem::path& install) {
    const std::filesystem::path tools_root = install / "VC" / "Tools" / "MSVC";
    if (!std::filesystem::exists(tools_root)) {
        return std::nullopt;
    }

    std::vector<std::filesystem::path> versions;
    for (const auto& entry : std::filesystem::directory_iterator(tools_root)) {
        if (entry.is_directory()) {
            versions.push_back(entry.path());
        }
    }
    std::sort(versions.begin(), versions.end(), std::greater<>());

    for (const auto& version_dir : versions) {
        const std::filesystem::path bin_dir = version_dir / "bin" / "Hostx64" / "x64";
        if (std::filesystem::exists(bin_dir / "ml64.exe")) {
            return bin_dir;
        }
    }
    return std::nullopt;
}

std::optional<std::filesystem::path> vcvars64_path() {
    const auto install = find_vs_install();
    if (!install.has_value()) {
        return std::nullopt;
    }
    const std::filesystem::path vcvars = *install / "VC" / "Auxiliary" / "Build" / "vcvars64.bat";
    if (!std::filesystem::exists(vcvars)) {
        return std::nullopt;
    }
    return vcvars;
}

int run_with_vcvars(const std::filesystem::path& vcvars, const std::vector<std::wstring>& args) {
    const std::wstring cmdline =
        L"cmd.exe /c call \"" + vcvars.wstring() + L"\" >nul && " + join_command_line(args);

    std::wcout << L"> " << cmdline << std::endl;

    std::vector<wchar_t> mutable_cmdline(cmdline.begin(), cmdline.end());
    mutable_cmdline.push_back(L'\0');

    STARTUPINFOW startup_info{};
    startup_info.cb = sizeof(startup_info);
    PROCESS_INFORMATION process_info{};

    const BOOL created = CreateProcessW(
        nullptr,
        mutable_cmdline.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &startup_info,
        &process_info);

    if (!created) {
        print_error("error: CreateProcess failed for MSVC command");
        return 1;
    }

    WaitForSingleObject(process_info.hProcess, INFINITE);
    DWORD exit_code = 1;
    GetExitCodeProcess(process_info.hProcess, &exit_code);
    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);
    return static_cast<int>(exit_code);
}

std::optional<std::map<std::wstring, std::wstring>> msvc_environment() {
    const auto vcvars = vcvars64_path();
    if (!vcvars.has_value()) {
        print_error("error: Visual Studio 2022 with C++ tools not found.");
        return std::nullopt;
    }

    const std::wstring command = L"call \"" + vcvars->wstring() + L"\" >nul && set";
    const std::wstring output = read_pipe_output({L"cmd.exe", L"/c", command});
    if (output.empty()) {
        print_error("error: failed to initialize MSVC environment via vcvars64.bat");
        return std::nullopt;
    }

    std::map<std::wstring, std::wstring> env = current_environment();
    std::wistringstream stream(output);
    std::wstring line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == L'\r') {
            line.pop_back();
        }
        merge_env_line(env, line);
    }

    const auto install = find_vs_install();
    if (!install.has_value()) {
        print_error("error: Visual Studio 2022 with C++ tools not found.");
        return std::nullopt;
    }

    const auto msvc_bin = find_msvc_bin(*install);
    if (!msvc_bin.has_value()) {
        print_error("error: ml64.exe not found under MSVC tools.");
        return std::nullopt;
    }

    const auto path_it = env.find(L"PATH");
    const std::wstring existing_path = path_it != env.end() ? path_it->second : L"";
    env[L"PATH"] = msvc_bin->wstring() + L";" + existing_path;
    return env;
}

}  // namespace bfpe
