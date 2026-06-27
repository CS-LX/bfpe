#include "cli.hpp"

#include "build_cmd.hpp"
#include "bundle.hpp"
#include "exec_cmd.hpp"
#include "parse_sig.hpp"
#include "paths.hpp"
#include "process.hpp"
#include "run_cmd.hpp"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace bfpe {

namespace {

void print_help() {
    std::cout
        << "usage: bfpe build <file.bf> [...] -o <out.dll|out.exe>\n"
        << "       bfpe run <pe> <ExportName> [args...]\n"
        << "       bfpe exec <file.bf> [args...]\n"
        << "\n"
        << "shorthand:\n"
        << "  bfpe <file.bf> [...] -o <out.pe>\n"
        << "  bfpe <file.bf> <args...> <existing.pe>\n"
        << "  bfpe <file.bf> [args...]\n";
}

std::string wide_to_utf8(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }
    const int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0,
                                           nullptr, nullptr);
    if (length <= 0) {
        return {};
    }
    std::string output(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), output.data(), length, nullptr,
                        nullptr);
    return output;
}

std::vector<std::string> to_utf8_args(const std::vector<std::wstring>& args) {
    std::vector<std::string> converted;
    converted.reserve(args.size());
    for (const std::wstring& arg : args) {
        converted.push_back(wide_to_utf8(arg));
    }
    return converted;
}

std::optional<std::string> read_export_name(const std::filesystem::path& bf_path) {
    try {
        return codegen::parse_file(bf_path).export_name;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::optional<int> try_shorthand(const std::filesystem::path& root, const std::vector<std::wstring>& args) {
    if (args.empty()) {
        return std::nullopt;
    }

    const std::wstring& first = args.front();
    if (first == L"build" || first == L"run" || first == L"exec" || first == L"-h" || first == L"--help") {
        return std::nullopt;
    }

    const auto output_flag = std::find(args.begin(), args.end(), L"-o");
    if (output_flag != args.end()) {
        if (output_flag + 1 == args.end()) {
            print_error("error: shorthand build: bfpe <file.bf> [...] -o <out.pe>");
            return 1;
        }

        std::vector<std::filesystem::path> bf_paths;
        for (auto it = args.begin(); it != output_flag; ++it) {
            bf_paths.emplace_back(*it);
        }
        return cmd_build(root, bf_paths, std::filesystem::path(*(output_flag + 1)));
    }

    if (args.size() >= 2) {
        const std::filesystem::path pe_path(args.back());
        if (has_suffix_icase(pe_path, L".dll") || has_suffix_icase(pe_path, L".exe")) {
            if (std::filesystem::exists(pe_path)) {
                const std::filesystem::path bf_path(args.front());
                if (!has_suffix_icase(bf_path, L".bf")) {
                    return std::nullopt;
                }
                const auto export_name = read_export_name(bf_path);
                if (!export_name.has_value()) {
                    print_error("error: could not read export name from " + path_to_utf8(bf_path));
                    return 1;
                }
                return cmd_run(root, pe_path, *export_name, to_utf8_args({args.begin() + 1, args.end() - 1}));
            }
        }

        const std::filesystem::path bf_path(args.front());
        if (has_suffix_icase(bf_path, L".bf") && std::filesystem::exists(bf_path)) {
            return cmd_exec(root, bf_path, to_utf8_args({args.begin() + 1, args.end()}));
        }
    }

    return std::nullopt;
}

}  // namespace

int run_cli(int argc, wchar_t* argv[]) {
    const std::filesystem::path root = find_repo_root();
    std::vector<std::wstring> args;
    args.reserve(static_cast<size_t>(argc > 0 ? argc - 1 : 0));
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    if (args.empty() || args.front() == L"-h" || args.front() == L"--help") {
        print_help();
        return args.empty() ? 1 : 0;
    }

    if (const auto shorthand = try_shorthand(root, args)) {
        return *shorthand;
    }

    const std::wstring& command = args.front();
    if (command == L"build") {
        const auto output_flag = std::find(args.begin() + 1, args.end(), L"-o");
        if (output_flag == args.end() || output_flag + 1 == args.end()) {
            print_error("error: build requires -o <out.dll|out.exe>");
            return 1;
        }

        std::vector<std::filesystem::path> inputs;
        for (auto it = args.begin() + 1; it != output_flag; ++it) {
            if (*it == L"--output") {
                continue;
            }
            inputs.emplace_back(*it);
        }
        return cmd_build(root, inputs, std::filesystem::path(*(output_flag + 1)));
    }

    if (command == L"run") {
        if (args.size() < 3) {
            print_error("error: run requires <pe> <ExportName> [args...]");
            return 1;
        }
        return cmd_run(root, std::filesystem::path(args[1]), wide_to_utf8(args[2]), to_utf8_args({args.begin() + 3, args.end()}));
    }

    if (command == L"exec") {
        if (args.size() < 2) {
            print_error("error: exec requires <file.bf> [args...]");
            return 1;
        }
        return cmd_exec(root, std::filesystem::path(args[1]), to_utf8_args({args.begin() + 2, args.end()}));
    }

    print_help();
    return 1;
}

}  // namespace bfpe
