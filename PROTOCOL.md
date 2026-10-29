# Relay protocol (v1)

Plain TCP, default port 5151. Every frame is one line of UTF-8 (ASCII in
practice; the relay transliterates) terminated by `\n`. Fields are separated
by `\t`. Inside a field, `\\`, `\n` and `\t` are escaped as `\\\\`, `\\n`,
`\\t`. The only exception is `BLOB`, whose header line is followed by raw
bytes.

## Client -> relay

| Line | Meaning |
|------|---------|
| `HELLO	1	<platform>	<pin>` | platform: `0` DS, `1` DSi, `2` 3DS. Must be first. |
| `GUILDS` | List servers. |
| `CHANNELS	<guild_id>` | List text channels of a server. |
| `DMS` | List DM / group DM channels, newest first. |
| `HIST	<channel_id>	<before_id or 0>` | Fetch up to 20 messages; also subscribes to live events for that channel. |
| `SEND	<channel_id>	<text>` | Send a message. |
| `IMG	<message_id>	<index>	<maxw>	<maxh>` | Fetch an image attachment, resized by Discord's media proxy. |
| `PING` | Keepalive. |

## Relay -> client

| Line | Meaning |
|------|---------|
| `OK	<username>` | Login accepted, gateway ready. |
| `ERR	<text>` | Error (bad PIN closes the connection). |
| `G	<id>	<name>	<unread>` | One server. Ends with `END	G`. |
| `C	<id>	<name>	<unread>	<kind>` | One channel; kind `0` text, `1` category header. Ends with `END	C`. |
| `D	<id>	<name>	<unread>` | One DM channel. Ends with `END	D`. |
| `M	<chan>	<id>	<author>	<time>	<images>	<flags>	<text>` | History message, oldest first. Ends with `END	M	<chan>	<has_more>`. |
| `N	...` | New live message in the subscribed channel (same fields as `M`). |
| `U	...` | Edited message (same fields as `M`). |
| `X	<chan>	<id>` | Deleted message. |
| `NOTE	<text>` | Mention or DM elsewhere. |
| `SENT` | Send succeeded. |
| `BLOB	<message_id>	<index>	<count>	<length>` + `length` raw bytes | Image file (PNG/JPEG/GIF). `count` is the number of images on that message. |
| `PONG` | Keepalive reply. |

Flags: bit 0 mentions you, bit 1 edited, bit 2 sent by you.
