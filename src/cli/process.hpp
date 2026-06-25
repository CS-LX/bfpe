#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace bfpe {

std::map<std::wstring, std::wstring> current_environment();
std::wstring build_environment_block(const std::map<std::wstring, std::wstring>& env);
std::wstring quote_argument(const std::wstring& arg);
std::wstring join_command_line(const std::vector<std::wstring>& args);

int run_process(const std::vector<std::wstring>& args,
                const std::optional<std::map<std::wstring, std::wstring>>& env = std::nullopt,
                bool echo = true);

std::optional<std::filesystem::path> find_executable(const std::wstring& name,
                                                     const std::map<std::wstring, std::wstring>& env);

std::optional<std::filesystem::path> find_python();

void print_error(const std::string& message);

}  // namespace bfpe
