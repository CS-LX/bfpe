#include "bf2asm.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <sstream>

namespace bfpe::codegen {

namespace {

constexpr size_t kChunkSize = 64;
const std::string kBfChars = "+-<>[],.";

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

std::string read_file_text(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

void write_text_file(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << content;
}

std::string json_escape(const std::string& text) {
    std::string escaped;
    escaped.reserve(text.size() + 8);
    for (char ch : text) {
        switch (ch) {
            case '\\':
                escaped += "\\\\";
                break;
            case '"':
                escaped += "\\\"";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped += ch;
                break;
        }
    }
    return escaped;
}

void validate_export_uniqueness(const std::vector<Program>& programs) {
    std::map<std::string, std::string, std::less<>> by_name;
    std::map<std::string, std::string, std::less<>> by_symbol;
    for (const Program& program : programs) {
        const std::string key = to_lower(program.signature.export_name);
        if (by_name.find(key) != by_name.end()) {
            throw std::runtime_error("duplicate export name '" + program.signature.export_name +
                                     "' in " + program.source_name + " and " + by_name[key]);
        }
        by_name[key] = program.source_name;
        if (by_symbol.find(program.export_symbol) != by_symbol.end()) {
            throw std::runtime_error("duplicate export symbol " + program.export_symbol + " in " +
                                     program.source_name + " and " + by_symbol[program.export_symbol]);
        }
        by_symbol[program.export_symbol] = program.source_name;
    }
}

std::vector<std::string> emit_db_lines(const std::string& code) {
    std::vector<std::string> lines;
    for (size_t i = 0; i < code.size(); i += kChunkSize) {
        const std::string chunk = code.substr(i, kChunkSize);
        std::string escaped;
        escaped.reserve(chunk.size() + 4);
        for (char ch : chunk) {
            if (ch == '\'') {
                escaped += "''";
            } else {
                escaped += ch;
            }
        }
        lines.push_back("    db '" + escaped + "'");
    }
    lines.push_back("    db 0");
    return lines;
}

std::string generate_program_block(const Program& program) {
    std::ostringstream block;
    block << "; from " << program.source_name << "\n";
    block << "public " << program.program_symbol << "\n";
    block << program.program_symbol << " label byte\n";
    for (const std::string& line : emit_db_lines(program.code)) {
        block << line << "\n";
    }
    return block.str();
}

std::string generate_asm(const std::vector<Program>& programs) {
    std::ostringstream output;
    output << "; AUTO-GENERATED - DO NOT EDIT\n";
    output << "bf_text segment 'CODE'\n";
    output << "align 16\n\n";
    for (const Program& program : programs) {
        output << generate_program_block(program) << "\n";
    }
    output << "bf_text ends\nend\n";
    return output.str();
}

std::string c_return_type(const Signature& sig) {
    return sig.return_type == "const char*" ? "const char*" : sig.return_type;
}

std::string generate_header_decl(const Signature& sig) {
    std::string params;
    for (size_t i = 0; i < sig.params.size(); ++i) {
        if (i > 0) {
            params += ", ";
        }
        params += "int " + sig.params[i].name;
    }
    return "BFDLL_API " + c_return_type(sig) + " __cdecl " + sig.export_symbol + "(" + params + ")";
}

std::vector<std::string> generate_export_body(const Signature& sig, const std::string& program_symbol) {
    const std::string io_mode = sig.io_mode_c();
    if (sig.return_type == "int") {
        if (!sig.params.empty()) {
            std::string arg_names;
            std::string param_decls;
            for (size_t i = 0; i < sig.params.size(); ++i) {
                if (i > 0) {
                    arg_names += ", ";
                    param_decls += ", ";
                }
                arg_names += sig.params[i].name;
                param_decls += "int " + sig.params[i].name;
            }
            return {
                "int __cdecl " + sig.export_symbol + "(" + param_decls + ")",
                "{",
                "    const int args[] = { " + arg_names + " };",
                "    return bfpe_run_int_program(" + program_symbol + ", " + io_mode +
                    ", args, " + std::to_string(sig.params.size()) + ");",
                "}",
            };
        }
        return {
            "int __cdecl " + sig.export_symbol + "(void)",
            "{",
            "    return bfpe_run_int_program(" + program_symbol + ", " + io_mode + ", NULL, 0);",
            "}",
        };
    }

    if (sig.return_type == "const char*") {
        return {
            "const char* __cdecl " + sig.export_symbol + "(void)",
            "{",
            "    return bfpe_run_string_program(" + program_symbol + ", " + io_mode + ");",
            "}",
        };
    }

    return {
        "void __cdecl " + sig.export_symbol + "(void)",
        "{",
        "    bfpe_run_void_program(" + program_symbol + ", " + io_mode + ");",
        "}",
    };
}

std::string generate_header(const std::vector<Program>& programs, const std::string& pe_kind) {
    std::ostringstream output;
    output << "/* AUTO-GENERATED - DO NOT EDIT */\n";
    output << "#ifndef BF_EXPORTS_GEN_H\n";
    output << "#define BF_EXPORTS_GEN_H\n\n";
    output << "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n";
    if (pe_kind == "exe") {
        output << "#define BFDLL_API\n\n";
    } else {
        output << "#ifdef BFDLL_EXPORTS\n";
        output << "#define BFDLL_API __declspec(dllexport)\n";
        output << "#else\n";
        output << "#define BFDLL_API __declspec(dllimport)\n";
        output << "#endif\n\n";
    }
    for (const Program& program : programs) {
        output << generate_header_decl(program.signature) << ";\n";
    }
    output << "\n#ifdef __cplusplus\n}\n#endif\n\n#endif\n";
    return output.str();
}

std::string generate_source(const std::vector<Program>& programs) {
    std::ostringstream output;
    output << "/* AUTO-GENERATED - DO NOT EDIT */\n";
    output << "#include \"bf_exports.gen.h\"\n";
    output << "#include \"bf_export_runtime.h\"\n\n";
    for (const Program& program : programs) {
        output << "extern const char " << program.program_symbol << "[];\n\n";
        for (const std::string& line : generate_export_body(program.signature, program.program_symbol)) {
            output << line << "\n";
        }
        output << "\n";
    }
    return output.str();
}

std::string generate_exe_main(const Signature& sig) {
    std::ostringstream output;
    output << "/* AUTO-GENERATED - DO NOT EDIT */\n";
    output << "#include <stdio.h>\n";
    output << "#include <stdlib.h>\n";
    output << "#include \"bf_exports.gen.h\"\n\n";

    if (sig.return_type == "int") {
        output << "int main(int argc, char** argv)\n{\n";
        if (!sig.params.empty()) {
            const size_t expected = sig.params.size() + 1;
            output << "    if (argc < " << expected << ") {\n";
            output << "        fprintf(stderr, \"usage: %s";
            for (const Param& param : sig.params) {
                output << " " << param.name;
            }
            output << "\\n\", argv[0]);\n";
            output << "        return 1;\n";
            output << "    }\n";
            for (size_t index = 0; index < sig.params.size(); ++index) {
                output << "    int " << sig.params[index].name << " = atoi(argv[" << (index + 1)
                       << "]);\n";
            }
            output << "    printf(\"%d\\n\", " << sig.export_symbol << "(";
            for (size_t i = 0; i < sig.params.size(); ++i) {
                if (i > 0) {
                    output << ", ";
                }
                output << sig.params[i].name;
            }
            output << "));\n";
        } else {
            output << "    printf(\"%d\\n\", " << sig.export_symbol << "());\n";
        }
        output << "    return 0;\n}\n";
        return output.str();
    }

    if (sig.return_type == "const char*") {
        output << "int main(void)\n{\n";
        output << "    const char* out = " << sig.export_symbol << "();\n";
        output << "    if (out && out[0]) {\n";
        output << "        printf(\"%s\\n\", out);\n";
        output << "    }\n";
        output << "    return 0;\n}\n";
        return output.str();
    }

    output << "int main(void)\n{\n";
    output << "    " << sig.export_symbol << "();\n";
    output << "    return 0;\n}\n";
    return output.str();
}

std::string build_manifest_json(const std::vector<Program>& programs,
                                const std::filesystem::path& pe_path,
                                const std::string& pe_kind) {
    std::ostringstream json;
    json << "{\n";
    json << "  \"pe_path\": \"" << json_escape(std::filesystem::absolute(pe_path).string()) << "\",\n";
    json << "  \"pe_kind\": \"" << pe_kind << "\",\n";
    json << "  \"exports\": [";
    bool first_export = true;
    for (const Program& program : programs) {
        if (!first_export) {
            json << ", ";
        }
        first_export = false;
        json << "\"" << json_escape(program.export_symbol) << "\"";
    }
    if (pe_kind == "dll") {
        json << ", \"BF_GetLastOutput\", \"BF_SetOutputCallback\"";
    }
    json << "],\n";
    json << "  \"programs\": [\n";
    for (size_t i = 0; i < programs.size(); ++i) {
        const Program& program = programs[i];
        const Signature& sig = program.signature;
        json << "    {\n";
        json << "      \"source\": \"" << json_escape(program.source_name) << "\",\n";
        json << "      \"program_symbol\": \"" << json_escape(program.program_symbol) << "\",\n";
        json << "      \"export_name\": \"" << json_escape(sig.export_name) << "\",\n";
        json << "      \"export_symbol\": \"" << json_escape(program.export_symbol) << "\",\n";
        json << "      \"core_pattern\": \"" << json_escape(program.core_pattern) << "\",\n";
        json << "      \"return_type\": \"" << json_escape(sig.return_type) << "\",\n";
        json << "      \"io_mode\": \"" << json_escape(sig.io_mode) << "\",\n";
        json << "      \"is_entry\": " << (sig.is_entry ? "true" : "false") << ",\n";
        json << "      \"param_count\": " << sig.params.size() << "\n";
        json << "    }";
        if (i + 1 < programs.size()) {
            json << ",";
        }
        json << "\n";
    }
    json << "  ]\n";
    json << "}\n";
    return json.str();
}

}  // namespace

std::string strip_bf(const std::string& source) {
    std::string stripped;
    std::istringstream lines(source);
    std::string line;
    while (std::getline(lines, line)) {
        const size_t comment = line.find(';');
        if (comment != std::string::npos) {
            line = line.substr(0, comment);
        }
        for (char ch : line) {
            if (kBfChars.find(ch) != std::string::npos) {
                stripped += ch;
            }
        }
    }
    return stripped;
}

std::vector<Program> load_programs(const std::vector<std::filesystem::path>& bf_paths) {
    std::vector<Program> programs;
    programs.reserve(bf_paths.size());
    for (const auto& bf_path : bf_paths) {
        if (!std::filesystem::is_regular_file(bf_path)) {
            throw std::runtime_error(bf_path.string() + " not found");
        }
        Program program;
        program.path = bf_path;
        program.source = read_file_text(bf_path);
        program.signature = parse_source(bf_path, program.source);
        program.source_name = program.signature.source_name;
        program.code = strip_bf(program.source);
        program.program_symbol = program.signature.program_symbol;
        program.export_symbol = program.signature.export_symbol;
        program.core_pattern =
            program.code.size() >= 16 ? program.code.substr(0, 64) : program.code;
        programs.push_back(std::move(program));
    }
    validate_export_uniqueness(programs);
    return programs;
}

bool generate_artifacts(const std::vector<std::filesystem::path>& bf_paths,
                        const GenerateOptions& options,
                        std::string& error_message) {
    try {
        const std::vector<Program> programs = load_programs(bf_paths);
        write_text_file(options.asm_path, generate_asm(programs));
        write_text_file(options.header_path, generate_header(programs, options.pe_kind));
        write_text_file(options.source_path, generate_source(programs));

        if (options.exe_main_path.has_value()) {
            if (programs.size() != 1) {
                throw std::runtime_error("EXE main generation requires exactly one .bf input");
            }
            write_text_file(*options.exe_main_path, generate_exe_main(programs.front().signature));
        }

        write_text_file(options.manifest_path,
                        build_manifest_json(programs, options.pe_path, options.pe_kind));
        return true;
    } catch (const std::exception& exc) {
        error_message = exc.what();
        return false;
    }
}

}  // namespace bfpe::codegen
