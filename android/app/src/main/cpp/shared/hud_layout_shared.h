#ifndef HUD_LAYOUT_SHARED_H
#define HUD_LAYOUT_SHARED_H

typedef struct hud_layout_rect {
	int x;
	int y;
	int w;
	int h;
} hud_layout_rect;

static inline int hud_layout_rects_intersect(const hud_layout_rect *a, const hud_layout_rect *b)
{
	if (!a || !b || a->w <= 0 || a->h <= 0 || b->w <= 0 || b->h <= 0)
		return 0;

	return a->x < b->x + b->w && b->x < a->x + a->w &&
	       a->y < b->y + b->h && b->y < a->y + a->h;
}

#endif
