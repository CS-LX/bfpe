#include "bf_interp.hpp"

#include "bf2asm.hpp"
#include "parse_sig.hpp"

#include <array>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>

namespace bfpe::codegen {

namespace {

constexpr size_t kTapeSize = 65536;
constexpr size_t kStepLimit = 10'000'000;
const std::string kBfChars = "+-<>[],.";

std::map<int, int> build_jump_table(const std::string& code) {
    std::vector<int> stack;
    std::map<int, int> pairs;
    for (size_t index = 0; index < code.size(); ++index) {
        const char op = code[index];
        if (op == '[') {
            stack.push_back(static_cast<int>(index));
        } else if (op == ']') {
            if (stack.empty()) {
                throw std::runtime_error("unmatched ']'");
            }
            const int open_index = stack.back();
            stack.pop_back();
            pairs[open_index] = static_cast<int>(index);
            pairs[static_cast<int>(index)] = open_index;
        }
    }
    if (!stack.empty()) {
        throw std::runtime_error("unmatched '['");
    }
    return pairs;
}

std::string trim_output(std::string text) {
    while (!text.empty() && (text.back() == '\r' || text.back() == '\n')) {
        text.pop_back();
    }
    return text;
}

std::string run_program(const std::string& code,
                        std::array<int, kTapeSize>& tape,
                        const std::string& io_mode) {
    size_t pointer = 0;
    size_t ip = 0;
    size_t steps = 0;
    const std::map<int, int> jumps = build_jump_table(code);
    std::string output;

    while (ip < code.size()) {
        const char op = code[ip];
        if (kBfChars.find(op) == std::string::npos) {
            ++ip;
            continue;
        }

        if (op == '+') {
            tape[pointer] = (tape[pointer] + 1) & 0xFF;
            ++ip;
        } else if (op == '-') {
            tape[pointer] = (tape[pointer] - 1) & 0xFF;
            ++ip;
        } else if (op == '>') {
            pointer = (pointer + 1) % kTapeSize;
            ++ip;
        } else if (op == '<') {
            pointer = (pointer + kTapeSize - 1) % kTapeSize;
            ++ip;
        } else if (op == '.') {
            const char byte = static_cast<char>(tape[pointer] & 0xFF);
            if (io_mode == "buffer" || io_mode == "stdio") {
                output += byte;
            }
            if (io_mode == "stdio") {
                std::cout << byte << std::flush;
            }
            ++ip;
        } else if (op == ',') {
            if (io_mode == "stdio") {
                const int ch = std::getchar();
                tape[pointer] = ch == EOF ? 0 : ch & 0xFF;
            } else {
                tape[pointer] = 0;
            }
            ++ip;
        } else if (op == '[') {
            if (tape[pointer] == 0) {
                ip = static_cast<size_t>(jumps.at(static_cast<int>(ip))) + 1;
            } else {
                ++ip;
            }
        } else if (op == ']') {
            if (tape[pointer] != 0) {
                ip = static_cast<size_t>(jumps.at(static_cast<int>(ip))) + 1;
            } else {
                ++ip;
            }
        }

        ++steps;
        if (steps > kStepLimit) {
            throw std::runtime_error("step limit exceeded");
        }
    }

    return output;
}

}  // namespace

int exec_bf(const std::filesystem::path& bf_path, const std::vector<std::string>& run_args) {
    const Signature sig = parse_file(bf_path);
    std::ifstream input(bf_path, std::ios::binary);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    const std::string code = strip_bf(buffer.str());

    if (run_args.size() != sig.params.size()) {
        throw std::runtime_error(sig.export_name + " expects " + std::to_string(sig.params.size()) +
                                 " integer argument(s), got " + std::to_string(run_args.size()));
    }

    std::array<int, kTapeSize> tape{};
    for (size_t index = 0; index < run_args.size(); ++index) {
        tape[index] = std::stoi(run_args[index]) & 0xFF;
    }

    const std::string output = run_program(code, tape, sig.io_mode);

    if (sig.return_type == "int") {
        std::cout << (tape[0] & 0xFF) << std::endl;
    } else if (sig.return_type == "const char*") {
        const std::string trimmed = trim_output(output);
        if (!trimmed.empty()) {
            std::cout << trimmed << std::endl;
        }
    } else if (sig.return_type == "void" && sig.io_mode == "stdio") {
        std::cout << std::flush;
    }

    return 0;
}

}  // namespace bfpe::codegen
