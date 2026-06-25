#include "bf2asm.hpp"
#include "parse_sig.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

using bfpe::codegen::Param;
using bfpe::codegen::Signature;
using bfpe::codegen::parse_file;
using bfpe::codegen::parse_signature_line;

namespace {

int g_failures = 0;

void expect_true(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        ++g_failures;
    }
}

void expect_eq(const std::string& actual, const std::string& expected, const std::string& label) {
    if (actual != expected) {
        std::cerr << "FAIL: " << label << " expected '" << expected << "' got '" << actual << "'"
                  << std::endl;
        ++g_failures;
    }
}

void expect_eq(size_t actual, size_t expected, const std::string& label) {
    if (actual != expected) {
        std::cerr << "FAIL: " << label << " expected " << expected << " got " << actual << std::endl;
        ++g_failures;
    }
}

std::filesystem::path write_temp_bf(const std::string& text) {
    const auto path = std::filesystem::temp_directory_path() / "bfpe_test_temp.bf";
    std::ofstream output(path);
    output << text;
    return path;
}

void test_strip_bf() {
    expect_eq(bfpe::codegen::strip_bf("; bfpe: export=Add\n; bfpe: int add(int a, int b)\n>[-<+>]\n"),
              ">[-<+>]",
              "strip_bf preserves + and -");
}

void test_signature_line() {
    std::string ret;
    std::string name;
    std::vector<Param> params;

    parse_signature_line("int add(int a, int b)", ret, name, params);
    expect_eq(ret, "int", "int add ret");
    expect_eq(name, "add", "int add name");
    expect_eq(params.size(), 2U, "int add params");

    parse_signature_line("const char* hello(void)", ret, name, params);
    expect_eq(ret, "const char*", "hello ret");
    expect_eq(params.size(), 0U, "hello params");

    parse_signature_line("void hello(void)", ret, name, params);
    expect_eq(ret, "void", "void ret");
}

void test_parse_file() {
    const auto add_path = write_temp_bf("; bfpe: export=Add\n; bfpe: int add(int a, int b)\n>[-<+>]\n");
    const Signature add_sig = parse_file(add_path);
    expect_eq(add_sig.export_symbol, "BF_Add", "add export symbol");
    expect_eq(add_sig.return_type, "int", "add return type");
    expect_eq(add_sig.io_mode, "none", "add io mode");
    expect_eq(add_sig.params.size(), 2U, "add param count");

    const auto hello_path =
        write_temp_bf("; bfpe: export=Hello\n; bfpe: void hello(void)\n; bfpe: io=stdio\n.\n");
    const Signature hello_sig = parse_file(hello_path);
    expect_eq(hello_sig.export_symbol, "BF_Hello", "hello export symbol");
    expect_eq(hello_sig.io_mode, "stdio", "hello io mode");

    const auto legacy_path = std::filesystem::temp_directory_path() / "add.bf";
    {
        std::ofstream output(legacy_path);
        output << "; bfdll: export=output\n>[-<+>]\n";
    }
    const Signature legacy_sig = parse_file(legacy_path);
    expect_eq(legacy_sig.export_symbol, "BF_Add", "legacy export symbol");
    expect_eq(legacy_sig.return_type, "const char*", "legacy return type");
    expect_eq(legacy_sig.io_mode, "buffer", "legacy io mode");
}

void test_generate_artifacts() {
    const std::filesystem::path root = std::filesystem::current_path();
    const auto add = root / "examples" / "add.bf";
    const auto hello_world = root / "examples" / "hello_world.bf";
    if (!std::filesystem::exists(add) || !std::filesystem::exists(hello_world)) {
        std::cout << "skip generate test (examples not found from cwd)" << std::endl;
        return;
    }

    const auto out_dir = std::filesystem::temp_directory_path() / "bfpe_codegen_test";
    std::filesystem::create_directories(out_dir / "gen");

    bfpe::codegen::GenerateOptions options;
    options.asm_path = out_dir / "gen" / "bf_programs.asm";
    options.header_path = out_dir / "gen" / "bf_exports.gen.h";
    options.source_path = out_dir / "gen" / "bf_exports.gen.c";
    options.manifest_path = out_dir / "manifest.json";
    options.pe_path = out_dir / "out.dll";
    options.pe_kind = "dll";

    std::string error;
    expect_true(bfpe::codegen::generate_artifacts({add, hello_world}, options, error),
                "generate multi-export artifacts: " + error);
    expect_true(std::filesystem::exists(options.asm_path), "asm generated");
    expect_true(std::filesystem::exists(options.manifest_path), "manifest generated");
}

}  // namespace

int main() {
    test_strip_bf();
    test_signature_line();
    test_parse_file();
    test_generate_artifacts();

    if (g_failures != 0) {
        std::cerr << g_failures << " test(s) failed" << std::endl;
        return 1;
    }

    std::cout << "All codegen tests passed." << std::endl;
    return 0;
}
