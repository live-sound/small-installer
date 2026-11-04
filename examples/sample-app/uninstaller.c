/* The Sample App uninstaller: a confirmation page, then progress. The library removes the files, the shortcuts, the protocol handler
 * and the Programs entry it finds in cfg; the callback removes what the installer's callbacks added. */
#define SI_FEATURE_D2D
#define SI_FEATURE_PNG
#define SI_FEATURE_I18N
#define SI_FEATURE_DARK_MODE
#define SI_MODE_UNINSTALL
/* SI_NO_CRT comes from si_add_wizard in MinSizeRel builds. */
#define SI_IMPLEMENTATION
#include <small_installer/small_installer.h>

/* A file uninstaller.rc embeds as RCDATA. */
static const unsigned char* sample_resource(HINSTANCE instance, int id, DWORD* size) {
    HRSRC found = FindResourceW(instance, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10));
    HGLOBAL loaded = found ? LoadResource(instance, found) : NULL;
    *size = loaded ? SizeofResource(instance, found) : 0;
    return loaded ? (const unsigned char*)LockResource(loaded) : NULL;
}

static void sample_uninstall(SiContext* ctx, void* user_data) {
    (void)user_data;
    si_ctx_kill_process(ctx, L"SampleApp.exe");
    si_ctx_registry_delete(ctx, SI_REG_HKCU, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", L"Sample App");
    si_ctx_registry_delete(ctx, SI_REG_HKCU, L"Software\\Sample App", NULL);
}

int si_main(HINSTANCE instance) {
    SiConfig cfg = {0};
    const unsigned char* confirm_image;
    const unsigned char* finished_image;
    DWORD confirm_size, finished_size;

    cfg.name   = L"Sample App";
    cfg.app_id = L"{6C1B7F52-0D3E-4A7C-9E44-2B8A51D7C3E1}"; /* the installer's */
    cfg.exe    = L"SampleApp.exe";
    cfg.is_uninstaller = 1;
    cfg.protocol = L"sample-app";
    cfg.shortcuts[0].location = SI_PATH_START_MENU;
    cfg.shortcuts[0].name     = L"Sample App";
    cfg.shortcut_count = 1;

    confirm_image  = sample_resource(instance, 101, &confirm_size);
    finished_image = sample_resource(instance, 102, &finished_size);
    cfg.pages[0] = si_page_confirm_ex(confirm_image, confirm_size, L"Are you sure you want to uninstall $APP?");
    cfg.pages[1] = si_page_uninstalling_ex(finished_image, finished_size);
    cfg.page_count = 2;

    cfg.on_uninstall = sample_uninstall;

    return si_run(&cfg, instance);
}
