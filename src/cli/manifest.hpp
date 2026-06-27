#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace bfpe {

struct ProgramInfo {
    std::string source;
    std::string export_name;
    std::string export_symbol;
    std::string program_symbol;
    std::string core_pattern;
    std::string return_type;
    int param_count = 0;
};

struct Manifest {
    std::string pe_kind;
    std::string pe_path;
    std::vector<std::string> exports;
    std::vector<ProgramInfo> programs;
};

std::optional<Manifest> load_manifest(const std::filesystem::path& path);
const ProgramInfo* find_program(const Manifest& manifest, const std::string& export_name);

}  // namespace bfpe
