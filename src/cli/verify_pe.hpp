#pragma once

#include <filesystem>

namespace bfpe {

int verify_pe_manifest(const std::filesystem::path& manifest_path);

}  // namespace bfpe
