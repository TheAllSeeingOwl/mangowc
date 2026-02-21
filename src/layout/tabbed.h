#include <cairo.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <pango/pangocairo.h>
#include <wlr/interfaces/wlr_buffer.h>

#ifndef DRM_FORMAT_ARGB8888
#define DRM_FORMAT_ARGB8888 0x34325241
#endif

/* Simple pixel buffer wrapper for wlr_buffer */
struct tabbar_buffer {
	struct wlr_buffer base;
	void *data;
	uint32_t stride;
};

static void tabbar_buffer_destroy(struct wlr_buffer *wlr_buf) {
	struct tabbar_buffer *buf = wl_container_of(wlr_buf, buf, base);
	free(buf->data);
	free(buf);
}

static bool tabbar_buffer_begin_data_ptr_access(struct wlr_buffer *wlr_buf,
	uint32_t flags, void **data, uint32_t *format, size_t *stride) {
	struct tabbar_buffer *buf = wl_container_of(wlr_buf, buf, base);
	*data = buf->data;
	*format = DRM_FORMAT_ARGB8888;
	*stride = buf->stride;
	return true;
}

static void tabbar_buffer_end_data_ptr_access(struct wlr_buffer *wlr_buf) {
	/* no-op */
}

static const struct wlr_buffer_impl tabbar_buffer_impl = {
	.destroy = tabbar_buffer_destroy,
	.begin_data_ptr_access = tabbar_buffer_begin_data_ptr_access,
	.end_data_ptr_access = tabbar_buffer_end_data_ptr_access,
};

static void tabbar_draw_rounded_rect(cairo_t *cr, double x, double y,
	double w, double h, double r, bool round_tl, bool round_tr) {
	cairo_new_sub_path(cr);
	if (round_tr)
		cairo_arc(cr, x + w - r, y + r, r, -M_PI / 2, 0);
	else
		cairo_line_to(cr, x + w, y);

	cairo_line_to(cr, x + w, y + h);
	cairo_line_to(cr, x, y + h);

	if (round_tl)
		cairo_arc(cr, x + r, y + r, r, M_PI, 3 * M_PI / 2);
	else
		cairo_line_to(cr, x, y);

	cairo_close_path(cr);
}

static struct wlr_buffer *tabbar_create_text_buffer(const char *text,
	int width, int height, const float *bg_color, const float *text_color,
	int radius, bool round_tl, bool round_tr) {

	struct tabbar_buffer *buf = calloc(1, sizeof(*buf));
	if (!buf)
		return NULL;

	buf->stride = width * 4;
	buf->data = calloc(1, buf->stride * height);
	if (!buf->data) {
		free(buf);
		return NULL;
	}

	wlr_buffer_init(&buf->base, &tabbar_buffer_impl, width, height);

	cairo_surface_t *surface = cairo_image_surface_create_for_data(
		buf->data, CAIRO_FORMAT_ARGB32, width, height, buf->stride);
	cairo_t *cr = cairo_create(surface);

	/* Clear to transparent */
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr, 0, 0, 0, 0);
	cairo_paint(cr);

	/* Rounded background */
	tabbar_draw_rounded_rect(cr, 0, 0, width, height, radius, round_tl, round_tr);
	cairo_set_source_rgba(cr, bg_color[0] * bg_color[3],
		bg_color[1] * bg_color[3], bg_color[2] * bg_color[3], bg_color[3]);
	cairo_fill(cr);

	/* Text */
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	PangoLayout *layout = pango_cairo_create_layout(cr);
	PangoFontDescription *font_desc = pango_font_description_from_string(tabbar_font_family);
	pango_font_description_set_absolute_size(font_desc, tabbar_font_size * PANGO_SCALE);
	pango_layout_set_font_description(layout, font_desc);
	pango_layout_set_text(layout, text, -1);
	pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
	pango_layout_set_width(layout, (width - 12) * PANGO_SCALE);

	/* Center text vertically and horizontally */
	int tw, th;
	pango_layout_get_pixel_size(layout, &tw, &th);
	cairo_move_to(cr, (width - tw) / 2.0, (height - th) / 2.0);
	cairo_set_source_rgba(cr, text_color[0], text_color[1], text_color[2], text_color[3]);
	pango_cairo_show_layout(cr, layout);

	g_object_unref(layout);
	pango_font_description_free(font_desc);
	cairo_destroy(cr);
	cairo_surface_destroy(surface);

	return &buf->base;
}

static void tabbar_cleanup(Monitor *m) {
	if (m->tabbar_tree) {
		/* Re-enable all hidden scene nodes before cleanup */
		Client *c;
		wl_list_for_each(c, &clients, link) {
			if (VISIBLEON(c, m) && ISTILED(c))
				wlr_scene_node_set_enabled(&c->scene->node, true);
		}
		wlr_scene_node_destroy(&m->tabbar_tree->node);
		m->tabbar_tree = NULL;
	}
	free(m->tabbar_tab_offsets);
	m->tabbar_tab_offsets = NULL;
	free(m->tabbar_clients);
	m->tabbar_clients = NULL;
	m->tabbar_n = 0;
}

/* Called from buttonpress to handle tab bar clicks.
 * Returns true if click was on a tab and handled. */
static bool tabbar_handle_click(Monitor *m, double cx, double cy) {
	if (!m->tabbar_tree || m->tabbar_n == 0)
		return false;

	if (cy < m->tabbar_y || cy >= m->tabbar_y + m->tabbar_h)
		return false;
	if (cx < m->tabbar_x || cx >= m->tabbar_x + m->tabbar_total_w)
		return false;

	double rel_x = cx - m->tabbar_x;
	for (int i = 0; i < m->tabbar_n; i++) {
		int tab_start = m->tabbar_tab_offsets[i];
		int tab_end = (i < m->tabbar_n - 1) ? m->tabbar_tab_offsets[i + 1]
											 : m->tabbar_total_w;
		if (rel_x >= tab_start && rel_x < tab_end) {
			if (m->tabbar_clients[i]) {
				focusclient(m->tabbar_clients[i], 1);
				arrange(m, false, false);
			}
			return true;
		}
	}
	return false;
}

void
tabbed(Monitor *m) {
	Client *c = NULL;
	int n = m->visible_tiling_clients;

	if (n == 0) {
		tabbar_cleanup(m);
		return;
	}

	/* Clean up old tab bar and recreate */
	tabbar_cleanup(m);

	m->tabbar_tree = wlr_scene_tree_create(layers[LyrOverlay]);
	if (!m->tabbar_tree)
		return;

	/* Gaps */
	int32_t cur_gappoh = enablegaps ? m->gappoh : 0;
	int32_t cur_gappov = enablegaps ? m->gappov : 0;
	cur_gappoh = smartgaps && n == 1 ? 0 : cur_gappoh;
	cur_gappov = smartgaps && n == 1 ? 0 : cur_gappov;

	int bw = borderpx;
	int bar_h = tabbar_height;
	int bar_x = m->w.x + cur_gappoh + bw; /* inside border */
	int bar_y = m->w.y + cur_gappov + bw;
	int bar_w = m->w.width - 2 * cur_gappoh - 2 * bw;
	int tab_gap = bw; /* gap between tabs matches border width */
	int total_gaps = (n - 1) * tab_gap;
	int tab_width = (bar_w - total_gaps) / n;
	/* Draw border around tab bar only (window has its own border) */
	int outer_x = m->w.x + cur_gappoh;
	int outer_y = m->w.y + cur_gappov;
	int outer_w = m->w.width - 2 * cur_gappoh;
	int tabbar_border_h = bar_h + bw; /* tab bar + top border only */
	struct wlr_scene_rect *border_rect;

	/* Top border */
	border_rect = wlr_scene_rect_create(m->tabbar_tree, outer_w, bw, focuscolor);
	wlr_scene_node_set_position(&border_rect->node, outer_x, outer_y);
	/* Left border (tab bar height only) */
	border_rect = wlr_scene_rect_create(m->tabbar_tree, bw, tabbar_border_h, focuscolor);
	wlr_scene_node_set_position(&border_rect->node, outer_x, outer_y);
	/* Right border (tab bar height only) */
	border_rect = wlr_scene_rect_create(m->tabbar_tree, bw, tabbar_border_h, focuscolor);
	wlr_scene_node_set_position(&border_rect->node, outer_x + outer_w - bw, outer_y);
	/* No bottom border — window's own top border acts as separator */

	/* Store tab info for mouse click handling */
	m->tabbar_y = bar_y;
	m->tabbar_h = bar_h;
	m->tabbar_x = bar_x;
	m->tabbar_total_w = bar_w;
	m->tabbar_n = n;
	m->tabbar_tab_offsets = ecalloc(n, sizeof(int));
	m->tabbar_clients = ecalloc(n, sizeof(Client *));

	/* Collect clients in reverse order (newest last = rightmost) */
	int i = n;
	wl_list_for_each(c, &clients, link) {
		if (!VISIBLEON(c, m) || !ISTILED(c))
			continue;
		if (i <= 0)
			break;
		m->tabbar_clients[--i] = c;
	}

	for (i = 0; i < n; i++) {
		c = m->tabbar_clients[i];
		bool is_focused = (c == m->sel);
		const float *bg = is_focused ? tabbar_active_bg_color : tabbar_inactive_bg_color;
		const float *fg = is_focused ? tabbar_active_text_color : tabbar_inactive_text_color;

		int tw = (i == n - 1) ? (bar_w - (tab_width + tab_gap) * (n - 1)) : tab_width;
		int tx = i * (tab_width + tab_gap);

		bool round_tl = false;
		bool round_tr = false;

		/* Store for click handling */
		m->tabbar_tab_offsets[i] = tx;

		const char *title = client_get_title(c);
		if (!title)
			title = "untitled";
		struct wlr_buffer *text_buf = tabbar_create_text_buffer(
			title, tw, bar_h, bg, fg, 0, round_tl, round_tr);
		if (text_buf) {
			struct wlr_scene_buffer *sbuf = wlr_scene_buffer_create(m->tabbar_tree, text_buf);
			wlr_scene_node_set_position(&sbuf->node, bar_x + tx, bar_y);
			wlr_buffer_drop(text_buf);
		}

		/* Border separator between tabs */
		if (i < n - 1) {
			struct wlr_scene_rect *sep = wlr_scene_rect_create(
				m->tabbar_tree, bw, bar_h, focuscolor);
			wlr_scene_node_set_position(&sep->node,
				bar_x + tx + tw, bar_y);
		}
	}

	/* Position windows below tab bar, window's own border handles sides */
	struct wlr_box geom;
	geom.x = m->w.x + cur_gappoh;
	geom.y = m->w.y + cur_gappov + bar_h + bw;
	geom.width = m->w.width - 2 * cur_gappoh;
	geom.height = m->w.height - 2 * cur_gappov - bar_h - bw;

	/* Find the focused tiled client (not a floating window) */
	Client *focused = NULL;
	Client *top = focustop(m);
	if (top && ISTILED(top)) {
		focused = top;
	} else {
		/* Focused window is floating — find the last focused tiled client */
		wl_list_for_each(c, &fstack, flink) {
			if (VISIBLEON(c, m) && ISTILED(c)) {
				focused = c;
				break;
			}
		}
	}

	wl_list_for_each(c, &clients, link) {
		if (!VISIBLEON(c, m) || !ISTILED(c))
			continue;
		/* Suppress open animation for tabbed windows */
		c->is_pending_open_animation = false;
		resize(c, geom, 0);
		/* Snap to final position immediately (no move/open animation) */
		if (!c->animation.tagining) {
			c->animation.current = c->geom;
			c->animation.running = false;
			wlr_scene_node_set_position(&c->scene->node, c->geom.x, c->geom.y);
		}
		if (c == focused) {
			wlr_scene_node_set_enabled(&c->scene->node, true);
		} else {
			/* Hide non-focused windows entirely to prevent transparency
			 * bleed-through and overflow */
			wlr_scene_node_set_enabled(&c->scene->node, false);
		}
	}

	/* Raise focused tiled client to top */
	if (focused)
		wlr_scene_node_raise_to_top(&focused->scene->node);

	/* Hide tab bar if focused window is fullscreen/maximized */
	if (focused && (focused->isfullscreen || focused->ismaximizescreen)) {
		wlr_scene_node_set_enabled(&m->tabbar_tree->node, false);
	}

	/* Hide tab bar during tag-in animation so it slides in with the window */
	if (focused && focused->animation.tagining) {
		wlr_scene_node_set_enabled(&m->tabbar_tree->node, false);
	}
}
