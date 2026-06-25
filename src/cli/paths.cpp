#include "paths.hpp"

#include <windows.h>

#include <cstdlib>
#include <vector>

namespace bfpe {

std::filesystem::path exe_directory() {
    wchar_t buffer[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return std::filesystem::current_path();
    }
    return std::filesystem::path(buffer).parent_path();
}

std::filesystem::path find_repo_root() {
    if (const char* env_root = std::getenv("BFPE_ROOT")) {
        const std::filesystem::path configured = path_from_utf8(env_root);
        if (std::filesystem::exists(configured / "tools" / "verify_pe.ps1")) {
            return std::filesystem::canonical(configured);
        }
    }

    std::filesystem::path dir = exe_directory();
    for (int depth = 0; depth < 10; ++depth) {
        if (std::filesystem::exists(dir / "tools" / "verify_pe.ps1")) {
            return std::filesystem::canonical(dir);
        }
        if (!dir.has_parent_path() || dir.parent_path() == dir) {
            break;
        }
        dir = dir.parent_path();
    }

    const std::filesystem::path cwd = std::filesystem::current_path();
    if (std::filesystem::exists(cwd / "tools" / "verify_pe.ps1")) {
        return std::filesystem::canonical(cwd);
    }

    return cwd;
}

std::filesystem::path runtime_dir(const std::filesystem::path& root) {
    return root / "runtime";
}

std::filesystem::path verify_script(const std::filesystem::path& root) {
    return root / "tools" / "verify_pe.ps1";
}

std::filesystem::path build_dir_for_output(const std::filesystem::path& root,
                                           const std::filesystem::path& output) {
    return root / ".bfpe-build" / output.stem();
}

std::filesystem::path manifest_path_for_pe(const std::filesystem::path& root,
                                           const std::filesystem::path& pe_path) {
    return build_dir_for_output(root, pe_path) / "manifest.json";
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
