#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace bfpe {

int cmd_build(const std::filesystem::path& root,
              const std::vector<std::filesystem::path>& bf_paths,
              const std::filesystem::path& output);

}  // namespace bfpe
