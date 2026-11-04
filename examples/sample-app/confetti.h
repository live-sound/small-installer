/* confetti.h -- a finished-page decoration through SiFinishedAnim. It moves on the wizard's vsync pump, so its speed does not depend
 * on the display's refresh rate, and paints with the renderer's scratch brush, so a frame allocates nothing. Include it after
 * small_installer.h, in the file that defines SI_IMPLEMENTATION. */
#ifndef SAMPLE_CONFETTI_H
#define SAMPLE_CONFETTI_H

#define CONFETTI_COUNT 60

typedef struct ConfettiParticle {
    float x, y, vx, vy;
    float rotation, rot_speed;
    float width, height;
    uint32_t color;
} ConfettiParticle;

typedef struct ConfettiState {
    ConfettiParticle particles[CONFETTI_COUNT];
    int active;
    float area_w, area_h, dpi_scale;
    float img_aspect;  /* >0 → clip confetti to the image region on the finished page */
} ConfettiState;

static ConfettiState g_confetti = {0};

static const uint32_t g_confetti_colors[] = {
    0xE74C3C, 0x3498DB, 0xF1C40F, 0x2ECC71,
    0xE91E63, 0xFF9800, 0x9B59B6, 0x1ABC9C
};

static uint32_t g_confetti_rand_state = 0;
static uint32_t confetti_rand(void) {
    g_confetti_rand_state = g_confetti_rand_state * 1103515245 + 12345;
    return (g_confetti_rand_state >> 16) & 0x7FFF;
}
static float confetti_randf(float mn, float mx) {
    return mn + (float)(confetti_rand() % 10000) / 10000.0f * (mx - mn);
}

static float confetti_region_w(float area_h) {
    return g_confetti.img_aspect > 0 ? area_h * g_confetti.img_aspect : 0;
}

static void confetti_init_particle(ConfettiParticle* p, float area_w, float s) {
    p->x = confetti_randf(0, area_w);
    p->y = confetti_randf(-300.0f * s, -20.0f * s);
    p->vx = confetti_randf(-0.5f * s, 0.5f * s);
    p->vy = confetti_randf(0.7f * s, 2.0f * s);
    p->rotation = confetti_randf(0, 6.2832f);
    p->rot_speed = confetti_randf(-0.1f, 0.1f);
    p->width = 4.0f * s;
    p->height = 8.0f * s;
    p->color = g_confetti_colors[confetti_rand() % 8];
}

/* Vsync tick: dt_ms is delivered by the wndproc pump (vblank-locked).
 * Normalize to a 16ms baseline so motion stays the same speed across 60/120/144 Hz. */
static void confetti_vsync_tick(void* user, float dt_ms) {
    int i; float dt;
    float area_w, area_h, s, rw;
    (void)user;
    if (!g_confetti.active) return;

    dt = dt_ms / 16.0f;
    if (dt < 0.1f) dt = 0.1f;
    if (dt > 4.0f) dt = 4.0f;

    area_w = g_confetti.area_w;
    area_h = g_confetti.area_h;
    s = g_confetti.dpi_scale;
    rw = confetti_region_w(area_h);
    if (rw > 0) area_w = rw;

    for (i = 0; i < CONFETTI_COUNT; i++) {
        ConfettiParticle* p = &g_confetti.particles[i];
        p->x += p->vx * dt;
        p->y += p->vy * dt;
        p->vy += 0.035f * s * dt;
        p->rotation += p->rot_speed * dt;
        if (p->y > area_h) {
            p->x = confetti_randf(0, area_w);
            p->y = confetti_randf(-50.0f * s, -10.0f * s);
            p->vx = confetti_randf(-0.5f * s, 0.5f * s);
            p->vy = confetti_randf(0.7f * s, 2.0f * s);
            p->rot_speed = confetti_randf(-0.1f, 0.1f);
        }
    }
    /* InvalidateRect happens in wndproc after every vsync tick — no need here. */
}

static void confetti_attach(void* user, SiWizardState* st, float area_w, float area_h, float dpi_scale) {
    int i;
    (void)user;
    if (!st) return;

    /* Pull image aspect from the finished page so confetti clips to the image region. */
    g_confetti.img_aspect = 0.0f;
    if (st->current_page >= 0 && st->current_page < st->page_count) {
        SiPage* pg = &st->pages[st->current_page];
        if (pg->type == SI_PAGE_FINISHED &&
            pg->data.finished.img_width > 0 && pg->data.finished.img_height > 0)
        {
            g_confetti.img_aspect = (float)pg->data.finished.img_width
                                  / (float)pg->data.finished.img_height;
        }
    }

    g_confetti.area_w    = area_w;
    g_confetti.area_h    = area_h;
    g_confetti.dpi_scale = dpi_scale;
    g_confetti_rand_state = (uint32_t)si_now_ms();

    { float spawn_w = area_w;
      float rw = confetti_region_w(area_h);
      if (rw > 0) spawn_w = rw;
      for (i = 0; i < CONFETTI_COUNT; i++)
          confetti_init_particle(&g_confetti.particles[i], spawn_w, dpi_scale);
    }

    g_confetti.active = 1;
    sui_vsync_attach(&st->vsync, confetti_vsync_tick, NULL);
}

static void confetti_detach(void* user) {
    (void)user;
    if (!g_confetti.active) return;
    if (g_state) sui_vsync_detach(&g_state->vsync, confetti_vsync_tick, NULL);
    g_confetti.active = 0;
}

static void confetti_paint(void* user, SiWizardState* st, void* render_target, float area_h) {
    ID2D1RenderTarget* rt = (ID2D1RenderTarget*)render_target;
    ID2D1SolidColorBrush* br;
    D2D1_MATRIX_3X2_F identity;
    float rw;
    int i;
    (void)user;
    if (!rt || !g_confetti.active) return;

    identity._11=1; identity._12=0; identity._21=0; identity._22=1; identity._31=0; identity._32=0;

    rw = confetti_region_w(area_h);
    if (rw > 0) {
        D2D1_RECT_F clip;
        clip.left = 0; clip.top = 0; clip.right = rw; clip.bottom = area_h;
        rt->lpVtbl->PushAxisAlignedClip(rt, &clip, 0);
    }

    /* Cached scratch brush, recolored per particle — zero allocs per frame. */
    for (i = 0; i < CONFETTI_COUNT; i++) {
        ConfettiParticle* p = &g_confetti.particles[i];
        float cx = p->x + p->width/2, cy = p->y + p->height/2;
        float cosA, sinA;
        D2D1_MATRIX_3X2_F xform;
        D2D1_RECT_F rc;

        if (p->y < -p->height || p->y > area_h) continue;

        br = sui__brush(&st->renderer, p->color, 1.0f);
        if (!br) continue;

        cosA = si_cosf(p->rotation);
        sinA = si_sinf(p->rotation);
        xform._11 = cosA; xform._12 = sinA;
        xform._21 = -sinA; xform._22 = cosA;
        xform._31 = cx - cosA*cx + sinA*cy;
        xform._32 = cy - sinA*cx - cosA*cy;

        rt->lpVtbl->SetTransform(rt, &xform);
        rc.left = p->x; rc.top = p->y; rc.right = p->x + p->width; rc.bottom = p->y + p->height;
        rt->lpVtbl->FillRectangle(rt, &rc, (ID2D1Brush*)br);
    }
    rt->lpVtbl->SetTransform(rt, &identity);

    if (rw > 0)
        rt->lpVtbl->PopAxisAlignedClip(rt);
}

#endif
