/* `uninstall.exe --silent`, for real: a folder in the temp directory is uninstalled with no window, and the exit code tells whether a file
 * stayed. A window here would mean the wizard ran, so the test fails on one, and kills the process rather than wait for a click. */
#include <windows.h>
#include <stdio.h>
#include <wchar.h>

static int failures;

static void check(int condition, const char* message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s (error %lu)\n", message, GetLastError());
        failures++;
    }
}

typedef struct WindowSearch { DWORD pid; int found; } WindowSearch;

static BOOL CALLBACK find_window(HWND hwnd, LPARAM data) {
    WindowSearch* search = (WindowSearch*)data;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == search->pid && IsWindowVisible(hwnd)) search->found = 1;
    return TRUE;
}

static void write_file(const wchar_t* path) {
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD written;
    check(file != INVALID_HANDLE_VALUE, "create a file to uninstall");
    WriteFile(file, "data", 4, &written, NULL);
    CloseHandle(file);
}

/* A folder holding the uninstaller, a file and a nested file. */
static void make_install(const wchar_t* dir, wchar_t* exe) {
    wchar_t path[MAX_PATH];
    CreateDirectoryW(dir, NULL);
    swprintf(exe, MAX_PATH, L"%ls\\uninstall.exe", dir);
    check(CopyFileW(SI_TEST_UNINSTALLER, exe, FALSE), "copy the uninstaller");
    swprintf(path, MAX_PATH, L"%ls\\data.txt", dir);
    write_file(path);
    swprintf(path, MAX_PATH, L"%ls\\nested", dir);
    CreateDirectoryW(path, NULL);
    swprintf(path, MAX_PATH, L"%ls\\nested\\more.txt", dir);
    write_file(path);
}

/* Runs it silently and returns its exit code; a window fails the test. */
static DWORD run_silent(const wchar_t* exe) {
    wchar_t command[MAX_PATH + 32];
    STARTUPINFOW startup = {sizeof(startup)};
    PROCESS_INFORMATION process = {0};
    WindowSearch search = {0};
    DWORD code = 99, waited = 0;
    swprintf(command, MAX_PATH + 32, L"\"%ls\" --silent", exe);
    check(CreateProcessW(NULL, command, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &process), "start the uninstaller");
    search.pid = process.dwProcessId;
    while (WaitForSingleObject(process.hProcess, 20) == WAIT_TIMEOUT) {
        EnumWindows(find_window, (LPARAM)&search);
        waited += 20;
        if (search.found || waited > 30000) { TerminateProcess(process.hProcess, 98); break; }
    }
    check(!search.found, "a silent uninstall opens no window");
    WaitForSingleObject(process.hProcess, 5000);
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return code;
}

/* The uninstaller cannot delete itself: a script does, just after it exits. */
static int wait_until_gone(const wchar_t* dir) {
    int i;
    for (i = 0; i < 200; i++) {
        if (GetFileAttributesW(dir) == INVALID_FILE_ATTRIBUTES) { Sleep(500); return 1; } /* and the script's own exit */
        Sleep(100);
    }
    return 0;
}

int main(void) {
    wchar_t temp[MAX_PATH], dir[MAX_PATH], exe[MAX_PATH], path[MAX_PATH];
    HANDLE locked;
    GetTempPathW(MAX_PATH, temp);
    swprintf(dir, MAX_PATH, L"%lssi-silent-uninstall-%lu", temp, GetCurrentProcessId());

    make_install(dir, exe);
    check(run_silent(exe) == 0, "everything removed exits with 0");
    swprintf(path, MAX_PATH, L"%ls\\data.txt", dir);
    check(GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES, "the file is gone at exit");
    check(wait_until_gone(dir), "the folder is gone after the cleanup");

    make_install(dir, exe);
    swprintf(path, MAX_PATH, L"%ls\\data.txt", dir);
    locked = CreateFileW(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    check(locked != INVALID_HANDLE_VALUE, "hold a file open");
    check(run_silent(exe) == 1, "a file in use exits with 1");
    CloseHandle(locked);
    check(wait_until_gone(dir), "the cleanup removes it once it is free");

    if (failures) return 1;
    printf("silent uninstall: ok\n");
    return 0;
}
