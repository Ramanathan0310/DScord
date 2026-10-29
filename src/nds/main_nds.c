// Nintendo DS / DSi platform layer: libnds consoles, keyboard, dswifi, libfat.
#include "app.h"
#include "platform.h"

#include <dswifi9.h>
#include <fat.h>
#include <nds.h>
#include <stdio.h>
#include <string.h>

static PrintConsole top, bottom;
static PrintConsole *cur = &top;
static uint32_t down;
static bool wifi_started;

bool plat_init(void)
{
	videoSetMode(MODE_0_2D);
	videoSetModeSub(MODE_0_2D);
	vramSetBankA(VRAM_A_MAIN_BG);
	vramSetBankC(VRAM_C_SUB_BG);
	consoleInit(&top, 0, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);
	// Same layout as consoleDemoInit, so keyboardDemoInit (layer 3, map 20,
	// tiles 0) fits next to it on the sub screen.
	consoleInit(&bottom, 0, BgType_Text4bpp, BgSize_T_256x256, 22, 3, false, true);
	keyboardDemoInit();
	keyboardHide();
	keysSetRepeat(20, 5);
	consoleSelect(&bottom);
	fatInitDefault(); // no card / SD access just means settings aren't saved
	return true;
}

void plat_exit(void) {}

bool plat_running(void) { return true; }

void plat_frame(void)
{
	swiWaitForVBlank();
	scanKeys();
	uint32_t raw = keysDown() | (keysDownRepeat() & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT));
	static const struct {
		uint32_t nds, ours;
	} map[] = {
		{ KEY_A, K_A }, { KEY_B, K_B }, { KEY_X, K_X }, { KEY_Y, K_Y }, { KEY_L, K_L }, { KEY_R, K_R },
		{ KEY_UP, K_UP }, { KEY_DOWN, K_DOWN }, { KEY_LEFT, K_LEFT }, { KEY_RIGHT, K_RIGHT },
		{ KEY_START, K_START }, { KEY_SELECT, K_SELECT },
	};
	down = 0;
	for (unsigned i = 0; i < sizeof(map) / sizeof(*map); i++)
		if (raw & map[i].nds)
			down |= map[i].ours;
}

uint32_t plat_keys_down(void) { return down; }

int plat_id(void) { return isDSiMode() ? PLAT_DSI : PLAT_DS; }

void plat_select(int screen)
{
	cur = screen == SCR_TOP ? &top : &bottom;
	consoleSelect(cur);
}

int plat_cols(int screen) { return (screen == SCR_TOP ? &top : &bottom)->windowWidth; }
int plat_rows(int screen) { return (screen == SCR_TOP ? &top : &bottom)->windowHeight; }
void plat_clear(void) { consoleClear(); }

void plat_goto(int row, int col)
{
	cur->cursorY = row;
	cur->cursorX = col;
}

bool plat_net_init(char *err, size_t errlen)
{
	iprintf("Connecting to Wi-Fi using the\nsaved WFC settings...\n");
	bool ok;
	if (!wifi_started) {
		wifi_started = true;
		ok = Wifi_InitDefault(WFC_CONNECT);
	} else {
		Wifi_AutoConnect();
		int st = ASSOCSTATUS_DISCONNECTED;
		for (int i = 0; i < 60 * 30; i++) {
			swiWaitForVBlank();
			st = Wifi_AssocStatus();
			if (st == ASSOCSTATUS_ASSOCIATED || st == ASSOCSTATUS_CANNOTCONNECT)
				break;
		}
		ok = st == ASSOCSTATUS_ASSOCIATED;
	}
	if (!ok)
		snprintf(err, errlen, "No Wi-Fi. Set up a connection in a DS game's WFC settings (open or WEP only).");
	return ok;
}

// Draws the prompt above the keyboard, which covers the lower half.
static void draw_input(const char *hint, const char *text, size_t len)
{
	const int w = 31, rows = 7;
	consoleSelect(&bottom);
	consoleClear();
	char line[32];
	snprintf(line, sizeof(line), "%s", hint);
	iprintf("\x1b[36m%s\x1b[37m", line);
	bottom.cursorY = 1;
	bottom.cursorX = 0;
	iprintf("\x1b[32mSTART:done  B:cancel\x1b[37m");
	// show the tail of the text with a cursor
	size_t shown = len + 1, cap = (size_t)(w * rows);
	size_t start = shown > cap ? shown - cap : 0;
	start -= start % w;
	for (int r = 0; r < rows && start <= len; r++, start += w) {
		bottom.cursorY = 2 + r;
		bottom.cursorX = 0;
		size_t n = len - start < (size_t)w ? len - start : (size_t)w;
		snprintf(line, sizeof(line), "%.*s%s", (int)n, text + start, n < (size_t)w ? "_" : "");
		iprintf("%s", line);
	}
}

bool plat_text_input(const char *hint, const char *initial, char *out, size_t max, bool numeric)
{
	char buf[512];
	size_t cap = max < sizeof(buf) ? max : sizeof(buf);
	snprintf(buf, cap, "%s", initial); // copy first: initial may be out itself
	size_t len = strlen(buf);
	bool ok = false, redraw = true;
	keyboardShow();
	for (;;) {
		if (redraw)
			draw_input(hint, buf, len);
		redraw = false;
		swiWaitForVBlank();
		scanKeys();
		int c = keyboardUpdate();
		uint32_t kd = keysDown();
		if (kd & KEY_B)
			break;
		if (c == DVK_ENTER || (kd & KEY_START)) {
			ok = true;
			break;
		}
		if (c == DVK_BACKSPACE && len) {
			buf[--len] = 0;
			redraw = true;
		} else if (c >= 32 && c < 127 && len + 1 < cap && (!numeric || (c >= '0' && c <= '9'))) {
			buf[len++] = (char)c;
			buf[len] = 0;
			redraw = true;
		}
	}
	keyboardHide();
	consoleSelect(&bottom);
	consoleClear();
	if (ok)
		memcpy(out, buf, len + 1);
	return ok;
}

const char *plat_config_path(void) { return "/dscord.cfg"; }

bool plat_has_images(void) { return false; }
int plat_image_w(void) { return 256; }
int plat_image_h(void) { return 192; }
uint32_t plat_show_image(const uint8_t *data, size_t len, const char *caption)
{
	(void)data, (void)len, (void)caption;
	return 0;
}

int main(void) { return app_run(); }
