#pragma once

#include "parse_sig.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace bfpe::codegen {

struct Program {
    std::filesystem::path path;
    std::string source_name;
    std::string source;
    std::string code;
    Signature signature;
    std::string program_symbol;
    std::string export_symbol;
    std::string core_pattern;
};

struct GenerateOptions {
    std::filesystem::path asm_path;
    std::filesystem::path header_path;
    std::filesystem::path source_path;
    std::filesystem::path manifest_path;
    std::optional<std::filesystem::path> exe_main_path;
    std::filesystem::path pe_path;
    std::string pe_kind;
};

std::string strip_bf(const std::string& source);
std::vector<Program> load_programs(const std::vector<std::filesystem::path>& bf_paths);

bool generate_artifacts(const std::vector<std::filesystem::path>& bf_paths,
                        const GenerateOptions& options,
                        std::string& error_message);

}  // namespace bfpe::codegen
