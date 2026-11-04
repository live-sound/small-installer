/* The installer si_silent_launch_test runs, with no archive to extract. Copied as si_silent_launch_app.exe it is also the app that
 * installer starts, and writes which launch arguments it got to launched.txt beside itself. */
#define SI_FEATURE_D2D
#define SI_IMPLEMENTATION
#include <small_installer/small_installer.h>

#define APP_EXE L"si_silent_launch_app.exe"

static int record_launch(wchar_t* self) {
    const char* started = si_cmdline_has_arg(L"--from-update") ? "update"
        : si_cmdline_has_arg(L"--from-install") ? "install" : "no arguments";
    DWORD written = 0;
    HANDLE file;
    wcscpy(wcsrchr(self, L'\\') + 1, L"launched.txt");
    file = CreateFileW(self, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) return 1;
    WriteFile(file, started, (DWORD)lstrlenA(started), &written, NULL);
    CloseHandle(file);
    return 0;
}

int si_main(HINSTANCE instance) {
    static wchar_t self[MAX_PATH], dir[MAX_PATH];
    SiConfig cfg = {0};
    GetModuleFileNameW(NULL, self, MAX_PATH);
    if (lstrcmpiW(wcsrchr(self, L'\\') + 1, APP_EXE) == 0) return record_launch(self);
    GetEnvironmentVariableW(L"SI_TEST_INSTALL_DIR", dir, MAX_PATH);
    cfg.name   = L"Silent Launch Test";
    cfg.version = L"1.0.0";
    cfg.app_id = L"{3B7C1E52-9D46-4A0F-8E21-6F5A2C9D7B14}";
    cfg.exe    = APP_EXE;
    cfg.silent_update_launch_args = L"--from-update";
    cfg.silent_launch_args = L"--from-install";
    cfg.install_dir_override = dir;
    cfg.privileges = SI_PRIV_PER_USER;
    return si_run(&cfg, instance);
}
