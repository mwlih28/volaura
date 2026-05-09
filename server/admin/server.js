// =====================================================================
//  VoLaura Admin / Update Server
// ---------------------------------------------------------------------
//  Endpoints:
//    Public:
//      GET  /api/version            → en son sürüm manifesti (JSON)
//      GET  /downloads/:filename    → installer indirme
//      GET  /api/notifications      → kullanıcılara push edilen bildirim listesi
//
//    Admin (Bearer token korumalı):
//      POST /api/admin/login        → token doğrula → cookie set
//      POST /api/admin/logout
//      GET  /api/admin/releases     → tüm sürümler
//      POST /api/admin/releases     → yeni sürüm yükle (multipart: file + meta)
//      DEL  /api/admin/releases/:v
//      POST /api/admin/release/:v/promote → bu sürümü "latest" yap
//      GET  /api/admin/notifications
//      POST /api/admin/notifications → broadcast bildirim ekle
//      DEL  /api/admin/notifications/:id
//      GET  /api/admin/users        → (signaling backend bağlıysa) kullanıcı listesi
//      GET  /api/admin/messages     → (signaling backend bağlıysa) mesaj listesi
//
//    Static:
//      /admin                       → admin panel SPA (login + dashboard)
//
//  Veri kalıcılığı: dosya tabanlı JSON (releases.json, notifications.json).
//  Üretim için PostgreSQL/SQLite eklenebilir; şu an basit ve self-contained.
// =====================================================================

require('dotenv').config();

const express   = require('express');
const path      = require('path');
const fs        = require('fs');
const fsp       = require('fs/promises');
const crypto    = require('crypto');
const multer    = require('multer');
const cookieP   = require('cookie-parser');

const PORT          = process.env.PORT || 3000;
const ADMIN_TOKEN   = process.env.ADMIN_TOKEN || '';
const PUBLIC_URL    = process.env.PUBLIC_URL || `http://localhost:${PORT}`;
const ROOT          = __dirname;
const RELEASES_DIR  = path.join(ROOT, 'releases');
const RELEASES_DB   = path.join(ROOT, 'releases.json');
const NOTIFS_DB     = path.join(ROOT, 'notifications.json');

if (!ADMIN_TOKEN || ADMIN_TOKEN === 'changeme_64_character_random_string') {
  console.warn('[!] UYARI: ADMIN_TOKEN .env dosyasında değiştirilmemiş.');
  console.warn('    Üretim için: openssl rand -hex 32');
}

fs.mkdirSync(RELEASES_DIR, { recursive: true });

// ----- Persistent storage (basit JSON) -----
function loadJson(file, fallback) {
  try { return JSON.parse(fs.readFileSync(file, 'utf8')); }
  catch { return fallback; }
}
function saveJson(file, data) {
  fs.writeFileSync(file, JSON.stringify(data, null, 2));
}

// Releases şeması:
// { latest: "1.2.0",
//   versions: { "1.2.0": { version, notes, filename, sha256, size, uploadedAt, isLatest } } }
let releases = loadJson(RELEASES_DB, { latest: null, versions: {} });
// Notifications şeması (broadcast):
// [{ id, type, title, body, createdAt, expiresAt }]
let notifications = loadJson(NOTIFS_DB, []);

// ----- App -----
const app = express();
app.set('trust proxy', 1);
app.use(express.json({ limit: '2mb' }));
app.use(cookieP());

// CORS — gerekirse
app.use((req, res, next) => {
  res.set('Access-Control-Allow-Origin', '*');
  res.set('Access-Control-Allow-Headers', 'Content-Type,Authorization');
  res.set('Access-Control-Allow-Methods', 'GET,POST,DELETE,OPTIONS');
  if (req.method === 'OPTIONS') return res.sendStatus(204);
  next();
});

// ----- Public API -----

// GET /api/version → istemcinin update_checker.cpp'sinin çağırdığı endpoint
app.get('/api/version', (req, res) => {
  if (!releases.latest || !releases.versions[releases.latest]) {
    return res.status(404).json({ error: 'no_release' });
  }
  const r = releases.versions[releases.latest];
  res.json({
    latest: r.version,
    notes:  r.notes || '',
    url:    `${PUBLIC_URL}/downloads/${r.filename}`,
    sha256: r.sha256 || '',
    size:   r.size  || 0,
    publishedAt: r.uploadedAt
  });
});

// GET /downloads/:filename
app.get('/downloads/:file', (req, res) => {
  const safe = path.basename(req.params.file);
  const fp = path.join(RELEASES_DIR, safe);
  if (!fp.startsWith(RELEASES_DIR) || !fs.existsSync(fp)) return res.sendStatus(404);
  res.download(fp, safe);
});

// GET /api/notifications → app içi bildirim merkezi için (gelecekte client poll edebilir)
app.get('/api/notifications', (_req, res) => {
  const now = Date.now();
  const active = notifications.filter(n => !n.expiresAt || n.expiresAt > now);
  res.json(active);
});

// ----- Auth helpers -----
function isAdmin(req) {
  const t = req.cookies?.volaura_admin
    || (req.headers.authorization || '').replace(/^Bearer\s+/i, '');
  return ADMIN_TOKEN && t && crypto.timingSafeEqual(
    Buffer.from(t.padEnd(ADMIN_TOKEN.length, ' ').slice(0, ADMIN_TOKEN.length)),
    Buffer.from(ADMIN_TOKEN));
}
function requireAdmin(req, res, next) {
  if (!isAdmin(req)) return res.status(401).json({ error: 'unauthorized' });
  next();
}

// ----- Admin auth -----
app.post('/api/admin/login', (req, res) => {
  const { token } = req.body || {};
  if (!ADMIN_TOKEN || token !== ADMIN_TOKEN) {
    return res.status(401).json({ error: 'invalid_token' });
  }
  res.cookie('volaura_admin', ADMIN_TOKEN, {
    httpOnly: true, sameSite: 'lax',
    secure: req.protocol === 'https',
    maxAge: 1000 * 60 * 60 * 8
  });
  res.json({ ok: true });
});
app.post('/api/admin/logout', (req, res) => {
  res.clearCookie('volaura_admin');
  res.json({ ok: true });
});
app.get('/api/admin/me', requireAdmin, (_req, res) => res.json({ ok: true }));

// ----- Admin: Releases -----
const upload = multer({
  storage: multer.diskStorage({
    destination: (_req, _f, cb) => cb(null, RELEASES_DIR),
    filename:    (_req, f, cb)  => cb(null, f.originalname.replace(/[^A-Za-z0-9._-]/g, '_')),
  }),
  limits: { fileSize: 500 * 1024 * 1024 } // 500 MB
});

app.get('/api/admin/releases', requireAdmin, (_req, res) => {
  res.json({
    latest: releases.latest,
    versions: Object.values(releases.versions).sort((a, b) =>
      (b.uploadedAt || 0) - (a.uploadedAt || 0))
  });
});

app.post('/api/admin/releases', requireAdmin, upload.single('file'), async (req, res) => {
  const { version, notes, isLatest } = req.body;
  if (!version || !req.file) {
    return res.status(400).json({ error: 'version_or_file_missing' });
  }
  if (releases.versions[version]) {
    fs.unlinkSync(req.file.path);
    return res.status(409).json({ error: 'version_exists' });
  }
  // SHA256 + size
  const sha256 = await sha256File(req.file.path);
  const size   = (await fsp.stat(req.file.path)).size;
  releases.versions[version] = {
    version,
    notes: notes || '',
    filename: req.file.filename,
    sha256, size,
    uploadedAt: Date.now()
  };
  if (isLatest === 'true' || isLatest === true || !releases.latest) {
    releases.latest = version;
  }
  saveJson(RELEASES_DB, releases);

  // Otomatik olarak "Yeni güncelleme" bildirimi de oluştur
  if (releases.latest === version) {
    pushNotification({
      type: 'update',
      title: `VoLaura ${version} yayında 🚀`,
      body:  notes || `Yeni sürüm hazır. Şimdi güncelle.`,
      // 30 gün sonra otomatik silinir
      expiresAt: Date.now() + 1000 * 60 * 60 * 24 * 30
    });
  }
  res.json({ ok: true, release: releases.versions[version] });
});

app.delete('/api/admin/releases/:v', requireAdmin, (req, res) => {
  const v = req.params.v;
  const r = releases.versions[v];
  if (!r) return res.status(404).json({ error: 'not_found' });
  try { fs.unlinkSync(path.join(RELEASES_DIR, r.filename)); } catch {}
  delete releases.versions[v];
  if (releases.latest === v) {
    const sorted = Object.values(releases.versions)
      .sort((a, b) => compareVersions(b.version, a.version));
    releases.latest = sorted[0]?.version || null;
  }
  saveJson(RELEASES_DB, releases);
  res.json({ ok: true });
});

app.post('/api/admin/release/:v/promote', requireAdmin, (req, res) => {
  const v = req.params.v;
  if (!releases.versions[v]) return res.status(404).json({ error: 'not_found' });
  releases.latest = v;
  saveJson(RELEASES_DB, releases);
  res.json({ ok: true, latest: v });
});

// ----- Admin: Notifications -----
app.get('/api/admin/notifications', requireAdmin, (_req, res) => {
  res.json(notifications);
});
app.post('/api/admin/notifications', requireAdmin, (req, res) => {
  const { type, title, body, expiresInDays } = req.body || {};
  if (!title) return res.status(400).json({ error: 'title_missing' });
  const note = pushNotification({
    type: type || 'announcement',
    title, body: body || '',
    expiresAt: expiresInDays
      ? Date.now() + expiresInDays * 24 * 3600 * 1000
      : null
  });
  res.json(note);
});
app.delete('/api/admin/notifications/:id', requireAdmin, (req, res) => {
  const id = req.params.id;
  const i = notifications.findIndex(n => n.id === id);
  if (i < 0) return res.status(404).json({ error: 'not_found' });
  notifications.splice(i, 1);
  saveJson(NOTIFS_DB, notifications);
  res.json({ ok: true });
});

// ----- Admin: Users / Messages — vps-server (signaling) ile master parola proxy'si -----
//
// VOLAURA_MASTER_KEY .env'de ayarlanır. Aynı master key vps-server'da da
// SIGNALING_ADMIN_KEY/VOLAURA_MASTER_KEY olarak set edilmelidir; admin panel
// kullanıcısı parolayı sadece tarayıcıda girer, biz request-time'da geçici
// olarak bellek dışında tutarız.
//
// Akış: client → POST /api/admin/master-list-users { masterPassword }
//        admin server timing-safe karşılaştırır → vps-server'a HTTP GET
//        /admin/users (X-Master-Key header) → JSON döner.
const SIGNALING_BASE = process.env.SIGNALING_API_BASE || 'https://volaura.xyz:8444';
const MASTER_KEY     = process.env.VOLAURA_MASTER_KEY || '';

function masterEq(input) {
  if (!MASTER_KEY || !input) return false;
  const A = Buffer.from(String(MASTER_KEY), 'utf8');
  const B = Buffer.from(String(input),     'utf8');
  if (A.length !== B.length) return false;
  return crypto.timingSafeEqual(A, B);
}

async function signalingFetch(pathAndQuery) {
  // Node 18+ global fetch
  const url = SIGNALING_BASE.replace(/\/$/, '') + pathAndQuery;
  return fetch(url, {
    method: 'GET',
    headers: { 'X-Master-Key': MASTER_KEY },
    // Self-signed sertifika ile çalışırsa: NODE_TLS_REJECT_UNAUTHORIZED env
  });
}

app.post('/api/admin/master-list-users', requireAdmin, async (req, res) => {
  const provided = String(req.body && req.body.masterPassword || '');
  if (!masterEq(provided)) return res.status(403).json({ error: 'invalid_master_password' });
  if (!MASTER_KEY) return res.status(500).json({ error: 'master_key_not_configured' });
  try {
    const r = await signalingFetch('/admin/users');
    if (!r.ok) return res.status(r.status).json({ error: 'signaling_error', status: r.status });
    const data = await r.json();
    res.json({ ok: true, users: data.users || [] });
  } catch (e) {
    console.error('[master-list-users] hata:', e.message);
    res.status(502).json({ error: 'signaling_unreachable', detail: e.message });
  }
});

// === Kullanıcı onaylı erişim akışı ===
//
// Burada master parola İSTENMEZ — sadece admin paneli oturumu (cookie).
// Talep oluşturulur, kullanıcıya e-posta gider; kullanıcı onaylayana kadar
// admin .txt indiremez. Onaylar ise admin tek seferlik dışa aktarma yapabilir.

app.post('/api/admin/grant-request', requireAdmin, async (req, res) => {
  if (!MASTER_KEY) return res.status(500).json({ error: 'master_key_not_configured' });
  const username    = String(req.body && req.body.username || '').trim();
  const requestedBy = String(req.body && req.body.requestedBy || 'VoLaura Yönetim').slice(0, 100);
  if (!username) return res.status(400).json({ error: 'username_required' });
  try {
    const r = await fetch(SIGNALING_BASE.replace(/\/$/, '') + '/admin/request-message-access', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json', 'X-Master-Key': MASTER_KEY },
      body: JSON.stringify({ username, requestedBy }),
    });
    const data = await r.json().catch(() => ({}));
    res.status(r.status).json(data);
  } catch (e) {
    console.error('[grant-request] hata:', e.message);
    res.status(502).json({ error: 'signaling_unreachable', detail: e.message });
  }
});

app.get('/api/admin/grant-list', requireAdmin, async (_req, res) => {
  if (!MASTER_KEY) return res.status(500).json({ error: 'master_key_not_configured' });
  try {
    const r = await signalingFetch('/admin/access-grants');
    const data = await r.json().catch(() => ({}));
    res.status(r.status).json(data);
  } catch (e) {
    console.error('[grant-list] hata:', e.message);
    res.status(502).json({ error: 'signaling_unreachable' });
  }
});

app.get('/api/admin/grant-export/:token', requireAdmin, async (req, res) => {
  if (!MASTER_KEY) return res.status(500).json({ error: 'master_key_not_configured' });
  const token = String(req.params.token || '');
  try {
    const r = await signalingFetch('/admin/messages-by-grant?token=' + encodeURIComponent(token));
    if (!r.ok) {
      const j = await r.json().catch(() => ({}));
      return res.status(r.status).json(j);
    }
    const buf = Buffer.from(await r.arrayBuffer());
    res.set('Content-Type', 'text/plain; charset=utf-8');
    res.set('Content-Disposition', `attachment; filename="volaura-grant-${Date.now()}.txt"`);
    res.send(buf);
  } catch (e) {
    console.error('[grant-export] hata:', e.message);
    res.status(502).json({ error: 'signaling_unreachable' });
  }
});

// Mesaj export — txt indirme. Master parola query param yerine POST body ile gelir.
app.post('/api/admin/master-export-messages', requireAdmin, async (req, res) => {
  const provided = String(req.body && req.body.masterPassword || '');
  const username = String(req.body && req.body.username || '').trim();
  if (!username) return res.status(400).json({ error: 'username_required' });
  if (!masterEq(provided)) return res.status(403).json({ error: 'invalid_master_password' });
  if (!MASTER_KEY) return res.status(500).json({ error: 'master_key_not_configured' });
  try {
    const r = await signalingFetch('/admin/messages?username=' + encodeURIComponent(username));
    if (!r.ok) {
      const txt = await r.text().catch(() => '');
      return res.status(r.status).json({ error: 'signaling_error', status: r.status, detail: txt });
    }
    const buf = Buffer.from(await r.arrayBuffer());
    res.set('Content-Type', 'text/plain; charset=utf-8');
    res.set('Content-Disposition', `attachment; filename="volaura-${username}-${Date.now()}.txt"`);
    res.send(buf);
  } catch (e) {
    console.error('[master-export] hata:', e.message);
    res.status(502).json({ error: 'signaling_unreachable', detail: e.message });
  }
});

// Eski placeholder endpoint'ler — geriye dönük uyumluluk
app.get('/api/admin/users', requireAdmin, (_req, res) => {
  res.json({
    integrated: false,
    requiresMasterPassword: true,
    note: 'Kullanıcı verisi için master parola gerekli — Mesajlar sekmesini kullan.',
    users: []
  });
});
app.get('/api/admin/messages', requireAdmin, (_req, res) => {
  res.json({ integrated: false, requiresMasterPassword: true, messages: [] });
});

// ----- Admin SPA static -----
// no-cache: Her deploy sonrası kullanıcı anında yeni JS/HTML'i alsın
app.use('/admin', (_req, res, next) => {
  res.set('Cache-Control', 'no-cache, no-store, must-revalidate');
  res.set('Pragma', 'no-cache');
  res.set('Expires', '0');
  next();
}, express.static(path.join(ROOT, 'public/admin'), {
  index: 'index.html',
  extensions: ['html'],
  etag: false,
  lastModified: false
}));

// Root → admin redirect
app.get('/', (_req, res) => res.redirect('/admin'));

// ----- Helpers -----
function pushNotification(data) {
  const note = {
    id: crypto.randomBytes(8).toString('hex'),
    type: data.type || 'info',
    title: data.title,
    body: data.body || '',
    createdAt: Date.now(),
    expiresAt: data.expiresAt || null
  };
  notifications.unshift(note);
  if (notifications.length > 200) notifications = notifications.slice(0, 200);
  saveJson(NOTIFS_DB, notifications);
  return note;
}

async function sha256File(fp) {
  return new Promise((resolve, reject) => {
    const h = crypto.createHash('sha256');
    fs.createReadStream(fp)
      .on('data', d => h.update(d))
      .on('end',  () => resolve(h.digest('hex')))
      .on('error', reject);
  });
}

function compareVersions(a, b) {
  const sa = String(a).split(/[.\-+]/).map(x => parseInt(x, 10) || 0);
  const sb = String(b).split(/[.\-+]/).map(x => parseInt(x, 10) || 0);
  const n = Math.max(sa.length, sb.length);
  for (let i = 0; i < n; i++) {
    const x = sa[i] || 0, y = sb[i] || 0;
    if (x !== y) return x - y;
  }
  return 0;
}

// ----- Start -----
app.listen(PORT, () => {
  console.log(`[volaura-admin] listening on :${PORT}`);
  console.log(`[volaura-admin] public URL  : ${PUBLIC_URL}`);
  console.log(`[volaura-admin] admin panel : ${PUBLIC_URL}/admin`);
});
