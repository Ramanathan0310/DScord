#pragma once
#include <stdbool.h>

typedef struct {
	char host[64];
	int port;
	char pin[16];
} Config;

bool config_load(Config *c);
bool config_save(const Config *c);
// Asks for each setting with the on-screen keyboard. False if cancelled.
bool config_prompt(Config *c);
