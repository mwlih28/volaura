// ===================================================================
// VoLaura Admin Panel SPA
// ===================================================================

const $ = (sel, root = document) => root.querySelector(sel);
const $$ = (sel, root = document) => Array.from(root.querySelectorAll(sel));

// --- Auth check ---
async function checkAuth() {
  try {
    const r = await fetch('/api/admin/me', { credentials: 'include' });
    return r.ok;
  } catch { return false; }
}

function showLogin() { $('#login').hidden = false; $('#dashboard').hidden = true; }
function showDashboard() {
  $('#login').hidden = true;
  $('#dashboard').hidden = false;
  loadReleases();
  loadNotifs();
}

// --- Toast ---
function toast(msg, kind = 'info') {
  const el = $('#toast');
  el.className = 'toast ' + kind;
  el.textContent = msg;
  el.hidden = false;
  setTimeout(() => { el.hidden = true; }, 3500);
}

// --- Login form ---
$('#loginForm').addEventListener('submit', async (e) => {
  e.preventDefault();
  const token = $('#tokenInput').value.trim();
  const err = $('#loginError');
  err.hidden = true;
  try {
    const r = await fetch('/api/admin/login', {
      method: 'POST', credentials: 'include',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ token })
    });
    if (!r.ok) {
      err.textContent = 'Geçersiz token. .env içindeki ADMIN_TOKEN ile aynı olmalı.';
      err.hidden = false;
      return;
    }
    showDashboard();
  } catch (e) {
    err.textContent = 'Sunucuya bağlanılamadı: ' + e.message;
    err.hidden = false;
  }
});

$('#logoutBtn').addEventListener('click', async () => {
  await fetch('/api/admin/logout', { method: 'POST', credentials: 'include' });
  $('#tokenInput').value = '';
  showLogin();
});

// --- Tab switching ---
$$('.tab').forEach(btn => {
  btn.addEventListener('click', () => {
    $$('.tab').forEach(b => b.classList.remove('active'));
    btn.classList.add('active');
    const id = btn.dataset.tab;
    $$('.tab-panel').forEach(p => p.hidden = true);
    $('#tab-' + id).hidden = false;
    if (id === 'users')    loadUsers();
    if (id === 'messages') loadMessages();
    if (id === 'notifs')   loadNotifs();
  });
});

// --- Releases ---
async function loadReleases() {
  const r = await fetch('/api/admin/releases', { credentials: 'include' });
  if (!r.ok) return;
  const data = await r.json();
  const list = $('#releasesList');
  list.innerHTML = '';
  if (!data.versions?.length) {
    list.innerHTML = '<div class="empty">Henüz sürüm yüklenmemiş. Yukarıdan ilk sürümü ekle.</div>';
    return;
  }
  for (const v of data.versions) {
    const isLatest = v.version === data.latest;
    const div = document.createElement('div');
    div.className = 'release-item';
    const sizeMb = (v.size / 1024 / 1024).toFixed(1);
    const date = new Date(v.uploadedAt).toLocaleString('tr-TR');
    div.innerHTML = `
      <span class="ver">v${escape(v.version)}</span>
      ${isLatest ? '<span class="badge">LATEST</span>' : ''}
      <span class="meta">
        <b>${escape(v.filename)}</b> · ${sizeMb} MB · ${date}<br>
        SHA256: <code>${v.sha256.slice(0, 24)}…</code>
      </span>
      <div class="actions">
        ${isLatest ? '' : `<button class="btn ghost sm" data-act="promote" data-v="${escape(v.version)}">⭐ Latest yap</button>`}
        <button class="btn ghost sm" data-act="download" data-f="${escape(v.filename)}">⬇️ İndir</button>
        <button class="btn danger sm" data-act="delete" data-v="${escape(v.version)}">🗑</button>
      </div>
      ${v.notes ? `<div class="notes">${escapeHtml(v.notes)}</div>` : ''}
    `;
    list.appendChild(div);
  }
  list.addEventListener('click', onReleaseAction, { once: true });
}

async function onReleaseAction(e) {
  const btn = e.target.closest('button[data-act]');
  if (btn) {
    const act = btn.dataset.act;
    if (act === 'promote') {
      await fetch(`/api/admin/release/${encodeURIComponent(btn.dataset.v)}/promote`,
        { method: 'POST', credentials: 'include' });
      toast('Latest olarak işaretlendi', 'success');
      loadReleases();
    } else if (act === 'download') {
      window.location = '/downloads/' + encodeURIComponent(btn.dataset.f);
    } else if (act === 'delete') {
      if (!confirm(`v${btn.dataset.v} silinsin mi?`)) return loadReleases();
      await fetch(`/api/admin/releases/${encodeURIComponent(btn.dataset.v)}`,
        { method: 'DELETE', credentials: 'include' });
      toast('Sürüm silindi', 'success');
      loadReleases();
    }
  }
  // Re-attach for next click
  $('#releasesList').addEventListener('click', onReleaseAction, { once: true });
}

$('#refreshReleases').addEventListener('click', loadReleases);

// --- Upload ---
$('#uploadForm').addEventListener('submit', async (e) => {
  e.preventDefault();
  const form = e.currentTarget;
  const fd = new FormData(form);
  fd.set('isLatest', form.isLatest.checked ? 'true' : 'false');

  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/api/admin/releases', true);
  xhr.withCredentials = true;
  const prog = $('#uploadProgress');
  prog.hidden = false; prog.value = 0;
  xhr.upload.onprogress = (ev) => {
    if (ev.lengthComputable) prog.value = (ev.loaded / ev.total) * 100;
  };
  xhr.onload = () => {
    prog.hidden = true;
    if (xhr.status >= 200 && xhr.status < 300) {
      toast('✓ Sürüm yüklendi — kullanıcılara bildirim gönderildi', 'success');
      form.reset();
      loadReleases();
      loadNotifs();
    } else {
      let msg = 'Yükleme başarısız';
      try { msg = JSON.parse(xhr.responseText).error || msg; } catch {}
      toast('✗ ' + msg, 'error');
    }
  };
  xhr.onerror = () => { prog.hidden = true; toast('Bağlantı hatası', 'error'); };
  xhr.send(fd);
});

// --- Notifications ---
async function loadNotifs() {
  const r = await fetch('/api/admin/notifications', { credentials: 'include' });
  if (!r.ok) return;
  const data = await r.json();
  const list = $('#notifsList');
  list.innerHTML = '';
  if (!data.length) {
    list.innerHTML = '<div class="empty">Aktif bildirim yok.</div>';
    return;
  }
  const icons = {
    update: '🚀', announcement: '📢', warning: '⚠️', event: '🎉', info: 'ℹ️'
  };
  for (const n of data) {
    const div = document.createElement('div');
    div.className = 'notif-item';
    const date = new Date(n.createdAt).toLocaleString('tr-TR');
    const exp = n.expiresAt ? `· son: ${new Date(n.expiresAt).toLocaleDateString('tr-TR')}` : '';
    div.innerHTML = `
      <div class="icon">${icons[n.type] || icons.info}</div>
      <div class="body-col">
        <h4>${escapeHtml(n.title)}</h4>
        <p>${escapeHtml(n.body || '')}</p>
        <small>${date} ${exp}</small>
      </div>
      <button class="btn danger sm" data-id="${escape(n.id)}">🗑</button>
    `;
    list.appendChild(div);
  }
}

$('#notifsList').addEventListener('click', async (e) => {
  const btn = e.target.closest('button[data-id]');
  if (!btn) return;
  await fetch('/api/admin/notifications/' + encodeURIComponent(btn.dataset.id),
    { method: 'DELETE', credentials: 'include' });
  toast('Bildirim silindi', 'success');
  loadNotifs();
});

$('#refreshNotifs').addEventListener('click', loadNotifs);

$('#notifForm').addEventListener('submit', async (e) => {
  e.preventDefault();
  const fd = new FormData(e.currentTarget);
  const body = Object.fromEntries(fd);
  if (body.expiresInDays) body.expiresInDays = parseInt(body.expiresInDays, 10);
  const r = await fetch('/api/admin/notifications', {
    method: 'POST', credentials: 'include',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body)
  });
  if (r.ok) {
    toast('✓ Bildirim yayınlandı', 'success');
    e.currentTarget.reset();
    loadNotifs();
  } else {
    toast('✗ Bildirim oluşturulamadı', 'error');
  }
});

// --- Users / Messages (placeholder) ---
async function loadUsers() {
  const r = await fetch('/api/admin/users', { credentials: 'include' });
  const data = await r.json();
  const el = $('#usersBody');
  if (!data.integrated) {
    el.innerHTML = `
      <p>📡 <b>Signaling backend entegrasyonu gerekli.</b></p>
      <p class="muted">${escapeHtml(data.note || '')}</p>
      <p class="muted">Yapılması gereken: <code>server/admin/server.js</code> içinde
      <code>/api/admin/users</code> endpoint'inden, ana <code>wss://volaura.xyz:8444</code>
      sunucusuna admin API çağrısı yap. <code>SIGNALING_API</code> ve
      <code>SIGNALING_ADMIN_KEY</code>'i <code>.env</code>'e ekle.</p>`;
  } else {
    // ... gerçek render
    el.textContent = JSON.stringify(data.users, null, 2);
  }
}
async function loadMessages() {
  const r = await fetch('/api/admin/messages', { credentials: 'include' });
  const data = await r.json();
  const el = $('#msgsBody');
  if (!data.integrated) {
    el.innerHTML = `<p>📡 <b>Signaling backend entegrasyonu gerekli.</b></p>
      <p class="muted">Sürümler ve bildirimler tam çalışıyor — kullanıcı/mesaj yönetimi
      ana sunucu API'si bağlanınca aktive olacak.</p>`;
  } else {
    el.textContent = JSON.stringify(data.messages, null, 2);
  }
}

// --- Helpers ---
function escapeHtml(s) {
  return String(s)
    .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;').replace(/'/g, '&#39;');
}
function escape(s) { return encodeURIComponent(String(s)); }

// --- Init ---
(async () => {
  if (await checkAuth()) showDashboard();
  else showLogin();
})();
