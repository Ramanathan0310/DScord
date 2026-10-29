// TCP link to the relay and the line protocol framing (see PROTOCOL.md).
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NET_MAX_FIELDS 12

typedef struct {
	void (*on_line)(char **f, int n);
	void (*on_blob)(char **f, int n, uint8_t *data, size_t len); // data is freed after the call
} NetHandlers;

bool net_connect(const char *host, int port, char *err, size_t errlen);
void net_close(void);
bool net_connected(void);

// Sends one frame; each field is escaped. Pass NULL to end the list.
bool net_send(const char *first, ...);

// Reads whatever is available and dispatches complete frames.
// Returns false once the connection has dropped.
bool net_poll(const NetHandlers *h);
