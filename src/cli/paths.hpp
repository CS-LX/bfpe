#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace bfpe {

std::filesystem::path exe_directory();
std::filesystem::path bfpe_cache_directory();
std::filesystem::path find_repo_root();
std::filesystem::path runtime_dir(const std::filesystem::path& root);
std::filesystem::path verify_script(const std::filesystem::path& root);
std::filesystem::path build_dir_for_output(const std::filesystem::path& output);
std::filesystem::path manifest_path_for_pe(const std::filesystem::path& pe_path);

std::string path_to_utf8(const std::filesystem::path& path);
std::wstring path_to_wide(const std::filesystem::path& path);
std::filesystem::path path_from_utf8(const std::string& text);

bool has_suffix_icase(const std::filesystem::path& path, const std::wstring& suffix);

}  // namespace bfpe
