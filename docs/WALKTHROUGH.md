# DScord: the complete beginner's walkthrough

This guide takes you from nothing to chatting on Discord from a Nintendo DS,
DSi or 3DS. You don't need to know how to program. Follow the steps in order
and tick them off as you go. Most people finish in 30 to 60 minutes.

If you get stuck, jump to [Troubleshooting](#troubleshooting) at the end.

---

## Contents

1. [How it works (two-minute version)](#1-how-it-works-two-minute-version)
2. [What you need](#2-what-you-need)
3. [Install Node.js on your computer](#3-install-nodejs-on-your-computer)
4. [Download DScord](#4-download-dscord)
5. [Run the relay setup](#5-run-the-relay-setup)
6. [Start the relay](#6-start-the-relay)
7. [The address, port and PIN for the handheld](#7-the-address-port-and-pin-for-the-handheld)
8. [Changing settings later](#8-changing-settings-later)
9. [Get your handheld online](#9-get-your-handheld-online)
10. [Copy the app to your handheld](#10-copy-the-app-to-your-handheld)
11. [First launch](#11-first-launch)
12. [Using DScord](#12-using-dscord)
13. [Everyday use](#13-everyday-use)
14. [Optional: log in as yourself instead of a bot](#14-optional-log-in-as-yourself-instead-of-a-bot)
15. [Troubleshooting](#troubleshooting)
16. [FAQ](#faq)

---

## 1. How it works (two-minute version)

Discord needs strong modern encryption that a DS can't do. So DScord comes
in two parts:

```
 ┌──────────────┐   your home Wi-Fi    ┌───────────────────┐   internet   ┌─────────┐
 │ DS / DSi /   │ ───────────────────▶ │  The relay        │ ───────────▶ │ Discord │
 │ 3DS app      │ ◀─────────────────── │  (on your computer│ ◀─────────── │         │
 └──────────────┘    simple messages   │   or a Pi)        │   encrypted  └─────────┘
                                       └───────────────────┘
```

- **The relay** is a small program that runs on a computer in your home
  (Windows, Mac, Linux or a Raspberry Pi). It logs in to Discord for you.
- **The app** runs on your handheld. It talks only to the relay.

The relay has to be **running** whenever you want to use the app, and the
handheld has to be able to **reach** it over Wi-Fi.

By default the relay logs in as a **Discord bot**: a separate account you
create for free that appears in your server with a "BOT" tag. The DS shows
whatever that bot can see. (Logging in as yourself is possible too, but it
carries real risk; see [section 14](#14-optional-log-in-as-yourself-instead-of-a-bot).)

---

## 2. What you need

- [ ] **A computer** that can stay on while you chat: Windows 10/11, macOS,
      Linux or a Raspberry Pi.
- [ ] **A handheld that can run homebrew:**
  - a **DS or DS Lite** with a flashcart (R4, Acekard, DSTT and similar), or
  - a **DSi / DSi XL** with Unlaunch or hiyaCFW and TWiLight Menu++, or
  - a **3DS / 2DS / New 3DS** with custom firmware (it has the Homebrew Launcher).

  Setting up homebrew is outside this guide. If you haven't done it yet, follow
  the community guides: [3ds.hacks.guide](https://3ds.hacks.guide) for 3DS and
  [dsi.cfw.guide](https://dsi.cfw.guide) for DSi. A DS or DS Lite only needs a
  flashcart.
- [ ] **A way to copy files to the handheld's SD card** (an SD card reader, or
      FTP on a 3DS).
- [ ] **A Discord account** and a server where you're allowed to add bots (any
      server you created yourself works).
- [ ] **Wi-Fi the handheld can join.** Read this now, because it's the most common
      blocker:
  - **3DS:** any normal home Wi-Fi works.
  - **DS, DS Lite and DSi:** these can only join Wi-Fi with **no password** or
    with old **WEP** security. Most home routers use WPA2, which the DS can't
    use. You'll set this up in [section 9](#9-get-your-handheld-online).
- [ ] **The app files:** `dscord.nds` (for DS/DSi) and/or `dscord.3dsx`
      (for 3DS). Download them from the [Releases page](https://github.com/Ramanathan0310/DScord/releases), or build them
      yourself (see the main [README](../README.md#4-build)).

---

## 3. Install Node.js on your computer

The relay is written in JavaScript and needs **Node.js version 22 or newer**.

1. Go to <https://nodejs.org>.
2. Download the **LTS** version and run the installer, accepting the defaults.
3. Check it worked. Open a terminal:
   - **Windows:** press Start, type `PowerShell`, open it.
   - **Mac:** press Cmd+Space, type `Terminal`, open it.
   - **Linux / Raspberry Pi:** open your Terminal app.

   Type this and press Enter:

   ```bash
   node --version
   ```

   You should see something like `v22.11.0`. If the first number is **22 or
   higher**, you're good. If it says "command not found", restart the computer
   and try again.

> **Raspberry Pi tip:** the Node.js in Raspberry Pi OS's own package list is
> often too old. Use the instructions at
> <https://nodejs.org/en/download> (choose "Linux" and "nvm" or "fnm").

---

## 4. Download DScord

1. On the project's GitHub page (<https://github.com/Ramanathan0310/DScord>), click the green **Code** button, then
   **Download ZIP**. (If you know git, `git clone` works too.)
2. Unzip it somewhere easy to find, such as your Documents folder.
3. You should now have a folder called `DScord` (it may be called
   `DScord-main`; either is fine). Inside it are folders named `relay`,
   `src`, `docs` and others.

From now on, **"the DScord folder"** means this folder.

---

## 5. Run the relay setup

The relay has a built-in setup that asks a few questions, checks your answers
with Discord, and saves them. You only do this once.

**Start it:** open the DScord folder and double-click the starter for your
computer:

| Computer | Double-click | If it doesn't open |
|---|---|---|
| Windows | `start-relay.bat` | If Windows shows "Windows protected your PC", click **More info > Run anyway**. |
| Mac | `start-relay.command` | If macOS says it "cannot be opened", **right-click** it, choose **Open**, then **Open** again. |
| Linux / Pi | `start-relay.sh` | Run `./start-relay.sh` in a terminal from the DScord folder. |

(Prefer the terminal? `cd` into the DScord folder and run
`node relay/relay.mjs`. It's the same thing.)

A window opens with **"Welcome to the DScord relay setup"**. The setup has
four steps. Keep the window and a web browser side by side.

### 5a. Step 1: account type

The setup asks:

```
  1) Bot account (recommended)
  2) Your own user account
```

Type **1** and press Enter. (Option 2 is explained in
[section 14](#14-optional-log-in-as-yourself-instead-of-a-bot). Read that
first if you're tempted, because it can get your account banned.)

### 5b. Step 2: create the bot and paste its token

In your browser:

1. Go to the **Discord Developer Portal**:
   <https://discord.com/developers/applications> and log in with your normal
   Discord account.
2. Click **New Application** (top right). Give it a name, such as `My DS`.
   This becomes the bot's name. Tick the box to agree to the terms and click
   **Create**.
3. In the left menu, click **Bot**. Click **Reset Token**, confirm, and enter
   your 2FA code if asked. Click **Copy**.

Back in the setup window, **paste** the token (Windows: right-click or
Ctrl+V; Mac: Cmd+V) and press Enter. It shows as `*****` on purpose. The setup
checks it with Discord and says **OK: bot "My DS"**.

> ⚠️ **The token is a password.** Anyone with it can control your bot. Never
> post it in Discord, in screenshots, or on GitHub. If it leaks, click Reset
> Token again and re-run the setup (see [section 8](#8-changing-settings-later)).

If the setup says the token is invalid, copy it again. Reset Token makes a new
one each time, so only the newest works.

### 5c. Step 3: let the bot read messages, and invite it

The setup checks whether the bot is allowed to read message text. If it says
**Message Content Intent is OFF**:

1. In the Developer Portal, still on the **Bot** page, scroll to
   **Privileged Gateway Intents**.
2. Turn on **Message Content Intent** and click **Save Changes**.
3. Press Enter in the setup window. It checks again.

Next it prints an **invite link** (starting with
`https://discord.com/oauth2/authorize?...`). Copy it into your browser, pick
your server, and click **Continue**, then **Authorize**. Press Enter in the
setup window, and it confirms **The bot is in 1 server: …**. You can invite it
to more servers now, or later with the same link.

> **Only servers you manage.** To add a bot to a server you need the **Manage
> Server** permission there. For other servers, send the invite link to an
> admin. The bot only sees channels its roles are allowed to see, the same as
> a person would.

### 5d. Step 4: PIN and port

- **PIN:** the setup suggests a random 6-digit PIN. Press Enter to accept it,
  or type your own (4+ digits). You'll type it on the handheld once. It stops
  other people on your Wi-Fi from using your relay.
- **Port:** press Enter to keep `5151`.
- **Check for updates:** press Enter for yes (see
  [Updating DScord](#updating-dscord)).

The setup saves everything to `relay/config.json` and shows what to enter on
the handheld:

```
  On the DS / DSi / 3DS, enter:
    Relay address: 192.168.1.23
    Relay port:    5151
    Relay PIN:     482915
```

**Write these three down.** (You can show them again any time, see
[section 7](#7-the-address-port-and-pin-for-the-handheld).)

---

## 6. Start the relay

At the end of the setup it asks **Start the relay now?** Press Enter. After a
few seconds you should see:

```
12:00:01 DScord relay v1.2.0 listening on 0.0.0.0:5151

  On the DS / DSi / 3DS, enter:
    Relay address: 192.168.1.23
    Relay port:    5151

12:00:01 Gateway: connecting
12:00:02 Logged in as bot My DS; 1 servers
```

In Discord, your bot now shows as online. 🎉

**Leave this window open.** Closing it, or letting the computer sleep, stops
the relay. To stop it on purpose, click the window and press **Ctrl+C**.

**Next time**, just double-click the same starter (`start-relay.bat` /
`.command` / `.sh`). The setup only runs when there are no settings yet.

**If your computer asks "Allow incoming connections?"** (Windows Firewall or
the macOS firewall), click **Allow**. On Windows, tick **Private networks**.
Without this, the handheld can't reach the relay.

**If you see an error instead**, check [Troubleshooting](#troubleshooting). The
relay tells you what's wrong.

---

## 7. The address, port and PIN for the handheld

The relay prints the **address** every time it starts. That's your computer's
local IP address, something like `192.168.1.23` or `10.0.0.5`. The setup
showed the PIN. To see all three again, run this in the DScord folder:

```bash
node relay/relay.mjs --info
```

If it lists more than one address, use the one on the same Wi-Fi as the
handheld (usually the one starting with `192.168.`).

Finding it by hand instead:

- **Windows:** in PowerShell, run `ipconfig`. Look for **IPv4 Address** under
  your Wi-Fi or Ethernet adapter.
- **Mac:** System Settings > Wi-Fi > click **Details…** next to your network.
- **Linux / Pi:** run `hostname -I`. The first address is usually the one.

> **Tip:** this address can change after a router restart. If the app suddenly
> can't connect one day, check it again. You can usually make it permanent with
> a "DHCP reservation" in your router's settings.

---

## 8. Changing settings later

- **Run the setup again:** in the DScord folder run
  `node relay/relay.mjs --setup` (or `start-relay.bat --setup` /
  `./start-relay.command --setup`). It shows your current answers as defaults,
  so you can press Enter to keep them. Use this to switch between bot and user
  accounts, change the token or PIN, or invite the bot to more servers.
- **Edit the file by hand:** the settings live in `relay/config.json`, a plain
  text file:

  ```json
  {
    "token": "EXAMPLE.not-a-real-token.paste-yours-here",
    "tokenType": "bot",
    "pin": "482915",
    "port": 5151,
    "host": "0.0.0.0",
    "updateCheck": true
  }
  ```

  Use a plain text editor (Notepad, or TextEdit in *Format > Make Plain Text*
  mode). Keep the quote marks and commas exactly as shown. Restart the relay
  after editing.

- **More commands:** `node relay/relay.mjs --help`.

---

## 9. Get your handheld online

### 3DS / 2DS / New 3DS

1. **System Settings > Internet Settings > Connection Settings**.
2. Set up a connection to your normal home Wi-Fi and run the connection test.
   That's it.

### DS, DS Lite and DSi

These only support Wi-Fi with **no security** or **WEP**. You have a few options:

| Option | Difficulty | Notes |
|---|---|---|
| **Phone hotspot with no password** | Easy | Most phones let you turn the hotspot password off. The relay computer must join the same hotspot. Turn the hotspot off when you're done. |
| **Computer hotspot** | Easy to medium | Windows "Mobile hotspot" and macOS "Internet Sharing" can share an open network. Then the relay runs on the same computer, and you use the IP address the computer has *on that hotspot*. |
| **Old router** | Medium | Any old Wi-Fi router set to open or WEP, plugged into your main router. Use it only for the DS. |
| **Guest network on your router** | Varies | Some routers allow an open guest network. Guest networks often block devices from talking to each other, so check that "client isolation" or "AP isolation" is off. |

> **Security note:** an open network can be joined by anyone nearby. Use one
> just for the DS, turn it off when you're done, and keep your relay PIN secret.

**Saving the connection on a DS or DS Lite:** DScord uses the
**Nintendo Wi-Fi Connection (WFC) settings**. These live inside games that had
online play, not in the DS menu:

1. Start any DS game with Wi-Fi features (e.g. *Mario Kart DS*, *Pokémon
   Diamond/Pearl*, *Animal Crossing: Wild World*). Look for an option like
   **Nintendo WFC Settings** (in Mario Kart DS: *Nintendo WFC > Nintendo WFC
   Settings*).
2. Choose **Nintendo Wi-Fi Connection Settings > Connection 1**, then **Search
   for an Access Point**.
3. Pick your open/WEP network, enter the WEP key if it has one, and save. The
   connection test may fail (Nintendo's servers are gone). **That's fine**, the
   settings are saved anyway.

Some flashcart menus and homebrew tools can also edit these settings.

**DSi:** go to **System Settings > Internet > Connection Settings** and set up
**Connection 1, 2 or 3** (the DS-compatible slots). Even in DSi mode, homebrew
Wi-Fi only uses these slots, so the open/WEP rule still applies.

---

## 10. Copy the app to your handheld

Put the handheld's SD card (or the flashcart's microSD) into your computer.

### DS / DS Lite with a flashcart

1. Copy `dscord.nds` anywhere on the flashcart's microSD (the root is fine).
2. Put the card back and start the flashcart's menu. You'll find **DScord**
   in the file list.

### DSi with TWiLight Menu++

1. Copy `dscord.nds` to the SD card, for example into the `roms/nds` folder
   or wherever you keep your `.nds` files.
2. Start TWiLight Menu++ and launch **DScord**.

### 3DS / 2DS with custom firmware

1. Copy `dscord.3dsx` into the `3ds` folder on the SD card (so it ends up at
   `sdmc:/3ds/dscord.3dsx`). Create the folder if it doesn't exist.
2. Put the card back, open the **Homebrew Launcher** (usually by holding R
   while starting the Mii Maker, or from the Rosalina menu), and choose
   **DScord**.

> A 3DS can also run the `.nds` version through TWiLight Menu++, but then it
> behaves like a DS (open/WEP Wi-Fi only, no images). Use the `.3dsx`.

---

## 11. First launch

1. **Make sure the relay is running** on your computer ([section 6](#6-start-the-relay)).
2. Start DScord. The first time, it asks three questions using the
   on-screen keyboard:

   | Question | What to enter |
   |---|---|
   | **Relay address** | The address the relay shows ([section 7](#7-the-address-port-and-pin-for-the-handheld)), e.g. `192.168.1.23` |
   | **Relay port** | `5151` (already filled in; just confirm) |
   | **Relay PIN** | The PIN from the setup (`node relay/relay.mjs --info` shows it) |

   - **DS / DSi:** tap the keys on the touch screen with the stylus. Press
     **START** (or the keyboard's Enter key) when done. **B** cancels.
   - **3DS:** the normal system keyboard appears. Tap **OK** when done.

3. The DS connects to Wi-Fi, then to the relay. After a moment the bottom screen
   shows **Online as …** and your server list. Your settings are saved, so next
   time it connects by itself.

To change the settings later, press **X** on the server list or on the
"Not connected" screen.

---

## 12. Using DScord

### The two screens

- **Top screen:** messages, when you're in a chat.
- **Bottom screen:** where you are, a status line, menus, notifications and a
  reminder of the buttons.

### Picking a channel

The first screen lists your servers, with **Direct Messages** at the top:

```
┌────────────────────────────────┐
│Servers (2)                     │  ← where you are
│Online as My DS                 │  ← status line
│--------------------------------│
│> Direct Messages               │  ← ">" is the selection
│ *My Server                     │  ← "*" means unread messages
│  Friends Hangout               │
│                                │
│--------------------------------│
│alice (DM): are you there?      │  ← notifications
│A:open  Y:refresh  X:settings   │  ← buttons for this screen
│START:quit                      │
└────────────────────────────────┘
```

- **Up / Down** moves the selection. **L / R** jumps a page.
- **A** opens the server, and you'll see its channels grouped under category
  names (in CAPITALS). Categories can't be selected; the cursor skips them.
- **B** goes back. **Y** refreshes the list.

### Reading a chat

Open a channel with **A**. Messages appear on the top screen, oldest at the top
and newest at the bottom:

```
┌────────────────────────────────┐
│ alice 12:04                    │  ← who and when
│ did anyone see the update?     │
│>bob 12:05 [img]                │  ← ">" = selected message
│ yes! here's a screenshot       │     [img] = has pictures
│ [img 1: screenshot.png]        │
│ My DS 12:06                    │  ← your messages are green
│ hello from my DS lite :)       │
└────────────────────────────────┘
```

- **Up / Down** selects a message. The bottom screen shows its details.
- **L / R** (or **Left / Right**) scrolls a whole page.
- **Y** loads older messages. Keep pressing to go further back.
- New messages appear live. If the newest message is selected, the view follows
  new messages; otherwise it stays where you're reading.
- Messages that mention you are shown in **red**, edited messages say
  **(edited)**, and replies start with `[re @name: ...]`.

### Writing a message

Press **X** in a chat, type, and confirm (**START** / Enter on DS, **OK** on
3DS). The status line shows **Sent** once Discord has it.

### Pictures (3DS only)

Select a message that shows `[img]` and press **A**. The picture fills the top
screen. **L / R** step through multiple pictures; **B** returns to the chat.

On a DS or DSi you'll see `[img 1: name.png]` in place of the picture.

### Emoji and special characters

The DS font only has basic English letters, so the relay converts text:
😂 becomes `:joy:`, ❤ becomes `<3`, "é" becomes "e", and custom server emoji
show as `:name:`. Characters it can't convert appear as `?` or `*`.

### Notifications

When someone mentions you or sends a DM while you're in another channel, a line
appears in the notification area at the bottom. The last three are kept.

### Full list of buttons

| Where | Button | What it does |
|---|---|---|
| Any list | Up / Down | Move |
| Any list | L / R, Left / Right | Page up / down |
| Any list | A | Open |
| Any list | B | Back |
| Any list | Y | Refresh |
| Server list, Not connected | X | Change relay settings |
| Server list, Not connected | START | Quit |
| Not connected | A | Try again now |
| Chat | Up / Down | Select message |
| Chat | L / R, Left / Right | Scroll a page |
| Chat | Y | Load older messages |
| Chat | X | Write a message |
| Chat | A | View pictures (3DS) |
| Chat | B | Back to channel list |
| Picture viewer | L / R | Previous / next picture |
| Picture viewer | B | Close |

---

## 13. Everyday use

Each time you want to chat:

1. Start the relay on your computer (`node relay/relay.mjs` in the DScord
   folder) and leave it open.
2. If you use a hotspot for the DS, turn it on.
3. Start DScord on the handheld. It reconnects by itself.

If the connection drops (for example you walk out of Wi-Fi range), the app
shows **Not connected** and tries again every 15 seconds. Press **A** to retry
immediately.

**Keep the computer awake.** If your computer goes to sleep, the relay stops
responding. On a laptop, change the sleep settings while you chat, or run the
relay on an always-on machine such as a Raspberry Pi.

**Running it permanently (optional, for people comfortable with terminals):**
any tool that keeps a program running works, e.g. `pm2`
(`npm install -g pm2`, then `pm2 start relay/relay.mjs --name DScord`), a
systemd service on Linux, or Task Scheduler on Windows.

### Updating DScord

DScord **doesn't update itself**. Instead it reminds you:

- On the handheld, the start screen (top) shows **Update available: v…**
  with the download address. Older versions show a short notification line
  at the bottom instead.
- If the relay on your computer is the older one, the handheld says so, and
  the relay window prints `Update available: DScord v…`.

(The relay asks GitHub for the latest version when it starts and every 6
hours. To turn that off, add `"updateCheck": false` to `config.json`.)

To update:

1. Download the new `dscord.nds` / `dscord.3dsx` from the
   [Releases page](https://github.com/Ramanathan0310/DScord/releases) and copy
   it over the old file on the SD card, just like in
   [section 10](#10-copy-the-app-to-your-handheld). Your settings are kept.
2. Download the new **Source code (zip)** from the same release page. Copy its
   `relay/relay.mjs` over the one in your DScord folder. **Keep your
   `config.json`**: don't replace or delete it.
3. Stop the relay (Ctrl+C) and start it again.

---

## 14. Optional: log in as yourself instead of a bot

The relay can log in with **your own account's token** instead of a bot's.
Then the DS shows **all your servers and your real DMs**, with no bot needed.

**Read this first:** Discord's Terms of Service do not allow automating a normal
user account ("self-bots"). Discord can disable accounts that do it. That
covers using an account through unofficial software at all, not just abuse or
ban evasion. DScord doesn't disguise itself as the official app. **Only do
this with an account you could afford to lose.**

Also:

- **Never share a relay that's logged in as you.** Anyone with its address and
  PIN can read your DMs and post as you. If friends want to use DScord,
  each person runs their own relay with their own account.
- Your user token is the master key to your account, much more powerful than
  your password: it skips 2FA. Never paste it anywhere except the DScord
  setup. Logging out of Discord everywhere (*User Settings > Devices >
  Log out of all known devices*) or changing your password makes the old token
  stop working.

If you accept the risk, run the setup (`node relay/relay.mjs --setup`, or the
first time you start the relay) and choose **2) Your own user account**:

1. The setup repeats the risks and asks you to type `I understand`.
   Anything else switches back to a bot.
2. It asks for your account's token. That's the long code the Discord website
   sends as the `Authorization` header with each request. Paste it (it shows
   as stars). The setup checks it and says **OK: user "yourname"**. If you
   paste a bot token by mistake, it tells you.
3. There's nothing to invite: all your servers and DMs appear on the handheld.
4. Choose a PIN and port as in [section 5d](#5d-step-4-pin-and-port).

The relay prints a warning each time it starts in this mode. To go back to a
bot, run `--setup` again and choose 1.

---

## Troubleshooting

### On the computer (relay window)

| Message | What it means / what to do |
|---|---|
| `node: command not found` / `'node' is not recognized` | Node.js isn't installed or the terminal was open during install. Install it (section 3) and open a new terminal. |
| `The DScord relay needs Node.js 22 or newer` | Install the current LTS from <https://nodejs.org> (section 3). |
| `No usable config … Run the setup in a terminal` | The relay was started without a window it can ask questions in. Double-click the starter, or run `node relay/relay.mjs --setup` in a terminal. |
| `No token in …config.json` | Run the setup: `node relay/relay.mjs --setup`. |
| `Set a numeric PIN (4+ digits)` | The PIN in `config.json` must be digits only, at least 4 of them, inside quotes. |
| `config.json isn't valid JSON` | A hand edit broke the file (missing comma or quote, curly quotes). Fix it using the example in [section 8](#8-changing-settings-later), or run `--setup` again. |
| `Discord rejected the bot token (4004)` | The token is wrong or was reset. Run `--setup` and paste a fresh one (section 5b). |
| `Message Content intent not enabled (4014)` | Turn on Message Content Intent (section 5c) and click Save. The setup checks this too. |
| `Port 5151 is already in use` | The relay is already running in another window, or another program uses the port. Close the other one, or pick another port with `--setup` (and enter it on the handheld). |
| `Logged in as bot …; 0 servers` | The bot hasn't been invited anywhere yet. Run `--setup` again: it prints the invite link (section 5c). |
| Setup: "that looks like a user token" / "looks like a bot token" | You pasted the other kind of token. For a bot, copy it from the Developer Portal's **Bot** page. |
| Double-clicking the starter does nothing / closes at once | Run it from a terminal instead (`node relay/relay.mjs`) to see the message. On Mac, right-click > Open the first time. |

### On the handheld

| Message | What to do |
|---|---|
| **No Wi-Fi. Set up a connection in a DS game's WFC settings…** | The DS has no working saved connection, or the network uses WPA/WPA2. See section 9. |
| **Wi-Fi isn't connected…** (3DS) | Turn Wi-Fi on (the slider or switch on the side) and check System Settings > Internet Settings. |
| **Can't reach 192.168.x.x:5151** | The relay isn't running, the IP is wrong (check [section 7](#7-the-address-port-and-pin-for-the-handheld) again), the DS and computer are on different networks, or the firewall is blocking it (allow Node.js on *Private networks*). On a guest network, turn off client/AP isolation. |
| **Can't resolve …** | You typed a name instead of an IP, or there's a typo. Use the numeric IP. |
| **Wrong PIN** | Press X and re-enter the PIN exactly as in `config.json`. |
| **The relay didn't answer the login** | The relay is running but not logged in to Discord. Look at the relay window for an error. |
| **Connection to the relay was lost / stopped responding** | The relay was closed, the computer slept, or Wi-Fi dropped. It retries automatically. |
| **Error: 403 Missing Access** (in a chat) | The bot can't read that channel. Give its role "View Channel" and "Read Message History" there. |
| **Error: 403 Missing Permissions** when sending | Give the bot "Send Messages" in that channel. |
| A server or channel is missing | The bot isn't in that server, or its role can't see the channel. |
| No DMs are listed (bot mode) | Bots only have DMs that people send **to the bot**, and the relay only learns about them while it's running. With the relay running, DM the bot from your phone or PC, then refresh with Y. |
| Everything shows as read | In bot mode, the relay treats everything as read when it starts, because bots have no unread tracking. New messages after that are marked. |

### Still stuck?

- Restart everything in order: relay, then Wi-Fi/hotspot, then the app.
- Watch the relay window while connecting. It prints a line when a handheld
  connects (`… connected`), logs in, or uses a wrong PIN. If nothing appears,
  the handheld isn't reaching the computer at all, which means a network or
  firewall problem.

---

## FAQ

**Is this allowed?**
Bot mode uses Discord's official bot system the way it's meant to be used.
User-token mode is against Discord's Terms of Service (see section 14).

**Can my friends use my relay?**
In bot mode, yes. Give them your IP, port and PIN. Everyone sees what the bot
sees and posts as the bot. In user mode, no: see section 14.

**Can I use it away from home?**
The relay is meant for your home network. Don't open the port on your router to
the internet: the handheld connection isn't encrypted, and only the PIN
protects it.

**Does it support voice, reactions or uploading pictures?**
No. It does text chat, DMs, mentions and (on 3DS) viewing pictures.

**Why are some messages cut short or shown as `[file: …]`?**
Attachments other than pictures are shown by name only, and embeds are
shortened to fit the small screen.

**Where are my settings stored?**
DS/DSi: `/dscord.cfg` on the SD card. 3DS: `sdmc:/3ds/dscord.cfg`.
Delete the file to start over with the setup questions.
