/* si_wndproc.h — Wizard state methods (si_layout_buttons, si_update_buttons,
 * si_show_page, si_go_next, si_go_back), WndProc, si_run_wizard, si_run
 * Included from small_installer.h under #ifdef SI_IMPLEMENTATION — no include guard.
 */

static int si_footer_button_width(SiWizardState* st, SiControl* c) {
    int min_w = si_scale(st, SI_BTN_W);
    int pad = si_scale(st, 32);
    float text_w = 0.0f;
    if (st && c && c->text[0] && st->renderer.dwrite_factory) {
        float font_size = sui_ctrl_style_font_size(c, st->renderer.font_size);
        const wchar_t* family = sui_ctrl_style_font_family(c);
        int bold = (c->state & SI_CTRL_BOLD) ? 1 : 0;
        text_w = sui_renderer_measure_text_width_ex(&st->renderer, c->text, bold, font_size, family);
    }
    if (text_w > 0.0f) {
        int needed = (int)(text_w + (float)pad + 0.999f);
        if (needed > min_w) return needed;
    }
    return min_w;
}

static void si_layout_buttons(SiWizardState* st) {
    int bh, m, ww, wh, by, bx;
    SiControl *btn_next, *btn_back;
    if (!st) return;
    bh=si_scale(st,SI_BTN_H);
    m=si_scale(st,SI_BTN_M);
    ww=si_scale(st,SI_BASE_W);
    wh=si_scale(st,SI_BASE_H);
    by = wh - si_scale(st,SI_BTNAREA) + (si_scale(st,SI_BTNAREA)-bh)/2;
    btn_next = si_ctrl_find(&st->ctrl, SI_ID_NEXT);
    btn_back = si_ctrl_find(&st->ctrl, SI_ID_BACK);
    bx = ww - m;
    if (btn_next) {
        int bw = si_footer_button_width(st, btn_next);
        int visible = (btn_next->state & SI_CTRL_HIDDEN) ? 0 : 1;
        int next_x = ww - m - bw;
        if (visible) bx -= bw;
        btn_next->x=(float)next_x; btn_next->y=(float)by; btn_next->w=(float)bw; btn_next->h=(float)bh;
    }
    if (btn_back) {
        int bw = si_footer_button_width(st, btn_back);
        if (btn_next && !(btn_next->state & SI_CTRL_HIDDEN)) bx -= m;
        bx -= bw;
        btn_back->x=(float)bx; btn_back->y=(float)by; btn_back->w=(float)bw; btn_back->h=(float)bh;
    }
}

static int si_should_skip_page(SiWizardState* st, int index) {
    SiPageType pt;
    if (!st || index < 0 || index >= st->page_count) return 1;
    pt = st->pages[index].type;
    if (pt == SI_PAGE_REINSTALL && !st->existing_installed) return 1;
    if (st->existing_installed && (pt == SI_PAGE_LICENSE || pt == SI_PAGE_INSTALL_DIR)) return 1;
    return 0;
}

static int si_resolve_page_index(SiWizardState* st, int index, int dir) {
    if (dir == 0) dir = 1;
    while (index >= 0 && index < st->page_count && si_should_skip_page(st, index))
        index += dir;
    return index;
}

static void si_insert_reinstall_page(SiWizardState* st) {
    int i, insert_at = -1;
    if (!st || !st->existing_installed || st->is_uninstaller) return;
    for (i = 0; i < st->page_count; i++) {
        if (st->pages[i].type == SI_PAGE_REINSTALL) return;
        if (st->pages[i].type == SI_PAGE_WELCOME) insert_at = i + 1;
    }
    if (st->page_count >= SI_MAX_PAGES) return;
    if (insert_at < 0) {
        insert_at = st->page_count;
        for (i = 0; i < st->page_count; i++) {
            if (st->pages[i].type == SI_PAGE_INSTALLING) { insert_at = i; break; }
        }
    }
    for (i = st->page_count; i > insert_at; i--)
        st->pages[i] = st->pages[i - 1];
    st->pages[insert_at] = si_page_reinstall();
    st->page_count++;
}

static void si_update_buttons(SiWizardState* st) {
    SiPageType pt;
    SiControl *btn_back, *btn_next;
    if (st->page_count == 0) return;
    pt = st->pages[st->current_page].type;
    btn_back = si_ctrl_find(&st->ctrl, SI_ID_BACK);
    btn_next = si_ctrl_find(&st->ctrl, SI_ID_NEXT);
    /* Back button: hidden on first page, finished, and during install/uninstall */
    if (btn_back) {
        if (st->current_page == 0 || pt == SI_PAGE_FINISHED
            || pt == SI_PAGE_INSTALLING || pt == SI_PAGE_UNINSTALLING) {
            btn_back->state |= SI_CTRL_HIDDEN;
        } else {
            btn_back->state &= ~SI_CTRL_HIDDEN;
            btn_back->state &= ~SI_CTRL_DISABLED;
        }
        wcscpy(btn_back->text, L(L"< Back"));
    }
    /* Next/Finish/Install button: hidden during install/uninstall (use X to cancel) */
    if (btn_next) {
        if (pt == SI_PAGE_INSTALLING || pt == SI_PAGE_UNINSTALLING) {
            btn_next->state |= SI_CTRL_HIDDEN;
            btn_next->state |= SI_CTRL_DISABLED;
            wcscpy(btn_next->text, st->is_uninstaller ? L(L"Uninstall") : L(L"Install"));
        } else if (pt == SI_PAGE_FINISHED) {
            btn_next->state &= ~SI_CTRL_HIDDEN;
            wcscpy(btn_next->text, L(L"Finish"));
            btn_next->state &= ~SI_CTRL_DISABLED;
        } else if (pt == SI_PAGE_CONFIRM) {
            btn_next->state &= ~SI_CTRL_HIDDEN;
            wcscpy(btn_next->text, L(L"Uninstall"));
            btn_next->state &= ~SI_CTRL_DISABLED;
        } else if (pt == SI_PAGE_REINSTALL) {
            btn_next->state &= ~SI_CTRL_HIDDEN;
            wcscpy(btn_next->text, L(L"Install"));
            btn_next->state &= ~SI_CTRL_DISABLED;
        } else {
            btn_next->state &= ~SI_CTRL_HIDDEN;
            wcscpy(btn_next->text, L(L"Next >"));
            btn_next->state &= ~SI_CTRL_DISABLED;
        }
        /* Block Next on install-dir page when there isn't enough free disk space */
        if (pt == SI_PAGE_INSTALL_DIR && g_idir_space.valid && !g_idir_space.has_enough)
            btn_next->state |= SI_CTRL_DISABLED;
    }
    si_layout_buttons(st);
}

static void si_show_page(SiWizardState* st, int index) {
    int old_page = st->current_page;
    int dir = (index >= old_page) ? 1 : -1;
    index = si_resolve_page_index(st, index, dir);
    if (index < 0 || index >= st->page_count) return;
#ifdef SI_FEATURE_ANIMATIONS
    if (st->transition_type != SI_TRANSITION_NONE
        && old_page >= 0 && old_page < st->page_count
        && st->pages[old_page].type != SI_PAGE_INSTALLING
        && st->pages[old_page].type != SI_PAGE_UNINSTALLING) {
        si_transition_capture(st);
    }
#endif
    if (st->current_page >= 0 && st->current_page < st->page_count) {
        SiPage* old = &st->pages[st->current_page];
        if (old->vtable && old->vtable->on_destroy) old->vtable->on_destroy(old);
    }
    si_ctrl_clear_page(&st->ctrl);
    st->current_page = index;
    { SiPage* cur = &st->pages[index];
      if (cur->vtable && cur->vtable->on_create) cur->vtable->on_create(cur, st->hwnd); }
    si_update_buttons(st);
    /* Flush pending keyboard messages so Enter doesn't carry over between pages */
    { MSG m; while (PeekMessageW(&m, st->hwnd, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE)) {;} }
    sui_ctrl_set_focus(&st->ctrl,
        si_ctrl_next_focusable(&st->ctrl, st->ctrl.persistent_control_count - 1, 1));
#ifdef SI_FEATURE_ANIMATIONS
    if (st->transition_bitmap) {
        si_transition_start(st, index > old_page ? 1 : -1);
    } else
#endif
    InvalidateRect(st->hwnd, NULL, TRUE);
}

static void si_go_next(SiWizardState* st) {
    SiPage* pg;
    if (st->current_page < 0 || st->current_page >= st->page_count) return;
    pg = &st->pages[st->current_page];
    if (pg->vtable && pg->vtable->on_next && !pg->vtable->on_next(pg)) return;
    if (st->current_page == st->page_count - 1) { DestroyWindow(st->hwnd); return; }
    si_show_page(st, st->current_page + 1);
}

static void si_go_back(SiWizardState* st) {
    SiPage* pg;
    if (st->current_page <= 0) return;
    pg = &st->pages[st->current_page];
    if (pg->vtable && pg->vtable->on_back && !pg->vtable->on_back(pg)) return;
    si_show_page(st, st->current_page - 1);
}

typedef struct SiConfirmDialogState {
    HWND hwnd;
    HWND owner;
    UINT dpi;
    const SiColorScheme* scheme;
    wchar_t title[SI_MAX_NAME * 2];
    const wchar_t* message;
    const wchar_t* cancel_label;
    const wchar_t* continue_label;
    int result;
    int ready;
    SiRenderer renderer;
    SiControlHost ctrl;
} SiConfirmDialogState;

static SiConfirmDialogState g_si_confirm_dlg;

static int si_confirm_scale(const SiConfirmDialogState* dlg, int v) {
    UINT dpi = (dlg && dlg->dpi) ? dlg->dpi : 96;
    return MulDiv(v, (int)dpi, 96);
}

static int si_confirm_button_width(SiConfirmDialogState* dlg, SiControl* c) {
    int min_w = si_confirm_scale(dlg, 92);
    int pad = si_confirm_scale(dlg, 32);
    float text_w = 0.0f;
    if (dlg && c && c->text[0] && dlg->renderer.dwrite_factory) {
        float font_size = sui_ctrl_style_font_size(c, dlg->renderer.font_size);
        const wchar_t* family = sui_ctrl_style_font_family(c);
        int bold = (c->state & SI_CTRL_BOLD) ? 1 : 0;
        text_w = sui_renderer_measure_text_width_ex(&dlg->renderer, c->text, bold, font_size, family);
    }
    if (text_w > 0.0f) {
        int needed = (int)(text_w + (float)pad + 0.999f);
        if (needed > min_w) return needed;
    }
    return min_w;
}

#ifdef SI_FEATURE_DARK_MODE
static void si_apply_dark_title_bar(HWND hwnd, const SiColorScheme* scheme) {
    if (scheme == &si_scheme_dark) {
        typedef HRESULT (WINAPI *FnDwm)(HWND, DWORD, const void*, DWORD);
        HMODULE hDwm = LoadLibraryW(L"dwmapi.dll");
        if (hDwm) {
            FnDwm fn = (FnDwm)GetProcAddress(hDwm, "DwmSetWindowAttribute");
            if (fn) { BOOL val = TRUE; fn(hwnd, 20, &val, sizeof(val)); }
            FreeLibrary(hDwm);
        }
    }
}
#else
static void si_apply_dark_title_bar(HWND hwnd, const SiColorScheme* scheme) {
    (void)hwnd; (void)scheme;
}
#endif

static void si_confirm_layout(SiConfirmDialogState* dlg) {
    RECT cr;
    int yes_w, no_w, bh, gap, pad, footer, y, x;
    SiControl *yes_btn, *no_btn;
    if (!dlg || !dlg->hwnd) return;
    GetClientRect(dlg->hwnd, &cr);
    bh = si_confirm_scale(dlg, 30);
    gap = si_confirm_scale(dlg, 8);
    pad = si_confirm_scale(dlg, 16);
    footer = si_confirm_scale(dlg, 48);
    y = cr.bottom - footer + ((footer - bh) / 2);

    x = cr.right - pad;
    no_btn = si_ctrl_find(&dlg->ctrl, IDNO);
    if (no_btn) {
        no_w = si_confirm_button_width(dlg, no_btn);
        x -= no_w;
        no_btn->x = (float)x;
        no_btn->y = (float)y;
        no_btn->w = (float)no_w;
        no_btn->h = (float)bh;
    }
    yes_btn = si_ctrl_find(&dlg->ctrl, IDYES);
    if (yes_btn) {
        yes_w = si_confirm_button_width(dlg, yes_btn);
        if (no_btn) x -= gap;
        x -= yes_w;
        yes_btn->x = (float)x;
        yes_btn->y = (float)y;
        yes_btn->w = (float)yes_w;
        yes_btn->h = (float)bh;
    }
}

static void si_confirm_on_click(int ctrl_id, void* user_data) {
    SiConfirmDialogState* dlg = (SiConfirmDialogState*)user_data;
    if (!dlg) return;
    dlg->result = (ctrl_id == IDYES) ? IDYES : IDNO;
    if (dlg->hwnd) DestroyWindow(dlg->hwnd);
}

static void si_cancel_confirm_paint(SiConfirmDialogState* dlg, HDC dc) {
    PAINTSTRUCT ps;
    RECT cr;
    float w, h, footer, text_x, text_y, text_w, text_h;
    uint32_t footer_sep;
    if (!dlg || !dlg->hwnd) return;
    BeginPaint(dlg->hwnd, &ps);
    (void)dc;
    si_renderer_set_dc(&dlg->renderer, ps.hdc);
    si_renderer_begin(&dlg->renderer);
    si_renderer_clear(&dlg->renderer, dlg->scheme->background);
    GetClientRect(dlg->hwnd, &cr);
    w = (float)cr.right;
    h = (float)cr.bottom;
    footer = (float)si_confirm_scale(dlg, 48);
    footer_sep = si_darken_rgb(dlg->scheme->btn_area_border, 17, 20);

    si_renderer_fill_rect(&dlg->renderer, 0.0f, h - footer, w, footer, dlg->scheme->btn_area);
    si_renderer_draw_line(&dlg->renderer, 0.0f, h - footer, w, h - footer, footer_sep);

    text_x = (float)si_confirm_scale(dlg, 24);
    text_y = (float)si_confirm_scale(dlg, 24);
    text_w = w - (float)si_confirm_scale(dlg, 48);
    text_h = h - footer - text_y - (float)si_confirm_scale(dlg, 10);
    si_accessible_text(&dlg->ctrl, 400, dlg->message, text_x, text_y, text_w, text_h);
    si_renderer_draw_text(&dlg->renderer, dlg->message, text_x, text_y, text_w, text_h,
        dlg->scheme->text, 1, 13.0f);

    si_ctrl_draw_all(&dlg->ctrl);
    si_renderer_end(&dlg->renderer);
    EndPaint(dlg->hwnd, &ps);
}

static LRESULT CALLBACK si_confirm_wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    SiConfirmDialogState* dlg = (SiConfirmDialogState*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_NCCREATE:
        dlg = (SiConfirmDialogState*)((CREATESTRUCTW*)lParam)->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)dlg);
        return TRUE;
    case WM_ERASEBKGND:
        return 1;
    case WM_DPICHANGED:
        if (dlg && dlg->ready) {
            RECT* rc = (RECT*)lParam;
            dlg->dpi = HIWORD(wParam);
            dlg->ctrl.dpi = dlg->dpi;
            SetWindowPos(hwnd, NULL, rc->left, rc->top, rc->right - rc->left, rc->bottom - rc->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
            sui_renderer_apply_typography(&dlg->renderer, &dlg->renderer.typography);
            si_confirm_layout(dlg);
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return 0;
    case WM_SIZE:
        if (dlg && dlg->ready && LOWORD(lParam) > 0 && HIWORD(lParam) > 0) {
            si_renderer_resize(&dlg->renderer, LOWORD(lParam), HIWORD(lParam));
            si_confirm_layout(dlg);
        }
        return 0;
    case WM_MOUSEMOVE:
        if (dlg && dlg->ready) {
            si_ctrl_on_mouse_move(&dlg->ctrl, (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam));
            { TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 }; TrackMouseEvent(&tme); }
        }
        return 0;
    case WM_MOUSELEAVE:
        if (dlg && dlg->ready) sui_ctrl_on_mouse_leave(&dlg->ctrl);
        return 0;
    case WM_TIMER:
        if (dlg && dlg->ready) sui_ctrl_on_timer(&dlg->ctrl, wParam);
        return 0;
#ifdef SI_FEATURE_ANIMATIONS
    case WM_SUI_VSYNC:
        if (dlg && dlg->ready) {
            sui_ctrl_on_vsync(&dlg->ctrl);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
#endif
    case WM_LBUTTONDOWN:
        if (dlg && dlg->ready) si_ctrl_on_lbutton_down(&dlg->ctrl, (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam));
        return 0;
    case WM_LBUTTONUP:
        if (dlg && dlg->ready) si_ctrl_on_lbutton_up(&dlg->ctrl, (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam));
        return 0;
    case WM_KEYDOWN:
        if (dlg && dlg->ready) {
            if (wParam == VK_ESCAPE) {
                si_confirm_on_click(IDNO, dlg);
            } else {
                si_ctrl_on_keydown(&dlg->ctrl, wParam);
            }
        }
        return 0;
    case WM_KEYUP:
        if (dlg && dlg->ready) si_ctrl_on_keyup(&dlg->ctrl, wParam);
        return 0;
    case WM_GETOBJECT:
        if (dlg && dlg->ready) return si_uia_handle_getobject(&dlg->ctrl, wParam, lParam);
        break;
    case WM_PAINT:
        if (dlg && dlg->ready) {
            si_cancel_confirm_paint(dlg, (HDC)wParam);
            return 0;
        }
        break;
    case WM_CLOSE:
        if (dlg) dlg->result = IDNO;
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static int si_confirm_cancel(SiWizardState* st) {
    SiConfirmDialogState* dlg = &g_si_confirm_dlg;
    WNDCLASSEXW wc;
    RECT owner_rc, rc;
    DWORD style, exstyle;
    int client_w, client_h, total_w, total_h, x, y;
    MSG msg = {0};

    if (!st || !st->hwnd) return IDNO;
    memset(dlg, 0, sizeof(*dlg));
    dlg->owner = st->hwnd;
    dlg->dpi = st->dpi ? st->dpi : 96;
    dlg->scheme = st->scheme ? st->scheme : &si_scheme_light;
    GetWindowTextW(st->hwnd, dlg->title, (int)(sizeof(dlg->title) / sizeof(dlg->title[0])));
    if (!dlg->title[0]) {
        lstrcpynW(dlg->title, st->app_name,
            (int)(sizeof(dlg->title) / sizeof(dlg->title[0])));
    }
    if (st->is_uninstaller) {
        dlg->message = L(L"Stop uninstalling?");
        dlg->cancel_label = L(L"Stop uninstalling");
        dlg->continue_label = L(L"Continue uninstalling");
    } else {
        dlg->message = L(L"Cancel installation?");
        dlg->cancel_label = L(L"Cancel installation");
        dlg->continue_label = L(L"Continue installing");
    }
    dlg->result = 0;

    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = si_confirm_wndproc;
    wc.hInstance = st->hInst;
    wc.hCursor = LoadCursorW(NULL, MAKEINTRESOURCEW(32512));
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"SI_ConfirmDialog";
    wc.hIcon = st->icon ? st->icon : LoadIconW(st->hInst, MAKEINTRESOURCEW(1));
    if (!wc.hIcon) wc.hIcon = LoadIconW(NULL, MAKEINTRESOURCEW(32512));
    RegisterClassExW(&wc);

    client_w = si_confirm_scale(dlg, 400);
    client_h = si_confirm_scale(dlg, 122);
    style = WS_POPUP | WS_CAPTION | WS_SYSMENU;
    exstyle = WS_EX_DLGMODALFRAME;
    rc.left = 0; rc.top = 0; rc.right = client_w; rc.bottom = client_h;
    AdjustWindowRectEx(&rc, style, FALSE, exstyle);
    total_w = rc.right - rc.left;
    total_h = rc.bottom - rc.top;
    if (!GetWindowRect(st->hwnd, &owner_rc)) {
        owner_rc.left = 0; owner_rc.top = 0;
        owner_rc.right = GetSystemMetrics(SM_CXSCREEN);
        owner_rc.bottom = GetSystemMetrics(SM_CYSCREEN);
    }
    x = owner_rc.left + ((owner_rc.right - owner_rc.left) - total_w) / 2;
    y = owner_rc.top + ((owner_rc.bottom - owner_rc.top) - total_h) / 2;

    dlg->hwnd = CreateWindowExW(exstyle, L"SI_ConfirmDialog", dlg->title, style,
        x, y, total_w, total_h, st->hwnd, NULL, st->hInst, dlg);
    if (!dlg->hwnd) {
        return MessageBoxW(st->hwnd, dlg->message, st->app_name, MB_YESNO | MB_ICONQUESTION);
    }

    si_apply_dark_title_bar(dlg->hwnd, dlg->scheme);
    si_renderer_init_fields(&dlg->renderer);
    si_renderer_init(&dlg->renderer, dlg->hwnd);
    si_ctrl_host_init(&dlg->ctrl, dlg->hwnd, &dlg->renderer, dlg->scheme, dlg->dpi);
    dlg->ctrl.on_click = si_confirm_on_click;
    dlg->ctrl.on_click_data = dlg;
    si_ctrl_add_button(&dlg->ctrl, IDYES, 0, 0, 0, 0, dlg->cancel_label, 0);
    si_ctrl_add_button(&dlg->ctrl, IDNO, 0, 0, 0, 0, dlg->continue_label, 1);
    dlg->ctrl.persistent_control_count = dlg->ctrl.control_count;
    dlg->ready = 1;
    si_confirm_layout(dlg);

    EnableWindow(st->hwnd, FALSE);
    ShowWindow(dlg->hwnd, SW_SHOW);
    SetActiveWindow(dlg->hwnd);
    UpdateWindow(dlg->hwnd);

    while (dlg->result == 0 && GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (msg.message == WM_QUIT) PostQuitMessage((int)msg.wParam);

    EnableWindow(st->hwnd, TRUE);
    SetActiveWindow(st->hwnd);
    dlg->ready = 0;
    si_ctrl_host_release(&dlg->ctrl);
    si_renderer_release(&dlg->renderer);
    return dlg->result ? dlg->result : IDNO;
}

/** Wizard click callback — routes control clicks to wizard navigation. */
static void si_wizard_on_click(int ctrl_id, void* user_data) {
    SiWizardState* st = (SiWizardState*)user_data;
    if (ctrl_id == SI_ID_BACK) si_go_back(st);
    else if (ctrl_id == SI_ID_NEXT) si_go_next(st);
    else if (ctrl_id == IDCANCEL) {
        if (si_confirm_cancel(st) == IDYES)
            DestroyWindow(st->hwnd);
    } else if (st->current_page >= 0 && st->current_page < st->page_count) {
        SiPage* pg = &st->pages[st->current_page];
        if (pg->vtable && pg->vtable->on_command)
            pg->vtable->on_command(pg, MAKEWPARAM(ctrl_id, BN_CLICKED), 0);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
   WNDPROC + WIZARD RUN
   ═══════════════════════════════════════════════════════════════════════════ */

static LRESULT CALLBACK si_wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        if (g_state) {
            LRESULT result;
            if (sui_default_msg(&g_state->ctrl, hwnd, msg, wParam, lParam, &result)) return result;
        }
        break;
    case DM_GETDEFID:
        return MAKELRESULT(SI_ID_NEXT, DC_HASDEFID);
    case WM_DPICHANGED: {
        RECT* rc;
        if (!g_state) break;
        g_state->dpi = HIWORD(wParam);
        g_state->ctrl.dpi = g_state->dpi;
        rc = (RECT*)lParam;
        SetWindowPos(hwnd, NULL, rc->left, rc->top, rc->right-rc->left, rc->bottom-rc->top, SWP_NOZORDER|SWP_NOACTIVATE);
        /* Rebuild D2D text formats so cached point sizes pick up the new DPI. */
        sui_renderer_apply_typography(&g_state->renderer, &g_state->renderer.typography);
        si_layout_buttons(g_state);
        if (g_state->current_page >= 0 && g_state->current_page < g_state->page_count) {
            SiPage* pg = &g_state->pages[g_state->current_page];
            if (pg->vtable && pg->vtable->on_resize) pg->vtable->on_resize(pg, (int)g_state->dpi);
        }
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }
    case WM_SIZE:
        if (g_state && LOWORD(lParam)>0 && HIWORD(lParam)>0)
            si_renderer_resize(&g_state->renderer, LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_GETMINMAXINFO:
        if (g_state) sui_handle_wm_getminmaxinfo(&g_state->ctrl, hwnd, lParam);
        return 0;
    case WM_MOUSEMOVE:
#ifdef SI_FEATURE_ANIMATIONS
        if (g_state && g_state->transitioning) return 0;
#endif
        if (g_state) {
            si_ctrl_on_mouse_move(&g_state->ctrl, (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam));
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
        }
        return 0;
    case WM_MOUSELEAVE:
        if (g_state) sui_ctrl_on_mouse_leave(&g_state->ctrl);
        return 0;
    case WM_LBUTTONDOWN:
#ifdef SI_FEATURE_ANIMATIONS
        if (g_state && g_state->transitioning) return 0;
#endif
        if (g_state) si_ctrl_on_lbutton_down(&g_state->ctrl, (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam));
        return 0;
    case WM_LBUTTONUP:
#ifdef SI_FEATURE_ANIMATIONS
        if (g_state && g_state->transitioning) return 0;
#endif
        if (g_state) si_ctrl_on_lbutton_up(&g_state->ctrl, (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam));
        return 0;
    case WM_MOUSEWHEEL:
#ifdef SI_FEATURE_ANIMATIONS
        if (g_state && g_state->transitioning) return 0;
#endif
        if (g_state) {
            POINT pt; pt.x = (int)(short)LOWORD(lParam); pt.y = (int)(short)HIWORD(lParam);
            ScreenToClient(hwnd, &pt);
            si_ctrl_on_mouse_wheel(&g_state->ctrl, pt.x, pt.y, (int)(short)HIWORD(wParam));
        }
        return 0;
    case WM_KEYDOWN:
#ifdef SI_FEATURE_ANIMATIONS
        if (g_state && g_state->transitioning) return 0;
#endif
        if (g_state) { si_ctrl_on_keydown(&g_state->ctrl, wParam); return 0; }
        break;
    case WM_KEYUP:
#ifdef SI_FEATURE_ANIMATIONS
        if (g_state && g_state->transitioning) return 0;
#endif
        if (g_state) { si_ctrl_on_keyup(&g_state->ctrl, wParam); return 0; }
        break;
    case WM_CHAR:
        if (g_state) { si_ctrl_on_char(&g_state->ctrl, (wchar_t)wParam); return 0; }
        break;
#ifdef SI_FEATURE_ANIMATIONS
    case WM_SUI_VSYNC: {
        if (!g_state) return 0;
        sui_vsync_dispatch(&g_state->vsync);
        sui_ctrl_on_vsync(&g_state->ctrl);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
#endif
    case WM_PAINT: {
        PAINTSTRUCT ps; RECT cr;
        if (!g_state) break;
        BeginPaint(hwnd, &ps);
        si_renderer_set_dc(&g_state->renderer, ps.hdc);
        si_renderer_begin(&g_state->renderer);
        si_renderer_clear(&g_state->renderer, g_state->scheme->background);
        { int i; for (i = 0; i < g_state->ctrl.control_count; ++i)
            if (g_state->ctrl.controls[i].state & SUI_CTRL_LABEL_NO_DRAW)
                g_state->ctrl.controls[i].state |= SUI_CTRL_HIDDEN;
        }
        GetClientRect(hwnd, &cr);
        { float w=(float)cr.right, h=(float)cr.bottom, bh=(float)si_scale(g_state,SI_BTNAREA);
          RECT pa;
          si_renderer_fill_rect(&g_state->renderer, 0, h-bh, w, bh, g_state->scheme->btn_area);
          si_renderer_draw_line(&g_state->renderer, 0, h-bh, w, h-bh, si_button_area_separator_color(g_state));
          pa.left=0; pa.top=0; pa.right=cr.right; pa.bottom=cr.bottom-(LONG)bh;
          if (g_state->current_page >= 0 && g_state->current_page < g_state->page_count) {
              SiPage* pg = &g_state->pages[g_state->current_page];
              if (pg->vtable && pg->vtable->on_paint) pg->vtable->on_paint(pg, ps.hdc, pa);
          }
        }
        si_ctrl_draw_all(&g_state->ctrl);
#ifdef SI_FEATURE_ANIMATIONS
        if (g_state->transitioning)
            si_transition_paint(g_state);
#endif
        si_renderer_end(&g_state->renderer);
        EndPaint(hwnd, &ps);
        /* Vblank pacing lives in the WM_SUI_VSYNC worker thread (sui_vsync.h).
         * No DwmFlush here — incidental paints (resize/redraw) stay responsive. */
        return 0;
    }
    case WM_COMMAND: {
        int id;
        if (!g_state) break;
        id = LOWORD(wParam);
        if (HIWORD(wParam) == BN_CLICKED) {
            if (id == SI_ID_BACK) si_go_back(g_state);
            else if (id == SI_ID_NEXT) si_go_next(g_state);
            else if (id == IDCANCEL) {
                if (si_confirm_cancel(g_state) == IDYES)
                    DestroyWindow(hwnd);
            } else {
                if (g_state->current_page>=0 && g_state->current_page<g_state->page_count) {
                    SiPage* pg = &g_state->pages[g_state->current_page];
                    if (pg->vtable && pg->vtable->on_command) pg->vtable->on_command(pg, wParam, lParam);
                }
            }
        } else {
            if (g_state->current_page>=0 && g_state->current_page<g_state->page_count) {
                SiPage* pg = &g_state->pages[g_state->current_page];
                if (pg->vtable && pg->vtable->on_command) pg->vtable->on_command(pg, wParam, lParam);
            }
        }
        return 0;
    }
    case WM_TIMER:
        if (g_state && g_state->current_page>=0 && g_state->current_page<g_state->page_count &&
            wParam == SI_UNINSTALL_PROGRESS_TIMER_ID) {
            SiPage* pg = &g_state->pages[g_state->current_page];
            if (pg->type == SI_PAGE_UNINSTALLING && pg->vtable && pg->vtable->on_app_message) {
                pg->vtable->on_app_message(pg, msg, wParam, lParam);
                return 0;
            }
        }
        if (g_state) sui_ctrl_on_timer(&g_state->ctrl, wParam);
        return 0;
    case WM_SI_PROGRESS: case WM_SI_STATUS: case WM_SI_DONE: case WM_SI_FILE_PROGRESS:
        if (g_state && g_state->current_page>=0 && g_state->current_page<g_state->page_count) {
            SiPage* pg = &g_state->pages[g_state->current_page];
            if (pg->vtable && pg->vtable->on_app_message) pg->vtable->on_app_message(pg, msg, wParam, lParam);
        }
        return 0;
    case WM_GETOBJECT:
        if (g_state) return si_uia_handle_getobject(&g_state->ctrl, wParam, lParam);
        break;
    case WM_CLOSE:
        if (g_state && g_state->current_page>=0 && g_state->current_page<g_state->page_count) {
            if (g_state->pages[g_state->current_page].type != SI_PAGE_FINISHED) {
                if (si_confirm_cancel(g_state) != IDYES)
                    return 0;
            }
        }
        DestroyWindow(hwnd); return 0;
    case WM_DESTROY:
#ifdef SI_FEATURE_ANIMATIONS
        /* Join vsync thread before tearing down — thread holds SuiVsyncState* */
        if (g_state) sui_vsync_stop(&g_state->vsync);
#endif
        if (g_state) sui_ctrl_host_release(&g_state->ctrl);
        PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static int si_run_wizard(SiWizardState* st) {
    WNDCLASSEXW wc; HDC sdc; int w, h, total_w, total_h, sx, sy, x, y;
    RECT adj; DWORD style; wchar_t title[512]; MSG msg;
    g_state = st;
    { INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS };
      InitCommonControlsEx(&icc); }
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    si_init_i18n();

    memset(&wc, 0, sizeof(wc)); wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW|CS_VREDRAW; wc.lpfnWndProc = si_wndproc;
    wc.hInstance = st->hInst;
    wc.hCursor = LoadCursorW(NULL, MAKEINTRESOURCEW(32512));
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
#ifdef SI_FEATURE_DARK_MODE
    if (st->scheme == &si_scheme_dark) {
        wc.hbrBackground = CreateSolidBrush(RGB(0x1E, 0x1E, 0x1E));
    }
#endif
    wc.lpszClassName = L"SI_WizardWindow";
    wc.hIcon = st->icon ? st->icon : LoadIconW(st->hInst, MAKEINTRESOURCEW(1));
    if (!wc.hIcon) wc.hIcon = LoadIconW(NULL, MAKEINTRESOURCEW(32512));
    RegisterClassExW(&wc);

    sdc = GetDC(NULL); st->dpi = (UINT)GetDeviceCaps(sdc, LOGPIXELSX); ReleaseDC(NULL, sdc);
    if (st->dpi == 0) st->dpi = 96;

    w = si_scale(st, SI_BASE_W); h = si_scale(st, SI_BASE_H);
    style = WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX;
    adj.left=0; adj.top=0; adj.right=w; adj.bottom=h;
    AdjustWindowRectEx(&adj, style, FALSE, 0);
    total_w = adj.right-adj.left; total_h = adj.bottom-adj.top;
    sx = GetSystemMetrics(SM_CXSCREEN); sy = GetSystemMetrics(SM_CYSCREEN);
    x = (sx-total_w)/2; y = (sy-total_h)/2;

    wcscpy(title, st->app_name);
    if (st->app_version[0]) { wcscat(title, L" "); wcscat(title, st->app_version); }
    wcscat(title, L" "); wcscat(title, st->is_uninstaller ? L(L"- Uninstall") : L(L"- Setup"));

    st->hwnd = CreateWindowExW(0, L"SI_WizardWindow", title, style,
        x, y, total_w, total_h, NULL, NULL, st->hInst, NULL);
    if (!st->hwnd) return 1;

#ifdef SI_FEATURE_ANIMATIONS
    sui_vsync_init(&st->vsync, st->hwnd);
#endif

#ifdef SI_FEATURE_DARK_MODE
    si_apply_dark_title_bar(st->hwnd, st->scheme);
#endif

    st->dpi = si_get_window_dpi(st->hwnd);
    /* Seed default typography (Segoe UI body / Variable Display title / Fluent
     * Icons) before the renderer initializes so cached point sizes are correct. */
    si_renderer_init_fields(&st->renderer);
    si_renderer_init(&st->renderer, st->hwnd);
    ShowWindow(st->hwnd, SW_SHOW);

    si_ctrl_host_init(&st->ctrl, st->hwnd, &st->renderer, st->scheme, st->dpi);
    st->ctrl.on_click = si_wizard_on_click;
    st->ctrl.on_click_data = st;
    { int bh=si_scale(st,SI_BTN_H);
      si_ctrl_add_button(&st->ctrl, SI_ID_NEXT, 0.0f, 0.0f, 0.0f, (float)bh, L(L"Next >"), 1);
      si_ctrl_add_button(&st->ctrl, SI_ID_BACK, 0.0f, 0.0f, 0.0f, (float)bh, L(L"< Back"), 0);
      si_layout_buttons(st);
    }
    st->ctrl.persistent_control_count = st->ctrl.control_count;

    if (st->page_count > 0) si_show_page(st, 0);
    UpdateWindow(st->hwnd);

    /* Bump system timer resolution so SetTimer fires close to its requested
     * interval (default ~15.6ms on Windows). Released at wizard exit. */
    timeBeginPeriod(1);

    while (GetMessageW(&msg, NULL, 0, 0)) {
        /* Keyboard input is handled in WndProc via si_ctrl_on_keydown/keyup */
        TranslateMessage(&msg); DispatchMessageW(&msg);
    }

    timeEndPeriod(1);
    si_renderer_release(&st->renderer);
    g_state = NULL;
    CoUninitialize();
    return 0;
}

#ifdef SI_MODE_UNINSTALL
static int si_run_silent_uninstall(SiWizardState* st) {
    int rc;
    g_state = st;
    st->dpi = 96;
    si_init_i18n();
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    rc = si_perform_uninstall(st);
    CoUninitialize();
    g_state = NULL;
    return rc;
}
#endif

static int si_run_silent_install(SiWizardState* st) {
    int rc;
    g_state = st;
    st->dpi = 96;
    si_init_i18n();
    rc = si_perform_install(st, 1);
    if (rc == 0 && (st->silent_update || st->silent_launch) && st->app_exe[0]) {
        wchar_t exe[MAX_PATH];
        wcscpy(exe, st->install_path);
        if (exe[wcslen(exe) - 1] != L'\\') wcscat(exe, L"\\");
        wcscat(exe, st->app_exe);
        si_launch_detached(exe, st->silent_update ? st->silent_update_launch_args
                                                  : st->silent_launch_args);
    }
    g_state = NULL;
    return rc;
}

/* ═══════════════════════════════════════════════════════════════════════════
   si_run — PUBLIC API
   ═══════════════════════════════════════════════════════════════════════════ */

int si_run(SiConfig* cfg, HINSTANCE hInst) {
    static SiWizardState st; int i;
    si_init_dpi_awareness();
    memset(&st, 0, sizeof(st));
    st.hInst = hInst;
    if (cfg->name) wcscpy(st.app_name, cfg->name);
    if (cfg->version) wcscpy(st.app_version, cfg->version);
    if (cfg->publisher) wcscpy(st.app_publisher, cfg->publisher);
    if (cfg->app_id) wcscpy(st.app_id, cfg->app_id);
    if (cfg->exe) wcscpy(st.app_exe, cfg->exe);
    st.silent_update_launch_args = cfg->silent_update_launch_args;
    st.silent_launch_args = cfg->silent_launch_args;
    st.install_base = cfg->install_base;
    st.privs = cfg->privileges;
    st.accent = cfg->accent_color;
    st.scheme = &si_scheme_light;
#ifdef SI_FEATURE_DARK_MODE
    if (si_is_dark_mode()) st.scheme = &si_scheme_dark;
#endif
    st.is_uninstaller = cfg->is_uninstaller;
    st.cb_pre_install = cfg->on_pre_install;
    st.cb_clean_reinstall = cfg->on_clean_reinstall;
    st.cb_finished = cfg->on_finished;
    st.cb_uninstall = cfg->on_uninstall;
    st.user_data = cfg->user_data;
    st.protocol_scheme = cfg->protocol;
    st.icon = cfg->icon;
    st.silent_update = si_cmdline_has_arg(L"--silent-update") || si_cmdline_has_arg(L"/update");
    st.silent = st.silent_update || si_cmdline_has_arg(L"--silent")
        || si_cmdline_has_arg(L"/silent") || si_cmdline_has_arg(L"/S");
    st.silent_launch = si_cmdline_has_arg(L"--launch");
#ifdef SI_FEATURE_ANIMATIONS
    st.transition_type = cfg->transition;
#endif

    if (!cfg->is_uninstaller)
        si_detect_existing_install(&st);

    /* Copy pages */
    st.page_count = cfg->page_count;
    for (i=0; i<cfg->page_count && i<SI_MAX_PAGES; i++) st.pages[i] = cfg->pages[i];
    si_insert_reinstall_page(&st);

    /* Copy registry */
    st.registry_count = cfg->registry_count;
    for (i=0; i<cfg->registry_count && i<SI_MAX_REG_ENTRIES; i++) st.registry[i] = cfg->registry[i];

    /* Copy shortcuts */
    st.shortcut_count = cfg->shortcut_count;
    for (i=0; i<cfg->shortcut_count && i<SI_MAX_SHORTCUTS; i++) st.shortcuts[i] = cfg->shortcuts[i];

    /* Resolve default install path */
    if (!cfg->is_uninstaller) {
        if (cfg->install_dir_override && cfg->install_dir_override[0]) {
            sui__wstrncpy_z(st.install_path, cfg->install_dir_override, MAX_PATH);
        } else if (!st.install_path[0]) {
            wchar_t base[MAX_PATH];
            const KNOWNFOLDERID* fid = &FOLDERID_LocalAppData;
            if (cfg->install_base == SI_PATH_PROGRAM_FILES) fid = &FOLDERID_ProgramFiles;
            else if (cfg->install_base == SI_PATH_DESKTOP) fid = &FOLDERID_Desktop;
            si_get_known_folder(fid, base, MAX_PATH);
            if (base[0] && cfg->install_sub && cfg->install_sub[0]) {
                if (base[wcslen(base)-1] != L'\\') wcscat(base, L"\\");
                wcscat(base, cfg->install_sub);
            }
            wcscpy(st.install_path, base);
        }
    } else {
        /* Uninstaller: use own exe directory */
        wchar_t exe[MAX_PATH]; wchar_t* sep;
        GetModuleFileNameW(NULL, exe, MAX_PATH);
        sep = wcsrchr(exe, L'\\');
        if (sep) *sep = L'\0';
        wcscpy(st.install_path, exe);
    }

#ifdef SI_MODE_UNINSTALL
    if (st.silent) return si_run_silent_uninstall(&st);
#else
    if (st.silent && !cfg->is_uninstaller) return si_run_silent_install(&st);
#endif
    return si_run_wizard(&st);
}
