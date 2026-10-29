# DScord

A small Discord client for the Nintendo DS, DSi and 3DS.

**New here? Read the [beginner's walkthrough](docs/WALKTHROUGH.md).** It
covers everything step by step, from creating the bot to your first message.
This README is the short technical version.

The handhelds can't do modern TLS, so they don't talk to Discord directly. A
relay (`relay/relay.mjs`, Node, no dependencies) runs on a computer on your
network, holds the Discord connection, and speaks a plain-TCP line protocol
([PROTOCOL.md](PROTOCOL.md)) to the handheld.

```
DS / DSi / 3DS  --plain TCP-->  relay (your computer)  --HTTPS / WSS-->  Discord
```

Text is ASCII only: the relay turns emoji and accented letters into
approximations (`:joy:`, `e`, ...). Images can be viewed on the 3DS.

## 1. Choose an account type

The relay logs in with a **bot token** (the default) or, if you opt in, a
**user token**.

| | Bot token (`"tokenType": "bot"`) | User token (`"tokenType": "user"`) |
|---|---|---|
| Servers | Only servers the bot was invited to (private ones too) | Every server your account is in |
| DMs | Only DMs sent to the bot | Your own DMs and group DMs |
| Unread marks | Everything starts as read when the relay starts | Uses your account's read state |
| Discord's rules | Allowed | **Against the Terms of Service** (see below) |

**About user tokens:** Discord's Terms of Service and Developer Policy forbid
automating a normal user account ("self-bots"), and Discord's support pages say
accounts doing it may be terminated. Discord doesn't have to see abuse first:
the ban covers using the account through unofficial software at all. The relay
doesn't hide what it is. It identifies itself as `DScord` and doesn't
pretend to be the official client. Only use user mode on an account you're
willing to lose. Anyone who has the relay's address and PIN acts **as you**:
they can read your DMs and send messages under your name. So run one relay
per person, each with that person's own token. Never share a relay that holds
your user token. We do not accept responsibility, for someone's account being suspended, or banned from Discord

### Bot setup (recommended)

1. Open the [Discord Developer Portal](https://discord.com/developers/applications)
   and click **New Application**.
2. Under **Bot**, click **Reset Token** and copy the token. Keep it secret.
3. On the same page, under **Privileged Gateway Intents**, turn on
   **Message Content Intent**. Without it the relay can't read messages and
   stops with close code 4014.
4. Under **OAuth2**, copy the **Client ID** and open this link with your ID
   filled in, to invite the bot to a server you manage:

   ```
   https://discord.com/oauth2/authorize?client_id=YOUR_CLIENT_ID&scope=bot&permissions=68608
   ```

   `68608` = View Channels + Send Messages + Read Message History. Someone with
   **Manage Server** has to approve the invite. The bot only sees channels its
   roles can see.

### User token (opt-in)

Set `"tokenType": "user"` and put your own account's token in `"token"`.
Read the warning above first. A user token gives full access to your
account, so treat it like a password.

## 2. Run the relay

Needs Node.js 22 or newer.

```bash
cp relay/config.example.json relay/config.json
```

Edit `relay/config.json`:

```json
{
  "token": "YOUR_BOT_TOKEN",
  "tokenType": "bot",
  "pin": "1234",
  "port": 5151,
  "host": "0.0.0.0"
}
```

- `token`: the bot token (or user token in user mode).
- `tokenType`: `"bot"` (default) or `"user"`.
- `pin`: 4 or more digits. The handheld must send it before the relay does
  anything. Choose your own.
- `port` / `host`: where the relay listens. `0.0.0.0` means every network
  interface.
- `updateCheck`: `true` (default) lets the relay ask GitHub for the latest
  DScord release at startup and every 6 hours (see Updating).

Each setting can also come from the environment: `DISCORD_TOKEN`,
`DISCORD_TOKEN_TYPE`, `RELAY_PIN`, `RELAY_PORT`, `RELAY_HOST`,
`RELAY_UPDATE_CHECK`, and `RELAY_CONFIG` for a different config file path.

```bash
make relay
```

(or `node relay/relay.mjs`). It prints `Logged in as bot ...` once Discord
accepts the token, and logs each handheld that connects.

Find the computer's LAN IP address (macOS: System Settings > Wi-Fi > Details;
Windows: `ipconfig`; Linux: `ip addr`). That's the address you enter on the
handheld. Allow incoming connections on the port if a firewall asks.

**Security:** the handheld link is unencrypted and only protected by the PIN.
Keep the relay on your own network. Don't port-forward it to the internet.

## 3. Network notes

- **Original DS / DS Lite:** Wi-Fi supports only **open or WEP** networks. Most
  home routers use WPA2, so either:
  - make a hotspot (phone, laptop, or an old router) that's open or WEP, ideally
    used only for this, or
  - enable a separate legacy/guest network on your router if it can do WEP.

  The relay computer must be reachable from that network. The simplest setup is
  a laptop that shares an open hotspot and also runs the relay; the handheld then
  uses the laptop's hotspot IP.
- **DSi:** homebrew uses the DS-compatible Wi-Fi settings (connections 1 to 3),
  so the same open/WEP limit applies even in DSi mode.
- **3DS:** uses the system's Wi-Fi settings, WPA2 included.

Set up the DS connection in the **Nintendo WFC settings** of any Wi-Fi DS game
(e.g. Mario Kart DS), or in the DSi's System Settings under the DS-compatible
connections. The 3DS uses System Settings > Internet Settings.

## 4. Build

Install [devkitPro](https://devkitpro.org/wiki/Getting_Started). On macOS,
install the devkitPro pacman package, then:

```bash
sudo dkp-pacman -S nds-dev 3ds-dev
```

Open a new terminal so `DEVKITPRO` / `DEVKITARM` are set, then:

```bash
make nds
```

```bash
make 3ds
```

The outputs are `dist/dscord.nds` and `dist/dscord.3dsx`. `make check`
syntax-checks the relay.

No local toolchain? `.github/workflows/build.yml` builds both apps on GitHub
Actions with devkitPro's official `devkitpro/devkitarm` container. Every push
uploads them as a workflow artifact, and pushing a `v*` tag (e.g. `v1.0.0`)
attaches them to a GitHub release.

Layout: `src/core` is the shared client (UI in `app.c`, protocol framing in
`net.c`, settings in `config.c`, platform interface in `platform.h`).
`src/nds` and `src/3ds` hold the platform layers and their Makefiles.
`third_party/stb_image.h` (public domain, from
[nothings/stb](https://github.com/nothings/stb)) decodes images on the 3DS.

## 5. Install

- **DS flashcart (R4 and similar):** copy `dscord.nds` to the card's microSD
  and launch it from the cart's menu. If the cart supports DLDI auto-patching
  (most do), settings are saved to `/dscord.cfg` on the card.
- **DSi / 3DS with TWiLight Menu++:** copy `dscord.nds` to the SD card and
  launch it from TWiLight Menu++. On a DSi it runs in DSi mode and saves
  `/dscord.cfg` to the SD card root. On a 3DS, TWiLight runs it in DS mode
  with DS Wi-Fi limits, so prefer the `.3dsx` there.
- **3DS Homebrew Launcher:** copy `dscord.3dsx` to `sdmc:/3ds/` and start it
  from the Homebrew Launcher. Settings are saved to `sdmc:/3ds/dscord.cfg`.

On first start the client asks for the relay address, port and PIN. Press X
on the offline screen (or the server list) to change them later.

## Updating

Nothing updates by itself. The handhelds can't reach GitHub, so the relay
checks `api.github.com` for the latest release (unless `updateCheck` is
`false`) and:

- prints `Update available: DScord vX` in its console when the relay is behind;
- tells each connecting app that's behind. v1.2+ apps show the update on the
  top screen; older apps get a notification line.
- v1.2+ apps also warn when the relay is older than the app.

To update, download the new `.nds` / `.3dsx` from
[Releases](https://github.com/Ramanathan0310/DScord/releases) and copy it over
the old one, then replace the `relay` folder's `relay.mjs` with the new one
(keep your `config.json`) and restart the relay.

## 6. Controls

Chat messages appear on the top screen. Menus, status, notifications (mentions
and DMs from other channels) and key help appear on the bottom screen.

| Where | Key | Action |
|---|---|---|
| Lists | Up / Down | Move (category headers are skipped) |
| Lists | L / R or Left / Right | Page |
| Lists | A | Open |
| Lists | B | Back |
| Lists | Y | Refresh |
| Server list / offline | X | Edit relay settings |
| Server list / offline | START | Quit |
| Offline | A | Retry now (it also retries by itself after a lost connection) |
| Chat | Up / Down | Select a message |
| Chat | L / R or Left / Right | Scroll a page |
| Chat | Y | Load older messages |
| Chat | X | Write a message (on-screen keyboard) |
| Chat | A | View the selected message's images (3DS) |
| Chat | B | Back to the channel list |
| Image viewer (3DS) | L / R | Previous / next image |
| Image viewer (3DS) | B | Close |
| DS keyboard | START or Enter | Send |
| DS keyboard | B | Cancel |

## Troubleshooting

- **"Can't reach host:port"**: check the IP, that the relay is running, that
  the handheld and computer are on the same network, and the firewall.
- **"Wrong PIN"**: the PIN must match `pin` in `relay/config.json`.
- **Relay exits with 4004**: the token or `tokenType` is wrong.
- **Relay exits with 4014**: turn on Message Content Intent (bot mode).
- **DS says "No Wi-Fi"**: set up an open/WEP connection in WFC settings (see
  Network notes).
