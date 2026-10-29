// Interface each handheld (src/nds, src/3ds) implements for the shared core.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
	K_A = 1 << 0, K_B = 1 << 1, K_X = 1 << 2, K_Y = 1 << 3,
	K_L = 1 << 4, K_R = 1 << 5, K_UP = 1 << 6, K_DOWN = 1 << 7,
	K_LEFT = 1 << 8, K_RIGHT = 1 << 9, K_START = 1 << 10, K_SELECT = 1 << 11,
};

enum { SCR_TOP = 0, SCR_BOTTOM = 1 };

// ANSI colour numbers (30 + n)
enum { COL_BLACK, COL_RED, COL_GREEN, COL_YELLOW, COL_BLUE, COL_MAGENTA, COL_CYAN, COL_WHITE };

enum { PLAT_DS = 0, PLAT_DSI = 1, PLAT_3DS = 2 };

bool plat_init(void);
void plat_exit(void);
bool plat_running(void);
void plat_frame(void); // wait for vblank, then scan input
uint32_t plat_keys_down(void); // newly pressed, with auto-repeat on the d-pad
int plat_id(void);

// Text consoles: one per screen.
void plat_select(int screen);
int plat_cols(int screen);
int plat_rows(int screen);
void plat_clear(void);
void plat_goto(int row, int col);

// Brings up Wi-Fi; blocking. Writes progress to the selected console.
bool plat_net_init(char *err, size_t errlen);

// Blocking on-screen keyboard. Returns false if cancelled.
bool plat_text_input(const char *hint, const char *initial, char *out, size_t max, bool numeric);

const char *plat_config_path(void);

// Image support (3DS only). plat_show_image blocks until a key is pressed
// and returns that key.
bool plat_has_images(void);
int plat_image_w(void);
int plat_image_h(void);
uint32_t plat_show_image(const uint8_t *data, size_t len, const char *caption);
