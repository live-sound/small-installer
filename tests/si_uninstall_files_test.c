#include <windows.h>
#include <stdio.h>
#include <wchar.h>

#define SI_FEATURE_D2D
#define SI_MODE_UNINSTALL
#define SI_IMPLEMENTATION
#include <small_installer/small_installer.h>

int si_main(HINSTANCE instance) {
    (void)instance;
    return 0;
}

static int failures;
static unsigned int progress_calls;
static unsigned int previous_completed;
static unsigned int reported_total;

static void check(int condition, const char* message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s (error %lu)\n", message, GetLastError());
        failures++;
    }
}

static int join_path(wchar_t* output, const wchar_t* left, const wchar_t* right) {
    size_t left_len = wcslen(left), right_len = wcslen(right);
    if (left_len + 1 + right_len >= MAX_PATH) return 0;
    wcscpy(output, left);
    wcscat(output, L"\\");
    wcscat(output, right);
    return 1;
}

static int create_file(const wchar_t* path) {
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    CloseHandle(file);
    return 1;
}

static void record_progress(unsigned int completed, unsigned int total) {
    check(completed == previous_completed + 1, "file progress advances exactly once per file");
    previous_completed = completed;
    reported_total = total;
    progress_calls++;
}

int main(void) {
    wchar_t temp[MAX_PATH], root[MAX_PATH], self[MAX_PATH], first[MAX_PATH];
    wchar_t nested[MAX_PATH], second[MAX_PATH], deep[MAX_PATH], third[MAX_PATH];
    unsigned int total, completed = 0;

    check(GetTempPathW(MAX_PATH, temp) > 0, "resolve temp directory");
    check(GetTempFileNameW(temp, L"siu", 0, root) != 0, "reserve test directory");
    check(DeleteFileW(root) && CreateDirectoryW(root, NULL), "create test directory");
    check(join_path(self, root, L"uninstall.exe") && create_file(self), "create retained self");
    check(join_path(first, root, L"app.exe") && create_file(first), "create root file");
    check(join_path(nested, root, L"assets") && CreateDirectoryW(nested, NULL), "create nested directory");
    check(join_path(second, nested, L"model.bin") && create_file(second), "create nested file");
    check(join_path(deep, nested, L"locales") && CreateDirectoryW(deep, NULL), "create deep directory");
    check(join_path(third, deep, L"en.txt") && create_file(third), "create deep file");
    check(SetFileAttributesW(third, FILE_ATTRIBUTE_READONLY), "make deep file read-only");

    total = si_uninstall_count_files(root, self);
    check(total == 3, "count every removable file while excluding the running uninstaller");
    si_uninstall_delete_files(root, self, total, &completed, record_progress);

    check(completed == total, "report one progress unit for each handled file");
    check(progress_calls == total, "publish progress after every handled file");
    check(reported_total == total, "publish the pre-counted file total with every update");
    check(GetFileAttributesW(first) == INVALID_FILE_ATTRIBUTES, "delete root file");
    check(GetFileAttributesW(second) == INVALID_FILE_ATTRIBUTES, "delete nested file");
    check(GetFileAttributesW(third) == INVALID_FILE_ATTRIBUTES, "delete read-only file");
    check(GetFileAttributesW(nested) == INVALID_FILE_ATTRIBUTES, "remove emptied directories");
    check(GetFileAttributesW(self) != INVALID_FILE_ATTRIBUTES, "retain running uninstaller for cleanup handoff");

    DeleteFileW(self);
    RemoveDirectoryW(root);
    if (failures) return 1;
    printf("Uninstall file progress tests passed\n");
    return 0;
}
