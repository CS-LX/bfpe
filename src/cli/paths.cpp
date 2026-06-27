#include "paths.hpp"

#include "bundle.hpp"

#include <windows.h>

namespace bfpe {

std::filesystem::path exe_directory() {
    wchar_t buffer[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return std::filesystem::current_path();
    }
    return std::filesystem::path(buffer).parent_path();
}

std::filesystem::path runtime_dir(const std::filesystem::path& root) {
    return root / "runtime";
}

std::filesystem::path build_dir_for_output(const std::filesystem::path& output) {
    return bfpe_cache_directory() / "build" / output.stem();
}

std::filesystem::path manifest_path_for_pe(const std::filesystem::path& pe_path) {
    return build_dir_for_output(pe_path) / "manifest.json";
}

std::string path_to_utf8(const std::filesystem::path& path) {
    return path.u8string();
}

std::wstring path_to_wide(const std::filesystem::path& path) {
    return path.wstring();
}

std::filesystem::path path_from_utf8(const std::string& text) {
    return std::filesystem::u8path(text);
}

bool has_suffix_icase(const std::filesystem::path& path, const std::wstring& suffix) {
    const std::wstring value = path_to_wide(path.extension());
    if (value.size() != suffix.size()) {
        return false;
    }
    for (size_t i = 0; i < value.size(); ++i) {
        if (towlower(value[i]) != towlower(suffix[i])) {
            return false;
        }
    }
    return true;
}

}  // namespace bfpe
