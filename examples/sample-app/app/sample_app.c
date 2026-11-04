/* The application the example installs. It says that it runs and, opened from a sample-app:// link, shows the link. */
#include <windows.h>
#include <wchar.h>

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command_line, int show) {
    wchar_t message[1024];
    const wchar_t* link = wcsstr(GetCommandLineW(), L"sample-app:");
    (void)instance; (void)previous; (void)command_line; (void)show;
    if (link) wsprintfW(message, L"Sample App is running.\n\nOpened from %s", link);
    else wsprintfW(message, L"Sample App is running.");
    MessageBoxW(NULL, message, L"Sample App", MB_OK | MB_ICONINFORMATION);
    return 0;
}
