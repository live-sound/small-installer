/* The Sample App installer: a welcome page with an image, a license, a folder choice, progress, and a finished page with two options
 * and confetti, in the language of the user's Windows. build.ps1 appends the application, the uninstaller and the locales to this
 * stub with si_pack. */
#define SI_FEATURE_D2D
#define SI_FEATURE_PNG
#define SI_FEATURE_I18N
#define SI_FEATURE_DARK_MODE
#define SI_FEATURE_ANIMATIONS
#define SI_MODE_INSTALL
/* SI_NO_CRT comes from si_add_wizard in MinSizeRel builds. */
#define SI_IMPLEMENTATION
#include <small_installer/small_installer.h>

#include "confetti.h"
#include "welcome_bubbles.h"
#include "version.h"

#define SAMPLE_MIN_WINDOWS_BUILD 19041u /* Windows 10 version 2004 */
#define SAMPLE_KEY         L"Software\\Sample App"
#define SAMPLE_RUN_KEY     L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#define SAMPLE_STARTUP_KEY L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run"

/* A file installer.rc embeds as RCDATA. */
static const unsigned char* sample_resource(HINSTANCE instance, int id, DWORD* size) {
    HRSRC found = FindResourceW(instance, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10));
    HGLOBAL loaded = found ? LoadResource(instance, found) : NULL;
    *size = loaded ? SizeofResource(instance, found) : 0;
    return loaded ? (const unsigned char*)LockResource(loaded) : NULL;
}

/* GetVersionEx reports the version in the manifest; RtlGetVersion reports the real one. */
static DWORD sample_windows_build(void) {
    typedef LONG (WINAPI *RtlGetVersionFn)(OSVERSIONINFOW*);
    OSVERSIONINFOW version;
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    RtlGetVersionFn rtl_get_version = ntdll ? (RtlGetVersionFn)GetProcAddress(ntdll, "RtlGetVersion") : NULL;
    ZeroMemory(&version, sizeof(version));
    version.dwOSVersionInfoSize = sizeof(version);
    return rtl_get_version && rtl_get_version(&version) == 0 ? version.dwBuildNumber : 0;
}

/* A silent install reports the refusal only through its exit code. */
static int sample_windows_is_supported(void) {
    DWORD build = sample_windows_build();
    wchar_t message[512];
    if (!build || build >= SAMPLE_MIN_WINDOWS_BUILD) return 1;
    if (!si_cmdline_has_arg(L"--silent") && !si_cmdline_has_arg(L"/silent") && !si_cmdline_has_arg(L"/S") &&
        !si_cmdline_has_arg(L"--silent-update") && !si_cmdline_has_arg(L"/update")) {
        wsprintfW(message, L(L"unsupportedWindowsBody"), (unsigned long)SAMPLE_MIN_WINDOWS_BUILD, L"Windows", (unsigned long)build);
        MessageBoxW(NULL, message, L(L"unsupportedWindowsTitle"), MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
    }
    return 0;
}

/* Before the files are written: the application can read the language the user installed it in. */
static void sample_pre_install(SiContext* ctx, void* user_data) {
    (void)user_data;
    si_ctx_registry_set(ctx, SI_REG_HKCU, SAMPLE_KEY, L"InstallerLanguage", si_ctx_locale(ctx));
}

/* A clean reinstall (always offered when a newer version is installed): what the application keeps outside its folder goes too. */
static void sample_clean_reinstall(SiContext* ctx, void* user_data) {
    wchar_t path[MAX_PATH];
    (void)user_data;
    si_get_known_folder(&FOLDERID_RoamingAppData, path, MAX_PATH);
    if (path[0]) {
        wcscat(path, L"\\Sample App");
        si_ctx_delete_path(ctx, path);
    }
    si_ctx_registry_delete(ctx, SI_REG_HKCU, SAMPLE_KEY, NULL);
    si_ctx_registry_delete(ctx, SI_REG_HKCU, SAMPLE_RUN_KEY, L"Sample App");
    si_ctx_registry_delete(ctx, SI_REG_HKCU, SAMPLE_STARTUP_KEY, L"Sample App");
}

/* The finished page's options, after the user clicks Finish. */
static void sample_finished(SiContext* ctx, void* user_data) {
    wchar_t path[MAX_PATH];
    (void)user_data;
    if (si_ctx_checked(ctx, L"launch")) {
        si_ctx_install_dir(ctx, path, MAX_PATH);
        wcscat(path, L"\\SampleApp.exe");
        si_ctx_launch(ctx, path, NULL);
    }
    if (si_ctx_checked(ctx, L"startup")) {
        si_ctx_quoted_path(ctx, L"SampleApp.exe", path, MAX_PATH);
        si_ctx_registry_set(ctx, SI_REG_HKCU, SAMPLE_RUN_KEY, L"Sample App", path);
    }
}

int si_main(HINSTANCE instance) {
    SiFinishedCheckbox options[] = {
        { L"launch",  L"Launch $APP",             1 },
        { L"startup", L"Start $APP with Windows", 0 },
    };
    SiConfig cfg = {0};
    const unsigned char* image;
    DWORD image_size;

    si_init_dpi_awareness();
    si_init_i18n();
    if (!sample_windows_is_supported()) return ERROR_OLD_WIN_VERSION;

    cfg.name         = L"Sample App";
    cfg.version      = SAMPLE_VERSION_W;
    cfg.publisher    = L"Sample Publisher";
    cfg.app_id       = L"{6C1B7F52-0D3E-4A7C-9E44-2B8A51D7C3E1}"; /* make your own: it identifies the installed app */
    cfg.exe          = L"SampleApp.exe";
    cfg.install_base = SI_PATH_LOCAL_APP_DATA;
    cfg.install_sub  = L"Sample App";
    cfg.privileges   = SI_PRIV_PER_USER;
    cfg.accent_color = 0x3B62C4;
    cfg.transition   = SI_TRANSITION_CROSSFADE;

    image = sample_resource(instance, 102, &image_size);
    cfg.pages[0] = sample_welcome_page(image, image_size);
    cfg.pages[1] = si_page_license(L"licenseText", 0);
    cfg.pages[2] = si_page_install_dir();
    cfg.pages[3] = si_page_installing();
    cfg.pages[4] = si_page_finished_ex(options, 2, image, image_size);
    cfg.pages[4].data.finished.anim.attach = confetti_attach;
    cfg.pages[4].data.finished.anim.paint  = confetti_paint;
    cfg.pages[4].data.finished.anim.detach = confetti_detach;
    cfg.page_count = 5;

    cfg.registry[0].root  = SI_REG_HKCU;
    cfg.registry[0].key   = SAMPLE_KEY;
    cfg.registry[0].name  = L"InstallDir";
    cfg.registry[0].value = L"$INSTALLDIR";
    cfg.registry[1].root  = SI_REG_HKCU;
    cfg.registry[1].key   = SAMPLE_KEY;
    cfg.registry[1].name  = L"Version";
    cfg.registry[1].value = SAMPLE_VERSION_W;
    cfg.registry_count = 2;

    cfg.protocol = L"sample-app"; /* sample-app:// links open the application */

    cfg.shortcuts[0].location = SI_PATH_START_MENU;
    cfg.shortcuts[0].name     = L"Sample App";
    cfg.shortcut_count = 1;

    cfg.on_pre_install     = sample_pre_install;
    cfg.on_clean_reinstall = sample_clean_reinstall;
    cfg.on_finished        = sample_finished;

    {
        int result = si_run(&cfg, instance);
        sample_bubbles_stop(NULL, NULL);
        return result;
    }
}
