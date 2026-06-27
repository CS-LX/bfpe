#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace bfpe {

std::filesystem::path bfpe_cache_directory();
std::optional<std::filesystem::path> find_dev_root(const std::filesystem::path& start);
std::filesystem::path ensure_embedded_bundle_root();
std::filesystem::path find_repo_root();

}  // namespace bfpe
