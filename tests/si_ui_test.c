#include <stdio.h>
#define SI_FEATURE_D2D
#define SI_FEATURE_ANIMATIONS
#define SI_MODE_INSTALL
#define SI_IMPLEMENTATION
#include <small_installer/small_installer.h>
#include "../examples/sample-app/welcome_bubbles.h"
#include "win32_test_support.h"

int si_main(HINSTANCE instance) { (void)instance; return 0; }
static int failures, last_command;
static void check(int ok, const char* message) {
    if (!ok) { fprintf(stderr, "FAIL: %s\n", message); ++failures; }
}
static int record_command(SiPage* page, WPARAM wp, LPARAM lp) {
    (void)page; (void)lp;
    last_command = LOWORD(wp);
    return 1;
}
static void record_click(int id, void* data) { (void)data; last_command = id; }

static int move_entries;
static LRESULT CALLBACK moving_wizard_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_ENTERSIZEMOVE) {
        ++move_entries;
        /* Finish the real caption-move loop using only this private window's queue. */
        PostMessageW(hwnd, WM_KEYDOWN, VK_RIGHT, 0);
        PostMessageW(hwnd, WM_KEYDOWN, VK_RIGHT, 0);
        PostMessageW(hwnd, WM_KEYDOWN, VK_RETURN, 0);
    }
    return si_wndproc(hwnd, msg, wp, lp);
}

static void check_reinstall_move(int dpi, int clean_only) {
    static SiWizardState st;
    WNDCLASSW wc = {0};
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT before, after, bounds = {0};
    MINMAXINFO limits = {0};
    MSG quit;
    int entered = move_entries, repeat;
    memset(&st, 0, sizeof(st));
    st.dpi = dpi;
    st.scheme = &sui_scheme_light;
    st.page_count = 1;
    st.reinstall_clean_only = clean_only;
    wcscpy(st.app_name, L"Example App");
    wcscpy(st.app_version, L"1.0.0");
    wcscpy(st.existing_version, L"2.0.0");
    st.pages[0] = si_page_reinstall();
    g_state = &st;
    wc.lpfnWndProc = moving_wizard_proc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"SI_MovingWizardTest";
    RegisterClassW(&wc);
    bounds.right = si_scale(&st, SI_BASE_W);
    bounds.bottom = si_scale(&st, SI_BASE_H);
    AdjustWindowRectEx(&bounds, style, FALSE, 0);
    st.hwnd = CreateWindowExW(0, wc.lpszClassName, L"Setup move regression", style,
        100, 100, bounds.right - bounds.left, bounds.bottom - bounds.top,
        NULL, NULL, wc.hInstance, NULL);
    check(st.hwnd != NULL, "create fixed-size wizard");
    if (!st.hwnd) { g_state = NULL; return; }
    si_renderer_init_fields(&st.renderer);
    check(si_renderer_init(&st.renderer, st.hwnd), "initialize wizard renderer");
    si_ctrl_host_init(&st.ctrl, st.hwnd, &st.renderer, st.scheme, dpi);
    si_reinstall_create(&st.pages[0], st.hwnd);
    ShowWindow(st.hwnd, SW_SHOW);
    UpdateWindow(st.hwnd);
    check(sui_ctrl_find(&st.ctrl, 406) != NULL, "reinstall prose enters the accessible control tree");
    GetWindowRect(st.hwnd, &before);
    SendMessageW(st.hwnd, WM_GETMINMAXINFO, 0, (LPARAM)&limits);
    check(limits.ptMinTrackSize.x <= before.right - before.left,
        "wrapped reinstall prose cannot increase the wizard's minimum width");
    check(limits.ptMinTrackSize.y <= before.bottom - before.top,
        "reinstall controls cannot increase the wizard's minimum height");
    for (repeat = 0; repeat < 3; ++repeat) {
        SendMessageW(st.hwnd, WM_SYSCOMMAND, SC_MOVE, 0);
        GetWindowRect(st.hwnd, &after);
        check(after.right - after.left == before.right - before.left &&
              after.bottom - after.top == before.bottom - before.top,
            "caption movement preserves the wizard's dimensions");
    }
    check(move_entries == entered + 3, "all caption moves enter the native move loop");
    DestroyWindow(st.hwnd);
    si_renderer_release(&st.renderer);
    g_state = NULL;
    while (PeekMessageW(&quit, NULL, WM_QUIT, WM_QUIT, PM_REMOVE)) {}
}

static int run_tests(int argc, const char* const argv[]) {
    static SiWizardState st;
    SiPage page;
    SiPageVtable commands = {0};
    SuiControl* control;
    SuiUiaProvider* provider;
    VARIANT name;
    wchar_t paragraph[256];
    int i;
    (void)argc; (void)argv;
    st.hwnd = CreateWindowExW(0, L"STATIC", L"UI regression", WS_POPUP,
        0, 0, 640, 480, NULL, NULL, GetModuleHandleW(NULL), NULL);
    check(st.hwnd != NULL, "create test window");
    st.dpi = 96;
    st.scheme = &sui_scheme_light;
    st.current_page = 0;
    st.page_count = 1;
    sui_renderer_init_fields(&st.renderer);
    sui_ctrl_host_init(&st.ctrl, st.hwnd, &st.renderer, st.scheme, 96);
    g_state = &st;

    commands.on_command = record_command;
    st.pages[0].vtable = &commands;
    si_wizard_on_click(SI_IDC_IDIR_BROWSE, &st);
    check(last_command == SI_IDC_IDIR_BROWSE, "custom Browse click reaches page handler");

    page = si_page_license(L"License text", 0);
    si_lic_create(&page, st.hwnd);
    sui_ctrl_find(&st.ctrl, SI_IDC_LICENSE_CHECK)->state |= SUI_CTRL_CHECKED;
    si_lic_destroy(&page);
    si_lic_create(&page, st.hwnd);
    check((sui_ctrl_find(&st.ctrl, SI_IDC_LICENSE_CHECK)->state & SUI_CTRL_CHECKED) != 0,
        "license acceptance survives Back");
    si_lic_destroy(&page);

    page = si_page_install_dir();
    wcscpy(st.install_path, L"C:\\Original");
    si_idir_create(&page, st.hwnd);
    wcscpy(sui_ctrl_find(&st.ctrl, SI_IDC_IDIR_EDIT)->text, L"C:\\Edited destination");
    si_idir_destroy(&page);
    si_idir_create(&page, st.hwnd);
    check(!wcscmp(sui_ctrl_find(&st.ctrl, SI_IDC_IDIR_EDIT)->text, L"C:\\Edited destination"),
        "edited path survives Back before pressing Next");
    si_idir_destroy(&page);

    for (i = 0; i < 255; ++i) paragraph[i] = L'x';
    paragraph[255] = 0;
    si_accessible_text(&st.ctrl, 400, paragraph, 10, 20, 300, 30);
    control = sui_ctrl_find(&st.ctrl, 400);
    check(control && !(control->state & SUI_CTRL_HIDDEN), "painted prose stays accessible");
    provider = (SuiUiaProvider*)sui_uia_create_control_provider(&st.ctrl, 0);
    provider->lpVtbl->GetPropertyValue(provider, SUI_UIA_NamePropertyId, &name);
    check(name.vt == VT_BSTR && !wcscmp(name.bstrVal, paragraph), "painted paragraph has complete accessible text");
    VariantClear(&name);
    provider->lpVtbl->Release(provider);
    sui_ctrl_draw_label(&st.ctrl, control); /* No live renderer: this must not draw a second label. */
    sui_ctrl_clear_page(&st.ctrl);

    sui_ctrl_add_button(&st.ctrl, 1, 0, 0, 100, 30, L"Continue", 1);
    sui_ctrl_add_button(&st.ctrl, 2, 110, 0, 100, 30, L"Cancel", 0);
    st.ctrl.on_click = record_click;
    sui_ctrl_set_focus(&st.ctrl, 1);
    last_command = 0;
    sui_ctrl_on_keydown(&st.ctrl, VK_RETURN);
    sui_ctrl_on_keyup(&st.ctrl, VK_RETURN);
    check(last_command == 2, "Enter activates focused button before default button");

    /* A page transition must not unsubscribe the ongoing decoration. */
    sui_vsync_init(&st.vsync, st.hwnd);
    sample_bubbles.active = sui_vsync_attach(&st.vsync, sample_bubbles_tick, &st);
    check(sample_bubbles.active, "welcome animation subscribes to the frame clock");
    si_transition_start(&st, 1);
    si_transition_cleanup(&st);
    check(st.vsync.ticker_count == 1, "transition cleanup preserves the welcome animation");
    st.pages[0] = sample_welcome_page(NULL, 0);
    st.pages[0].data.welcome.img_width = 458;
    st.pages[0].data.welcome.img_height = 877;
    sample_bubbles.random = 123;
    for (i = 0; i < SAMPLE_BUBBLE_COUNT; ++i)
        sample_bubble_init(&sample_bubbles.particles[i], 200, 400, 0);
    sample_bubbles.particles[0].x = 100;
    sample_bubbles.particles[0].vx = 0.2f;
    sample_bubbles_tick(&st, 16);
    check(sample_bubbles.particles[0].x > 100 && sample_bubbles.particles[0].life == 1,
        "bubbles drift and age with elapsed frame time");
    sample_bubbles.particles[0].life = sample_bubbles.particles[0].max_life;
    sample_bubbles_tick(&st, 16);
    check(sample_bubbles.particles[0].life == 0,
        "expired bubbles respawn fully faded out");
    sample_bubbles_stop(NULL, NULL);
    check(st.vsync.ticker_count == 0 && !sample_bubbles.active,
        "leaving Welcome stops its animation");
    page = sample_welcome_page(NULL, 0);
    page.vtable->on_create(&page, st.hwnd);
    check(!sample_bubbles.active && st.vsync.ticker_count == 0,
        "missing artwork keeps Welcome static");
    page.vtable->on_destroy(&page);
    sui_vsync_stop(&st.vsync);

    sui_ctrl_host_release(&st.ctrl);
    DestroyWindow(st.hwnd);
    g_state = NULL;
    for (i = 96; i <= 192; i += 48) {
        check_reinstall_move(i, 0);
        check_reinstall_move(i, 1);
    }
    return failures ? 1 : 0;
}

int main(int argc, const char* const argv[]) {
    return sui_test_main(argc, argv, run_tests);
}
