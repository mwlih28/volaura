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
  const btn = e.target.querySelector('button[type=submit]');
  err.hidden = true;
  console.log('[login] submit başladı, token uzunluk:', token.length);
  if (!token) {
    err.textContent = 'Token boş.';
    err.hidden = false;
    return;
  }
  if (btn) { btn.disabled = true; btn.textContent = 'Giriş yapılıyor…'; }
  try {
    const r = await fetch('/api/admin/login', {
      method: 'POST', credentials: 'include',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ token })
    });
    console.log('[login] status:', r.status);
    const txt = await r.text();
    console.log('[login] body:', txt);
    if (!r.ok) {
      err.textContent = `Geçersiz giriş (HTTP ${r.status}): ${txt}`;
      err.hidden = false;
      return;
    }
    console.log('[login] başarılı, dashboard açılıyor');
    showDashboard();
  } catch (e) {
    console.error('[login] hata:', e);
    err.textContent = 'Sunucuya bağlanılamadı: ' + e.message;
    err.hidden = false;
  } finally {
    if (btn) { btn.disabled = false; btn.textContent = 'Giriş yap'; }
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

// --- Users (master parola ile) ---
function loadUsers() {
  // İlk açılışta master parola yoksa hiç istek atma — UI form bekler
  const el = $('#usersBody');
  if (!el.dataset.loaded) {
    el.innerHTML = '<p class="muted">🔒 Listelemek için yukarıdaki master parolayı gir.</p>';
  }
}

$('#usersMasterForm')?.addEventListener('submit', async (e) => {
  e.preventDefault();
  const pwd = $('#usersMasterPwd').value;
  const err = $('#usersError');
  err.hidden = true;
  if (!pwd) {
    err.textContent = 'Master parola boş olamaz.';
    err.hidden = false;
    return;
  }
  const btn = e.target.querySelector('button[type=submit]');
  btn.disabled = true;
  btn.textContent = 'Yükleniyor...';
  try {
    const r = await fetch('/api/admin/master-list-users', {
      method: 'POST', credentials: 'include',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ masterPassword: pwd })
    });
    const data = await r.json();
    if (!r.ok || !data.ok) {
      err.textContent = data.error === 'invalid_master_password'
        ? 'Master parola hatalı.'
        : data.error === 'master_key_not_configured'
          ? 'Sunucuda VOLAURA_MASTER_KEY env eksik.'
          : 'Hata: ' + (data.error || r.status);
      err.hidden = false;
      return;
    }
    renderUsers(data.users || []);
    $('#usersBody').dataset.loaded = '1';
    // Master parolayı in-memory tutuyoruz: mesaj tabında otomatik doldur
    sessionStorage.setItem('vl_master_tmp', pwd);
    $('#msgsMasterPwd').value = pwd;
  } catch (e2) {
    err.textContent = 'Sunucuya ulaşılamadı: ' + e2.message;
    err.hidden = false;
  } finally {
    btn.disabled = false;
    btn.textContent = 'Kullanıcıları Getir';
  }
});

function renderUsers(users) {
  const el = $('#usersBody');
  if (!users.length) {
    el.innerHTML = '<p class="muted">Hiç kullanıcı bulunamadı.</p>';
    return;
  }
  const rows = users.map(u => {
    const date = u.created_at ? new Date(u.created_at).toLocaleDateString('tr-TR') : '—';
    const tags = [];
    if (u.email_verified)    tags.push('<span class="tag ok">✓ Doğrulandı</span>');
    if (u.totp_enabled)      tags.push('<span class="tag warn">🔐 TOTP</span>');
    if (u.email_2fa_enabled) tags.push('<span class="tag warn">📧 E-posta 2FA</span>');
    return `
      <div class="user-row">
        <div class="user-meta">
          <div class="user-name">${escapeHtml(u.username)}</div>
          <div class="user-email">${escapeHtml(u.email || '')}</div>
          <div class="user-tags">${tags.join(' ')}</div>
        </div>
        <div class="user-stats">
          <span class="muted">#${u.id}</span>
          <span class="muted">${date}</span>
          <button class="btn ghost sm" data-export="${escapeHtml(u.username)}">📥 Mesajları İndir</button>
        </div>
      </div>`;
  }).join('');
  el.innerHTML = `<div class="users-grid">${rows}</div>
    <p class="muted small">Toplam ${users.length} kullanıcı</p>`;

  el.querySelectorAll('[data-export]').forEach(b => {
    b.addEventListener('click', () => {
      // Mesajlar sekmesine geç + kullanıcı adını doldur
      $('#msgsUsername').value = b.dataset.export;
      document.querySelector('.tab[data-tab=messages]').click();
    });
  });
}

// --- Messages export (master parola + kullanıcı adı → .txt) ---
function loadMessages() {
  const stored = sessionStorage.getItem('vl_master_tmp');
  if (stored) $('#msgsMasterPwd').value = stored;
}

$('#msgsExportForm')?.addEventListener('submit', async (e) => {
  e.preventDefault();
  const pwd = $('#msgsMasterPwd').value;
  const username = $('#msgsUsername').value.trim();
  const err = $('#msgsError');
  const ok  = $('#msgsSuccess');
  err.hidden = ok.hidden = true;
  if (!pwd) { err.textContent = 'Master parola boş.'; err.hidden = false; return; }
  if (!username) { err.textContent = 'Kullanıcı adı gerekli.'; err.hidden = false; return; }

  const btn = e.target.querySelector('button[type=submit]');
  btn.disabled = true;
  btn.textContent = 'İndiriliyor...';
  try {
    const r = await fetch('/api/admin/master-export-messages', {
      method: 'POST', credentials: 'include',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ masterPassword: pwd, username })
    });
    if (!r.ok) {
      let msg = 'HTTP ' + r.status;
      try { const j = await r.json(); msg = j.error || msg; } catch {}
      err.textContent = msg === 'invalid_master_password' ? 'Master parola hatalı.'
                       : msg === 'user_not_found'         ? 'Kullanıcı bulunamadı.'
                       : 'Hata: ' + msg;
      err.hidden = false;
      return;
    }
    // Dosyayı indir
    const blob = await r.blob();
    const url  = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `volaura-${username}-${Date.now()}.txt`;
    document.body.appendChild(a);
    a.click();
    a.remove();
    URL.revokeObjectURL(url);
    ok.textContent = `✓ ${username} kullanıcısının mesajları indirildi.`;
    ok.hidden = false;
  } catch (e2) {
    err.textContent = 'Sunucuya ulaşılamadı: ' + e2.message;
    err.hidden = false;
  } finally {
    btn.disabled = false;
    btn.textContent = 'Mesajları .txt indir';
  }
});

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
