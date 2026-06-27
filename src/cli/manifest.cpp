#include "manifest.hpp"

#include "paths.hpp"
#include "process.hpp"

#include <cctype>
#include <fstream>
#include <sstream>

namespace bfpe {

namespace {

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

std::string unquote(const std::string& text) {
    const std::string trimmed = trim(text);
    if (trimmed.size() >= 2 && trimmed.front() == '"' && trimmed.back() == '"') {
        return trimmed.substr(1, trimmed.size() - 2);
    }
    return trimmed;
}

std::optional<std::string> extract_string_field(const std::string& object, const std::string& key) {
    const std::string needle = "\"" + key + "\"";
    const size_t key_pos = object.find(needle);
    if (key_pos == std::string::npos) {
        return std::nullopt;
    }
    const size_t colon = object.find(':', key_pos + needle.size());
    if (colon == std::string::npos) {
        return std::nullopt;
    }
    const size_t quote_start = object.find('"', colon + 1);
    if (quote_start == std::string::npos) {
        return std::nullopt;
    }
    const size_t quote_end = object.find('"', quote_start + 1);
    if (quote_end == std::string::npos) {
        return std::nullopt;
    }
    return object.substr(quote_start + 1, quote_end - quote_start - 1);
}

std::optional<int> extract_int_field(const std::string& object, const std::string& key) {
    const std::string needle = "\"" + key + "\"";
    const size_t key_pos = object.find(needle);
    if (key_pos == std::string::npos) {
        return std::nullopt;
    }
    const size_t colon = object.find(':', key_pos + needle.size());
    if (colon == std::string::npos) {
        return std::nullopt;
    }
    size_t start = colon + 1;
    while (start < object.size() && std::isspace(static_cast<unsigned char>(object[start])) != 0) {
        ++start;
    }
    size_t end = start;
    while (end < object.size() && (std::isdigit(static_cast<unsigned char>(object[end])) != 0 || object[end] == '-')) {
        ++end;
    }
    if (start == end) {
        return std::nullopt;
    }
    return std::stoi(object.substr(start, end - start));
}

std::vector<std::string> extract_string_array(const std::string& json, const std::string& array_key) {
    std::vector<std::string> values;
    const std::string needle = "\"" + array_key + "\"";
    const size_t key_pos = json.find(needle);
    if (key_pos == std::string::npos) {
        return values;
    }
    const size_t array_start = json.find('[', key_pos);
    if (array_start == std::string::npos) {
        return values;
    }

    size_t index = array_start + 1;
    while (index < json.size()) {
        while (index < json.size() && std::isspace(static_cast<unsigned char>(json[index])) != 0 &&
               json[index] != '"' && json[index] != ']') {
            ++index;
        }
        if (index >= json.size() || json[index] == ']') {
            break;
        }
        if (json[index] != '"') {
            break;
        }
        const size_t quote_start = index;
        const size_t quote_end = json.find('"', quote_start + 1);
        if (quote_end == std::string::npos) {
            break;
        }
        values.push_back(json.substr(quote_start + 1, quote_end - quote_start - 1));
        index = quote_end + 1;
    }
    return values;
}

std::vector<std::string> extract_objects(const std::string& json, const std::string& array_key) {
    std::vector<std::string> objects;
    const std::string needle = "\"" + array_key + "\"";
    const size_t key_pos = json.find(needle);
    if (key_pos == std::string::npos) {
        return objects;
    }
    const size_t array_start = json.find('[', key_pos);
    if (array_start == std::string::npos) {
        return objects;
    }

    size_t index = array_start + 1;
    while (index < json.size()) {
        const size_t object_start = json.find('{', index);
        if (object_start == std::string::npos) {
            break;
        }

        int depth = 0;
        size_t object_end = object_start;
        for (; object_end < json.size(); ++object_end) {
            if (json[object_end] == '{') {
                ++depth;
            } else if (json[object_end] == '}') {
                --depth;
                if (depth == 0) {
                    break;
                }
            }
        }
        if (depth != 0) {
            break;
        }

        objects.push_back(json.substr(object_start, object_end - object_start + 1));
        index = object_end + 1;
    }
    return objects;
}

bool equals_icase(const std::string& left, const std::string& right) {
    if (left.size() != right.size()) {
        return false;
    }
    for (size_t i = 0; i < left.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(left[i])) !=
            std::tolower(static_cast<unsigned char>(right[i]))) {
            return false;
        }
    }
    return true;
}

}  // namespace

std::optional<Manifest> load_manifest(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        print_error("error: manifest not found: " + path_to_utf8(path));
        return std::nullopt;
    }

    std::ifstream input(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    const std::string json = buffer.str();

    Manifest manifest;
    if (auto pe_kind = extract_string_field(json, "pe_kind")) {
        manifest.pe_kind = *pe_kind;
    } else {
        manifest.pe_kind = "dll";
    }
    if (auto pe_path = extract_string_field(json, "pe_path")) {
        manifest.pe_path = *pe_path;
    }

    manifest.exports = extract_string_array(json, "exports");

    for (const std::string& object : extract_objects(json, "programs")) {
        ProgramInfo program;
        if (auto source = extract_string_field(object, "source")) {
            program.source = *source;
        }
        if (auto export_name = extract_string_field(object, "export_name")) {
            program.export_name = *export_name;
        }
        if (auto export_symbol = extract_string_field(object, "export_symbol")) {
            program.export_symbol = *export_symbol;
        }
        if (auto program_symbol = extract_string_field(object, "program_symbol")) {
            program.program_symbol = *program_symbol;
        }
        if (auto core_pattern = extract_string_field(object, "core_pattern")) {
            program.core_pattern = *core_pattern;
        }
        if (auto return_type = extract_string_field(object, "return_type")) {
            program.return_type = *return_type;
        }
        if (auto param_count = extract_int_field(object, "param_count")) {
            program.param_count = *param_count;
        }
        if (!program.export_name.empty()) {
            manifest.programs.push_back(std::move(program));
        }
    }

    if (manifest.programs.empty()) {
        print_error("error: manifest has no programs");
        return std::nullopt;
    }
    return manifest;
}

const ProgramInfo* find_program(const Manifest& manifest, const std::string& export_name) {
    for (const ProgramInfo& program : manifest.programs) {
        if (equals_icase(program.export_name, export_name)) {
            return &program;
        }
    }
    return nullptr;
}

}  // namespace bfpe
