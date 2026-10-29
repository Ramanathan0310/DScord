#include "config.h"
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool config_load(Config *c)
{
	FILE *fp = fopen(plat_config_path(), "r");
	if (!fp)
		return false;
	char line[128];
	c->host[0] = c->pin[0] = 0;
	c->port = 5151;
	while (fgets(line, sizeof(line), fp)) {
		line[strcspn(line, "\r\n")] = 0;
		char *eq = strchr(line, '=');
		if (!eq)
			continue;
		*eq = 0;
		const char *v = eq + 1;
		if (!strcmp(line, "host"))
			snprintf(c->host, sizeof(c->host), "%s", v);
		else if (!strcmp(line, "port"))
			c->port = atoi(v);
		else if (!strcmp(line, "pin"))
			snprintf(c->pin, sizeof(c->pin), "%s", v);
	}
	fclose(fp);
	return c->host[0] && c->pin[0] && c->port > 0;
}

bool config_save(const Config *c)
{
	FILE *fp = fopen(plat_config_path(), "w");
	if (!fp)
		return false;
	fprintf(fp, "host=%s\nport=%d\npin=%s\n", c->host, c->port, c->pin);
	fclose(fp);
	return true;
}

bool config_prompt(Config *c)
{
	char port[12];
	snprintf(port, sizeof(port), "%d", c->port > 0 ? c->port : 5151);
	if (!plat_text_input("Relay address (IP or hostname)", c->host, c->host, sizeof(c->host), false))
		return false;
	if (!plat_text_input("Relay port", port, port, sizeof(port), true))
		return false;
	if (!plat_text_input("Relay PIN", c->pin, c->pin, sizeof(c->pin), true))
		return false;
	c->port = atoi(port);
	return c->host[0] && c->pin[0] && c->port > 0;
}
