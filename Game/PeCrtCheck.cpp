#include "PeCrtCheck.h"
#include <Windows.h>
#include <set>

namespace
{
    // Classify a CRT import name. "dynamic" = ucrtbase/api-ms-ucrt/vcruntime/msvcrt
    // loaded from system; "static" = statically linked CRT has no such imports.
    bool b_IsCrtImport(const std::string& dll_name)
    {
        auto contains = [&dll_name](const char* frag)
        {
            return dll_name.find(frag) != std::string::npos;
        };
        return contains("ucrtbase")
            || contains("api-ms-win-crt")
            || contains("vcruntime")
            || contains("msvcrt");
    }
}

std::vector<std::string> GetModuleCrtImports(const char* module_path)
{
    std::vector<std::string> crt_imports;
    HANDLE file = CreateFileA(module_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return crt_imports;

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0) { CloseHandle(file); return crt_imports; }
    const size_t file_size = static_cast<size_t>(size.QuadPart);

    HANDLE mapping = CreateFileMappingA(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (mapping == nullptr) { CloseHandle(file); return crt_imports; }

    HANDLE view = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
    if (view == nullptr) { CloseHandle(mapping); CloseHandle(file); return crt_imports; }

    const BYTE* base = static_cast<const BYTE*>(view);

    // Bounds-checked readers: return 0 on out-of-range access so a malformed
    // or truncated PE simply parses as "no CRT imports" instead of crashing.
    const auto read_u16 = [&](size_t off) -> uint16_t
    { return (off + sizeof(uint16_t) <= file_size) ? *reinterpret_cast<const uint16_t*>(base + off) : 0; };
    const auto read_u32 = [&](size_t off) -> uint32_t
    { return (off + sizeof(uint32_t) <= file_size) ? *reinterpret_cast<const uint32_t*>(base + off) : 0; };

    const auto fail = [&]() -> std::vector<std::string>
    {
        UnmapViewOfFile(view); CloseHandle(mapping); CloseHandle(file);
        return crt_imports;
    };

    if (read_u16(0) != IMAGE_DOS_SIGNATURE) return fail();

    const size_t e_lfanew = read_u32(offsetof(IMAGE_DOS_HEADER, e_lfanew));
    if (e_lfanew == 0 || e_lfanew + 4 > file_size
        || read_u32(e_lfanew) != IMAGE_NT_SIGNATURE)
    {
        return fail();
    }

    const size_t file_header_off = e_lfanew + 4;
    const size_t opt_off = file_header_off + sizeof(IMAGE_FILE_HEADER);
    const uint16_t num_sections = read_u16(file_header_off + offsetof(IMAGE_FILE_HEADER, NumberOfSections));
    const uint16_t opt_size = read_u16(file_header_off + offsetof(IMAGE_FILE_HEADER, SizeOfOptionalHeader));

    const uint16_t opt_magic = read_u16(opt_off + offsetof(IMAGE_OPTIONAL_HEADER32, Magic));
    size_t import_dir_off = 0;
    if (opt_magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
    {
        import_dir_off = opt_off + offsetof(IMAGE_OPTIONAL_HEADER32, DataDirectory)
            + IMAGE_DIRECTORY_ENTRY_IMPORT * sizeof(IMAGE_DATA_DIRECTORY);
    }
    else if (opt_magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
    {
        import_dir_off = opt_off + offsetof(IMAGE_OPTIONAL_HEADER64, DataDirectory)
            + IMAGE_DIRECTORY_ENTRY_IMPORT * sizeof(IMAGE_DATA_DIRECTORY);
    }
    else
    {
        return fail();
    }

    const uint32_t import_rva = read_u32(import_dir_off);
    const uint32_t import_size = read_u32(import_dir_off + 4);
    if (import_rva == 0 || import_size == 0)
    {
        return fail();
    }

    // Section table: needed to translate RVAs to file offsets
    const size_t section_table_off = opt_off + opt_size;
    const size_t section_table_end = section_table_off
        + static_cast<size_t>(num_sections) * sizeof(IMAGE_SECTION_HEADER);
    if (num_sections == 0 || section_table_end > file_size)
    {
        return fail();
    }

    const auto rva_to_file = [&](uint32_t rva) -> size_t
    {
        for (uint16_t i = 0; i < num_sections; ++i)
        {
            const size_t sh = section_table_off + static_cast<size_t>(i) * sizeof(IMAGE_SECTION_HEADER);
            const uint32_t virt_addr = read_u32(sh + offsetof(IMAGE_SECTION_HEADER, VirtualAddress));
            const uint32_t virt_size = read_u32(sh + offsetof(IMAGE_SECTION_HEADER, Misc.VirtualSize));
            const uint32_t raw_ptr = read_u32(sh + offsetof(IMAGE_SECTION_HEADER, PointerToRawData));
            const uint32_t raw_size = read_u32(sh + offsetof(IMAGE_SECTION_HEADER, SizeOfRawData));
            if (rva >= virt_addr && rva < virt_addr + (virt_size ? virt_size : raw_size))
            {
                return static_cast<size_t>(rva - virt_addr) + raw_ptr;
            }
        }
        return static_cast<size_t>(-1); // not found
    };

    // Walk import descriptors until the terminating all-zero entry
    size_t desc = rva_to_file(import_rva);
    if (desc == static_cast<size_t>(-1))
    {
        return fail();
    }

    const size_t max_entries = static_cast<size_t>(import_size) / sizeof(IMAGE_IMPORT_DESCRIPTOR) + 1;
    for (size_t i = 0; i < max_entries; ++i)
    {
        const size_t d = desc + i * sizeof(IMAGE_IMPORT_DESCRIPTOR);
        const uint32_t name_rva = read_u32(d + offsetof(IMAGE_IMPORT_DESCRIPTOR, Name));
        if (name_rva == 0) break;

        const size_t name_off = rva_to_file(name_rva);
        if (name_off == static_cast<size_t>(-1) || name_off >= file_size) continue;

        std::string dll_name;
        for (size_t j = name_off; j < file_size && base[j] != 0 && dll_name.size() < 256; ++j)
        {
            dll_name.push_back(static_cast<char>(base[j]));
        }
        if (b_IsCrtImport(dll_name))
        {
            crt_imports.push_back(dll_name);
        }
    }

    UnmapViewOfFile(view);
    CloseHandle(mapping);
    CloseHandle(file);
    return crt_imports;
}

bool b_CrtImportsCompatible(const std::vector<std::string>& exe_imports,
                            const std::vector<std::string>& dll_imports)
{
    // Normalize: we only care about dynamic-vs-static flavor and, for vcruntime,
    // matching major version digits.
    auto flavor = [](const std::vector<std::string>& imports) -> std::set<std::string>
    {
        std::set<std::string> set;
        for (const auto& name : imports)
        {
            if (name.find("vcruntime140") != std::string::npos
                || name.find("vcruntime141") != std::string::npos)
            {
                set.insert("vcruntime-dynamic");
            }
            else if (name.find("ucrtbase") != std::string::npos
                || name.find("api-ms-win-crt") != std::string::npos
                || name.find("msvcrt") != std::string::npos)
            {
                set.insert("ucrt-dynamic");
            }
        }
        return set;
    };

    std::set<std::string> exe_flavor = flavor(exe_imports);
    std::set<std::string> dll_flavor = flavor(dll_imports);

    // Statically-linked CRT in either module: no dynamic CRT imports recorded.
    bool exe_static = exe_flavor.empty();
    bool dll_static = dll_flavor.empty();

    if (exe_static != dll_static)
    {
        return false; // one static, one dynamic -> heap corruption risk
    }
    return true;
}
