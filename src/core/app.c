#include "app.h"
#include "config.h"
#include "net.h"
#include "platform.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FPS 60
#define PING_FRAMES (30 * FPS)
#define DEAD_FRAMES (75 * FPS)
#define LOGIN_FRAMES (60 * FPS)
#define RETRY_FRAMES (15 * FPS)
#define STATUS_FRAMES (3 * FPS)
#define MAX_MSGS 200
#define MAX_BATCH 40
#define MAX_NOTES 3
#define LIST_TOP 3 // first bottom-screen row of the menu area

// ---------------------------------------------------------------- state

enum { KIND_TEXT = 0, KIND_HEADER = 1, KIND_DMS = 2 };

typedef struct {
	char id[24];
	char name[40];
	bool unread;
	int kind;
} Item;

typedef struct {
	Item *v;
	int n, cap, sel, top;
	bool loading;
} List;

enum { F_MENTION = 1, F_EDITED = 2, F_MINE = 4 };

typedef struct {
	char id[24];
	char author[33];
	char time[8];
	int images, flags, lines;
	char *text;
} Msg;

enum { V_OFFLINE, V_LOGIN, V_HOME, V_CHANNELS, V_DMS, V_CHAT };

static struct {
	Config cfg;
	bool have_cfg, wifi, quit;
	int view, back; // back: list view the chat was opened from
	char user[40];
	char err[96];
	char status[64];
	uint32_t status_until;
	char notes[MAX_NOTES][120];
	int nnotes;

	List home, chans, dms;
	char guild_id[24], guild_name[40];

	char chan_id[24], chan_name[40];
	bool chan_dm;
	Msg msgs[MAX_MSGS];
	int nmsgs, sel, scroll;
	Msg batch[MAX_BATCH];
	int nbatch;
	bool hist_loading, hist_older, has_more;

	bool img_wait;
	char img_msg[24];
	int img_idx;

	uint32_t frame, last_rx, last_ping, login_start, retry_at, last_second;
	bool dirty_top, dirty_bot;
} S;

static void dirty(void) { S.dirty_top = S.dirty_bot = true; }

static void cp(char *dst, size_t size, const char *src) { snprintf(dst, size, "%s", src ? src : ""); }

static void set_status(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(S.status, sizeof(S.status), fmt, ap);
	va_end(ap);
	S.status_until = S.frame + STATUS_FRAMES;
	S.dirty_bot = true;
}

// ---------------------------------------------------------------- drawing

// Prints at (row, col) in a colour, clipped so nothing lands in the last
// column (writing there would wrap and scroll the console).
static void put(int scr, int row, int col, int color, const char *fmt, ...)
{
	int w = plat_cols(scr) - 1 - col;
	if (w <= 0 || row < 0 || row >= plat_rows(scr))
		return;
	char buf[160];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (w < (int)sizeof(buf))
		buf[w] = 0;
	for (char *p = buf; *p; p++)
		if ((unsigned char)*p < 32 || (unsigned char)*p > 126)
			*p = '?';
	plat_select(scr);
	plat_goto(row, col);
	printf("\x1b[%dm%s\x1b[%dm", 30 + color, buf, 30 + COL_WHITE);
}

static void hline(int scr, int row)
{
	char buf[64];
	int w = plat_cols(scr) - 1;
	if (w > (int)sizeof(buf) - 1)
		w = sizeof(buf) - 1;
	memset(buf, '-', w);
	buf[w] = 0;
	put(scr, row, 0, COL_BLUE, "%s", buf);
}

// Word-wraps s to width, calling emit for each line. Returns the line count.
typedef void (*EmitFn)(const char *s, int len, void *ctx);
static int wrap(const char *s, int width, EmitFn emit, void *ctx)
{
	int lines = 0;
	if (width < 1)
		width = 1;
	while (*s) {
		const char *nl = strchr(s, '\n');
		int left = nl ? (int)(nl - s) : (int)strlen(s);
		const char *p = s;
		s += left;
		if (*s == '\n')
			s++;
		do {
			int take = left;
			if (take > width) {
				int b = width;
				while (b > 0 && p[b] != ' ')
					b--;
				take = b > 0 ? b : width;
			}
			if (emit)
				emit(p, take, ctx);
			lines++;
			p += take;
			left -= take;
			while (left > 0 && *p == ' ')
				p++, left--;
		} while (left > 0);
	}
	return lines;
}

// ---------------------------------------------------------------- lists

static Item *list_add(List *l)
{
	if (l->n == l->cap) {
		int cap = l->cap ? l->cap * 2 : 32;
		Item *v = realloc(l->v, cap * sizeof(Item));
		if (!v)
			return NULL;
		l->v = v;
		l->cap = cap;
	}
	Item *it = &l->v[l->n++];
	memset(it, 0, sizeof(*it));
	return it;
}

static void list_begin(List *l, bool keep_sel)
{
	l->n = 0;
	l->loading = true;
	if (!keep_sel)
		l->sel = l->top = 0;
}

static void list_move(List *l, int d)
{
	for (int i = l->sel + d; i >= 0 && i < l->n; i += d)
		if (l->v[i].kind != KIND_HEADER) {
			l->sel = i;
			return;
		}
}

// Keeps the selection in range and off category headers.
static void list_fix(List *l)
{
	if (l->sel >= l->n)
		l->sel = l->n - 1;
	if (l->sel < 0)
		l->sel = 0;
	if (l->n && l->v[l->sel].kind == KIND_HEADER) {
		int s = l->sel;
		list_move(l, 1);
		if (l->sel == s)
			list_move(l, -1);
	}
}

static void draw_list(List *l, int r0, int r1)
{
	int rows = r1 - r0 + 1;
	if (l->sel < l->top)
		l->top = l->sel;
	if (l->sel >= l->top + rows)
		l->top = l->sel - rows + 1;
	if (l->top < 0)
		l->top = 0;
	if (!l->n) {
		put(SCR_BOTTOM, r0, 1, COL_WHITE, "%s", l->loading ? "Loading..." : "(nothing here)");
		return;
	}
	for (int i = 0; i < rows && l->top + i < l->n; i++) {
		int idx = l->top + i;
		Item *it = &l->v[idx];
		if (it->kind == KIND_HEADER)
			put(SCR_BOTTOM, r0 + i, 1, COL_MAGENTA, "%s", it->name);
		else if (idx == l->sel)
			put(SCR_BOTTOM, r0 + i, 0, COL_YELLOW, ">%c%s", it->unread ? '*' : ' ', it->name);
		else
			put(SCR_BOTTOM, r0 + i, 0, it->unread ? COL_WHITE : COL_CYAN, " %c%s", it->unread ? '*' : ' ', it->name);
	}
}

// ---------------------------------------------------------------- messages

static int text_width(void) { return plat_cols(SCR_TOP) - 2; } // 1 indent + spare last column
static int chat_rows(void) { return plat_rows(SCR_TOP); }

static void msg_free(Msg *m)
{
	free(m->text);
	m->text = NULL;
}

static const char *msg_text(const Msg *m) { return m->text ? m->text : ""; }

// Fills m from the fields of an M / N / U line.
static bool msg_fill(Msg *m, char **f, int n)
{
	if (n < 8)
		return false;
	cp(m->id, sizeof(m->id), f[2]);
	cp(m->author, sizeof(m->author), f[3]);
	cp(m->time, sizeof(m->time), f[4]);
	m->images = atoi(f[5]);
	m->flags = atoi(f[6]);
	m->text = strdup(f[7]);
	m->lines = 1 + wrap(msg_text(m), text_width(), NULL, NULL);
	return true;
}

// Snowflakes compare as numbers: longer is newer, then lexically.
static int id_cmp(const char *a, const char *b)
{
	size_t la = strlen(a), lb = strlen(b);
	if (la != lb)
		return la < lb ? -1 : 1;
	return strcmp(a, b);
}

static int find_msg(const char *id)
{
	for (int i = S.nmsgs - 1; i >= 0; i--)
		if (!strcmp(S.msgs[i].id, id))
			return i;
	return -1;
}

static void clear_msgs(void)
{
	for (int i = 0; i < S.nmsgs; i++)
		msg_free(&S.msgs[i]);
	for (int i = 0; i < S.nbatch; i++)
		msg_free(&S.batch[i]);
	S.nmsgs = S.nbatch = S.sel = S.scroll = 0;
}

static int line_of(int idx)
{
	int l = 0;
	for (int i = 0; i < idx; i++)
		l += S.msgs[i].lines;
	return l;
}

static int total_lines(void) { return line_of(S.nmsgs); }

static int msg_at_line(int line)
{
	int l = 0;
	for (int i = 0; i < S.nmsgs; i++) {
		l += S.msgs[i].lines;
		if (line < l)
			return i;
	}
	return S.nmsgs - 1;
}

static void clamp_scroll(void)
{
	int max = total_lines() - chat_rows();
	if (S.scroll > max)
		S.scroll = max;
	if (S.scroll < 0)
		S.scroll = 0;
}

static void clamp_sel(void)
{
	if (S.sel >= S.nmsgs)
		S.sel = S.nmsgs - 1;
	if (S.sel < 0)
		S.sel = 0;
}

static void ensure_visible(void)
{
	clamp_sel();
	if (S.nmsgs) {
		int s = line_of(S.sel), e = s + S.msgs[S.sel].lines, h = chat_rows();
		if (s < S.scroll || e - s > h)
			S.scroll = s;
		else if (e > S.scroll + h)
			S.scroll = e - h;
	}
	clamp_scroll();
	S.dirty_top = S.dirty_bot = true;
}

static void page(int dir)
{
	int h = chat_rows(), total = total_lines();
	if (!S.nmsgs)
		return;
	S.scroll += dir * (h - 1);
	clamp_scroll();
	int last = S.scroll + h - 1;
	if (last >= total)
		last = total - 1;
	S.sel = msg_at_line(dir < 0 ? S.scroll : last);
	S.dirty_top = S.dirty_bot = true;
}

static void append_msg(Msg *m)
{
	bool follow = S.nmsgs == 0 || S.sel >= S.nmsgs - 1;
	if (S.nmsgs == MAX_MSGS) {
		S.scroll -= S.msgs[0].lines;
		msg_free(&S.msgs[0]);
		memmove(S.msgs, S.msgs + 1, (S.nmsgs - 1) * sizeof(Msg));
		S.nmsgs--;
		S.sel--;
	}
	S.msgs[S.nmsgs++] = *m;
	if (follow)
		S.sel = S.nmsgs - 1;
	ensure_visible();
	if (!follow)
		clamp_scroll();
}

// Sorts the history batch by id and drops duplicates (live messages can
// arrive while the history request is in flight).
static void tidy_batch(void)
{
	for (int i = 1; i < S.nbatch; i++) {
		Msg t = S.batch[i];
		int j = i - 1;
		while (j >= 0 && id_cmp(S.batch[j].id, t.id) > 0) {
			S.batch[j + 1] = S.batch[j];
			j--;
		}
		S.batch[j + 1] = t;
	}
	int o = 0;
	for (int i = 0; i < S.nbatch; i++) {
		if ((o && !strcmp(S.batch[o - 1].id, S.batch[i].id)) || find_msg(S.batch[i].id) >= 0)
			msg_free(&S.batch[i]);
		else
			S.batch[o++] = S.batch[i];
	}
	S.nbatch = o;
}

static void finish_history(bool has_more)
{
	tidy_batch();
	S.hist_loading = false;
	S.has_more = has_more;
	int k = S.nbatch;
	if (S.hist_older) {
		int drop = S.nmsgs + k - MAX_MSGS;
		if (drop > 0) { // keep the newest of the older batch
			for (int i = 0; i < drop; i++)
				msg_free(&S.batch[i]);
			memmove(S.batch, S.batch + drop, (k - drop) * sizeof(Msg));
			k -= drop;
			if (k == 0)
				set_status("History limit reached");
		}
		memmove(S.msgs + k, S.msgs, S.nmsgs * sizeof(Msg));
		int added = 0;
		for (int i = 0; i < k; i++) {
			S.msgs[i] = S.batch[i];
			added += S.batch[i].lines;
		}
		S.nmsgs += k;
		S.scroll += added; // keep the view where it was...
		if (k) // ...then select the newest of the older messages
			S.sel = k - 1;
		ensure_visible();
	} else {
		for (int i = 0; i < k && S.nmsgs < MAX_MSGS; i++)
			S.msgs[S.nmsgs++] = S.batch[i];
		S.sel = S.nmsgs - 1;
		S.scroll = total_lines();
		ensure_visible();
	}
	S.nbatch = 0;
}

static void open_chat(const char *id, const char *name, bool dm)
{
	clear_msgs();
	cp(S.chan_id, sizeof(S.chan_id), id);
	cp(S.chan_name, sizeof(S.chan_name), name);
	S.chan_dm = dm;
	S.back = S.view;
	S.view = V_CHAT;
	S.hist_loading = true;
	S.hist_older = S.has_more = false;
	net_send("HIST", id, "0", NULL);
	dirty();
}

static void load_older(void)
{
	if (S.hist_loading || !S.has_more || !S.nmsgs)
		return;
	S.hist_loading = S.hist_older = true;
	S.nbatch = 0;
	net_send("HIST", S.chan_id, S.msgs[0].id, NULL);
	S.dirty_bot = true;
}

// ---------------------------------------------------------------- chat drawing

typedef struct {
	int line, color;
} DrawCtx;

static void emit_draw(const char *s, int len, void *c)
{
	DrawCtx *d = c;
	int row = d->line - S.scroll;
	if (row >= 0 && row < chat_rows())
		put(SCR_TOP, row, 1, d->color, "%.*s", len, s);
	d->line++;
}

static void draw_chat_top(void)
{
	int h = chat_rows();
	if (!S.nmsgs) {
		put(SCR_TOP, 1, 1, COL_WHITE, "%s", S.hist_loading ? "Loading messages..." : "No messages yet.");
		if (!S.hist_loading)
			put(SCR_TOP, 2, 1, COL_WHITE, "Press X to write one.");
		return;
	}
	int line = 0;
	for (int i = 0; i < S.nmsgs && line < S.scroll + h; i++) {
		Msg *m = &S.msgs[i];
		if (line + m->lines <= S.scroll) {
			line += m->lines;
			continue;
		}
		bool sel = i == S.sel;
		int color = sel ? COL_YELLOW : m->flags & F_MENTION ? COL_RED : m->flags & F_MINE ? COL_GREEN : COL_CYAN;
		int row = line - S.scroll;
		if (row >= 0)
			put(SCR_TOP, row, 0, color, "%c%s %s%s%s", sel ? '>' : ' ', m->author, m->time,
			    m->flags & F_EDITED ? " (edited)" : "", m->images ? " [img]" : "");
		DrawCtx d = { line + 1, COL_WHITE };
		wrap(msg_text(m), text_width(), emit_draw, &d);
		line += m->lines;
	}
}

static void draw_top(void)
{
	plat_select(SCR_TOP);
	plat_clear();
	if (S.view == V_CHAT) {
		draw_chat_top();
		return;
	}
	put(SCR_TOP, 1, 1, COL_CYAN, "DScord");
	put(SCR_TOP, 2, 1, COL_WHITE, "Discord for DS / DSi / 3DS");
	hline(SCR_TOP, 3);
	if (S.view >= V_HOME)
		put(SCR_TOP, 5, 1, COL_GREEN, "Online as %s", S.user);
	else
		put(SCR_TOP, 5, 1, COL_YELLOW, "Not connected");
	if (S.have_cfg)
		put(SCR_TOP, 6, 1, COL_WHITE, "Relay %s:%d", S.cfg.host, S.cfg.port);
	int r = 8;
	static const char *tips[] = {
		"Pick a server or Direct",
		"Messages on the bottom screen.",
		"",
		"In a chat: Up/Down selects a",
		"message, L/R pages, Y loads",
		"older ones, X writes.",
	};
	for (unsigned i = 0; i < sizeof(tips) / sizeof(*tips); i++)
		put(SCR_TOP, r++, 1, COL_WHITE, "%s", tips[i]);
	if (plat_has_images())
		put(SCR_TOP, r++, 1, COL_WHITE, "A on a message shows images.");
}

static const char *default_status(void)
{
	static char buf[64];
	switch (S.view) {
	case V_OFFLINE: return "Offline";
	case V_LOGIN: return "Logging in...";
	default: break;
	}
	if (S.img_wait)
		return "Loading image...";
	if (S.hist_loading || (S.view == V_HOME && S.home.loading) || (S.view == V_CHANNELS && S.chans.loading) ||
	    (S.view == V_DMS && S.dms.loading))
		return "Loading...";
	snprintf(buf, sizeof(buf), "Online as %s", S.user);
	return buf;
}

static void draw_bottom(void)
{
	int rows = plat_rows(SCR_BOTTOM), w = plat_cols(SCR_BOTTOM) - 1;
	int list_end = rows - 7, note_sep = rows - 6, help = rows - 2;
	plat_select(SCR_BOTTOM);
	plat_clear();

	const char *help1 = "", *help2 = "";
	switch (S.view) {
	case V_OFFLINE: {
		put(SCR_BOTTOM, 0, 0, COL_CYAN, "Not connected");
		int r = LIST_TOP;
		char tmp[sizeof(S.err)];
		cp(tmp, sizeof(tmp), S.err[0] ? S.err : "Not connected yet.");
		// error text, wrapped by hand into the menu area
		for (const char *p = tmp; *p && r <= list_end - 4; r++) {
			int len = (int)strlen(p);
			int take = len > w ? w : len;
			if (take < len)
				for (int b = take; b > 0; b--)
					if (p[b] == ' ') {
						take = b;
						break;
					}
			put(SCR_BOTTOM, r, 0, COL_RED, "%.*s", take, p);
			p += take;
			while (*p == ' ')
				p++;
		}
		r++;
		if (S.have_cfg)
			put(SCR_BOTTOM, r++, 0, COL_WHITE, "Relay %s:%d", S.cfg.host, S.cfg.port);
		else
			put(SCR_BOTTOM, r++, 0, COL_WHITE, "No relay set up yet.");
		if (S.retry_at) {
			int secs = ((int)(S.retry_at - S.frame) + FPS - 1) / FPS;
			put(SCR_BOTTOM, r++, 0, COL_WHITE, "Retrying in %ds", secs < 0 ? 0 : secs);
		}
		help1 = "A:retry  X:settings";
		help2 = "START:quit";
		break;
	}
	case V_LOGIN:
		put(SCR_BOTTOM, 0, 0, COL_CYAN, "Connecting");
		put(SCR_BOTTOM, LIST_TOP, 0, COL_WHITE, "Relay %s:%d", S.cfg.host, S.cfg.port);
		put(SCR_BOTTOM, LIST_TOP + 1, 0, COL_WHITE, "Waiting for the relay to log");
		put(SCR_BOTTOM, LIST_TOP + 2, 0, COL_WHITE, "in to Discord...");
		help1 = "B:cancel";
		break;
	case V_HOME:
		put(SCR_BOTTOM, 0, 0, COL_CYAN, "Servers (%d)", S.home.n > 0 ? S.home.n - 1 : 0);
		draw_list(&S.home, LIST_TOP, list_end);
		help1 = "A:open  Y:refresh  X:settings";
		help2 = "START:quit";
		break;
	case V_CHANNELS:
		put(SCR_BOTTOM, 0, 0, COL_CYAN, "%s", S.guild_name);
		draw_list(&S.chans, LIST_TOP, list_end);
		help1 = "A:open  B:back  Y:refresh";
		help2 = "L/R:page";
		break;
	case V_DMS:
		put(SCR_BOTTOM, 0, 0, COL_CYAN, "Direct Messages");
		draw_list(&S.dms, LIST_TOP, list_end);
		help1 = "A:open  B:back  Y:refresh";
		help2 = "L/R:page";
		break;
	case V_CHAT: {
		put(SCR_BOTTOM, 0, 0, COL_CYAN, "%c%s", S.chan_dm ? '@' : '#', S.chan_name);
		int r = LIST_TOP;
		if (!S.chan_dm)
			put(SCR_BOTTOM, r++, 0, COL_WHITE, "in %s", S.guild_name);
		r++;
		if (S.nmsgs) {
			Msg *m = &S.msgs[S.sel];
			put(SCR_BOTTOM, r++, 0, COL_WHITE, "Message %d of %d", S.sel + 1, S.nmsgs);
			put(SCR_BOTTOM, r++, 0, COL_YELLOW, "%s at %s", m->author, m->time);
			if (m->flags & F_EDITED)
				put(SCR_BOTTOM, r++, 0, COL_WHITE, "(edited)");
			if (m->images) {
				if (plat_has_images())
					put(SCR_BOTTOM, r++, 0, COL_GREEN, "%d image%s - A to view", m->images, m->images > 1 ? "s" : "");
				else
					put(SCR_BOTTOM, r++, 0, COL_WHITE, "%d image%s (3DS only)", m->images, m->images > 1 ? "s" : "");
			}
		}
		r++;
		if (S.hist_loading)
			put(SCR_BOTTOM, r++, 0, COL_WHITE, "Loading messages...");
		else if (S.has_more)
			put(SCR_BOTTOM, r++, 0, COL_WHITE, "Y: load older messages");
		else if (S.nmsgs)
			put(SCR_BOTTOM, r++, 0, COL_WHITE, "Start of the conversation");
		help1 = "X:write  Y:older  B:back";
		help2 = plat_has_images() ? "Up/Dn:msg L/R:page A:image" : "Up/Dn:message  L/R:page";
		break;
	}
	}

	bool timed = S.status[0] && (int32_t)(S.status_until - S.frame) > 0;
	put(SCR_BOTTOM, 1, 0, COL_YELLOW, "%s", timed ? S.status : default_status());
	hline(SCR_BOTTOM, 2);

	hline(SCR_BOTTOM, note_sep);
	for (int i = 0; i < S.nnotes; i++)
		put(SCR_BOTTOM, note_sep + 1 + i, 0, COL_MAGENTA, "%s", S.notes[i]);
	put(SCR_BOTTOM, help, 0, COL_GREEN, "%s", help1);
	put(SCR_BOTTOM, help + 1, 0, COL_GREEN, "%s", help2);
}

static void redraw(void)
{
	if (S.dirty_top)
		draw_top();
	if (S.dirty_bot)
		draw_bottom();
	S.dirty_top = S.dirty_bot = false;
}

// ---------------------------------------------------------------- connection

static void go_offline(const char *why, bool retry)
{
	net_close();
	cp(S.err, sizeof(S.err), why);
	S.retry_at = retry ? S.frame + RETRY_FRAMES : 0;
	if (retry && !S.retry_at)
		S.retry_at = 1; // 0 means no retry
	S.view = V_OFFLINE;
	S.status_until = S.frame; // drop any "Connecting..." message
	S.img_wait = S.hist_loading = false;
	S.home.loading = S.chans.loading = S.dms.loading = false;
	dirty();
}

static bool edit_settings(void)
{
	Config c = S.cfg;
	bool ok = config_prompt(&c);
	dirty();
	if (!ok)
		return false;
	S.cfg = c;
	S.have_cfg = true;
	if (!config_save(&S.cfg))
		set_status("Couldn't save settings");
	return true;
}

static void show_progress(const char *msg)
{
	S.view = V_LOGIN;
	set_status("%s", msg);
	dirty();
	redraw();
	plat_select(SCR_BOTTOM);
}

static void do_connect(void)
{
	char err[96];
	if (!S.have_cfg && !edit_settings()) {
		go_offline("Set the relay address, port and PIN with X.", false);
		return;
	}
	if (!S.wifi) {
		show_progress("Starting Wi-Fi...");
		plat_select(SCR_BOTTOM);
		plat_clear();
		if (!plat_net_init(err, sizeof(err))) {
			go_offline(err, true);
			return;
		}
		S.wifi = true;
	}
	show_progress("Connecting to the relay...");
	if (!net_connect(S.cfg.host, S.cfg.port, err, sizeof(err))) {
		go_offline(err, true);
		return;
	}
	char plat[4];
	snprintf(plat, sizeof(plat), "%d", plat_id());
	net_send("HELLO", "1", plat, S.cfg.pin, NULL);
	S.view = V_LOGIN;
	S.login_start = S.last_rx = S.last_ping = S.frame;
	S.status_until = S.frame;
	dirty();
}

static void request_home(bool keep)
{
	list_begin(&S.home, keep);
	Item *it = list_add(&S.home);
	if (it) {
		cp(it->name, sizeof(it->name), "Direct Messages");
		it->kind = KIND_DMS;
	}
	net_send("GUILDS", NULL);
	S.dirty_bot = true;
}

static void request_channels(bool keep)
{
	list_begin(&S.chans, keep);
	net_send("CHANNELS", S.guild_id, NULL);
	S.dirty_bot = true;
}

static void request_dms(bool keep)
{
	list_begin(&S.dms, keep);
	net_send("DMS", NULL);
	S.dirty_bot = true;
}

static void request_image(const char *id, int idx)
{
	char si[12], sw[12], sh[12];
	snprintf(si, sizeof(si), "%d", idx);
	snprintf(sw, sizeof(sw), "%d", plat_image_w());
	snprintf(sh, sizeof(sh), "%d", plat_image_h());
	S.img_wait = true;
	cp(S.img_msg, sizeof(S.img_msg), id);
	S.img_idx = idx;
	net_send("IMG", id, si, sw, sh, NULL);
	S.dirty_bot = true;
}

// ---------------------------------------------------------------- protocol

static void add_item(List *l, char **f, int n, int kind)
{
	if (n < 3)
		return;
	Item *it = list_add(l);
	if (!it)
		return;
	cp(it->id, sizeof(it->id), f[1]);
	cp(it->name, sizeof(it->name), f[2]);
	it->unread = n > 3 && atoi(f[3]);
	it->kind = kind;
}

static void on_line(char **f, int n)
{
	const char *c = f[0];
	S.last_rx = S.frame;

	if (!strcmp(c, "OK")) {
		cp(S.user, sizeof(S.user), n > 1 ? f[1] : "?");
		S.view = V_HOME;
		S.err[0] = 0;
		request_home(true);
		dirty();
	} else if (!strcmp(c, "ERR")) {
		const char *msg = n > 1 ? f[1] : "error";
		if (S.view == V_LOGIN) {
			go_offline(msg, false);
			return;
		}
		S.img_wait = false;
		if (S.hist_loading) {
			S.hist_loading = false;
			for (int i = 0; i < S.nbatch; i++)
				msg_free(&S.batch[i]);
			S.nbatch = 0;
		}
		S.home.loading = S.chans.loading = S.dms.loading = false;
		set_status("Error: %s", msg);
		dirty();
	} else if (!strcmp(c, "G")) {
		add_item(&S.home, f, n, KIND_TEXT);
		S.dirty_bot = S.view == V_HOME;
	} else if (!strcmp(c, "C")) {
		add_item(&S.chans, f, n, n > 4 && atoi(f[4]) == 1 ? KIND_HEADER : KIND_TEXT);
		S.dirty_bot = S.view == V_CHANNELS;
	} else if (!strcmp(c, "D")) {
		add_item(&S.dms, f, n, KIND_TEXT);
		S.dirty_bot = S.view == V_DMS;
	} else if (!strcmp(c, "END") && n > 1) {
		List *l = !strcmp(f[1], "G") ? &S.home : !strcmp(f[1], "C") ? &S.chans : !strcmp(f[1], "D") ? &S.dms : NULL;
		if (l) {
			l->loading = false;
			list_fix(l);
			S.dirty_bot = true;
		} else if (!strcmp(f[1], "M") && n > 3 && !strcmp(f[2], S.chan_id) && S.hist_loading) {
			finish_history(atoi(f[3]) != 0);
		}
	} else if (!strcmp(c, "M")) {
		if (n > 1 && S.hist_loading && !strcmp(f[1], S.chan_id) && S.nbatch < MAX_BATCH)
			if (msg_fill(&S.batch[S.nbatch], f, n))
				S.nbatch++;
	} else if (!strcmp(c, "N") || !strcmp(c, "U")) {
		if (n < 8 || strcmp(f[1], S.chan_id) || S.view != V_CHAT)
			return;
		Msg m;
		if (!msg_fill(&m, f, n))
			return;
		int i = find_msg(m.id);
		if (i >= 0) {
			bool at_top = S.scroll <= line_of(i);
			msg_free(&S.msgs[i]);
			S.msgs[i] = m;
			if (!at_top)
				clamp_scroll();
			S.dirty_top = S.dirty_bot = true;
		} else if (c[0] == 'N') {
			if (S.hist_loading && !S.hist_older) {
				if (S.nbatch < MAX_BATCH)
					S.batch[S.nbatch++] = m;
				else
					msg_free(&m);
			} else {
				append_msg(&m);
			}
		} else {
			msg_free(&m);
		}
	} else if (!strcmp(c, "X")) {
		if (n < 3 || strcmp(f[1], S.chan_id))
			return;
		int i = find_msg(f[2]);
		if (i < 0)
			return;
		msg_free(&S.msgs[i]);
		memmove(S.msgs + i, S.msgs + i + 1, (S.nmsgs - i - 1) * sizeof(Msg));
		S.nmsgs--;
		if (S.sel > i)
			S.sel--;
		clamp_sel();
		clamp_scroll();
		S.dirty_top = S.dirty_bot = true;
	} else if (!strcmp(c, "NOTE") && n > 1) {
		memmove(S.notes[1], S.notes[0], (MAX_NOTES - 1) * sizeof(S.notes[0]));
		cp(S.notes[0], sizeof(S.notes[0]), f[1]);
		if (S.nnotes < MAX_NOTES)
			S.nnotes++;
		S.dirty_bot = true;
	} else if (!strcmp(c, "SENT")) {
		set_status("Sent");
	}
}

static void on_blob(char **f, int n, uint8_t *data, size_t len)
{
	S.last_rx = S.frame;
	if (n < 4 || !S.img_wait || strcmp(f[1], S.img_msg) || atoi(f[2]) != S.img_idx)
		return;
	S.img_wait = false;
	int count = atoi(f[3]);
	int i = find_msg(S.img_msg);
	char caption[96];
	snprintf(caption, sizeof(caption), "%s: image %d of %d", i >= 0 ? S.msgs[i].author : "?", S.img_idx + 1, count);
	uint32_t k = plat_show_image(data, len, caption);
	dirty();
	if (count > 1 && (k & (K_L | K_LEFT)))
		request_image(S.img_msg, (S.img_idx + count - 1) % count);
	else if (count > 1 && (k & (K_R | K_RIGHT)))
		request_image(S.img_msg, (S.img_idx + 1) % count);
}

static const NetHandlers handlers = { on_line, on_blob };

// ---------------------------------------------------------------- input

static void list_keys(List *l, uint32_t k)
{
	int before = l->sel;
	if (k & K_UP)
		list_move(l, -1);
	if (k & K_DOWN)
		list_move(l, 1);
	for (int i = 0; i < 10; i++) {
		if (k & (K_L | K_LEFT))
			list_move(l, -1);
		if (k & (K_R | K_RIGHT))
			list_move(l, 1);
	}
	if (l->sel != before)
		S.dirty_bot = true;
}

static void compose(void)
{
	static char text[512];
	char hint[64];
	text[0] = 0;
	snprintf(hint, sizeof(hint), "Message %c%s", S.chan_dm ? '@' : '#', S.chan_name);
	bool ok = plat_text_input(hint, "", text, sizeof(text), false);
	dirty();
	if (!ok || !text[0])
		return;
	if (net_send("SEND", S.chan_id, text, NULL))
		set_status("Sending...");
}

static void handle_keys(uint32_t k)
{
	if (!k)
		return;
	switch (S.view) {
	case V_OFFLINE:
		if (k & K_A)
			do_connect();
		else if (k & K_X) {
			if (edit_settings())
				do_connect();
		} else if (k & K_START)
			S.quit = true;
		break;
	case V_LOGIN:
		if (k & K_B)
			go_offline("Cancelled.", false);
		break;
	case V_HOME:
		list_keys(&S.home, k);
		if (k & K_A && S.home.n) {
			Item *it = &S.home.v[S.home.sel];
			if (it->kind == KIND_DMS) {
				S.view = V_DMS;
				request_dms(false);
			} else {
				cp(S.guild_id, sizeof(S.guild_id), it->id);
				cp(S.guild_name, sizeof(S.guild_name), it->name);
				S.view = V_CHANNELS;
				request_channels(false);
			}
			dirty();
		} else if (k & K_Y)
			request_home(true);
		else if (k & K_X) {
			net_close();
			edit_settings();
			do_connect(); // with the new settings, or the old ones if cancelled
		} else if (k & K_START)
			S.quit = true;
		break;
	case V_CHANNELS:
	case V_DMS: {
		List *l = S.view == V_DMS ? &S.dms : &S.chans;
		list_keys(l, k);
		if (k & K_A && l->n && l->v[l->sel].kind != KIND_HEADER)
			open_chat(l->v[l->sel].id, l->v[l->sel].name, S.view == V_DMS);
		else if (k & K_B) {
			S.view = V_HOME;
			request_home(true);
			dirty();
		} else if (k & K_Y)
			S.view == V_DMS ? request_dms(true) : request_channels(true);
		break;
	}
	case V_CHAT:
		if (S.img_wait) {
			if (k & K_B) {
				S.img_wait = false;
				S.dirty_bot = true;
			}
			break;
		}
		if (k & K_UP && S.sel > 0) {
			S.sel--;
			ensure_visible();
		}
		if (k & K_DOWN && S.sel < S.nmsgs - 1) {
			S.sel++;
			ensure_visible();
		}
		if (k & (K_L | K_LEFT))
			page(-1);
		if (k & (K_R | K_RIGHT))
			page(1);
		if (k & K_Y)
			load_older();
		if (k & K_X)
			compose();
		else if (k & K_A && plat_has_images() && S.nmsgs && S.msgs[S.sel].images)
			request_image(S.msgs[S.sel].id, 0);
		else if (k & K_B) {
			clear_msgs();
			S.chan_id[0] = 0;
			S.hist_loading = false;
			S.view = S.back;
			S.view == V_DMS ? request_dms(true) : request_channels(true);
			dirty();
		}
		break;
	}
}

// ---------------------------------------------------------------- main loop

static void tick(void)
{
	if (net_connected()) {
		if (!net_poll(&handlers)) {
			if (S.view != V_OFFLINE) // an ERR may already have said why
				go_offline("Connection to the relay was lost.", true);
			return;
		}
		if (S.view == V_LOGIN && S.frame - S.login_start > LOGIN_FRAMES) {
			go_offline("The relay didn't answer the login.", true);
			return;
		}
		if (S.view >= V_HOME) {
			if (S.frame - S.last_ping > PING_FRAMES) {
				S.last_ping = S.frame;
				net_send("PING", NULL);
			}
			if (S.frame - S.last_rx > DEAD_FRAMES) {
				go_offline("The relay stopped responding.", true);
				return;
			}
		}
	} else if (S.view != V_OFFLINE) {
		go_offline("Connection to the relay was lost.", true);
		return;
	}

	if (S.view == V_OFFLINE && S.retry_at) {
		if ((int32_t)(S.frame - S.retry_at) >= 0) {
			S.retry_at = 0;
			do_connect();
		} else if (S.frame / FPS != S.last_second) {
			S.dirty_bot = true;
		}
	}
	if (S.status[0] && S.status_until == S.frame)
		S.dirty_bot = true; // status message just expired
	S.last_second = S.frame / FPS;
}

int app_run(void)
{
	if (!plat_init())
		return 1;
	memset(&S, 0, sizeof(S));
	S.cfg.port = 5151;
	S.have_cfg = config_load(&S.cfg);
	S.view = V_OFFLINE;
	dirty();
	redraw();
	do_connect();

	while (!S.quit && plat_running()) {
		plat_frame();
		S.frame++;
		tick();
		handle_keys(plat_keys_down());
		redraw();
	}
	net_close();
	clear_msgs();
	plat_exit();
	return 0;
}
