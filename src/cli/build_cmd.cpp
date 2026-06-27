#include "build_cmd.hpp"

#include "bf2asm.hpp"
#include "manifest.hpp"
#include "msvc_env.hpp"
#include "parse_sig.hpp"
#include "paths.hpp"
#include "process.hpp"

#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace bfpe {

namespace {

constexpr const char* kBaseRuntimeExports[] = {"BF_GetLastOutput", "BF_SetOutputCallback"};

std::string pe_kind_for_output(const std::filesystem::path& output) {
    if (has_suffix_icase(output, L".dll")) {
        return "dll";
    }
    if (has_suffix_icase(output, L".exe")) {
        return "exe";
    }
    throw std::runtime_error("unsupported output extension (expected .dll or .exe)");
}

void write_def(const std::filesystem::path& path,
               const std::string& library_name,
               const std::vector<std::string>& exports) {
    std::ofstream output(path);
    output << "LIBRARY " << library_name << "\nEXPORTS\n";
    for (const std::string& name : exports) {
        output << "    " << name << "\n";
    }
}

std::vector<std::string> read_export_symbols(const std::filesystem::path& manifest_path) {
    const auto manifest = load_manifest(manifest_path);
    if (!manifest.has_value()) {
        return {};
    }
    std::vector<std::string> exports;
    exports.reserve(manifest->programs.size());
    for (const ProgramInfo& program : manifest->programs) {
        exports.push_back(program.export_symbol);
    }
    return exports;
}

std::vector<std::string> read_program_symbols(const std::filesystem::path& manifest_path) {
    const auto manifest = load_manifest(manifest_path);
    if (!manifest.has_value()) {
        return {};
    }
    std::vector<std::string> symbols;
    symbols.reserve(manifest->programs.size());
    for (const ProgramInfo& program : manifest->programs) {
        symbols.push_back(program.program_symbol);
    }
    return symbols;
}

int run_or_fail(const std::vector<std::wstring>& args) {
    const int code = run_process(args);
    if (code != 0) {
        return 1;
    }
    return 0;
}

int run_msvc_or_fail(const std::filesystem::path& vcvars, const std::vector<std::wstring>& args) {
    const int code = run_with_vcvars(vcvars, args);
    if (code != 0) {
        return 1;
    }
    return 0;
}

}  // namespace

int cmd_build(const std::filesystem::path& root,
              const std::vector<std::filesystem::path>& bf_paths,
              const std::filesystem::path& output) {
    if (bf_paths.empty()) {
        print_error("error: no .bf inputs");
        return 1;
    }

    for (const auto& bf_path : bf_paths) {
        if (!std::filesystem::exists(bf_path)) {
            print_error("error: " + path_to_utf8(bf_path) + " not found");
            return 1;
        }
    }

    std::string pe_kind;
    try {
        pe_kind = pe_kind_for_output(output);
    } catch (const std::exception& exc) {
        print_error(std::string("error: ") + exc.what());
        return 1;
    }

    std::vector<std::filesystem::path> resolved_inputs;
    resolved_inputs.reserve(bf_paths.size());
    for (const auto& bf_path : bf_paths) {
        resolved_inputs.push_back(std::filesystem::absolute(bf_path));
    }
    const std::filesystem::path resolved_output = std::filesystem::absolute(output);

    if (pe_kind == "exe") {
        if (resolved_inputs.size() != 1) {
            print_error("error: EXE build supports exactly one .bf input");
            return 1;
        }
        try {
            const auto signature = codegen::parse_file(resolved_inputs.front());
            if (!signature.is_entry) {
                print_error("error: EXE build requires '; bfpe: entry' in " +
                             path_to_utf8(resolved_inputs.front().filename()));
                return 1;
            }
        } catch (const std::exception& exc) {
            print_error(std::string("error: ") + exc.what());
            return 1;
        }
    }

    const std::filesystem::path build_dir = build_dir_for_output(resolved_output);
    const std::filesystem::path gen_dir = build_dir / "gen";
    const std::filesystem::path obj_dir = build_dir / "obj";
    std::filesystem::create_directories(gen_dir);
    std::filesystem::create_directories(obj_dir);
    std::filesystem::create_directories(resolved_output.parent_path());

    const std::filesystem::path gen_asm = gen_dir / "bf_programs.asm";
    const std::filesystem::path gen_header = gen_dir / "bf_exports.gen.h";
    const std::filesystem::path gen_source = gen_dir / "bf_exports.gen.c";
    const std::filesystem::path gen_exe_main = gen_dir / "exe_main.gen.c";
    const std::filesystem::path manifest_path = build_dir / "manifest.json";
    const std::filesystem::path def_path = build_dir / (resolved_output.stem().wstring() + L".def");

    codegen::GenerateOptions gen_options;
    gen_options.asm_path = gen_asm;
    gen_options.header_path = gen_header;
    gen_options.source_path = gen_source;
    gen_options.manifest_path = manifest_path;
    gen_options.pe_path = resolved_output;
    gen_options.pe_kind = pe_kind;
    if (pe_kind == "exe") {
        gen_options.exe_main_path = gen_exe_main;
    }

    std::string codegen_error;
    if (!codegen::generate_artifacts(resolved_inputs, gen_options, codegen_error)) {
        print_error("error: " + codegen_error);
        return 1;
    }
    std::cout << "Generated " << path_to_utf8(gen_asm) << " from " << resolved_inputs.size()
              << " program(s)" << std::endl;

    const std::vector<std::string> generated_exports = read_export_symbols(manifest_path);
    if (generated_exports.empty()) {
        print_error("error: no exports generated from .bf input");
        return 1;
    }

    const auto vcvars = vcvars64_path();
    if (!vcvars.has_value()) {
        print_error("error: Visual Studio 2022 with C++ tools not found.");
        return 1;
    }

    const std::filesystem::path asm_obj = obj_dir / "bf_programs.obj";
    if (run_msvc_or_fail(*vcvars, {L"ml64.exe", L"/c", L"/Fo", asm_obj.wstring(), gen_asm.wstring()}) != 0) {
        return 1;
    }

    const std::filesystem::path runtime = runtime_dir(root);
    const std::vector<std::filesystem::path> runtime_core = {
        runtime / "vm" / "bf_vm.c",
        runtime / "bf_io.c",
        runtime / "bf_export_runtime.c",
        runtime / "bf_stub.c",
    };

    std::vector<std::filesystem::path> c_sources = runtime_core;
    c_sources.push_back(gen_source);
    if (pe_kind == "dll") {
        c_sources.push_back(runtime / "dllmain.c");
    } else {
        c_sources.push_back(gen_exe_main);
    }

    std::vector<std::filesystem::path> c_objects;
    c_objects.reserve(c_sources.size());
    for (const auto& source : c_sources) {
        const std::filesystem::path obj_path = obj_dir / (source.stem().wstring() + L".obj");
        std::vector<std::wstring> compile_cmd = {
            L"cl.exe", L"/nologo", L"/O2", L"/W4", L"/c", L"/DBFDLL_EXPORTS",
            L"/I", runtime.wstring(),
            L"/I", (runtime / "vm").wstring(),
            L"/I", gen_dir.wstring(),
            L"/Fo" + obj_path.wstring(),
            source.wstring(),
        };
        if (run_msvc_or_fail(*vcvars, compile_cmd) != 0) {
            return 1;
        }
        c_objects.push_back(obj_path);
    }

    std::vector<std::wstring> link_cmd = {
        L"link.exe",
        L"/nologo",
        L"/OUT:" + resolved_output.wstring(),
        L"/MERGE:bf_text=.text",
    };

    for (const std::string& symbol : read_program_symbols(manifest_path)) {
        link_cmd.push_back(L"/INCLUDE:" + std::wstring(symbol.begin(), symbol.end()));
    }

    link_cmd.push_back(asm_obj.wstring());
    for (const auto& obj : c_objects) {
        link_cmd.push_back(obj.wstring());
    }

    if (pe_kind == "dll") {
        std::vector<std::string> def_exports = generated_exports;
        for (const char* export_name : kBaseRuntimeExports) {
            def_exports.emplace_back(export_name);
        }
        write_def(def_path, path_to_utf8(resolved_output.stem()), def_exports);
        link_cmd.insert(link_cmd.begin() + 2, L"/DLL");
        link_cmd.insert(link_cmd.begin() + 3, L"/DEF:" + def_path.wstring());
        link_cmd.insert(link_cmd.begin() + 4, L"/SUBSYSTEM:WINDOWS");
    } else {
        link_cmd.insert(link_cmd.begin() + 2, L"/SUBSYSTEM:CONSOLE");
    }

    if (run_msvc_or_fail(*vcvars, link_cmd) != 0) {
        return 1;
    }

    if (run_or_fail(
            {L"powershell",
             L"-ExecutionPolicy",
             L"Bypass",
             L"-File",
             verify_script(root).wstring(),
             L"-ManifestPath",
             manifest_path.wstring()}) != 0) {
        return 1;
    }

    std::cout << "Built and verified: " << path_to_utf8(resolved_output) << std::endl;
    return 0;
}

}  // namespace bfpe
