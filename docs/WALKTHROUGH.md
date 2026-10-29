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
5. [Create your Discord bot](#5-create-your-discord-bot)
6. [Set up the relay](#6-set-up-the-relay)
7. [Start the relay](#7-start-the-relay)
8. [Find your computer's IP address](#8-find-your-computers-ip-address)
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

## 5. Create your Discord bot

You'll make a free bot account, give it permission to read messages, and
invite it to your server. This happens on Discord's website, not in the app.

### 5a. Create the application

1. Go to the **Discord Developer Portal**:
   <https://discord.com/developers/applications> and log in with your normal
   Discord account.
2. Click **New Application** (top right).
3. Give it a name, such as `My DS`. This becomes the bot's name. Tick the box
   to agree to the terms and click **Create**.

### 5b. Get the bot token

1. In the left menu, click **Bot**.
2. Click **Reset Token**, confirm, and enter your 2FA code if asked.
3. A long string of letters and numbers appears. This is the **bot token**.
   Click **Copy** and paste it somewhere safe for a minute (a Notepad / TextEdit
   window is fine).

> ⚠️ **The token is a password.** Anyone with it can control your bot. Never
> post it in Discord, in screenshots, or on GitHub. If it leaks, come back here
> and click Reset Token again.

### 5c. Turn on Message Content Intent (important!)

Still on the **Bot** page, scroll down to **Privileged Gateway Intents** and:

- turn **on** **Message Content Intent**
- click **Save Changes** at the bottom.

Without this, the relay can't read message text and stops with the error
`4014`.

### 5d. Invite the bot to your server

1. In the left menu, click **OAuth2**. Copy the **Client ID** (a long number).
2. Paste this link into your browser's address bar, but replace
   `YOUR_CLIENT_ID` with the number you copied:

   ```
   https://discord.com/oauth2/authorize?client_id=YOUR_CLIENT_ID&scope=bot&permissions=68608
   ```

3. Pick your server from the list, click **Continue**, then **Authorize**.
4. Open Discord. Your bot should now appear in the server's member list (it will
   look offline until the relay is running).

The `68608` in the link gives the bot permission to **view channels**, **send
messages** and **read message history**. That's all it needs.

> **Only servers you manage.** To add a bot to a server you need the **Manage
> Server** permission there. For other servers, ask an admin to open your
> invite link. The bot only sees channels its roles are allowed to see, the same
> as a person would.

---

## 6. Set up the relay

1. Open the DScord folder, then the `relay` folder.
2. Find the file `config.example.json`. **Make a copy** of it in the same folder
   and name the copy exactly `config.json`.
   - **Windows:** copy and paste the file, then rename it. If you can't see
     `.json` at the end of file names, turn on *View > Show > File name
     extensions* in File Explorer, otherwise you may end up with
     `config.json.json`.
   - **Mac:** select the file, press Cmd+D to duplicate it, then rename it.
3. Open `config.json` in a plain text editor (Notepad on Windows, TextEdit on Mac
   using *Format > Make Plain Text*, or any code editor). It looks like this:

   ```json
   {
     "token": "YOUR_BOT_TOKEN",
     "tokenType": "bot",
     "pin": "1234",
     "port": 5151,
     "host": "0.0.0.0"
   }
   ```

4. Change two things:
   - Replace `YOUR_BOT_TOKEN` with your bot token from step 5b. **Keep the quote
     marks** around it.
   - Replace `1234` with your **own PIN**: 4 or more digits that only you know,
     such as `482915`. You'll type this on the DS once. It stops other people on
     your Wi-Fi from using your relay.
5. Leave the other lines as they are. Save the file.

When you're done it should look something like this (with your own token):

```json
{
  "token": "EXAMPLE.not-a-real-token.paste-yours-here",
  "tokenType": "bot",
  "pin": "482915",
  "port": 5151,
  "host": "0.0.0.0"
}
```

**Common mistakes:** a missing quote mark or comma, curly "smart quotes"
(TextEdit can add these, so use Plain Text mode), or spaces inside the token.

---

## 7. Start the relay

1. Open a terminal (see [section 3](#3-install-nodejs-on-your-computer)).
2. Move into the DScord folder. Type `cd ` (with a space after it), then
   **drag the DScord folder from your file manager into the terminal
   window**. That pastes its location. Press Enter.
3. Start the relay:

   ```bash
   node relay/relay.mjs
   ```

4. After a few seconds you should see:

   ```
   12:00:01 Relay listening on 0.0.0.0:5151
   12:00:01 Gateway: connecting
   12:00:02 Logged in as bot My DS; 1 servers
   ```

   In Discord, your bot now shows as online. 🎉

5. **Leave this window open.** Closing it, or letting the computer sleep, stops
   the relay. To stop it on purpose, click the window and press **Ctrl+C**.

**If your computer asks "Allow incoming connections?"** (Windows Firewall or
the macOS firewall), click **Allow**. On Windows, tick **Private networks**.
Without this, the handheld can't reach the relay.

**If you see an error instead**, check [Troubleshooting](#troubleshooting). The
relay tells you what's wrong, for example a missing token or wrong PIN format.

---

## 8. Find your computer's IP address

The handheld needs your computer's **local IP address**: its address on your
home network, usually something like `192.168.1.23` or `10.0.0.5`.

- **Windows:** in PowerShell, run `ipconfig`. Look for **IPv4 Address** under
  your Wi-Fi or Ethernet adapter.
- **Mac:** System Settings > Wi-Fi > click **Details…** next to your network.
  The **IP address** is listed there. (Or run `ipconfig getifaddr en0` in
  Terminal.)
- **Linux / Pi:** run `hostname -I`. The first address is usually the one.

Write it down. Ignore addresses starting with `127.` (those only point to the
computer itself).

> **Tip:** this address can change after a router restart. If the app suddenly
> can't connect one day, check it again. You can usually make it permanent with
> a "DHCP reservation" in your router's settings.

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

1. **Make sure the relay is running** on your computer (section 7).
2. Start DScord. The first time, it asks three questions using the
   on-screen keyboard:

   | Question | What to enter |
   |---|---|
   | **Relay address** | Your computer's IP from section 8, e.g. `192.168.1.23` |
   | **Relay port** | `5151` (already filled in; just confirm) |
   | **Relay PIN** | The PIN you put in `config.json` |

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
  your password: it skips 2FA. Never paste it anywhere except your own
  `config.json`. Logging out of Discord everywhere (*User Settings > Devices >
  Log out of all known devices*) or changing your password makes the old token
  stop working.

If you accept the risk, set `"tokenType": "user"` in `config.json`, put your
own token in `"token"`, and restart the relay. It prints a warning when it
starts in this mode.

---

## Troubleshooting

### On the computer (relay window)

| Message | What it means / what to do |
|---|---|
| `node: command not found` / `'node' is not recognized` | Node.js isn't installed or the terminal was open during install. Install it (section 3) and open a new terminal. |
| `No token. Copy relay/config.example.json to …` | `config.json` is missing or still has `YOUR_BOT_TOKEN`. Redo section 6. Check the name isn't `config.json.json` or `config.json.txt`. |
| `Set a numeric PIN (4+ digits)` | The PIN in `config.json` must be digits only, at least 4 of them, inside quotes. |
| `SyntaxError: … JSON` | `config.json` has a typo: a missing comma or quote, or curly quotes. Compare it with the example in section 6. |
| `Discord rejected the bot token (4004)` | The token is wrong or was reset. Copy it again (section 5b). If you're using a user token, set `"tokenType": "user"`. |
| `Message Content intent not enabled (4014)` | Turn on Message Content Intent (section 5c) and click Save. |
| `EADDRINUSE` | The relay is already running in another window, or something else uses port 5151. Close the other one, or change `port` in `config.json` (and on the DS). |
| `Logged in as bot …; 0 servers` | The bot hasn't been invited anywhere yet. Do section 5d. |

### On the handheld

| Message | What to do |
|---|---|
| **No Wi-Fi. Set up a connection in a DS game's WFC settings…** | The DS has no working saved connection, or the network uses WPA/WPA2. See section 9. |
| **Wi-Fi isn't connected…** (3DS) | Turn Wi-Fi on (the slider or switch on the side) and check System Settings > Internet Settings. |
| **Can't reach 192.168.x.x:5151** | The relay isn't running, the IP is wrong (check section 8 again), the DS and computer are on different networks, or the firewall is blocking it (allow Node.js on *Private networks*). On a guest network, turn off client/AP isolation. |
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
