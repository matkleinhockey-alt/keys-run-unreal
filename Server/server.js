// Keys Run online server.
//
// Players log in with a name and a crew code. Everyone in the same crew sees each other's boats live and shares
// a crew leaderboard (biggest fish and best scores). There are no accounts or passwords: the crew code is what
// keeps your group together, so pick one your friends know and strangers won't guess.
//
// Protocol: JSON text messages over one WebSocket.
//   client -> server  {t:"hello", name, crew, boat, v:1}
//                     {t:"s", x, z, h, y, p, r, v, b, on}          your boat, about 10 times a second
//                     {t:"catch", key, w, boat, score}             a landed fish (for the crew board)
//                     {t:"score", score}                           a new best session score
//   server -> client  {t:"welcome", id, name, crew}
//                     {t:"error", msg}
//                     {t:"peers", list:[{id, name, b, x, z, h, y, p, r, v, on}]}   everyone else in your crew, 10 times a second
//                     {t:"join", name} / {t:"leave", name}
//                     {t:"toast", msg}
//                     {t:"board", catches:[{name, key, w, boat, date}], scores:[{name, score}]}
//
// Run: npm install, then npm start. PORT sets the port (default 8787). Crew boards are saved to data.json.

const http = require("http");
const fs = require("fs");
const path = require("path");
const { WebSocketServer } = require("ws");

const PORT = Number(process.env.PORT) || 8787;
const DATA = process.env.DATA_FILE || path.join(__dirname, "data.json");
const TICK_MS = 100;
const MAX_TOP = 15;
const MAX_PER_CREW = 16;

// ---- crew boards, saved to disk
let boards = {};
try { boards = JSON.parse(fs.readFileSync(DATA, "utf8")); } catch { boards = {}; }
let saveTimer = null;
function saveSoon() {
  if (saveTimer) return;
  saveTimer = setTimeout(() => {
    saveTimer = null;
    fs.writeFile(DATA, JSON.stringify(boards), () => {});
  }, 2000);
}
function boardFor(crew) {
  if (!boards[crew]) boards[crew] = { catches: [], scores: {} };
  return boards[crew];
}
function boardMsg(crew) {
  const b = boardFor(crew);
  const scores = Object.entries(b.scores).map(([name, score]) => ({ name, score })).sort((a, c) => c.score - a.score).slice(0, MAX_TOP);
  return JSON.stringify({ t: "board", catches: b.catches, scores });
}

// ---- input cleaning
const clean = (s, max) => String(s ?? "").replace(/[\u0000-\u001f\u007f"\\<>]/g, "").trim().slice(0, max);
const num = (v, lo, hi) => { const n = Number(v); return Number.isFinite(n) ? Math.min(hi, Math.max(lo, n)) : 0; };

// ---- live crews
const crews = new Map();   // crew code -> Set of clients
let nextId = 1;

function send(ws, obj) { if (ws.readyState === 1) ws.send(typeof obj === "string" ? obj : JSON.stringify(obj)); }
function toCrew(crew, obj, except) {
  const set = crews.get(crew);
  if (!set) return;
  const msg = typeof obj === "string" ? obj : JSON.stringify(obj);
  for (const c of set) if (c !== except) send(c, msg);
}

const server = http.createServer((req, res) => {
  // a plain page so hosting health checks (and you, in a browser) can see it's up
  let players = 0;
  for (const s of crews.values()) players += s.size;
  res.writeHead(200, { "Content-Type": "text/plain" });
  res.end(`Keys Run server is running. ${players} on the water.\n`);
});

const wss = new WebSocketServer({ server, maxPayload: 4096 });

wss.on("connection", (ws) => {
  ws.id = String(nextId++);
  ws.alive = true;
  ws.budget = 60;   // messages allowed per second
  ws.on("pong", () => { ws.alive = true; });

  ws.on("message", (raw) => {
    if (--ws.budget < 0) return;   // flooding: drop it
    let m;
    try { m = JSON.parse(raw.toString()); } catch { return; }
    if (!m || typeof m.t !== "string") return;

    if (m.t === "hello") {
      if (ws.crew) return;
      const crew = clean(m.crew, 24).toLowerCase();
      let name = clean(m.name, 20);
      if (!name) return send(ws, { t: "error", msg: "Pick a name first." });
      if (!crew) return send(ws, { t: "error", msg: "Pick a crew code - your friends type the same one." });
      const set = crews.get(crew) || new Set();
      if (set.size >= MAX_PER_CREW) return send(ws, { t: "error", msg: `Crew "${crew}" is full (${MAX_PER_CREW} boats).` });
      // two people called Mat: the second becomes Mat 2
      const taken = new Set([...set].map((c) => c.name.toLowerCase()));
      if (taken.has(name.toLowerCase())) { let k = 2; while (taken.has(`${name} ${k}`.toLowerCase())) k++; name = `${name} ${k}`; }
      ws.name = name;
      ws.crew = crew;
      ws.state = { b: clean(m.boat, 24), x: 0, z: 0, h: 0, y: 0, p: 0, r: 0, v: 0, on: false };
      set.add(ws);
      crews.set(crew, set);
      send(ws, { t: "welcome", id: ws.id, name, crew });
      send(ws, boardMsg(crew));
      toCrew(crew, { t: "join", name }, ws);
      console.log(`${name} joined crew "${crew}" (${set.size} on)`);
      return;
    }
    if (!ws.crew) return;

    if (m.t === "s") {
      const s = ws.state;
      s.x = num(m.x, -1e6, 1e6); s.z = num(m.z, -1e6, 1e6); s.h = num(m.h, -1e3, 1e3);
      s.y = num(m.y, -50, 50); s.p = num(m.p, -2, 2); s.r = num(m.r, -2, 2); s.v = num(m.v, -100, 100);
      if (m.b) s.b = clean(m.b, 24);
      s.on = !!m.on;
      return;
    }
    if (m.t === "catch") {
      const key = clean(m.key, 32), w = Math.round(num(m.w, 0, 5000) * 10) / 10;
      if (!key || !w) return;
      const b = boardFor(ws.crew);
      b.catches.push({ name: ws.name, key, w, boat: clean(m.boat, 48), date: new Date().toISOString().slice(0, 10) });
      b.catches.sort((a, c) => c.w - a.w);
      b.catches.length = Math.min(b.catches.length, MAX_TOP);
      const score = Math.round(num(m.score, 0, 1e12));
      if (score > (b.scores[ws.name] || 0)) b.scores[ws.name] = score;
      saveSoon();
      toCrew(ws.crew, { t: "catch", name: ws.name, key, w }, ws);
      toCrew(ws.crew, boardMsg(ws.crew));
      return;
    }
    if (m.t === "score") {
      const b = boardFor(ws.crew), score = Math.round(num(m.score, 0, 1e12));
      if (score > (b.scores[ws.name] || 0)) { b.scores[ws.name] = score; saveSoon(); toCrew(ws.crew, boardMsg(ws.crew)); }
    }
  });

  ws.on("close", () => {
    if (!ws.crew) return;
    const set = crews.get(ws.crew);
    if (set) { set.delete(ws); if (!set.size) crews.delete(ws.crew); }
    toCrew(ws.crew, { t: "leave", name: ws.name });
    console.log(`${ws.name} left crew "${ws.crew}"`);
  });
});

// 10 times a second: send every crew its boats
setInterval(() => {
  for (const set of crews.values()) {
    const list = [...set].map((c) => ({ id: c.id, name: c.name, ...c.state }));
    const msg = JSON.stringify({ t: "peers", list });
    for (const c of set) send(c, msg);
  }
}, TICK_MS);

// refill message budgets, and drop connections that stopped answering
setInterval(() => { for (const c of wss.clients) c.budget = 60; }, 1000);
setInterval(() => {
  for (const c of wss.clients) {
    if (!c.alive) { c.terminate(); continue; }
    c.alive = false;
    c.ping();
  }
}, 20000);

server.listen(PORT, () => console.log(`Keys Run server on port ${PORT}`));
