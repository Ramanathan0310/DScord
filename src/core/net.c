#include "net.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef PLATFORM_NDS
#include <dswifi9.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#define CLOSE_SOCKET closesocket
#else
#include <3ds.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#define CLOSE_SOCKET close
#endif

static int sock = -1;
static char rx[8192];
static size_t rxlen;

// BLOB in progress
static uint8_t *blob;
static size_t blob_len, blob_got;
static char blob_hdr[256];

static bool parse_ipv4(const char *s, uint32_t *out)
{
	unsigned a, b, c, d;
	char tail;
	if (sscanf(s, "%u.%u.%u.%u%c", &a, &b, &c, &d, &tail) != 4 || a > 255 || b > 255 || c > 255 || d > 255)
		return false;
	*out = htonl((a << 24) | (b << 16) | (c << 8) | d);
	return true;
}

bool net_connect(const char *host, int port, char *err, size_t errlen)
{
	net_close();
	struct sockaddr_in sa;
	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	uint32_t ip;
	if (!parse_ipv4(host, &ip)) {
		struct hostent *he = gethostbyname(host);
		if (!he || !he->h_addr_list[0]) {
			snprintf(err, errlen, "Can't resolve %s", host);
			return false;
		}
		memcpy(&ip, he->h_addr_list[0], 4);
	}
	sa.sin_addr.s_addr = ip;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		snprintf(err, errlen, "socket() failed");
		return false;
	}
	if (connect(sock, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		snprintf(err, errlen, "Can't reach %s:%d", host, port);
		CLOSE_SOCKET(sock);
		sock = -1;
		return false;
	}
#ifdef PLATFORM_NDS
	int on = 1;
	ioctl(sock, FIONBIO, &on);
#else
	fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
#endif
	rxlen = 0;
	return true;
}

void net_close(void)
{
	if (sock >= 0)
		CLOSE_SOCKET(sock);
	sock = -1;
	free(blob);
	blob = NULL;
	rxlen = 0;
}

bool net_connected(void) { return sock >= 0; }

static bool send_all(const char *p, size_t n)
{
	int spins = 0;
	while (n) {
		int r = send(sock, p, n, 0);
		if (r > 0) {
			p += r;
			n -= r;
			spins = 0;
			continue;
		}
		if (r == 0 || ++spins > 200000)
			return false;
#ifndef PLATFORM_NDS
		if (errno != EAGAIN && errno != EWOULDBLOCK)
			return false;
#endif
	}
	return true;
}

bool net_send(const char *first, ...)
{
	if (sock < 0)
		return false;
	static char out[4200];
	size_t o = 0;
	va_list ap;
	va_start(ap, first);
	for (const char *f = first; f; f = va_arg(ap, const char *)) {
		if (f != first && o < sizeof(out) - 1)
			out[o++] = '\t';
		for (; *f && o < sizeof(out) - 3; f++) {
			char c = *f;
			if (c == '\\' || c == '\n' || c == '\t') {
				out[o++] = '\\';
				c = c == '\n' ? 'n' : c == '\t' ? 't' : '\\';
			}
			out[o++] = c;
		}
	}
	va_end(ap);
	out[o++] = '\n';
	if (!send_all(out, o)) {
		net_close();
		return false;
	}
	return true;
}

// Splits a line into fields in place and unescapes them.
static int split(char *line, char **f)
{
	int n = 0;
	f[n++] = line;
	for (char *p = line; *p; p++)
		if (*p == '\t' && n < NET_MAX_FIELDS) {
			*p = 0;
			f[n++] = p + 1;
		}
	for (int i = 0; i < n; i++) {
		char *w = f[i];
		for (char *r = f[i]; *r; r++) {
			if (*r == '\\' && r[1]) {
				r++;
				*w++ = *r == 'n' ? '\n' : *r == 't' ? '\t' : *r;
			} else {
				*w++ = *r;
			}
		}
		*w = 0;
	}
	return n;
}

static void finish_blob(const NetHandlers *h)
{
	char *f[NET_MAX_FIELDS];
	int n = split(blob_hdr, f);
	uint8_t *data = blob;
	blob = NULL;
	if (h->on_blob)
		h->on_blob(f, n, data, blob_len);
	free(data);
}

// Consumes buffered bytes: blob payload first, then complete lines.
static void drain(const NetHandlers *h)
{
	size_t pos = 0;
	while (pos < rxlen) {
		if (blob) {
			size_t take = rxlen - pos;
			if (take > blob_len - blob_got)
				take = blob_len - blob_got;
			memcpy(blob + blob_got, rx + pos, take);
			blob_got += take;
			pos += take;
			if (blob_got == blob_len)
				finish_blob(h);
			continue;
		}
		char *nl = memchr(rx + pos, '\n', rxlen - pos);
		if (!nl)
			break;
		*nl = 0;
		char *line = rx + pos;
		pos = nl - rx + 1;
		if (!strncmp(line, "BLOB\t", 5)) {
			const char *len = strrchr(line, '\t') + 1;
			blob_len = strtoul(len, NULL, 10);
			blob_got = 0;
			snprintf(blob_hdr, sizeof(blob_hdr), "%s", line);
			blob = malloc(blob_len ? blob_len : 1);
			if (!blob) {
				net_close(); // can't hold it; drop the link rather than desync
				return;
			}
			if (blob_len == 0)
				finish_blob(h);
			continue;
		}
		char *f[NET_MAX_FIELDS];
		int n = split(line, f);
		h->on_line(f, n);
		if (sock < 0)
			return;
	}
	memmove(rx, rx + pos, rxlen - pos);
	rxlen -= pos;
	if (rxlen == sizeof(rx) && !blob) // oversized line: discard
		rxlen = 0;
}

bool net_poll(const NetHandlers *h)
{
	if (sock < 0)
		return false;
	for (int rounds = 0; rounds < 64; rounds++) {
		int r = recv(sock, rx + rxlen, sizeof(rx) - rxlen, 0);
		if (r == 0) {
			net_close();
			return false;
		}
		if (r < 0) {
#ifndef PLATFORM_NDS
			if (errno != EAGAIN && errno != EWOULDBLOCK) {
				net_close();
				return false;
			}
#endif
			break;
		}
		rxlen += r;
		drain(h);
		if (sock < 0)
			return false;
	}
	return true;
}
