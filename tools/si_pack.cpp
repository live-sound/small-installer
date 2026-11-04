#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <vector>
#include <string>
#include <cassert>
#include <map>
#include <set>
#include <algorithm>

// LZMA SDK
#include "LzmaEnc.h"
#include "LzmaDec.h"

// ---------------------------------------------------------------------------
// ISzAlloc wrapper for malloc/free
// ---------------------------------------------------------------------------
static void* SzAlloc(ISzAllocPtr, size_t size) { return malloc(size); }
static void  SzFree(ISzAllocPtr, void* ptr)    { free(ptr); }
static const ISzAlloc g_Alloc = { SzAlloc, SzFree };

// ---------------------------------------------------------------------------
// Footer magic & struct
// ---------------------------------------------------------------------------
static const char SI_MAGIC[4] = {'S', 'I', 'P', '2'};

#pragma pack(push, 1)
struct SiFooter {
    char     magic[4];         // "SIP2"
    uint64_t archive_offset;
    uint64_t archive_size;
    uint32_t checksum;         // CRC32
    uint64_t strings_offset;   // compressed strings blob offset in exe (0 = absent)
    uint32_t strings_size;     // compressed strings blob size in bytes
    uint32_t strings_uncompressed_size;
    uint32_t strings_checksum; // CRC32 of compressed strings blob
};
#pragma pack(pop)

static_assert(sizeof(SiFooter) == 44, "Footer must be 44 bytes");

// Forward declarations
static std::vector<uint8_t> lzma_compress(const uint8_t* data, size_t size, uint32_t dict_size = (1u << 24));
static std::vector<uint8_t> lzma_decompress(const uint8_t* data, size_t compSize, size_t origSize);

// ---------------------------------------------------------------------------
// CRC32 (IEEE 802.3 polynomial, reflected)
// ---------------------------------------------------------------------------
static uint32_t crc32_table[256];
static void crc32_init() {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++)
            c = (c >> 1) ^ (c & 1 ? 0xEDB88320 : 0);
        crc32_table[i] = c;
    }
}
static uint32_t crc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++)
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFF;
}

// ---------------------------------------------------------------------------
// FileEntry & file I/O helpers
// ---------------------------------------------------------------------------
struct FileEntry {
    std::string name;
    std::vector<uint8_t> data;
};

static std::vector<uint8_t> read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "Cannot open: %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END);
    size_t sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> buf(sz);
    fread(buf.data(), 1, sz, f);
    fclose(f);
    return buf;
}

// ---------------------------------------------------------------------------
// Key=value locale file parser
//   Format: one "key = value" per line, split on first " = "
//   Supports escape sequences in values: \n \r \t \\
//   Lines starting with # are comments, empty lines are ignored
// ---------------------------------------------------------------------------
struct KV {
    std::string key;
    std::string value;
};

static std::string kv_unescape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[i + 1]) {
                case 'n':  out += '\n'; i++; break;
                case 'r':  out += '\r'; i++; break;
                case 't':  out += '\t'; i++; break;
                case '\\': out += '\\'; i++; break;
                default:   out += s[i]; break;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

static std::vector<KV> parse_kv_file(const char* data, size_t len) {
    std::vector<KV> result;
    const char* p = data;
    const char* end = data + len;

    // Skip BOM if present (UTF-8 BOM: EF BB BF)
    if (len >= 3 && (uint8_t)p[0] == 0xEF && (uint8_t)p[1] == 0xBB && (uint8_t)p[2] == 0xBF)
        p += 3;

    while (p < end) {
        // Read one line
        const char* line_start = p;
        while (p < end && *p != '\n') p++;
        std::string line(line_start, p);
        if (p < end) p++; // skip \n

        // Strip trailing \r
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#')
            continue;

        // Split on first " = "
        size_t sep = line.find(" = ");
        if (sep == std::string::npos)
            continue;

        std::string key = kv_unescape(line.substr(0, sep));
        std::string value = kv_unescape(line.substr(sep + 3));
        result.push_back({std::move(key), std::move(value)});
    }
    return result;
}

// ---------------------------------------------------------------------------
// FNV-1a hash for wide strings (UTF-16LE)
//   Must match the runtime hash in small_installer.h
// ---------------------------------------------------------------------------
static uint32_t fnv1a_wide(const uint16_t* wstr, size_t len) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < len; i++) {
        // Hash each byte of the 16-bit wchar (little-endian)
        uint8_t lo = (uint8_t)(wstr[i] & 0xFF);
        uint8_t hi = (uint8_t)((wstr[i] >> 8) & 0xFF);
        hash ^= lo;
        hash *= 16777619u;
        hash ^= hi;
        hash *= 16777619u;
    }
    return hash;
}

// ---------------------------------------------------------------------------
// UTF-8 to UTF-16LE conversion
// ---------------------------------------------------------------------------
static std::vector<uint16_t> utf8_to_utf16(const std::string& utf8) {
    std::vector<uint16_t> result;
    const uint8_t* p = (const uint8_t*)utf8.data();
    const uint8_t* end = p + utf8.size();

    while (p < end) {
        uint32_t cp;
        if ((*p & 0x80) == 0) {
            cp = *p++;
        } else if ((*p & 0xE0) == 0xC0) {
            cp = (*p++ & 0x1F) << 6;
            if (p < end) cp |= (*p++ & 0x3F);
        } else if ((*p & 0xF0) == 0xE0) {
            cp = (*p++ & 0x0F) << 12;
            if (p < end) cp |= (*p++ & 0x3F) << 6;
            if (p < end) cp |= (*p++ & 0x3F);
        } else if ((*p & 0xF8) == 0xF0) {
            cp = (*p++ & 0x07) << 18;
            if (p < end) cp |= (*p++ & 0x3F) << 12;
            if (p < end) cp |= (*p++ & 0x3F) << 6;
            if (p < end) cp |= (*p++ & 0x3F);
        } else {
            p++; // skip invalid byte
            continue;
        }

        if (cp < 0x10000) {
            result.push_back((uint16_t)cp);
        } else {
            // Surrogate pair
            cp -= 0x10000;
            result.push_back((uint16_t)(0xD800 | (cp >> 10)));
            result.push_back((uint16_t)(0xDC00 | (cp & 0x3FF)));
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// Locale info for compilation
// ---------------------------------------------------------------------------
struct LocaleInput {
    std::string code;      // e.g. "en", "de"
    std::string file_path; // path to locale .txt file
    std::vector<KV> entries;
};

// ---------------------------------------------------------------------------
// build_string_table  --  compile locale files into binary string table
//
//   Binary format (__si_strings):
//     uint16_t locale_count
//     uint16_t key_count
//     struct { char code[4]; wchar_t display_name[32]; } locales[locale_count]
//     struct { uint32_t key_hash; uint16_t offsets[locale_count]; } entries[key_count]
//     uint8_t pool[]   -- concatenated null-terminated UTF-16LE strings
//
//   offsets are byte offsets into pool. 0xFFFF = missing translation.
// ---------------------------------------------------------------------------
static std::vector<uint8_t> build_string_table(std::vector<LocaleInput>& locales) {
    size_t locale_count = locales.size();

    // Parse all locale files
    for (auto& loc : locales) {
        auto file_data = read_file(loc.file_path.c_str());
        loc.entries = parse_kv_file((const char*)file_data.data(), file_data.size());
        printf("  locale '%s': %zu keys from %s\n",
               loc.code.c_str(), loc.entries.size(), loc.file_path.c_str());
    }

    // Collect all unique keys (preserving insertion order via set)
    std::vector<std::string> all_keys;
    std::set<std::string> seen_keys;
    for (auto& loc : locales) {
        for (auto& kv : loc.entries) {
            if (seen_keys.insert(kv.key).second) {
                all_keys.push_back(kv.key);
            }
        }
    }
    size_t key_count = all_keys.size();

    // Build lookup: locale_idx -> { key -> value }
    std::vector<std::map<std::string, std::string>> locale_maps(locale_count);
    for (size_t li = 0; li < locale_count; li++) {
        for (auto& kv : locales[li].entries) {
            locale_maps[li][kv.key] = kv.value;
        }
    }

    // Extract display names from "nativeName" key in each locale
    std::vector<std::string> display_names(locale_count);
    for (size_t li = 0; li < locale_count; li++) {
        auto it = locale_maps[li].find("nativeName");
        if (it != locale_maps[li].end()) {
            display_names[li] = it->second;
        } else {
            display_names[li] = locales[li].code; // fallback
        }
    }

    // Build wide string pool and compute offsets
    std::vector<uint8_t> pool;
    // offset_table[key_idx][locale_idx] = byte offset into pool, or 0xFFFFFFFF
    std::vector<std::vector<uint32_t>> offset_table(key_count, std::vector<uint32_t>(locale_count, 0xFFFFFFFF));

    for (size_t ki = 0; ki < key_count; ki++) {
        for (size_t li = 0; li < locale_count; li++) {
            auto it = locale_maps[li].find(all_keys[ki]);
            if (it == locale_maps[li].end()) continue;

            offset_table[ki][li] = (uint32_t)pool.size();

            // Convert value to UTF-16LE and append with null terminator
            std::vector<uint16_t> wide = utf8_to_utf16(it->second);
            wide.push_back(0); // null terminator
            const uint8_t* bytes = (const uint8_t*)wide.data();
            pool.insert(pool.end(), bytes, bytes + wide.size() * sizeof(uint16_t));
        }
    }

    // Compute key hashes (hash of UTF-16LE key)
    std::vector<uint32_t> key_hashes(key_count);
    for (size_t ki = 0; ki < key_count; ki++) {
        std::vector<uint16_t> wkey = utf8_to_utf16(all_keys[ki]);
        key_hashes[ki] = fnv1a_wide(wkey.data(), wkey.size());
    }

    // Build binary blob
    std::vector<uint8_t> blob;
    auto write_u16 = [&](uint16_t v) {
        blob.push_back(v & 0xFF);
        blob.push_back((v >> 8) & 0xFF);
    };
    auto write_u32 = [&](uint32_t v) {
        for (int i = 0; i < 4; i++) { blob.push_back(v & 0xFF); v >>= 8; }
    };

    // Header
    write_u16((uint16_t)locale_count);
    write_u16((uint16_t)key_count);

    // Locale info: code[4] + display_name[32] as wchar_t (UTF-16LE)
    for (size_t li = 0; li < locale_count; li++) {
        // Code: 4 bytes, null-padded
        char code_buf[4] = {};
        size_t code_len = locales[li].code.size();
        if (code_len > 4) code_len = 4;
        memcpy(code_buf, locales[li].code.c_str(), code_len);
        blob.insert(blob.end(), code_buf, code_buf + 4);

        // Display name: 32 wchar_t (64 bytes), null-padded UTF-16LE
        std::vector<uint16_t> wname = utf8_to_utf16(display_names[li]);
        uint16_t name_buf[32] = {};
        size_t copy_len = wname.size();
        if (copy_len > 31) copy_len = 31; // leave room for null
        for (size_t i = 0; i < copy_len; i++) name_buf[i] = wname[i];
        const uint8_t* name_bytes = (const uint8_t*)name_buf;
        blob.insert(blob.end(), name_bytes, name_bytes + sizeof(name_buf));
    }

    // Entry table: key_hash (4 bytes) + offsets[locale_count] (4 bytes each)
    for (size_t ki = 0; ki < key_count; ki++) {
        write_u32(key_hashes[ki]);
        for (size_t li = 0; li < locale_count; li++) {
            write_u32(offset_table[ki][li]);
        }
    }

    // Value pool
    blob.insert(blob.end(), pool.begin(), pool.end());

    printf("  String table: %zu locales, %zu keys, %zu bytes pool, %zu bytes total\n",
           locale_count, key_count, pool.size(), blob.size());

    return blob;
}

// ---------------------------------------------------------------------------
// build_archive  --  build uncompressed archive blob
//   Layout:
//     uint32_t file_count
//     Per file:
//       uint16_t name_len
//       char[name_len] filename (no null terminator)
//       uint64_t original_size
//       uint64_t offset_into_data_block
//     Concatenated file data
// ---------------------------------------------------------------------------
static std::vector<uint8_t> build_archive(const std::vector<FileEntry>& files)
{
    // Calculate header size
    size_t header_size = sizeof(uint32_t); // file_count
    for (auto& f : files) {
        header_size += sizeof(uint16_t);   // name_len
        header_size += f.name.size();       // filename
        header_size += sizeof(uint64_t);    // original_size
        header_size += sizeof(uint64_t);    // offset_into_data
    }

    // Calculate total data size
    size_t total_data = 0;
    for (auto& f : files)
        total_data += f.data.size();

    std::vector<uint8_t> archive;
    archive.reserve(header_size + total_data);

    // Helper to write little-endian values
    auto write_u16 = [&](uint16_t v) {
        archive.push_back(v & 0xFF);
        archive.push_back((v >> 8) & 0xFF);
    };
    auto write_u32 = [&](uint32_t v) {
        for (int i = 0; i < 4; i++) { archive.push_back(v & 0xFF); v >>= 8; }
    };
    auto write_u64 = [&](uint64_t v) {
        for (int i = 0; i < 8; i++) { archive.push_back(v & 0xFF); v >>= 8; }
    };

    // Write file count
    write_u32(static_cast<uint32_t>(files.size()));

    // Write per-file headers
    uint64_t offset = 0;
    for (auto& f : files) {
        write_u16(static_cast<uint16_t>(f.name.size()));
        archive.insert(archive.end(), f.name.begin(), f.name.end());
        write_u64(static_cast<uint64_t>(f.data.size()));
        write_u64(offset);
        offset += f.data.size();
    }

    // Write concatenated file data
    for (auto& f : files)
        archive.insert(archive.end(), f.data.begin(), f.data.end());

    return archive;
}

// ---------------------------------------------------------------------------
// pack  --  create a packed installer exe
//   Output layout:
//     [original exe bytes]
//     [LZMA-compressed archive]
//     uint64_t archive_uncompressed_size
//     [LZMA-compressed string table]
//     SiFooter { archive metadata, string metadata }
// ---------------------------------------------------------------------------
static void pack(const char* exe_path, const char* output_path,
                 const std::vector<std::string>& file_paths,
                 std::vector<LocaleInput>& locales)
{
    crc32_init();

    // Collect files
    std::vector<FileEntry> entries;
    size_t total_raw = 0;
    for (auto& path : file_paths) {
        FileEntry e;
        // Extract just the filename (strip directory)
        size_t slash = path.find_last_of("/\\");
        e.name = (slash != std::string::npos) ? path.substr(slash + 1) : path;
        e.data = read_file(path.c_str());
        total_raw += e.data.size();
        printf("  + %-40s %zu bytes\n", e.name.c_str(), e.data.size());
        entries.push_back(std::move(e));
    }

    // Build string table from locales — compressed separately from the file archive
    std::vector<uint8_t> strings_blob;
    std::vector<uint8_t> strings_compressed;
    if (!locales.empty()) {
        printf("Building string table from %zu locale(s)...\n", locales.size());
        strings_blob = build_string_table(locales);
        if (strings_blob.size() > 0xFFFFFFFFu) {
            fprintf(stderr, "String table is too large: %zu bytes\n", strings_blob.size());
            exit(1);
        }
        strings_compressed = lzma_compress(strings_blob.data(), strings_blob.size(), 1u << 20);
        if (strings_compressed.size() > 0xFFFFFFFFu) {
            fprintf(stderr, "Compressed string table is too large: %zu bytes\n", strings_compressed.size());
            exit(1);
        }
        printf("  + %-40s %zu -> %zu bytes (string table, compressed %.1f%%)\n",
               "__si_strings", strings_blob.size(), strings_compressed.size(),
               100.0 * strings_compressed.size() / strings_blob.size());
    }

    // Build archive (files only — no __si_strings)
    std::vector<uint8_t> archive = build_archive(entries);
    printf("Archive: %zu bytes (%zu files, %zu bytes raw data)\n",
           archive.size(), entries.size(), total_raw);

    // Compress
    std::vector<uint8_t> compressed = lzma_compress(archive.data(), archive.size());
    printf("Compressed: %zu -> %zu bytes (%.1f%%)\n",
           archive.size(), compressed.size(),
           100.0 * compressed.size() / archive.size());

    // Read original exe
    std::vector<uint8_t> exe = read_file(exe_path);
    printf("Exe stub: %zu bytes\n", exe.size());

    // Build footer
    uint64_t uncompressed_size = archive.size();
    SiFooter footer = {};
    memcpy(footer.magic, SI_MAGIC, 4);
    footer.archive_offset = exe.size();
    footer.archive_size   = compressed.size();
    footer.checksum       = crc32(compressed.data(), compressed.size());
    // Strings blob sits immediately after: exe + compressed + uncompressed_size(8)
    footer.strings_offset = strings_compressed.empty() ? 0
        : (uint64_t)(exe.size() + compressed.size() + sizeof(uint64_t));
    footer.strings_size   = (uint32_t)strings_compressed.size();
    footer.strings_uncompressed_size = (uint32_t)strings_blob.size();
    footer.strings_checksum = strings_compressed.empty()
        ? 0
        : crc32(strings_compressed.data(), strings_compressed.size());

    // Write output
    FILE* out = fopen(output_path, "wb");
    if (!out) { fprintf(stderr, "Cannot create: %s\n", output_path); exit(1); }

    fwrite(exe.data(), 1, exe.size(), out);
    fwrite(compressed.data(), 1, compressed.size(), out);
    fwrite(&uncompressed_size, sizeof(uncompressed_size), 1, out);
    if (!strings_compressed.empty())
        fwrite(strings_compressed.data(), 1, strings_compressed.size(), out);
    fwrite(&footer, sizeof(footer), 1, out);
    fclose(out);

    size_t total_size = exe.size() + compressed.size() + sizeof(uncompressed_size)
                        + strings_compressed.size() + sizeof(footer);
    printf("Output: %s (%zu bytes)\n", output_path, total_size);
    printf("  exe stub:    %8zu bytes\n", exe.size());
    printf("  archive:     %8zu bytes (compressed)\n", compressed.size());
    printf("  strings:     %8zu bytes (compressed, %zu raw)\n",
           strings_compressed.size(), strings_blob.size());
    printf("  metadata:    %8zu bytes\n", sizeof(uncompressed_size) + sizeof(footer));
    printf("  CRC32:       0x%08X\n", footer.checksum);
}

// ---------------------------------------------------------------------------
// lzma_compress  --  compress data using LZMA
//   Returns: 5-byte props header + compressed payload
// ---------------------------------------------------------------------------
static std::vector<uint8_t> lzma_compress(const uint8_t* data, size_t size, uint32_t dict_size)
{
    // Set up encoder properties
    CLzmaEncProps props;
    LzmaEncProps_Init(&props);
    props.level    = 9;
    props.dictSize = dict_size;
    props.fb       = 273;      // max fast bytes (better ratio on executables)
    props.numThreads = 2;

    // Worst-case output size: input size + some overhead
    SizeT destLen  = size + size / 3 + 128;
    std::vector<uint8_t> dest(destLen);

    // Props header (5 bytes)
    Byte propsEncoded[LZMA_PROPS_SIZE];
    SizeT propsSize = LZMA_PROPS_SIZE;

    SRes res = LzmaEncode(
        dest.data(), &destLen,
        data, size,
        &props, propsEncoded, &propsSize,
        0,          // writeEndMark = 0 (we know the original size)
        nullptr,    // progress callback
        &g_Alloc,   // alloc
        &g_Alloc    // allocBig
    );

    if (res != SZ_OK) {
        fprintf(stderr, "LZMA compression failed (error %d)\n", res);
        exit(1);
    }

    // Build output: 5-byte props + compressed data
    std::vector<uint8_t> out;
    out.reserve(LZMA_PROPS_SIZE + destLen);
    out.insert(out.end(), propsEncoded, propsEncoded + LZMA_PROPS_SIZE);
    out.insert(out.end(), dest.data(), dest.data() + destLen);

    return out;
}

// ---------------------------------------------------------------------------
// lzma_decompress  --  decompress LZMA data
//   Input layout: 5-byte props header + compressed payload
//   origSize: expected decompressed size
// ---------------------------------------------------------------------------
static std::vector<uint8_t> lzma_decompress(const uint8_t* data, size_t compSize, size_t origSize)
{
    if (compSize < LZMA_PROPS_SIZE) {
        fprintf(stderr, "LZMA decompress: input too small for props header\n");
        exit(1);
    }

    std::vector<uint8_t> out(origSize);

    const Byte* propsData    = data;
    const Byte* compData     = data + LZMA_PROPS_SIZE;
    SizeT       compDataLen  = compSize - LZMA_PROPS_SIZE;
    SizeT       destLen      = origSize;
    ELzmaStatus status;

    SRes res = LzmaDecode(
        out.data(), &destLen,
        compData, &compDataLen,
        propsData, LZMA_PROPS_SIZE,
        LZMA_FINISH_END,
        &status,
        &g_Alloc
    );

    if (res != SZ_OK) {
        fprintf(stderr, "LZMA decompression failed (error %d)\n", res);
        exit(1);
    }

    if (destLen != origSize) {
        fprintf(stderr, "LZMA decompress: size mismatch (got %zu, expected %zu)\n",
                (size_t)destLen, origSize);
        exit(1);
    }

    return out;
}

// ---------------------------------------------------------------------------
// self_test  --  round-trip compress/decompress verification
// ---------------------------------------------------------------------------
static void self_test()
{
    const char* testStr = "Hello, Small Installer! This is a compression test.";
    size_t testLen = strlen(testStr);

    printf("Self-test: compressing %zu bytes...\n", testLen);

    std::vector<uint8_t> input(reinterpret_cast<const uint8_t*>(testStr),
                               reinterpret_cast<const uint8_t*>(testStr) + testLen);
    std::vector<uint8_t> compressed = lzma_compress(input.data(), input.size());
    printf("  Compressed: %zu -> %zu bytes\n", testLen, compressed.size());

    std::vector<uint8_t> decompressed = lzma_decompress(
        compressed.data(), compressed.size(), testLen);

    // Verify round-trip
    assert(decompressed.size() == testLen);
    assert(memcmp(decompressed.data(), testStr, testLen) == 0);

    printf("  Round-trip OK: %zu -> %zu -> %zu bytes\n",
           testLen, compressed.size(), decompressed.size());
    printf("Self-test PASSED.\n");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    if (argc >= 2 && strcmp(argv[1], "--self-test") == 0) {
        self_test();
        return 0;
    }

    // Parse: si_pack <input.exe> -o <output.exe> --add <file> [--add <file>...]
    //        [--locale <code> <json_path>...]
    const char* input_exe = nullptr;
    const char* output_exe = nullptr;
    std::vector<std::string> add_files;
    std::vector<LocaleInput> locales;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_exe = argv[++i];
        } else if (strcmp(argv[i], "--add") == 0 && i + 1 < argc) {
            add_files.push_back(argv[++i]);
        } else if (strcmp(argv[i], "--locale") == 0 && i + 2 < argc) {
            LocaleInput loc;
            loc.code = argv[++i];
            loc.file_path = argv[++i];
            locales.push_back(std::move(loc));
        } else if (!input_exe) {
            input_exe = argv[i];
        } else {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            return 1;
        }
    }

    if (!input_exe || !output_exe || (add_files.empty() && locales.empty())) {
        printf("si_pack: Small Installer Packer\n");
        printf("Usage:\n");
        printf("  si_pack <input.exe> -o <output.exe> --add <file> [--add <file>...]\n");
        printf("         [--locale <code> <file_path>...]\n");
        printf("  si_pack --self-test\n");
        printf("\nOptions:\n");
        printf("  --add <file>                 Add a file to the archive\n");
        printf("  --locale <code> <file_path>  Add a locale (e.g., --locale en locales/en.txt)\n");
        printf("                               Key=value text format, one pair per line\n");
        return 1;
    }

    printf("si_pack: packing %zu file(s)%s into %s\n",
           add_files.size(),
           locales.empty() ? "" : " + locale string table",
           output_exe);
    pack(input_exe, output_exe, add_files, locales);
    printf("Done.\n");
    return 0;
}
