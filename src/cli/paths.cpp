#include "paths.hpp"

#include <windows.h>

#include <cstdlib>
#include <vector>

namespace bfpe {

namespace {

bool is_bfpe_root(const std::filesystem::path& dir) {
    return std::filesystem::exists(dir / "tools" / "verify_pe.ps1") &&
           std::filesystem::exists(dir / "runtime" / "vm" / "bf_vm.c");
}

std::filesystem::path canonical_if_exists(const std::filesystem::path& dir) {
    if (!is_bfpe_root(dir)) {
        return {};
    }
    return std::filesystem::canonical(dir);
}

}  // namespace

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
        if (const std::filesystem::path root = canonical_if_exists(configured); !root.empty()) {
            return root;
        }
    }

    if (const std::filesystem::path beside_exe = canonical_if_exists(exe_directory());
        !beside_exe.empty()) {
        return beside_exe;
    }

    std::filesystem::path dir = exe_directory();
    for (int depth = 0; depth < 10; ++depth) {
        if (const std::filesystem::path root = canonical_if_exists(dir); !root.empty()) {
            return root;
        }
        if (!dir.has_parent_path() || dir.parent_path() == dir) {
            break;
        }
        dir = dir.parent_path();
    }

    const std::filesystem::path cwd = std::filesystem::current_path();
    if (const std::filesystem::path root = canonical_if_exists(cwd); !root.empty()) {
        return root;
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
