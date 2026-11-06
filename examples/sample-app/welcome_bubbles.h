/* Floating bubbles over the welcome image. The
 * default page still paints its text and accessibility controls. */
#ifndef SAMPLE_WELCOME_BUBBLES_H
#define SAMPLE_WELCOME_BUBBLES_H

#define SAMPLE_BUBBLE_COUNT 16
typedef struct SampleBubble {
    float x, y, vx, vy, radius, peak_alpha, life, max_life;
} SampleBubble;
static struct {
    SampleBubble particles[SAMPLE_BUBBLE_COUNT];
    uint32_t random;
    int active;
} sample_bubbles;

static float sample_bubble_random(float low, float high) {
    sample_bubbles.random = sample_bubbles.random * 1103515245 + 12345;
    return low + (float)(((sample_bubbles.random >> 16) & 0x7FFF) % 10000)
        / 10000.0f * (high - low);
}

static float sample_bubble_smoothstep(float t) {
    if (t <= 0) return 0;
    if (t >= 1) return 1;
    return t * t * (3 - 2 * t);
}

static void sample_bubble_init(SampleBubble* p, float width, float height, int scatter) {
    float angle = sample_bubble_random(0, 6.2832f);
    float speed = sample_bubble_random(0.08f, 0.25f);
    float radius = sample_bubble_random(0, 1);
    p->x = sample_bubble_random(0, width);
    p->y = sample_bubble_random(0, height);
    p->vx = si_cosf(angle) * speed;
    p->vy = si_sinf(angle) * speed;
    p->radius = 5 + radius * radius * 20;
    /* Strong enough to show on the pale sample artwork. */
    p->peak_alpha = sample_bubble_random(0.18f, 0.34f);
    p->max_life = sample_bubble_random(360, 840);
    p->life = scatter ? sample_bubble_random(0, p->max_life) : 0;
}

static void sample_bubbles_tick(void* user, float dt_ms) {
    SiWizardState* st = (SiWizardState*)user;
    SiPage* page;
    SiRectF image;
    float scale, dt;
    int i;
    if (!st || !sample_bubbles.active || st->current_page < 0 || st->current_page >= st->page_count) return;
    page = &st->pages[st->current_page];
    if (page->type != SI_PAGE_WELCOME) return;
    scale = si_dpi_scale(st);
    si_media_split_layout(st, si_page_area_for_state(st),
        page->data.welcome.img_width, page->data.welcome.img_height, &image, NULL);
    dt = dt_ms / 16.0f;
    if (dt < 0) dt = 0;
    if (dt > 4) dt = 4;
    for (i = 0; i < SAMPLE_BUBBLE_COUNT; ++i) {
        SampleBubble* p = &sample_bubbles.particles[i];
        p->x += p->vx * dt;
        p->y += p->vy * dt;
        p->vx += sample_bubble_random(-0.003f, 0.003f) * dt;
        p->vy += sample_bubble_random(-0.003f, 0.003f) * dt;
        if (p->vx > 0.28f) p->vx = 0.28f;
        if (p->vx < -0.28f) p->vx = -0.28f;
        if (p->vy > 0.28f) p->vy = 0.28f;
        if (p->vy < -0.28f) p->vy = -0.28f;
        p->life += dt;
        if (p->life >= p->max_life)
            sample_bubble_init(p, image.w / scale, image.h / scale, 0);
    }
}

static void sample_bubbles_start(SiPage* page, HWND parent, void* user) {
    SiRectF image;
    BOOL animations = TRUE;
    float scale;
    int i;
    (void)parent; (void)user;
    SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animations, 0);
    if (!animations || !g_state || !page->data.welcome.img_handle) return;
    scale = si_dpi_scale(g_state);
    si_media_split_layout(g_state, si_page_area_for_state(g_state),
        page->data.welcome.img_width, page->data.welcome.img_height, &image, NULL);
    sample_bubbles.random = (uint32_t)si_now_ms();
    for (i = 0; i < SAMPLE_BUBBLE_COUNT; ++i)
        sample_bubble_init(&sample_bubbles.particles[i], image.w / scale, image.h / scale, 1);
    sample_bubbles.active = sui_vsync_attach(&g_state->vsync, sample_bubbles_tick, g_state);
}

static void sample_bubbles_stop(SiPage* page, void* user) {
    (void)page; (void)user;
    if (sample_bubbles.active && g_state)
        sui_vsync_detach(&g_state->vsync, sample_bubbles_tick, g_state);
    sample_bubbles.active = 0;
}

static void sample_bubbles_paint(SiPage* page, HDC dc, RECT area) {
    ID2D1RenderTarget* target = (ID2D1RenderTarget*)g_state->renderer.render_target;
    ID2D1SolidColorBrush* brush;
    SiRectF image;
    D2D1_RECT_F clip;
    float scale;
    int i;
    si_welcome_paint(page, dc, area);
    if (!target || !sample_bubbles.active) return;
    scale = si_dpi_scale(g_state);
    si_media_split_layout(g_state, area, page->data.welcome.img_width,
        page->data.welcome.img_height, &image, NULL);
    clip.left = image.x; clip.top = image.y;
    clip.right = image.x + image.w; clip.bottom = image.y + image.h;
    target->lpVtbl->PushAxisAlignedClip(target, &clip, 0);
    brush = sui__brush(&g_state->renderer, 0xFFF8E7, 1);
    for (i = 0; brush && i < SAMPLE_BUBBLE_COUNT; ++i) {
        SampleBubble* p = &sample_bubbles.particles[i];
        float t = p->life / p->max_life;
        D2D1_COLOR_F color = {1, 0.973f, 0.906f, 0};
        D2D1_ELLIPSE circle;
        color.a = p->peak_alpha * sample_bubble_smoothstep(t / 0.15f)
            * (1 - sample_bubble_smoothstep((t - 0.75f) / 0.25f));
        if (color.a < 0.005f) continue;
        brush->lpVtbl->SetColor(brush, &color);
        circle.point.x = image.x + p->x * scale;
        circle.point.y = image.y + p->y * scale;
        circle.radiusX = circle.radiusY = p->radius * scale;
        target->lpVtbl->FillEllipse(target, &circle, (ID2D1Brush*)brush);
    }
    target->lpVtbl->PopAxisAlignedClip(target);
}

static SiPage sample_welcome_page(const unsigned char* image, size_t size) {
    static const SiPageVtable vtable = {
        si_welcome_create, si_welcome_destroy, sample_bubbles_paint,
        si_welcome_next, NULL, NULL, NULL, NULL
    };
    SiPage page = si_page_welcome_ex(image, size, sample_bubbles_start, sample_bubbles_stop, NULL, NULL);
    page.vtable = &vtable;
    return page;
}
#endif
