// VoLaura - Sinyalleşme + Auth + Arkadaş + Çağrı sunucusu
// Veritabanı: NeonDB (Postgres)  |  E-posta: SendGrid
require('dotenv').config();

const WebSocket = require('ws');
const http = require('http');
const https = require('https');
const fs = require('fs');
const url = require('url');
const crypto = require('crypto');

const db = require('./db');
const mailer = require('./email');
const legal = require('./legal');

// ---- 2FA (TOTP + Twilio SMS) ----
let speakeasy = null;
try { speakeasy = require('speakeasy'); } catch (_) {
    console.warn('[2FA] speakeasy kurulu değil (npm install speakeasy).');
}
let twilioClient = null;
if (process.env.TWILIO_ACCOUNT_SID && process.env.TWILIO_AUTH_TOKEN) {
    try {
        twilioClient = require('twilio')(process.env.TWILIO_ACCOUNT_SID, process.env.TWILIO_AUTH_TOKEN);
        console.log('[2FA] Twilio SMS aktif.');
    } catch (e) {
        console.warn('[2FA] Twilio başlatılamadı:', e.message);
    }
} else {
    console.log('[2FA] Twilio env vars yok — SMS dev/log moduna düşecek.');
}
function genNumericCode(len = 6) {
    let s = '';
    for (let i = 0; i < len; ++i) s += Math.floor(Math.random() * 10);
    return s;
}
function maskPhone(p) {
    if (!p) return null;
    const s = String(p);
    if (s.length < 4) return s;
    return s.slice(0, 3) + '***' + s.slice(-2);
}
async function trySendSms(to, body) {
    if (twilioClient && process.env.TWILIO_FROM_NUMBER) {
        try {
            await twilioClient.messages.create({ from: process.env.TWILIO_FROM_NUMBER, to, body });
            return { ok: true };
        } catch (e) {
            console.error('[2FA] SMS hatası:', e.message);
            return { ok: false, error: e.message };
        }
    }
    // Dev fallback: konsola yaz
    console.log(`[2FA-DEV-SMS] to=${to} body=${body}`);
    return { ok: true, dev: true };
}
function verifyTotp(secretBase32, token) {
    if (!speakeasy || !secretBase32 || !token) return false;
    try {
        return speakeasy.totp.verify({
            secret: secretBase32, encoding: 'base32',
            token: String(token).replace(/\s+/g, ''),
            window: 1,
        });
    } catch { return false; }
}

// ============================================================
//                       Yardımcılar
// ============================================================
const norm = (s) => String(s || '').trim().toLowerCase();
const BASE_URL = (process.env.BASE_URL || 'https://volaura.qzz.io:8444').replace(/\/$/, '');

function hashPassword(password, salt) {
    return crypto.scryptSync(String(password), salt, 32).toString('hex');
}
function makeSalt() { return crypto.randomBytes(16).toString('hex'); }
function makeToken() { return crypto.randomBytes(32).toString('hex'); }

function isValidUserName(u) {
    return typeof u === 'string' && /^[a-zA-Z0-9_.-]{3,24}$/.test(u);
}
function isValidEmail(e) {
    if (typeof e !== 'string') return false;
    const v = e.trim();
    if (v.length < 5 || v.length > 254) return false;
    return /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(v);
}
function isValidPassword(p) {
    return typeof p === 'string' && p.length >= 6 && p.length <= 200;
}

// Online users:  norm(username) -> Set<ws>
const onlineUsers = new Map();
function isOnline(userName) {
    const s = onlineUsers.get(norm(userName));
    return !!(s && s.size > 0);
}
function addOnline(userName, ws) {
    const k = norm(userName);
    if (!onlineUsers.has(k)) onlineUsers.set(k, new Set());
    onlineUsers.get(k).add(ws);
}
function removeOnline(userName, ws) {
    const k = norm(userName);
    const s = onlineUsers.get(k);
    if (!s) return;
    s.delete(ws);
    if (s.size === 0) onlineUsers.delete(k);
}
function sendToUser(userName, payload) {
    const s = onlineUsers.get(norm(userName));
    if (!s) return;
    const msg = JSON.stringify(payload);
    for (const w of s) if (w.readyState === WebSocket.OPEN) w.send(msg);
}

// ============================================================
//                  HTTP istek yöneticisi
// ============================================================
async function handleHttpRequest(req, res) {
    try {
        const u = url.parse(req.url, true);
        if (req.method === 'GET' && u.pathname === '/') {
            return sendHtml(res, 200, mailer.shellHtml('Ana Sayfa', legal.HOME_HTML));
        }
        if (req.method === 'GET' && u.pathname === '/health') {
            return sendJson(res, 200, { ok: true });
        }
        if (req.method === 'GET' && (u.pathname === '/logo.png' || u.pathname === '/favicon.ico')) {
            return sendStaticFile(res, u.pathname === '/logo.png' ? 'image/png' : 'image/x-icon',
                                  __dirname + '/assets' + u.pathname);
        }
        if (req.method === 'GET' && u.pathname === '/verify') {
            return handleVerifyGet(req, res, u.query);
        }
        if (req.method === 'GET' && u.pathname === '/reset') {
            return handleResetGet(req, res, u.query);
        }
        if (req.method === 'POST' && u.pathname === '/reset') {
            return handleResetPost(req, res);
        }
        if (req.method === 'GET' && u.pathname === '/enable-2fa') {
            return handleEnable2faGet(req, res, u.query);
        }
        if (req.method === 'GET' && u.pathname === '/terms') {
            return sendHtml(res, 200, mailer.shellHtml('Hizmet Sartlari', legal.TERMS_HTML));
        }
        if (req.method === 'GET' && u.pathname === '/privacy') {
            return sendHtml(res, 200, mailer.shellHtml('Gizlilik Politikasi', legal.PRIVACY_HTML));
        }
        return sendHtml(res, 404, mailer.shellHtml('Bulunamadı',
            `<h2 style="color:#f4f7ff;">404</h2><p>Sayfa bulunamadı.</p>`));
    } catch (err) {
        console.error('HTTP hata:', err);
        return sendHtml(res, 500, mailer.shellHtml('Hata',
            `<h2 style="color:#ff9da8;">Sunucu Hatası</h2><p>Lütfen tekrar deneyin.</p>`));
    }
}

function sendHtml(res, status, html) {
    res.writeHead(status, { 'Content-Type': 'text/html; charset=utf-8' });
    res.end(html);
}
function sendStaticFile(res, contentType, filePath) {
    fs.readFile(filePath, (err, buf) => {
        if (err) { res.writeHead(404); return res.end('Not found'); }
        res.writeHead(200, {
            'Content-Type': contentType,
            'Content-Length': buf.length,
            'Cache-Control': 'public, max-age=86400, immutable',
        });
        res.end(buf);
    });
}
function sendJson(res, status, obj) {
    res.writeHead(status, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify(obj));
}

async function handleVerifyGet(req, res, q) {
    const token = String(q.token || '');
    if (!token) {
        return sendHtml(res, 400, mailer.shellHtml('Geçersiz', `<h2 style="color:#ff9da8;">Eksik token</h2>`));
    }
    const r = await db.consumeToken(token, 'verify');
    if (!r.ok) {
        const reasonMsg = r.reason === 'expired' ? 'Bağlantının süresi dolmuş.' :
                          r.reason === 'used'    ? 'Bu bağlantı zaten kullanılmış.' :
                                                   'Geçersiz bağlantı.';
        return sendHtml(res, 400, mailer.shellHtml('Doğrulama Başarısız',
            `<h2 style="color:#ff9da8;margin:6px 0 10px 0;">Doğrulama Başarısız</h2>
             <p>${reasonMsg}</p>
             <p style="color:#7d8ba7;font-size:12px;">Uygulamadan yeni bir doğrulama e-postası talep edebilirsin.</p>`));
    }
    await db.setEmailVerified(r.userId);
    return sendHtml(res, 200, mailer.shellHtml('Hesap Doğrulandı',
        `<h2 style="color:#2ecc71;margin:6px 0 10px 0;">✓ Hesabın doğrulandı</h2>
         <p>Merhaba <b>${mailer.escapeHtml(r.username)}</b>, e-postan başarıyla doğrulandı.</p>
         <p>Şimdi VoLaura uygulamasından giriş yapabilirsin.</p>`));
}

async function handleEnable2faGet(req, res, q) {
    const token = String(q.token || '');
    if (!token) {
        return sendHtml(res, 400, mailer.shellHtml('Geçersiz', `<h2 style="color:#ff9da8;">Eksik token</h2>`));
    }
    const r = await db.consumeToken(token, 'enable_2fa');
    if (!r.ok) {
        const reasonMsg = r.reason === 'expired' ? 'Bağlantının süresi dolmuş.' :
                          r.reason === 'used'    ? 'Bu bağlantı zaten kullanılmış.' :
                                                   'Geçersiz bağlantı.';
        return sendHtml(res, 400, mailer.shellHtml('İşlem Başarısız',
            `<h2 style="color:#ff9da8;margin:6px 0 10px 0;">2FA Açılamadı</h2>
             <p>${reasonMsg}</p>
             <p style="color:#7d8ba7;font-size:12px;">Uygulamadan ayarlar menüsünden 2FA'yı manuel olarak da açabilirsin.</p>`));
    }
    // E-posta hesabı zaten doğrulanmış kabul edilir (yeni cihaz uyarısı sadece email adresine gitti)
    await db.setEmailVerified(r.userId);
    await db.setEmail2fa(r.userId, true);
    return sendHtml(res, 200, mailer.shellHtml('2FA Etkinleştirildi',
        `<h2 style="color:#2ecc71;margin:6px 0 10px 0;">✓ İki adımlı doğrulama açıldı</h2>
         <p>Merhaba <b>${mailer.escapeHtml(r.username)}</b>, hesabın için <b>e-posta tabanlı 2FA</b> etkinleştirildi.</p>
         <p>Bundan sonra her girişte e-postana 6 haneli bir kod gönderilecek.</p>
         <p style="color:#7d8ba7;font-size:12px;margin-top:14px;">2FA'yı kapatmak veya TOTP uygulamasına geçmek için VoLaura uygulamasında <i>Hesap Güvenliği</i> sayfasını kullanabilirsin.</p>`));
}

async function handleResetGet(req, res, q) {
    const token = String(q.token || '');
    if (!token) {
        return sendHtml(res, 400, mailer.shellHtml('Geçersiz', `<h2 style="color:#ff9da8;">Eksik token</h2>`));
    }
    // Bu noktada token'i tüketmiyoruz; sadece var/geçerli mi diye bakacağız.
    const peek = await db.pool.query(
        `SELECT et.expires_at, et.used FROM email_tokens et
         WHERE et.token = $1 AND et.type = 'reset'`, [token]);
    if (peek.rowCount === 0) {
        return sendHtml(res, 400, mailer.shellHtml('Geçersiz', `<h2 style="color:#ff9da8;">Geçersiz bağlantı</h2>`));
    }
    const row = peek.rows[0];
    if (row.used) {
        return sendHtml(res, 400, mailer.shellHtml('Kullanılmış', `<h2 style="color:#ff9da8;">Bu bağlantı kullanılmış</h2>`));
    }
    if (new Date(row.expires_at).getTime() < Date.now()) {
        return sendHtml(res, 400, mailer.shellHtml('Süresi Dolmuş', `<h2 style="color:#ff9da8;">Bağlantı süresi dolmuş</h2>`));
    }
    const safeToken = mailer.escapeHtml(token);
    return sendHtml(res, 200, mailer.shellHtml('Şifre Sıfırla', `
        <h2 style="color:#f4f7ff;margin:6px 0 12px 0;">Yeni Şifre Belirle</h2>
        <form method="POST" action="/reset" style="display:flex;flex-direction:column;gap:10px;">
            <input type="hidden" name="token" value="${safeToken}">
            <input type="password" name="password" placeholder="Yeni şifre (min 6)" required minlength="6"
                   style="padding:12px 14px;border-radius:10px;border:1px solid #2a3346;background:#1a1f2c;color:#eaf0ff;">
            <input type="password" name="password2" placeholder="Yeni şifre (tekrar)" required minlength="6"
                   style="padding:12px 14px;border-radius:10px;border:1px solid #2a3346;background:#1a1f2c;color:#eaf0ff;">
            <button type="submit" style="padding:12px;border:none;border-radius:10px;background:#2b6df5;
                    color:#fff;font-weight:700;cursor:pointer;font-size:14px;">Şifreyi Güncelle</button>
        </form>`));
}

function readBody(req) {
    return new Promise((resolve, reject) => {
        let data = '';
        req.on('data', c => { data += c; if (data.length > 1e5) { req.destroy(); reject(new Error('body too large')); } });
        req.on('end', () => resolve(data));
        req.on('error', reject);
    });
}

async function handleResetPost(req, res) {
    const body = await readBody(req);
    const params = new url.URLSearchParams(body);
    const token = String(params.get('token') || '');
    const password = String(params.get('password') || '');
    const password2 = String(params.get('password2') || '');
    if (!token || !isValidPassword(password)) {
        return sendHtml(res, 400, mailer.shellHtml('Hata',
            `<h2 style="color:#ff9da8;">Şifre en az 6 karakter olmalı.</h2>`));
    }
    if (password !== password2) {
        return sendHtml(res, 400, mailer.shellHtml('Hata',
            `<h2 style="color:#ff9da8;">Şifreler eşleşmiyor.</h2>`));
    }
    const r = await db.consumeToken(token, 'reset');
    if (!r.ok) {
        const reasonMsg = r.reason === 'expired' ? 'Bağlantının süresi dolmuş.' :
                          r.reason === 'used'    ? 'Bu bağlantı zaten kullanılmış.' :
                                                   'Geçersiz bağlantı.';
        return sendHtml(res, 400, mailer.shellHtml('Hata',
            `<h2 style="color:#ff9da8;">${reasonMsg}</h2>`));
    }
    const salt = makeSalt();
    const hash = hashPassword(password, salt);
    await db.updatePassword(r.userId, hash, salt);
    return sendHtml(res, 200, mailer.shellHtml('Şifre Güncellendi',
        `<h2 style="color:#2ecc71;">✓ Şifren güncellendi</h2>
         <p>VoLaura uygulamasından yeni şifrenle giriş yapabilirsin.</p>`));
}

// ============================================================
//             HTTP/HTTPS sunucuları (WS upgrade dahil)
// ============================================================
const WS_PORT = parseInt(process.env.WS_PORT || '8443', 10);
const WSS_PORT = parseInt(process.env.WSS_PORT || '8444', 10);

const httpServer = http.createServer(handleHttpRequest);
const wss = new WebSocket.Server({ server: httpServer });

let wssSecure = null;
try {
    const httpsServer = https.createServer({
        cert: fs.readFileSync('/etc/letsencrypt/live/volaura.qzz.io/fullchain.pem'),
        key:  fs.readFileSync('/etc/letsencrypt/live/volaura.qzz.io/privkey.pem'),
    }, handleHttpRequest);
    wssSecure = new WebSocket.Server({ server: httpsServer });
    httpsServer.listen(WSS_PORT, () => {
        console.log(`VoLaura WSS+HTTPS çalışıyor: ${WSS_PORT}`);
    });
} catch (e) {
    console.log('SSL sertifikaları bulunamadı, sadece WS modu (HTTP)');
}

// ============================================================
//                      WebSocket akışı
// ============================================================
function setupWebSocketHandlers(ws, req) {
    console.log('Yeni bağlantı');
    // Cihaz fingerprint için IP ve User-Agent yakala — yeni cihaz bildirimleri için
    try {
        const xfwd = (req && req.headers && req.headers['x-forwarded-for']) || '';
        const remoteIp = (req && req.socket && req.socket.remoteAddress) || '';
        ws._clientIp = (typeof xfwd === 'string' && xfwd.split(',')[0].trim()) || remoteIp || '';
        ws._userAgent = (req && req.headers && req.headers['user-agent']) || '';
    } catch (_) { ws._clientIp = ''; ws._userAgent = ''; }
    ws.on('message', async (data) => {
        let message;
        try { message = JSON.parse(data); }
        catch { return ws.send(JSON.stringify({ type: 'error', message: 'Geçersiz mesaj formatı' })); }
        try { await handleMessage(ws, message); }
        catch (err) {
            console.error('handleMessage hata:', err);
            try { ws.send(JSON.stringify({ type: 'error', message: 'Sunucu hatası' })); } catch {}
        }
    });
    ws.on('close', () => handleDisconnect(ws));
}
wss.on('connection', (ws, req) => setupWebSocketHandlers(ws, req));
if (wssSecure) wssSecure.on('connection', (ws, req) => setupWebSocketHandlers(ws, req));

// ============================================================
//                Oda yönetimi (kendi state'i)
// ============================================================
const rooms = new Map();      // roomCode -> { roomName, password, creator, participants, createdAt }
const clients = new Map();    // clientId -> { ws, roomCode, userName }

function generateRoomCode() {
    let code;
    do { code = crypto.randomInt(100000, 999999).toString(); } while (rooms.has(code));
    return code;
}

async function handleMessage(ws, message) {
    const { type } = message;
    switch (type) {
        // ---- Auth ----
        case 'register':            return handleRegister(ws, message);
        case 'login':               return handleLogin(ws, message);
        case 'verify_login_2fa':    return handleVerifyLogin2fa(ws, message);
        case 'get_security_status': return handleGetSecurityStatus(ws);
        case 'start_totp_setup':    return handleStartTotpSetup(ws);
        case 'confirm_totp_setup':  return handleConfirmTotpSetup(ws, message);
        case 'disable_totp':        return handleDisableTotp(ws, message);
        case 'set_phone':           return handleSetPhone(ws, message);
        case 'send_phone_code':     return handleSendPhoneCode(ws);
        case 'verify_phone_code':   return handleVerifyPhoneCode(ws, message);
        case 'toggle_sms_2fa':      return handleToggleSms2fa(ws, message);
        case 'toggle_email_2fa':    return handleToggleEmail2fa(ws, message);
        case 'logout':              return handleLogout(ws);
        case 'resend_verification': return handleResendVerification(ws, message);
        case 'request_password_reset': return handleRequestPasswordReset(ws, message);
        // ---- Parolasız giriş (telefon/e-posta + kod) ----
        case 'request_login_code':  return handleRequestLoginCode(ws, message);
        case 'verify_login_code':   return handleVerifyLoginCode(ws, message);
        // ---- E2E DM public key ----
        case 'announce_pubkey':     return handleAnnouncePubkey(ws, message);
        case 'get_pubkey':          return handleGetPubkey(ws, message);

        // ---- Friends ----
        case 'list_friends':           if (ws._authUserId) return sendFriendsList(ws); break;
        case 'send_friend_request':    return handleSendFriendRequest(ws, message);
        case 'accept_friend_request':  return handleAcceptFriendRequest(ws, message);
        case 'reject_friend_request':  return handleRejectFriendRequest(ws, message);
        case 'cancel_friend_request':  return handleCancelFriendRequest(ws, message);
        case 'remove_friend':          return handleRemoveFriend(ws, message);

        // ---- Calls ----
        case 'call_friend':  return handleCallFriend(ws, message);
        case 'call_decline': return handleCallDecline(ws, message);
        case 'call_cancel':  return handleCallCancel(ws, message);
        case 'call_accept':  return handleCallAccept(ws, message);

        // ---- Rooms / media (mevcut akış) ----
        case 'create_room': return handleCreateRoom(ws, message.clientId, message.roomName, message.userName, message.password);
        case 'join_room':   return handleJoinRoom(ws, message.clientId, message.roomCode, message.userName, message.password);
        case 'leave_room':  return handleLeaveRoom(ws, message.clientId, message.roomCode);
        case 'media_offer':
        case 'media_answer':
        case 'ice_candidate': return handleMediaSignaling(ws, message);
        case 'chat_message':  return handleChatMessage(ws, message);
        case 'media_state':   return handleMediaState(ws, message);
        case 'media_chunk':   return handleMediaChunk(ws, message);

        // ---- Servers (Discord-benzeri) ----
        case 'list_servers':    return handleListServers(ws);
        case 'create_server':   return handleCreateServer(ws, message);
        case 'join_server':     return handleJoinServer(ws, message);
        case 'leave_server':    return handleLeaveServer(ws, message);
        case 'delete_server':   return handleDeleteServer(ws, message);
        case 'rename_server':   return handleRenameServer(ws, message);
        case 'list_members':    return handleListMembers(ws, message);

        // ---- Channels ----
        case 'list_channels':   return handleListChannels(ws, message);
        case 'create_channel':  return handleCreateChannel(ws, message);
        case 'delete_channel':  return handleDeleteChannel(ws, message);
        case 'rename_channel':  return handleRenameChannel(ws, message);

        // ---- Channel messages ----
        case 'list_messages':   return handleListMessages(ws, message);
        case 'send_message':    return handleSendMessage(ws, message);
        case 'delete_message':  return handleDeleteMessage(ws, message);
        case 'edit_message':    return handleEditMessage(ws, message);

        // ---- Direct messages ----
        case 'list_dm_threads':  return handleListDmThreads(ws);
        case 'list_dm_messages': return handleListDmMessages(ws, message);
        case 'send_dm':          return handleSendDm(ws, message);
        case 'mark_dm_read':     return handleMarkDmRead(ws, message);
        case 'delete_dm':        return handleDeleteDm(ws, message);
        case 'edit_dm':          return handleEditDm(ws, message);
        case 'typing_channel':   return handleTypingChannel(ws, message);
        case 'typing_dm':        return handleTypingDm(ws, message);

        // ---- Voice channels ----
        case 'voice_join':   return handleVoiceJoin(ws, message);
        case 'voice_leave':  return handleVoiceLeave(ws, message);
        case 'voice_chunk':  return handleVoiceChunk(ws, message);
        case 'voice_state':  return handleVoiceState(ws, message);
        case 'voice_list':   return handleVoiceList(ws, message);
        case 'voice_share_start': return handleVoiceShareStart(ws, message);
        case 'voice_share_stop':  return handleVoiceShareStop(ws, message);
        case 'voice_media_chunk': return handleVoiceMediaChunk(ws, message);

        default:
            ws.send(JSON.stringify({ type: 'error', message: 'Bilinmeyen mesaj tipi' }));
    }
}

// ============================================================
//                       Auth handlers
// ============================================================
async function handleRegister(ws, msg) {
    const userName = String(msg.userName || '').trim();
    const email    = String(msg.email || '').trim();
    const password = String(msg.password || '');

    if (!isValidUserName(userName))
        return ws.send(JSON.stringify({ type: 'register_result', ok: false,
            error: 'Kullanıcı adı 3-24 karakter, sadece harf/rakam/._- olmalı.' }));
    if (!isValidEmail(email))
        return ws.send(JSON.stringify({ type: 'register_result', ok: false,
            error: 'Geçersiz e-posta adresi.' }));
    if (!isValidPassword(password))
        return ws.send(JSON.stringify({ type: 'register_result', ok: false,
            error: 'Şifre en az 6 karakter olmalı.' }));

    if (await db.findUserByUsername(userName))
        return ws.send(JSON.stringify({ type: 'register_result', ok: false,
            error: 'Bu kullanıcı adı zaten kayıtlı.' }));
    if (await db.findUserByEmail(email))
        return ws.send(JSON.stringify({ type: 'register_result', ok: false,
            error: 'Bu e-posta zaten kayıtlı.' }));

    const salt = makeSalt();
    const hash = hashPassword(password, salt);
    const user = await db.createUser({
        username: userName, email, passwordHash: hash, passwordSalt: salt,
    });

    // Doğrulama tokenı + e-posta
    try { await sendVerificationEmail(user); }
    catch (e) {
        console.error('Doğrulama e-postası gönderilemedi:', e && e.message);
        return ws.send(JSON.stringify({ type: 'register_result', ok: false,
            error: 'Doğrulama e-postası gönderilemedi. Lütfen tekrar dene.' }));
    }
    ws.send(JSON.stringify({
        type: 'register_result', ok: true, userName: user.username, email: user.email,
        message: 'Kayıt başarılı. Doğrulama bağlantısı e-postana gönderildi.',
    }));
    console.log(`Kayıt: ${user.username} <${user.email}>`);
}

async function sendVerificationEmail(user) {
    const token = makeToken();
    const expiresAt = new Date(Date.now() + 24 * 60 * 60 * 1000); // 24 saat
    await db.createToken({ userId: user.id, type: 'verify', token, expiresAt });
    const link = `${BASE_URL}/verify?token=${encodeURIComponent(token)}`;
    const tpl = mailer.verificationEmail({ userName: user.username, link });
    await mailer.sendMail({ to: user.email, ...tpl });
}

async function sendPasswordResetEmail(user) {
    const token = makeToken();
    const expiresAt = new Date(Date.now() + 60 * 60 * 1000); // 1 saat
    await db.createToken({ userId: user.id, type: 'reset', token, expiresAt });
    const link = `${BASE_URL}/reset?token=${encodeURIComponent(token)}`;
    const tpl = mailer.passwordResetEmail({ userName: user.username, link });
    await mailer.sendMail({ to: user.email, ...tpl });
}

// Yeni cihaz fingerprint = SHA-256(IP + UA), kısaltılmış.
// IP'yi /24 alt-ağa indirgeyerek dinamik IP değişimlerinde yanlış pozitifi azaltırız.
function ipv4Subnet24(ip) {
    const m = String(ip || '').match(/(\d+)\.(\d+)\.(\d+)\.\d+$/);
    if (m) return `${m[1]}.${m[2]}.${m[3]}.`;
    // IPv6 veya parse edilemiyor: olduğu gibi döndür
    return String(ip || '');
}
function deviceFingerprint(ip, ua) {
    const sub = ipv4Subnet24(ip);
    const uaShort = String(ua || '').slice(0, 80);
    const raw = sub + '|' + uaShort;
    return crypto.createHash('sha256').update(raw).digest('hex').slice(0, 32);
}

// IP geolokasyon — best-effort, ücretsiz ip-api.com (rate limit ~45 rpm)
function ipGeolocation(ip) {
    return new Promise((resolve) => {
        if (!ip || ip === '127.0.0.1' || ip === '::1' || ip.startsWith('::ffff:127')) {
            return resolve('Yerel ağ');
        }
        // ::ffff:1.2.3.4 -> 1.2.3.4 dönüştür
        const cleanIp = String(ip).replace(/^::ffff:/, '');
        const reqOpts = {
            hostname: 'ip-api.com',
            path: `/json/${encodeURIComponent(cleanIp)}?fields=status,country,regionName,city`,
            method: 'GET', timeout: 3000,
        };
        const req2 = http.request(reqOpts, (resp) => {
            let body = '';
            resp.on('data', (c) => body += c);
            resp.on('end', () => {
                try {
                    const j = JSON.parse(body);
                    if (j.status === 'success') {
                        const parts = [j.city, j.regionName, j.country].filter(Boolean);
                        resolve(parts.join(', ') || 'bilinmiyor');
                    } else resolve('bilinmiyor');
                } catch { resolve('bilinmiyor'); }
            });
        });
        req2.on('error', () => resolve('bilinmiyor'));
        req2.on('timeout', () => { req2.destroy(); resolve('bilinmiyor'); });
        req2.end();
    });
}

// Login başarılı olduğunda çağrılır — cihaz daha önce görülmediyse uyarı yollar.
// Spam önleme:
//   - Aynı /24 alt-ağdan 30 gün içinde bilinen cihaz varsa uyarma (sadece kaydet).
//   - Aynı kullanıcıya 24 saat içinde bir uyarı gönderildiyse atla.
async function checkAndAlertNewDevice(ws, user) {
    try {
        const ip = ws._clientIp || '';
        const ua = ws._userAgent || '';
        if (!ip && !ua) return; // Veri yok, atla
        const fp = deviceFingerprint(ip, ua);
        const existing = await db.findKnownDevice(user.id, fp);
        if (existing) {
            // Bilinen cihaz: sadece son görülme zamanını güncelle
            await db.touchKnownDevice(user.id, fp);
            return;
        }
        // Aynı /24 alt-ağdan son 30 gün içinde bilinen cihaz varsa: sessizce kaydet
        const subnet = ipv4Subnet24(ip);
        const sameSubnet = subnet ? await db.hasRecentDeviceInSubnet(user.id, subnet) : false;
        await db.registerKnownDevice(user.id, fp, ip, ua);
        if (sameSubnet) {
            return; // Aynı evden/işten farklı UA gibi durum — uyarı yok
        }
        // İlk cihaz için uyarı yok
        const allCount = await db.pool.query(
            'SELECT COUNT(*)::int AS n FROM known_devices WHERE user_id = $1', [user.id]);
        if (allCount.rows[0].n <= 1) return;
        // 24 saat throttle
        const lastAt = await db.getLastNewDeviceAlertAt(user.id);
        if (lastAt && (Date.now() - lastAt) < 24 * 60 * 60 * 1000) {
            console.log(`[NEW-DEVICE] ${user.username} throttled (son uyarı 24h içinde)`);
            return;
        }
        await db.touchLastNewDeviceAlert(user.id);

        const location = await ipGeolocation(ip);
        const when = new Date().toLocaleString('tr-TR', {
            timeZone: 'Europe/Istanbul', dateStyle: 'medium', timeStyle: 'short' });

        // Action linkleri: şifre sıfırla + 2FA aç (her ikisi de tek-kullanım token).
        let resetLink = null, enable2faLink = null;
        if (user.email) {
            try {
                const resetToken = makeToken();
                await db.createToken({ userId: user.id, type: 'reset', token: resetToken,
                    expiresAt: new Date(Date.now() + 60 * 60 * 1000) }); // 1 saat
                resetLink = `${BASE_URL}/reset?token=${encodeURIComponent(resetToken)}`;
            } catch (e) { console.warn('[NEW-DEVICE] reset token üretilemedi:', e && e.message); }
            try {
                const e2faToken = makeToken();
                await db.createToken({ userId: user.id, type: 'enable_2fa', token: e2faToken,
                    expiresAt: new Date(Date.now() + 24 * 60 * 60 * 1000) }); // 24 saat
                enable2faLink = `${BASE_URL}/enable-2fa?token=${encodeURIComponent(e2faToken)}`;
            } catch (e) { console.warn('[NEW-DEVICE] 2fa token üretilemedi:', e && e.message); }
        }
        // E-posta gönder
        if (user.email) {
            const tpl = mailer.newDeviceEmail({
                userName: user.username, ip, userAgent: ua, location, when,
                resetLink, enable2faLink,
            });
            mailer.sendMail({ to: user.email, ...tpl }).catch((e) => {
                console.warn('[NEW-DEVICE] e-posta hatası:', e && e.message);
            });
        }
        // SMS gönder (telefon doğrulanmışsa)
        if (user.phone_e164 && user.phone_verified) {
            const sms = `VoLaura: ${user.username} hesabina yeni cihazdan giris (${location}). Sen degilsen sifreni hemen degistir.`;
            trySendSms(user.phone_e164, sms).catch(() => {});
        }
        console.log(`[NEW-DEVICE] ${user.username} <- ${ip} (${location})`);
    } catch (e) {
        console.error('[NEW-DEVICE] hata:', e && e.message);
    }
}

async function sendLoginEmailCode(user, code) {
    try {
        const tpl = mailer.loginCodeEmail({ userName: user.username, code });
        await mailer.sendMail({ to: user.email, ...tpl });
        return { ok: true };
    } catch (e) {
        console.error('[2FA-EMAIL] Mail hatası:', e && e.message);
        return { ok: false, error: e && e.message };
    }
}

// ============================================================
//        Parolasız giriş: identifier + tek seferlik kod
// ============================================================
function hashLoginCode(code) {
    // Statik salt ile, kod kısa ve user_id ile beraber kullanıldığı için yeterli
    return crypto.createHash('sha256')
        .update('volaura-pwl|' + String(code))
        .digest('hex');
}

// İstek-bazlı IP rate-limit (memory) — basit: identifier bazında 60 sn'de 3 istek
const _pwlRateMap = new Map(); // key -> [timestamps]
function pwlRateLimited(key) {
    const now = Date.now();
    const arr = (_pwlRateMap.get(key) || []).filter(t => now - t < 60_000);
    if (arr.length >= 3) { _pwlRateMap.set(key, arr); return true; }
    arr.push(now);
    _pwlRateMap.set(key, arr);
    return false;
}

async function handleRequestLoginCode(ws, msg) {
    const identifier = String(msg.identifier || '').trim();
    if (!identifier)
        return ws.send(JSON.stringify({ type: 'request_login_code_result',
            ok: false, error: 'E-posta gerekli.' }));

    if (pwlRateLimited('pwl:' + identifier.toLowerCase())) {
        return ws.send(JSON.stringify({ type: 'request_login_code_result',
            ok: false, error: 'Çok fazla istek. Lütfen biraz sonra tekrar dene.' }));
    }

    const user = await db.findUserByLogin(identifier);
    // Kullanıcı bulunamasa bile generic OK döndürmek security açısından iyi olur,
    // ama UX için bilgi verelim — telefon E.164 formatı zorunlu olmadığından
    // kullanıcı yanlış yazmış olabilir.
    if (!user)
        return ws.send(JSON.stringify({ type: 'request_login_code_result',
            ok: false, error: 'Bu telefon/e-posta ile kayıtlı kullanıcı bulunamadı.' }));
    if (!user.email_verified)
        return ws.send(JSON.stringify({ type: 'request_login_code_result',
            ok: false, error: 'E-postan henüz doğrulanmamış.' }));

    // SMS desteği kaldırıldı — sadece e-posta ile kod gönderilir.
    if (!user.email) {
        return ws.send(JSON.stringify({ type: 'request_login_code_result',
            ok: false, error: 'Hesaba bağlı e-posta yok.' }));
    }
    const channel = 'email';
    const code = genNumericCode(6);
    const codeHash = hashLoginCode(code);
    await db.createPasswordlessCode(user.id, channel, codeHash, 600);
    const masked = maskEmail(user.email);
    try {
        const tpl = mailer.loginCodeEmail({ userName: user.username, code });
        await mailer.sendMail({ to: user.email, ...tpl });
    } catch (e) {
        console.error('[PWL] e-posta hatası:', e && e.message);
        return ws.send(JSON.stringify({ type: 'request_login_code_result',
            ok: false, error: 'Kod gönderilemedi.' }));
    }
    ws.send(JSON.stringify({ type: 'request_login_code_result',
        ok: true, channel, target: masked, userName: user.username }));
}

// ============================================================
//                 E2E DM Public Key handlers
// ============================================================
async function handleAnnouncePubkey(ws, msg) {
    if (!requireAuth(ws)) return;
    const pub = String(msg.publicKey || '').trim();
    // base64 32 byte = 44 char (padding dahil)
    if (!pub || pub.length < 32 || pub.length > 128)
        return ws.send(JSON.stringify({ type: 'announce_pubkey_result',
            ok: false, error: 'Geçersiz anahtar.' }));
    await db.setUserPublicKey(ws._authUserId, pub);
    ws.send(JSON.stringify({ type: 'announce_pubkey_result', ok: true }));
}

async function handleGetPubkey(ws, msg) {
    if (!requireAuth(ws)) return;
    const username = String(msg.username || '').trim();
    if (!username) return;
    const row = await db.getUserPublicKeyByName(username);
    ws.send(JSON.stringify({
        type: 'pubkey_result',
        username: row ? row.username : username,
        publicKey: row ? (row.public_key || '') : '',
        found: !!row,
    }));
}

async function handleVerifyLoginCode(ws, msg) {
    const identifier = String(msg.identifier || '').trim();
    const code = String(msg.code || '').trim();
    if (!identifier || !code)
        return ws.send(JSON.stringify({ type: 'verify_login_code_result',
            ok: false, error: 'Eksik bilgi.' }));

    if (pwlRateLimited('pwlv:' + identifier.toLowerCase())) {
        return ws.send(JSON.stringify({ type: 'verify_login_code_result',
            ok: false, error: 'Çok fazla deneme. Bekle.' }));
    }

    const user = await db.findUserByLogin(identifier);
    if (!user)
        return ws.send(JSON.stringify({ type: 'verify_login_code_result',
            ok: false, error: 'Kullanıcı bulunamadı.' }));

    const looksLikeEmail = identifier.includes('@');
    let channel = looksLikeEmail ? 'email' : 'sms';
    // Önce hangi kanal kullanıldı bilmiyoruz — her ikisini de dene
    const tryChannels = (channel === 'email')
        ? ['email', 'sms']
        : ['sms', 'email'];
    let result = { ok: false, reason: 'not_found' };
    for (const ch of tryChannels) {
        result = await db.consumePasswordlessCode(user.id, ch, hashLoginCode(code));
        if (result.ok || result.reason !== 'not_found') break;
    }
    if (!result.ok) {
        const messages = {
            not_found: 'Kod bulunamadı. Önce kod iste.',
            expired: 'Kod süresi doldu. Yeni kod iste.',
            wrong_code: 'Kod hatalı.',
            too_many_attempts: 'Çok fazla yanlış deneme. Yeni kod iste.',
        };
        return ws.send(JSON.stringify({ type: 'verify_login_code_result',
            ok: false, error: messages[result.reason] || 'Kod doğrulanamadı.' }));
    }

    // Login finalize — handleLogin ile aynı akış
    ws._authUserId = user.id;
    ws._authUserName = user.username;
    addOnline(user.username, ws);

    ws.send(JSON.stringify({ type: 'verify_login_code_result', ok: true,
        userName: user.username, email: user.email }));
    ws.send(JSON.stringify({ type: 'login_result', ok: true,
        userName: user.username, email: user.email }));
    await sendFriendsList(ws);
    checkAndAlertNewDevice(ws, user).catch(() => {});

    const friendNames = await db.listFriends(user.id);
    for (const f of friendNames)
        sendToUser(f, { type: 'friend_status', userName: user.username, online: true });
    console.log(`Giriş (Kod): ${user.username}`);
}

function maskEmail(e) {
    if (!e) return '';
    const [local, domain] = String(e).split('@');
    if (!domain) return e;
    const visible = local.length <= 2 ? local[0] : local.slice(0, 2);
    return visible + '***@' + domain;
}

async function handleLogin(ws, msg) {
    const id = String(msg.userName || '').trim();
    const password = String(msg.password || '');
    const user = await db.findUserByLogin(id);
    if (!user)
        return ws.send(JSON.stringify({ type: 'login_result', ok: false, error: 'Kullanıcı bulunamadı.' }));

    if (hashPassword(password, user.password_salt) !== user.password_hash)
        return ws.send(JSON.stringify({ type: 'login_result', ok: false, error: 'Şifre hatalı.' }));

    if (!user.email_verified)
        return ws.send(JSON.stringify({
            type: 'login_result', ok: false,
            error: 'E-postan henüz doğrulanmamış. Lütfen e-postandaki bağlantıya tıkla.',
            errorCode: 'email_not_verified', userName: user.username, email: user.email,
        }));

    // 2FA zorunluysa challenge döndür
    if (user.totp_enabled || user.sms_2fa_enabled || user.email_2fa_enabled) {
        ws._pendingLoginUserId = user.id;
        ws._pendingLoginUserName = user.username;
        ws._pendingLoginExpires = Date.now() + 10 * 60 * 1000;
        // SMS desteği kaldırıldı — SMS 2FA etkinse e-posta ile kod gönder.
        if ((user.sms_2fa_enabled || user.email_2fa_enabled) && user.email) {
            const code = genNumericCode(6);
            await db.createTwoFaCode(user.id, 'login_email', code, 600);
            sendLoginEmailCode(user, code).catch(() => {});
        }
        return ws.send(JSON.stringify({
            type: 'login_2fa_required',
            userName: user.username,
            methods: {
                totp:  !!user.totp_enabled,
                sms:   false,
                email: !!(user.email_2fa_enabled || user.sms_2fa_enabled),
                phoneHint: null,
                emailHint: (user.email_2fa_enabled || user.sms_2fa_enabled) ? maskEmail(user.email) : null,
            },
        }));
    }

    ws._authUserId = user.id;
    ws._authUserName = user.username;
    addOnline(user.username, ws);

    ws.send(JSON.stringify({ type: 'login_result', ok: true, userName: user.username, email: user.email }));
    await sendFriendsList(ws);

    // Yeni cihaz tespiti — async, login response'u geciktirme
    checkAndAlertNewDevice(ws, user).catch(() => {});

    // Online bildirimi
    const friendNames = await db.listFriends(user.id);
    for (const f of friendNames) sendToUser(f, { type: 'friend_status', userName: user.username, online: true });
    console.log(`Giriş: ${user.username}`);
}

async function handleLogout(ws) {
    if (!ws._authUserId) return;
    const userName = ws._authUserName;
    const userId   = ws._authUserId;
    removeOnline(userName, ws);
    ws._authUserId = null;
    ws._authUserName = null;
    if (!isOnline(userName)) {
        const friends = await db.listFriends(userId);
        for (const f of friends) sendToUser(f, { type: 'friend_status', userName, online: false });
    }
}

// ============================================================
//                          2FA handlers
// ============================================================

async function handleVerifyLogin2fa(ws, msg) {
    const code = String(msg.code || '').trim();
    if (!ws._pendingLoginUserId) {
        return ws.send(JSON.stringify({ type: 'login_2fa_result', ok: false,
            error: 'Aktif bir 2FA isteği yok. Lütfen tekrar giriş yap.' }));
    }
    if (Date.now() > (ws._pendingLoginExpires || 0)) {
        ws._pendingLoginUserId = ws._pendingLoginUserName = ws._pendingLoginExpires = null;
        return ws.send(JSON.stringify({ type: 'login_2fa_result', ok: false,
            error: '2FA isteğinin süresi doldu. Yeniden giriş yap.' }));
    }
    const userId = ws._pendingLoginUserId;
    const sec = await db.getUserSecurity(userId);
    if (!sec) {
        return ws.send(JSON.stringify({ type: 'login_2fa_result', ok: false, error: 'Kullanıcı bulunamadı.' }));
    }
    let ok = false;
    if (sec.totp_enabled && sec.totp_secret) ok = verifyTotp(sec.totp_secret, code);
    if (!ok && sec.sms_2fa_enabled)   ok = await db.consumeTwoFaCode(userId, 'login_sms', code);
    if (!ok && sec.email_2fa_enabled) ok = await db.consumeTwoFaCode(userId, 'login_email', code);
    if (!ok) {
        return ws.send(JSON.stringify({ type: 'login_2fa_result', ok: false,
            error: 'Kod hatalı veya süresi dolmuş.' }));
    }
    // Login'i tamamla
    ws._authUserId = userId;
    ws._authUserName = sec.username;
    ws._pendingLoginUserId = ws._pendingLoginUserName = ws._pendingLoginExpires = null;
    addOnline(sec.username, ws);

    const user = await db.findUserById(userId);
    ws.send(JSON.stringify({ type: 'login_2fa_result', ok: true, userName: sec.username, email: user && user.email }));
    ws.send(JSON.stringify({ type: 'login_result', ok: true, userName: sec.username, email: user && user.email }));
    await sendFriendsList(ws);
    // Yeni cihaz tespiti — 2FA sonrası finalize
    if (user) checkAndAlertNewDevice(ws, user).catch(() => {});
    const friendNames = await db.listFriends(userId);
    for (const f of friendNames) sendToUser(f, { type: 'friend_status', userName: sec.username, online: true });
    console.log(`Giriş (2FA): ${sec.username}`);
}

async function handleGetSecurityStatus(ws) {
    if (!requireAuth(ws)) return;
    const sec = await db.getUserSecurity(ws._authUserId);
    if (!sec) return;
    ws.send(JSON.stringify({
        type: 'security_status',
        totpEnabled:   !!sec.totp_enabled,
        smsEnabled:    !!sec.sms_2fa_enabled,
        emailEnabled:  !!sec.email_2fa_enabled,
        phoneVerified: !!sec.phone_verified,
        phone:         sec.phone_e164 || '',
        phoneMasked:   sec.phone_e164 ? maskPhone(sec.phone_e164) : '',
        emailMasked:   sec.email ? maskEmail(sec.email) : '',
        twilioConfigured: !!twilioClient,
    }));
}

async function handleToggleEmail2fa(ws, msg) {
    if (!requireAuth(ws)) return;
    const enable = !!msg.enable;
    const sec = await db.getUserSecurity(ws._authUserId);
    if (!sec) return sendErr(ws, 'Kullanıcı bulunamadı.');
    if (enable && !sec.email_verified) {
        return ws.send(JSON.stringify({ type: 'toggle_email_2fa_result', ok: false,
            error: 'Önce e-posta adresini doğrula.' }));
    }
    // KAPATMA güvenliği: TOTP açıksa authenticator kodu ile onayla
    if (!enable && sec.email_2fa_enabled && sec.totp_enabled && sec.totp_secret) {
        const code = String(msg.code || '').trim();
        if (!code || !verifyTotp(sec.totp_secret, code)) {
            return ws.send(JSON.stringify({ type: 'toggle_email_2fa_result', ok: false,
                error: 'Authenticator kodu hatalı.' }));
        }
    }
    await db.setEmail2fa(ws._authUserId, enable);
    ws.send(JSON.stringify({ type: 'toggle_email_2fa_result', ok: true, enabled: enable }));
    return handleGetSecurityStatus(ws);
}

async function handleStartTotpSetup(ws) {
    if (!requireAuth(ws)) return;
    if (!speakeasy) return sendErr(ws, 'Sunucuda TOTP kütüphanesi kurulu değil.');
    const secret = speakeasy.generateSecret({
        name: `VoLaura (${ws._authUserName})`,
        issuer: 'VoLaura',
        length: 20,
    });
    await db.setTotpSecret(ws._authUserId, secret.base32);
    ws.send(JSON.stringify({
        type: 'totp_setup',
        ok: true,
        secretBase32: secret.base32,
        otpauthUrl: secret.otpauth_url,
    }));
}

async function handleConfirmTotpSetup(ws, msg) {
    if (!requireAuth(ws)) return;
    const code = String(msg.code || '').trim();
    const sec = await db.getUserSecurity(ws._authUserId);
    if (!sec || !sec.totp_secret) {
        return ws.send(JSON.stringify({ type: 'totp_confirm_result', ok: false,
            error: 'Önce TOTP kurulumunu başlat.' }));
    }
    if (!verifyTotp(sec.totp_secret, code)) {
        return ws.send(JSON.stringify({ type: 'totp_confirm_result', ok: false,
            error: 'Kod hatalı. Uygulamayı tekrar dene.' }));
    }
    await db.enableTotp(ws._authUserId);
    // Doğrulama: gerçekten DB'ye yazıldı mı?
    const verify = await db.getUserSecurity(ws._authUserId);
    console.log(`[TOTP] enable userId=${ws._authUserId} user=${ws._authUserName} totp_enabled=${verify && verify.totp_enabled}`);
    ws.send(JSON.stringify({ type: 'totp_confirm_result', ok: true }));
    return handleGetSecurityStatus(ws);
}

async function handleDisableTotp(ws, msg) {
    if (!requireAuth(ws)) return;
    const code = String(msg.code || '').trim();
    const sec = await db.getUserSecurity(ws._authUserId);
    if (!sec || !sec.totp_enabled) {
        return ws.send(JSON.stringify({ type: 'totp_disable_result', ok: false, error: 'TOTP zaten kapalı.' }));
    }
    if (!verifyTotp(sec.totp_secret, code)) {
        return ws.send(JSON.stringify({ type: 'totp_disable_result', ok: false, error: 'Kod hatalı.' }));
    }
    await db.disableTotp(ws._authUserId);
    ws.send(JSON.stringify({ type: 'totp_disable_result', ok: true }));
    return handleGetSecurityStatus(ws);
}

async function handleSetPhone(ws, msg) {
    if (!requireAuth(ws)) return;
    const phone = String(msg.phone || '').trim();
    if (!/^\+[1-9][0-9]{7,14}$/.test(phone)) {
        return ws.send(JSON.stringify({ type: 'set_phone_result', ok: false,
            error: 'Geçersiz telefon. E.164 formatı bekleniyor (ör: +905551234567).' }));
    }
    await db.setPhoneNumber(ws._authUserId, phone);
    ws.send(JSON.stringify({ type: 'set_phone_result', ok: true }));
    return handleGetSecurityStatus(ws);
}

async function handleSendPhoneCode(ws) {
    if (!requireAuth(ws)) return;
    const sec = await db.getUserSecurity(ws._authUserId);
    if (!sec || !sec.phone_e164) {
        return ws.send(JSON.stringify({ type: 'send_phone_code_result', ok: false,
            error: 'Önce telefon numaranı ayarla.' }));
    }
    const code = genNumericCode(6);
    await db.createTwoFaCode(ws._authUserId, 'phone_verify', code, 600);
    const r = await trySendSms(sec.phone_e164, `VoLaura doğrulama kodun: ${code}`);
    ws.send(JSON.stringify({
        type: 'send_phone_code_result',
        ok: true,
        dev: !!r.dev,
        error: r.ok ? null : r.error,
    }));
}

async function handleVerifyPhoneCode(ws, msg) {
    if (!requireAuth(ws)) return;
    const code = String(msg.code || '').trim();
    const ok = await db.consumeTwoFaCode(ws._authUserId, 'phone_verify', code);
    if (!ok) {
        return ws.send(JSON.stringify({ type: 'verify_phone_code_result', ok: false,
            error: 'Kod hatalı veya süresi dolmuş.' }));
    }
    await db.setPhoneVerified(ws._authUserId);
    ws.send(JSON.stringify({ type: 'verify_phone_code_result', ok: true }));
    return handleGetSecurityStatus(ws);
}

async function handleToggleSms2fa(ws, _msg) {
    if (!requireAuth(ws)) return;
    return ws.send(JSON.stringify({ type: 'toggle_sms_2fa_result', ok: false,
        error: 'SMS 2FA artık desteklenmiyor. Lütfen e-posta veya TOTP kullan.' }));
}
async function _handleToggleSms2faLegacy(ws, msg) {
    if (!requireAuth(ws)) return;
    const enable = !!msg.enable;
    const sec = await db.getUserSecurity(ws._authUserId);
    if (!sec) return sendErr(ws, 'Kullanıcı bulunamadı.');
    if (enable && !sec.phone_verified) {
        return ws.send(JSON.stringify({ type: 'toggle_sms_2fa_result', ok: false,
            error: 'Önce telefon numaranı doğrula.' }));
    }
    await db.setSms2fa(ws._authUserId, enable);
    ws.send(JSON.stringify({ type: 'toggle_sms_2fa_result', ok: true, enabled: enable }));
    return handleGetSecurityStatus(ws);
}

async function handleResendVerification(ws, msg) {
    const id = String(msg.userName || '').trim();
    const user = await db.findUserByLogin(id);
    // Bilgi sızdırmayalım: cevap her durumda success
    if (user && !user.email_verified) {
        try { await sendVerificationEmail(user); } catch (e) { console.error('resend verify hata:', e && e.message); }
    }
    ws.send(JSON.stringify({ type: 'verification_sent', ok: true,
        message: 'Doğrulama e-postası adresinize gönderildi (eğer hesap mevcutsa).' }));
}

async function handleRequestPasswordReset(ws, msg) {
    const id = String(msg.userName || '').trim();
    const user = await db.findUserByLogin(id);
    if (user) {
        try { await sendPasswordResetEmail(user); } catch (e) { console.error('reset email hata:', e && e.message); }
    }
    ws.send(JSON.stringify({ type: 'password_reset_sent', ok: true,
        message: 'Şifre sıfırlama bağlantısı (varsa) e-postanıza gönderildi.' }));
}

// ============================================================
//                     Friend handlers
// ============================================================
async function sendFriendsList(ws) {
    if (!ws._authUserId) return;
    const [friendNames, incoming, outgoing] = await Promise.all([
        db.listFriends(ws._authUserId),
        db.listIncomingRequests(ws._authUserId),
        db.listOutgoingRequests(ws._authUserId),
    ]);
    const friends = friendNames.map(name => ({ userName: name, online: isOnline(name) }));
    ws.send(JSON.stringify({
        type: 'friends_list', friends, pendingIn: incoming, pendingOut: outgoing,
    }));
}

async function requireAuth(ws) {
    if (!ws._authUserId) {
        ws.send(JSON.stringify({ type: 'error', message: 'Önce giriş yapmalısın.' }));
        return false;
    }
    return true;
}

async function handleSendFriendRequest(ws, msg) {
    if (!await requireAuth(ws)) return;
    const target = String(msg.userName || '').trim();
    if (!target || norm(target) === norm(ws._authUserName))
        return ws.send(JSON.stringify({ type: 'friend_op_result', ok: false, error: 'Geçersiz kullanıcı adı.' }));
    const other = await db.findUserByUsername(target);
    if (!other)
        return ws.send(JSON.stringify({ type: 'friend_op_result', ok: false, error: 'Kullanıcı bulunamadı.' }));
    if (await db.areFriends(ws._authUserId, other.id))
        return ws.send(JSON.stringify({ type: 'friend_op_result', ok: false, error: 'Zaten arkadaşsınız.' }));
    if (await db.hasOutgoingRequest(ws._authUserId, other.id))
        return ws.send(JSON.stringify({ type: 'friend_op_result', ok: false, error: 'İstek zaten gönderilmiş.' }));

    // Eğer karşı taraf zaten bize istek attıysa otomatik kabul
    if (await db.hasOutgoingRequest(other.id, ws._authUserId)) {
        return acceptRequestById(ws, other);
    }

    await db.addFriendRequest(ws._authUserId, other.id);
    ws.send(JSON.stringify({ type: 'friend_op_result', ok: true, op: 'sent', userName: other.username }));
    sendToUser(other.username, { type: 'friend_request', fromUserName: ws._authUserName });
    await sendFriendsList(ws);
    console.log(`İstek: ${ws._authUserName} -> ${other.username}`);
}

async function acceptRequestById(ws, other) {
    await db.removeAnyRequestBetween(ws._authUserId, other.id);
    await db.addFriendship(ws._authUserId, other.id);
    await sendFriendsList(ws);
    sendToUser(other.username, {
        type: 'friend_added', userName: ws._authUserName, online: isOnline(ws._authUserName)
    });
    // Karşı taraf da güncel listeyi alsın
    const otherSet = onlineUsers.get(norm(other.username));
    if (otherSet) for (const w of otherSet) await sendFriendsList(w);
    console.log(`Arkadaşlık: ${ws._authUserName} <-> ${other.username}`);
}

async function handleAcceptFriendRequest(ws, msg) {
    if (!await requireAuth(ws)) return;
    const target = String(msg.userName || '').trim();
    const other = await db.findUserByUsername(target);
    if (!other) return;
    return acceptRequestById(ws, other);
}

async function handleRejectFriendRequest(ws, msg) {
    if (!await requireAuth(ws)) return;
    const target = String(msg.userName || '').trim();
    const other = await db.findUserByUsername(target);
    if (!other) return;
    // Karşı taraftan bana gelen isteği sil
    await db.removeFriendRequest(other.id, ws._authUserId);
    await sendFriendsList(ws);
    // Karşı taraf da güncel listeyi alsın
    const otherSet = onlineUsers.get(norm(other.username));
    if (otherSet) for (const w of otherSet) await sendFriendsList(w);
}

async function handleCancelFriendRequest(ws, msg) {
    if (!await requireAuth(ws)) return;
    const target = String(msg.userName || '').trim();
    const other = await db.findUserByUsername(target);
    if (!other) return;
    await db.removeFriendRequest(ws._authUserId, other.id);
    await sendFriendsList(ws);
    const otherSet = onlineUsers.get(norm(other.username));
    if (otherSet) for (const w of otherSet) await sendFriendsList(w);
}

async function handleRemoveFriend(ws, msg) {
    if (!await requireAuth(ws)) return;
    const target = String(msg.userName || '').trim();
    const other = await db.findUserByUsername(target);
    if (!other) return;
    await db.removeFriendship(ws._authUserId, other.id);
    await sendFriendsList(ws);
    sendToUser(other.username, { type: 'friend_removed', userName: ws._authUserName });
    const otherSet = onlineUsers.get(norm(other.username));
    if (otherSet) for (const w of otherSet) await sendFriendsList(w);
}

// ============================================================
//                       Call handlers
// ============================================================
async function handleCallFriend(ws, msg) {
    if (!await requireAuth(ws)) return;
    const target = String(msg.userName || '').trim();
    const roomCode = String(msg.roomCode || '').trim();
    if (!target || !roomCode)
        return ws.send(JSON.stringify({ type: 'call_error', userName: target, error: 'Geçersiz arama.' }));
    const other = await db.findUserByUsername(target);
    if (!other)
        return ws.send(JSON.stringify({ type: 'call_error', userName: target, error: 'Kullanıcı bulunamadı.' }));
    if (!await db.areFriends(ws._authUserId, other.id))
        return ws.send(JSON.stringify({ type: 'call_error', userName: target, error: 'Sadece arkadaşlarını arayabilirsin.' }));
    if (!isOnline(other.username))
        return ws.send(JSON.stringify({ type: 'call_unreachable', userName: other.username }));
    sendToUser(other.username, { type: 'incoming_call', fromUserName: ws._authUserName, roomCode });
    console.log(`Arama: ${ws._authUserName} -> ${other.username} (oda: ${roomCode})`);
}
async function handleCallDecline(ws, msg) {
    if (!await requireAuth(ws)) return;
    const target = String(msg.userName || '').trim();
    sendToUser(target, { type: 'call_declined', fromUserName: ws._authUserName });
}
async function handleCallCancel(ws, msg) {
    if (!await requireAuth(ws)) return;
    const target = String(msg.userName || '').trim();
    sendToUser(target, { type: 'call_cancelled', fromUserName: ws._authUserName });
}
async function handleCallAccept(ws, msg) {
    if (!await requireAuth(ws)) return;
    const target = String(msg.userName || '').trim();
    sendToUser(target, { type: 'call_accepted', fromUserName: ws._authUserName });
}

// ============================================================
//                       Room handlers
// ============================================================
function broadcastToRoom(roomCode, senderClientId, payload) {
    const room = rooms.get(roomCode);
    if (!room) return;
    const data = JSON.stringify(payload);
    room.participants.forEach((p, pid) => {
        if (pid === senderClientId) return;
        if (p.ws.readyState === WebSocket.OPEN) p.ws.send(data);
    });
}

function handleCreateRoom(ws, clientId, roomName, userName, password) {
    const roomCode = generateRoomCode();
    const map = new Map();
    map.set(clientId, { userName, ws });
    rooms.set(roomCode, {
        roomName: roomName || 'İsimsiz Oda',
        password: password || null,
        creator: { clientId, userName },
        participants: map,
        createdAt: new Date(),
    });
    clients.set(clientId, { ws, roomCode, userName });
    ws.send(JSON.stringify({ type: 'room_created', roomCode, roomName: roomName || 'İsimsiz Oda' }));
    console.log(`Oda oluşturuldu: ${roomCode} - "${roomName}" (${userName})`);
}

function handleJoinRoom(ws, clientId, roomCode, userName, password) {
    const room = rooms.get(roomCode);
    if (!room) return ws.send(JSON.stringify({ type: 'error', message: 'Oda bulunamadı. Kod doğru mu?' }));
    if (room.password && room.password !== password)
        return ws.send(JSON.stringify({ type: 'error', message: 'Yanlış şifre!' }));

    room.participants.forEach((p) => {
        if (p.ws.readyState === WebSocket.OPEN) {
            p.ws.send(JSON.stringify({ type: 'participant_joined', participantId: clientId, userName }));
        }
    });
    room.participants.set(clientId, { userName, ws });
    clients.set(clientId, { ws, roomCode, userName });

    const existing = [];
    room.participants.forEach((p, pid) => { if (pid !== clientId) existing.push({ participantId: pid, userName: p.userName }); });
    ws.send(JSON.stringify({ type: 'room_joined', roomCode, roomName: room.roomName, participants: existing }));
    console.log(`${userName} (${clientId}) odaya katıldı: ${roomCode} - "${room.roomName}" (${room.participants.size} kişi)`);
}

function handleLeaveRoom(ws, clientId, roomCode) {
    const room = rooms.get(roomCode);
    if (!room) return;
    const p = room.participants.get(clientId);
    const userName = p ? p.userName : 'Bilinmeyen';
    room.participants.delete(clientId);
    clients.delete(clientId);
    room.participants.forEach((part) => {
        if (part.ws.readyState === WebSocket.OPEN) {
            part.ws.send(JSON.stringify({ type: 'participant_left', participantId: clientId, userName }));
        }
    });
    if (room.participants.size === 0) {
        rooms.delete(roomCode);
        console.log(`Oda silindi: ${roomCode} - "${room.roomName}"`);
    } else {
        console.log(`${userName} odadan ayrıldı: ${roomCode} (${room.participants.size} kişi kaldı)`);
    }
}

function handleMediaSignaling(ws, message) {
    const target = clients.get(message.to);
    if (target && target.ws.readyState === WebSocket.OPEN) {
        target.ws.send(JSON.stringify(message));
    }
}
function handleChatMessage(ws, message) {
    const { roomCode, clientId, userName, message: text } = message;
    if (!roomCode || !clientId || !text) return;
    broadcastToRoom(roomCode, clientId, {
        type: 'chat_message', participantId: clientId, userName: userName || 'Bilinmeyen',
        message: text, timestamp: new Date().toLocaleTimeString('tr-TR', { hour12: false }),
    });
}
function handleMediaState(ws, message) {
    const { roomCode, clientId, audioMuted, screenSharing } = message;
    if (!roomCode || !clientId) return;
    broadcastToRoom(roomCode, clientId, {
        type: 'media_state', participantId: clientId, audioMuted: !!audioMuted, screenSharing: !!screenSharing,
    });
}
function handleMediaChunk(ws, message) {
    const { roomCode, clientId, mediaKind, payload } = message;
    if (!roomCode || !clientId || !mediaKind || !payload) return;
    broadcastToRoom(roomCode, clientId, { type: 'media_chunk', participantId: clientId, mediaKind, payload });
}

// ============================================================
//                      Voice channels
// ============================================================
// channelId -> Map<userId, { ws, username, muted, deafened, joinedAt }>
const voiceRooms = new Map();

function voiceParticipantsArray(channelId) {
    const m = voiceRooms.get(channelId);
    if (!m) return [];
    const arr = [];
    for (const [uid, p] of m.entries()) {
        arr.push({
            userId: uid, username: p.username,
            muted: !!p.muted, deafened: !!p.deafened,
            sharingScreen: !!p.sharingScreen, sharingCamera: !!p.sharingCamera,
        });
    }
    return arr;
}

function voiceBroadcast(channelId, fromUserId, payload, includeSelf = false) {
    const m = voiceRooms.get(channelId);
    if (!m) return;
    const data = JSON.stringify(payload);
    for (const [uid, p] of m.entries()) {
        if (!includeSelf && uid === fromUserId) continue;
        if (p.ws.readyState === 1) p.ws.send(data);
    }
}

async function handleVoiceJoin(ws, msg) {
    if (!requireAuth(ws)) return;
    const channelId = Number(msg.channelId);
    if (!channelId) return sendErr(ws, 'Geçersiz kanal.');
    // Kanal doğrulama: voice olmalı + kullanıcı server üyesi olmalı
    try {
        const ch = await db.getChannelById(channelId);
        if (!ch || ch.type !== 'voice') return sendErr(ws, 'Sesli kanal değil.');
        const role = await db.getServerMemberRole(ch.server_id, ws._authUserId);
        if (!role) return sendErr(ws, 'Bu sunucunun üyesi değilsin.');
    } catch (e) { return sendErr(ws, 'Kanal bulunamadı.'); }

    // Zaten bu kanaldaysa sessizce geç (duplicate join önle)
    const existingRoom = voiceRooms.get(channelId);
    if (existingRoom && existingRoom.has(ws._authUserId)) {
        ws.send(JSON.stringify({ type: 'voice_joined', channelId, participants: voiceParticipantsArray(channelId) }));
        return;
    }

    // Başka voice kanalındaysa önce çıkar
    for (const [cid, m] of voiceRooms.entries()) {
        if (m.has(ws._authUserId)) {
            m.delete(ws._authUserId);
            voiceBroadcast(cid, ws._authUserId, { type: 'voice_member_left', channelId: cid, userId: ws._authUserId, username: ws._authUserName });
            if (m.size === 0) voiceRooms.delete(cid);
        }
    }

    if (!voiceRooms.has(channelId)) voiceRooms.set(channelId, new Map());
    const room = voiceRooms.get(channelId);
    const entry = { ws, username: ws._authUserName, muted: false, deafened: false, joinedAt: Date.now() };
    room.set(ws._authUserId, entry);

    // Yeni üyeye mevcut listeyi gönder
    ws.send(JSON.stringify({ type: 'voice_joined', channelId, participants: voiceParticipantsArray(channelId) }));
    // Diğerlerine yeni üyeyi bildir
    voiceBroadcast(channelId, ws._authUserId, {
        type: 'voice_member_joined', channelId,
        userId: ws._authUserId, username: ws._authUserName, muted: false, deafened: false,
    });
}

function leaveVoiceForUser(userId, username) {
    for (const [cid, m] of voiceRooms.entries()) {
        if (m.has(userId)) {
            m.delete(userId);
            voiceBroadcast(cid, userId, { type: 'voice_member_left', channelId: cid, userId, username });
            if (m.size === 0) voiceRooms.delete(cid);
        }
    }
}

async function handleVoiceLeave(ws) {
    if (!ws._authUserId) return;
    leaveVoiceForUser(ws._authUserId, ws._authUserName);
    ws.send(JSON.stringify({ type: 'voice_left' }));
}

function handleVoiceChunk(ws, msg) {
    if (!ws._authUserId) return;
    const channelId = Number(msg.channelId);
    const payload = msg.payload;
    if (!channelId || !payload) return;
    const room = voiceRooms.get(channelId);
    if (!room || !room.has(ws._authUserId)) return;
    const me = room.get(ws._authUserId);
    if (me.muted) return; // muted'ken ses yollama
    voiceBroadcast(channelId, ws._authUserId, {
        type: 'voice_chunk', channelId, userId: ws._authUserId, payload,
    });
}

function handleVoiceState(ws, msg) {
    if (!ws._authUserId) return;
    const channelId = Number(msg.channelId);
    if (!channelId) return;
    const room = voiceRooms.get(channelId);
    if (!room || !room.has(ws._authUserId)) return;
    const p = room.get(ws._authUserId);
    if (typeof msg.muted === 'boolean') p.muted = msg.muted;
    if (typeof msg.deafened === 'boolean') p.deafened = msg.deafened;
    voiceBroadcast(channelId, ws._authUserId, {
        type: 'voice_state', channelId, userId: ws._authUserId,
        muted: !!p.muted, deafened: !!p.deafened,
    }, true);
}

async function handleVoiceList(ws, msg) {
    if (!requireAuth(ws)) return;
    const channelId = Number(msg.channelId);
    if (!channelId) return;
    ws.send(JSON.stringify({
        type: 'voice_participants', channelId,
        participants: voiceParticipantsArray(channelId),
    }));
}

// kind: "screen" | "camera"
function handleVoiceShareStart(ws, msg) {
    if (!ws._authUserId) return;
    const channelId = Number(msg.channelId);
    const kind = String(msg.kind || '');
    if (!channelId || (kind !== 'screen' && kind !== 'camera')) return;
    const room = voiceRooms.get(channelId);
    if (!room || !room.has(ws._authUserId)) return;
    const p = room.get(ws._authUserId);
    if (kind === 'screen') p.sharingScreen = true;
    if (kind === 'camera') p.sharingCamera = true;
    voiceBroadcast(channelId, ws._authUserId, {
        type: 'voice_share_started', channelId,
        userId: ws._authUserId, username: ws._authUserName, kind,
    }, true);
}

function handleVoiceShareStop(ws, msg) {
    if (!ws._authUserId) return;
    const channelId = Number(msg.channelId);
    const kind = String(msg.kind || '');
    if (!channelId || (kind !== 'screen' && kind !== 'camera')) return;
    const room = voiceRooms.get(channelId);
    if (!room || !room.has(ws._authUserId)) return;
    const p = room.get(ws._authUserId);
    if (kind === 'screen') p.sharingScreen = false;
    if (kind === 'camera') p.sharingCamera = false;
    voiceBroadcast(channelId, ws._authUserId, {
        type: 'voice_share_stopped', channelId,
        userId: ws._authUserId, username: ws._authUserName, kind,
    }, true);
}

function handleVoiceMediaChunk(ws, msg) {
    if (!ws._authUserId) return;
    const channelId = Number(msg.channelId);
    const kind = String(msg.kind || '');
    const payload = msg.payload;
    if (!channelId || !payload || (kind !== 'screen' && kind !== 'camera')) return;
    const room = voiceRooms.get(channelId);
    if (!room || !room.has(ws._authUserId)) return;
    // broadcast (kendisi hariç)
    voiceBroadcast(channelId, ws._authUserId, {
        type: 'voice_media_chunk', channelId, kind,
        userId: ws._authUserId, username: ws._authUserName,
        payload,
    });
}

async function handleDisconnect(ws) {
    // Voice room'dan çıkar
    if (ws._authUserId) leaveVoiceForUser(ws._authUserId, ws._authUserName);
    // Odadan çıkar
    for (const [clientId, c] of clients.entries()) {
        if (c.ws === ws) { handleLeaveRoom(ws, clientId, c.roomCode); break; }
    }
    // Auth'tan çıkar ve arkadaşlara offline bildir
    if (ws._authUserId) {
        const userName = ws._authUserName;
        const userId   = ws._authUserId;
        removeOnline(userName, ws);
        ws._authUserId = null;
        ws._authUserName = null;
        if (!isOnline(userName)) {
            try {
                const friends = await db.listFriends(userId);
                for (const f of friends) sendToUser(f, { type: 'friend_status', userName, online: false });
            } catch (e) { console.error('disconnect listFriends hata:', e && e.message); }
        }
    }
}

// ============================================================
//                          Bakım
// ============================================================
setInterval(() => {
    const now = Date.now();
    for (const [code, room] of rooms.entries()) {
        if (now - new Date(room.createdAt).getTime() > 24 * 60 * 60 * 1000) {
            rooms.delete(code);
            console.log(`Eski oda temizlendi: ${code} - "${room.roomName}"`);
        }
    }
    db.purgeOldTokens().catch(() => {});
}, 60 * 60 * 1000);

setInterval(() => {
    console.log(`\n=== VoLaura İstatistikleri ===`);
    console.log(`Aktif odalar: ${rooms.size}`);
    console.log(`Online kullanıcı: ${onlineUsers.size}`);
    console.log(`==============================\n`);
}, 5 * 60 * 1000);

// ============================================================
//        Discord-benzeri sunucu / kanal / mesaj / DM
// ============================================================
function requireAuth(ws) {
    if (!ws._authUserId) {
        ws.send(JSON.stringify({ type: 'error', message: 'Önce giriş yapmalısın.' }));
        return false;
    }
    return true;
}

function sendErr(ws, message) {
    ws.send(JSON.stringify({ type: 'error', message }));
}

async function broadcastToServerMembers(serverId, payload, exceptWs = null) {
    try {
        const members = await db.listServerMembers(serverId);
        const json = JSON.stringify(payload);
        for (const m of members) {
            const set = onlineUsers.get(norm(m.username));
            if (!set) continue;
            for (const w of set) {
                if (w !== exceptWs && w.readyState === WebSocket.OPEN) w.send(json);
            }
        }
    } catch (e) {
        console.error('broadcastToServerMembers:', e && e.message);
    }
}

// ---- Servers ----
async function sendServersList(ws) {
    const list = await db.listUserServers(ws._authUserId);
    ws.send(JSON.stringify({
        type: 'servers_list',
        servers: list.map(s => ({
            id: s.id, name: s.name, inviteCode: s.invite_code,
            iconUrl: s.icon_url, role: s.role,
        })),
    }));
}
async function handleListServers(ws) {
    if (!requireAuth(ws)) return;
    return sendServersList(ws);
}

async function handleCreateServer(ws, msg) {
    if (!requireAuth(ws)) return;
    const name = String(msg.name || '').trim();
    if (name.length < 2 || name.length > 40) return sendErr(ws, 'Sunucu adı 2-40 karakter olmalı.');
    try {
        const srv = await db.createServer({ name, ownerId: ws._authUserId });
        ws.send(JSON.stringify({ type: 'server_created',
            server: { id: srv.id, name: srv.name, inviteCode: srv.invite_code, role: 'owner' } }));
        await sendServersList(ws);
    } catch (e) {
        console.error('createServer:', e && e.message);
        sendErr(ws, 'Sunucu oluşturulamadı.');
    }
}

async function handleJoinServer(ws, msg) {
    if (!requireAuth(ws)) return;
    const code = String(msg.inviteCode || '').trim().toUpperCase();
    if (!code) return sendErr(ws, 'Davet kodu gerekli.');
    const r = await db.joinServerByInvite(ws._authUserId, code);
    if (!r.ok) return sendErr(ws, 'Geçersiz davet kodu.');
    const s = r.server;
    ws.send(JSON.stringify({ type: 'server_joined',
        server: { id: s.id, name: s.name, inviteCode: s.invite_code } }));
    await sendServersList(ws);
    // Üye listesini güncellemek için diğer üyelere bildir
    await broadcastToServerMembers(s.id,
        { type: 'member_joined', serverId: s.id,
          member: { id: ws._authUserId, username: ws._authUserName, role: 'member' } }, ws);
}

async function handleLeaveServer(ws, msg) {
    if (!requireAuth(ws)) return;
    const serverId = Number(msg.serverId);
    if (!serverId) return sendErr(ws, 'serverId gerekli.');
    const r = await db.leaveServer(ws._authUserId, serverId);
    if (!r.ok) {
        return sendErr(ws, r.reason === 'owner_cannot_leave'
            ? 'Sahibi sunucudan ayrılamaz; sunucuyu silebilirsin.'
            : 'Ayrılma başarısız.');
    }
    ws.send(JSON.stringify({ type: 'server_left', serverId }));
    await sendServersList(ws);
    await broadcastToServerMembers(serverId,
        { type: 'member_left', serverId, userId: ws._authUserId, username: ws._authUserName });
}

async function handleDeleteServer(ws, msg) {
    if (!requireAuth(ws)) return;
    const serverId = Number(msg.serverId);
    if (!serverId) return sendErr(ws, 'serverId gerekli.');
    // Silmeden önce üyelere bildirelim
    const members = await db.listServerMembers(serverId);
    const r = await db.deleteServer(serverId, ws._authUserId);
    if (!r.ok) return sendErr(ws, r.reason === 'not_owner' ? 'Yetkin yok.' : 'Silinemedi.');
    const payload = JSON.stringify({ type: 'server_deleted', serverId });
    for (const m of members) {
        const set = onlineUsers.get(norm(m.username));
        if (!set) continue;
        for (const w of set) if (w.readyState === WebSocket.OPEN) w.send(payload);
    }
    await sendServersList(ws);
}

async function handleRenameServer(ws, msg) {
    if (!requireAuth(ws)) return;
    const serverId = Number(msg.serverId);
    const name = String(msg.name || '').trim();
    if (!serverId || name.length < 2 || name.length > 40)
        return sendErr(ws, 'Geçersiz istek.');
    const r = await db.renameServer(serverId, ws._authUserId, name);
    if (!r.ok) return sendErr(ws, 'Yetkin yok.');
    await broadcastToServerMembers(serverId, { type: 'server_renamed', serverId, name });
}

async function handleListMembers(ws, msg) {
    if (!requireAuth(ws)) return;
    const serverId = Number(msg.serverId);
    if (!serverId) return sendErr(ws, 'serverId gerekli.');
    if (!await db.isServerMember(serverId, ws._authUserId)) return sendErr(ws, 'Üyesi değilsin.');
    const members = await db.listServerMembers(serverId);
    ws.send(JSON.stringify({
        type: 'members_list', serverId,
        members: members.map(m => ({
            id: m.id, username: m.username, role: m.role,
            online: isOnline(m.username),
        })),
    }));
}

// ---- Channels ----
async function handleListChannels(ws, msg) {
    if (!requireAuth(ws)) return;
    const serverId = Number(msg.serverId);
    if (!serverId) return sendErr(ws, 'serverId gerekli.');
    if (!await db.isServerMember(serverId, ws._authUserId)) return sendErr(ws, 'Üyesi değilsin.');
    const channels = await db.listChannels(serverId);
    ws.send(JSON.stringify({ type: 'channels_list', serverId, channels }));
}

async function handleCreateChannel(ws, msg) {
    if (!requireAuth(ws)) return;
    const serverId = Number(msg.serverId);
    const name = String(msg.name || '').trim();
    const type = msg.channelType === 'voice' ? 'voice' : 'text';
    if (!serverId || name.length < 1 || name.length > 40)
        return sendErr(ws, 'Geçersiz istek.');
    const role = await db.getServerMemberRole(serverId, ws._authUserId);
    if (role !== 'owner' && role !== 'admin') return sendErr(ws, 'Yetkin yok.');
    const ch = await db.addChannel(serverId, name, type);
    await broadcastToServerMembers(serverId, { type: 'channel_created', serverId, channel: ch });
}

async function handleDeleteChannel(ws, msg) {
    if (!requireAuth(ws)) return;
    const channelId = Number(msg.channelId);
    if (!channelId) return sendErr(ws, 'channelId gerekli.');
    const ch = await db.getChannelById(channelId);
    if (!ch) return sendErr(ws, 'Kanal yok.');
    const role = await db.getServerMemberRole(ch.server_id, ws._authUserId);
    if (role !== 'owner' && role !== 'admin') return sendErr(ws, 'Yetkin yok.');
    await db.deleteChannel(channelId);
    await broadcastToServerMembers(ch.server_id,
        { type: 'channel_deleted', serverId: ch.server_id, channelId });
}

async function handleRenameChannel(ws, msg) {
    if (!requireAuth(ws)) return;
    const channelId = Number(msg.channelId);
    const name = String(msg.name || '').trim();
    if (!channelId || name.length < 1 || name.length > 40) return sendErr(ws, 'Geçersiz istek.');
    const ch = await db.getChannelById(channelId);
    if (!ch) return sendErr(ws, 'Kanal yok.');
    const role = await db.getServerMemberRole(ch.server_id, ws._authUserId);
    if (role !== 'owner' && role !== 'admin') return sendErr(ws, 'Yetkin yok.');
    await db.renameChannel(channelId, name);
    await broadcastToServerMembers(ch.server_id,
        { type: 'channel_renamed', serverId: ch.server_id, channelId, name });
}

// ---- Channel messages ----
async function handleListMessages(ws, msg) {
    if (!requireAuth(ws)) return;
    const channelId = Number(msg.channelId);
    if (!channelId) return sendErr(ws, 'channelId gerekli.');
    const ch = await db.getChannelById(channelId);
    if (!ch) return sendErr(ws, 'Kanal yok.');
    if (!await db.isServerMember(ch.server_id, ws._authUserId)) return sendErr(ws, 'Üyesi değilsin.');
    const before = msg.beforeId ? Number(msg.beforeId) : null;
    const limit = msg.limit ? Number(msg.limit) : 50;
    const messages = await db.listMessages(channelId, { beforeId: before, limit });
    ws.send(JSON.stringify({ type: 'messages_list', channelId, messages }));
}

async function handleSendMessage(ws, msg) {
    if (!requireAuth(ws)) return;
    const channelId = Number(msg.channelId);
    const content = String(msg.content || '').trim();
    if (!channelId || !content) return sendErr(ws, 'Boş mesaj.');
    // Resim ekleri base64 olarak gömülü olabilir — 1.5 MB limit
    if (content.length > 1500000) return sendErr(ws, 'Mesaj çok uzun.');
    const ch = await db.getChannelById(channelId);
    if (!ch) return sendErr(ws, 'Kanal yok.');
    if (ch.type !== 'text') return sendErr(ws, 'Metin kanalı değil.');
    if (!await db.isServerMember(ch.server_id, ws._authUserId)) return sendErr(ws, 'Üyesi değilsin.');
    const m = await db.addMessage(channelId, ws._authUserId, content);
    await broadcastToServerMembers(ch.server_id,
        { type: 'message_received', serverId: ch.server_id, channelId, message: m });
}

async function handleDeleteMessage(ws, msg) {
    if (!requireAuth(ws)) return;
    const messageId = Number(msg.messageId);
    if (!messageId) return sendErr(ws, 'messageId gerekli.');
    const r = await db.deleteMessage(messageId, ws._authUserId);
    if (!r) return sendErr(ws, 'Silinemedi (yetki/bulunamadı).');
    const ch = await db.getChannelById(r.channel_id);
    if (ch) await broadcastToServerMembers(ch.server_id,
        { type: 'message_deleted', serverId: ch.server_id, channelId: r.channel_id, messageId });
}

async function handleEditMessage(ws, msg) {
    if (!requireAuth(ws)) return;
    const messageId = Number(msg.messageId);
    const content = String(msg.content || '').trim();
    if (!messageId || !content) return sendErr(ws, 'Geçersiz istek.');
    const r = await db.editMessage(messageId, ws._authUserId, content);
    if (!r) return sendErr(ws, 'Düzenlenemedi.');
    const ch = await db.getChannelById(r.channel_id);
    if (ch) await broadcastToServerMembers(ch.server_id, {
        type: 'message_edited', serverId: ch.server_id, channelId: r.channel_id,
        messageId, content: r.content, editedAt: r.edited_at,
    });
}

// ---- DM ----
async function handleListDmThreads(ws) {
    if (!requireAuth(ws)) return;
    const threads = await db.listDMThreads(ws._authUserId);
    ws.send(JSON.stringify({
        type: 'dm_threads',
        threads: threads.map(t => ({
            userId: t.id, username: t.username, lastAt: t.last_at,
            online: isOnline(t.username),
            unreadCount: Number(t.unread_count || 0),
        })),
    }));
}

async function handleListDmMessages(ws, msg) {
    if (!requireAuth(ws)) return;
    const peerUsername = String(msg.peerUsername || '').trim();
    if (!peerUsername) return sendErr(ws, 'peerUsername gerekli.');
    const peer = await db.findUserByUsername(peerUsername);
    if (!peer) return sendErr(ws, 'Kullanıcı bulunamadı.');
    const before = msg.beforeId ? Number(msg.beforeId) : null;
    const limit = msg.limit ? Number(msg.limit) : 50;
    const messages = await db.listDirectMessages(ws._authUserId, peer.id,
        { beforeId: before, limit });
    ws.send(JSON.stringify({ type: 'dm_messages', peerUsername, messages }));
}

async function handleSendDm(ws, msg) {
    if (!requireAuth(ws)) return;
    const peerUsername = String(msg.peerUsername || '').trim();
    const content = String(msg.content || '').trim();
    if (!peerUsername || !content) return sendErr(ws, 'Boş DM.');
    // E2E ciphertext base64 olduğu için biraz daha uzun olabilir
    // E2E ciphertext + resim ekleri için 1.5 MB
    if (content.length > 1500000) return sendErr(ws, 'Mesaj çok uzun.');
    const peer = await db.findUserByUsername(peerUsername);
    if (!peer) return sendErr(ws, 'Kullanıcı bulunamadı.');
    if (peer.id === ws._authUserId) return sendErr(ws, 'Kendine DM atamazsın.');
    // Sadece arkadaşlar DM atabilsin (istersen kaldır)
    if (!await db.areFriends(ws._authUserId, peer.id))
        return sendErr(ws, 'Sadece arkadaşlarına DM atabilirsin.');
    // E2E meta — opsiyonel; eski client'lar göndermez, plaintext kalır
    const isEncrypted = !!msg.isEncrypted;
    const nonce = isEncrypted ? String(msg.nonce || '') : null;
    const senderPub = isEncrypted ? String(msg.senderPub || '') : null;
    if (isEncrypted && (!nonce || !senderPub))
        return sendErr(ws, 'E2E meta eksik.');
    const m = await db.sendDirectMessage(ws._authUserId, peer.id, content,
        { isEncrypted, nonce, senderPub });
    // Hem gönderene hem alıcıya gönder
    const payload = JSON.stringify({ type: 'dm_received', message: m });
    const sendBoth = (uname) => {
        const set = onlineUsers.get(norm(uname));
        if (!set) return;
        for (const w of set) if (w.readyState === WebSocket.OPEN) w.send(payload);
    };
    sendBoth(ws._authUserName);
    sendBoth(peerUsername);
}

async function handleMarkDmRead(ws, msg) {
    if (!requireAuth(ws)) return;
    const peerUsername = String(msg.peerUsername || '').trim();
    if (!peerUsername) return;
    const peer = await db.findUserByUsername(peerUsername);
    if (!peer) return;
    await db.markDMRead(ws._authUserId, peer.id);
}

function sendToUserIds(userIds, payload) {
    const data = typeof payload === 'string' ? payload : JSON.stringify(payload);
    const seen = new Set();
    for (const uid of userIds) {
        if (seen.has(uid)) continue;
        seen.add(uid);
        for (const [, set] of onlineUsers.entries()) {
            for (const w of set) {
                if (w._authUserId === uid && w.readyState === WebSocket.OPEN) w.send(data);
            }
        }
    }
}

async function handleDeleteDm(ws, msg) {
    if (!requireAuth(ws)) return;
    const messageId = Number(msg.messageId);
    if (!messageId) return sendErr(ws, 'messageId gerekli.');
    const r = await db.deleteDirectMessage(messageId, ws._authUserId);
    if (!r.ok) return sendErr(ws, r.reason === 'forbidden' ? 'Yetkisiz.' : 'Silinemedi.');
    // Kullanıcı adlarını bul
    const [us, ur] = await Promise.all([
        db.findUserById(r.senderId), db.findUserById(r.recipientId),
    ]);
    const peerOf = (uid) => (uid === r.senderId ? ur : us);
    const payloadFor = (uid) => JSON.stringify({
        type: 'dm_deleted', messageId,
        peerUsername: (peerOf(uid) || {}).username || '',
    });
    // Her iki tarafa kendi perspektifiyle bildir
    for (const [, set] of onlineUsers.entries()) {
        for (const w of set) {
            if (w.readyState !== WebSocket.OPEN) continue;
            if (w._authUserId === r.senderId)    w.send(payloadFor(r.senderId));
            if (w._authUserId === r.recipientId) w.send(payloadFor(r.recipientId));
        }
    }
}

async function handleEditDm(ws, msg) {
    if (!requireAuth(ws)) return;
    const messageId = Number(msg.messageId);
    const content = String(msg.content || '');
    if (!messageId) return sendErr(ws, 'messageId gerekli.');
    const r = await db.editDirectMessage(messageId, ws._authUserId, content);
    if (!r.ok) return sendErr(ws, r.reason === 'forbidden' ? 'Yetkisiz.' : 'Düzenlenemedi.');
    const [us, ur] = await Promise.all([
        db.findUserById(r.senderId), db.findUserById(r.recipientId),
    ]);
    const peerOf = (uid) => (uid === r.senderId ? ur : us);
    const payloadFor = (uid) => JSON.stringify({
        type: 'dm_edited', messageId, content: r.content,
        peerUsername: (peerOf(uid) || {}).username || '',
    });
    for (const [, set] of onlineUsers.entries()) {
        for (const w of set) {
            if (w.readyState !== WebSocket.OPEN) continue;
            if (w._authUserId === r.senderId)    w.send(payloadFor(r.senderId));
            if (w._authUserId === r.recipientId) w.send(payloadFor(r.recipientId));
        }
    }
}

// ---- Typing indicators (throttled, ephemeral) ----
async function handleTypingChannel(ws, msg) {
    if (!requireAuth(ws)) return;
    const channelId = Number(msg.channelId);
    if (!channelId) return;
    const ch = await db.getChannelById(channelId);
    if (!ch || ch.type !== 'text') return;
    if (!await db.isServerMember(ch.server_id, ws._authUserId)) return;
    const payload = {
        type: 'typing_channel', serverId: ch.server_id, channelId,
        userId: ws._authUserId, username: ws._authUserName,
    };
    await broadcastToServerMembers(ch.server_id, payload, ws);
}

async function handleTypingDm(ws, msg) {
    if (!requireAuth(ws)) return;
    const peerUsername = String(msg.peerUsername || '').trim();
    if (!peerUsername) return;
    const peer = await db.findUserByUsername(peerUsername);
    if (!peer) return;
    if (!await db.areFriends(ws._authUserId, peer.id)) return;
    const payload = JSON.stringify({
        type: 'typing_dm',
        fromUsername: ws._authUserName,
        peerUsername: peer.username,
    });
    const set = onlineUsers.get(norm(peer.username));
    if (set) for (const w of set) if (w.readyState === WebSocket.OPEN) w.send(payload);
}

// ============================================================
//                        Başlatma
// ============================================================
(async () => {
    try {
        await db.initSchema();
        console.log('DB şeması hazır.');
    } catch (e) {
        console.error('DB başlatılamadı:', e);
        process.exit(1);
    }
    httpServer.listen(WS_PORT, () => {
        console.log(`VoLaura WS+HTTP çalışıyor: ${WS_PORT}`);
        console.log(`Public BASE_URL: ${BASE_URL}`);
    });
})();
