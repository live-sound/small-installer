#ifndef SI_TRANSACTION_H
#define SI_TRANSACTION_H

/* Transactional payload replacement for the streaming installer.
 *
 * Payload files are decompressed into a sibling directory on the same volume.
 * Commit moves each old destination into that directory before publishing its
 * staged replacement. A failure or user cancellation restores the published
 * subset in reverse order. Same-volume renames keep the normal commit path
 * allocation-free and avoid copying existing installations. */

typedef struct SiTxnFile {
    wchar_t destination[MAX_PATH];
    wchar_t staged[MAX_PATH];
    wchar_t backup[MAX_PATH];
    int had_original;
    int backed_up;
    int installed;
} SiTxnFile;

typedef struct SiTxnPath {
    wchar_t path[MAX_PATH];
    int moved;
} SiTxnPath;

typedef struct SiInstallTransaction {
    wchar_t target[MAX_PATH];
    wchar_t work[MAX_PATH];
    wchar_t clean_backup[MAX_PATH];
    SiTxnFile* files;
    int file_count;
    int file_capacity;
    SiTxnPath* created_dirs;
    int created_dir_count;
    int created_dir_capacity;
    SiTxnPath* clean_items;
    int clean_item_count;
    int clean_item_capacity;
    int target_existed;
    int clean_install;
    int clean_prepared;
    int active;
    /* Tests can force a failure before publishing this zero-based entry. */
    int fail_before_file;
} SiInstallTransaction;

typedef int (*SiTxnRetryFn)(void* context, const wchar_t* path, DWORD error);

static int si_txn_copy_path(wchar_t* dest, const wchar_t* source) {
    size_t length;
    if (!dest || !source) return 0;
    length = wcslen(source);
    if (length >= MAX_PATH) return 0;
    wcscpy(dest, source);
    return 1;
}

static int si_txn_append_text(wchar_t* dest, const wchar_t* text) {
    size_t used, extra;
    if (!dest || !text) return 0;
    used = wcslen(dest);
    extra = wcslen(text);
    if (used + extra >= MAX_PATH) return 0;
    wcscat(dest, text);
    return 1;
}

static int si_txn_append_uint(wchar_t* dest, uint64_t value) {
    wchar_t digits[24];
    int count = 0, i;
    do {
        digits[count++] = (wchar_t)(L'0' + (value % 10));
        value /= 10;
    } while (value && count < (int)(sizeof(digits) / sizeof(digits[0])));
    if (wcslen(dest) + (size_t)count >= MAX_PATH) return 0;
    for (i = count - 1; i >= 0; --i) {
        size_t used = wcslen(dest);
        dest[used] = digits[i];
        dest[used + 1] = L'\0';
    }
    return 1;
}

static int si_txn_path_exists(const wchar_t* path) {
    return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
}

static int si_txn_remove_file_retry(const wchar_t* path) {
    int attempt;
    DWORD attributes = GetFileAttributesW(path);
    if (attributes == INVALID_FILE_ATTRIBUTES) return 1;
    if (attributes & FILE_ATTRIBUTE_READONLY)
        SetFileAttributesW(path, attributes & ~FILE_ATTRIBUTE_READONLY);
    for (attempt = 0; attempt < 100; ++attempt) {
        if (DeleteFileW(path) || GetLastError() == ERROR_FILE_NOT_FOUND) return 1;
        Sleep(20);
    }
    return 0;
}

static int si_txn_remove_dir_retry(const wchar_t* path) {
    int attempt;
    if (!si_txn_path_exists(path)) return 1;
    for (attempt = 0; attempt < 100; ++attempt) {
        if (RemoveDirectoryW(path) || GetLastError() == ERROR_PATH_NOT_FOUND) return 1;
        Sleep(20);
    }
    return 0;
}

static int si_txn_move_retry(const wchar_t* source, const wchar_t* destination) {
    int attempt;
    for (attempt = 0; attempt < 100; ++attempt) {
        if (MoveFileExW(source, destination,
                        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            return 1;
        }
        Sleep(20);
    }
    return 0;
}

static int si_txn_delete_tree(const wchar_t* path) {
    WIN32_FIND_DATAW data;
    wchar_t search[MAX_PATH], child[MAX_PATH];
    HANDLE find;
    int ok = 1;
    DWORD attributes = GetFileAttributesW(path);
    if (attributes == INVALID_FILE_ATTRIBUTES) return 1;
    if (!(attributes & FILE_ATTRIBUTE_DIRECTORY)) return si_txn_remove_file_retry(path);
    /* Never follow a junction or directory symlink moved out of an old install. */
    if (attributes & FILE_ATTRIBUTE_REPARSE_POINT) return si_txn_remove_dir_retry(path);
    if (!si_txn_copy_path(search, path) || !si_txn_append_text(search, L"\\*")) return 0;
    find = FindFirstFileW(search, &data);
    if (find != INVALID_HANDLE_VALUE) {
        do {
            if (wcscmp(data.cFileName, L".") == 0 ||
                wcscmp(data.cFileName, L"..") == 0) continue;
            if (!si_txn_copy_path(child, path) || !si_txn_append_text(child, L"\\") ||
                !si_txn_append_text(child, data.cFileName)) {
                ok = 0;
                continue;
            }
            if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (!si_txn_delete_tree(child)) ok = 0;
            } else if (!si_txn_remove_file_retry(child)) {
                ok = 0;
            }
        } while (FindNextFileW(find, &data));
        FindClose(find);
    }
    if (!si_txn_remove_dir_retry(path)) ok = 0;
    return ok && !si_txn_path_exists(path);
}

static int si_txn_valid_relative_path(const wchar_t* path) {
    const wchar_t* segment;
    const wchar_t* cursor;
    if (!path || !path[0] || path[0] == L'\\' || path[0] == L'/' ||
        path[1] == L':') return 0;
    segment = path;
    for (cursor = path;; ++cursor) {
        wchar_t ch = *cursor;
        if (ch == L':' || ch == L'\0' || ch == L'\\' || ch == L'/') {
            size_t length = (size_t)(cursor - segment);
            if (length == 0 || (length == 1 && segment[0] == L'.') ||
                (length == 2 && segment[0] == L'.' && segment[1] == L'.')) {
                return 0;
            }
            if (ch == L'\0') break;
            segment = cursor + 1;
        }
    }
    return 1;
}

static int si_txn_reserve_files(SiInstallTransaction* transaction, int required) {
    SiTxnFile* next;
    int capacity;
    if (required <= transaction->file_capacity) return 1;
    capacity = transaction->file_capacity ? transaction->file_capacity * 2 : 16;
    while (capacity < required) capacity *= 2;
    next = (SiTxnFile*)si_realloc(transaction->files,
                                  (size_t)capacity * sizeof(SiTxnFile));
    if (!next) return 0;
    transaction->files = next;
    transaction->file_capacity = capacity;
    return 1;
}

static int si_txn_reserve_paths(SiTxnPath** paths, int* capacity, int required) {
    SiTxnPath* next;
    int next_capacity;
    if (required <= *capacity) return 1;
    next_capacity = *capacity ? *capacity * 2 : 8;
    while (next_capacity < required) next_capacity *= 2;
    next = (SiTxnPath*)si_realloc(*paths,
                                  (size_t)next_capacity * sizeof(SiTxnPath));
    if (!next) return 0;
    *paths = next;
    *capacity = next_capacity;
    return 1;
}

static void si_txn_free(SiInstallTransaction* transaction) {
    if (transaction->files) si_free(transaction->files);
    if (transaction->created_dirs) si_free(transaction->created_dirs);
    if (transaction->clean_items) si_free(transaction->clean_items);
    transaction->files = NULL;
    transaction->created_dirs = NULL;
    transaction->clean_items = NULL;
    transaction->file_count = transaction->file_capacity = 0;
    transaction->created_dir_count = transaction->created_dir_capacity = 0;
    transaction->clean_item_count = transaction->clean_item_capacity = 0;
    transaction->active = 0;
}

static int si_txn_begin(SiInstallTransaction* transaction,
                        const wchar_t* target, int clean_install) {
    DWORD target_attributes;
    int attempt;
    size_t target_length;
    memset(transaction, 0, sizeof(*transaction));
    transaction->fail_before_file = -1;
    if (!si_txn_copy_path(transaction->target, target)) return 0;
    target_length = wcslen(transaction->target);
    while (target_length > 3 &&
           (transaction->target[target_length - 1] == L'\\' ||
            transaction->target[target_length - 1] == L'/')) {
        transaction->target[--target_length] = L'\0';
    }
    target_attributes = GetFileAttributesW(transaction->target);
    transaction->target_existed = target_attributes != INVALID_FILE_ATTRIBUTES;
    if (transaction->target_existed &&
        !(target_attributes & FILE_ATTRIBUTE_DIRECTORY)) return 0;
    if (!transaction->target_existed) {
        int create_result = SHCreateDirectoryExW(NULL, transaction->target, NULL);
        if (create_result != ERROR_SUCCESS && create_result != ERROR_FILE_EXISTS &&
            create_result != ERROR_ALREADY_EXISTS) return 0;
    }
    transaction->clean_install = clean_install;
    for (attempt = 0; attempt < 32; ++attempt) {
        transaction->work[0] = L'\0';
        if (!si_txn_copy_path(transaction->work, transaction->target) ||
            !si_txn_append_text(transaction->work, L".si-rollback-") ||
            !si_txn_append_uint(transaction->work, GetCurrentProcessId()) ||
            !si_txn_append_text(transaction->work, L"-") ||
            !si_txn_append_uint(transaction->work,
                                GetTickCount64() + (uint64_t)attempt)) {
            return 0;
        }
        if (CreateDirectoryW(transaction->work, NULL)) break;
        if (GetLastError() != ERROR_ALREADY_EXISTS) return 0;
    }
    if (attempt == 32) return 0;
    if (!si_txn_copy_path(transaction->clean_backup, transaction->work) ||
        !si_txn_append_text(transaction->clean_backup, L"\\clean")) return 0;
    transaction->active = 1;
    return 1;
}

static int si_txn_add_file(SiInstallTransaction* transaction,
                           const wchar_t* relative, wchar_t* staged_out) {
    SiTxnFile* file;
    wchar_t normalized[260];
    int i;
    size_t length;
    if (!transaction || !transaction->active ||
        !si_txn_valid_relative_path(relative)) return 0;
    length = wcslen(relative);
    if (length >= sizeof(normalized) / sizeof(normalized[0])) return 0;
    for (i = 0; i <= (int)length; ++i)
        normalized[i] = relative[i] == L'/' ? L'\\' : relative[i];
    for (i = 0; i < transaction->file_count; ++i) {
        const wchar_t* existing = transaction->files[i].destination;
        const wchar_t* suffix = existing + wcslen(transaction->target);
        if (*suffix == L'\\') ++suffix;
        if (_wcsicmp(suffix, normalized) == 0) return 0;
    }
    if (!si_txn_reserve_files(transaction, transaction->file_count + 1)) return 0;
    file = &transaction->files[transaction->file_count];
    memset(file, 0, sizeof(*file));
    if (!si_txn_copy_path(file->destination, transaction->target) ||
        !si_txn_append_text(file->destination, L"\\") ||
        !si_txn_append_text(file->destination, normalized) ||
        !si_txn_copy_path(file->staged, transaction->work) ||
        !si_txn_append_text(file->staged, L"\\new-") ||
        !si_txn_append_uint(file->staged, (uint64_t)transaction->file_count) ||
        !si_txn_append_text(file->staged, L".tmp") ||
        !si_txn_copy_path(file->backup, transaction->work) ||
        !si_txn_append_text(file->backup, L"\\old-") ||
        !si_txn_append_uint(file->backup, (uint64_t)transaction->file_count) ||
        !si_txn_append_text(file->backup, L".bak")) return 0;
    if (!si_txn_copy_path(staged_out, file->staged)) return 0;
    transaction->file_count++;
    return 1;
}

static int si_txn_record_created_dir(SiInstallTransaction* transaction,
                                     const wchar_t* path) {
    SiTxnPath* entry;
    if (!si_txn_reserve_paths(&transaction->created_dirs,
                              &transaction->created_dir_capacity,
                              transaction->created_dir_count + 1)) return 0;
    entry = &transaction->created_dirs[transaction->created_dir_count];
    memset(entry, 0, sizeof(*entry));
    if (!si_txn_copy_path(entry->path, path)) return 0;
    transaction->created_dir_count++;
    return 1;
}

static int si_txn_ensure_parent_dirs(SiInstallTransaction* transaction,
                                     const wchar_t* destination) {
    wchar_t path[MAX_PATH];
    wchar_t* cursor;
    size_t root_length = wcslen(transaction->target);
    if (!si_txn_copy_path(path, destination)) return 0;
    for (cursor = path + root_length + 1; *cursor; ++cursor) {
        DWORD error;
        if (*cursor != L'\\') continue;
        *cursor = L'\0';
        if (CreateDirectoryW(path, NULL)) {
            if (!si_txn_record_created_dir(transaction, path)) {
                RemoveDirectoryW(path);
                *cursor = L'\\';
                return 0;
            }
        } else {
            error = GetLastError();
            if (error != ERROR_ALREADY_EXISTS) {
                *cursor = L'\\';
                return 0;
            }
        }
        *cursor = L'\\';
    }
    return 1;
}

static int si_txn_move_existing_with_retry(const wchar_t* source,
                                           const wchar_t* destination,
                                           SiTxnRetryFn retry,
                                           void* retry_context) {
    for (;;) {
        DWORD error;
        if (MoveFileExW(source, destination, MOVEFILE_WRITE_THROUGH)) return 1;
        error = GetLastError();
        if (!retry || !retry(retry_context, source, error)) return 0;
    }
}

static int si_txn_collect_clean_items(SiInstallTransaction* transaction) {
    WIN32_FIND_DATAW data;
    wchar_t search[MAX_PATH];
    HANDLE find;
    if (!si_txn_copy_path(search, transaction->target) ||
        !si_txn_append_text(search, L"\\*")) return 0;
    find = FindFirstFileW(search, &data);
    if (find == INVALID_HANDLE_VALUE)
        return GetLastError() == ERROR_FILE_NOT_FOUND;
    do {
        SiTxnPath* entry;
        if (wcscmp(data.cFileName, L".") == 0 ||
            wcscmp(data.cFileName, L"..") == 0) continue;
        if (!si_txn_reserve_paths(&transaction->clean_items,
                                  &transaction->clean_item_capacity,
                                  transaction->clean_item_count + 1)) {
            FindClose(find);
            return 0;
        }
        entry = &transaction->clean_items[transaction->clean_item_count];
        memset(entry, 0, sizeof(*entry));
        if (!si_txn_copy_path(entry->path, transaction->target) ||
            !si_txn_append_text(entry->path, L"\\") ||
            !si_txn_append_text(entry->path, data.cFileName)) {
            FindClose(find);
            return 0;
        }
        transaction->clean_item_count++;
    } while (FindNextFileW(find, &data));
    FindClose(find);
    return 1;
}

static int si_txn_restore_clean(SiInstallTransaction* transaction) {
    int i, ok = 1;
    if (!transaction->clean_prepared) return 1;
    for (i = transaction->clean_item_count - 1; i >= 0; --i) {
        SiTxnPath* entry = &transaction->clean_items[i];
        const wchar_t* name;
        wchar_t source[MAX_PATH];
        if (!entry->moved) continue;
        name = wcsrchr(entry->path, L'\\');
        if (!name || !si_txn_copy_path(source, transaction->clean_backup) ||
            !si_txn_append_text(source, name) ||
            !si_txn_move_retry(source, entry->path)) {
            ok = 0;
        } else {
            entry->moved = 0;
        }
    }
    return ok;
}

static int si_txn_rollback(SiInstallTransaction* transaction) {
    int i, ok = 1;
    for (i = transaction->file_count - 1; i >= 0; --i) {
        SiTxnFile* file = &transaction->files[i];
        if (file->installed) {
            if (!si_txn_remove_file_retry(file->destination)) {
                ok = 0;
                continue;
            }
            file->installed = 0;
        }
        if (file->backed_up) {
            if (!si_txn_move_retry(file->backup, file->destination)) {
                ok = 0;
            } else {
                file->backed_up = 0;
            }
        }
    }
    for (i = transaction->created_dir_count - 1; i >= 0; --i)
        RemoveDirectoryW(transaction->created_dirs[i].path);
    if (transaction->clean_install && !si_txn_restore_clean(transaction)) ok = 0;
    return ok;
}

static int si_txn_prepare_clean(SiInstallTransaction* transaction,
                                SiTxnRetryFn retry, void* retry_context) {
    int i;
    if (!CreateDirectoryW(transaction->clean_backup, NULL) &&
        GetLastError() != ERROR_ALREADY_EXISTS) return 0;
    transaction->clean_prepared = 1;
    if (!si_txn_collect_clean_items(transaction)) return 0;
    for (i = 0; i < transaction->clean_item_count; ++i) {
        SiTxnPath* entry = &transaction->clean_items[i];
        const wchar_t* name = wcsrchr(entry->path, L'\\');
        wchar_t destination[MAX_PATH];
        if (!name || !si_txn_copy_path(destination, transaction->clean_backup) ||
            !si_txn_append_text(destination, name) ||
            !si_txn_move_existing_with_retry(
                entry->path, destination, retry, retry_context)) return 0;
        entry->moved = 1;
    }
    return 1;
}

static void si_txn_finish(SiInstallTransaction* transaction, int remove_work) {
    if (remove_work) si_txn_delete_tree(transaction->work);
    if (!transaction->target_existed) RemoveDirectoryW(transaction->target);
    si_txn_free(transaction);
}

static void si_txn_abort(SiInstallTransaction* transaction) {
    if (!transaction || !transaction->active) return;
    si_txn_finish(transaction, 1);
}

static int si_txn_commit(SiInstallTransaction* transaction,
                         SiTxnRetryFn retry, void* retry_context) {
    int i, rollback_ok;
    if (!transaction || !transaction->active) return 0;
    if (transaction->clean_install &&
        !si_txn_prepare_clean(transaction, retry, retry_context)) {
        rollback_ok = si_txn_rollback(transaction);
        si_txn_finish(transaction, rollback_ok);
        return 0;
    }
    for (i = 0; i < transaction->file_count; ++i) {
        SiTxnFile* file = &transaction->files[i];
        DWORD attributes;
        if (transaction->fail_before_file == i ||
            !si_txn_ensure_parent_dirs(transaction, file->destination)) {
            rollback_ok = si_txn_rollback(transaction);
            si_txn_finish(transaction, rollback_ok);
            return 0;
        }
        attributes = GetFileAttributesW(file->destination);
        file->had_original = attributes != INVALID_FILE_ATTRIBUTES;
        if (file->had_original) {
            if (attributes & FILE_ATTRIBUTE_DIRECTORY ||
                !si_txn_move_existing_with_retry(
                    file->destination, file->backup, retry, retry_context)) {
                rollback_ok = si_txn_rollback(transaction);
                si_txn_finish(transaction, rollback_ok);
                return 0;
            }
            file->backed_up = 1;
        }
        if (!MoveFileExW(file->staged, file->destination,
                         MOVEFILE_WRITE_THROUGH)) {
            rollback_ok = si_txn_rollback(transaction);
            si_txn_finish(transaction, rollback_ok);
            return 0;
        }
        file->installed = 1;
    }
    /* The new state is complete. A cleanup failure may leave only a sibling
       rollback directory; it never changes the committed target contents. */
    si_txn_delete_tree(transaction->work);
    si_txn_free(transaction);
    return 1;
}

#endif /* SI_TRANSACTION_H */
