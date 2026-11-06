/* si_pages.h — All page implementations (language, license, install_dir, installing,
 * finished, welcome, confirm, uninstalling) + page constructors + header layout helpers
 * Included from small_installer.h under #ifdef SI_IMPLEMENTATION — no include guard.
 */

/* ── Common page header + stack layout helpers ─────────────────────────── */

#define SI_HEADER_PAD_TOP 10  /* pixels before title (unscaled) */
#define SI_HEADER_PAD_MID 12  /* pixels between title and subtitle (unscaled) */
#define SI_HEADER_PAD_BOT 16  /* pixels between subtitle and content (unscaled) */
#define SI_HEADER_LEFT    20  /* left margin (unscaled) */
#define SI_STACK_GAP       8  /* default gap between stacked controls (unscaled) */
#define SI_MEDIA_GAP      20  /* gap between media and text columns (unscaled) */
#define SI_TEXT_GAP       14  /* gap between stacked text blocks (unscaled) */

typedef struct SiRectF {
    float x, y, w, h;
} SiRectF;

/** Simple stack layout cursor — tracks current Y, advances after each item.
 *  Kept as a local installer struct (not SuiLayout) because page code uses
 *  the "left" field name and int-width API pervasively. */
typedef struct SiLayout {
    int y;      /* current Y position (pixels, DPI-scaled) */
    int left;   /* left margin (pixels) */
    int width;  /* available width (pixels) */
    int gap;    /* gap between items (pixels, DPI-scaled) */
} SiLayout;

/** Initialize layout starting below the header (title + subtitle). */
static SiLayout si_layout_begin(const SiWizardState* st) {
    SiLayout lay;
    float dpi = (float)st->dpi;
    float title_h = SI_TITLE_PT * dpi / 72.0f;
    float sub_h = SI_SUBTITLE_PT * dpi / 72.0f;
    float y = (float)si_scale(st, SI_HEADER_PAD_TOP) + title_h
            + (float)si_scale(st, SI_HEADER_PAD_MID) + sub_h
            + (float)si_scale(st, SI_HEADER_PAD_BOT);
    lay.y = (int)(y + 0.5f);
    lay.left = si_scale(st, SI_HEADER_LEFT);
    lay.width = si_scale(st, SI_BASE_W) - lay.left * 2;
    lay.gap = si_scale(st, SI_STACK_GAP);
    return lay;
}

/** Consume vertical space and return the Y that was current before advancing. */
static int si_layout_next(SiLayout* lay, int height) {
    int y = lay->y;
    lay->y += height + lay->gap;
    return y;
}

/** Compute the Y position where page content should start (legacy compat). */
static int si_header_content_y(const SiWizardState* st) {
    SiLayout lay = si_layout_begin(st);
    return lay.y;
}

static RECT si_page_area_for_state(const SiWizardState* st) {
    RECT area;
    area.left = 0;
    area.top = 0;
    area.right = si_scale(st, SI_BASE_W);
    area.bottom = si_scale(st, SI_BASE_H) - si_scale(st, SI_BTNAREA);
    return area;
}

static SiRectF si_page_content_rect(const SiWizardState* st, RECT area) {
    SiRectF rc;
    float inset = (float)si_scale(st, SI_HEADER_LEFT);
    rc.x = (float)area.left + inset;
    rc.y = (float)area.top;
    rc.w = (float)(area.right - area.left) - inset * 2.0f;
    rc.h = (float)(area.bottom - area.top);
    if (rc.w < 0.0f) rc.w = 0.0f;
    if (rc.h < 0.0f) rc.h = 0.0f;
    return rc;
}

static float si_measured_text_height(SiWizardState* st, const wchar_t* text,
                                     float width, int bold, float pt) {
    float h = si_renderer_measure_text_height(&st->renderer, text, width, bold, pt);
    if (h <= 0.0f) h = pt * (float)st->dpi / 72.0f;
    return h;
}

static int si_measured_button_width(SiWizardState* st, const wchar_t* text,
                                    int min_dip, int pad_dip) {
    int min_w = si_scale(st, min_dip);
    int pad = si_scale(st, pad_dip);
    float text_w = 0.0f;
    if (st && text && text[0] && st->renderer.dwrite_factory)
        text_w = sui_renderer_measure_text_width_ex(&st->renderer, text, 0,
            st->renderer.font_size, NULL);
    if (text_w > 0.0f) {
        int needed = (int)(text_w + (float)pad + 0.999f);
        if (needed > min_w) return needed;
    }
    return min_w;
}

static SiRectF si_fit_image_rect(SiRectF bounds, int img_w, int img_h) {
    SiRectF rc = bounds;
    float aspect, w, h;
    if (img_w <= 0 || img_h <= 0 || bounds.w <= 0.0f || bounds.h <= 0.0f) {
        rc.w = 0.0f; rc.h = 0.0f;
        return rc;
    }
    aspect = (float)img_w / (float)img_h;
    w = bounds.w;
    h = w / aspect;
    if (h > bounds.h) {
        h = bounds.h;
        w = h * aspect;
    }
    rc.x = bounds.x + (bounds.w - w) * 0.5f;
    rc.y = bounds.y + (bounds.h - h) * 0.5f;
    rc.w = w;
    rc.h = h;
    return rc;
}

static void si_media_split_layout(const SiWizardState* st, RECT area, int img_w, int img_h,
                                  SiRectF* image_rc, SiRectF* text_rc) {
    SiRectF page;
    float gap, aspect, desired_w, max_w, media_w;
    page.x = (float)area.left;
    page.y = (float)area.top;
    page.w = (float)(area.right - area.left);
    page.h = (float)(area.bottom - area.top);
    gap = (float)si_scale(st, SI_MEDIA_GAP);
    if (img_w <= 0 || img_h <= 0 || page.w <= 0.0f || page.h <= 0.0f) {
        if (image_rc) { image_rc->x = page.x; image_rc->y = page.y; image_rc->w = 0.0f; image_rc->h = 0.0f; }
        if (text_rc) *text_rc = si_page_content_rect(st, area);
        return;
    }
    aspect = (float)img_w / (float)img_h;
    desired_w = page.h * aspect;
    max_w = page.w * 0.48f;
    media_w = desired_w < max_w ? desired_w : max_w;
    if (image_rc) {
        SiRectF media_bounds;
        media_bounds.x = page.x;
        media_bounds.y = page.y;
        media_bounds.w = media_w;
        media_bounds.h = page.h;
        *image_rc = si_fit_image_rect(media_bounds, img_w, img_h);
    }
    if (text_rc) {
        float right_inset = (float)si_scale(st, SI_HEADER_LEFT);
        text_rc->x = page.x + media_w + gap;
        text_rc->y = page.y;
        text_rc->w = (page.x + page.w) - text_rc->x - right_inset;
        text_rc->h = page.h;
        if (text_rc->w < 0.0f) text_rc->w = 0.0f;
    }
}

/* Keep custom-painted prose in the control tree without drawing it twice. */
static void si_accessible_text(SuiControlHost* host, int id, const wchar_t* text,
                               float x, float y, float w, float h) {
    SuiControl* c = sui_ctrl_find(host, id);
    if (!c) {
        int idx = sui_ctrl_add_label(host, id, x, y, w, h, text);
        if (idx < 0) return;
        c = &host->controls[idx];
    }
    sui__wstrncpy_z(c->text, text ? text : L"", SUI_CTRL_TEXT_MAX);
    c->x = x; c->y = y; c->w = w; c->h = h;
    c->state = SUI_CTRL_LABEL_NO_DRAW;
}

static void si_page_draw_text(int id, SiRenderer* r, const wchar_t* text,
                              float x, float y, float w, float h,
                              uint32_t color, int bold, float pt) {
    if (g_state) si_accessible_text(&g_state->ctrl, id, text, x, y, w, h);
    si_renderer_draw_text(r, text, x, y, w, h, color, bold, pt);
}

static void si_page_draw_text_centered(int id, SiRenderer* r, const wchar_t* text,
                                       float x, float y, float w, float h,
                                       uint32_t color, float pt) {
    if (g_state) si_accessible_text(&g_state->ctrl, id, text, x, y, w, h);
    si_renderer_draw_text_centered(r, text, x, y, w, h, color, pt);
}

static void si_draw_title(SiRenderer* r, float s, const SiWizardState* st, float w,
                           const wchar_t* text, uint32_t color) {
    float dpi = (float)st->dpi;
    float title_h = SI_TITLE_PT * dpi / 72.0f;
    si_page_draw_text(400, r, text, (float)si_scale(st, SI_HEADER_LEFT),
        (float)si_scale(st, SI_HEADER_PAD_TOP), w, title_h + 4, color, 1, SI_TITLE_PT);
}

static void si_draw_subtitle(SiRenderer* r, float s, const SiWizardState* st, float w,
                              const wchar_t* text, uint32_t color) {
    float dpi = (float)st->dpi;
    float title_h = SI_TITLE_PT * dpi / 72.0f;
    float sub_y = (float)si_scale(st, SI_HEADER_PAD_TOP) + title_h
                + (float)si_scale(st, SI_HEADER_PAD_MID);
    float sub_h = SI_SUBTITLE_PT * dpi / 72.0f;
    si_page_draw_text(401, r, text, (float)si_scale(st, SI_HEADER_LEFT),
        sub_y, w, sub_h + 4, color, 0, SI_SUBTITLE_PT);
}

/* ── License ────────────────────────────────────────────────────────────── */

static void si_lic_create(SiPage* pg, HWND parent) {
    SiWizardState* st = g_state;
    SiLayout lay = si_layout_begin(st);
    int eh = si_scale(st, SI_BASE_H) - si_scale(st, SI_BTNAREA) - lay.y - si_scale(st,40);
    int ch = si_scale(st,24);
    int lic_y, cb_y;
    (void)parent;
    lic_y = si_layout_next(&lay, eh);
    cb_y = si_layout_next(&lay, ch);
    {
        const wchar_t* lic_text = L(pg->data.license.text);
        int lic_cap = lic_text ? (int)wcslen(lic_text) + 1 : 1;
        si_ctrl_add_textview(&st->ctrl, SI_IDC_LICENSE_EDIT,
            (float)lay.left, (float)lic_y, (float)lay.width, (float)eh,
            lic_text, lic_cap, /*readonly=*/1);
    }
    si_ctrl_add_checkbox(&st->ctrl, SI_IDC_LICENSE_CHECK, (float)lay.left, (float)cb_y,
        (float)lay.width, (float)ch, L(L"I accept the license agreement"),
        pg->data.license.checked_default);
}

static void si_lic_destroy(SiPage* pg) {
    if (g_state) {
        SiControl* c = si_ctrl_find(&g_state->ctrl, SI_IDC_LICENSE_CHECK);
        if (c) pg->data.license.checked_default = (c->state & SI_CTRL_CHECKED) != 0;
        si_ctrl_clear_page(&g_state->ctrl);
    }
}

static void si_lic_paint(SiPage* pg, HDC dc, RECT area) {
    SiWizardState* st = g_state;
    float s = si_dpi_scale(st);
    SiRectF content = si_page_content_rect(st, area);
    (void)pg; (void)dc;
    si_draw_title(&st->renderer, s, st, content.w, L(L"License Agreement"), st->scheme->text);
    si_draw_subtitle(&st->renderer, s, st, content.w, L(L"Read carefully."), st->scheme->text_secondary);
}

static int si_lic_next(SiPage* pg) {
    SiControl* chk;
    (void)pg;
    if (!g_state) return 0;
    chk = si_ctrl_find(&g_state->ctrl, SI_IDC_LICENSE_CHECK);
    if (!chk || !(chk->state & SI_CTRL_CHECKED)) {
        MessageBoxW(g_state->hwnd, L(L"You must accept the license agreement to continue."),
            g_state->app_name, MB_OK|MB_ICONWARNING);
        return 0;
    }
    return 1;
}

static int si_lic_command(SiPage* pg, WPARAM wp, LPARAM lp) {
    (void)pg; (void)wp; (void)lp;
    return 0;
}

static void si_lic_resize(SiPage* pg, int dpi) {
    SiWizardState* st = g_state;
    (void)dpi; if (!st) return;
    { SiLayout lay = si_layout_begin(st);
      int eh = si_scale(st, SI_BASE_H) - si_scale(st, SI_BTNAREA) - lay.y - si_scale(st, 40);
      int ch = si_scale(st, 24);
      int lic_y = si_layout_next(&lay, eh);
      int cb_y  = si_layout_next(&lay, ch);
      SiControl* c;
      (void)pg;
      c = si_ctrl_find(&st->ctrl, SI_IDC_LICENSE_EDIT);
      if (c) { c->x=(float)lay.left; c->y=(float)lic_y; c->w=(float)lay.width; c->h=(float)eh; }
      c = si_ctrl_find(&st->ctrl, SI_IDC_LICENSE_CHECK);
      if (c) { c->x=(float)lay.left; c->y=(float)cb_y; c->w=(float)lay.width; c->h=(float)ch; }
    }
}

static const SiPageVtable si_vtable_license = { si_lic_create, si_lic_destroy, si_lic_paint, si_lic_next, NULL, si_lic_resize, si_lic_command, NULL };

/* ── InstallDir ─────────────────────────────────────────────────────────── */

#include <shobjidl.h>

/* Gap between the edit and Browse button (DIPs). */
#define SI_IDIR_BTN_GAP  8

/* Forward decl — defined in si_wndproc.h, called from si_idir_recompute_space. */
static void si_update_buttons(SiWizardState* st);

/* Disk-space cache: refreshed whenever the install path changes.
 * required_bytes is read once from the embedded payload footer.
 * free_bytes / has_enough are recomputed per path. */
static struct {
    uint64_t required_bytes;
    uint64_t free_bytes;
    int has_enough;
    int valid;
} g_idir_space = { 0, 0, 0, 0 };

/* Format a byte count as "X KB", "X MB" or "X.X GB" using only wsprintfW (no CRT %f). */
static void si_idir_format_size(wchar_t* buf, uint64_t bytes) {
    uint64_t mb = bytes / (1024ull * 1024ull);
    if (mb == 0) {
        wsprintfW(buf, L"%u KB", (UINT)((bytes + 1023ull) / 1024ull));
    } else if (mb < 1024) {
        wsprintfW(buf, L"%u MB", (UINT)mb);
    } else {
        UINT gb = (UINT)(mb / 1024);
        UINT tenths = (UINT)((mb * 10ull / 1024ull) % 10ull);
        wsprintfW(buf, L"%u.%u GB", gb, tenths);
    }
}

/* Pull current path from edit control (if any), recompute free space, sync
 * Next button state, and trigger a repaint. Safe to call repeatedly. */
static void si_idir_recompute_space(void) {
    SiWizardState* st = g_state;
    if (!st) { g_idir_space.valid = 0; return; }
#ifdef SI_HAS_PAYLOAD
    if (g_idir_space.required_bytes == 0)
        si_payload_get_uncompressed_size(&g_idir_space.required_bytes);
#endif
    /* Sync install_path from the live control (may differ after typing/browse). */
    {
        SuiControl* c = sui_ctrl_find(&st->ctrl, SI_IDC_IDIR_EDIT);
        if (c) {
            const wchar_t* path = c->text;
            size_t cap = sizeof(st->install_path) / sizeof(st->install_path[0]);
            size_t n = wcslen(path);
            size_t i;
            if (n >= cap) n = cap - 1;
            for (i = 0; i < n; i++) st->install_path[i] = path[i];
            st->install_path[n] = L'\0';
        }
    }
    if (!si_get_free_space(st->install_path, &g_idir_space.free_bytes))
        g_idir_space.free_bytes = 0;
    g_idir_space.has_enough = (g_idir_space.required_bytes == 0)
        || (g_idir_space.free_bytes >= g_idir_space.required_bytes);
    g_idir_space.valid = 1;
    si_update_buttons(st);
    if (st->hwnd) InvalidateRect(st->hwnd, NULL, FALSE);
}

/* Open the system pick-folder dialog. On selection, write the chosen path
 * into st->install_path AND the edit control. Returns 1 on selection, 0 on
 * cancel/error. */
static int si_idir_browse(SiWizardState* st) {
    IFileOpenDialog* pfd = NULL;
    HRESULT hr_init, hr;
    int     com_inited = 0;
    int     selected = 0;

    hr_init = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (hr_init == S_OK || hr_init == S_FALSE) com_inited = 1;

    hr = CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IFileOpenDialog, (void**)&pfd);
    if (FAILED(hr) || !pfd) {
        if (com_inited) CoUninitialize();
        return 0;
    }
    {
        DWORD opt = 0;
        pfd->lpVtbl->GetOptions(pfd, &opt);
        opt |= FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM;
        pfd->lpVtbl->SetOptions(pfd, opt);
    }
    hr = pfd->lpVtbl->Show(pfd, st->hwnd);
    if (SUCCEEDED(hr)) {
        IShellItem* psi = NULL;
        if (SUCCEEDED(pfd->lpVtbl->GetResult(pfd, &psi)) && psi) {
            wchar_t* path = NULL;
            if (SUCCEEDED(psi->lpVtbl->GetDisplayName(psi, SIGDN_FILESYSPATH, &path)) && path) {
                size_t cap = sizeof(st->install_path) / sizeof(st->install_path[0]);
                size_t n = wcslen(path);
                size_t i;
                SuiControl* c;
                if (n >= cap) n = cap - 1;
                for (i = 0; i < n; i++) st->install_path[i] = path[i];
                st->install_path[n] = L'\0';
                c = sui_ctrl_find(&st->ctrl, SI_IDC_IDIR_EDIT);
                if (c) {
                    size_t en;
                    sui__wstrncpy_z(c->text, path, SUI_CTRL_TEXT_MAX);
                    en = wcslen(c->text);
                    c->d.edit.cursor = (int)en;
                    c->d.edit.sel_start = -1;
                    c->d.edit.sel_end   = -1;
                    c->d.edit.drag_sel_anchor = (int)en;
                    c->d.edit.scroll_x  = 0.0f;
                }
                CoTaskMemFree(path);
                selected = 1;
            }
            psi->lpVtbl->Release(psi);
        }
    }
    pfd->lpVtbl->Release(pfd);
    if (com_inited) CoUninitialize();
    if (st->hwnd) InvalidateRect(st->hwnd, NULL, FALSE);
    return selected;
}

static void si_idir_create(SiPage* pg, HWND parent) {
    SiWizardState* st = g_state;
    SiLayout lay = si_layout_begin(st);
    int eh  = si_scale(st, 28);
    int bw  = si_measured_button_width(st, L(L"Browse..."), SI_BTN_W, 32);
    int gap = si_scale(st, SI_IDIR_BTN_GAP);
    int ew  = lay.width - bw - gap;
    int row_y = si_layout_next(&lay, eh);
    (void)pg; (void)parent;
    if (ew < 0) ew = 0;
    /* Editable path field on the left, separate "Browse..." button on the
     * right. Browse fires the Pick-Folder dialog (see si_idir_browse). */
    sui_ctrl_add_edit(&st->ctrl, SI_IDC_IDIR_EDIT,
        (float)lay.left, (float)row_y, (float)ew, (float)eh,
        st->install_path, NULL, 0);
    sui_ctrl_add_button(&st->ctrl, SI_IDC_IDIR_BROWSE,
        (float)(lay.left + ew + gap), (float)row_y, (float)bw, (float)eh,
        L(L"Browse..."), 0);
    si_idir_recompute_space();
}

static void si_idir_destroy(SiPage* pg) {
    (void)pg;
    if (g_state) {
        si_idir_recompute_space();
        si_ctrl_clear_page(&g_state->ctrl);
    }
}

static void si_idir_paint(SiPage* pg, HDC dc, RECT area) {
    SiWizardState* st = g_state; float s = si_dpi_scale(st);
    SiRectF content = si_page_content_rect(st, area);
    wchar_t desc[512];
    (void)pg; (void)dc;
    si_draw_title(&st->renderer, s, st, content.w, L(L"Choose Install Location"), st->scheme->text);
    { const wchar_t* tmpl = L(L"Select the folder where $APP will be installed.");
      const wchar_t* pos = wcsstr(tmpl, L"$APP");
      if (pos) { wcsncpy(desc, tmpl, (size_t)(pos-tmpl)); desc[pos-tmpl]=0; wcscat(desc, st->app_name); wcscat(desc, pos+4); }
      else wcscpy(desc, tmpl); }
    si_draw_subtitle(&st->renderer, s, st, content.w, desc, st->scheme->text_secondary);
    /* Disk-space line below the path edit row. */
    if (g_idir_space.valid && g_idir_space.required_bytes > 0) {
        SiLayout lay = si_layout_begin(st);
        int eh = si_scale(st, 28);
        int info_h = si_scale(st, 18);
        si_layout_next(&lay, eh);
        wchar_t req[32], avail[32], line[160];
        uint32_t color = g_idir_space.has_enough ? st->scheme->text_secondary : 0xCC3333;
        si_idir_format_size(req, g_idir_space.required_bytes);
        si_idir_format_size(avail, g_idir_space.free_bytes);
        wsprintfW(line, L(L"Required: %s    Available: %s"), req, avail);
        si_page_draw_text(402, &st->renderer, line, (float)lay.left,
            (float)si_layout_next(&lay, info_h), (float)lay.width, (float)info_h, color, 0, 10.0f);
        if (!g_idir_space.has_enough) {
            si_page_draw_text(403, &st->renderer, L(L"Not enough free disk space at this location."),
                (float)lay.left, (float)si_layout_next(&lay, info_h), (float)lay.width, (float)info_h,
                0xCC3333, 0, 10.0f);
        }
    }
}

static int si_idir_next(SiPage* pg) {
    (void)pg;
    if (!g_state) return 0;
    {
        SuiControl* c = sui_ctrl_find(&g_state->ctrl, SI_IDC_IDIR_EDIT);
        const wchar_t* path = c ? c->text : NULL;
        if (path) {
            size_t cap = sizeof(g_state->install_path) / sizeof(g_state->install_path[0]);
            size_t n = wcslen(path);
            size_t i;
            if (n >= cap) n = cap - 1;
            for (i = 0; i < n; i++) g_state->install_path[i] = path[i];
            g_state->install_path[n] = L'\0';
        }
    }
    /* Belt-and-braces: re-check space at click time. UI also disables the button. */
    si_idir_recompute_space();
    if (g_idir_space.valid && !g_idir_space.has_enough) {
        MessageBoxW(g_state->hwnd,
            L(L"Not enough free disk space at this location."),
            g_state->app_name, MB_OK | MB_ICONWARNING);
        return 0;
    }
    return 1;
}

static int si_idir_command(SiPage* pg, WPARAM wp, LPARAM lp) {
    (void)lp; (void)pg;
    if (LOWORD(wp)==SI_IDC_IDIR_EDIT) { si_idir_recompute_space(); return 1; }
    if (LOWORD(wp)==SI_IDC_IDIR_BROWSE) {
        if (g_state) si_idir_browse(g_state);
        si_idir_recompute_space();
        return 1;
    }
    return 0;
}

static void si_idir_resize(SiPage* pg, int dpi) {
    SiWizardState* st = g_state;
    (void)dpi; if (!st) return;
    { SiLayout lay = si_layout_begin(st);
      int eh  = si_scale(st, 28);
      int bw  = si_measured_button_width(st, L(L"Browse..."), SI_BTN_W, 32);
      int gap = si_scale(st, SI_IDIR_BTN_GAP);
      int ew  = lay.width - bw - gap;
      int row_y = si_layout_next(&lay, eh);
      SiControl* c;
      (void)pg;
      if (ew < 0) ew = 0;
      c = si_ctrl_find(&st->ctrl, SI_IDC_IDIR_EDIT);
      if (c) { c->x=(float)lay.left; c->y=(float)row_y; c->w=(float)ew; c->h=(float)eh; }
      c = si_ctrl_find(&st->ctrl, SI_IDC_IDIR_BROWSE);
      if (c) { c->x=(float)(lay.left + ew + gap); c->y=(float)row_y; c->w=(float)bw; c->h=(float)eh; }
    }
}

static const SiPageVtable si_vtable_idir = { si_idir_create, si_idir_destroy, si_idir_paint, si_idir_next, NULL, si_idir_resize, si_idir_command, NULL };

/* ── Reinstall options ───────────────────────────────────────────────────── */

#define SI_IDC_REINSTALL_KEEP   207
#define SI_IDC_REINSTALL_CLEAN  208
#define SI_REINSTALL_GROUP      1

static const wchar_t* si_reinstall_description_text(SiWizardState* st) {
    if (st && st->reinstall_clean_only) {
        return L(L"Keeping settings is disabled for this downgrade. Continue only if you want setup to remove the current installation data and install this version cleanly.");
    }
    return L(L"Keeping settings preserves app data and preferences. A clean reinstall removes the current installation and any app data this installer manages before installing again.");
}

static int si_reinstall_description_height(SiWizardState* st, int width) {
    float h;
    if (!st || width <= 0) return 0;
    h = si_renderer_measure_text_height(&st->renderer, si_reinstall_description_text(st),
        (float)width, 0, SI_SUBTITLE_PT);
    if (h <= 0.0f) return si_scale(st, 56);
    return (int)(h + 2.999f);
}

static int si_reinstall_option_y(SiWizardState* st) {
    SiLayout lay;
    int line_h;
    if (!st) return 0;
    lay = si_layout_begin(st);
    line_h = si_scale(st, 21);
    lay.y += line_h * 2 + si_scale(st, 10);
    lay.y += si_reinstall_description_height(st, lay.width);
    return lay.y + si_scale(st, 16);
}

static void si_reinstall_create(SiPage* pg, HWND parent) {
    SiWizardState* st = g_state;
    SiLayout lay;
    int y, h;
    SiControl* c;
    (void)pg; (void)parent;
    if (!st) return;
    lay = si_layout_begin(st);
    y = si_reinstall_option_y(st);
    h = si_scale(st, 24);
    if (st->reinstall_clean_only) {
        si_ctrl_add_radio(&st->ctrl, SI_IDC_REINSTALL_CLEAN,
            (float)lay.left, (float)y, (float)lay.width, (float)h,
            L(L"Install cleanly"), SI_REINSTALL_GROUP, 1);
        c = si_ctrl_find(&st->ctrl, SI_IDC_REINSTALL_CLEAN);
        if (c) c->state |= SI_CTRL_DISABLED;
    } else {
        si_ctrl_add_radio(&st->ctrl, SI_IDC_REINSTALL_KEEP,
            (float)lay.left, (float)y, (float)lay.width, (float)h,
            L(L"Keep current settings"), SI_REINSTALL_GROUP, 1);
        si_ctrl_add_radio(&st->ctrl, SI_IDC_REINSTALL_CLEAN,
            (float)lay.left, (float)(y + h + lay.gap), (float)lay.width, (float)h,
            L(L"Install cleanly"), SI_REINSTALL_GROUP, 0);
    }
}

static void si_reinstall_destroy(SiPage* pg) {
    (void)pg;
    if (g_state) si_ctrl_clear_page(&g_state->ctrl);
}

static void si_reinstall_paint(SiPage* pg, HDC dc, RECT area) {
    SiWizardState* st = g_state;
    float s, w;
    const wchar_t* installed;
    wchar_t title[512], line[256];
    SiLayout lay;
    SiRectF content;
    int line_h, desc_h;
    (void)pg; (void)dc;
    if (!st) return;

    s = si_dpi_scale(st);
    content = si_page_content_rect(st, area);
    w = content.w;
    lay = si_layout_begin(st);
    line_h = si_scale(st, 21);
    installed = st->existing_version[0] ? st->existing_version : L(L"unknown");

    if (st->reinstall_clean_only) {
        si_draw_title(&st->renderer, s, st, w, L(L"Clean Reinstall Required"), st->scheme->text);
        si_draw_subtitle(&st->renderer, s, st, w,
            L(L"This setup is older than the installed version."), st->scheme->text_secondary);
    } else {
        wcscpy(title, st->app_name);
        wcscat(title, L(L" Is Already Installed"));
        si_draw_title(&st->renderer, s, st, w, title, st->scheme->text);
        si_draw_subtitle(&st->renderer, s, st, w,
            L(L"Choose how setup should reinstall the application."), st->scheme->text_secondary);
    }

    wsprintfW(line, L(L"Installed version: %s"), installed);
    si_page_draw_text(404, &st->renderer, line, (float)lay.left,
        (float)si_layout_next(&lay, line_h), w, (float)line_h,
        st->scheme->text_secondary, 0, SI_SUBTITLE_PT);
    wsprintfW(line, L(L"Setup version: %s"), st->app_version[0] ? st->app_version : L(L"unknown"));
    lay.y -= lay.gap;
    si_page_draw_text(405, &st->renderer, line, (float)lay.left,
        (float)si_layout_next(&lay, line_h), w, (float)line_h,
        st->scheme->text_secondary, 0, SI_SUBTITLE_PT);
    lay.y += si_scale(st, 10) - lay.gap;

    desc_h = si_reinstall_description_height(st, (int)w);
    si_page_draw_text(406, &st->renderer, si_reinstall_description_text(st),
        (float)lay.left, (float)si_layout_next(&lay, desc_h), w, (float)desc_h,
        st->scheme->text_secondary, 0, SI_SUBTITLE_PT);
}

static int si_reinstall_next(SiPage* pg) {
    (void)pg;
    if (!g_state) return 0;
    if (g_state->reinstall_clean_only) {
        g_state->reinstall_clean = 1;
        return 1;
    }
    { SiControl* clean = si_ctrl_find(&g_state->ctrl, SI_IDC_REINSTALL_CLEAN);
      g_state->reinstall_clean = (clean && (clean->state & SI_CTRL_CHECKED)) ? 1 : 0; }
    return 1;
}

static void si_reinstall_resize(SiPage* pg, int dpi) {
    SiWizardState* st = g_state;
    SiLayout lay;
    int y, h;
    SiControl* c;
    (void)pg; (void)dpi;
    if (!st) return;
    lay = si_layout_begin(st);
    y = si_reinstall_option_y(st);
    h = si_scale(st, 24);
    c = si_ctrl_find(&st->ctrl, SI_IDC_REINSTALL_KEEP);
    if (c) { c->x = (float)lay.left; c->y = (float)y; c->w = (float)lay.width; c->h = (float)h; }
    c = si_ctrl_find(&st->ctrl, SI_IDC_REINSTALL_CLEAN);
    if (c) {
        c->x = (float)lay.left;
        c->y = (float)(y + (st->reinstall_clean_only ? 0 : h + lay.gap));
        c->w = (float)lay.width;
        c->h = (float)h;
    }
}

static const SiPageVtable si_vtable_reinstall = { si_reinstall_create, si_reinstall_destroy, si_reinstall_paint, si_reinstall_next, NULL, si_reinstall_resize, NULL, NULL };

/* ── Installing page ────────────────────────────────────────────────────── */

static void si_post_status(const wchar_t* text) {
    size_t len; wchar_t* copy;
    if (!g_state || !g_state->hwnd) return;
    len = wcslen(text)+1; copy = (wchar_t*)si_malloc(len*sizeof(wchar_t));
    if (copy) { memcpy(copy, text, len*sizeof(wchar_t)); PostMessageW(g_state->hwnd, WM_SI_STATUS, 0, (LPARAM)copy); }
}

static void si_post_progress(int pct) {
    if (g_state && g_state->hwnd) PostMessageW(g_state->hwnd, WM_SI_PROGRESS, (WPARAM)pct, 0);
}

static const wchar_t* si_registry_install_value(SiWizardState* st, const wchar_t* value,
                                                wchar_t* buffer, int buffer_len) {
    if (!value) return L"";
    if (wcscmp(value, L"$INSTALLDIR") == 0) {
        int i;
        if (buffer_len <= 0) return value;
        for (i = 0; st->install_path[i] && i < buffer_len - 1; i++) buffer[i] = st->install_path[i];
        buffer[i] = 0;
        return buffer;
    }
    return value;
}

static int si_perform_install(SiWizardState* st, int silent) {
    int i, extract_ok = 1, extract_cancelled = 0;
    DWORD t_start;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    t_start = GetTickCount();

    if (!silent) { si_post_status(L(L"Preparing...")); si_post_progress(0); }
    if (st->app_exe[0])
        si_kill_process_by_name(st->app_exe);
    if (st->reinstall_clean) {
        SiContext ctx;
        if (!silent) si_post_status(L(L"Removing existing installation..."));
        ctx._state = st;
        if (st->cb_clean_reinstall) st->cb_clean_reinstall(&ctx, st->user_data);
    }
    if (st->cb_pre_install) { SiContext ctx; ctx._state = st; st->cb_pre_install(&ctx, st->user_data); }

    if (!silent) si_post_status(L(L"Extracting files..."));
#ifdef SI_HAS_PAYLOAD
    extract_ok = si_payload_extract_stream(
        st->install_path, silent ? NULL : si_post_progress,
        st->hwnd, !silent, st->reinstall_clean, &extract_cancelled);
#endif
    if (!extract_ok) {
        if (!silent && !extract_cancelled) {
            MessageBoxW(st->hwnd,
                L(L"Setup could not finish installing the application."),
                L(L"Installation failed"),
                MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
        }
        CoUninitialize();
        return 1;
    }

    if (!silent) si_post_status(L(L"Writing registry entries..."));
    for (i=0; i<st->registry_count; i++) {
        wchar_t value_buf[MAX_PATH];
        const wchar_t* value = si_registry_install_value(st, st->registry[i].value, value_buf, MAX_PATH);
        si_reg_set_string(st->registry[i].root, st->registry[i].key, st->registry[i].name, value);
    }

    if (!silent) si_post_status(L(L"Creating shortcuts..."));
    for (i=0; i<st->shortcut_count; i++) {
        wchar_t target[MAX_PATH], lnk[MAX_PATH];
        wcscpy(target, st->install_path); wcscat(target, L"\\"); wcscat(target, st->app_exe);
        si_resolve_shortcut_path(st->shortcuts[i].location, st->shortcuts[i].name, lnk, MAX_PATH);
        if (lnk[0]) si_create_shortcut_file(target, lnk, st->app_name, target, st->install_path);
    }

    if (st->protocol_scheme && st->protocol_scheme[0]) {
        wchar_t exe[MAX_PATH];
        if (!silent) si_post_status(L(L"Registering protocol handler..."));
        wcscpy(exe, st->install_path); wcscat(exe, L"\\"); wcscat(exe, st->app_exe);
        si_register_protocol_fn(st->protocol_scheme, exe);
    }

    if (!silent) si_post_status(L(L"Registering application..."));
    si_write_arp(st);
    if (!silent) si_post_progress(100);

    /* Ensure progress bar is visible for at least 1 second */
    if (!silent) {
        DWORD elapsed = GetTickCount() - t_start;
        if (elapsed < 1000) Sleep(1000 - elapsed);
    }

    if (!silent) si_post_status(L(L"Installation complete."));
    CoUninitialize();
    return 0;
}

static DWORD WINAPI si_install_thread(LPVOID param) {
    SiWizardState* st = g_state;
    int result;
    (void)param;
    result = si_perform_install(st, 0);
    PostMessageW(st->hwnd, WM_SI_DONE, (WPARAM)result, 0);
    return 0;
}

static void si_inst_create(SiPage* pg, HWND parent) {
    SiWizardState* st = g_state;
    (void)st; (void)parent;
    pg->data.installing.cc_progress = 0;
    wcscpy(pg->data.installing.cc_status, L(L"Preparing..."));
    pg->data.installing.installing = 1; pg->data.installing.done = 0;
    CreateThread(NULL, 65536, si_install_thread, pg, 0, NULL); /* 64KB committed stack for no-CRT builds */
}

static void si_inst_destroy(SiPage* pg) {
    (void)pg;
    if (g_state) si_ctrl_clear_page(&g_state->ctrl);
}

static void si_inst_paint(SiPage* pg, HDC dc, RECT area) {
    SiWizardState* st = g_state; float s = si_dpi_scale(st);
    SiRectF content = si_page_content_rect(st, area);
    (void)dc;
    si_draw_title(&st->renderer, s, st, content.w, L(L"Installing..."), st->scheme->text);
    si_draw_subtitle(&st->renderer, s, st, content.w, L(L"Please, wait."), st->scheme->text_secondary);
    /* Draw progress bar and status text via D2D */
    { SiLayout lay = si_layout_begin(st);
      float ph=22.0f*s;
      float py=(float)si_layout_next(&lay, (int)ph);
      float px=(float)lay.left, pw=(float)lay.width;
      float pct = (float)pg->data.installing.cc_progress / 100.0f;
      si_renderer_draw_progress(&st->renderer, px, py, pw, ph, pct,
          st->scheme->progress_fill, st->scheme->progress_bg);
      if (pg->data.installing.cc_status[0])
          si_page_draw_text(407, &st->renderer, pg->data.installing.cc_status,
              px, (float)lay.y, pw, 20.0f*s, st->scheme->text_secondary, 0, SI_SUBTITLE_PT);
    }
}

static int si_inst_next(SiPage* pg) { return pg->data.installing.done; }

static void si_inst_appmsg(SiPage* pg, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_SI_PROGRESS) {
        pg->data.installing.cc_progress = (int)wp;
        if (g_state) InvalidateRect(g_state->hwnd, NULL, FALSE);
    } else if (msg == WM_SI_STATUS) {
        wchar_t* text = (wchar_t*)lp;
        if (text) {
            wcsncpy(pg->data.installing.cc_status, text, 255);
            pg->data.installing.cc_status[255] = 0;
            si_free(text);
            if (g_state) InvalidateRect(g_state->hwnd, NULL, FALSE);
        }
    } else if (msg == WM_SI_DONE) {
        pg->data.installing.installing = 0; pg->data.installing.done = 1;
        if (g_state) {
            if (wp != 0) {
                DestroyWindow(g_state->hwnd);
                return;
            }
            SiControl* btn = si_ctrl_find(&g_state->ctrl, SI_ID_NEXT);
            if (btn) btn->state &= ~SI_CTRL_DISABLED;
            si_go_next(g_state);
        }
    }
}

static void si_inst_resize(SiPage* pg, int dpi) {
    SiWizardState* st = g_state;
    (void)pg; (void)st; (void)dpi;
    /* Progress bar and status are drawn in si_inst_paint, no controls to resize */
}

static const SiPageVtable si_vtable_installing = { si_inst_create, si_inst_destroy, si_inst_paint, si_inst_next, NULL, si_inst_resize, NULL, si_inst_appmsg };

/* ── Finished page ──────────────────────────────────────────────────────── */

static void si_fin_layout(SiWizardState* st, SiPageFinishedData* fd,
                          SiRectF* image_rc, SiRectF* text_rc) {
    RECT area = si_page_area_for_state(st);
    if (fd->img_handle && fd->img_width > 0 && fd->img_height > 0) {
        si_media_split_layout(st, area, fd->img_width, fd->img_height, image_rc, text_rc);
    } else {
        if (image_rc) { image_rc->x = 0.0f; image_rc->y = 0.0f; image_rc->w = 0.0f; image_rc->h = 0.0f; }
        if (text_rc) *text_rc = si_page_content_rect(st, area);
    }
}

static float si_fin_title_pt(SiPageFinishedData* fd) {
    return fd->img_handle ? 18.0f : SI_TITLE_PT;
}

static void si_fin_checkbox_metrics(SiWizardState* st, SiPageFinishedData* fd,
                                    float* x, float* y, float* w, float* h, float* gap) {
    SiRectF text_rc;
    float title_pt, title_y, title_h;
    si_fin_layout(st, fd, NULL, &text_rc);
    title_pt = si_fin_title_pt(fd);
    title_y = text_rc.y + (float)si_scale(st, SI_HEADER_PAD_TOP);
    title_h = si_measured_text_height(st, L(L"Installation Complete"), text_rc.w, 1, title_pt);
    if (x) *x = text_rc.x;
    if (y) *y = title_y + title_h + (float)si_scale(st, SI_TEXT_GAP);
    if (w) *w = text_rc.w;
    if (h) *h = (float)si_scale(st, 24);
    if (gap) *gap = (float)si_scale(st, 6);
}

static void si_fin_create(SiPage* pg, HWND parent) {
    SiWizardState* st = g_state;
    SiPageFinishedData* fd = &pg->data.finished;
    float left, top, w, ch, gap;
    int i;
    /* Decode image if provided */
    if (fd->image_data && fd->image_size > 0) {
        SiImageHandle ih = si_renderer_load_image(&st->renderer, fd->image_data, fd->image_size);
        fd->img_handle = ih.data; fd->img_width = ih.width; fd->img_height = ih.height;
    }
    si_fin_checkbox_metrics(st, fd, &left, &top, &w, &ch, &gap);
    (void)parent;
    for (i=0; i<fd->checkbox_count; i++) {
        float y = top + (float)i * (ch + gap);
        { const wchar_t* lbl = L(fd->checkboxes[i].label);
          wchar_t buf[256]; const wchar_t* ap = wcsstr(lbl, L"$APP");
          if (ap) { wcsncpy(buf, lbl, (size_t)(ap-lbl)); buf[ap-lbl]=0; wcscat(buf, st->app_name); wcscat(buf, ap+4); lbl=buf; }
          si_ctrl_add_checkbox(&st->ctrl, SI_IDC_CB_BASE+i, left, y, w, ch,
              lbl, fd->checkboxes[i].default_checked);
        }
    }
    if (fd->anim.attach) {
        float ah = (float)(si_scale(g_state, SI_BASE_H) - si_scale(g_state, SI_BTNAREA));
        float aw = (float)si_scale(g_state, SI_BASE_W);
        fd->anim.attach(fd->anim.user, g_state, aw, ah, si_dpi_scale(g_state));
    }
}

static void si_fin_destroy(SiPage* pg) {
    SiPageFinishedData* fd = &pg->data.finished;
    if (fd->anim.detach) fd->anim.detach(fd->anim.user);
    if (g_state) si_ctrl_clear_page(&g_state->ctrl);
    if (fd->img_handle && g_state) {
        SiImageHandle ih; ih.data=fd->img_handle; ih.width=fd->img_width; ih.height=fd->img_height;
        si_renderer_free_image(&g_state->renderer, ih); fd->img_handle=NULL;
    }
}

static void si_fin_paint(SiPage* pg, HDC dc, RECT area) {
    SiWizardState* st = g_state;
    SiPageFinishedData* fd = &pg->data.finished;
    SiRectF image_rc, text_rc;
    float title_pt, title_y, title_h, paint_h;
    (void)dc;
    (void)area;
    si_fin_layout(st, fd, &image_rc, &text_rc);
    title_pt = si_fin_title_pt(fd);
    title_y = text_rc.y + (float)si_scale(st, SI_HEADER_PAD_TOP);
    title_h = si_measured_text_height(st, L(L"Installation Complete"), text_rc.w, 1, title_pt);
    paint_h = (float)(area.bottom - area.top);

    if (fd->img_handle) {
        SiImageHandle ih;
        ih.data = fd->img_handle; ih.width = fd->img_width; ih.height = fd->img_height;
        si_renderer_draw_image(&st->renderer, ih, image_rc.x, image_rc.y, image_rc.w, image_rc.h);
    }
    si_page_draw_text(408, &st->renderer, L(L"Installation Complete"),
        text_rc.x, title_y, text_rc.w, title_h, st->scheme->text, 1, title_pt);
    if (fd->anim.paint)
        fd->anim.paint(fd->anim.user, st, st->renderer.render_target,
                       paint_h);
}

static int si_fin_next(SiPage* pg) {
    (void)pg;
    if (g_state && g_state->cb_finished) {
        SiContext ctx; ctx._state = g_state;
        g_state->cb_finished(&ctx, g_state->user_data);
    }
    return 1;
}

static void si_fin_resize(SiPage* pg, int dpi) {
    SiWizardState* st = g_state; SiPageFinishedData* fd = &pg->data.finished;
    float left, top, w, ch, gap;
    int i;
    (void)dpi; if (!st) return;
    si_fin_checkbox_metrics(st, fd, &left, &top, &w, &ch, &gap);
    for (i=0; i<fd->checkbox_count; i++) {
        SiControl* c = si_ctrl_find(&st->ctrl, SI_IDC_CB_BASE+i);
        if (c) { c->x=left; c->y=top + (float)i * (ch + gap); c->w=w; c->h=ch; }
    }
}

static const SiPageVtable si_vtable_finished = { si_fin_create, si_fin_destroy, si_fin_paint, si_fin_next, NULL, si_fin_resize, NULL, NULL };

/* ── Welcome page ───────────────────────────────────────────────────────── */

static void si_welcome_create(SiPage* pg, HWND parent) {
    (void)parent;
    if (pg->data.welcome.image_data && pg->data.welcome.image_size > 0 && g_state) {
        SiImageHandle ih = si_renderer_load_image(&g_state->renderer, pg->data.welcome.image_data, pg->data.welcome.image_size);
        pg->data.welcome.img_handle = ih.data; pg->data.welcome.img_width = ih.width; pg->data.welcome.img_height = ih.height;
    }
    if (pg->data.welcome.on_create)
        pg->data.welcome.on_create(pg, parent, pg->data.welcome.user_data);
}

static void si_welcome_destroy(SiPage* pg) {
    if (pg->data.welcome.on_destroy)
        pg->data.welcome.on_destroy(pg, pg->data.welcome.user_data);
    if (pg->data.welcome.img_handle && g_state) {
        SiImageHandle ih; ih.data=pg->data.welcome.img_handle; ih.width=pg->data.welcome.img_width; ih.height=pg->data.welcome.img_height;
        si_renderer_free_image(&g_state->renderer, ih); pg->data.welcome.img_handle=NULL;
    }
}

static void si_welcome_paint(SiPage* pg, HDC dc, RECT area) {
    if (pg->data.welcome.on_paint) {
        pg->data.welcome.on_paint(pg, dc, area, pg->data.welcome.user_data);
        return;
    }
    SiWizardState* st = g_state; float s = si_dpi_scale(st);
    wchar_t title[256], subtitle[512];
    (void)dc;

    /* Build $APP-substituted strings */
    { const wchar_t* tmpl = L(L"Welcome to $APP Setup");
      const wchar_t* ap = wcsstr(tmpl, L"$APP");
      if (ap) { wcsncpy(title, tmpl, (size_t)(ap-tmpl)); title[ap-tmpl]=0; wcscat(title, st->app_name); wcscat(title, ap+4); }
      else wcscpy(title, tmpl); }
    { const wchar_t* tmpl = L(L"This will install $APP on your computer.");
      const wchar_t* ap = wcsstr(tmpl, L"$APP");
      if (ap) { wcsncpy(subtitle, tmpl, (size_t)(ap-tmpl)); subtitle[ap-tmpl]=0; wcscat(subtitle, st->app_name); wcscat(subtitle, ap+4); }
      else wcscpy(subtitle, tmpl); }
    if (pg->data.welcome.img_handle) {
        SiImageHandle ih;
        SiRectF image_rc, text_rc;
        float title_y, title_ah, sub_y, sub_ah;
        ih.data = pg->data.welcome.img_handle; ih.width = pg->data.welcome.img_width; ih.height = pg->data.welcome.img_height;
        si_media_split_layout(st, area, ih.width, ih.height, &image_rc, &text_rc);
        si_renderer_draw_image(&st->renderer, ih, image_rc.x, image_rc.y, image_rc.w, image_rc.h);
        title_y = text_rc.y + (float)si_scale(st, SI_HEADER_PAD_TOP);
        title_ah = si_measured_text_height(st, title, text_rc.w, 1, 18.0f);
        sub_y = title_y + title_ah + (float)si_scale(st, SI_TEXT_GAP);
        sub_ah = si_measured_text_height(st, subtitle, text_rc.w, 0, 11.0f);
        si_page_draw_text(409, &st->renderer, title, text_rc.x, title_y, text_rc.w, title_ah, st->scheme->text, 1, 18.0f);
        si_page_draw_text(410, &st->renderer, subtitle, text_rc.x, sub_y, text_rc.w, sub_ah, st->scheme->text_secondary, 0, 11.0f);
    } else {
        SiRectF text_rc = si_page_content_rect(st, area);
        float title_y = text_rc.y + (float)si_scale(st, SI_HEADER_PAD_TOP);
        float title_ah = si_measured_text_height(st, title, text_rc.w, 1, SI_TITLE_PT);
        float sub_y = title_y + title_ah + (float)si_scale(st, SI_TEXT_GAP);
        float sub_ah = si_measured_text_height(st, subtitle, text_rc.w, 0, SI_SUBTITLE_PT);
        si_page_draw_text(411, &st->renderer, title, text_rc.x, title_y, text_rc.w, title_ah, st->scheme->text, 1, SI_TITLE_PT);
        si_page_draw_text(412, &st->renderer, subtitle, text_rc.x, sub_y, text_rc.w, sub_ah, st->scheme->text_secondary, 0, SI_SUBTITLE_PT);
    }
}

static int si_welcome_next(SiPage* pg) { (void)pg; return 1; }

static const SiPageVtable si_vtable_welcome = { si_welcome_create, si_welcome_destroy, si_welcome_paint, si_welcome_next, NULL, NULL, NULL, NULL };

/* ── Confirm page (uninstaller) ─────────────────────────────────────────── */

static void si_confirm_create(SiPage* pg, HWND parent) {
    (void)parent;
    if (pg->data.confirm.image_data && pg->data.confirm.image_size > 0 && g_state) {
        SiImageHandle ih = si_renderer_load_image(&g_state->renderer, pg->data.confirm.image_data, pg->data.confirm.image_size);
        pg->data.confirm.img_handle = ih.data; pg->data.confirm.img_width = ih.width; pg->data.confirm.img_height = ih.height;
    }
}

static void si_confirm_destroy(SiPage* pg) {
    if (pg->data.confirm.img_handle && g_state) {
        SiImageHandle ih; ih.data=pg->data.confirm.img_handle; ih.width=pg->data.confirm.img_width; ih.height=pg->data.confirm.img_height;
        si_renderer_free_image(&g_state->renderer, ih);
        pg->data.confirm.img_handle = NULL;
    }
}

static void si_confirm_paint(SiPage* pg, HDC dc, RECT area) {
    SiWizardState* st = g_state;
    SiRectF content = si_page_content_rect(st, area);
    const wchar_t* key = pg->data.confirm.text;
    wchar_t msg[512];
    float text_h = 0.0f;
    (void)dc;
    if (!key && !pg->data.confirm.img_handle) key = L"Are you sure you want to uninstall $APP?";
    if (key) {
        const wchar_t* tmpl = L(key);
        const wchar_t* ap = wcsstr(tmpl, L"$APP");
        if (ap) { wcsncpy(msg, tmpl, (size_t)(ap-tmpl)); msg[ap-tmpl]=0; wcscat(msg, st->app_name); wcscat(msg, ap+4); }
        else wcscpy(msg, tmpl);
        text_h = si_measured_text_height(st, msg, content.w, 0, 13.0f);
    }
    if (pg->data.confirm.img_handle) {
        SiImageHandle ih;
        SiRectF image_rc, bounds = content;
        float gap = key ? (float)si_scale(st, SI_TEXT_GAP) : 0.0f;
        ih.data=pg->data.confirm.img_handle; ih.width=pg->data.confirm.img_width; ih.height=pg->data.confirm.img_height;
        bounds.h -= text_h + gap;
        image_rc = si_fit_image_rect(bounds, ih.width, ih.height);
        si_renderer_draw_image(&st->renderer, ih, image_rc.x, image_rc.y, image_rc.w, image_rc.h);
        if (key)
            si_page_draw_text_centered(413, &st->renderer, msg, content.x, image_rc.y + image_rc.h + gap, content.w, text_h,
                st->scheme->text_secondary, 13.0f);
    } else {
        si_page_draw_text(414, &st->renderer, msg, content.x,
            content.y + (content.h - text_h) * 0.5f, content.w, text_h,
            st->scheme->text_secondary, 0, 13.0f);
    }
}

static int si_confirm_next(SiPage* pg) { (void)pg; return 1; }

static int si_confirm_back(SiPage* pg) {
    (void)pg;
    if (g_state && g_state->hwnd) DestroyWindow(g_state->hwnd);
    return 0;
}

static const SiPageVtable si_vtable_confirm = { si_confirm_create, si_confirm_destroy, si_confirm_paint, si_confirm_next, si_confirm_back, NULL, NULL, NULL };

/* ── Uninstalling page ──────────────────────────────────────────────────── */

static int si_uninstall_join_path(wchar_t* output, const wchar_t* directory,
                                  const wchar_t* name) {
    size_t directory_len = wcslen(directory), name_len = wcslen(name);
    if (directory_len + 1 + name_len >= MAX_PATH) return 0;
    wcscpy(output, directory);
    if (directory_len > 0 && output[directory_len - 1] != L'\\') wcscat(output, L"\\");
    wcscat(output, name);
    return 1;
}

static unsigned int si_uninstall_count_files(const wchar_t* directory,
                                             const wchar_t* self_path) {
    wchar_t search[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE find;
    unsigned int total = 0;
    if (!si_uninstall_join_path(search, directory, L"*")) return 0;
    find = FindFirstFileW(search, &fd);
    if (find == INVALID_HANDLE_VALUE) return 0;
    do {
        wchar_t full[MAX_PATH];
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0 ||
            !si_uninstall_join_path(full, directory, fd.cFileName) ||
            _wcsicmp(full, self_path) == 0) continue;
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
            (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0) {
            unsigned int nested = si_uninstall_count_files(full, self_path);
            total = nested > 0xFFFFFFFFu - total ? 0xFFFFFFFFu : total + nested;
        } else if (total < 0xFFFFFFFFu) {
            total++;
        }
    } while (FindNextFileW(find, &fd));
    FindClose(find);
    return total;
}

static void si_post_file_progress(unsigned int completed, unsigned int total) {
    if (g_state && g_state->hwnd)
        PostMessageW(g_state->hwnd, WM_SI_FILE_PROGRESS, (WPARAM)completed, (LPARAM)total);
}

typedef void (*SiUninstallFileProgressFn)(unsigned int completed, unsigned int total);

static void si_uninstall_delete_files(const wchar_t* directory,
                                      const wchar_t* self_path,
                                      unsigned int total,
                                      unsigned int* completed,
                                      SiUninstallFileProgressFn progress) {
    wchar_t search[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE find;
    if (!si_uninstall_join_path(search, directory, L"*")) return;
    find = FindFirstFileW(search, &fd);
    if (find == INVALID_HANDLE_VALUE) return;
    do {
        wchar_t full[MAX_PATH];
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0 ||
            !si_uninstall_join_path(full, directory, fd.cFileName) ||
            _wcsicmp(full, self_path) == 0) continue;
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
            (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0) {
            si_uninstall_delete_files(full, self_path, total, completed, progress);
            SetFileAttributesW(full, FILE_ATTRIBUTE_NORMAL);
            RemoveDirectoryW(full);
        } else {
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
                /* Operate on the link itself without touching attributes that could resolve to its target. */
                if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
                    RemoveDirectoryW(full);
                else
                    DeleteFileW(full);
            } else {
                SetFileAttributesW(full, FILE_ATTRIBUTE_NORMAL);
                DeleteFileW(full);
            }
            if (*completed < total) (*completed)++;
            if (progress) progress(*completed, total);
        }
    } while (FindNextFileW(find, &fd));
    FindClose(find);
}

/* The uninstall itself, with or without a window. 0, or 1 when a file stayed: the cleanup after exit tries it again. */
static int si_perform_uninstall(SiWizardState* st) {
    int i, left = 0;

    si_post_status(L(L"Preparing to uninstall...")); si_post_progress(8);
    if (st->cb_uninstall) { SiContext ctx; ctx._state = st; st->cb_uninstall(&ctx, st->user_data); }
    si_post_progress(20);

    si_post_status(L(L"Removing shortcuts..."));
    si_post_progress(30);
    for (i=0; i<st->shortcut_count; i++) {
        wchar_t lnk[MAX_PATH];
        si_resolve_shortcut_path(st->shortcuts[i].location, st->shortcuts[i].name, lnk, MAX_PATH);
        if (lnk[0]) DeleteFileW(lnk);
    }
    si_post_progress(38);

    if (st->protocol_scheme && st->protocol_scheme[0]) {
        si_post_status(L(L"Removing protocol handler..."));
        si_unregister_protocol_fn(st->protocol_scheme);
    }
    si_post_progress(48);

    si_post_status(L(L"Cleaning registry..."));
    for (i=0; i<st->registry_count; i++)
        si_reg_delete_value(st->registry[i].root, st->registry[i].key, st->registry[i].name);
    si_post_progress(58);

    si_post_status(L(L"Removing from Programs list..."));
    si_remove_arp(st);
    si_post_progress(66);

    si_post_status(L(L"Removing files..."));
    si_post_progress(66);
    if (st->install_path[0]) {
        wchar_t self[MAX_PATH];
        unsigned int total_files, completed_files = 0;
        GetModuleFileNameW(NULL, self, MAX_PATH);
        total_files = si_uninstall_count_files(st->install_path, self);
        if (total_files > 0)
            si_uninstall_delete_files(st->install_path, self, total_files, &completed_files,
                                      si_post_file_progress);
        else
            si_post_file_progress(1, 1);
        left = si_uninstall_count_files(st->install_path, self) > 0;
        RemoveDirectoryW(st->install_path);
    }
    /* Roaming app data folder is deleted later if user checks the option */
    si_post_progress(98);

    si_post_status(L(L"Finishing..."));
    { wchar_t self[MAX_PATH], tmp[MAX_PATH], bat[MAX_PATH]; HANDLE hBat;
      char sn[MAX_PATH], dn[MAX_PATH]; char batbuf[1024]; int blen;
      STARTUPINFOW si2; PROCESS_INFORMATION pi2; wchar_t cmd[MAX_PATH+32];
      GetModuleFileNameW(NULL, self, MAX_PATH);
      GetTempPathW(MAX_PATH, tmp);
      wcscpy(bat, tmp); wcscat(bat, L"si_cleanup.bat");
      hBat = CreateFileW(bat, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
      if (hBat != INVALID_HANDLE_VALUE) {
          WideCharToMultiByte(CP_ACP, 0, self, -1, sn, MAX_PATH, NULL, NULL);
          WideCharToMultiByte(CP_ACP, 0, st->install_path, -1, dn, MAX_PATH, NULL, NULL);
          blen = 0;
          /* Retry loop: wait until the uninstaller exe is unlocked, then delete */
          { const char* lines[] = {
              "@echo off\r\n",
              ":w\r\n",
              "ping -n 2 127.0.0.1 >nul\r\n",
              "del /f /q \"", NULL};
            int li; for (li=0; lines[li]; li++) { const char* s=lines[li]; while (*s) batbuf[blen++]=*s++; } }
          { const char* s=sn; while (*s) batbuf[blen++]=*s++; }
          { const char* s="\" 2>nul\r\n"; while (*s) batbuf[blen++]=*s++; }
          { const char* s="if exist \""; while (*s) batbuf[blen++]=*s++; }
          { const char* s=sn; while (*s) batbuf[blen++]=*s++; }
          { const char* s="\" goto w\r\n"; while (*s) batbuf[blen++]=*s++; }
          { const char* s="rmdir /s /q \""; while (*s) batbuf[blen++]=*s++; }
          { const char* s=dn; while (*s) batbuf[blen++]=*s++; }
          batbuf[blen++]='"'; batbuf[blen++]='\r'; batbuf[blen++]='\n';
          { const char* s="del /f /q \"%~f0\"\r\n"; while (*s) batbuf[blen++]=*s++; }
          { DWORD wr; WriteFile(hBat, batbuf, (DWORD)blen, &wr, NULL); }
          CloseHandle(hBat);
          memset(&si2, 0, sizeof(si2)); si2.cb=sizeof(si2); si2.dwFlags=STARTF_USESHOWWINDOW; si2.wShowWindow=SW_HIDE;
          memset(&pi2, 0, sizeof(pi2));
          wcscpy(cmd, L"cmd.exe /c \""); wcscat(cmd, bat); wcscat(cmd, L"\"");
          CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si2, &pi2);
          if (pi2.hProcess) CloseHandle(pi2.hProcess);
          if (pi2.hThread) CloseHandle(pi2.hThread);
      }
    }
    si_post_progress(100);
    return left;
}

static DWORD WINAPI si_uninstall_thread(LPVOID param) {
    SiWizardState* st = g_state;
    DWORD t_start;
    (void)param;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    t_start = GetTickCount();
    si_perform_uninstall(st);
    /* Ensure progress bar is visible for at least 1 second */
    { DWORD elapsed = GetTickCount() - t_start;
      if (elapsed < 1000) Sleep(1000 - elapsed); }
    CoUninitialize();
    PostMessageW(st->hwnd, WM_SI_DONE, 0, 0);
    return 0;
}

static void si_uninst_create(SiPage* pg, HWND parent) {
    SiWizardState* st = g_state;
    (void)st; (void)parent;
    pg->data.uninstalling.cc_progress = 0.0f;
    pg->data.uninstalling.cc_progress_target = 0.0f;
    pg->data.uninstalling.cc_completion_pending = 0;
    pg->data.uninstalling.cc_progress_tick_ms = si_now_ms();
    wcscpy(pg->data.uninstalling.cc_status, L(L"Preparing..."));
    pg->data.uninstalling.uninstalling = 1; pg->data.uninstalling.done = 0;
    if (g_state && g_state->hwnd)
        SetTimer(g_state->hwnd, SI_UNINSTALL_PROGRESS_TIMER_ID, si_display_refresh_ms(), NULL);
    CreateThread(NULL, 65536, si_uninstall_thread, pg, 0, NULL); /* 64KB committed stack for no-CRT builds */
}

static void si_uninst_destroy(SiPage* pg) {
    if (g_state && g_state->hwnd) KillTimer(g_state->hwnd, SI_UNINSTALL_PROGRESS_TIMER_ID);
    if (g_state) si_ctrl_clear_page(&g_state->ctrl);
    if (pg->data.uninstalling.img_handle && g_state) {
        SiImageHandle ih; ih.data=pg->data.uninstalling.img_handle; ih.width=pg->data.uninstalling.img_width; ih.height=pg->data.uninstalling.img_height;
        si_renderer_free_image(&g_state->renderer, ih); pg->data.uninstalling.img_handle=NULL;
    }
}

static void si_uninst_done_layout(SiWizardState* st, SiPageUninstallingData* ud,
                                  SiRectF* image_rc, SiRectF* text_rc) {
    RECT area = si_page_area_for_state(st);
    if (ud->img_handle && ud->img_width > 0 && ud->img_height > 0) {
        si_media_split_layout(st, area, ud->img_width, ud->img_height, image_rc, text_rc);
    } else {
        if (image_rc) { image_rc->x = 0.0f; image_rc->y = 0.0f; image_rc->w = 0.0f; image_rc->h = 0.0f; }
        if (text_rc) *text_rc = si_page_content_rect(st, area);
    }
}

static float si_uninst_done_title_pt(SiPageUninstallingData* ud) {
    return ud->img_handle ? 18.0f : SI_TITLE_PT;
}

static void si_uninst_delete_metrics(SiWizardState* st, SiPageUninstallingData* ud,
                                     float* x, float* y, float* w, float* h) {
    SiRectF text_rc;
    float title_pt, title_y, title_h;
    si_uninst_done_layout(st, ud, NULL, &text_rc);
    title_pt = si_uninst_done_title_pt(ud);
    title_y = text_rc.y + (float)si_scale(st, SI_HEADER_PAD_TOP);
    title_h = si_measured_text_height(st, L(L"Uninstall Complete"), text_rc.w, 1, title_pt);
    if (x) *x = text_rc.x;
    if (y) *y = title_y + title_h + (float)si_scale(st, SI_TEXT_GAP);
    if (w) *w = text_rc.w;
    if (h) *h = (float)si_scale(st, 24);
}

static void si_uninst_paint(SiPage* pg, HDC dc, RECT area) {
    SiWizardState* st = g_state; float s = si_dpi_scale(st);
    SiRectF content = si_page_content_rect(st, area);
    (void)dc;
    if (pg->data.uninstalling.done && pg->data.uninstalling.img_handle) {
        SiImageHandle ih;
        SiRectF image_rc, text_rc;
        float title_pt, title_y, title_h;
        ih.data = pg->data.uninstalling.img_handle; ih.width = pg->data.uninstalling.img_width; ih.height = pg->data.uninstalling.img_height;
        si_uninst_done_layout(st, &pg->data.uninstalling, &image_rc, &text_rc);
        title_pt = si_uninst_done_title_pt(&pg->data.uninstalling);
        title_y = text_rc.y + (float)si_scale(st, SI_HEADER_PAD_TOP);
        title_h = si_measured_text_height(st, L(L"Uninstall Complete"), text_rc.w, 1, title_pt);
        si_renderer_draw_image(&st->renderer, ih, image_rc.x, image_rc.y, image_rc.w, image_rc.h);
        si_page_draw_text(415, &st->renderer, L(L"Uninstall Complete"),
            text_rc.x, title_y, text_rc.w, title_h, st->scheme->text, 1, title_pt);
    } else if (pg->data.uninstalling.done) {
        SiRectF text_rc;
        float title_y, title_h;
        si_uninst_done_layout(st, &pg->data.uninstalling, NULL, &text_rc);
        title_y = text_rc.y + (float)si_scale(st, SI_HEADER_PAD_TOP);
        title_h = si_measured_text_height(st, L(L"Uninstall Complete"), text_rc.w, 1, SI_TITLE_PT);
        si_page_draw_text(416, &st->renderer, L(L"Uninstall Complete"),
            text_rc.x, title_y, text_rc.w, title_h, st->scheme->text, 1, SI_TITLE_PT);
    } else {
        si_draw_title(&st->renderer, s, st, content.w, L(L"Uninstalling..."), st->scheme->text);
        si_draw_subtitle(&st->renderer, s, st, content.w, L(L"Please wait while the application is being removed."), st->scheme->text_secondary);
        /* Draw progress bar and status text via D2D */
        { SiLayout lay = si_layout_begin(st);
          float ph=22.0f*s;
          float py=(float)si_layout_next(&lay, (int)ph);
          float px=(float)lay.left, pw=(float)lay.width;
          float pct = pg->data.uninstalling.cc_progress / 100.0f;
          si_renderer_draw_progress(&st->renderer, px, py, pw, ph, pct,
              st->scheme->progress_fill, st->scheme->progress_bg);
          if (pg->data.uninstalling.cc_status[0])
              si_page_draw_text(417, &st->renderer, pg->data.uninstalling.cc_status,
                  px, (float)lay.y, pw, 20.0f*s, st->scheme->text_secondary, 0, SI_SUBTITLE_PT);
        }
    }
}

static int si_uninst_next(SiPage* pg) {
    if (pg->data.uninstalling.done) {
        /* Delete Roaming app data if user opted in */
        { int delete_data = 0;
          { SiControl* c = g_state ? si_ctrl_find(&g_state->ctrl, SI_IDC_UNINST_DELETE) : NULL;
            if (c && (c->state & SI_CTRL_CHECKED)) delete_data = 1;
          }
          if (delete_data && g_state) {
              wchar_t roaming[MAX_PATH];
              si_get_known_folder(&FOLDERID_RoamingAppData, roaming, MAX_PATH);
              if (roaming[0] && g_state->app_name[0]) {
                  wchar_t dp[MAX_PATH+2]; SHFILEOPSTRUCTW op;
                  wcscat(roaming, L"\\"); wcscat(roaming, g_state->app_name);
                  if (GetFileAttributesW(roaming) != INVALID_FILE_ATTRIBUTES) {
                      wcscpy(dp, roaming); dp[wcslen(dp)+1]=0;
                      memset(&op, 0, sizeof(op)); op.wFunc=FO_DELETE; op.pFrom=dp;
                      op.fFlags=FOF_NOCONFIRMATION|FOF_NOERRORUI|FOF_SILENT;
                      SHFileOperationW(&op);
                  }
              }
          }
        }
        if (g_state && g_state->hwnd) DestroyWindow(g_state->hwnd);
    }
    return 0; /* Never advance to next page — window closes on Finish */
}

static void si_uninst_finish_page(SiPage* pg) {
    pg->data.uninstalling.cc_progress = 100.0f;
    pg->data.uninstalling.cc_progress_target = 100.0f;
    pg->data.uninstalling.cc_completion_pending = 0;
    pg->data.uninstalling.uninstalling = 0;
    pg->data.uninstalling.done = 1;
    if (g_state) {
        KillTimer(g_state->hwnd, SI_UNINSTALL_PROGRESS_TIMER_ID);
        /* Decode completion image if provided */
        if (pg->data.uninstalling.image_data && pg->data.uninstalling.image_size > 0 && !pg->data.uninstalling.img_handle) {
            SiImageHandle ih = si_renderer_load_image(&g_state->renderer, pg->data.uninstalling.image_data, pg->data.uninstalling.image_size);
            pg->data.uninstalling.img_handle = ih.data; pg->data.uninstalling.img_width = ih.width; pg->data.uninstalling.img_height = ih.height;
        }
        /* Clear page controls (progress display) and add delete-data checkbox */
        si_ctrl_clear_page(&g_state->ctrl);
        { float cb_left, cb_top, cb_w, cb_h;
          si_uninst_delete_metrics(g_state, &pg->data.uninstalling, &cb_left, &cb_top, &cb_w, &cb_h);
          si_ctrl_add_checkbox(&g_state->ctrl, SI_IDC_UNINST_DELETE, cb_left, cb_top,
              cb_w, cb_h, L(L"Delete all app data and settings"), 1);
        }
        /* Update button controls for finish state */
        { SiControl *btn_next, *btn_back;
          btn_next = si_ctrl_find(&g_state->ctrl, SI_ID_NEXT);
          btn_back = si_ctrl_find(&g_state->ctrl, SI_ID_BACK);
          if (btn_next) {
              wcscpy(btn_next->text, L(L"Finish"));
              btn_next->state &= ~(SI_CTRL_DISABLED|SI_CTRL_HIDDEN);
          }
          if (btn_back) btn_back->state |= SI_CTRL_HIDDEN;
          si_layout_buttons(g_state);
        }
        /* Repaint to show completion screen with image */
        InvalidateRect(g_state->hwnd, NULL, TRUE);
    }
}

static void si_uninst_progress_tick(SiPage* pg) {
    SiPageUninstallingData* ud = &pg->data.uninstalling;
    uint64_t now = si_now_ms();
    uint64_t elapsed = now > ud->cc_progress_tick_ms ? now - ud->cc_progress_tick_ms : 0;
    float target = ud->cc_progress_target;
    int changed = 0;
    ud->cc_progress_tick_ms = now;
    if (elapsed > 64) elapsed = 64; /* Never skip visibly across the bar after a delayed UI tick. */
    if (ud->cc_progress < target && elapsed > 0) {
        /* Phase updates are intentionally coarse; interpolate them at display cadence. Once the
           worker is done, finish the short remaining ramp promptly but still visibly. */
        float rate = ud->cc_completion_pending ? 180.0f : 32.0f; /* percentage points per second */
        float step = rate * (float)elapsed / 1000.0f;
        ud->cc_progress = (ud->cc_progress + step < target) ? ud->cc_progress + step : target;
        changed = 1;
    }
    if (ud->cc_completion_pending && ud->cc_progress >= 100.0f) {
        si_uninst_finish_page(pg);
        return;
    }
    if (g_state && changed) InvalidateRect(g_state->hwnd, NULL, FALSE);
    if (g_state && ud->cc_progress >= target && !ud->cc_completion_pending)
        KillTimer(g_state->hwnd, SI_UNINSTALL_PROGRESS_TIMER_ID);
}

static void si_uninst_appmsg(SiPage* pg, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_SI_PROGRESS) {
        float target = (float)(int)wp;
        if (target < 0) target = 0;
        if (target > 100) target = 100;
        if (target > pg->data.uninstalling.cc_progress_target)
            pg->data.uninstalling.cc_progress_target = target;
        pg->data.uninstalling.cc_progress_tick_ms = si_now_ms();
        if (g_state && !SetTimer(g_state->hwnd, SI_UNINSTALL_PROGRESS_TIMER_ID,
                                 si_display_refresh_ms(), NULL)) {
            pg->data.uninstalling.cc_progress = pg->data.uninstalling.cc_progress_target;
            InvalidateRect(g_state->hwnd, NULL, FALSE);
        }
    } else if (msg == WM_SI_FILE_PROGRESS) {
        unsigned int completed = (unsigned int)wp;
        unsigned int total = (unsigned int)lp;
        float target = total > 0
            ? 66.0f + 32.0f * (float)completed / (float)total
            : 98.0f;
        if (target > 98.0f) target = 98.0f;
        if (target > pg->data.uninstalling.cc_progress_target)
            pg->data.uninstalling.cc_progress_target = target;
        pg->data.uninstalling.cc_progress_tick_ms = si_now_ms();
        if (g_state && !SetTimer(g_state->hwnd, SI_UNINSTALL_PROGRESS_TIMER_ID,
                                 si_display_refresh_ms(), NULL)) {
            pg->data.uninstalling.cc_progress = pg->data.uninstalling.cc_progress_target;
            InvalidateRect(g_state->hwnd, NULL, FALSE);
        }
    } else if (msg == WM_SI_STATUS) {
        wchar_t* text = (wchar_t*)lp;
        if (text) {
            wcsncpy(pg->data.uninstalling.cc_status, text, 255);
            pg->data.uninstalling.cc_status[255] = 0;
            si_free(text);
            if (g_state) InvalidateRect(g_state->hwnd, NULL, FALSE);
        }
    } else if (msg == WM_SI_DONE) {
        pg->data.uninstalling.uninstalling = 0;
        pg->data.uninstalling.cc_progress_target = 100.0f;
        pg->data.uninstalling.cc_completion_pending = 1;
        pg->data.uninstalling.cc_progress_tick_ms = si_now_ms();
        if (!g_state || !SetTimer(g_state->hwnd, SI_UNINSTALL_PROGRESS_TIMER_ID,
                                  si_display_refresh_ms(), NULL))
            si_uninst_finish_page(pg);
    } else if (msg == WM_TIMER && wp == SI_UNINSTALL_PROGRESS_TIMER_ID) {
        si_uninst_progress_tick(pg);
    }
}

static void si_uninst_resize(SiPage* pg, int dpi) {
    SiWizardState* st = g_state;
    (void)dpi; if (!st) return;
    /* Reposition delete-data checkbox if it exists */
    { SiControl* c = si_ctrl_find(&st->ctrl, SI_IDC_UNINST_DELETE);
      if (c) {
          float cb_left, cb_top, cb_w, cb_h;
          si_uninst_delete_metrics(st, &pg->data.uninstalling, &cb_left, &cb_top, &cb_w, &cb_h);
          c->x = cb_left; c->y = cb_top; c->w = cb_w; c->h = cb_h;
      }
    }
    (void)pg;
}

static const SiPageVtable si_vtable_uninst = { si_uninst_create, si_uninst_destroy, si_uninst_paint, si_uninst_next, NULL, si_uninst_resize, NULL, si_uninst_appmsg };

/* ── Custom page ────────────────────────────────────────────────────────── */

static void si_custom_paint(SiPage* pg, HDC dc, RECT area) {
    if (pg->data.custom.paint_cb) pg->data.custom.paint_cb(dc, area, pg->data.custom.paint_user_data);
}

static const SiPageVtable si_vtable_custom = { NULL, NULL, si_custom_paint, NULL, NULL, NULL, NULL, NULL };

/* ═══════════════════════════════════════════════════════════════════════════
   PAGE CONSTRUCTORS
   ═══════════════════════════════════════════════════════════════════════════ */

SiPage si_page_license(const wchar_t* text, int checked_default) { SiPage p; memset(&p,0,sizeof(p)); p.type=SI_PAGE_LICENSE; p.vtable=&si_vtable_license; p.data.license.text=text; p.data.license.checked_default=checked_default; return p; }
SiPage si_page_install_dir(void) { SiPage p; memset(&p,0,sizeof(p)); p.type=SI_PAGE_INSTALL_DIR; p.vtable=&si_vtable_idir; return p; }
SiPage si_page_reinstall(void) { SiPage p; memset(&p,0,sizeof(p)); p.type=SI_PAGE_REINSTALL; p.vtable=&si_vtable_reinstall; return p; }
SiPage si_page_installing(void) { SiPage p; memset(&p,0,sizeof(p)); p.type=SI_PAGE_INSTALLING; p.vtable=&si_vtable_installing; return p; }

SiPage si_page_finished(const SiFinishedCheckbox* cbs, int count) {
    return si_page_finished_ex(cbs, count, NULL, 0);
}

SiPage si_page_finished_ex(const SiFinishedCheckbox* cbs, int count, const unsigned char* image_data, size_t image_size) {
    SiPage p; int i; memset(&p,0,sizeof(p)); p.type=SI_PAGE_FINISHED; p.vtable=&si_vtable_finished;
    if (count > SI_MAX_CHECKBOXES) count = SI_MAX_CHECKBOXES;
    p.data.finished.checkbox_count = count;
    for (i=0; i<count; i++) p.data.finished.checkboxes[i] = cbs[i];
    p.data.finished.image_data = image_data;
    p.data.finished.image_size = image_size;
    return p;
}

SiPage si_page_confirm(const unsigned char* img, size_t img_size) { return si_page_confirm_ex(img, img_size, NULL); }

SiPage si_page_confirm_ex(const unsigned char* img, size_t img_size, const wchar_t* text) {
    SiPage p; memset(&p,0,sizeof(p)); p.type=SI_PAGE_CONFIRM; p.vtable=&si_vtable_confirm;
    p.data.confirm.image_data=img; p.data.confirm.image_size=img_size; p.data.confirm.text=text; return p;
}

SiPage si_page_uninstalling(void) { return si_page_uninstalling_ex(NULL, 0); }

SiPage si_page_uninstalling_ex(const unsigned char* image_data, size_t image_size) {
    SiPage p; memset(&p,0,sizeof(p)); p.type=SI_PAGE_UNINSTALLING; p.vtable=&si_vtable_uninst;
    p.data.uninstalling.image_data = image_data; p.data.uninstalling.image_size = image_size;
    return p;
}

SiPage si_page_welcome(const unsigned char* image_data, size_t image_size) {
    SiPage p; memset(&p,0,sizeof(p)); p.type=SI_PAGE_WELCOME; p.vtable=&si_vtable_welcome;
    p.data.welcome.image_data = image_data; p.data.welcome.image_size = image_size;
    return p;
}

SiPage si_page_welcome_ex(const unsigned char* image_data, size_t image_size,
    SiWelcomeCreateFn on_create, SiWelcomeDestroyFn on_destroy,
    SiWelcomePaintFn on_paint, void* user_data) {
    SiPage p = si_page_welcome(image_data, image_size);
    p.data.welcome.on_create = on_create;
    p.data.welcome.on_destroy = on_destroy;
    p.data.welcome.on_paint = on_paint;
    p.data.welcome.user_data = user_data;
    return p;
}

SiPage si_page_custom(const wchar_t* name, SiCustomPaintFn paint_cb, void* user_data) {
    SiPage p; memset(&p,0,sizeof(p)); p.type=SI_PAGE_CUSTOM; p.vtable=&si_vtable_custom;
    p.data.custom.name=name; p.data.custom.paint_cb=paint_cb; p.data.custom.paint_user_data=user_data; return p;
}
