/* si_animations.h — Page transitions (crossfade, slide).
 * Concrete decorative animations belong to the app, not this header.
 * Everything guarded by SI_FEATURE_ANIMATIONS.
 * Included from small_installer.h under #ifdef SI_IMPLEMENTATION — no include guard.
 */

#ifdef SI_FEATURE_ANIMATIONS

#define SI_TRANSITION_DURATION_MS 200

/* ── Page Transitions ──────────────────────────────────────────────────── */

static float si_ease_out(float t) { return 1.0f - (1.0f - t) * (1.0f - t); }

/* Vsync ticker: pure no-op. The wndproc invalidates after every tick anyway,
 * and si_transition_paint reads (si_now_ms() - transition_start) for its own
 * progress — no per-tick state to advance here. We subscribe purely to keep
 * the vsync thread alive while transitioning. */
static void si_transition_vsync_tick(void* user, float dt_ms) {
    (void)user; (void)dt_ms;
}

static void si_transition_cleanup(SiWizardState* st) {
    sui_vsync_detach(&st->vsync, si_transition_vsync_tick, NULL);
    if (st->transition_bitmap) {
        ((ID2D1Bitmap*)st->transition_bitmap)->lpVtbl->Base.Base.Release((IUnknown*)st->transition_bitmap);
        st->transition_bitmap = NULL;
    }
    if (st->transition_brt) {
        ((ID2D1BitmapRenderTarget*)st->transition_brt)->lpVtbl->Base.Release(
            (ID2D1RenderTarget*)st->transition_brt);
        st->transition_brt = NULL;
    }
    st->transitioning = 0;
    InvalidateRect(st->hwnd, NULL, FALSE);
}

static void si_transition_capture(SiWizardState* st) {
    ID2D1RenderTarget* rt = (ID2D1RenderTarget*)st->renderer.render_target;
    ID2D1BitmapRenderTarget* brt = NULL;
    void* old_rt;
    HRESULT hr;
    if (!rt) return;

    hr = rt->lpVtbl->CreateCompatibleRenderTarget(rt, NULL, NULL, NULL, 0, (void**)&brt);
    if (FAILED(hr) || !brt) return;

    /* Temporarily swap renderer's render target to the offscreen one */
    old_rt = st->renderer.render_target;
    st->renderer.render_target = brt;

    brt->lpVtbl->Base.BeginDraw((ID2D1RenderTarget*)brt);
    { D2D1_COLOR_F bg;
      bg.r=(float)((st->scheme->background>>16)&0xFF)/255.0f;
      bg.g=(float)((st->scheme->background>>8)&0xFF)/255.0f;
      bg.b=(float)(st->scheme->background&0xFF)/255.0f; bg.a=1.0f;
      brt->lpVtbl->Base.Clear((ID2D1RenderTarget*)brt, &bg);
    }
    { RECT cr; float bh;
      GetClientRect(st->hwnd, &cr);
      bh = (float)si_scale(st, SI_BTNAREA);
      { float w=(float)cr.right, h=(float)cr.bottom;
        si_renderer_fill_rect(&st->renderer, 0, h-bh, w, bh, st->scheme->btn_area);
        si_renderer_draw_line(&st->renderer, 0, h-bh, w, h-bh, si_button_area_separator_color(st)); }
      { RECT pa; pa.left=0; pa.top=0; pa.right=cr.right; pa.bottom=cr.bottom-(LONG)bh;
        if (st->current_page >= 0 && st->current_page < st->page_count) {
            SiPage* pg = &st->pages[st->current_page];
            if (pg->vtable && pg->vtable->on_paint) pg->vtable->on_paint(pg, NULL, pa);
        }
      }
    }
    brt->lpVtbl->Base.EndDraw((ID2D1RenderTarget*)brt, NULL, NULL);

    /* Restore original render target */
    st->renderer.render_target = old_rt;

    /* Extract bitmap */
    { ID2D1Bitmap* bmp = NULL;
      brt->lpVtbl->GetBitmap(brt, &bmp);
      st->transition_bitmap = bmp;
    }
    st->transition_brt = brt;
}

static void si_transition_start(SiWizardState* st, int direction) {
    st->transitioning = 1;
    st->transition_start = si_now_ms();
    st->transition_direction = direction;
    sui_vsync_attach(&st->vsync, si_transition_vsync_tick, NULL);
}

static void si_transition_paint(SiWizardState* st) {
    ID2D1RenderTarget* rt = (ID2D1RenderTarget*)st->renderer.render_target;
    uint64_t elapsed = si_now_ms() - st->transition_start;
    float t = (float)elapsed / (float)SI_TRANSITION_DURATION_MS;
    RECT cr;

    if (t >= 1.0f) { si_transition_cleanup(st); return; }
    t = si_ease_out(t);

    if (!rt || !st->transition_bitmap) { si_transition_cleanup(st); return; }

    GetClientRect(st->hwnd, &cr);

    if (st->transition_type == SI_TRANSITION_CROSSFADE) {
        D2D1_RECT_F dest;
        dest.left=0; dest.top=0; dest.right=(float)cr.right; dest.bottom=(float)cr.bottom;
        rt->lpVtbl->DrawBitmap(rt, (ID2D1Bitmap*)st->transition_bitmap, &dest,
            1.0f - t, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, NULL);
    } else if (st->transition_type == SI_TRANSITION_SLIDE) {
        float w = (float)cr.right;
        float offset = w * t * (float)st->transition_direction;
        D2D1_RECT_F dest;
        dest.left = -offset; dest.top = 0;
        dest.right = w - offset; dest.bottom = (float)cr.bottom;
        rt->lpVtbl->DrawBitmap(rt, (ID2D1Bitmap*)st->transition_bitmap, &dest,
            1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, NULL);
    }
}

#endif /* SI_FEATURE_ANIMATIONS */
