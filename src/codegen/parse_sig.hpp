#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace bfpe::codegen {

struct Param {
    std::string type_name;
    std::string name;
};

struct Signature {
    std::string export_name;
    std::string export_symbol;
    std::string return_type;
    std::string c_name;
    std::vector<Param> params;
    std::string io_mode = "buffer";
    bool is_entry = false;
    std::string program_symbol;
    std::string source_name;

    std::string io_mode_c() const;
    std::string c_declaration() const;
};

std::string program_symbol_from_path(const std::filesystem::path& path);
std::string export_symbol_from_name(const std::string& export_name);
std::string default_io_mode(const std::string& return_type);

void parse_signature_line(const std::string& line,
                          std::string& return_type,
                          std::string& c_name,
                          std::vector<Param>& params);

Signature parse_file(const std::filesystem::path& path);
Signature parse_source(const std::filesystem::path& path, const std::string& source);

}  // namespace bfpe::codegen
