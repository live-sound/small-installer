/* The uninstaller si_silent_uninstall_test runs: it removes the folder it sits in. */
#define SI_FEATURE_D2D
#define SI_MODE_UNINSTALL
#define SI_IMPLEMENTATION
#include <small_installer/small_installer.h>

int si_main(HINSTANCE instance) {
    SiConfig cfg = {0};
    cfg.name   = L"Silent Uninstall Test";
    cfg.app_id = L"{7E2D1F6A-3B4C-4D5E-9F60-718293A4B5C6}";
    cfg.is_uninstaller = 1;
    cfg.pages[0] = si_page_confirm(NULL, 0);
    cfg.pages[1] = si_page_uninstalling();
    cfg.page_count = 2;
    return si_run(&cfg, instance);
}
