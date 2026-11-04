/* si_core.h — CRT stubs, CRC32, payload, I18N, DPI, color schemes, wizard state,
 * registry, shortcut/protocol/process helpers, download, context implementation
 * Included from small_installer.h under #ifdef SI_IMPLEMENTATION — no include guard.
 */

/* ── CRT STUBS ──────────────────────────────────────────────────────────── */

#ifdef SI_NO_CRT

static HANDLE si__heap_handle(void) {
    static HANDLE h = NULL;
    if (!h) h = GetProcessHeap();
    return h;
}

void* si_memcpy(void* dst, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    while (n--) *d++ = *s++;
    return dst;
}

void* si_memset(void* dst, int val, size_t n) {
    unsigned char* d = (unsigned char*)dst;
    while (n--) *d++ = (unsigned char)val;
    return dst;
}

size_t si_wcslen(const wchar_t* s) {
    const wchar_t* p = s;
    while (*p) p++;
    return (size_t)(p - s);
}

int si_wcsicmp(const wchar_t* a, const wchar_t* b) {
    while (*a && *b) {
        wchar_t ca = *a, cb = *b;
        if (ca >= L'A' && ca <= L'Z') ca += 32;
        if (cb >= L'A' && cb <= L'Z') cb += 32;
        if (ca != cb) return (int)ca - (int)cb;
        a++; b++;
    }
    return (int)*a - (int)*b;
}

int si_wcscmp(const wchar_t* a, const wchar_t* b) {
    while (*a && *a == *b) { a++; b++; }
    return (int)*a - (int)*b;
}

wchar_t* si_wcscpy(wchar_t* dst, const wchar_t* src) {
    wchar_t* d = dst;
    while ((*d++ = *src++)) {}
    return dst;
}

wchar_t* si_wcscat(wchar_t* dst, const wchar_t* src) {
    wchar_t* d = dst;
    while (*d) d++;
    while ((*d++ = *src++)) {}
    return dst;
}

wchar_t* si_wcsrchr(const wchar_t* s, wchar_t c) {
    const wchar_t* last = NULL;
    while (*s) { if (*s == c) last = s; s++; }
    return (wchar_t*)last;
}

wchar_t* si_wcsstr(const wchar_t* hay, const wchar_t* needle) {
    const wchar_t* h; const wchar_t* n;
    if (!*needle) return (wchar_t*)hay;
    for (; *hay; hay++) {
        h = hay; n = needle;
        while (*h && *n && *h == *n) { h++; n++; }
        if (!*n) return (wchar_t*)hay;
    }
    return NULL;
}

wchar_t* si_wcsncpy(wchar_t* dst, const wchar_t* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i]; i++) dst[i] = src[i];
    for (; i < n; i++) dst[i] = L'\0';
    return dst;
}

int si_strcmp(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

void* si_malloc(size_t size) {
    return HeapAlloc(si__heap_handle(), 0, size);
}
void* si_realloc(void* ptr, size_t size) {
    if (!ptr) return HeapAlloc(si__heap_handle(), 0, size);
    return HeapReAlloc(si__heap_handle(), 0, ptr, size);
}
void si_free(void* ptr) {
    if (ptr) HeapFree(si__heap_handle(), 0, ptr);
}

/* Provide actual memcpy/memset symbols for compiler-generated calls and LZMA.
   We temporarily undef the macros so we can define the real functions. */
#pragma push_macro("memcpy")
#pragma push_macro("memset")
#undef memcpy
#undef memset

#pragma function(memcpy, memset)
void* memcpy(void* dst, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    while (n--) *d++ = *s++;
    return dst;
}
void* memset(void* dst, int val, size_t n) {
    unsigned char* d = (unsigned char*)dst;
    while (n--) *d++ = (unsigned char)val;
    return dst;
}

#pragma pop_macro("memcpy")
#pragma pop_macro("memset")

/* Floating-point used marker */
int _fltused = 0;

/* Thread-local storage index */
unsigned long _tls_index = 0;

#if defined(_MSC_VER)
void __cdecl __security_init_cookie(void);
#endif

void WINAPI WinMainCRTStartup(void) {
#if defined(_MSC_VER)
    /* /GS remains enabled in the no-CRT installer. Initialize the static
       vcruntime cookie before entering any protected application function. */
    __security_init_cookie();
#endif
    HINSTANCE hInst = GetModuleHandleW(NULL);
    int ret = si_main(hInst);
    ExitProcess((UINT)ret);
}

#else /* !SI_NO_CRT — normal CRT build */

#include <stdlib.h>

void* si_malloc(size_t size) { return malloc(size); }
void* si_realloc(void* ptr, size_t size) { return realloc(ptr, size); }
void  si_free(void* ptr) { free(ptr); }

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrev; (void)lpCmdLine; (void)nCmdShow;
    return si_main(hInst);
}

#endif /* SI_NO_CRT */

/* ── CRC32 ──────────────────────────────────────────────────────────────── */

static uint32_t si_crc32_table[256];
static int si_crc32_inited = 0;

static void si_crc32_init(void) {
    uint32_t i, j, c;
    if (si_crc32_inited) return;
    for (i = 0; i < 256; i++) {
        c = i;
        for (j = 0; j < 8; j++)
            c = (c >> 1) ^ (c & 1 ? 0xEDB88320u : 0u);
        si_crc32_table[i] = c;
    }
    si_crc32_inited = 1;
}

static uint32_t si_crc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    size_t i;
    si_crc32_init();
    for (i = 0; i < len; i++)
        crc = si_crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFF;
}

#ifdef SI_HAS_PAYLOAD
#include "si_transaction.h"

/* ── LZMA alloc wrapper ─────────────────────────────────────────────────── */

static void* SiLzmaAlloc(ISzAllocPtr p, size_t size) { (void)p; return si_malloc(size); }
static void  SiLzmaFree(ISzAllocPtr p, void* ptr)    { (void)p; si_free(ptr); }
static const ISzAlloc g_SiAlloc = { SiLzmaAlloc, SiLzmaFree };

/* ── SiFooter ───────────────────────────────────────────────────────────── */

#pragma pack(push, 1)
typedef struct SiFooter {
    char     magic[4];
    uint64_t archive_offset;
    uint64_t archive_size;
    uint32_t checksum;
    uint64_t strings_offset;  /* compressed strings blob offset in exe (0 = absent) */
    uint32_t strings_size;    /* compressed strings blob size in bytes */
    uint32_t strings_uncompressed_size;
    uint32_t strings_checksum;
} SiFooter;
#pragma pack(pop)

static int si_footer_magic_ok(const SiFooter* footer) {
    return footer->magic[0]=='S' && footer->magic[1]=='I' &&
           footer->magic[2]=='P' && footer->magic[3]=='2';
}

/* Authenticode signing appends a WIN_CERTIFICATE blob after the executable's
   existing bytes. si_pack stores the payload footer before that signature, so a
   signed installer must scan backward instead of assuming the footer is at EOF. */
static int si_find_payload_footer(HANDLE hFile, uint64_t fsize,
                                  SiFooter* footer, uint64_t* footer_offset) {
    enum { SI_FOOTER_SCAN_MAX = 16 * 1024 * 1024 };
    uint64_t scan_size64;
    DWORD scan_size, br = 0;
    uint64_t scan_start;
    uint8_t* buf;
    int64_t i;

    if (fsize < sizeof(SiFooter) + 8) return 0;
    scan_size64 = fsize;
    if (scan_size64 > SI_FOOTER_SCAN_MAX) scan_size64 = SI_FOOTER_SCAN_MAX;
    scan_size = (DWORD)scan_size64;
    scan_start = fsize - scan_size64;

    buf = (uint8_t*)si_malloc(scan_size);
    if (!buf) return 0;
    { LARGE_INTEGER sp; sp.QuadPart = (LONGLONG)scan_start;
      if (!SetFilePointerEx(hFile, sp, NULL, FILE_BEGIN) ||
          !ReadFile(hFile, buf, scan_size, &br, NULL) || br != scan_size)
          { si_free(buf); return 0; } }

    for (i = (int64_t)scan_size - (int64_t)sizeof(SiFooter); i >= 0; i--) {
        SiFooter cand;
        uint64_t off, end;
        if (buf[i] != 'S' || buf[i+1] != 'I' || buf[i+2] != 'P' || buf[i+3] != '2')
            continue;
        memcpy(&cand, buf + i, sizeof(cand));
        if (!si_footer_magic_ok(&cand)) continue;

        off = scan_start + (uint64_t)i;
        if (cand.archive_size < LZMA_PROPS_SIZE) continue;
        if (cand.archive_offset > off || cand.archive_size > off - cand.archive_offset)
            continue;
        end = cand.archive_offset + cand.archive_size + 8;
        if (end > off) continue;
        if (cand.strings_size > 0) {
            if (cand.strings_offset != end) continue;
            if (cand.strings_uncompressed_size == 0) continue;
            end += cand.strings_size;
        } else if (cand.strings_offset != 0 || cand.strings_uncompressed_size != 0) {
            continue;
        }
        if (end != off) continue;

        *footer = cand;
        if (footer_offset) *footer_offset = off;
        si_free(buf);
        return 1;
    }

    si_free(buf);
    return 0;
}
#endif /* SI_HAS_PAYLOAD */

/* ── Byte readers ───────────────────────────────────────────────────────── */

static uint16_t si_rd16(const uint8_t* p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }
static uint32_t si_rd32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint64_t si_rd64(const uint8_t* p) { return (uint64_t)si_rd32(p) | ((uint64_t)si_rd32(p+4) << 32); }

/* ── High-resolution monotonic time ─────────────────────────────────────────
 * QueryPerformanceCounter-based clock — sub-millisecond resolution and stable
 * frame-to-frame, unlike GetTickCount (15.6ms granularity by default). Used
 * by entrance/transition animations so their timing is independent of the
 * system timer interval. Returns milliseconds since first call. */
static LARGE_INTEGER g_si_qpc_freq = { 0 };
static uint64_t si_now_ms(void) {
    LARGE_INTEGER c;
    if (g_si_qpc_freq.QuadPart == 0) QueryPerformanceFrequency(&g_si_qpc_freq);
    QueryPerformanceCounter(&c);
    return (uint64_t)((c.QuadPart * 1000ull) / (uint64_t)g_si_qpc_freq.QuadPart);
}

/* Display refresh interval in ms, queried once via EnumDisplaySettingsW.
 * 60Hz→16, 120Hz→8, 144Hz→7, 240Hz→4. Defaults to 16 on failure. Used to
 * pace SetTimer-driven animations so each tick aligns with one displayed
 * frame; pair with DwmFlush() in WM_PAINT to anchor that pacing to vblank. */
static UINT g_si_refresh_ms = 0;
static UINT si_display_refresh_ms(void) {
    DEVMODEW dm;
    if (g_si_refresh_ms != 0) return g_si_refresh_ms;
    g_si_refresh_ms = 16;
    memset(&dm, 0, sizeof(dm));
    dm.dmSize = sizeof(dm);
    if (EnumDisplaySettingsW(NULL, ENUM_CURRENT_SETTINGS, &dm) && dm.dmDisplayFrequency > 1) {
        UINT ms = 1000u / dm.dmDisplayFrequency;
        if (ms < 4) ms = 4;       /* clamp to USER timer floor */
        if (ms > 33) ms = 33;     /* sanity cap (sub-30Hz unlikely) */
        g_si_refresh_ms = ms;
    }
    return g_si_refresh_ms;
}

#ifdef SI_HAS_PAYLOAD
/* ── PAYLOAD READER ─────────────────────────────────────────────────────── */

typedef struct SiArchiveFile { char name[260]; const uint8_t* data; size_t size; } SiArchiveFile;
typedef struct SiPayloadEntry { char name[260]; uint64_t size; uint64_t offset; } SiPayloadEntry;
typedef struct SiPayloadReader {
    uint8_t* archive; size_t archive_size;
    SiPayloadEntry* entries; int entry_count;
    const uint8_t* data_start;
} SiPayloadReader;

static void si_payload_init(SiPayloadReader* r) { memset(r, 0, sizeof(*r)); }
static void si_payload_free(SiPayloadReader* r) {
    if (r->archive) { si_free(r->archive); r->archive = NULL; }
    if (r->entries) { si_free(r->entries); r->entries = NULL; }
    r->entry_count = 0; r->data_start = NULL;
}

/* Read the compressed strings blob without decompressing the file archive.
   Caller must si_free(*out_data) when done. */
static int si_strings_open(const uint8_t** out_data, uint32_t* out_size) {
    wchar_t exe_path[MAX_PATH]; HANDLE hFile; LARGE_INTEGER fs;
    uint64_t fsize, footer_offset; SiFooter footer; DWORD len, br;
    uint8_t* compressed = NULL; uint8_t* buf = NULL;
    *out_data = NULL; *out_size = 0;
    len = GetModuleFileNameW(NULL, exe_path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return 0;
    hFile = CreateFileW(exe_path, GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return 0;
    if (!GetFileSizeEx(hFile, &fs)) { CloseHandle(hFile); return 0; }
    fsize = (uint64_t)fs.QuadPart;
    if (!si_find_payload_footer(hFile, fsize, &footer, &footer_offset))
        { CloseHandle(hFile); return 0; }
    if (footer.strings_size < LZMA_PROPS_SIZE ||
        footer.strings_uncompressed_size == 0 || footer.strings_offset == 0 ||
        footer.strings_offset > footer_offset ||
        (uint64_t)footer.strings_size > footer_offset - footer.strings_offset ||
        footer.strings_offset + footer.strings_size > footer_offset)
        { CloseHandle(hFile); return 0; }
    compressed = (uint8_t*)si_malloc(footer.strings_size);
    if (!compressed) { CloseHandle(hFile); return 0; }
    { LARGE_INTEGER sp; sp.QuadPart = (LONGLONG)footer.strings_offset; br = 0;
      if (!SetFilePointerEx(hFile, sp, NULL, FILE_BEGIN)
          || !ReadFile(hFile, compressed, footer.strings_size, &br, NULL)
          || br != footer.strings_size) { si_free(compressed); CloseHandle(hFile); return 0; } }
    CloseHandle(hFile);
    if (si_crc32(compressed, footer.strings_size) != footer.strings_checksum)
        { si_free(compressed); return 0; }
    buf = (uint8_t*)si_malloc(footer.strings_uncompressed_size);
    if (!buf) { si_free(compressed); return 0; }
    { CLzmaDec dec; int ok = 0;
      const uint8_t* in_p = compressed + LZMA_PROPS_SIZE;
      SizeT in_rem = (SizeT)(footer.strings_size - LZMA_PROPS_SIZE);
      uint8_t* out_p = buf; uint32_t out_done = 0;
      LzmaDec_Construct(&dec);
      if (LzmaDec_Allocate(&dec, compressed, LZMA_PROPS_SIZE, &g_SiAlloc) == SZ_OK) {
          LzmaDec_Init(&dec);
          while (out_done < footer.strings_uncompressed_size) {
              SizeT ochunk = footer.strings_uncompressed_size - out_done;
              SizeT ichunk = in_rem; ELzmaStatus st;
              if (LzmaDec_DecodeToBuf(&dec, out_p, &ochunk, in_p, &ichunk,
                                      LZMA_FINISH_ANY, &st) != SZ_OK) break;
              in_p += ichunk; in_rem -= ichunk;
              out_p += ochunk; out_done += (uint32_t)ochunk;
              if (ochunk == 0 && ichunk == 0) break;
              if (st == LZMA_STATUS_FINISHED_WITH_MARK ||
                  st == LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK) { ok = 1; break; }
          }
          if (out_done == footer.strings_uncompressed_size) ok = 1;
          LzmaDec_Free(&dec, &g_SiAlloc);
      }
      si_free(compressed);
      if (!ok) { si_free(buf); return 0; }
    }
    *out_data = buf; *out_size = footer.strings_uncompressed_size;
    return 1;
}

/* Read the uncompressed payload size (sum of all extracted files + header)
 * without decompressing the LZMA archive. Returns 1 on success, 0 on failure. */
static int si_payload_get_uncompressed_size(uint64_t* out) {
    wchar_t exe_path[MAX_PATH]; HANDLE hFile; LARGE_INTEGER fs;
    uint64_t fsize, usize; SiFooter footer; DWORD len, br;
    *out = 0;
    len = GetModuleFileNameW(NULL, exe_path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return 0;
    hFile = CreateFileW(exe_path, GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return 0;
    if (!GetFileSizeEx(hFile, &fs)) { CloseHandle(hFile); return 0; }
    fsize = (uint64_t)fs.QuadPart;
    if (!si_find_payload_footer(hFile, fsize, &footer, NULL))
        { CloseHandle(hFile); return 0; }
    { LARGE_INTEGER sp; sp.QuadPart = (LONGLONG)(footer.archive_offset + footer.archive_size); br = 0;
      if (!SetFilePointerEx(hFile, sp, NULL, FILE_BEGIN)
          || !ReadFile(hFile, &usize, 8, &br, NULL) || br != 8)
          { CloseHandle(hFile); return 0; } }
    CloseHandle(hFile);
    *out = usize;
    return 1;
}

/* pfn(0..35) called during LZMA decompression; pass NULL for no progress. */
static int si_payload_open_p(SiPayloadReader* r, void(*pfn)(int)) {
    wchar_t exe_path[MAX_PATH]; HANDLE hFile; LARGE_INTEGER fileSize;
    uint64_t fsize, needed, uncompressed_size, footer_offset; SiFooter footer;
    uint8_t* compressed; uint32_t file_cnt, i;
    const uint8_t *ptr, *end;
    DWORD len;

    si_payload_init(r);
    len = GetModuleFileNameW(NULL, exe_path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return 0;

    hFile = CreateFileW(exe_path, GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return 0;
    if (!GetFileSizeEx(hFile, &fileSize)) { CloseHandle(hFile); return 0; }
    fsize = (uint64_t)fileSize.QuadPart;
    if (!si_find_payload_footer(hFile, fsize, &footer, &footer_offset))
        { CloseHandle(hFile); return 0; }
    needed = footer.archive_offset + footer.archive_size + 8 + footer.strings_size + sizeof(SiFooter);
    if (needed != footer_offset + sizeof(SiFooter)) { CloseHandle(hFile); return 0; }

    compressed = (uint8_t*)si_malloc((size_t)footer.archive_size);
    if (!compressed) { CloseHandle(hFile); return 0; }

    { LARGE_INTEGER sp; DWORD br = 0; sp.QuadPart = (LONGLONG)footer.archive_offset;
      if (!SetFilePointerEx(hFile, sp, NULL, FILE_BEGIN)) { si_free(compressed); CloseHandle(hFile); return 0; }
      if (!ReadFile(hFile, compressed, (DWORD)footer.archive_size, &br, NULL) ||
          br != (DWORD)footer.archive_size) { si_free(compressed); CloseHandle(hFile); return 0; } }

    { LARGE_INTEGER sp; DWORD br = 0; sp.QuadPart = (LONGLONG)(footer.archive_offset + footer.archive_size);
      if (!SetFilePointerEx(hFile, sp, NULL, FILE_BEGIN)) { si_free(compressed); CloseHandle(hFile); return 0; }
      if (!ReadFile(hFile, &uncompressed_size, 8, &br, NULL) || br != 8)
          { si_free(compressed); CloseHandle(hFile); return 0; } }
    CloseHandle(hFile);

    if (si_crc32(compressed, (size_t)footer.archive_size) != footer.checksum)
        { si_free(compressed); return 0; }
    if (footer.archive_size < LZMA_PROPS_SIZE) { si_free(compressed); return 0; }

    r->archive = (uint8_t*)si_malloc((size_t)uncompressed_size);
    if (!r->archive) { si_free(compressed); return 0; }
    r->archive_size = (size_t)uncompressed_size;
    { CLzmaDec dec; int ok = 0;
      const uint8_t* in_p = compressed + LZMA_PROPS_SIZE;
      SizeT in_rem = (SizeT)(footer.archive_size - LZMA_PROPS_SIZE);
      uint8_t* out_p = r->archive; uint64_t out_done = 0;
      LzmaDec_Construct(&dec);
      if (LzmaDec_Allocate(&dec, compressed, LZMA_PROPS_SIZE, &g_SiAlloc) == SZ_OK) {
          LzmaDec_Init(&dec);
          while (out_done < uncompressed_size) {
              SizeT ochunk = (uncompressed_size - out_done > 262144) ? 262144
                             : (SizeT)(uncompressed_size - out_done);
              SizeT ichunk = in_rem; ELzmaStatus st;
              if (LzmaDec_DecodeToBuf(&dec, out_p, &ochunk, in_p, &ichunk,
                                      LZMA_FINISH_ANY, &st) != SZ_OK) break;
              in_p += ichunk; in_rem -= ichunk;
              out_p += ochunk; out_done += (uint64_t)ochunk;
              if (pfn && uncompressed_size > 0)
                  pfn((int)((uint64_t)35 * out_done / uncompressed_size));
              if (ochunk == 0 && ichunk == 0) break;
              if (st == LZMA_STATUS_FINISHED_WITH_MARK ||
                  st == LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK) { ok = 1; break; }
          }
          if (out_done == uncompressed_size) ok = 1;
          LzmaDec_Free(&dec, &g_SiAlloc);
      }
      si_free(compressed);
      if (!ok) { si_payload_free(r); return 0; }
    }

    ptr = r->archive; end = r->archive + r->archive_size;
    if (ptr + 4 > end) { si_payload_free(r); return 0; }
    file_cnt = si_rd32(ptr); ptr += 4;
    r->entries = (SiPayloadEntry*)si_malloc(file_cnt * sizeof(SiPayloadEntry));
    if (!r->entries) { si_payload_free(r); return 0; }
    r->entry_count = (int)file_cnt;

    for (i = 0; i < file_cnt; i++) {
        uint16_t nlen;
        if (ptr + 2 > end) { si_payload_free(r); return 0; }
        nlen = si_rd16(ptr); ptr += 2;
        if (ptr + nlen > end || nlen >= 260) { si_payload_free(r); return 0; }
        memcpy(r->entries[i].name, ptr, nlen);
        r->entries[i].name[nlen] = '\0'; ptr += nlen;
        if (ptr + 16 > end) { si_payload_free(r); return 0; }
        r->entries[i].size = si_rd64(ptr); ptr += 8;
        r->entries[i].offset = si_rd64(ptr); ptr += 8;
    }
    r->data_start = ptr;
    return 1;
}
static int si_payload_open(SiPayloadReader* r) { return si_payload_open_p(r, NULL); }

static int si_payload_find(const SiPayloadReader* r, const char* name, SiArchiveFile* out) {
    int i;
    for (i = 0; i < r->entry_count; i++) {
        if (strcmp(r->entries[i].name, name) == 0) {
            memcpy(out->name, r->entries[i].name, 260);
            out->data = r->data_start + r->entries[i].offset;
            out->size = (size_t)r->entries[i].size;
            return 1;
        }
    }
    return 0;
}

static void si_append_wide(wchar_t* dest, size_t capacity, const wchar_t* text) {
    size_t used = wcslen(dest);
    if (!text || used >= capacity) return;
    while (*text && used + 1 < capacity) dest[used++] = *text++;
    dest[used] = L'\0';
}

static int si_prompt_replace_retry(HWND owner, const wchar_t* path, DWORD error) {
    wchar_t message[1024], system_error[256];
    DWORD system_error_len;

    message[0] = L'\0';
    system_error[0] = L'\0';
    si_append_wide(message, 1024, L(L"Setup could not replace this file:"));
    si_append_wide(message, 1024, L"\r\n\r\n");
    si_append_wide(message, 1024, path);
    system_error_len = FormatMessageW(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, error, 0, system_error, 256, NULL);
    while (system_error_len > 0 &&
           (system_error[system_error_len - 1] == L'\r' ||
            system_error[system_error_len - 1] == L'\n')) {
        system_error[--system_error_len] = L'\0';
    }
    if (system_error_len > 0) {
        si_append_wide(message, 1024, L"\r\n\r\n");
        si_append_wide(message, 1024, system_error);
    }
    si_append_wide(message, 1024, L"\r\n\r\n");
    si_append_wide(message, 1024,
        L(L"Close any program using this file, then click Retry. Click Cancel to stop setup."));
    return MessageBoxW(owner, message, L(L"Installation blocked"),
        MB_RETRYCANCEL | MB_ICONWARNING | MB_SETFOREGROUND);
}

typedef struct SiPayloadCommitPrompt {
    HWND owner;
    int interactive;
    int* cancelled;
} SiPayloadCommitPrompt;

static int si_payload_commit_retry(void* context, const wchar_t* path, DWORD error) {
    SiPayloadCommitPrompt* prompt = (SiPayloadCommitPrompt*)context;
    if (!prompt || !prompt->interactive) return 0;
    if (si_prompt_replace_retry(prompt->owner, path, error) == IDRETRY) return 1;
    if (prompt->cancelled) *prompt->cancelled = 1;
    return 0;
}

static int si_payload_stage_entries(SiInstallTransaction* transaction,
                                    SiPayloadEntry* entries, uint32_t file_count,
                                    int** transaction_indices_out) {
    int* indices;
    uint32_t i;
    indices = (int*)si_malloc((size_t)file_count * sizeof(int));
    if (!indices && file_count != 0) return 0;
    for (i = 0; i < file_count; ++i) indices[i] = -1;
    for (i = 0; i < file_count; ++i) {
        SiPayloadEntry* entry = &entries[i];
        wchar_t staged[MAX_PATH], wide_name[260];
        HANDLE file;
        int wide_length;
        if (entry->name[0] == '_' && entry->name[1] == '_') continue;
        wide_length = MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, entry->name, -1, wide_name, 260);
        if (wide_length <= 0) {
            if (indices) si_free(indices);
            return 0;
        }
        indices[i] = transaction->file_count;
        if (!si_txn_add_file(transaction, wide_name, staged)) {
            if (indices) si_free(indices);
            return 0;
        }
        /* Create every staged file up front so zero-byte payload entries are
           committed too; streaming later only opens these files for writes. */
        file = CreateFileW(staged, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                           FILE_ATTRIBUTE_NORMAL, NULL);
        if (file == INVALID_HANDLE_VALUE) {
            if (indices) si_free(indices);
            return 0;
        }
        CloseHandle(file);
    }
    *transaction_indices_out = indices;
    return 1;
}

/* Stream-decompress all files from the LZMA payload into same-volume staging,
   then transactionally publish the complete set. pfn(0..100) tracks progress.
   Interactive installs let the user retry a destination file that cannot be
   replaced. Cancel and every extraction/commit failure restore the target
   directory to its pre-extraction state. Returns 1 on success, 0 on failure. */
static int si_payload_extract_stream(const wchar_t* dest_dir, void(*pfn)(int),
                                     HWND owner, int interactive, int clean_install,
                                     int* cancelled) {
    wchar_t exe_path[MAX_PATH]; HANDLE hFile; LARGE_INTEGER fileSize;
    uint64_t fsize, uncompressed_size, footer_offset; SiFooter footer;
    uint8_t* compressed = NULL; DWORD len; int result = 0;
    SiInstallTransaction transaction;
    int transaction_active = 0;

    if (cancelled) *cancelled = 0;
    len = GetModuleFileNameW(NULL, exe_path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return 0;
    hFile = CreateFileW(exe_path, GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return 0;
    if (!GetFileSizeEx(hFile, &fileSize)) { CloseHandle(hFile); return 0; }
    fsize = (uint64_t)fileSize.QuadPart;
    if (!si_find_payload_footer(hFile, fsize, &footer, &footer_offset))
        { CloseHandle(hFile); return 0; }
    if (footer.archive_offset + footer.archive_size + 8 + footer.strings_size + sizeof(SiFooter) !=
        footer_offset + sizeof(SiFooter))
        { CloseHandle(hFile); return 0; }
    compressed = (uint8_t*)si_malloc((size_t)footer.archive_size);
    if (!compressed) { CloseHandle(hFile); return 0; }
    { LARGE_INTEGER sp; DWORD br = 0; sp.QuadPart = (LONGLONG)footer.archive_offset;
      if (!SetFilePointerEx(hFile, sp, NULL, FILE_BEGIN))
          { si_free(compressed); CloseHandle(hFile); return 0; }
      if (!ReadFile(hFile, compressed, (DWORD)footer.archive_size, &br, NULL) ||
          br != (DWORD)footer.archive_size)
          { si_free(compressed); CloseHandle(hFile); return 0; } }
    { LARGE_INTEGER sp; DWORD br = 0;
      sp.QuadPart = (LONGLONG)(footer.archive_offset + footer.archive_size);
      if (!SetFilePointerEx(hFile, sp, NULL, FILE_BEGIN))
          { si_free(compressed); CloseHandle(hFile); return 0; }
      if (!ReadFile(hFile, &uncompressed_size, 8, &br, NULL) || br != 8)
          { si_free(compressed); CloseHandle(hFile); return 0; } }
    CloseHandle(hFile);
    if (si_crc32(compressed, (size_t)footer.archive_size) != footer.checksum)
        { si_free(compressed); return 0; }
    if (footer.archive_size < LZMA_PROPS_SIZE) { si_free(compressed); return 0; }
    if (!si_txn_begin(&transaction, dest_dir, clean_install)) {
        si_free(compressed);
        return 0;
    }
    transaction_active = 1;

    { /* Streaming extract — heap-alloc buffers; install thread has only 64KB stack */
      CLzmaDec dec;
      const uint8_t* in_p  = compressed + LZMA_PROPS_SIZE;
      SizeT          in_rem = (SizeT)(footer.archive_size - LZMA_PROPS_SIZE);
      uint64_t       out_done = 0; int lzma_ok = 0;
      /* 256KB output chunk + 64KB header accumulator — both heap */
      uint8_t* out_buf  = (uint8_t*)si_malloc(262144);
      uint8_t* hdr_buf  = (uint8_t*)si_malloc(65536);
      size_t   hdr_got  = 0, hdr_size = 0;
      int      hdr_parsed = 0;
      uint32_t file_cnt = 0;
      SiPayloadEntry* entries = NULL;
      int* transaction_indices = NULL;
      /* Data routing state */
      int      cur_file  = 0;
      HANDLE   cur_hfile = INVALID_HANDLE_VALUE;
      int      cur_skip  = 0, cur_fresh = 1;
      uint64_t data_pos  = 0;
      int      write_err = 0;

      if (!out_buf || !hdr_buf) {
          if (out_buf) si_free(out_buf);
          if (hdr_buf) si_free(hdr_buf);
          si_txn_abort(&transaction);
          si_free(compressed); return 0;
      }
      LzmaDec_Construct(&dec);
      if (LzmaDec_Allocate(&dec, compressed, LZMA_PROPS_SIZE, &g_SiAlloc) != SZ_OK) {
          si_free(out_buf); si_free(hdr_buf); si_txn_abort(&transaction);
          si_free(compressed); return 0;
      }
      LzmaDec_Init(&dec);

      while (out_done < uncompressed_size && !write_err) {
          SizeT ochunk = 262144; ELzmaStatus st; SizeT ichunk = in_rem;
          if (uncompressed_size - out_done < (uint64_t)ochunk)
              ochunk = (SizeT)(uncompressed_size - out_done);
          if (LzmaDec_DecodeToBuf(&dec, out_buf, &ochunk, in_p, &ichunk,
                                  LZMA_FINISH_ANY, &st) != SZ_OK) break;
          in_p += ichunk; in_rem -= ichunk;

          { size_t pos = 0;
            /* Phase 1: accumulate header until parsed */
            if (!hdr_parsed && ochunk > 0) {
                size_t copy = ochunk;
                if (hdr_got + copy > 65536) copy = 65536 - hdr_got;
                memcpy(hdr_buf + hdr_got, out_buf, copy);
                hdr_got += copy;
                { const uint8_t *hp = hdr_buf, *hend = hdr_buf + hdr_got;
                  if (hp + 4 <= hend) {
                      uint32_t i; int complete = 1;
                      file_cnt = si_rd32(hp); hp += 4;
                      entries  = (SiPayloadEntry*)si_malloc(file_cnt * sizeof(SiPayloadEntry));
                      if (!entries) { write_err = 1; }
                      else {
                          for (i = 0; i < file_cnt; i++) {
                              uint16_t nlen;
                              if (hp + 2 > hend) { complete = 0; break; }
                              nlen = si_rd16(hp); hp += 2;
                              if (hp + nlen > hend) { complete = 0; break; }
                              if (nlen >= 260) { write_err = 1; break; }
                              memcpy(entries[i].name, hp, nlen);
                              entries[i].name[nlen] = '\0'; hp += nlen;
                              if (hp + 16 > hend) { complete = 0; break; }
                              entries[i].size   = si_rd64(hp); hp += 8;
                              entries[i].offset = si_rd64(hp); hp += 8;
                          }
                          if (!complete) { si_free(entries); entries = NULL; }
                          else {
                              hdr_size = (size_t)(hp - hdr_buf);
                              hdr_parsed = 1;
                              if (!si_payload_stage_entries(
                                      &transaction, entries, file_cnt,
                                      &transaction_indices)) {
                                  write_err = 1;
                              }
                              pos = (hdr_size > (size_t)out_done)
                                    ? hdr_size - (size_t)out_done : 0;
                              if (pos > (size_t)ochunk) pos = (size_t)ochunk;
                          }
                      }
                  }
                }
                if (!hdr_parsed) pos = ochunk;
            }
            /* Phase 2: route data bytes to files */
            while (hdr_parsed && entries && pos < ochunk && !write_err) {
                /* Advance past fully-written entries */
                while (cur_file < (int)file_cnt &&
                       data_pos >= entries[cur_file].offset + entries[cur_file].size) {
                    if (cur_hfile != INVALID_HANDLE_VALUE)
                        { CloseHandle(cur_hfile); cur_hfile = INVALID_HANDLE_VALUE; }
                    cur_file++; cur_skip = 0; cur_fresh = 1;
                }
                if (cur_file >= (int)file_cnt) { pos = ochunk; break; }
                /* Open / decide on first byte of entry */
                if (cur_fresh) {
                    SiPayloadEntry* e = &entries[cur_file];
                    cur_fresh = 0;
                    if (e->name[0] == '_' && e->name[1] == '_') {
                        cur_skip = 1;
                    } else {
                        int transaction_index = transaction_indices
                            ? transaction_indices[cur_file] : -1;
                        if (transaction_index < 0 ||
                            transaction_index >= transaction.file_count) {
                            write_err = 1;
                            break;
                        }
                        cur_hfile = CreateFileW(
                            transaction.files[transaction_index].staged,
                            GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL, NULL);
                        if (cur_hfile == INVALID_HANDLE_VALUE) {
                            write_err = 1;
                            break;
                        }
                    }
                }
                { SiPayloadEntry* e = &entries[cur_file];
                  uint64_t file_rem  = (e->offset + e->size) - data_pos;
                  size_t   chunk_rem = ochunk - pos;
                  size_t   to_write  = (file_rem < (uint64_t)chunk_rem) ? (size_t)file_rem : chunk_rem;
                  if (!cur_skip) {
                      DWORD wr = 0;
                      if (!WriteFile(cur_hfile, out_buf + pos, (DWORD)to_write, &wr, NULL)
                          || wr != (DWORD)to_write) { write_err = 1; break; }
                  }
                  pos += to_write; data_pos += to_write;
                }
            }
          }

          out_done += ochunk;
          if (pfn && uncompressed_size > 0)
              pfn((int)((uint64_t)100 * out_done / uncompressed_size));
          if (ochunk == 0 && ichunk == 0) break;
          if (st == LZMA_STATUS_FINISHED_WITH_MARK ||
              st == LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK) { lzma_ok = 1; break; }
      }
      if (out_done == uncompressed_size) lzma_ok = 1;
      if (cur_hfile != INVALID_HANDLE_VALUE) CloseHandle(cur_hfile);
      if (transaction_indices) si_free(transaction_indices);
      if (entries) si_free(entries);
      LzmaDec_Free(&dec, &g_SiAlloc);
      si_free(hdr_buf); si_free(out_buf);
      if (lzma_ok && !write_err && hdr_parsed) {
          SiPayloadCommitPrompt prompt;
          prompt.owner = owner;
          prompt.interactive = interactive;
          prompt.cancelled = cancelled;
          result = si_txn_commit(
              &transaction, si_payload_commit_retry, &prompt);
          transaction_active = 0;
      }
    }
    if (transaction_active) si_txn_abort(&transaction);
    si_free(compressed);
    return result;
}

static int si_payload_extract_all(const SiPayloadReader* r, const wchar_t* dest_dir) {
    int i;
    if (GetFileAttributesW(dest_dir) == INVALID_FILE_ATTRIBUTES)
        SHCreateDirectoryExW(NULL, dest_dir, NULL);
    for (i = 0; i < r->entry_count; i++) {
        SiPayloadEntry* e = &r->entries[i];
        wchar_t dest[MAX_PATH], wname[260]; int wlen; HANDLE hf; DWORD written;
        if (e->name[0] == '_' && e->name[1] == '_') continue;
        wcscpy(dest, dest_dir);
        if (dest[wcslen(dest)-1] != L'\\') wcscat(dest, L"\\");
        wlen = MultiByteToWideChar(CP_UTF8, 0, e->name, -1, wname, 260);
        if (wlen <= 0) continue;
        wcscat(dest, wname);
        { size_t bl = wcslen(dest_dir)+1; wchar_t* s = dest+bl;
          while (*s) { if (*s==L'\\') { wchar_t sv=*(s+1); *(s+1)=0; CreateDirectoryW(dest,NULL); *(s+1)=sv; } s++; } }
        hf = CreateFileW(dest, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hf == INVALID_HANDLE_VALUE) return 0;
        written = 0;
        WriteFile(hf, r->data_start + e->offset, (DWORD)e->size, &written, NULL);
        CloseHandle(hf);
        if (written != (DWORD)e->size) return 0;
    }
    return 1;
}
#endif /* SI_HAS_PAYLOAD */

/* ═══════════════════════════════════════════════════════════════════════════
   I18N STRING TABLE
   ═══════════════════════════════════════════════════════════════════════════ */

#ifdef SI_FEATURE_I18N

typedef struct SiLocaleInfo { wchar_t code[4]; wchar_t display_name[32]; } SiLocaleInfo;
typedef struct SiStringEntry { uint32_t key_hash; uint32_t offsets[SI_MAX_LOCALES]; } SiStringEntry;
typedef struct SiStringTable {
    int current_locale, locale_count, key_count;
    SiLocaleInfo locales[SI_MAX_LOCALES];
    SiStringEntry* entries; uint8_t* pool; size_t pool_size;
} SiStringTable;

static SiStringTable g_strings;
static int g_strings_loaded = 0;

static uint32_t si_fnv1a_w(const wchar_t* str) {
    uint32_t hash = 2166136261u;
    while (*str) {
        const uint8_t* b = (const uint8_t*)str; size_t i;
        for (i = 0; i < sizeof(wchar_t); i++) { hash ^= b[i]; hash *= 16777619u; }
        str++;
    }
    return hash;
}

static int si_strtable_load(SiStringTable* st, const uint8_t* data, size_t size) {
    const uint8_t *p, *end; int li, ki; size_t ps;
    memset(st, 0, sizeof(*st));
    if (!data || size < 4) return 0;
    p = data; end = data + size;
    st->locale_count = si_rd16(p); p += 2;
    st->key_count = si_rd16(p); p += 2;
    if (st->locale_count==0||st->key_count==0||st->locale_count>SI_MAX_LOCALES) return 0;
    if (p + (size_t)st->locale_count * 68 > end) return 0;
    for (li = 0; li < st->locale_count; li++) {
        int ci; const uint16_t* wn;
        for (ci = 0; ci < 4; ci++) st->locales[li].code[ci] = (wchar_t)(uint8_t)p[ci];
        p += 4;
        wn = (const uint16_t*)p;
        for (ci = 0; ci < 32; ci++) st->locales[li].display_name[ci] = (wchar_t)wn[ci];
        p += 64;
    }
    if (p + (size_t)st->key_count * (4 + (size_t)st->locale_count * 4) > end) return 0;
    st->entries = (SiStringEntry*)si_malloc(st->key_count * sizeof(SiStringEntry));
    if (!st->entries) return 0;
    for (ki = 0; ki < st->key_count; ki++) {
        st->entries[ki].key_hash = si_rd32(p); p += 4;
        for (li = 0; li < st->locale_count; li++) { st->entries[ki].offsets[li] = si_rd32(p); p += 4; }
    }
    ps = (size_t)(end - p);
    st->pool = (uint8_t*)si_malloc(ps);
    if (!st->pool) { si_free(st->entries); return 0; }
    memcpy(st->pool, p, ps); st->pool_size = ps;
    return 1;
}

static const wchar_t* si_strtable_get(const SiStringTable* st, const wchar_t* key) {
    uint32_t hash; int i;
    if (!key || !st->entries) return NULL;
    hash = si_fnv1a_w(key);
    for (i = 0; i < st->key_count; i++) {
        if (st->entries[i].key_hash == hash) {
            uint32_t off = st->entries[i].offsets[st->current_locale];
            if (off == 0xFFFFFFFF && st->current_locale != 0) off = st->entries[i].offsets[0];
            if (off == 0xFFFFFFFF) return NULL;
            if ((size_t)off + sizeof(wchar_t) <= st->pool_size)
                return (const wchar_t*)(st->pool + off);
            return NULL;
        }
    }
    return NULL;
}

const wchar_t* si_tr(const wchar_t* key) {
    if (g_strings_loaded) { const wchar_t* v = si_strtable_get(&g_strings, key); if (v) return v; }
    return key;
}

static void si_init_i18n(void) {
    const uint8_t* str_data = NULL; uint32_t str_sz = 0;
    if (g_strings_loaded) return;
    if (si_strings_open(&str_data, &str_sz)) {
        g_strings_loaded = si_strtable_load(&g_strings, str_data, str_sz);
        si_free((void*)str_data);
    }
    /* Auto-detect system UI language and select matching locale */
    if (g_strings_loaded && g_strings.locale_count > 0) {
        wchar_t name[LOCALE_NAME_MAX_LENGTH]; int i, found = 0;
        /* Try UI language first (what Windows is displayed in) */
        if (GetUserDefaultLocaleName(name, LOCALE_NAME_MAX_LENGTH) > 0) {
            wchar_t code[4]; code[0] = name[0]; code[1] = name[1]; code[2] = L'\0';
            for (i = 0; i < g_strings.locale_count; i++) {
                if (g_strings.locales[i].code[0] == code[0] && g_strings.locales[i].code[1] == code[1]) {
                    g_strings.current_locale = i; found = 1; break;
                }
            }
        }
        /* Fallback: find English */
        if (!found) {
            for (i = 0; i < g_strings.locale_count; i++) {
                if (g_strings.locales[i].code[0] == L'e' && g_strings.locales[i].code[1] == L'n') {
                    g_strings.current_locale = i; break;
                }
            }
        }
    }
}

#else

static void si_init_i18n(void) {}

#endif /* SI_FEATURE_I18N */

static wchar_t si_arg_lower(wchar_t c) {
    return (c >= L'A' && c <= L'Z') ? (wchar_t)(c + 32) : c;
}

static int si_arg_equals(const wchar_t* start, const wchar_t* end, const wchar_t* arg) {
    while (start < end && *arg) {
        if (si_arg_lower(*start) != si_arg_lower(*arg)) return 0;
        start++; arg++;
    }
    return start == end && *arg == 0;
}

static int si_cmdline_has_arg(const wchar_t* arg) {
    const wchar_t* p = GetCommandLineW();
    while (p && *p) {
        const wchar_t* start;
        const wchar_t* end;
        int quoted = 0;
        while (*p == L' ' || *p == L'\t' || *p == L'\r' || *p == L'\n') p++;
        if (!*p) break;
        start = p;
        while (*p) {
            if (*p == L'"') quoted = !quoted;
            else if (!quoted && (*p == L' ' || *p == L'\t' || *p == L'\r' || *p == L'\n')) break;
            p++;
        }
        end = p;
        if (start < end && *start == L'"' && *(end - 1) == L'"') { start++; end--; }
        if (si_arg_equals(start, end, arg)) return 1;
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
   DPI HELPERS
   ═══════════════════════════════════════════════════════════════════════════ */

static void si_init_dpi_awareness(void) {
    typedef BOOL (WINAPI *FnA)(DPI_AWARENESS_CONTEXT);
    typedef HRESULT (WINAPI *FnB)(int);
    FnA fa = (FnA)GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetProcessDpiAwarenessContext");
    if (fa) { fa(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2); return; }
    { HMODULE sc = LoadLibraryW(L"shcore.dll");
      if (sc) { FnB fb = (FnB)GetProcAddress(sc, "SetProcessDpiAwareness"); if (fb) fb(2); FreeLibrary(sc); } }
}

/* si_get_window_dpi, si_scheme_light, si_scheme_dark — now provided by Slim UI
 * (bridged as si_get_window_dpi -> sui_get_window_dpi, etc. in small_installer.h) */

#ifdef SI_FEATURE_DARK_MODE
static int si_is_dark_mode(void) {
    DWORD val = 1, sz = sizeof(val);
    if (RegGetValueW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme", RRF_RT_DWORD, NULL, &val, &sz) == ERROR_SUCCESS)
        return val == 0;
    return 0;
}
#endif

/* ═══════════════════════════════════════════════════════════════════════════
   KNOWN FOLDER HELPER
   ═══════════════════════════════════════════════════════════════════════════ */

static void si_get_known_folder(const KNOWNFOLDERID* fid, wchar_t* buf, int buf_len) {
    wchar_t* path = NULL; int i; buf[0] = L'\0';
    if (SUCCEEDED(SHGetKnownFolderPath(fid, 0, NULL, &path))) {
        for (i = 0; path[i] && i < buf_len-1; i++) buf[i] = path[i];
        buf[i] = L'\0'; CoTaskMemFree(path);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
   WIZARD STATE
   ═══════════════════════════════════════════════════════════════════════════ */

#define SI_BASE_W  640
#define SI_BASE_H  486
#define SI_BTN_W   80
#define SI_BTN_H   28
#define SI_BTN_M   10
#define SI_BTNAREA 48

#define SI_IDC_LANG_COMBO    200
#define SI_IDC_LICENSE_EDIT  201
#define SI_IDC_LICENSE_CHECK 202
#define SI_IDC_IDIR_EDIT     203
#define SI_IDC_IDIR_BROWSE   204
#define SI_IDC_STATUS_LABEL  205
#define SI_IDC_UNINST_DELETE 206
#define SI_IDC_CB_BASE       300
#define SI_ID_BACK           100
#define SI_ID_NEXT           101

#define WM_SI_PROGRESS (WM_APP + 1)
#define WM_SI_STATUS   (WM_APP + 2)
#define WM_SI_DONE     (WM_APP + 3)
#define WM_SI_FILE_PROGRESS (WM_APP + 4)
#define SI_UNINSTALL_PROGRESS_TIMER_ID 0x5349u
/* Vblank pump message + types live in slim_ui/sui_vsync.h (WM_SUI_VSYNC). */

/* Forward typedef lives in small_installer.h so public headers can refer to
 * SiWizardState* without including this private header. */
struct SiWizardState {
    HINSTANCE hInst; HWND hwnd;
    wchar_t app_name[SI_MAX_NAME]; wchar_t app_version[SI_MAX_NAME];
    wchar_t app_publisher[SI_MAX_NAME]; wchar_t app_id[SI_MAX_NAME];
    wchar_t app_exe[SI_MAX_NAME]; wchar_t install_path[MAX_PATH];
    const wchar_t* silent_update_launch_args;
    const wchar_t* silent_launch_args;
    wchar_t existing_version[SI_MAX_NAME];
    SiSpecialPath install_base; SiPrivileges privs; uint32_t accent;
    SiPage pages[SI_MAX_PAGES]; int page_count; int current_page;
    UINT dpi; SiRenderer renderer;
    SiCallback cb_pre_install; SiCallback cb_clean_reinstall; SiCallback cb_finished; SiCallback cb_uninstall;
    void* user_data;
    SiRegEntry registry[SI_MAX_REG_ENTRIES]; int registry_count;
    SiShortcutEntry shortcuts[SI_MAX_SHORTCUTS]; int shortcut_count;
    const wchar_t* protocol_scheme;
    int is_uninstaller;
    int silent;
    int silent_update;
    int silent_launch;
    int existing_installed;
    int reinstall_clean;
    int reinstall_clean_only;
    HICON icon;
    const SiColorScheme* scheme;
    SiControlHost ctrl;
#ifdef SI_FEATURE_ANIMATIONS
    SiTransition transition_type;
    int transitioning;
    uint64_t transition_start;  /* QPC milliseconds (si_now_ms) */
    int transition_direction;
    void* transition_bitmap;    /* ID2D1Bitmap* */
    void* transition_brt;       /* ID2D1BitmapRenderTarget* */
    /* Vblank-locked animation pump (worker thread + DwmFlush + WM_SUI_VSYNC).
     * Implementation in slim_ui/sui_vsync.h. */
    SuiVsyncState vsync;
#endif
};

static SiWizardState* g_state = NULL;
static float si_dpi_scale(const SiWizardState* s) { return s->dpi / 96.0f; }
static int si_scale(const SiWizardState* s, int v) { return (int)(v * si_dpi_scale(s)); }
static uint32_t si_darken_rgb(uint32_t color, unsigned numerator, unsigned denominator) {
    uint32_t r = ((color >> 16) & 0xFF) * numerator / denominator;
    uint32_t g = ((color >> 8) & 0xFF) * numerator / denominator;
    uint32_t b = (color & 0xFF) * numerator / denominator;
    return (r << 16) | (g << 8) | b;
}
static uint32_t si_button_area_separator_color(const SiWizardState* st) {
    return si_darken_rgb(st->scheme->btn_area_border, 17, 20);
}

/* Forward declarations — functions defined in later sub-headers */
static void si_show_page(SiWizardState* st, int index);
static void si_go_next(SiWizardState* st);
static void si_go_back(SiWizardState* st);
static void si_update_buttons(SiWizardState* st);
static void si_layout_buttons(SiWizardState* st);
static SiControl* si_ctrl_find(SiControlHost* host, int id);
static void si_ctrl_clear_page(SiControlHost* host);
static int si_ctrl_next_focusable(SiControlHost* host, int from, int dir);
static void si_ctrl_draw_all(SiControlHost* host);
static void si_ctrl_on_mouse_move(SiControlHost* host, int mx, int my);
static void si_ctrl_on_lbutton_down(SiControlHost* host, int mx, int my);
static void si_ctrl_on_lbutton_up(SiControlHost* host, int mx, int my);
static void si_ctrl_on_mouse_wheel(SiControlHost* host, int mx, int my, int delta);
static void si_ctrl_on_keydown(SiControlHost* host, WPARAM vk);
static void si_ctrl_on_keyup(SiControlHost* host, WPARAM vk);
#ifdef SI_FEATURE_ANIMATIONS
static void si_transition_capture(SiWizardState* st);
static void si_transition_start(SiWizardState* st, int direction);
static void si_transition_paint(SiWizardState* st);
static void si_transition_cleanup(SiWizardState* st);
/* Vsync helpers (sui_vsync_attach/detach/stop/dispatch) live in
 * slim_ui/sui_vsync.h, included from small_installer.h. */
#endif
static int si_reg_set_string(SiRegRoot root, const wchar_t* subkey, const wchar_t* name, const wchar_t* value);
static int si_reg_set_dword(SiRegRoot root, const wchar_t* subkey, const wchar_t* name, DWORD value);
static int si_reg_read_string(SiRegRoot root, const wchar_t* subkey, const wchar_t* name, wchar_t* out, DWORD out_count);
static int si_reg_delete_value(SiRegRoot root, const wchar_t* subkey, const wchar_t* name);
static int si_reg_delete_key(SiRegRoot root, const wchar_t* subkey);
static int si_reg_exists_fn(SiRegRoot root, const wchar_t* subkey, const wchar_t* name);
static void si_delete_tree(const wchar_t* path);
static void si_write_arp(const SiWizardState* st);
static void si_remove_arp(const SiWizardState* st);
static void si_resolve_shortcut_path(SiSpecialPath loc, const wchar_t* name, wchar_t* buf, int blen);
static int si_create_shortcut_file(const wchar_t* target, const wchar_t* lnk, const wchar_t* desc, const wchar_t* icon, const wchar_t* wdir);
static int si_register_protocol_fn(const wchar_t* scheme, const wchar_t* exe);
static int si_unregister_protocol_fn(const wchar_t* scheme);
static int si_kill_process_by_name(const wchar_t* name);
static int si_exec_wait(const wchar_t* path, const wchar_t* args);
static int si_launch_detached(const wchar_t* path, const wchar_t* args);

/* Get free disk space (in bytes) on the volume containing `path`. Returns 1
 * on success, 0 on failure. Path may be a directory that doesn't yet exist —
 * we walk up to the drive root before querying. */
static int si_get_free_space(const wchar_t* path, uint64_t* out_free) {
    wchar_t root[MAX_PATH]; ULARGE_INTEGER avail; size_t len;
    *out_free = 0;
    if (!path || !path[0]) return 0;
    wcsncpy(root, path, MAX_PATH-1); root[MAX_PATH-1] = 0;
    len = wcslen(root);
    if (len >= 2 && root[1] == L':') {
        /* Drive-letter path "X:\..." → "X:\" */
        if (len >= 3 && (root[2] == L'\\' || root[2] == L'/')) { root[2] = L'\\'; root[3] = 0; }
        else { root[2] = L'\\'; root[3] = 0; }
    } else if (len >= 2 && root[0] == L'\\' && root[1] == L'\\') {
        /* UNC "\\server\share\..." — keep up to and including share name */
        wchar_t* p = root + 2; int slashes = 0;
        while (*p) { if (*p == L'\\' || *p == L'/') { slashes++; if (slashes == 2) { *p = 0; break; } } p++; }
        if (slashes < 2) return 0;
    } else {
        return 0;
    }
    if (!GetDiskFreeSpaceExW(root, &avail, NULL, NULL)) return 0;
    *out_free = (uint64_t)avail.QuadPart;
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════════════
   REGISTRY OPERATIONS
   ═══════════════════════════════════════════════════════════════════════════ */

static HKEY si_root_to_hkey(SiRegRoot root) {
    switch (root) {
        case SI_REG_HKCU: return HKEY_CURRENT_USER;
        case SI_REG_HKLM: case SI_REG_HKLM64: return HKEY_LOCAL_MACHINE;
        default: return HKEY_CURRENT_USER;
    }
}

static REGSAM si_reg_access(SiRegRoot root, REGSAM base) {
    return (root == SI_REG_HKLM64) ? (base | KEY_WOW64_64KEY) : base;
}

static int si_reg_set_string(SiRegRoot root, const wchar_t* subkey, const wchar_t* name, const wchar_t* value) {
    HKEY hKey; LSTATUS r;
    if (RegCreateKeyExW(si_root_to_hkey(root), subkey, 0, NULL, 0, si_reg_access(root, KEY_ALL_ACCESS),
        NULL, &hKey, NULL) != ERROR_SUCCESS) return 0;
    r = RegSetValueExW(hKey, name, 0, REG_SZ, (const BYTE*)value, (DWORD)((wcslen(value)+1)*sizeof(wchar_t)));
    RegCloseKey(hKey); return r == ERROR_SUCCESS;
}

static int si_reg_set_dword(SiRegRoot root, const wchar_t* subkey, const wchar_t* name, DWORD value) {
    HKEY hKey; LSTATUS r;
    if (RegCreateKeyExW(si_root_to_hkey(root), subkey, 0, NULL, 0, si_reg_access(root, KEY_ALL_ACCESS),
        NULL, &hKey, NULL) != ERROR_SUCCESS) return 0;
    r = RegSetValueExW(hKey, name, 0, REG_DWORD, (const BYTE*)&value, sizeof(DWORD));
    RegCloseKey(hKey); return r == ERROR_SUCCESS;
}

static int si_reg_read_string(SiRegRoot root, const wchar_t* subkey, const wchar_t* name, wchar_t* out, DWORD out_count) {
    HKEY hKey; DWORD type = REG_SZ, bytes;
    if (!out || out_count == 0) return 0;
    out[0] = 0;
    bytes = out_count * sizeof(wchar_t);
    if (RegOpenKeyExW(si_root_to_hkey(root), subkey, 0, si_reg_access(root, KEY_QUERY_VALUE), &hKey) != ERROR_SUCCESS) return 0;
    if (RegQueryValueExW(hKey, name, NULL, &type, (LPBYTE)out, &bytes) != ERROR_SUCCESS ||
        (type != REG_SZ && type != REG_EXPAND_SZ)) {
        RegCloseKey(hKey);
        out[0] = 0;
        return 0;
    }
    RegCloseKey(hKey);
    out[out_count - 1] = 0;
    return out[0] != 0;
}

static int si_reg_delete_value(SiRegRoot root, const wchar_t* subkey, const wchar_t* name) {
    HKEY hKey; LSTATUS r;
    if (RegOpenKeyExW(si_root_to_hkey(root), subkey, 0, si_reg_access(root, KEY_ALL_ACCESS), &hKey) != ERROR_SUCCESS) return 0;
    r = RegDeleteValueW(hKey, name); RegCloseKey(hKey); return r == ERROR_SUCCESS;
}

static int si_reg_delete_key(SiRegRoot root, const wchar_t* subkey) {
    return RegDeleteTreeW(si_root_to_hkey(root), subkey) == ERROR_SUCCESS;
}

static int si_reg_exists_fn(SiRegRoot root, const wchar_t* subkey, const wchar_t* name) {
    HKEY hKey; LSTATUS r;
    if (RegOpenKeyExW(si_root_to_hkey(root), subkey, 0, si_reg_access(root, KEY_QUERY_VALUE), &hKey) != ERROR_SUCCESS) return 0;
    r = (name != NULL) ? RegQueryValueExW(hKey, name, NULL, NULL, NULL, NULL) : ERROR_SUCCESS;
    RegCloseKey(hKey); return r == ERROR_SUCCESS;
}

static void si_write_arp(const SiWizardState* st) {
    wchar_t key[512], uninst[MAX_PATH + 16], icon[MAX_PATH];
    SiRegRoot root = (st->privs == SI_PRIV_ADMIN) ? SI_REG_HKLM : SI_REG_HKCU;
    wcscpy(key, L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\");
    wcscat(key, st->app_id);
    si_reg_set_string(root, key, L"DisplayName", st->app_name);
    si_reg_set_string(root, key, L"DisplayVersion", st->app_version);
    si_reg_set_string(root, key, L"Publisher", st->app_publisher);
    wcscpy(uninst, L"\""); wcscat(uninst, st->install_path); wcscat(uninst, L"\\uninstall.exe\"");
    si_reg_set_string(root, key, L"UninstallString", uninst);
    wcscat(uninst, L" --silent");
    si_reg_set_string(root, key, L"QuietUninstallString", uninst);
    wcscpy(icon, st->install_path); wcscat(icon, L"\\"); wcscat(icon, st->app_exe);
    si_reg_set_string(root, key, L"DisplayIcon", icon);
    si_reg_set_string(root, key, L"InstallLocation", st->install_path);
    si_reg_set_dword(root, key, L"NoModify", 1);
    si_reg_set_dword(root, key, L"NoRepair", 1);
}

static void si_remove_arp(const SiWizardState* st) {
    wchar_t key[512];
    SiRegRoot root = (st->privs == SI_PRIV_ADMIN) ? SI_REG_HKLM : SI_REG_HKCU;
    wcscpy(key, L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\");
    wcscat(key, st->app_id);
    si_reg_delete_key(root, key);
}

static int si_version_part(const wchar_t** p) {
    int value = 0;
    while (**p && (**p < L'0' || **p > L'9')) (*p)++;
    while (**p >= L'0' && **p <= L'9') {
        if (value < 1000000) value = value * 10 + (**p - L'0');
        (*p)++;
    }
    return value;
}

static int si_compare_versions(const wchar_t* setup, const wchar_t* installed) {
    int i;
    const wchar_t* a = setup ? setup : L"";
    const wchar_t* b = installed ? installed : L"";
    for (i = 0; i < 8; i++) {
        int va = si_version_part(&a);
        int vb = si_version_part(&b);
        if (va < vb) return -1;
        if (va > vb) return 1;
        if (!*a && !*b) return 0;
    }
    return 0;
}

static void si_detect_existing_install(SiWizardState* st) {
    wchar_t key[512];
    SiRegRoot root;
    int has_location, has_version;
    st->existing_installed = 0;
    st->reinstall_clean = 0;
    st->reinstall_clean_only = 0;
    st->existing_version[0] = 0;
    if (!st->app_id[0] || st->is_uninstaller) return;

    root = (st->privs == SI_PRIV_ADMIN) ? SI_REG_HKLM : SI_REG_HKCU;
    wcscpy(key, L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\");
    wcscat(key, st->app_id);

    has_location = si_reg_read_string(root, key, L"InstallLocation", st->install_path, MAX_PATH);
    has_version = si_reg_read_string(root, key, L"DisplayVersion", st->existing_version, SI_MAX_NAME);
    st->existing_installed = has_location || has_version;
    if (!st->existing_installed) return;

    if (st->existing_version[0] && si_compare_versions(st->app_version, st->existing_version) < 0) {
        st->reinstall_clean_only = 1;
        st->reinstall_clean = 1;
    }
}

static void si_delete_tree(const wchar_t* path) {
    wchar_t from[MAX_PATH + 2];
    SHFILEOPSTRUCTW op;
    size_t len;
    if (!path || !path[0]) return;
    if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) return;
    wcsncpy(from, path, MAX_PATH);
    from[MAX_PATH] = 0;
    len = wcslen(from);
    if (len == 0 || len >= MAX_PATH) return;
    from[len + 1] = 0;
    memset(&op, 0, sizeof(op));
    op.wFunc = FO_DELETE;
    op.pFrom = from;
    op.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
    SHFileOperationW(&op);
}

/* ═══════════════════════════════════════════════════════════════════════════
   SHORTCUT + PROTOCOL + PROCESS HELPERS
   ═══════════════════════════════════════════════════════════════════════════ */

static void si_resolve_shortcut_path(SiSpecialPath loc, const wchar_t* name, wchar_t* buf, int blen) {
    wchar_t base[MAX_PATH]; buf[0] = L'\0';
    if (loc == SI_PATH_START_MENU) si_get_known_folder(&FOLDERID_Programs, base, MAX_PATH);
    else if (loc == SI_PATH_DESKTOP) si_get_known_folder(&FOLDERID_Desktop, base, MAX_PATH);
    else return;
    if (base[0] == L'\0') return;
    wcscpy(buf, base); wcscat(buf, L"\\"); wcscat(buf, name); wcscat(buf, L".lnk");
}

static int si_create_shortcut_file(const wchar_t* target, const wchar_t* lnk,
                                   const wchar_t* desc, const wchar_t* icon, const wchar_t* wdir) {
    IShellLinkW* psl = NULL; IPersistFile* ppf = NULL; HRESULT hr;
    hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void**)&psl);
    if (FAILED(hr)) return 0;
    psl->lpVtbl->SetPath(psl, target);
    if (desc) psl->lpVtbl->SetDescription(psl, desc);
    if (icon) psl->lpVtbl->SetIconLocation(psl, icon, 0);
    if (wdir) psl->lpVtbl->SetWorkingDirectory(psl, wdir);
    hr = psl->lpVtbl->QueryInterface(psl, &IID_IPersistFile, (void**)&ppf);
    if (SUCCEEDED(hr)) { hr = ppf->lpVtbl->Save(ppf, lnk, TRUE); ppf->lpVtbl->Release(ppf); }
    psl->lpVtbl->Release(psl);
    return SUCCEEDED(hr);
}

static int si_register_protocol_fn(const wchar_t* scheme, const wchar_t* exe) {
    wchar_t base[512], sub[512], val[512];
    wcscpy(base, L"Software\\Classes\\"); wcscat(base, scheme);
    wcscpy(val, L"URL:"); wcscat(val, scheme); wcscat(val, L" Protocol");
    si_reg_set_string(SI_REG_HKCU, base, NULL, val);
    si_reg_set_string(SI_REG_HKCU, base, L"URL Protocol", L"");
    wcscpy(sub, base); wcscat(sub, L"\\DefaultIcon");
    wcscpy(val, L"\""); wcscat(val, exe); wcscat(val, L"\",0");
    si_reg_set_string(SI_REG_HKCU, sub, NULL, val);
    wcscpy(sub, base); wcscat(sub, L"\\shell\\open\\command");
    wcscpy(val, L"\""); wcscat(val, exe); wcscat(val, L"\" \"%1\"");
    si_reg_set_string(SI_REG_HKCU, sub, NULL, val);
    return 1;
}

static int si_unregister_protocol_fn(const wchar_t* scheme) {
    wchar_t base[512];
    wcscpy(base, L"Software\\Classes\\"); wcscat(base, scheme);
    return si_reg_delete_key(SI_REG_HKCU, base);
}

static int si_kill_process_by_name(const wchar_t* exe_name) {
    int killed = 0; HANDLE snap; PROCESSENTRY32W pe;
    snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, exe_name) == 0) {
                HANDLE proc = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pe.th32ProcessID);
                if (proc) {
                    if (TerminateProcess(proc, 0)) {
                        WaitForSingleObject(proc, 3000);
                        killed = 1;
                    }
                    CloseHandle(proc);
                }
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap); return killed;
}

static int si_exec_wait(const wchar_t* path, const wchar_t* args) {
    wchar_t cmd[1024]; STARTUPINFOW si; PROCESS_INFORMATION pi; DWORD ec;
    wcscpy(cmd, L"\""); wcscat(cmd, path); wcscat(cmd, L"\"");
    if (args && args[0]) { wcscat(cmd, L" "); wcscat(cmd, args); }
    memset(&si, 0, sizeof(si)); si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) return -1;
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &ec);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    return (int)ec;
}

static int si_launch_detached(const wchar_t* path, const wchar_t* args) {
    wchar_t cmd[1024]; STARTUPINFOW si; PROCESS_INFORMATION pi;
    wcscpy(cmd, L"\""); wcscat(cmd, path); wcscat(cmd, L"\"");
    if (args && args[0]) { wcscat(cmd, L" "); wcscat(cmd, args); }
    memset(&si, 0, sizeof(si)); si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_SHOWNORMAL;
    memset(&pi, 0, sizeof(pi));
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) return 0;
    AllowSetForegroundWindow(pi.dwProcessId);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread); return 1;
}

/* ═══════════════════════════════════════════════════════════════════════════
   DOWNLOAD (WinHTTP)
   ═══════════════════════════════════════════════════════════════════════════ */

#ifdef SI_FEATURE_DOWNLOAD
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

static int si_download_file(const wchar_t* url, const wchar_t* dest) {
    URL_COMPONENTS uc; wchar_t host[256], path[2048], extra[2048];
    HINTERNET hSess, hConn, hReq; DWORD flags, status, sz; HANDLE hFile;
    uint8_t buffer[8192]; DWORD br; int ok = 1;
    memset(&uc, 0, sizeof(uc)); uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = host; uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path; uc.dwUrlPathLength = 2048;
    uc.lpszExtraInfo = extra; uc.dwExtraInfoLength = 2048;
    if (!WinHttpCrackUrl(url, 0, 0, &uc)) return 0;
    /* Append query string to path */
    if (extra[0]) wcscat(path, extra);
    hSess = WinHttpOpen(L"SmallInstaller/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSess) return 0;
    /* Allow all redirects including HTTPS->HTTP */
    { DWORD pol = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
      WinHttpSetOption(hSess, WINHTTP_OPTION_REDIRECT_POLICY, &pol, sizeof(pol)); }
    hConn = WinHttpConnect(hSess, host, uc.nPort, 0);
    if (!hConn) { WinHttpCloseHandle(hSess); return 0; }
    flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    hReq = WinHttpOpenRequest(hConn, L"GET", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hReq) { WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSess); return 0; }
    if (!WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(hReq, NULL)) {
        WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSess); return 0;
    }
    /* Check HTTP status */
    sz = sizeof(status);
    WinHttpQueryHeaders(hReq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz, WINHTTP_NO_HEADER_INDEX);
    if (status != 200) { WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSess); return 0; }
    hFile = CreateFileW(dest, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) { WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSess); return 0; }
    while (WinHttpReadData(hReq, buffer, sizeof(buffer), &br) && br > 0) {
        DWORD wr; if (!WriteFile(hFile, buffer, br, &wr, NULL) || wr != br) { ok = 0; break; }
    }
    CloseHandle(hFile); WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSess);
    if (!ok) DeleteFileW(dest);
    return ok;
}
#endif /* SI_FEATURE_DOWNLOAD */

/* ═══════════════════════════════════════════════════════════════════════════
   CONTEXT IMPLEMENTATION
   ═══════════════════════════════════════════════════════════════════════════ */

void si_ctx_kill_process(SiContext* ctx, const wchar_t* name) { (void)ctx; si_kill_process_by_name(name); }
void si_ctx_launch(SiContext* ctx, const wchar_t* path, const wchar_t* args) { (void)ctx; si_launch_detached(path, args); }
void si_ctx_exec(SiContext* ctx, const wchar_t* cmd) {
    /* cmd is a full command line — pass directly to CreateProcessW (no extra quoting) */
    wchar_t buf[1024]; STARTUPINFOW si; PROCESS_INFORMATION pi;
    (void)ctx; wcscpy(buf, cmd);
    memset(&si, 0, sizeof(si)); si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));
    if (CreateProcessW(NULL, buf, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    }
}

void si_ctx_install_dir(SiContext* ctx, wchar_t* buf, int buf_len) {
    SiWizardState* st = (SiWizardState*)ctx->_state;
    if (st && buf_len > 0) { int i; for (i=0; st->install_path[i] && i<buf_len-1; i++) buf[i]=st->install_path[i]; buf[i]=0; }
}

void si_ctx_quoted_path(SiContext* ctx, const wchar_t* relative, wchar_t* buf, int buf_len) {
    SiWizardState* st = (SiWizardState*)ctx->_state;
    if (!st || buf_len < 4) { buf[0]=0; return; }
    wcscpy(buf, L"\""); wcscat(buf, st->install_path);
    if (relative) { wcscat(buf, L"\\"); wcscat(buf, relative); }
    wcscat(buf, L"\"");
}

void si_ctx_delete_path(SiContext* ctx, const wchar_t* path) {
    (void)ctx;
    si_delete_tree(path);
}

int si_ctx_checked(SiContext* ctx, const wchar_t* id) {
    SiWizardState* st = (SiWizardState*)ctx->_state;
    int i, j;
    if (!st || !id) return 0;
    for (i = 0; i < st->page_count; i++) {
        if (st->pages[i].type == SI_PAGE_FINISHED) {
            SiPageFinishedData* fd = &st->pages[i].data.finished;
            for (j = 0; j < fd->checkbox_count; j++) {
                if (fd->checkboxes[j].id && wcscmp(fd->checkboxes[j].id, id) == 0) {
                    { SiControl* c = si_ctrl_find(&st->ctrl, SI_IDC_CB_BASE + j);
                      if (c) return (c->state & SI_CTRL_CHECKED) ? 1 : 0;
                    }
                    return 0;
                }
            }
        }
    }
    return 0;
}

void si_ctx_registry_set(SiContext* ctx, SiRegRoot root, const wchar_t* key, const wchar_t* name, const wchar_t* value) {
    (void)ctx; si_reg_set_string(root, key, name, value);
}

void si_ctx_registry_delete(SiContext* ctx, SiRegRoot root, const wchar_t* key, const wchar_t* name) {
    (void)ctx;
    if (name == NULL) si_reg_delete_key(root, key);
    else si_reg_delete_value(root, key, name);
}

int si_ctx_registry_exists(SiContext* ctx, SiRegRoot root, const wchar_t* key, const wchar_t* name) {
    (void)ctx; return si_reg_exists_fn(root, key, name);
}

#ifdef SI_FEATURE_DOWNLOAD
int si_ctx_download(SiContext* ctx, const wchar_t* url, const wchar_t* dest) {
    SiWizardState* st = (SiWizardState*)ctx->_state;
    wchar_t full[MAX_PATH];
    int absolute = dest &&
        ((dest[0] && dest[1] == L':') ||
         (dest[0] == L'\\' && dest[1] == L'\\'));
    if (st && !absolute) {
        wcscpy(full, st->install_path);
        wcscat(full, L"\\");
        wcscat(full, dest);
    } else {
        wcscpy(full, dest);
    }
    return si_download_file(url, full);
}
#endif

#ifdef SI_FEATURE_I18N
const wchar_t* si_ctx_locale(SiContext* ctx) {
    (void)ctx;
    if (g_strings_loaded && g_strings.current_locale < g_strings.locale_count)
        return g_strings.locales[g_strings.current_locale].code;
    return L"en";
}
#endif
