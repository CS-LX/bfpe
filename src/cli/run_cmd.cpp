#include "run_cmd.hpp"

#include "manifest.hpp"
#include "paths.hpp"
#include "process.hpp"

#include <iostream>
#include <sstream>
#include <windows.h>

namespace bfpe {

namespace {

int run_exe(const std::filesystem::path& pe_path, const std::vector<std::string>& run_args) {
    std::vector<std::wstring> args = {pe_path.wstring()};
    for (const std::string& arg : run_args) {
        args.push_back(path_from_utf8(arg).wstring());
    }
    return run_process(args, std::nullopt, false);
}

int invoke_int_export(HMODULE module, const std::string& symbol, const std::vector<int>& args) {
    using Fn0 = int(__cdecl*)();
    using Fn1 = int(__cdecl*)(int);
    using Fn2 = int(__cdecl*)(int, int);

    switch (args.size()) {
        case 0: {
            const auto fn = reinterpret_cast<Fn0>(GetProcAddress(module, symbol.c_str()));
            if (!fn) {
                return -1;
            }
            std::cout << fn() << std::endl;
            return 0;
        }
        case 1: {
            const auto fn = reinterpret_cast<Fn1>(GetProcAddress(module, symbol.c_str()));
            if (!fn) {
                return -1;
            }
            std::cout << fn(args[0]) << std::endl;
            return 0;
        }
        case 2: {
            const auto fn = reinterpret_cast<Fn2>(GetProcAddress(module, symbol.c_str()));
            if (!fn) {
                return -1;
            }
            std::cout << fn(args[0], args[1]) << std::endl;
            return 0;
        }
        default:
            print_error("error: unsupported argument count for int export");
            return 1;
    }
}

int invoke_cstr_export(HMODULE module, const std::string& symbol) {
    using Fn = const char*(__cdecl*)();
    const auto fn = reinterpret_cast<Fn>(GetProcAddress(module, symbol.c_str()));
    if (!fn) {
        return -1;
    }
    const char* result = fn();
    if (result != nullptr) {
        std::cout << result << std::endl;
    }
    return 0;
}

int invoke_void_export(HMODULE module, const std::string& symbol, const std::vector<int>& args) {
    using Fn0 = void(__cdecl*)();
    using Fn1 = void(__cdecl*)(int);
    using Fn2 = void(__cdecl*)(int, int);

    switch (args.size()) {
        case 0: {
            const auto fn = reinterpret_cast<Fn0>(GetProcAddress(module, symbol.c_str()));
            if (!fn) {
                return -1;
            }
            fn();
            return 0;
        }
        case 1: {
            const auto fn = reinterpret_cast<Fn1>(GetProcAddress(module, symbol.c_str()));
            if (!fn) {
                return -1;
            }
            fn(args[0]);
            return 0;
        }
        case 2: {
            const auto fn = reinterpret_cast<Fn2>(GetProcAddress(module, symbol.c_str()));
            if (!fn) {
                return -1;
            }
            fn(args[0], args[1]);
            return 0;
        }
        default:
            print_error("error: unsupported argument count for void export");
            return 1;
    }
}

}  // namespace

int cmd_run(const std::filesystem::path& root,
            const std::filesystem::path& pe_path,
            const std::string& export_name,
            const std::vector<std::string>& run_args) {
    const std::filesystem::path resolved_pe = std::filesystem::absolute(pe_path);
    if (!std::filesystem::exists(resolved_pe)) {
        print_error("error: PE not found: " + path_to_utf8(resolved_pe));
        return 1;
    }

    const auto manifest = load_manifest(manifest_path_for_pe(root, resolved_pe));
    if (!manifest.has_value()) {
        print_error("error: rebuild with bfpe build ... -o " + path_to_utf8(resolved_pe.filename()));
        return 1;
    }

    const ProgramInfo* program = find_program(*manifest, export_name);
    if (program == nullptr) {
        std::ostringstream known;
        for (size_t i = 0; i < manifest->programs.size(); ++i) {
            if (i > 0) {
                known << ", ";
            }
            known << manifest->programs[i].export_name;
        }
        print_error("error: export '" + export_name + "' not found (available: " + known.str() + ")");
        return 1;
    }

    if (manifest->pe_kind == "exe") {
        return run_exe(resolved_pe, run_args);
    }

    if (static_cast<int>(run_args.size()) != program->param_count) {
        print_error("error: " + export_name + " expects " + std::to_string(program->param_count) +
                    " integer argument(s), got " + std::to_string(run_args.size()));
        return 1;
    }

    std::vector<int> int_args;
    int_args.reserve(run_args.size());
    for (const std::string& arg : run_args) {
        try {
            int_args.push_back(std::stoi(arg));
        } catch (const std::exception&) {
            print_error("error: invalid integer argument: " + arg);
            return 1;
        }
    }

    HMODULE module = LoadLibraryW(resolved_pe.wstring().c_str());
    if (!module) {
        print_error("error: LoadLibrary failed");
        return 1;
    }

    int status = 1;
    if (program->return_type == "int") {
        status = invoke_int_export(module, program->export_symbol, int_args);
    } else if (program->return_type == "const char*") {
        status = invoke_cstr_export(module, program->export_symbol);
    } else {
        status = invoke_void_export(module, program->export_symbol, int_args);
    }

    if (status < 0) {
        print_error("error: missing export " + program->export_symbol + " in " + path_to_utf8(resolved_pe.filename()));
        FreeLibrary(module);
        return 1;
    }

    FreeLibrary(module);
    return status;
}

}  // namespace bfpe
