#include "verify_pe.hpp"

#include "manifest.hpp"
#include "paths.hpp"
#include "process.hpp"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace bfpe {

namespace {

struct PeSection {
    std::string name;
    uint32_t virtual_size = 0;
    uint32_t virtual_address = 0;
    uint32_t raw_size = 0;
    uint32_t raw_ptr = 0;
};

struct PeInfo {
    uint16_t characteristics = 0;
    uint32_t export_rva = 0;
    std::vector<PeSection> sections;
};

std::vector<unsigned char> read_file_bytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("failed to read " + path_to_utf8(path));
    }
    return std::vector<unsigned char>(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

uint32_t read_u32_le(const std::vector<unsigned char>& bytes, size_t offset) {
    return static_cast<uint32_t>(bytes[offset]) | (static_cast<uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<uint32_t>(bytes[offset + 2]) << 16) | (static_cast<uint32_t>(bytes[offset + 3]) << 24);
}

uint16_t read_u16_le(const std::vector<unsigned char>& bytes, size_t offset) {
    return static_cast<uint16_t>(bytes[offset]) | (static_cast<uint16_t>(bytes[offset + 1]) << 8);
}

PeInfo parse_pe_sections(const std::vector<unsigned char>& bytes) {
    if (bytes.size() < 0x40) {
        throw std::runtime_error("PE file too small");
    }

    const size_t pe_offset = read_u32_le(bytes, 0x3C);
    if (pe_offset + 24 >= bytes.size()) {
        throw std::runtime_error("invalid PE header offset");
    }

    const uint16_t num_sections = read_u16_le(bytes, pe_offset + 6);
    const uint16_t optional_header_size = read_u16_le(bytes, pe_offset + 20);
    const size_t optional_header = pe_offset + 24;
    const size_t section_table = optional_header + optional_header_size;

    PeInfo info;
    info.characteristics = read_u16_le(bytes, pe_offset + 22);
    if (optional_header + 116 <= bytes.size()) {
        info.export_rva = read_u32_le(bytes, optional_header + 112);
    }

    info.sections.reserve(num_sections);
    for (uint16_t index = 0; index < num_sections; ++index) {
        const size_t off = section_table + static_cast<size_t>(index) * 40;
        if (off + 40 > bytes.size()) {
            throw std::runtime_error("truncated PE section table");
        }

        PeSection section;
        section.name.assign(reinterpret_cast<const char*>(&bytes[off]), 8);
        const auto null_pos = section.name.find('\0');
        if (null_pos != std::string::npos) {
            section.name.resize(null_pos);
        }
        section.virtual_size = read_u32_le(bytes, off + 8);
        section.virtual_address = read_u32_le(bytes, off + 12);
        section.raw_size = read_u32_le(bytes, off + 16);
        section.raw_ptr = read_u32_le(bytes, off + 20);
        info.sections.push_back(std::move(section));
    }

    return info;
}

size_t rva_to_offset(uint32_t rva, const std::vector<PeSection>& sections) {
    for (const PeSection& section : sections) {
        const uint64_t section_end =
            static_cast<uint64_t>(section.virtual_address) +
            static_cast<uint64_t>(std::max(section.virtual_size, section.raw_size));
        if (rva >= section.virtual_address && static_cast<uint64_t>(rva) < section_end) {
            return static_cast<size_t>(section.raw_ptr + (rva - section.virtual_address));
        }
    }

    std::ostringstream message;
    message << "RVA 0x" << std::hex << rva << " not mapped";
    throw std::runtime_error(message.str());
}

std::vector<std::string> read_export_names(const std::vector<unsigned char>& bytes,
                                           const std::vector<PeSection>& sections,
                                           uint32_t export_rva) {
    if (export_rva == 0) {
        return {};
    }

    const size_t export_offset = rva_to_offset(export_rva, sections);
    if (export_offset + 36 > bytes.size()) {
        throw std::runtime_error("truncated export directory");
    }

    const uint32_t name_count = read_u32_le(bytes, export_offset + 24);
    const uint32_t names_rva = read_u32_le(bytes, export_offset + 32);
    const size_t names_offset = rva_to_offset(names_rva, sections);

    std::vector<std::string> names;
    names.reserve(name_count);
    for (uint32_t index = 0; index < name_count; ++index) {
        const size_t entry_offset = names_offset + static_cast<size_t>(index) * 4;
        if (entry_offset + 4 > bytes.size()) {
            throw std::runtime_error("truncated export name table");
        }
        const uint32_t name_rva = read_u32_le(bytes, entry_offset);
        const size_t name_offset = rva_to_offset(name_rva, sections);

        std::string name;
        for (size_t pos = name_offset; pos < bytes.size(); ++pos) {
            if (bytes[pos] == 0) {
                break;
            }
            name.push_back(static_cast<char>(bytes[pos]));
        }
        names.push_back(std::move(name));
    }

    return names;
}

const PeSection* find_section(const std::vector<PeSection>& sections, const char* name) {
    for (const PeSection& section : sections) {
        if (section.name == name) {
            return &section;
        }
    }
    return nullptr;
}

bool contains_export(const std::vector<std::string>& export_names, const std::string& symbol) {
    for (const std::string& name : export_names) {
        if (name == symbol) {
            return true;
        }
    }
    return false;
}

std::string bytes_to_ascii(const std::vector<unsigned char>& bytes) {
    std::string text;
    text.reserve(bytes.size());
    for (unsigned char byte : bytes) {
        text.push_back(static_cast<char>(byte));
    }
    return text;
}

size_t find_pattern(const std::string& haystack, const std::string& pattern) {
    if (pattern.empty()) {
        return std::string::npos;
    }
    return haystack.find(pattern);
}

bool offset_in_section(size_t offset, const PeSection& section) {
    return offset >= section.raw_ptr && offset < static_cast<size_t>(section.raw_ptr + section.raw_size);
}

void trim_build_intermediates(const std::filesystem::path& build_dir) {
    const std::filesystem::path obj_dir = build_dir / "obj";
    if (!std::filesystem::exists(obj_dir)) {
        return;
    }
    std::error_code error;
    std::filesystem::remove_all(obj_dir, error);
}

}  // namespace

int verify_pe_manifest(const std::filesystem::path& manifest_path) {
    try {
        const auto manifest = load_manifest(manifest_path);
        if (!manifest.has_value()) {
            return 1;
        }

        const std::filesystem::path pe_path = path_from_utf8(manifest->pe_path);
        std::cout << "Verifying " << path_to_utf8(pe_path) << std::endl;

        if (!std::filesystem::exists(pe_path)) {
            throw std::runtime_error("PE not found: " + path_to_utf8(pe_path));
        }

        const std::vector<unsigned char> bytes = read_file_bytes(pe_path);
        const std::string ascii = bytes_to_ascii(bytes);
        const PeInfo pe = parse_pe_sections(bytes);
        const std::string pe_kind = manifest->pe_kind.empty() ? "dll" : manifest->pe_kind;

        if (pe_kind == "dll") {
            if ((pe.characteristics & 0x2000) == 0) {
                throw std::runtime_error("Not a DLL (IMAGE_FILE_DLL flag missing)");
            }
            std::cout << "[OK] PE headers indicate DLL" << std::endl;

            const std::vector<std::string> export_names =
                read_export_names(bytes, pe.sections, pe.export_rva);
            for (const std::string& symbol : manifest->exports) {
                if (!contains_export(export_names, symbol)) {
                    throw std::runtime_error("Missing export: " + symbol);
                }
            }
            std::cout << "[OK] Required exports present" << std::endl;
        } else if (pe_kind == "exe") {
            if ((pe.characteristics & 0x2000) != 0) {
                throw std::runtime_error("EXE must not set IMAGE_FILE_DLL");
            }
            std::cout << "[OK] PE headers indicate EXE (export table check skipped)" << std::endl;
        } else {
            std::cout << "[OK] PE kind=" << pe_kind << " (export table check skipped)" << std::endl;
        }

        const PeSection* text = find_section(pe.sections, ".text");
        const PeSection* rdata = find_section(pe.sections, ".rdata");
        const PeSection* rsrc = find_section(pe.sections, ".rsrc");

        if (text == nullptr) {
            throw std::runtime_error(".text section missing");
        }
        std::cout << "[OK] .text RawSize = " << text->raw_size << " bytes" << std::endl;

        if (rsrc != nullptr && rsrc->raw_size > 512) {
            throw std::runtime_error(".rsrc too large - possible resource embedding");
        }
        std::cout << "[OK] No large .rsrc section" << std::endl;

        for (const ProgramInfo& program : manifest->programs) {
            const std::string label = program.source.empty() ? program.export_name : program.source;
            if (program.core_pattern.empty()) {
                throw std::runtime_error("Program " + label + " has empty core_pattern");
            }

            const size_t offset = find_pattern(ascii, program.core_pattern);
            if (offset == std::string::npos) {
                throw std::runtime_error("Notepad test failed: " + label + " fragment not found in PE");
            }
            if (!offset_in_section(offset, *text)) {
                std::ostringstream message;
                message << "Fragment " << label << " at offset 0x" << std::hex << offset
                        << " is outside .text";
                throw std::runtime_error(message.str());
            }

            std::cout << "[OK] " << label << " plaintext found in .text at file offset 0x" << std::hex
                      << offset << std::dec << std::endl;
        }

        if (rdata != nullptr && rdata->raw_size > 0) {
            if (static_cast<size_t>(rdata->raw_ptr + rdata->raw_size) > bytes.size()) {
                throw std::runtime_error(".rdata section extends past file end");
            }
            const std::string rdata_ascii =
                bytes_to_ascii(std::vector<unsigned char>(bytes.begin() + rdata->raw_ptr,
                                                          bytes.begin() + rdata->raw_ptr + rdata->raw_size));
            for (const ProgramInfo& program : manifest->programs) {
                const std::string label = program.source.empty() ? program.export_name : program.source;
                if (!program.core_pattern.empty() && rdata_ascii.find(program.core_pattern) != std::string::npos) {
                    throw std::runtime_error(label + " fragment found in .rdata - disguised PE");
                }
            }
            std::cout << "[OK] BF program not in .rdata" << std::endl;
        }

        std::cout << "All verification checks passed." << std::endl;
        trim_build_intermediates(manifest_path.parent_path());
        return 0;
    } catch (const std::exception& exc) {
        print_error(std::string("verify failed: ") + exc.what());
        return 1;
    }
}

}  // namespace bfpe
