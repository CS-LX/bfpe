#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace bfpe {

int cmd_run(const std::filesystem::path& root,
            const std::filesystem::path& pe_path,
            const std::string& export_name,
            const std::vector<std::string>& run_args);

}  // namespace bfpe
