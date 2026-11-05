# Developer reference

[Back to the README](../README.md)

Configuration, API details and packaging options for building your own installer.
For a working starting point, see the [sample app](../examples/sample-app).

- [Feature flags](#feature-flags) and [limits](#limits)
- [Enums](#enums) and [configuration structs](#structs)
- [Pages](#page-constructors) and [callbacks](#context-functions)
- [Translations](#internationalization) and [command-line options](#command-line)
- [Packaging with si_pack](#si_pack-tool)
- [Replacing an existing installation](#replacing-an-existing-installation)
- [Architecture and dependencies](#architecture)
- [Size measurements](#size)

## API Reference

### Feature Flags

Define these **before** including the header:

| Flag | Description |
|------|-------------|
| `SI_IMPLEMENTATION` | Include the implementation (define in exactly one .c file) |
| `SI_NO_CRT` | Enable no-CRT mode: HeapAlloc memory, CRT stub functions (set by `si_add_wizard` in MinSizeRel) |
| `SI_FEATURE_D2D` | Required: Slim UI draws the wizard with Direct2D and DirectWrite (links d2d1.lib, dwrite.lib) |
| `SI_FEATURE_PNG` | Enable embedded images on the wizard's pages |
| `SI_FEATURE_I18N` | Enable internationalization with compiled string tables |
| `SI_FEATURE_DARK_MODE` | Follow the Windows dark app theme |
| `SI_FEATURE_ANIMATIONS` | Enable page transitions and the vsync pump behind them (`SiConfig.transition`) |
| `SI_FEATURE_DOWNLOAD` | Enable WinHTTP file downloads (links winhttp.lib) |
| `SI_FEATURE_ALL_CONTROLS` | Keep Slim UI's other controls for a custom page, and `si_ctrl_set_tooltip`: combo boxes, tabs, lists, spinners, sliders and the rest. A wizard leaves them out, which saves 68.5 KB per program |
| `SI_MODE_INSTALL` | An installer build: compiles the extraction of the appended archive |
| `SI_MODE_UNINSTALL` | An uninstaller build, which adds its silent mode; set `SiConfig.is_uninstaller` too |

### Limits

| Constant | Default | Description |
|----------|---------|-------------|
| `SI_MAX_PAGES` | 8 | Maximum wizard pages per config |
| `SI_MAX_REG_ENTRIES` | 16 | Maximum registry entries |
| `SI_MAX_SHORTCUTS` | 4 | Maximum shortcut entries |
| `SI_MAX_CHECKBOXES` | 4 | Maximum checkboxes on the finished page |
| `SI_MAX_NAME` | 256 | Maximum string length for names |
| `SI_MAX_LOCALES` | 32 | Maximum supported locales |
| `SI_MAX_KEYS` | 512 | Maximum i18n keys |

### Enums

#### `SiSpecialPath`

Base path for installation or shortcut placement.

| Value | Description |
|-------|-------------|
| `SI_PATH_LOCAL_APP_DATA` | `%LOCALAPPDATA%` (per-user) |
| `SI_PATH_PROGRAM_FILES` | `%ProgramFiles%` (system-wide) |
| `SI_PATH_DESKTOP` | User's desktop |
| `SI_PATH_START_MENU` | Start Menu programs folder |
| `SI_PATH_INSTALL_DIR` | Resolved install directory (for shortcuts) |

#### `SiPrivileges`

Installation privilege level.

| Value | Description |
|-------|-------------|
| `SI_PRIV_PER_USER` | Install for current user only (no UAC) |
| `SI_PRIV_ADMIN` | Require administrator privileges |

#### `SiUninstallerMode`

How the uninstaller binary is placed.

| Value | Description |
|-------|-------------|
| `SI_UNINST_EMBEDDED` | Uninstaller is embedded in the installer archive |
| `SI_UNINST_SELF_COPY` | Installer copies itself as the uninstaller |

#### `SiRegRoot`

Registry hive for registry operations.

| Value | Description |
|-------|-------------|
| `SI_REG_HKCU` | `HKEY_CURRENT_USER` |
| `SI_REG_HKLM` | `HKEY_LOCAL_MACHINE` (32-bit view) |
| `SI_REG_HKLM64` | `HKEY_LOCAL_MACHINE` (64-bit view) |

#### `SiPageType`

Wizard page type identifier.

| Value | Description |
|-------|-------------|
| `SI_PAGE_NONE` | Empty/uninitialized |
| `SI_PAGE_WELCOME` | Welcome page with an optional image on the left |
| `SI_PAGE_LICENSE` | License agreement with checkbox |
| `SI_PAGE_INSTALL_DIR` | Install directory picker |
| `SI_PAGE_REINSTALL` | Existing-install reinstall options, inserted automatically when `app_id` is already installed |
| `SI_PAGE_INSTALLING` | Progress bar during file extraction |
| `SI_PAGE_FINISHED` | Completion page with optional checkboxes |
| `SI_PAGE_CONFIRM` | Uninstall confirmation (with optional image) |
| `SI_PAGE_UNINSTALLING` | Uninstall progress bar |
| `SI_PAGE_CUSTOM` | User-defined page with custom paint callback |

### Structs

#### `SiConfig`

Main configuration struct. Zero-initialize with `{0}`, then set fields.

| Field | Type | Description |
|-------|------|-------------|
| `name` | `const wchar_t*` | Application display name |
| `version` | `const wchar_t*` | Version string (e.g., `L"1.0.0"`) |
| `publisher` | `const wchar_t*` | Publisher name for Add/Remove Programs |
| `app_id` | `const wchar_t*` | Unique GUID for registry identification |
| `exe` | `const wchar_t*` | Main executable filename; also used to kill the running app before extraction |
| `silent_update_launch_args` | `const wchar_t*` | Optional arguments for relaunching the app after `--silent-update` |
| `silent_launch_args` | `const wchar_t*` | Optional arguments for the launch that `--launch` requests after a silent install |
| `install_base` | `SiSpecialPath` | Base directory for installation |
| `install_sub` | `const wchar_t*` | Subdirectory under `install_base` |
| `install_dir_override` | `const wchar_t*` | Optional fully-resolved install directory |
| `privileges` | `SiPrivileges` | Required privilege level |
| `accent_color` | `uint32_t` | UI accent color as `0xRRGGBB` |
| `pages[]` | `SiPage[SI_MAX_PAGES]` | Wizard page array |
| `page_count` | `int` | Number of pages |
| `registry[]` | `SiRegEntry[SI_MAX_REG_ENTRIES]` | Registry entries to create on install |
| `registry_count` | `int` | Number of registry entries |
| `shortcuts[]` | `SiShortcutEntry[SI_MAX_SHORTCUTS]` | Shortcuts to create |
| `shortcut_count` | `int` | Number of shortcuts |
| `protocol` | `const wchar_t*` | URL protocol handler to register (e.g., `L"myapp"`) |
| `on_pre_install` | `SiCallback` | Called before file extraction |
| `on_clean_reinstall` | `SiCallback` | Called before a clean reinstall replaces the files: remove what the app keeps outside its folder |
| `on_finished` | `SiCallback` | Called when user clicks Finish |
| `on_uninstall` | `SiCallback` | Called during uninstall |
| `user_data` | `void*` | Passed to all callbacks |
| `is_uninstaller` | `int` | Set to 1 for uninstaller mode |
| `icon` | `HICON` | Custom window and taskbar icon; NULL uses embedded resource ID 1 |
| `transition` | `SiTransition` | Page transition (`SI_FEATURE_ANIMATIONS`): `SI_TRANSITION_NONE`, `SI_TRANSITION_CROSSFADE`, `SI_TRANSITION_SLIDE` |

#### `SiPage`

Wizard page instance. Create using `si_page_*()` constructors — don't initialize manually.

#### `SiFinishedCheckbox`

Checkbox on the finished page.

| Field | Type | Description |
|-------|------|-------------|
| `id` | `const wchar_t*` | Identifier for `si_ctx_checked()` lookup |
| `label` | `const wchar_t*` | Display text |
| `default_checked` | `int` | 1 = checked by default, 0 = unchecked |

#### `SiRegEntry`

Registry entry to create during installation.

| Field | Type | Description |
|-------|------|-------------|
| `root` | `SiRegRoot` | Registry hive |
| `key` | `const wchar_t*` | Registry key path |
| `name` | `const wchar_t*` | Value name |
| `value` | `const wchar_t*` | Value data (`$INSTALLDIR` is expanded at runtime) |

#### `SiShortcutEntry`

Shortcut to create during installation.

| Field | Type | Description |
|-------|------|-------------|
| `location` | `SiSpecialPath` | Where to create the shortcut |
| `name` | `const wchar_t*` | Shortcut display name |

#### `SiPageVtable`

Virtual function table for custom page behavior. All fields are optional (NULL = no-op).

| Field | Signature | Description |
|-------|-----------|-------------|
| `on_create` | `void(SiPage*, HWND)` | Called when page is created |
| `on_destroy` | `void(SiPage*)` | Called when page is destroyed |
| `on_paint` | `void(SiPage*, HDC, RECT)` | Custom painting |
| `on_next` | `int(SiPage*)` | Return 1 to allow Next, 0 to block |
| `on_back` | `int(SiPage*)` | Return 1 to allow Back, 0 to block |
| `on_resize` | `void(SiPage*, int)` | Called on DPI change |
| `on_command` | `int(SiPage*, WPARAM, LPARAM)` | Handle WM_COMMAND |
| `on_app_message` | `void(SiPage*, UINT, WPARAM, LPARAM)` | Handle custom messages |

### Page Constructors

```c
SiPage si_page_welcome(const unsigned char* image_data, size_t image_size);
SiPage si_page_welcome_ex(const unsigned char* image_data, size_t image_size,
                          SiWelcomeCreateFn on_create, SiWelcomeDestroyFn on_destroy,
                          SiWelcomePaintFn on_paint, void* user_data);
SiPage si_page_license(const wchar_t* text, int checked_default);
SiPage si_page_install_dir(void);
SiPage si_page_reinstall(void);
SiPage si_page_installing(void);
SiPage si_page_finished(const SiFinishedCheckbox* cbs, int count);
SiPage si_page_finished_ex(const SiFinishedCheckbox* cbs, int count,
                           const unsigned char* image_data, size_t image_size);
SiPage si_page_confirm(const unsigned char* image_data, size_t image_size);
SiPage si_page_confirm_ex(const unsigned char* image_data, size_t image_size, const wchar_t* text);
SiPage si_page_uninstalling(void);
SiPage si_page_uninstalling_ex(const unsigned char* image_data, size_t image_size);
SiPage si_page_custom(const wchar_t* name, SiCustomPaintFn paint_cb, void* user_data);
```

| Function | Description |
|----------|-------------|
| `si_page_welcome(image_data, image_size)` | Welcome page: "Welcome to $APP Setup" and "This will install $APP on your computer.", with an optional image on the left |
| `si_page_welcome_ex(...)` | Welcome page with your own create, destroy and paint callbacks; `on_paint` replaces the default painting |
| `si_page_license(text, checked_default)` | License agreement; `text` is a key for `L()`, `checked_default` the acceptance checkbox's initial state |
| `si_page_install_dir()` | Directory picker with Browse button and the required and available space |
| `si_page_reinstall()` | Reinstall options; inserted automatically when an existing installation is found |
| `si_page_installing()` | Progress bar shown during file extraction |
| `si_page_finished(cbs, count)` | Completion page with optional checkboxes; pass `NULL, 0` for no checkboxes |
| `si_page_finished_ex(cbs, count, image_data, image_size)` | Completion page with an image on the left; set `data.finished.anim` to decorate it |
| `si_page_confirm(image_data, image_size)` | Uninstall confirmation; pass `NULL, 0` to ask in text, an image shows alone |
| `si_page_confirm_ex(image_data, image_size, text)` | Uninstall confirmation with text under its image; `text` is a key for `L()` |
| `si_page_uninstalling()` | Progress bar shown during uninstall |
| `si_page_uninstalling_ex(image_data, image_size)` | Uninstall progress with an image on its completion screen, which also offers "Delete all app data and settings" (`%APPDATA%\<name>`) |
| `si_page_custom(name, paint_cb, user_data)` | Custom page with your own paint function |

### Context Functions

These are available inside callbacks via the `SiContext*` parameter:

```c
void si_ctx_kill_process(SiContext* ctx, const wchar_t* name);
void si_ctx_launch(SiContext* ctx, const wchar_t* path, const wchar_t* args);
void si_ctx_exec(SiContext* ctx, const wchar_t* cmd);
int  si_ctx_checked(SiContext* ctx, const wchar_t* id);
void si_ctx_install_dir(SiContext* ctx, wchar_t* buf, int buf_len);
void si_ctx_quoted_path(SiContext* ctx, const wchar_t* relative, wchar_t* buf, int buf_len);
void si_ctx_delete_path(SiContext* ctx, const wchar_t* path);
void si_ctx_registry_set(SiContext* ctx, SiRegRoot root, const wchar_t* key,
                         const wchar_t* name, const wchar_t* value);
void si_ctx_registry_delete(SiContext* ctx, SiRegRoot root, const wchar_t* key,
                            const wchar_t* name);
int  si_ctx_registry_exists(SiContext* ctx, SiRegRoot root, const wchar_t* key,
                            const wchar_t* name);
int  si_ctx_download(SiContext* ctx, const wchar_t* url, const wchar_t* dest);  /* SI_FEATURE_DOWNLOAD */
const wchar_t* si_ctx_locale(SiContext* ctx);                                   /* SI_FEATURE_I18N */
```

| Function | Description |
|----------|-------------|
| `si_ctx_kill_process(ctx, name)` | Kill all processes matching the executable name |
| `si_ctx_launch(ctx, path, args)` | Launch a process (non-blocking). `args` may be NULL |
| `si_ctx_exec(ctx, cmd)` | Execute a command line and wait for completion |
| `si_ctx_checked(ctx, id)` | Returns 1 if the finished-page checkbox with `id` is checked |
| `si_ctx_install_dir(ctx, buf, len)` | Copy the resolved install directory into `buf` |
| `si_ctx_quoted_path(ctx, relative, buf, len)` | Build a quoted full path: `"<install_dir>\<relative>"` |
| `si_ctx_delete_path(ctx, path)` | Recursively delete a file or directory tree |
| `si_ctx_registry_set(ctx, ...)` | Create or update a registry string value |
| `si_ctx_registry_delete(ctx, ...)` | Delete a registry value |
| `si_ctx_registry_exists(ctx, ...)` | Returns 1 if a registry value exists |
| `si_ctx_download(ctx, url, dest)` | Download a file via WinHTTP. Returns 1 on success. Requires `SI_FEATURE_DOWNLOAD` |
| `si_ctx_locale(ctx)` | The current locale code, such as `L"en"`. Requires `SI_FEATURE_I18N` |

### Entry Points

```c
int si_main(HINSTANCE hInst);       /* You implement this */
int si_run(SiConfig* cfg, HINSTANCE hInst);  /* Library provides this */
```

- **`si_main`** — You define this function. The library's `WinMainCRTStartup` calls it. Set up your `SiConfig` and call `si_run`.
- **`si_run`** — Creates the wizard window and runs the message loop. Returns 0 on success.

### Memory

```c
void* si_malloc(size_t size);
void* si_realloc(void* ptr, size_t size);
void  si_free(void* ptr);
```

These delegate to `HeapAlloc`/`HeapReAlloc`/`HeapFree` when `SI_NO_CRT` is defined, or to the standard CRT `malloc`/`realloc`/`free` otherwise. Use these for allocations that must work in both CRT and no-CRT modes.

### Internationalization

Requires `SI_FEATURE_I18N`. String tables are compiled by `si_pack` from locale files and embedded in the executable.

```c
const wchar_t* si_tr(const wchar_t* key);  /* Translate key to current locale */
#define L(key) si_tr(key)                   /* Shorthand macro (when SI_FEATURE_I18N is defined) */
```

When `SI_FEATURE_I18N` is **not** defined, `L(key)` is a no-op that passes the string through unchanged (standard wide string literal).

A locale file holds one `key = value` per line, in UTF-8; `\n`, `\r`, `\t` and `\\` are escapes, and lines starting with `#` are comments. The key is usually the English string itself, and `$APP` stands for `SiConfig.name` in the strings the wizard shows. `nativeName` gives the language's own name. [`examples/sample-app/locales`](../examples/sample-app/locales) has the wizard's strings in 30 languages.

The wizard selects the locale that matches the user's Windows locale, then English. A string a locale lacks falls back to the first `--locale` given, so pass English first.

### Command line

| Argument | Effect |
|----------|--------|
| `--silent`, `/silent`, `/S` | Install without a window; the exit code reports the result |
| `--silent-update`, `/update` | The same, then relaunch the app with `SiConfig.silent_update_launch_args` |
| `--launch` | With `--silent`, start the app with `SiConfig.silent_launch_args` once the install has succeeded; a failed launch leaves the exit code alone, and an interactive install ignores the argument |

An uninstaller built with `SI_MODE_UNINSTALL` takes `--silent`, `/silent` or `/S` too: it uninstalls without a window, keeps the app data, and exits with 0, or 1 when a file was in use. Its last files go just after it exits, because a running program cannot delete itself. The installer records this command in `QuietUninstallString`, where deployment tools look for it.

## si_pack Tool

`si_pack` bundles files into LZMA-compressed archives appended to your installer exe.

### Usage

```
si_pack <input.exe> -o <output.exe> --add <file> [--add <file> ...]
        [--locale <code> <locale_path> ...]
si_pack --self-test
```

### Options

| Option | Description |
|--------|-------------|
| `<input.exe>` | The compiled installer/uninstaller stub |
| `-o <output.exe>` | Output packed executable path |
| `--add <file>` | Add a file to the archive (repeatable) |
| `--locale <code> <path>` | Add a locale from a locale file (repeatable). Code is a short identifier like `en`, `de` |
| `--self-test` | Run LZMA compression round-trip verification |

### Example

Run these in PowerShell from the folder containing your compiled programs, with
`si_pack.exe` on your PATH:

```powershell
si_pack MyUninstall.exe -o uninstall.exe --locale en locales/en.txt --locale de locales/de.txt
si_pack MySetup.exe -o MyAppSetup.exe --add MyApp.exe --add uninstall.exe --locale en locales/en.txt --locale de locales/de.txt
```

The installer registers `uninstall.exe` beside the application as its uninstaller, so pack it under that name.

### Archive Format

The packed exe layout:

```
[original exe bytes]
[LZMA-compressed archive]
[uint64_t uncompressed_size]
[compressed string table, with locales]
[SiFooter: magic("SIP2") | archive_offset | archive_size | crc32 | string table offset, sizes and crc32]
```

The archive contains a flat file table:

```
uint32_t file_count
Per file:
    uint16_t name_len
    char[name_len] filename
    uint64_t original_size
    uint64_t offset_into_data_block
[concatenated file data]
```

Locale data is compiled into a binary string table, compressed on its own so the wizard can read it before extracting anything.

## Replacing an existing installation

Before cleanup and extraction, Small Installer terminates running processes whose
executable name matches `SiConfig.exe`. This happens before `on_pre_install`;
choose an application-specific executable name and account for this behavior when
designing your update flow.

If a payload file is still locked, interactive setup shows the file path and Windows
error with Retry and Cancel options. Retry attempts that file again. Cancel stops
setup without showing the finished page or launching the application. Silent setup
returns a nonzero result and does not relaunch the application.

## Architecture

### Header-Only Design

Include `small_installer.h` alone. Define `SI_IMPLEMENTATION` in exactly one C source file to include the implementation. All other files that include the header get only the declarations.

```c
/* installer.c — the one file that gets the implementation */
#define SI_FEATURE_D2D
#define SI_FEATURE_DOWNLOAD
#define SI_FEATURE_I18N
#define SI_MODE_INSTALL
#define SI_IMPLEMENTATION
#include <small_installer/small_installer.h>
```

### No-CRT Approach

When `SI_NO_CRT` is defined (automatic for `si_add_wizard` targets in MinSizeRel builds):

- Memory uses `HeapAlloc`/`HeapFree` via `si_malloc`/`si_free`
- CRT string functions are replaced with hand-rolled implementations (`si_memcpy`, `si_wcslen`, etc.)
- Standard names are redirected via macros (`#define memcpy si_memcpy`)
- The entry point is `WinMainCRTStartup` which calls your `si_main`
- Linker flags: `/NODEFAULTLIB /ENTRY:WinMainCRTStartup`
- Slim UI draws Direct2D's own rounded rectangles, because its continuous corners need the C runtime's math

### Rendering

Slim UI draws the wizard: Direct2D for composited rendering and DirectWrite for text.

### Dependencies

The library links against standard Windows system DLLs only:

| Always | Optional |
|--------|----------|
| user32, gdi32, shell32, ole32, comctl32, shlwapi, advapi32, d2d1, dwrite, dwmapi, msimg32, winmm | winhttp (`SI_FEATURE_DOWNLOAD`) |

Third-party:
- **[Slim UI](https://github.com/live-sound/slim-ui)** (MIT) — the wizard's rendering and controls, a submodule in `external/slim-ui`
- **LZMA SDK** (public domain) — archive compression and decompression, in `third_party/lzma`

## Size

These previously recorded measurements used Visual Studio 2022, x64, `MinSizeRel`. They are illustrative, not a size guarantee for the current build; compiler options, resources and payload files affect the result.

| Setup program | Size |
|---------------|------|
| The [minimal installer](../README.md#configure-your-installer) installer: three pages, a manifest, one small file, no uninstaller | 122.1 KB |
| The same with an uninstaller | 168.8 KB |
| The example, `SampleAppSetup.exe` | 381.6 KB |

The first row is the minimum: the wizard, its Direct2D drawing and the LZMA decoder are always there, and whatever you pack adds its compressed size on top. A minimal uninstaller is 100 KB, which compresses to 47 KB in the archive.

| Feature flag | Adds to each program |
|--------------|----------------------|
| `SI_FEATURE_DARK_MODE` | 1 KB |
| `SI_FEATURE_PNG` | 2 KB |
| `SI_FEATURE_I18N` | 3 KB |
| `SI_FEATURE_ANIMATIONS` | 7.5 KB |
| `SI_FEATURE_DOWNLOAD` | Nothing until the installer calls it |
| `SI_FEATURE_ALL_CONTROLS` | 68.5 KB |

The example's 381.6 KB:

| Part | Size |
|------|------|
| **Installer program** | **185.5 KB** |
| The library with PNG, i18n, dark mode and animations | 132.5 KB |
| The example's own code | 10 KB |
| Icon, welcome image, manifest and version information | 43 KB |
| **Archive** | **164.2 KB** |
| The uninstaller program, 49 KB of it two images | 108.1 KB |
| The uninstaller's copy of the translations | 32.4 KB |
| The application, most of it its icon | 23.7 KB |
| **The installer's translations**, 30 languages | **31.9 KB** |

