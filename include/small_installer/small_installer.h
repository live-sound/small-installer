#ifndef SMALL_INSTALLER_H
#define SMALL_INSTALLER_H

#ifndef _WIN32
#error "Small Installer is Windows-only"
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <stdint.h>
#include <stddef.h>

/* D2D/DWrite headers are C++ only; we use opaque void* and manual COM vtables in C */

/* ── Limits ─────────────────────────────────────────────────────────────── */
#define SI_MAX_PAGES        8   /** Maximum wizard pages per config */
#define SI_MAX_REG_ENTRIES  16  /** Maximum registry entries */
#define SI_MAX_SHORTCUTS    4   /** Maximum shortcut entries */
#define SI_MAX_CHECKBOXES   4   /** Maximum checkboxes on the finished page */
#define SI_MAX_NAME         256 /** Maximum string length for names */
#define SI_MAX_LOCALES      32  /** Maximum supported locales */
#define SI_MAX_KEYS         512 /** Maximum i18n translation keys */

/* ── I18N macro ─────────────────────────────────────────────────────────── */
#ifdef SI_FEATURE_I18N
/** Translate a key to the current locale's string. */
const wchar_t* si_tr(const wchar_t* key);
/** Shorthand: resolves key via compiled string table at runtime. */
#define L(key) si_tr(key)
#else
/** No-op passthrough when SI_FEATURE_I18N is not defined. */
#define L(key) (key)
#endif

/* ── Memory allocation (always available, delegates to HeapAlloc or CRT) ── */
/** Allocate memory (HeapAlloc when SI_NO_CRT, else malloc). */
void* si_malloc(size_t size);
/** Reallocate memory (HeapReAlloc when SI_NO_CRT, else realloc). */
void* si_realloc(void* ptr, size_t size);
/** Free memory (HeapFree when SI_NO_CRT, else free). */
void  si_free(void* ptr);

/* ── Custom CRT stubs (for /NODEFAULTLIB builds) ────────────────────────── */
#ifdef SI_NO_CRT

#ifdef __cplusplus
extern "C" {
#endif

void* si_memcpy(void* dst, const void* src, size_t n);
void* si_memset(void* dst, int val, size_t n);
size_t si_wcslen(const wchar_t* s);
int si_wcsicmp(const wchar_t* a, const wchar_t* b);
int si_wcscmp(const wchar_t* a, const wchar_t* b);
wchar_t* si_wcscpy(wchar_t* dst, const wchar_t* src);
wchar_t* si_wcscat(wchar_t* dst, const wchar_t* src);
wchar_t* si_wcsrchr(const wchar_t* s, wchar_t c);
wchar_t* si_wcsstr(const wchar_t* hay, const wchar_t* needle);
wchar_t* si_wcsncpy(wchar_t* dst, const wchar_t* src, size_t n);
int si_strcmp(const char* a, const char* b);

/* Redirect standard names */
#define memcpy  si_memcpy
#define memset  si_memset
#define wcslen  si_wcslen
#define _wcsicmp si_wcsicmp
#define wcscmp  si_wcscmp
#define wcscpy  si_wcscpy
#define wcscat  si_wcscat
#define wcsrchr si_wcsrchr
#define wcsstr  si_wcsstr
#define wcsncpy si_wcsncpy
#define strcmp   si_strcmp

#ifdef __cplusplus
}
#endif

#endif /* SI_NO_CRT */

/* ── Feature dependency checks ─────────────────────────────────────────── */
/* Controls are always SlimUI/D2D — no Win32 fallback. */
#ifndef SI_FEATURE_D2D
#error "small-installer requires SI_FEATURE_D2D (SlimUI/D2D rendering is mandatory)"
#endif
#if defined(SI_FEATURE_ANIMATIONS) && !defined(SI_FEATURE_D2D)
#error "SI_FEATURE_ANIMATIONS requires SI_FEATURE_D2D"
#endif

/* ── Slim UI integration ───────────────────────────────────────────────── */
#ifdef SI_FEATURE_PNG
#define SUI_FEATURE_PNG
#endif
/* SlimUI custom controls are always on. */
#define SUI_FEATURE_CUSTOM_CONTROLS
/* A wizard is made of buttons, check boxes, radio buttons, edit boxes and text views. The other controls would only add to every
 * installer; a custom page that wants one defines SI_FEATURE_ALL_CONTROLS. */
#ifndef SI_FEATURE_ALL_CONTROLS
#define SUI_NO_COMBOBOX
#define SUI_NO_TABS
#define SUI_NO_LIST
#define SUI_NO_NAV_LIST
#define SUI_NO_SPINNER
#define SUI_NO_SLIDER
#define SUI_NO_COLOR_SWATCH
#define SUI_NO_IMAGE
#define SUI_NO_SHORTCUT_RECORDER
#define SUI_NO_TOOLTIP
#define SUI_NO_SCROLLVIEW
#endif
#ifdef SI_FEATURE_ANIMATIONS
#define SUI_FEATURE_ANIMATIONS
#define SUI_FEATURE_VSYNC
#endif
#ifdef SI_NO_CRT
#define SUI_NO_CRT
#endif
/* When the caller defines SI_IMPLEMENTATION, we also pull in the Slim UI
 * implementation in one shot (single inclusion via the include guard). */
#ifdef SI_IMPLEMENTATION
#define SUI_IMPLEMENTATION
#endif

/* Match the wizard's paragraph buffers and allow a full Windows destination path. */
#ifndef SUI_CTRL_TEXT_MAX
#define SUI_CTRL_TEXT_MAX 512
#endif
#include <slim_ui/slim_ui.h>

/* ── Font sizes (points) ───────────────────────────────────────────────── */
#define SI_TITLE_PT    22.0f
#define SI_SUBTITLE_PT 11.0f
#define SI_CONTROL_PT  11.0f

/* ── Bridge: old Si* names -> Slim UI Sui* types ─────────────────────── */
typedef SuiRenderer    SiRenderer;
typedef SuiColorScheme SiColorScheme;
typedef SuiImageHandle SiImageHandle;

typedef SuiControl     SiControl;
typedef SuiControlHost SiControlHost;
#define SiCtrlType     SuiCtrlType
#define SiCtrlClickFn  SuiCtrlClickFn

#define SI_CTRL_BUTTON       SUI_CTRL_BUTTON
#define SI_CTRL_CHECKBOX     SUI_CTRL_CHECKBOX
#define SI_CTRL_RADIO        SUI_CTRL_RADIO
#define SI_CTRL_NORMAL   SUI_CTRL_NORMAL
#define SI_CTRL_HOVER    SUI_CTRL_HOVER
#define SI_CTRL_PRESSED  SUI_CTRL_PRESSED
#define SI_CTRL_DISABLED SUI_CTRL_DISABLED
#define SI_CTRL_FOCUSED  SUI_CTRL_FOCUSED
#define SI_CTRL_CHECKED  SUI_CTRL_CHECKED
#define SI_CTRL_HIDDEN   SUI_CTRL_HIDDEN
#define SI_CTRL_DEFAULT  SUI_CTRL_DEFAULT
#define SI_CTRL_BOLD     SUI_CTRL_BOLD
#define SI_MAX_CONTROLS  SUI_MAX_CONTROLS

/* Color scheme field bridges (old field names) */
#define edit_bg       input_bg
#define edit_border   input_border
#define edit_text     input_text

/* ── Enums ──────────────────────────────────────────────────────────────── */

/** Base path for installation or shortcut placement. */
typedef enum SiSpecialPath {
    SI_PATH_LOCAL_APP_DATA = 0, /** %LOCALAPPDATA% (per-user) */
    SI_PATH_PROGRAM_FILES,      /** %ProgramFiles% (system-wide) */
    SI_PATH_DESKTOP,            /** User's desktop folder */
    SI_PATH_START_MENU,         /** Start Menu programs folder */
    SI_PATH_INSTALL_DIR         /** Resolved install directory (for shortcuts) */
} SiSpecialPath;

/** Installation privilege level. */
typedef enum SiPrivileges {
    SI_PRIV_PER_USER = 0, /** Install for current user only (no UAC) */
    SI_PRIV_ADMIN         /** Require administrator privileges */
} SiPrivileges;

/** How the uninstaller binary is placed. */
typedef enum SiUninstallerMode {
    SI_UNINST_EMBEDDED = 0, /** Uninstaller is embedded in the archive */
    SI_UNINST_SELF_COPY     /** Installer copies itself as the uninstaller */
} SiUninstallerMode;

/** Registry hive for registry operations. */
typedef enum SiRegRoot {
    SI_REG_HKCU = 0, /** HKEY_CURRENT_USER */
    SI_REG_HKLM,     /** HKEY_LOCAL_MACHINE (32-bit view) */
    SI_REG_HKLM64    /** HKEY_LOCAL_MACHINE (64-bit view) */
} SiRegRoot;

/** Wizard page type identifier. */
typedef enum SiPageType {
    SI_PAGE_NONE = 0,       /** Empty/uninitialized */
    SI_PAGE_LICENSE,         /** License agreement with checkbox */
    SI_PAGE_INSTALL_DIR,     /** Install directory picker */
    SI_PAGE_REINSTALL,       /** Existing-install reinstall options */
    SI_PAGE_INSTALLING,      /** Progress bar during file extraction */
    SI_PAGE_FINISHED,        /** Completion page with optional checkboxes */
    SI_PAGE_CONFIRM,         /** Uninstall confirmation (with optional image) */
    SI_PAGE_UNINSTALLING,    /** Uninstall progress bar */
    SI_PAGE_WELCOME,         /** Welcome page with optional image on the left */
    SI_PAGE_CUSTOM           /** User-defined page with custom paint callback */
} SiPageType;

#ifdef SI_FEATURE_ANIMATIONS
typedef enum SiTransition {
    SI_TRANSITION_NONE = 0,
    SI_TRANSITION_CROSSFADE,
    SI_TRANSITION_SLIDE,
} SiTransition;
#endif

/* ── Forward declarations ───────────────────────────────────────────────── */

typedef struct SiContext   SiContext;
typedef struct SiPage      SiPage;
typedef struct SiConfig    SiConfig;
/* SiRenderer, SiColorScheme, SiImageHandle, SiControl, SiControlHost,
 * SiCtrlClickFn are now bridged from Slim UI (see typedef/macros above) */

/* ── Callback types ─────────────────────────────────────────────────────── */

/** General callback: receives context and user data pointer. */
typedef void (*SiCallback)(SiContext* ctx, void* user_data);
/** Custom page paint callback: draw into the given DC and area. */
typedef void (*SiCustomPaintFn)(HDC dc, RECT area, void* user_data);

/* ── Page vtable ────────────────────────────────────────────────────────── */

/** Virtual function table for custom page behavior. All fields are optional (NULL = no-op). */
typedef struct SiPageVtable {
    void (*on_create)(SiPage* page, HWND parent);   /** Called when page is created */
    void (*on_destroy)(SiPage* page);                /** Called when page is destroyed */
    void (*on_paint)(SiPage* page, HDC dc, RECT area); /** Custom painting */
    int  (*on_next)(SiPage* page);    /** Return 1 to allow Next, 0 to block */
    int  (*on_back)(SiPage* page);    /** Return 1 to allow Back, 0 to block */
    void (*on_resize)(SiPage* page, int dpi);        /** Called on DPI change */
    int  (*on_command)(SiPage* page, WPARAM wp, LPARAM lp); /** Handle WM_COMMAND */
    void (*on_app_message)(SiPage* page, UINT msg, WPARAM wp, LPARAM lp); /** Handle custom messages */
} SiPageVtable;

/* ── Finished page checkbox ─────────────────────────────────────────────── */

/** Checkbox shown on the finished page. */
typedef struct SiFinishedCheckbox {
    const wchar_t* id;          /** Identifier for si_ctx_checked() lookup */
    const wchar_t* label;       /** Display text */
    int default_checked;         /** 1 = checked by default, 0 = unchecked */
} SiFinishedCheckbox;

/* ── Page data union (internal per-page state) ─────────────────────────── */

/** Internal state for the license page. */
typedef struct SiPageLicenseData {
    const wchar_t* text;  /** License text content */
    HWND hEdit;           /** Read-only edit control */
    HWND hCheck;          /** "I accept" checkbox */
    int checked_default;  /** 1 = checkbox checked by default */
} SiPageLicenseData;

/** Internal state for the install directory page. */
typedef struct SiPageInstallDirData {
    HWND hEdit;    /** Path edit control */
    HWND hBrowse;  /** Browse button */
} SiPageInstallDirData;

/** Internal state for the installing (progress) page. */
typedef struct SiPageInstallingData {
    int installing;  /** 1 while extraction is running */
    int done;        /** 1 when extraction is complete */
    int cc_progress;        /** Progress percentage 0-100 for D2D rendering */
    wchar_t cc_status[256]; /** Status text for D2D rendering */
} SiPageInstallingData;

/* Forward decl for animation callbacks. Full definition is in si_core.h
 * (private; only visible under SI_IMPLEMENTATION). */
struct SiWizardState;
typedef struct SiWizardState SiWizardState;

/** Animation hook — app supplies these to decorate the finished page.
 * `attach`  — called once when the page is shown (set up state, timers).
 * `paint`   — called from the page paint cycle; rt is `ID2D1RenderTarget*`.
 * `detach`  — called when the page is destroyed (free state, kill timers).
 * Any field may be NULL. The library does not own `user`.
 */
typedef struct SiFinishedAnim {
    void* user;
    void (*attach)(void* user, SiWizardState* st, float area_w, float area_h, float dpi_scale);
    void (*paint)(void* user, SiWizardState* st, void* render_target, float area_h);
    void (*detach)(void* user);
} SiFinishedAnim;

/** Internal state for the finished page. */
typedef struct SiPageFinishedData {
    SiFinishedCheckbox checkboxes[SI_MAX_CHECKBOXES]; /** Checkbox definitions */
    int checkbox_count;                                /** Number of checkboxes */
    const unsigned char* image_data; /** Optional PNG image data (left side) */
    size_t image_size;               /** Size of image_data in bytes */
    void* img_handle;                /** Decoded image handle (internal) */
    int img_width;                   /** Decoded image width */
    int img_height;                  /** Decoded image height */
    SiFinishedAnim anim;             /** Optional decoration callbacks (zero-init = no anim) */
} SiPageFinishedData;

/** Internal state for the uninstall confirmation page. */
typedef struct SiPageConfirmData {
    const unsigned char* image_data; /** PNG image data (may be NULL) */
    size_t image_size;               /** Size of image_data in bytes */
    const wchar_t* text;             /** Optional key for L(): the text under the image */
    void* img_handle;                /** Decoded image handle (internal) */
    int img_width;                   /** Decoded image width */
    int img_height;                  /** Decoded image height */
} SiPageConfirmData;

/** Internal state for the uninstalling (progress) page. */
typedef struct SiPageUninstallingData {
    int uninstalling;  /** 1 while removal is running */
    int done;          /** 1 when removal is complete */
    const unsigned char* image_data; /** Optional PNG for completion screen */
    size_t image_size;               /** Size of image_data in bytes */
    void* img_handle;                /** Decoded image handle (internal) */
    int img_width;                   /** Decoded image width */
    int img_height;                  /** Decoded image height */
    float cc_progress;      /** Smooth displayed progress percentage 0-100 */
    float cc_progress_target; /** Latest worker phase/file target percentage */
    int cc_completion_pending; /** Worker finished; reveal completion after displayed 100% */
    uint64_t cc_progress_tick_ms; /** Last UI-thread animation tick */
    wchar_t cc_status[256]; /** Status text for D2D rendering */
} SiPageUninstallingData;

/** Welcome page callback signatures. */
typedef void (*SiWelcomeCreateFn)(SiPage* page, HWND parent, void* user_data);
typedef void (*SiWelcomeDestroyFn)(SiPage* page, void* user_data);
typedef void (*SiWelcomePaintFn)(SiPage* page, HDC dc, RECT area, void* user_data);

/** Internal state for the welcome page. */
typedef struct SiPageWelcomeData {
    const unsigned char* image_data; /** Optional PNG image data (left side) */
    size_t image_size;               /** Size of image_data in bytes */
    void* img_handle;                /** Decoded image handle (internal) */
    int img_width;                   /** Decoded image width */
    int img_height;                  /** Decoded image height */
    SiWelcomeCreateFn  on_create;    /** Optional: called after default create */
    SiWelcomeDestroyFn on_destroy;   /** Optional: called before default destroy */
    SiWelcomePaintFn   on_paint;     /** Optional: replaces default paint entirely */
    void* user_data;                 /** User data passed to callbacks */
} SiPageWelcomeData;

/** Internal state for a custom page. */
typedef struct SiPageCustomData {
    const wchar_t* name;          /** Page display name */
    SiCustomPaintFn paint_cb;     /** User paint callback */
    void* paint_user_data;        /** User data for paint_cb */
} SiPageCustomData;

/* ── Page struct ────────────────────────────────────────────────────────── */

/** Wizard page instance. Create via si_page_*() constructors. */
struct SiPage {
    SiPageType type;            /** Page type identifier */
    const SiPageVtable* vtable; /** Virtual function table */
    union {
        SiPageLicenseData     license;
        SiPageInstallDirData  install_dir;
        SiPageInstallingData  installing;
        SiPageFinishedData    finished;
        SiPageConfirmData     confirm;
        SiPageUninstallingData uninstalling;
        SiPageWelcomeData     welcome;
        SiPageCustomData      custom;
    } data;
};

/* ── Registry entry ─────────────────────────────────────────────────────── */

/** Registry entry to create during installation. */
typedef struct SiRegEntry {
    SiRegRoot root;         /** Registry hive */
    const wchar_t* key;     /** Registry key path */
    const wchar_t* name;    /** Value name */
    const wchar_t* value;   /** Value data ($INSTALLDIR is expanded at runtime) */
} SiRegEntry;

/* ── Shortcut entry ─────────────────────────────────────────────────────── */

/** Shortcut (.lnk) to create during installation. */
typedef struct SiShortcutEntry {
    SiSpecialPath location; /** Where to create the shortcut */
    const wchar_t* name;    /** Shortcut display name */
} SiShortcutEntry;

/* SiImageHandle, SiRenderer — now provided by Slim UI (bridged above) */

/* ── Config ─────────────────────────────────────────────────────────────── */

/** Main configuration struct. Zero-initialize with {0}, then set fields. */
struct SiConfig {
    const wchar_t* name;       /** Application display name */
    const wchar_t* version;    /** Version string (e.g., L"1.0.0") */
    const wchar_t* publisher;  /** Publisher name for Add/Remove Programs */
    const wchar_t* app_id;     /** Unique GUID for registry identification */
    const wchar_t* exe;        /** Main executable filename */
    const wchar_t* silent_update_launch_args; /** Optional arguments used only for the post-update relaunch. */
    const wchar_t* silent_launch_args; /** Optional arguments for the launch that --launch requests after a silent install. */

    SiSpecialPath install_base; /** Base directory for installation */
    const wchar_t* install_sub; /** Subdirectory under install_base */
    const wchar_t* install_dir_override; /** Optional fully-resolved install directory. */

    SiPrivileges privileges;    /** Required privilege level */
    uint32_t accent_color;      /** UI accent color as 0xRRGGBB */

    /* Pages */
    SiPage pages[SI_MAX_PAGES]; /** Wizard page array */
    int page_count;             /** Number of pages */

    /* Registry */
    SiRegEntry registry[SI_MAX_REG_ENTRIES]; /** Registry entries to create on install */
    int registry_count;                      /** Number of registry entries */

    /* Shortcuts */
    SiShortcutEntry shortcuts[SI_MAX_SHORTCUTS]; /** Shortcuts to create */
    int shortcut_count;                          /** Number of shortcuts */

    /* Protocol handler */
    const wchar_t* protocol;   /** URL protocol to register (e.g., L"myapp") */

    /* Callbacks */
    SiCallback on_pre_install;  /** Called before file extraction */
    SiCallback on_clean_reinstall; /** Called before transactional clean file replacement */
    SiCallback on_finished;     /** Called when user clicks Finish */
    SiCallback on_uninstall;    /** Called during uninstall */
    void* user_data;            /** Passed to all callbacks */

    /* Uninstaller mode */
    int is_uninstaller;         /** Set to 1 for uninstaller mode */

    /* Window icon (optional) */
    HICON icon;                 /** Custom window/taskbar icon. If NULL, uses embedded resource ID 1. */

#ifdef SI_FEATURE_ANIMATIONS
    SiTransition transition;
#endif
};

/* ── Context (passed to callbacks) ──────────────────────────────────────── */

/** Opaque context passed to callbacks. Provides helper functions for install/uninstall actions. */
struct SiContext {
    void* _state;  /* internal: pointer to SiWizardState */
};

/** Kill all processes matching the given executable name. */
void si_ctx_kill_process(SiContext* ctx, const wchar_t* name);
/** Launch a process (non-blocking). args may be NULL. */
void si_ctx_launch(SiContext* ctx, const wchar_t* path, const wchar_t* args);
/** Execute a command line and wait for completion. */
void si_ctx_exec(SiContext* ctx, const wchar_t* cmd);
/** Returns 1 if the finished-page checkbox with the given id is checked. */
int  si_ctx_checked(SiContext* ctx, const wchar_t* id);
/** Copy the resolved install directory path into buf. */
void si_ctx_install_dir(SiContext* ctx, wchar_t* buf, int buf_len);
/** Build a quoted full path: "<install_dir>\<relative>". */
void si_ctx_quoted_path(SiContext* ctx, const wchar_t* relative, wchar_t* buf, int buf_len);
/** Recursively delete a file or directory tree. Intended for app-specific clean reinstall callbacks. */
void si_ctx_delete_path(SiContext* ctx, const wchar_t* path);
/** Create or update a registry string value. */
void si_ctx_registry_set(SiContext* ctx, SiRegRoot root, const wchar_t* key,
                         const wchar_t* name, const wchar_t* value);
/** Delete a registry value. */
void si_ctx_registry_delete(SiContext* ctx, SiRegRoot root, const wchar_t* key,
                            const wchar_t* name);
/** Returns 1 if the specified registry value exists. */
int  si_ctx_registry_exists(SiContext* ctx, SiRegRoot root, const wchar_t* key,
                            const wchar_t* name);

#ifdef SI_FEATURE_DOWNLOAD
/** Download via WinHTTP. Absolute destinations are used as-is; relative paths
 * are resolved below the install directory. Returns 1 on success. */
int si_ctx_download(SiContext* ctx, const wchar_t* url, const wchar_t* dest);
#endif

#ifdef SI_FEATURE_I18N
/** Get the current locale code (e.g. L"en", L"fr"). Returns L"en" if i18n not loaded. */
const wchar_t* si_ctx_locale(SiContext* ctx);
#endif

/* ── Page constructors ──────────────────────────────────────────────────── */

/** Create a license agreement page. text=key for L(), checked=1 to pre-check "I accept". */
SiPage si_page_license(const wchar_t* text, int checked_default);
/** Create an install directory picker page. */
SiPage si_page_install_dir(void);
/** Create a reinstall options page. Usually inserted automatically when an existing app_id install is found. */
SiPage si_page_reinstall(void);
/** Create a progress bar page shown during file extraction. */
SiPage si_page_installing(void);
/** Create a completion page with optional checkboxes. Pass NULL, 0 for no checkboxes. */
SiPage si_page_finished(const SiFinishedCheckbox* cbs, int count);
/** Create an installation-complete page with an image on the left side. */
SiPage si_page_finished_ex(const SiFinishedCheckbox* cbs, int count, const unsigned char* image_data, size_t image_size);
/** Create an uninstall confirmation page. Pass NULL, 0 for no image: the page then asks in text. An image shows alone. */
SiPage si_page_confirm(const unsigned char* image_data, size_t image_size);
/** Create an uninstall confirmation page with text under its image. text=key for L(); $APP is expanded. */
SiPage si_page_confirm_ex(const unsigned char* image_data, size_t image_size, const wchar_t* text);
/** Create a progress bar page shown during uninstall. */
SiPage si_page_uninstalling(void);
/** Create a progress bar page shown during uninstall, with a completion image. */
SiPage si_page_uninstalling_ex(const unsigned char* image_data, size_t image_size);
/** Create a welcome page with an optional image on the left side. */
SiPage si_page_welcome(const unsigned char* image_data, size_t image_size);
/** Create a welcome page with custom create/destroy/paint callbacks.
 *  on_create is called after default image loading.
 *  on_destroy is called before default image cleanup.
 *  on_paint, if set, replaces the default paint entirely. */
SiPage si_page_welcome_ex(const unsigned char* image_data, size_t image_size,
    SiWelcomeCreateFn on_create, SiWelcomeDestroyFn on_destroy,
    SiWelcomePaintFn on_paint, void* user_data);
/** Create a custom page with your own paint callback. */
SiPage si_page_custom(const wchar_t* name, SiCustomPaintFn paint_cb, void* user_data);

/* ── Main entry point ───────────────────────────────────────────────────── */

/** Create the wizard window and run the message loop. Returns 0 on success. */
int si_run(SiConfig* cfg, HINSTANCE hInst);

/* ── User-defined entry point (called by WinMainCRTStartup) ─────────── */

/** You implement this: set up SiConfig and call si_run(). */
int si_main(HINSTANCE hInst);

/* ── Entry point (no CRT) ───────────────────────────────────────────────── */
#ifdef SI_NO_CRT
/** No-CRT entry point. Calls si_main(). Defined by the library. */
void WINAPI WinMainCRTStartup(void);
#endif

/* ═══════════════════════════════════════════════════════════════════════════
   IMPLEMENTATION
   ═══════════════════════════════════════════════════════════════════════════ */

#ifdef SI_IMPLEMENTATION

/* Slim UI implementation was already pulled in via the header-level include
 * (SUI_IMPLEMENTATION is defined when SI_IMPLEMENTATION is defined). */

/* ── Function bridges: si_* -> sui_* ───────────────────────────────────── */
#define si_renderer_init              sui_renderer_init
#define si_renderer_release           sui_renderer_release
#define si_renderer_resize            sui_renderer_resize
#define si_renderer_begin             sui_renderer_begin
#define si_renderer_end               sui_renderer_end
#define si_renderer_clear             sui_renderer_clear
#define si_renderer_set_font          sui_renderer_set_font
#define si_renderer_set_dc            sui_renderer_set_dc
#define si_renderer_is_init           sui_renderer_is_init
#define si_renderer_fill_rect         sui_renderer_fill_rect
#define si_renderer_fill_rounded_rect sui_renderer_fill_rounded_rect
#define si_renderer_draw_rounded_rect sui_renderer_draw_rounded_rect
#define si_renderer_draw_text         sui_renderer_draw_text
#define si_renderer_draw_text_centered sui_renderer_draw_text_centered
#define si_renderer_draw_line         sui_renderer_draw_line
#define si_renderer_draw_progress     sui_renderer_draw_progress
#define si_renderer_draw_image        sui_renderer_draw_image
#define si_renderer_load_image        sui_renderer_load_image
#define si_renderer_free_image        sui_renderer_free_image
#define si_renderer_measure_text_height sui_renderer_measure_text_height
#define si_renderer_init_fields       sui_renderer_init_fields
#define si_get_window_dpi             sui_get_window_dpi
#define si_scheme_light               sui_scheme_light
#define si_scheme_dark                sui_scheme_dark
#define si_sinf                       sui_sinf
#define si_cosf                       sui_cosf

#define si_ctrl_host_init       sui_ctrl_host_init
#define si_ctrl_add             sui_ctrl_add
#define si_ctrl_add_button      sui_ctrl_add_button
#define si_ctrl_add_checkbox    sui_ctrl_add_checkbox
#define si_ctrl_add_radio       sui_ctrl_add_radio
#define si_ctrl_add_textview    sui_ctrl_add_textview
#define si_ctrl_clear_page      sui_ctrl_clear_page
#define si_ctrl_hit_test        sui_ctrl_hit_test
#define si_ctrl_find            sui_ctrl_find
#define si_ctrl_draw_all        sui_ctrl_draw_all
#define si_ctrl_on_mouse_move   sui_ctrl_on_mouse_move
#define si_ctrl_on_lbutton_down sui_ctrl_on_lbutton_down
#define si_ctrl_on_lbutton_up   sui_ctrl_on_lbutton_up
#define si_ctrl_on_mouse_wheel  sui_ctrl_on_mouse_wheel
#define si_ctrl_on_keydown      sui_ctrl_on_keydown
#define si_ctrl_on_keyup        sui_ctrl_on_keyup
#define si_ctrl_on_char         sui_ctrl_on_char
#define si_ctrl_dpi_scale       sui_ctrl_dpi_scale
#define si_ctrl_next_focusable  sui_ctrl_next_focusable
#define si_ctrl_set_focus       sui_ctrl_set_focus
#define si_ctrl_host_release    sui_ctrl_host_release
#define si_ctrl_set_tooltip     sui_ctrl_set_tooltip
#define si_uia_handle_getobject sui_uia_handle_getobject

#include <shellapi.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shlguid.h>
#include <tlhelp32.h>
/* Vblank pacing (DwmFlush + worker thread) — gated via SUI_FEATURE_VSYNC,
 * pulled in by slim_ui.h further up. dwmapi.h is included by sui_vsync.h. */

#if defined(SI_MODE_INSTALL) || defined(SI_FEATURE_I18N)
#include "LzmaDec.h"
#define SI_HAS_PAYLOAD
#endif

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ole32.lib")

/* Sub-headers — D2D/renderer/controls now provided by Slim UI above */
#include "si_d2d.h"        /* Redirects to Slim UI */
#include "si_core.h"       /* CRT stubs, CRC32, payload, I18N, DPI, wizard state, registry/shortcut/process helpers, download, context */
#include "si_renderer.h"   /* Redirects to Slim UI */
#include "si_controls.h"   /* Redirects to Slim UI */
#include "si_animations.h" /* Page transitions */
#include "si_pages.h"      /* All page implementations + page constructors */
#include "si_wndproc.h"    /* Wizard state methods, WndProc, si_run */

#endif /* SI_IMPLEMENTATION */

#endif /* SMALL_INSTALLER_H */
