#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace bfpe {

std::optional<std::filesystem::path> find_vs_install();
std::optional<std::filesystem::path> find_msvc_bin(const std::filesystem::path& install);
std::optional<std::filesystem::path> vcvars64_path();
std::optional<std::map<std::wstring, std::wstring>> msvc_environment();

int run_with_vcvars(const std::filesystem::path& vcvars, const std::vector<std::wstring>& args);

}  // namespace bfpe

