#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace bfpe {

int cmd_exec(const std::filesystem::path& root,
             const std::filesystem::path& bf_path,
             const std::vector<std::string>& run_args);

}  // namespace bfpe
