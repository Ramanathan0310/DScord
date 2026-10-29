#!/usr/bin/env node
// discord-ds relay: holds the Discord connection (TLS, gateway, REST) and
// speaks the small line protocol in ../PROTOCOL.md to DS / DSi / 3DS clients.
// No dependencies; needs Node 22+ (global fetch and WebSocket).

import net from 'node:net';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));

// ---------------------------------------------------------------- config

function loadConfig() {
  let file = {};
  const p = process.env.RELAY_CONFIG || path.join(HERE, 'config.json');
  if (fs.existsSync(p)) file = JSON.parse(fs.readFileSync(p, 'utf8'));
  const cfg = {
    token: process.env.DISCORD_TOKEN || file.token,
    tokenType: String(process.env.DISCORD_TOKEN_TYPE || file.tokenType || 'bot').toLowerCase(),
    pin: String(process.env.RELAY_PIN || file.pin || ''),
    port: Number(process.env.RELAY_PORT || file.port || 5151),
    host: process.env.RELAY_HOST || file.host || '0.0.0.0',
  };
  if (!cfg.token || cfg.token === 'YOUR_BOT_TOKEN') die(`No token. Copy relay/config.example.json to ${p} and fill in "token" (or set DISCORD_TOKEN).`);
  if (cfg.tokenType !== 'bot' && cfg.tokenType !== 'user') die('"tokenType" must be "bot" or "user".');
  cfg.token = cfg.token.replace(/^Bot\s+/i, '');
  if (cfg.tokenType === 'user') console.warn('WARNING: user-token mode. Automating a user account is against Discord\'s Terms of Service and can get the account disabled. Use at your own risk.');
  if (!/^\d{4,}$/.test(cfg.pin)) die('Set a numeric PIN (4+ digits) in relay/config.json or RELAY_PIN.');
  return cfg;
}

function die(msg) { console.error(msg); process.exit(1); }
function log(...a) { console.log(new Date().toISOString().slice(11, 19), ...a); }

const cfg = loadConfig();

// ---------------------------------------------------------------- discord identity

const USER = cfg.tokenType === 'user';
// Identifies honestly as discord-ds in both modes; no official-client spoofing.
const UA = USER ? 'discord-ds/1.0 (https://github.com/Ramanathan0310/discord-ds)' : 'DiscordBot (https://github.com/Ramanathan0310/discord-ds, 1.0)';
const AUTH = USER ? cfg.token : `Bot ${cfg.token}`;
// GUILDS | GUILD_MESSAGES | DIRECT_MESSAGES | MESSAGE_CONTENT (privileged: enable it
// under Bot -> Privileged Gateway Intents in the Developer Portal).
const INTENTS = (1 << 0) | (1 << 9) | (1 << 12) | (1 << 15);
const API = 'https://discord.com/api/v10';

async function rest(method, route, body) {
  for (let attempt = 0; attempt < 3; attempt++) {
    const res = await fetch(API + route, {
      method,
      headers: {
        Authorization: AUTH,
        'User-Agent': UA,
        ...(body !== undefined ? { 'Content-Type': 'application/json' } : {}),
      },
      body: body !== undefined ? JSON.stringify(body) : undefined,
    });
    if (res.status === 429) {
      const j = await res.json().catch(() => ({}));
      await sleep(Math.min(10, j.retry_after || 1) * 1000);
      continue;
    }
    const text = await res.text();
    const data = text ? JSON.parse(text) : null;
    if (!res.ok) throw new Error(`${res.status} ${data?.message || res.statusText}`);
    return data;
  }
  throw new Error('rate limited');
}

const sleep = ms => new Promise(r => setTimeout(r, ms));

// ---------------------------------------------------------------- state

const state = {
  ready: false,
  me: null,
  users: new Map(),     // id -> user
  guilds: new Map(),    // id -> { id, name, roles: Map, myRoles: [] | null, channels: Set<id> }
  channels: new Map(),  // id -> channel (guild text channels and DMs)
  readState: new Map(), // channel id -> last read message id
};

const PERM_ADMIN = 1n << 3n;
const PERM_VIEW = 1n << 10n;

function addUser(u) { if (u?.id) state.users.set(u.id, { ...state.users.get(u.id), ...u }); }

function addGuild(g, myMember) {
  if (g.unavailable) return;
  const name = g.name ?? g.properties?.name ?? 'Server';
  const roles = new Map((g.roles || []).map(r => [r.id, r]));
  const old = state.guilds.get(g.id);
  const guild = { id: g.id, name, roles, myRoles: myMember?.roles ?? old?.myRoles ?? null, channels: new Set() };
  state.guilds.set(g.id, guild);
  for (const c of g.channels || []) addChannel({ ...c, guild_id: g.id });
  for (const t of g.threads || []) addChannel({ ...t, guild_id: g.id });
}

async function onGuildCreate(g) {
  addGuild(g, (g.members || []).find(mm => mm.user?.id === state.me?.id));
  // Bots have no server-side read state: treat everything as read at startup.
  for (const c of g.channels || []) if (c.last_message_id && !state.readState.has(c.id)) state.readState.set(c.id, c.last_message_id);
  const guild = state.guilds.get(g.id);
  if (guild && !guild.myRoles) {
    const route = USER ? `/users/@me/guilds/${g.id}/member` : `/guilds/${g.id}/members/${state.me.id}`;
    try { guild.myRoles = (await rest('GET', route)).roles; }
    catch (e) { log(`Couldn't read own roles in ${guild.name}: ${e.message}`); }
  }
}

function addChannel(c) {
  state.channels.set(c.id, { ...state.channels.get(c.id), ...c });
  if (c.guild_id) state.guilds.get(c.guild_id)?.channels.add(c.id);
  for (const u of c.recipients || []) addUser(u);
}

function removeChannel(id) {
  const c = state.channels.get(id);
  if (c?.guild_id) state.guilds.get(c.guild_id)?.channels.delete(id);
  state.channels.delete(id);
}

function canView(guild, ch) {
  if (!guild.myRoles) return true; // role data unknown: show everything
  const me = state.me.id;
  const everyone = guild.roles.get(guild.id);
  let perms = BigInt(everyone?.permissions ?? 0);
  for (const rid of guild.myRoles) perms |= BigInt(guild.roles.get(rid)?.permissions ?? 0);
  if (perms & PERM_ADMIN) return true;
  const ows = ch.permission_overwrites || [];
  const apply = o => { if (o) perms = (perms & ~BigInt(o.deny)) | BigInt(o.allow); };
  apply(ows.find(o => o.id === guild.id));
  let allow = 0n, deny = 0n;
  for (const o of ows) if (o.type === 0 && o.id !== guild.id && guild.myRoles.includes(o.id)) { allow |= BigInt(o.allow); deny |= BigInt(o.deny); }
  perms = (perms & ~deny) | allow;
  apply(ows.find(o => o.type === 1 && o.id === me));
  return (perms & PERM_VIEW) !== 0n;
}

function isUnread(ch) {
  if (!ch.last_message_id) return false;
  const read = state.readState.get(ch.id);
  if (!read) return false;
  return BigInt(ch.last_message_id) > BigInt(read);
}

function displayName(u, member) {
  return member?.nick || u?.global_name || u?.username || 'unknown';
}

function dmName(ch) {
  if (ch.name) return ch.name;
  const ids = ch.recipient_ids || (ch.recipients || []).map(r => r.id);
  const names = ids.map(id => displayName(state.users.get(id))).filter(Boolean);
  return names.join(', ') || 'DM';
}

// ---------------------------------------------------------------- text

const EMOJI = {
  '😀': ':grinning:', '😃': ':smiley:', '😄': ':smile:', '😁': ':grin:', '😆': ':laughing:',
  '😅': ':sweat_smile:', '🤣': ':rofl:', '😂': ':joy:', '🙂': ':)', '😉': ';)', '😊': ':blush:',
  '😍': ':heart_eyes:', '😘': ':kiss:', '😛': ':P', '😎': ':sunglasses:', '🤔': ':thinking:',
  '😐': ':|', '😑': ':expressionless:', '🙄': ':eyeroll:', '😏': ':smirk:', '😢': ":'(",
  '😭': ':sob:', '😡': ':rage:', '😠': ':angry:', '😮': ':o', '😱': ':scream:', '😴': ':sleeping:',
  '💀': ':skull:', '👍': ':+1:', '👎': ':-1:', '👏': ':clap:', '🙏': ':pray:', '💯': ':100:',
  '🔥': ':fire:', '✨': ':sparkles:', '🎉': ':tada:', '❤': '<3', '💔': '</3', '👀': ':eyes:',
  '✅': ':white_check_mark:', '❌': ':x:', '⭐': ':star:', '😳': ':flushed:', '🥺': ':pleading:',
  '🤡': ':clown:', '😤': ':triumph:', '🫡': ':salute:', '🗿': ':moyai:', '👋': ':wave:',
};
const PUNCT = {
  '‘': "'", '’': "'", '‚': "'", '“': '"', '”': '"', '„': '"', '–': '-', '—': '-', '…': '...',
  '•': '*', '·': '*', '×': 'x', '÷': '/', '€': 'EUR', '£': 'GBP', '¥': 'JPY', '©': '(c)',
  '®': '(r)', '™': '(tm)', '→': '->', '←': '<-', '↑': '^', '↓': 'v', '«': '<<', '»': '>>',
  'ß': 'ss', 'æ': 'ae', 'Æ': 'AE', 'ø': 'o', 'Ø': 'O', 'đ': 'd', 'Đ': 'D', 'ł': 'l', 'Ł': 'L',
  ' ': ' ', '​': '', '‍': '', '️': '',
};

// Reduce text to printable ASCII so the handheld's 8x8 font can draw it.
function asciify(s) {
  let out = '';
  for (const ch of s) {
    const code = ch.codePointAt(0);
    if (code === 10 || (code >= 32 && code < 127)) { out += ch; continue; }
    if (EMOJI[ch]) { out += EMOJI[ch]; continue; }
    if (PUNCT[ch] !== undefined) { out += PUNCT[ch]; continue; }
    if (code >= 0x1f3fb && code <= 0x1f3ff) continue; // skin tone modifiers
    const base = ch.normalize('NFKD').replace(/[̀-ͯ]/g, '');
    if (/^[\x20-\x7e]+$/.test(base)) { out += base; continue; }
    out += code >= 0x1f000 || (code >= 0x2600 && code < 0x2800) ? '*' : '?';
  }
  return out;
}

function fmtTime(iso) {
  const d = new Date(iso);
  const now = new Date();
  const pad = n => String(n).padStart(2, '0');
  if (d.toDateString() === now.toDateString()) return `${pad(d.getHours())}:${pad(d.getMinutes())}`;
  return `${pad(d.getMonth() + 1)}/${pad(d.getDate())}`;
}

function resolveMentions(text, m) {
  const ch = state.channels.get(m.channel_id);
  const guild = ch?.guild_id ? state.guilds.get(ch.guild_id) : null;
  return text
    .replace(/<@!?(\d+)>/g, (_, id) => {
      const u = (m.mentions || []).find(x => x.id === id) || state.users.get(id);
      return '@' + displayName(u, u?.member);
    })
    .replace(/<@&(\d+)>/g, (_, id) => '@' + (guild?.roles.get(id)?.name ?? 'role'))
    .replace(/<#(\d+)>/g, (_, id) => '#' + (state.channels.get(id)?.name ?? 'channel'))
    .replace(/<a?:(\w+):\d+>/g, ':$1:')
    .replace(/<t:(\d+)(?::\w)?>/g, (_, t) => new Date(Number(t) * 1000).toLocaleString('en-US', { dateStyle: 'short', timeStyle: 'short' }))
    .replace(/<\/([\w -]+):\d+>/g, '/$1');
}

const SYSTEM_TYPES = {
  1: 'added someone to the group', 2: 'removed someone from the group', 3: 'started a call',
  4: 'changed the channel name', 5: 'changed the channel icon', 6: 'pinned a message',
  7: 'joined the server', 8: 'boosted the server', 18: 'started a thread',
};

// Images per message, so IMG requests can be answered after the fact.
const imageCache = new Map();
function rememberImages(id, imgs) {
  imageCache.delete(id);
  imageCache.set(id, imgs);
  if (imageCache.size > 1000) imageCache.delete(imageCache.keys().next().value);
}

function isImage(a) {
  return (a.content_type || '').startsWith('image/') || /\.(png|jpe?g|gif|webp)$/i.test(a.filename || '');
}

function messageFields(m) {
  const author = displayName(m.author, m.member);
  let text = m.content || '';
  if (SYSTEM_TYPES[m.type] && !text) text = `* ${SYSTEM_TYPES[m.type]}`;
  text = resolveMentions(text, m);
  if (m.referenced_message) {
    const r = m.referenced_message;
    const snip = resolveMentions(r.content || '[attachment]', r).replace(/\n/g, ' ').slice(0, 40);
    text = `[re @${displayName(r.author, r.member)}: ${snip}]\n` + text;
  }
  const imgs = [];
  const extra = [];
  for (const a of m.attachments || []) {
    if (isImage(a)) imgs.push({ url: a.proxy_url || a.url, w: a.width, h: a.height }), extra.push(`[img ${imgs.length}: ${a.filename}]`);
    else extra.push(`[file: ${a.filename}]`);
  }
  for (const e of m.embeds || []) {
    const pic = e.image || (e.type === 'image' || e.type === 'gifv' ? e.thumbnail : null);
    if (pic?.proxy_url || pic?.url) {
      imgs.push({ url: pic.proxy_url || pic.url, w: pic.width, h: pic.height });
      if (e.type === 'image' || e.type === 'gifv') { extra.push(`[img ${imgs.length}]`); continue; }
    }
    if (e.title || e.description) extra.push(`[${e.title || ''}${e.title && e.description ? ': ' : ''}${(e.description || '').slice(0, 120)}]`);
  }
  for (const s of m.sticker_items || []) extra.push(`[sticker: ${s.name}]`);
  if (extra.length) text += (text ? '\n' : '') + extra.join('\n');
  if (imgs.length) rememberImages(m.id, imgs);

  const me = state.me?.id;
  let flags = 0;
  if (m.mention_everyone || (m.mentions || []).some(u => u.id === me)) flags |= 1;
  if (m.edited_timestamp) flags |= 2;
  if (m.author?.id === me) flags |= 4;
  return [m.channel_id, m.id, asciify(author), fmtTime(m.timestamp), imgs.length, flags, asciify(text)];
}

// ---------------------------------------------------------------- gateway

let ws = null, seq = null, sessionId = null, resumeUrl = null, hbTimer = null, acked = true;

function gatewayConnect() {
  const resume = !!sessionId;
  const url = (resume && resumeUrl ? resumeUrl : 'wss://gateway.discord.gg') + '/?v=10&encoding=json';
  log(resume ? 'Gateway: resuming' : 'Gateway: connecting');
  ws = new WebSocket(url);
  ws.addEventListener('message', ev => onGateway(JSON.parse(ev.data), resume));
  ws.addEventListener('close', ev => {
    clearInterval(hbTimer);
    log(`Gateway closed (${ev.code}) ${ev.reason || ''}`);
    if (ev.code === 4004) die(`Discord rejected the ${cfg.tokenType} token (4004). Check "token" and "tokenType" in relay/config.json.`);
    if (ev.code === 4014) die('Message Content intent not enabled (4014). Turn it on in the Developer Portal under Bot.');
    if ([4010, 4011, 4012, 4013].includes(ev.code)) die(`Fatal gateway close ${ev.code}`);
    if (ev.code === 4007 || ev.code === 4009) sessionId = null;
    setTimeout(gatewayConnect, 2000 + Math.random() * 3000);
  });
  ws.addEventListener('error', () => {});
}

function gwSend(op, d) { if (ws?.readyState === WebSocket.OPEN) ws.send(JSON.stringify({ op, d })); }

function onGateway(p, resume) {
  if (p.s != null) seq = p.s;
  switch (p.op) {
    case 10: // HELLO
      acked = true;
      clearInterval(hbTimer);
      hbTimer = setInterval(() => {
        if (!acked) { ws.close(4000, 'zombie'); return; }
        acked = false;
        gwSend(1, seq);
      }, p.d.heartbeat_interval);
      if (resume) gwSend(6, { token: cfg.token, session_id: sessionId, seq });
      else gwSend(2, {
        token: cfg.token,
        // User accounts get every event for their own servers; intents are a bot concept.
        ...(USER ? {} : { intents: INTENTS }),
        properties: { os: process.platform, browser: 'discord-ds', device: 'discord-ds' },
      });
      break;
    case 11: acked = true; break;
    case 1: gwSend(1, seq); break;
    case 7: ws.close(4000, 'reconnect requested'); break;
    case 9: // invalid session
      if (!p.d) { sessionId = null; seq = null; }
      setTimeout(() => ws.close(4000, 'invalid session'), 1000 + Math.random() * 4000);
      break;
    case 0: onDispatch(p.t, p.d); break;
  }
}

// A user account's READY already carries its servers, DMs and read state
// (bots get those through GUILD_CREATE instead).
function onUserReady(d) {
  for (const u of d.users || []) addUser(u);
  const rs = Array.isArray(d.read_state) ? d.read_state : d.read_state?.entries || [];
  for (const r of rs) if (r.id && r.last_message_id) state.readState.set(r.id, String(r.last_message_id));
  (d.guilds || []).forEach((g, i) => {
    const mine = (d.merged_members?.[i] || []).find(mm => (mm.user_id || mm.user?.id) === d.user.id);
    addGuild(g, mine);
  });
  for (const c of d.private_channels || []) {
    addChannel(c);
    if (c.last_message_id && !state.readState.has(c.id)) state.readState.set(c.id, c.last_message_id);
  }
}

function onDispatch(t, d) {
  switch (t) {
    case 'READY': {
      sessionId = d.session_id;
      resumeUrl = d.resume_gateway_url;
      state.me = d.user;
      addUser(d.user);
      if (USER) onUserReady(d);
      state.ready = true;
      log(`Logged in as ${USER ? 'user' : 'bot'} ${d.user.username}; ${(d.guilds || []).length} servers`);
      break;
    }
    case 'RESUMED': log('Gateway resumed'); break;
    case 'GUILD_CREATE': onGuildCreate(d); break;
    case 'GUILD_UPDATE': {
      const g = state.guilds.get(d.id);
      if (g) { g.name = d.name ?? g.name; if (d.roles) g.roles = new Map(d.roles.map(r => [r.id, r])); }
      break;
    }
    case 'GUILD_DELETE': if (!d.unavailable) state.guilds.delete(d.id); break;
    case 'GUILD_ROLE_CREATE': case 'GUILD_ROLE_UPDATE':
      state.guilds.get(d.guild_id)?.roles.set(d.role.id, d.role); break;
    case 'GUILD_ROLE_DELETE': state.guilds.get(d.guild_id)?.roles.delete(d.role_id); break;
    case 'GUILD_MEMBER_UPDATE':
      if (d.user?.id === state.me?.id) { const g = state.guilds.get(d.guild_id); if (g) g.myRoles = d.roles; }
      break;
    case 'CHANNEL_CREATE': case 'CHANNEL_UPDATE': addChannel(d); break;
    case 'CHANNEL_DELETE': removeChannel(d.id); break;
    case 'MESSAGE_CREATE': {
      addUser(d.author);
      let ch = state.channels.get(d.channel_id);
      if (!ch && !d.guild_id) addChannel({ id: d.channel_id, type: 1, recipient_ids: [d.author.id] }), ch = state.channels.get(d.channel_id);
      if (ch) ch.last_message_id = d.id;
      if (d.author.id === state.me.id) state.readState.set(d.channel_id, d.id);
      const fields = messageFields(d);
      for (const c of clients) if (c.sub === d.channel_id) c.send('N', ...fields);
      const mentioned = fields[5] & 1;
      const isDm = !d.guild_id;
      if ((mentioned || isDm) && d.author.id !== state.me.id) {
        const where = isDm ? 'DM' : '#' + (ch?.name ?? '?');
        const note = asciify(`${displayName(d.author, d.member)} (${where}): ${(d.content || '[attachment]').replace(/\n/g, ' ')}`).slice(0, 120);
        for (const c of clients) if (c.authed && c.sub !== d.channel_id) c.send('NOTE', note);
      }
      break;
    }
    case 'MESSAGE_UPDATE':
      if (d.author && d.content !== undefined) for (const c of clients) if (c.sub === d.channel_id) c.send('U', ...messageFields(d));
      break;
    case 'MESSAGE_DELETE':
      for (const c of clients) if (c.sub === d.channel_id) c.send('X', d.channel_id, d.id);
      break;
  }
}

// ---------------------------------------------------------------- clients

const clients = new Set();
const PLATFORMS = ['DS', 'DSi', '3DS'];
const esc = s => String(s).replace(/\\/g, '\\\\').replace(/\n/g, '\\n').replace(/\t/g, '\\t');
const unesc = s => s.replace(/\\(.)/g, (_, c) => (c === 'n' ? '\n' : c === 't' ? '\t' : c));

class Client {
  constructor(sock) {
    this.sock = sock;
    this.authed = false;
    this.sub = null;
    this.buf = '';
    this.name = `${sock.remoteAddress}:${sock.remotePort}`;
    this.queue = Promise.resolve();
    sock.setNoDelay(true);
    sock.setKeepAlive(true, 30000);
    sock.on('data', chunk => this.onData(chunk));
    sock.on('close', () => { clients.delete(this); log(`${this.name} disconnected`); });
    sock.on('error', () => {});
  }

  send(...fields) { if (!this.sock.destroyed) this.sock.write(fields.map(esc).join('\t') + '\n'); }

  onData(chunk) {
    this.buf += chunk.toString('latin1');
    if (this.buf.length > 65536) { this.sock.destroy(); return; }
    let i;
    while ((i = this.buf.indexOf('\n')) >= 0) {
      const line = this.buf.slice(0, i).replace(/\r$/, '');
      this.buf = this.buf.slice(i + 1);
      const fields = Buffer.from(line, 'latin1').toString('utf8').split('\t').map(unesc);
      // Commands run one at a time so replies stay in order.
      this.queue = this.queue.then(() => this.handle(fields)).catch(e => this.send('ERR', e.message.slice(0, 100)));
    }
  }

  async handle([cmd, ...a]) {
    if (!this.authed) {
      if (cmd !== 'HELLO') return this.send('ERR', 'say HELLO first');
      if (a[2] !== cfg.pin) { this.send('ERR', 'Wrong PIN'); log(`${this.name} wrong PIN`); return void this.sock.end(); }
      this.platform = Number(a[1]) || 0;
      while (!state.ready) await sleep(250);
      this.authed = true;
      clients.add(this);
      log(`${this.name} logged in (${PLATFORMS[this.platform] ?? '?'})`);
      return this.send('OK', asciify(displayName(state.me)));
    }
    switch (cmd) {
      case 'PING': return this.send('PONG');
      case 'GUILDS': {
        const guilds = [...state.guilds.values()].sort((x, y) => x.name.localeCompare(y.name));
        for (const g of guilds) {
          const unread = [...g.channels].some(id => { const c = state.channels.get(id); return c && (c.type === 0 || c.type === 5) && isUnread(c) && canView(g, c); });
          this.send('G', g.id, asciify(g.name), unread ? 1 : 0);
        }
        return this.send('END', 'G');
      }
      case 'CHANNELS': {
        const g = state.guilds.get(a[0]);
        if (!g) return this.send('END', 'C');
        const all = [...g.channels].map(id => state.channels.get(id)).filter(Boolean);
        const text = all.filter(c => (c.type === 0 || c.type === 5) && canView(g, c));
        const byPos = (x, y) => (x.position ?? 0) - (y.position ?? 0) || x.id.localeCompare(y.id);
        const cats = all.filter(c => c.type === 4).sort(byPos);
        for (const c of text.filter(c => !c.parent_id || !cats.some(k => k.id === c.parent_id)).sort(byPos))
          this.send('C', c.id, asciify(c.name), isUnread(c) ? 1 : 0, 0);
        for (const k of cats) {
          const kids = text.filter(c => c.parent_id === k.id).sort(byPos);
          if (!kids.length) continue;
          this.send('C', k.id, asciify(k.name).toUpperCase(), 0, 1);
          for (const c of kids) this.send('C', c.id, asciify(c.name), isUnread(c) ? 1 : 0, 0);
        }
        return this.send('END', 'C');
      }
      case 'DMS': {
        const dms = [...state.channels.values()].filter(c => c.type === 1 || c.type === 3)
          .sort((x, y) => (BigInt(y.last_message_id || 0) > BigInt(x.last_message_id || 0) ? 1 : -1)).slice(0, 60);
        for (const c of dms) this.send('D', c.id, asciify(dmName(c)), isUnread(c) ? 1 : 0);
        return this.send('END', 'D');
      }
      case 'HIST': {
        const [chan, before] = a;
        this.sub = chan;
        const q = before && before !== '0' ? `&before=${before}` : '';
        const msgs = await rest('GET', `/channels/${chan}/messages?limit=20${q}`);
        for (const m of msgs.slice().reverse()) this.send('M', ...messageFields(m));
        this.send('END', 'M', chan, msgs.length === 20 ? 1 : 0);
        if (!q && msgs[0]) state.readState.set(chan, msgs[0].id);
        return;
      }
      case 'SEND': {
        const [chan, text] = a;
        if (!text?.trim()) return;
        await rest('POST', `/channels/${chan}/messages`, { content: text.slice(0, 2000) });
        return this.send('SENT');
      }
      case 'IMG': return this.sendImage(a[0], Number(a[1]) || 0, Number(a[2]) || 400, Number(a[3]) || 240);
      default: return this.send('ERR', `unknown command ${cmd}`);
    }
  }

  async sendImage(msgId, idx, maxw, maxh) {
    const imgs = imageCache.get(msgId);
    const img = imgs?.[idx];
    if (!img) return this.send('ERR', 'image not found');
    let url = img.url;
    if (/media\.discordapp\.net|images-ext/.test(url) && img.w && img.h) {
      const s = Math.min(1, maxw / img.w, maxh / img.h);
      url += (url.includes('?') ? '&' : '?') + `width=${Math.max(1, Math.round(img.w * s))}&height=${Math.max(1, Math.round(img.h * s))}`;
    }
    let res = await fetch(url, { headers: { 'User-Agent': UA } });
    if (/webp/.test(res.headers.get('content-type') || '')) {
      res = await fetch(url + (url.includes('?') ? '&' : '?') + 'format=png', { headers: { 'User-Agent': UA } });
    }
    if (!res.ok) throw new Error(`image ${res.status}`);
    const buf = Buffer.from(await res.arrayBuffer());
    if (buf.length > 2 * 1024 * 1024) throw new Error('image too large');
    this.send('BLOB', msgId, idx, imgs.length, buf.length);
    this.sock.write(buf);
  }
}

// ---------------------------------------------------------------- main

net.createServer(sock => {
  const c = new Client(sock);
  log(`${c.name} connected`);
}).listen(cfg.port, cfg.host, () => log(`Relay listening on ${cfg.host}:${cfg.port}`));

gatewayConnect();
