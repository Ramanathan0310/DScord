// Nintendo 3DS platform layer: libctru consoles, swkbd, soc, stb_image viewer.
#include "app.h"
#include "platform.h"

#include <3ds.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stb_image.h"

#define SOC_SIZE 0x100000
#define TOP_W 400
#define TOP_H 240

static PrintConsole top, bottom;
static PrintConsole *cur = &top;
static uint32_t down;
static u32 *soc_buf;

bool plat_init(void)
{
	gfxInitDefault();
	consoleInit(GFX_TOP, &top);
	consoleInit(GFX_BOTTOM, &bottom);
	hidSetRepeatParameters(20, 5);
	consoleSelect(&bottom);
	return true;
}

void plat_exit(void)
{
	if (soc_buf) {
		socExit();
		free(soc_buf);
	}
	gfxExit();
}

bool plat_running(void) { return aptMainLoop(); }

static uint32_t map_keys(u32 raw)
{
	static const struct {
		u32 ctr;
		uint32_t ours;
	} map[] = {
		{ KEY_A, K_A }, { KEY_B, K_B }, { KEY_X, K_X }, { KEY_Y, K_Y },
		{ KEY_L | KEY_ZL, K_L }, { KEY_R | KEY_ZR, K_R },
		{ KEY_UP, K_UP }, { KEY_DOWN, K_DOWN }, { KEY_LEFT, K_LEFT }, { KEY_RIGHT, K_RIGHT },
		{ KEY_START, K_START }, { KEY_SELECT, K_SELECT },
	};
	uint32_t k = 0;
	for (unsigned i = 0; i < sizeof(map) / sizeof(*map); i++)
		if (raw & map[i].ctr)
			k |= map[i].ours;
	return k;
}

void plat_frame(void)
{
	gfxFlushBuffers();
	gfxSwapBuffers();
	gspWaitForVBlank();
	hidScanInput();
	down = map_keys(hidKeysDown() | (hidKeysDownRepeat() & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT)));
}

uint32_t plat_keys_down(void) { return down; }

int plat_id(void) { return PLAT_3DS; }

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
	if (!soc_buf) {
		soc_buf = memalign(0x1000, SOC_SIZE);
		if (!soc_buf) {
			snprintf(err, errlen, "Out of memory for sockets.");
			return false;
		}
		Result r = socInit(soc_buf, SOC_SIZE);
		if (R_FAILED(r)) {
			free(soc_buf);
			soc_buf = NULL;
			snprintf(err, errlen, "socInit failed (%08lX).", (unsigned long)r);
			return false;
		}
	}
	printf("Waiting for Wi-Fi...\n");
	u32 status = 0;
	acInit();
	for (int i = 0; i < 20 && aptMainLoop(); i++) { // up to ~10 s while it associates
		if (R_SUCCEEDED(ACU_GetWifiStatus(&status)) && status)
			break;
		svcSleepThread(500 * 1000 * 1000LL);
	}
	acExit();
	if (!status) {
		snprintf(err, errlen, "Wi-Fi isn't connected. Turn it on and set up a connection in System Settings.");
		return false;
	}
	return true;
}

bool plat_text_input(const char *hint, const char *initial, char *out, size_t max, bool numeric)
{
	char init[512];
	snprintf(init, sizeof(init), "%s", initial); // copy first: initial may be out itself
	SwkbdState kb;
	swkbdInit(&kb, numeric ? SWKBD_TYPE_NUMPAD : SWKBD_TYPE_NORMAL, 2, (int)max - 1);
	swkbdSetHintText(&kb, hint);
	swkbdSetInitialText(&kb, init);
	swkbdSetValidation(&kb, SWKBD_ANYTHING, 0, 0);
	SwkbdButton b = swkbdInputText(&kb, out, max);
	if (b != SWKBD_BUTTON_CONFIRM) {
		memcpy(out, init, strlen(init) + 1 <= max ? strlen(init) + 1 : max);
		out[max - 1] = 0;
		return false;
	}
	return true;
}

const char *plat_config_path(void) { return "sdmc:/3ds/discord3ds.cfg"; }

bool plat_has_images(void) { return true; }
int plat_image_w(void) { return TOP_W; }
int plat_image_h(void) { return TOP_H; }

// The top framebuffer is 240x400 RGB565, rotated: column-major, bottom to top.
static void blit_fit(u16 *fb, const uint8_t *px, int w, int h)
{
	float sx = (float)TOP_W / w, sy = (float)TOP_H / h;
	float s = sx < sy ? sx : sy;
	int dw = (int)(w * s), dh = (int)(h * s);
	if (dw < 1)
		dw = 1;
	if (dh < 1)
		dh = 1;
	int ox = (TOP_W - dw) / 2, oy = (TOP_H - dh) / 2;
	for (int x = 0; x < dw; x++) {
		int srcx = x * w / dw;
		u16 *col = fb + (ox + x) * TOP_H;
		for (int y = 0; y < dh; y++) {
			const uint8_t *p = px + ((y * h / dh) * w + srcx) * 3;
			col[TOP_H - 1 - (oy + y)] = (u16)(((p[0] >> 3) << 11) | ((p[1] >> 2) << 5) | (p[2] >> 3));
		}
	}
}

uint32_t plat_show_image(const uint8_t *data, size_t len, const char *caption)
{
	u16 *fb = top.frameBuffer;
	int w, h, comp;
	uint8_t *px = stbi_load_from_memory(data, (int)len, &w, &h, &comp, 3);

	consoleSelect(&bottom);
	consoleClear();
	memset(fb, 0, TOP_W * TOP_H * sizeof(u16));
	printf("\x1b[36m%.39s\x1b[37m\n", caption);
	if (px) {
		blit_fit(fb, px, w, h);
		stbi_image_free(px);
		printf("%dx%d\n\n", w, h);
	} else {
		printf("\x1b[31mCan't decode this image\n(%.30s).\x1b[37m\n\n", stbi_failure_reason());
	}
	printf("\x1b[32mL/R: previous / next image\nB: back to the chat\x1b[37m");

	uint32_t k = 0;
	while (aptMainLoop()) {
		gfxFlushBuffers();
		gfxSwapBuffers();
		gspWaitForVBlank();
		hidScanInput();
		u32 d = hidKeysDown();
		if (d & (KEY_A | KEY_B | KEY_X | KEY_Y | KEY_L | KEY_R | KEY_ZL | KEY_ZR | KEY_LEFT | KEY_RIGHT | KEY_START)) {
			k = map_keys(d);
			break;
		}
	}
	// give the top screen back to the console; the app redraws both screens
	memset(fb, 0, TOP_W * TOP_H * sizeof(u16));
	consoleSelect(&top);
	consoleClear();
	consoleSelect(cur);
	return k;
}

int main(void) { return app_run(); }
