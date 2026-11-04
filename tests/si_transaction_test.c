#include <windows.h>
#include <shellapi.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static void* si_malloc(size_t size) { return malloc(size); }
static void* si_realloc(void* pointer, size_t size) { return realloc(pointer, size); }
static void si_free(void* pointer) { free(pointer); }

#include <small_installer/si_transaction.h>

static int failures;

static void check(int condition, const char* message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s (error %lu)\n", message, GetLastError());
        failures++;
    }
}

static int join_path(wchar_t* output, const wchar_t* left, const wchar_t* right) {
    if (wcslen(left) + 1 + wcslen(right) >= MAX_PATH) return 0;
    wcscpy(output, left);
    wcscat(output, L"\\");
    wcscat(output, right);
    return 1;
}

static int write_bytes(const wchar_t* path, const char* bytes) {
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD size = (DWORD)strlen(bytes), written = 0;
    int ok;
    if (file == INVALID_HANDLE_VALUE) return 0;
    ok = WriteFile(file, bytes, size, &written, NULL) && written == size;
    CloseHandle(file);
    return ok;
}

static int file_equals(const wchar_t* path, const char* expected) {
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE |
                              FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    char buffer[128];
    DWORD read = 0;
    size_t expected_size = strlen(expected);
    if (file == INVALID_HANDLE_VALUE || expected_size >= sizeof(buffer)) return 0;
    if (!ReadFile(file, buffer, sizeof(buffer), &read, NULL)) {
        CloseHandle(file);
        return 0;
    }
    CloseHandle(file);
    return read == expected_size && memcmp(buffer, expected, expected_size) == 0;
}

static int add_staged(SiInstallTransaction* transaction,
                      const wchar_t* relative, const char* bytes) {
    wchar_t staged[MAX_PATH];
    return si_txn_add_file(transaction, relative, staged) &&
           write_bytes(staged, bytes);
}

static int cancel_retry(void* context, const wchar_t* path, DWORD error) {
    int* calls = (int*)context;
    (void)path;
    (void)error;
    (*calls)++;
    return 0;
}

static int make_case_root(wchar_t* root, const wchar_t* suffix) {
    wchar_t temp[MAX_PATH], unique[MAX_PATH];
    DWORD length = GetTempPathW(MAX_PATH, temp);
    if (length == 0 || length >= MAX_PATH ||
        !GetTempFileNameW(temp, L"sit", 0, unique) ||
        !DeleteFileW(unique) || !CreateDirectoryW(unique, NULL) ||
        !join_path(root, unique, suffix) || !CreateDirectoryW(root, NULL)) return 0;
    return 1;
}

static void remove_case_root(const wchar_t* root) {
    wchar_t parent[MAX_PATH];
    wchar_t* slash;
    si_txn_delete_tree(root);
    if (!si_txn_copy_path(parent, root)) return;
    slash = wcsrchr(parent, L'\\');
    if (slash) {
        *slash = L'\0';
        si_txn_delete_tree(parent);
    }
}

static void test_normal_success(void) {
    wchar_t root[MAX_PATH], target[MAX_PATH], old_path[MAX_PATH], extra_path[MAX_PATH];
    wchar_t nested[MAX_PATH], nested_file[MAX_PATH], new_path[MAX_PATH], work[MAX_PATH];
    SiInstallTransaction transaction;
    if (!make_case_root(root, L"normal-success") ||
        !join_path(target, root, L"app") || !CreateDirectoryW(target, NULL) ||
        !join_path(old_path, target, L"app.exe") ||
        !join_path(extra_path, target, L"keep.dat") ||
        !join_path(nested, target, L"assets") || !CreateDirectoryW(nested, NULL) ||
        !join_path(nested_file, nested, L"old.dat") ||
        !write_bytes(old_path, "old-app") || !write_bytes(extra_path, "keep") ||
        !write_bytes(nested_file, "old-asset")) {
        check(0, "normal success setup");
        return;
    }
    check(si_txn_begin(&transaction, target, 0), "begin normal transaction");
    wcscpy(work, transaction.work);
    check(add_staged(&transaction, L"app.exe", "new-app"), "stage replacement");
    check(add_staged(&transaction, L"new\\data.bin", "new-data"), "stage nested file");
    check(si_txn_commit(&transaction, NULL, NULL), "commit normal transaction");
    join_path(new_path, target, L"new\\data.bin");
    check(file_equals(old_path, "new-app"), "normal commit publishes replacement");
    check(file_equals(extra_path, "keep"), "normal commit preserves unrelated file");
    check(file_equals(nested_file, "old-asset"), "normal commit preserves unrelated tree");
    check(file_equals(new_path, "new-data"), "normal commit publishes nested file");
    check(!si_txn_path_exists(work), "normal commit removes rollback directory");
    remove_case_root(root);
}

static void test_cancel_restores_partial_commit(void) {
    wchar_t root[MAX_PATH], target[MAX_PATH], first[MAX_PATH], locked[MAX_PATH], work[MAX_PATH];
    SiInstallTransaction transaction;
    HANDLE lock;
    int retry_calls = 0;
    if (!make_case_root(root, L"cancel") ||
        !join_path(target, root, L"app") || !CreateDirectoryW(target, NULL) ||
        !join_path(first, target, L"first.dll") ||
        !join_path(locked, target, L"locked.dll") ||
        !write_bytes(first, "old-first") || !write_bytes(locked, "old-locked")) {
        check(0, "cancel setup");
        return;
    }
    check(si_txn_begin(&transaction, target, 0), "begin cancel transaction");
    wcscpy(work, transaction.work);
    check(add_staged(&transaction, L"first.dll", "new-first"), "stage first file");
    check(add_staged(&transaction, L"locked.dll", "new-locked"), "stage locked file");
    lock = CreateFileW(locked, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL, NULL);
    check(lock != INVALID_HANDLE_VALUE, "lock second destination");
    check(!si_txn_commit(&transaction, cancel_retry, &retry_calls),
          "Cancel rejects partial commit");
    check(retry_calls == 1, "Cancel callback invoked once");
    check(file_equals(first, "old-first"), "Cancel restores already replaced file");
    check(file_equals(locked, "old-locked"), "Cancel preserves locked file");
    check(!si_txn_path_exists(work), "Cancel removes rollback directory after restore");
    if (lock != INVALID_HANDLE_VALUE) CloseHandle(lock);
    remove_case_root(root);
}

static void test_fresh_failure_restores_absence(void) {
    wchar_t root[MAX_PATH], target[MAX_PATH], work[MAX_PATH];
    SiInstallTransaction transaction;
    if (!make_case_root(root, L"fresh-failure") ||
        !join_path(target, root, L"app")) {
        check(0, "fresh failure setup");
        return;
    }
    check(!si_txn_path_exists(target), "fresh target starts absent");
    check(si_txn_begin(&transaction, target, 0), "begin fresh transaction");
    wcscpy(work, transaction.work);
    check(add_staged(&transaction, L"app.exe", "new"), "stage fresh file");
    transaction.fail_before_file = 0;
    check(!si_txn_commit(&transaction, NULL, NULL), "force fresh commit failure");
    check(!si_txn_path_exists(target), "failed fresh install removes target directory");
    check(!si_txn_path_exists(work), "failed fresh install removes rollback directory");
    remove_case_root(root);
}

static void test_clean_failure_restores_tree(void) {
    wchar_t root[MAX_PATH], target[MAX_PATH], app[MAX_PATH], extra[MAX_PATH];
    wchar_t subdir[MAX_PATH], nested[MAX_PATH], created[MAX_PATH], work[MAX_PATH];
    SiInstallTransaction transaction;
    if (!make_case_root(root, L"clean-failure") ||
        !join_path(target, root, L"app") || !CreateDirectoryW(target, NULL) ||
        !join_path(app, target, L"app.exe") || !join_path(extra, target, L"extra.dat") ||
        !join_path(subdir, target, L"state") || !CreateDirectoryW(subdir, NULL) ||
        !join_path(nested, subdir, L"nested.dat") ||
        !write_bytes(app, "old-app") || !write_bytes(extra, "old-extra") ||
        !write_bytes(nested, "old-nested")) {
        check(0, "clean failure setup");
        return;
    }
    check(si_txn_begin(&transaction, target, 1), "begin clean transaction");
    wcscpy(work, transaction.work);
    check(add_staged(&transaction, L"app.exe", "new-app"), "stage clean replacement");
    check(add_staged(&transaction, L"new\\created.dat", "created"),
          "stage clean nested file");
    transaction.fail_before_file = 1;
    check(!si_txn_commit(&transaction, NULL, NULL), "force clean commit failure");
    join_path(created, target, L"new\\created.dat");
    check(file_equals(app, "old-app"), "clean failure restores app");
    check(file_equals(extra, "old-extra"), "clean failure restores extra file");
    check(file_equals(nested, "old-nested"), "clean failure restores old directory tree");
    check(!si_txn_path_exists(created), "clean failure removes new file");
    check(!si_txn_path_exists(work), "clean failure removes rollback directory");
    remove_case_root(root);
}

static void test_clean_success_replaces_tree(void) {
    wchar_t root[MAX_PATH], target[MAX_PATH], app[MAX_PATH], extra[MAX_PATH];
    wchar_t created[MAX_PATH], work[MAX_PATH];
    SiInstallTransaction transaction;
    if (!make_case_root(root, L"clean-success") ||
        !join_path(target, root, L"app") || !CreateDirectoryW(target, NULL) ||
        !join_path(app, target, L"app.exe") || !join_path(extra, target, L"extra.dat") ||
        !write_bytes(app, "old-app") || !write_bytes(extra, "old-extra")) {
        check(0, "clean success setup");
        return;
    }
    check(si_txn_begin(&transaction, target, 1), "begin successful clean transaction");
    wcscpy(work, transaction.work);
    check(add_staged(&transaction, L"app.exe", "new-app"), "stage clean app");
    check(add_staged(&transaction, L"new\\created.dat", "created"), "stage clean child");
    check(si_txn_commit(&transaction, NULL, NULL), "commit clean transaction");
    join_path(created, target, L"new\\created.dat");
    check(file_equals(app, "new-app"), "clean commit publishes app");
    check(file_equals(created, "created"), "clean commit publishes child");
    check(!si_txn_path_exists(extra), "clean commit removes prior extra file");
    check(!si_txn_path_exists(work), "clean commit removes rollback directory");
    remove_case_root(root);
}

static void test_rejects_unsafe_paths(void) {
    wchar_t root[MAX_PATH], target[MAX_PATH], staged[MAX_PATH];
    SiInstallTransaction transaction;
    if (!make_case_root(root, L"unsafe") || !join_path(target, root, L"app")) {
        check(0, "unsafe path setup");
        return;
    }
    check(si_txn_begin(&transaction, target, 0), "begin unsafe path transaction");
    check(!si_txn_add_file(&transaction, L"..\\escape.dll", staged),
          "reject parent traversal");
    check(!si_txn_add_file(&transaction, L"C:\\escape.dll", staged),
          "reject absolute drive path");
    check(!si_txn_add_file(&transaction, L"folder\\.\\escape.dll", staged),
          "reject dot segment");
    si_txn_abort(&transaction);
    remove_case_root(root);
}

int main(void) {
    test_normal_success();
    test_cancel_restores_partial_commit();
    test_fresh_failure_restores_absence();
    test_clean_failure_restores_tree();
    test_clean_success_replaces_tree();
    test_rejects_unsafe_paths();
    if (failures) {
        fprintf(stderr, "%d transactional installer assertion(s) failed\n", failures);
        return 1;
    }
    puts("small-installer transaction tests passed");
    return 0;
}
