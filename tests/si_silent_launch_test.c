/* `setup.exe --silent --launch`, for real: si_silent_installer installs into a folder in the temp directory, and the app it starts records
 * its arguments. Each install runs in a job, which empties once everything it started has exited, so "nothing started" needs no timed wait. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#define ARP_KEY L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{3B7C1E52-9D46-4A0F-8E21-6F5A2C9D7B14}"
#define WAIT_MS 30000

static int failures;

static void check(int condition, const char* message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s (error %lu)\n", message, GetLastError());
        failures++;
    }
}

static int wait_until_empty(HANDLE job) {
    DWORD start = GetTickCount();
    for (;;) {
        JOBOBJECT_BASIC_ACCOUNTING_INFORMATION info;
        if (!QueryInformationJobObject(job, JobObjectBasicAccountingInformation, &info, sizeof(info), NULL)) return 0;
        if (info.ActiveProcesses == 0) return 1;
        if (GetTickCount() - start > WAIT_MS) return 0;
        Sleep(5);
    }
}

/* What the app recorded, or "" when nothing started. */
static void read_record(const wchar_t* path, char* text, DWORD size) {
    DWORD read = 0;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (file != INVALID_HANDLE_VALUE) {
        if (!ReadFile(file, text, size - 1, &read, NULL)) read = 0;
        CloseHandle(file);
    }
    text[read] = 0;
}

static void run_install(const wchar_t* args, const wchar_t* record, const char* expected, const char* message) {
    wchar_t command[MAX_PATH + 64];
    char text[32];
    STARTUPINFOW startup = {sizeof(startup)};
    PROCESS_INFORMATION process = {0};
    HANDLE job = CreateJobObjectW(NULL, NULL);
    DWORD code = 99;

    DeleteFileW(record);
    swprintf(command, MAX_PATH + 64, L"\"%ls\" %ls", SI_TEST_INSTALLER, args);
    check(CreateProcessW(NULL, command, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &startup, &process), "start the installer");
    check(AssignProcessToJobObject(job, process.hProcess), "put the installer in a job");
    ResumeThread(process.hThread);
    check(WaitForSingleObject(process.hProcess, WAIT_MS) == WAIT_OBJECT_0, "the installer exits");
    GetExitCodeProcess(process.hProcess, &code);
    check(code == 0, "the install succeeds");
    check(wait_until_empty(job), "everything the installer started exits");
    read_record(record, text, sizeof(text));
    check(strcmp(text, expected) == 0, message);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    CloseHandle(job);
}

int main(void) {
    wchar_t temp[MAX_PATH], dir[MAX_PATH], app[MAX_PATH], record[MAX_PATH];
    GetTempPathW(MAX_PATH, temp);
    swprintf(dir, MAX_PATH, L"%lssi-silent-launch-%lu", temp, GetCurrentProcessId());
    swprintf(app, MAX_PATH, L"%ls\\si_silent_launch_app.exe", dir);
    swprintf(record, MAX_PATH, L"%ls\\launched.txt", dir);
    check(CreateDirectoryW(dir, NULL), "create the install folder");
    check(CopyFileW(SI_TEST_INSTALLER, app, FALSE), "place the app");
    check(SetEnvironmentVariableW(L"SI_TEST_INSTALL_DIR", dir), "name the install folder");

    run_install(L"--silent", record, "", "--silent starts nothing");
    run_install(L"--silent --launch", record, "install", "--launch starts the app with its arguments");
    run_install(L"--silent-update", record, "update", "--silent-update relaunches the app");
    run_install(L"--silent-update --launch", record, "update", "an update keeps its own launch arguments");

    RegDeleteTreeW(HKEY_CURRENT_USER, ARP_KEY);
    DeleteFileW(record);
    DeleteFileW(app);
    check(RemoveDirectoryW(dir), "remove the install folder");

    if (failures) return 1;
    printf("silent launch: ok\n");
    return 0;
}
