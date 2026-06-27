#include "bundle.hpp"

#include "paths.hpp"

#include <windows.h>
#include <shlobj.h>

#include <fstream>
#include <mutex>
#include <stdexcept>
#include <vector>

#include "embedded_bundle.gen.hpp"

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

std::filesystem::path bundle_install_root() {
    return bfpe_cache_directory() / "bundle" / kEmbeddedBundleVersion;
}

void write_file_bytes(const std::filesystem::path& path, const unsigned char* data, size_t size) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("failed to write " + path_to_utf8(path));
    }
    output.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
}

void write_bundle_marker(const std::filesystem::path& root) {
    std::ofstream marker(root / ".bfpe-bundle");
    marker << kEmbeddedBundleVersion << "\n" << kEmbeddedBundleFingerprint << "\n";
}

bool bundle_marker_matches(const std::filesystem::path& root) {
    std::ifstream marker(root / ".bfpe-bundle");
    if (!marker) {
        return false;
    }
    std::string version;
    std::string fingerprint;
    if (!std::getline(marker, version) || !std::getline(marker, fingerprint)) {
        return false;
    }
    return version == kEmbeddedBundleVersion && fingerprint == kEmbeddedBundleFingerprint;
}

void extract_embedded_bundle(const std::filesystem::path& root) {
    if (std::filesystem::exists(root)) {
        std::error_code error;
        std::filesystem::remove_all(root, error);
    }
    std::filesystem::create_directories(root);

    for (size_t index = 0; index < kEmbeddedBundleFileCount; ++index) {
        const EmbeddedBundleFile& file = kEmbeddedBundleFiles[index];
        write_file_bytes(root / file.relative_path, file.data, file.size);
    }

    write_bundle_marker(root);
    if (!is_bfpe_root(root)) {
        throw std::runtime_error("embedded bundle extraction incomplete");
    }
}

}  // namespace

std::filesystem::path bfpe_cache_directory() {
    wchar_t local_app_data[MAX_PATH]{};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, local_app_data))) {
        return std::filesystem::path(local_app_data) / "bfpe";
    }
    wchar_t temp_path[MAX_PATH]{};
    const DWORD length = GetTempPathW(MAX_PATH, temp_path);
    if (length == 0 || length >= MAX_PATH) {
        return std::filesystem::temp_directory_path() / "bfpe";
    }
    return std::filesystem::path(temp_path) / "bfpe";
}

std::optional<std::filesystem::path> find_dev_root(const std::filesystem::path& start) {
    if (const char* env_root = std::getenv("BFPE_ROOT")) {
        const std::filesystem::path configured = path_from_utf8(env_root);
        if (const std::filesystem::path root = canonical_if_exists(configured); !root.empty()) {
            return root;
        }
    }

    if (const std::filesystem::path beside_exe = canonical_if_exists(exe_directory()); !beside_exe.empty()) {
        return beside_exe;
    }

    std::filesystem::path dir = start;
    for (int depth = 0; depth < 10; ++depth) {
        if (const std::filesystem::path root = canonical_if_exists(dir); !root.empty()) {
            return root;
        }
        if (!dir.has_parent_path() || dir.parent_path() == dir) {
            break;
        }
        dir = dir.parent_path();
    }

    if (const std::filesystem::path cwd = canonical_if_exists(std::filesystem::current_path()); !cwd.empty()) {
        return cwd;
    }

    return std::nullopt;
}

std::filesystem::path ensure_embedded_bundle_root() {
    static std::once_flag once;
    static std::filesystem::path cached_root;

    std::call_once(once, [] {
        const std::filesystem::path root = bundle_install_root();
        if (!bundle_marker_matches(root) || !is_bfpe_root(root)) {
            extract_embedded_bundle(root);
        }
        cached_root = root;
    });

    return cached_root;
}

std::filesystem::path find_repo_root() {
    if (const auto dev_root = find_dev_root(exe_directory())) {
        return *dev_root;
    }
    return ensure_embedded_bundle_root();
}

}  // namespace bfpe
