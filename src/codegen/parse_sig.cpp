#include "parse_sig.hpp"

#include <cctype>
#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace bfpe::codegen {

namespace {

constexpr const char* kBfpePrefix = "bfpe:";
constexpr const char* kBfdllExportOutput = "bfdll: export=output";

const std::regex kSignatureRe(
    R"(^(void|int|const\s+char\s*\*)\s+([A-Za-z_]\w*)\s*\(([^)]*)\)\s*$)");
const std::regex kParamRe(R"(^int\s+([A-Za-z_]\w*)\s*$)");
const std::regex kIoRe(R"(^io\s*=\s*(buffer|stdio|none)\s*$)", std::regex_constants::icase);
const std::regex kExportRe(R"(^export\s*=\s*([A-Za-z_]\w*)\s*$)");

std::string trim(const std::string& text) {
    size_t start = 0;
    while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])) != 0) {
        ++start;
    }
    size_t end = text.size();
    while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }
    return text.substr(start, end - start);
}

std::string to_lower(std::string text) {
    for (char& ch : text) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return text;
}

std::string title_stem(const std::string& stem) {
    std::string normalized;
    normalized.reserve(stem.size());
    for (char ch : stem) {
        normalized += ch == '-' ? '_' : ch;
    }

    std::string result;
    result.reserve(normalized.size());
    bool capitalize_next = true;
    for (char ch : normalized) {
        if (ch == '_') {
            capitalize_next = true;
            continue;
        }
        if (capitalize_next) {
            result += static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
            capitalize_next = false;
        } else {
            result += ch;
        }
    }
    return result;
}

std::string read_file_text(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

}  // namespace

std::string Signature::io_mode_c() const {
    if (io_mode == "stdio") {
        return "BF_IO_MODE_STDIO";
    }
    if (io_mode == "none") {
        return "BF_IO_MODE_NONE";
    }
    return "BF_IO_MODE_BUFFER";
}

std::string Signature::c_declaration() const {
    const std::string ret = return_type == "const char*" ? "const char*" : return_type;
    std::string param_text;
    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) {
            param_text += ", ";
        }
        param_text += "int " + params[i].name;
    }
    return ret + " __cdecl " + export_symbol + "(" + param_text + ")";
}

std::string program_symbol_from_path(const std::filesystem::path& path) {
    return "BF_Prog_" + title_stem(path.stem().string());
}

std::string export_symbol_from_name(const std::string& export_name) {
    return "BF_" + export_name;
}

std::string default_io_mode(const std::string& return_type) {
    if (return_type == "const char*") {
        return "buffer";
    }
    if (return_type == "void") {
        return "stdio";
    }
    return "none";
}

void parse_signature_line(const std::string& line,
                          std::string& return_type,
                          std::string& c_name,
                          std::vector<Param>& params) {
    std::smatch match;
    const std::string trimmed = trim(line);
    if (!std::regex_match(trimmed, match, kSignatureRe)) {
        throw std::runtime_error("invalid signature line: " + line);
    }

    const std::string raw_ret = match[1].str();
    if (raw_ret.find("char") != std::string::npos) {
        return_type = "const char*";
    } else if (raw_ret == "void") {
        return_type = "void";
    } else {
        return_type = "int";
    }

    c_name = match[2].str();
    params.clear();

    std::string params_text = trim(match[3].str());
    if (params_text.empty() || to_lower(params_text) == "void") {
        return;
    }

    std::istringstream stream(params_text);
    std::string chunk;
    while (std::getline(stream, chunk, ',')) {
        chunk = trim(chunk);
        if (chunk.empty()) {
            continue;
        }
        std::smatch param_match;
        if (!std::regex_match(chunk, param_match, kParamRe)) {
            throw std::runtime_error("unsupported parameter: " + chunk + " (MVP supports int only)");
        }
        params.push_back(Param{"int", param_match[1].str()});
    }
}

Signature parse_source(const std::filesystem::path& path, const std::string& source) {
    std::string export_name;
    std::string signature_line;
    std::vector<Param> params;
    std::string io_mode;
    bool legacy_output = false;
    bool is_entry = false;

    std::istringstream lines(source);
    std::string raw_line;
    while (std::getline(lines, raw_line)) {
        const size_t comment_pos = raw_line.find(';');
        if (comment_pos == std::string::npos) {
            continue;
        }
        std::string comment = trim(raw_line.substr(comment_pos + 1));
        if (comment.empty()) {
            continue;
        }

        if (comment.find(kBfdllExportOutput) != std::string::npos) {
            legacy_output = true;
            continue;
        }

        if (to_lower(comment).rfind(kBfpePrefix, 0) != 0) {
            continue;
        }

        std::string payload = trim(comment.substr(std::char_traits<char>::length(kBfpePrefix)));
        if (payload.empty()) {
            continue;
        }

        std::smatch export_match;
        if (std::regex_match(payload, export_match, kExportRe)) {
            export_name = export_match[1].str();
            continue;
        }

        std::smatch io_match;
        if (std::regex_match(payload, io_match, kIoRe)) {
            io_mode = to_lower(io_match[1].str());
            continue;
        }

        if (payload == "entry") {
            is_entry = true;
            continue;
        }

        try {
            std::string return_type;
            std::string c_name;
            std::vector<Param> parsed_params;
            parse_signature_line(payload, return_type, c_name, parsed_params);
            signature_line = payload;
            params = std::move(parsed_params);
        } catch (const std::runtime_error&) {
            continue;
        }
    }

    std::string return_type;
    std::string c_name;

    if (legacy_output && export_name.empty() && signature_line.empty()) {
        export_name = title_stem(path.stem().string());
        return_type = "const char*";
        c_name = path.stem().string();
        for (char& ch : c_name) {
            if (ch == '-') {
                ch = '_';
            }
        }
        if (io_mode.empty()) {
            io_mode = "buffer";
        }
    } else if (export_name.empty() || signature_line.empty()) {
        throw std::runtime_error(path.filename().string() +
                                 ": expected '; bfpe: export=<Name>' and a C-style signature line "
                                 "(or legacy '; bfdll: export=output')");
    } else {
        parse_signature_line(signature_line, return_type, c_name, params);
        if (io_mode.empty()) {
            io_mode = default_io_mode(return_type);
        }
    }

    if (io_mode != "buffer" && io_mode != "stdio" && io_mode != "none") {
        throw std::runtime_error(path.filename().string() + ": unsupported io mode " + io_mode);
    }

    Signature sig;
    sig.export_name = export_name;
    sig.export_symbol = export_symbol_from_name(export_name);
    sig.return_type = return_type;
    sig.c_name = c_name;
    sig.params = std::move(params);
    sig.io_mode = io_mode;
    sig.is_entry = is_entry;
    sig.program_symbol = program_symbol_from_path(path);
    sig.source_name = path.filename().string();
    return sig;
}

Signature parse_file(const std::filesystem::path& path) {
    return parse_source(path, read_file_text(path));
}

}  // namespace bfpe::codegen
