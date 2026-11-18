/* Native bitmap storage for immutable XL side-selected frames, included by piggy.c */
struct xl_pog_side_views {
	int offsets[MAX_BITMAP_FILES];
	unsigned counts[MAX_BITMAP_FILES], total;
	grs_bitmap *frames;
};
static struct xl_pog_side_views *xl_pog_live_side_views;

void xl_pog_free_side_views(void *storage)
{
	struct xl_pog_side_views *views = (struct xl_pog_side_views *) storage;
	unsigned i;
	if (!views) return;
	if (views->frames) {
		for (i = 0; i < views->total; ++i) ogl_freebmtexture(&views->frames[i]);
		d_free(views->frames);
	}
	d_free(views);
}

void *xl_pog_prepare_side_views(const struct xl_pog_resources *resources)
{
	struct xl_pog_side_views *views;
	int i, previous;
	views = (struct xl_pog_side_views *) d_malloc(sizeof(*views));
	if (!views) return NULL;
	memset(views, 0, sizeof(*views));
	for (i = 0; i < MAX_BITMAP_FILES; ++i) views->offsets[i] = -1;
	for (i = 0; i < xl_pog_count(resources); ++i) {
		const struct xl_pog_bitmap *entry = xl_pog_entry(resources, i);
		if (!entry->side_frames) continue;
		if (!entry->scoped_texture || !entry->rgba || entry->index >= MAX_BITMAP_FILES ||
		    entry->frames < 2 || entry->frames > 128 || views->offsets[entry->index] >= 0) goto invalid;
		/* Aliases of one retained image share immutable views, never a current frame */
		for (previous = 0; previous < i; ++previous) {
			const struct xl_pog_bitmap *other = xl_pog_entry(resources, previous);
			if (other->side_frames && other->offset == entry->offset) {
				if (other->size != entry->size || other->frames != entry->frames || other->width != entry->width ||
				    other->frame_height != entry->frame_height) goto invalid;
				views->offsets[entry->index] = views->offsets[other->index];
				break;
			}
		}
		if (views->offsets[entry->index] < 0) {
			if (views->total > MAX_BITMAP_FILES * 128u - entry->frames) goto invalid;
			views->offsets[entry->index] = (int) views->total;
			views->total += entry->frames;
		}
		views->counts[entry->index] = entry->frames;
	}
	if (views->total) {
		views->frames = (grs_bitmap *) d_malloc(views->total * sizeof(*views->frames));
		if (!views->frames) goto invalid;
		memset(views->frames, 0, views->total * sizeof(*views->frames));
	}
	return views;
invalid:
	xl_pog_free_side_views(views);
	return NULL;
}

static void xl_pog_install_side_views(void *storage, const struct xl_pog_resources *resources)
{
	int i;
	xl_pog_live_side_views = (struct xl_pog_side_views *) storage;
	if (!xl_pog_live_side_views) return;
	for (i = 0; i < xl_pog_count(resources); ++i) {
		const struct xl_pog_bitmap *entry = xl_pog_entry(resources, i);
		unsigned frame;
		if (!entry->side_frames) continue;
		for (frame = 0; frame < entry->frames; ++frame) {
			grs_bitmap *view = &xl_pog_live_side_views->frames[xl_pog_live_side_views->offsets[entry->index] + frame];
			*view = GameBitmaps[entry->index];
			view->bm_data = Bitmap_replacement_data + entry->offset + frame * (entry->size / entry->frames);
			view->gltexture = NULL;
			view->gltexture_mask = NULL;
			gr_set_bitmap_flags(view, dxx_xl_texture_frame_flags(entry->transparent_frames, entry->super_frames, frame));
		}
	}
}

struct _grs_bitmap *xl_pog_side_frame(int bitmap, int frame)
{
	const struct xl_pog_side_views *views = xl_pog_live_side_views;
	if (!views || bitmap < 0 || bitmap >= MAX_BITMAP_FILES || frame < 0 || (unsigned) frame >= views->counts[bitmap]) return NULL;
	return &views->frames[views->offsets[bitmap] + frame];
}

int xl_pog_side_frame_count(int bitmap)
{
	return xl_pog_live_side_views && bitmap >= 0 && bitmap < MAX_BITMAP_FILES ? (int) xl_pog_live_side_views->counts[bitmap] : 0;
}

void xl_pog_side_views_discard_context(void)
{
	unsigned i;
	if (!xl_pog_live_side_views) return;
	/* The renderer owns deletion/reset of the texture registry itself */
	for (i = 0; i < xl_pog_live_side_views->total; ++i) {
		xl_pog_live_side_views->frames[i].gltexture = NULL;
		xl_pog_live_side_views->frames[i].gltexture_mask = NULL;
	}
}
