#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace bfpe::codegen {

int exec_bf(const std::filesystem::path& bf_path, const std::vector<std::string>& run_args);

}  // namespace bfpe::codegen
