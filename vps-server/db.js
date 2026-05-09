// VoLaura - Postgres (NeonDB) erişim katmanı.
// Şema otomatik oluşturulur (CREATE IF NOT EXISTS) ve tüm domain işlemleri burada açıklanır.
const { Pool } = require('pg');

const pool = new Pool({
    connectionString: process.env.DATABASE_URL,
    ssl: { rejectUnauthorized: false },
    max: 10,
    idleTimeoutMillis: 30000,
});

pool.on('error', (err) => {
    console.error('Postgres pool error:', err);
});

async function initSchema() {
    const ddl = `
    CREATE TABLE IF NOT EXISTS users (
        id SERIAL PRIMARY KEY,
        username TEXT NOT NULL UNIQUE,
        username_lower TEXT NOT NULL UNIQUE,
        email TEXT NOT NULL UNIQUE,
        email_lower TEXT NOT NULL UNIQUE,
        password_hash TEXT NOT NULL,
        password_salt TEXT NOT NULL,
        email_verified BOOLEAN NOT NULL DEFAULT FALSE,
        created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
    );

    -- 2FA sütunları (idempotent)
    ALTER TABLE users ADD COLUMN IF NOT EXISTS totp_secret TEXT;
    ALTER TABLE users ADD COLUMN IF NOT EXISTS totp_enabled BOOLEAN NOT NULL DEFAULT FALSE;
    ALTER TABLE users ADD COLUMN IF NOT EXISTS phone_e164 TEXT;
    ALTER TABLE users ADD COLUMN IF NOT EXISTS phone_verified BOOLEAN NOT NULL DEFAULT FALSE;
    ALTER TABLE users ADD COLUMN IF NOT EXISTS sms_2fa_enabled BOOLEAN NOT NULL DEFAULT FALSE;
    ALTER TABLE users ADD COLUMN IF NOT EXISTS email_2fa_enabled BOOLEAN NOT NULL DEFAULT FALSE;
    -- E2E DM şifreleme: X25519 public key (base64 32 byte). NULL ise henüz E2E hazır değil.
    ALTER TABLE users ADD COLUMN IF NOT EXISTS public_key TEXT;
    -- Yeni cihaz uyarısı için throttling: son uyarı zamanı
    ALTER TABLE users ADD COLUMN IF NOT EXISTS last_new_device_alert_at TIMESTAMPTZ;

    CREATE TABLE IF NOT EXISTS two_fa_codes (
        id SERIAL PRIMARY KEY,
        user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        kind TEXT NOT NULL,            -- 'phone_verify' | 'login_sms'
        code TEXT NOT NULL,            -- 6-digit plain (kısa ömür)
        expires_at TIMESTAMPTZ NOT NULL,
        used BOOLEAN NOT NULL DEFAULT FALSE,
        created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
    );
    CREATE INDEX IF NOT EXISTS idx_two_fa_codes_user ON two_fa_codes(user_id, kind, used);

    CREATE TABLE IF NOT EXISTS email_tokens (
        token TEXT PRIMARY KEY,
        user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        type TEXT NOT NULL CHECK (type IN ('verify','reset','enable_2fa')),
        expires_at TIMESTAMPTZ NOT NULL,
        used BOOLEAN NOT NULL DEFAULT FALSE,
        created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
    );
    -- Var olan tablolarda eski CHECK 'enable_2fa'ı izin vermiyor olabilir; günceller.
    ALTER TABLE email_tokens DROP CONSTRAINT IF EXISTS email_tokens_type_check;
    ALTER TABLE email_tokens ADD CONSTRAINT email_tokens_type_check
        CHECK (type IN ('verify','reset','enable_2fa'));
    CREATE INDEX IF NOT EXISTS idx_email_tokens_user ON email_tokens(user_id);
    CREATE INDEX IF NOT EXISTS idx_email_tokens_expires ON email_tokens(expires_at);

    CREATE TABLE IF NOT EXISTS friend_requests (
        id SERIAL PRIMARY KEY,
        from_user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        to_user_id   INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
        UNIQUE (from_user_id, to_user_id)
    );

    CREATE TABLE IF NOT EXISTS friendships (
        id SERIAL PRIMARY KEY,
        user_a_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        user_b_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
        UNIQUE (user_a_id, user_b_id),
        CHECK (user_a_id < user_b_id)
    );
    CREATE INDEX IF NOT EXISTS idx_friendships_a ON friendships(user_a_id);
    CREATE INDEX IF NOT EXISTS idx_friendships_b ON friendships(user_b_id);

    -- ==== Discord-benzeri sunucu/kanal/mesaj yapısı ====

    CREATE TABLE IF NOT EXISTS servers (
        id SERIAL PRIMARY KEY,
        name TEXT NOT NULL,
        owner_user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        invite_code TEXT NOT NULL UNIQUE,
        icon_url TEXT,
        created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
    );
    CREATE INDEX IF NOT EXISTS idx_servers_owner ON servers(owner_user_id);

    CREATE TABLE IF NOT EXISTS server_members (
        id SERIAL PRIMARY KEY,
        server_id INTEGER NOT NULL REFERENCES servers(id) ON DELETE CASCADE,
        user_id   INTEGER NOT NULL REFERENCES users(id)   ON DELETE CASCADE,
        role TEXT NOT NULL DEFAULT 'member' CHECK (role IN ('owner','admin','member')),
        joined_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
        UNIQUE (server_id, user_id)
    );
    CREATE INDEX IF NOT EXISTS idx_server_members_user   ON server_members(user_id);
    CREATE INDEX IF NOT EXISTS idx_server_members_server ON server_members(server_id);

    -- Bilinen cihazlar (yeni cihazdan giriş bildirimi için)
    CREATE TABLE IF NOT EXISTS known_devices (
        id SERIAL PRIMARY KEY,
        user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        fingerprint TEXT NOT NULL,
        ip TEXT,
        user_agent TEXT,
        first_seen TIMESTAMPTZ NOT NULL DEFAULT NOW(),
        last_seen TIMESTAMPTZ NOT NULL DEFAULT NOW(),
        UNIQUE (user_id, fingerprint)
    );
    CREATE INDEX IF NOT EXISTS idx_known_devices_user ON known_devices(user_id);

    -- Admin'in bir kullanıcının mesajlarına erişim talebi.
    -- Kullanıcı e-postasından onay/red kararı verir. Master parola olmadan,
    -- sadece kullanıcı 'approved' yaptığında admin tek seferlik export alabilir.
    CREATE TABLE IF NOT EXISTS message_access_grants (
        id BIGSERIAL PRIMARY KEY,
        token TEXT NOT NULL UNIQUE,
        user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        requested_by TEXT,
        status TEXT NOT NULL DEFAULT 'pending'
            CHECK (status IN ('pending','approved','denied','used','expired')),
        reason TEXT,
        created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
        expires_at TIMESTAMPTZ NOT NULL,
        decided_at TIMESTAMPTZ
    );
    CREATE INDEX IF NOT EXISTS idx_mag_user   ON message_access_grants(user_id);
    CREATE INDEX IF NOT EXISTS idx_mag_status ON message_access_grants(status);

    -- 2FA "30 gün bu cihaza güven" tokenları.
    -- 2FA başarılı olunca bu cihaz için sha256(token_hash) kaydedilir; 30 gün
    -- içinde aynı cihazdan girişte 2FA atlanır.
    CREATE TABLE IF NOT EXISTS trusted_devices_2fa (
        id SERIAL PRIMARY KEY,
        user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        token_hash TEXT NOT NULL,
        fingerprint TEXT,
        expires_at TIMESTAMPTZ NOT NULL,
        last_used_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
        created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
        UNIQUE (token_hash)
    );
    CREATE INDEX IF NOT EXISTS idx_trusted_2fa_user ON trusted_devices_2fa(user_id);
    CREATE INDEX IF NOT EXISTS idx_trusted_2fa_exp  ON trusted_devices_2fa(expires_at);

    -- Telefon/E-posta + kod ile parolasız giriş için: tek seferlik kodlar
    CREATE TABLE IF NOT EXISTS passwordless_login_codes (
        id SERIAL PRIMARY KEY,
        user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        channel TEXT NOT NULL CHECK (channel IN ('email','sms')),
        code_hash TEXT NOT NULL,
        expires_at TIMESTAMPTZ NOT NULL,
        used BOOLEAN NOT NULL DEFAULT FALSE,
        attempts INTEGER NOT NULL DEFAULT 0,
        created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
    );
    CREATE INDEX IF NOT EXISTS idx_pwl_user ON passwordless_login_codes(user_id, channel, used);

    CREATE TABLE IF NOT EXISTS channels (
        id SERIAL PRIMARY KEY,
        server_id INTEGER NOT NULL REFERENCES servers(id) ON DELETE CASCADE,
        name TEXT NOT NULL,
        type TEXT NOT NULL DEFAULT 'text' CHECK (type IN ('text','voice')),
        position INTEGER NOT NULL DEFAULT 0,
        created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
    );
    CREATE INDEX IF NOT EXISTS idx_channels_server ON channels(server_id);

    CREATE TABLE IF NOT EXISTS messages (
        id BIGSERIAL PRIMARY KEY,
        channel_id INTEGER NOT NULL REFERENCES channels(id) ON DELETE CASCADE,
        user_id    INTEGER NOT NULL REFERENCES users(id)    ON DELETE CASCADE,
        content TEXT NOT NULL,
        created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
        edited_at  TIMESTAMPTZ,
        deleted_at TIMESTAMPTZ
    );
    CREATE INDEX IF NOT EXISTS idx_messages_channel_created ON messages(channel_id, created_at DESC);

    CREATE TABLE IF NOT EXISTS direct_messages (
        id BIGSERIAL PRIMARY KEY,
        sender_id    INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        recipient_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        content TEXT NOT NULL,
        created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
        read_at    TIMESTAMPTZ,
        edited_at  TIMESTAMPTZ,
        deleted_at TIMESTAMPTZ
    );
    CREATE INDEX IF NOT EXISTS idx_dm_pair ON direct_messages(
        LEAST(sender_id, recipient_id), GREATEST(sender_id, recipient_id), created_at DESC
    );
    `;
    await pool.query(ddl);
    // Mevcut tablolar icin migration
    await pool.query(`ALTER TABLE direct_messages ADD COLUMN IF NOT EXISTS edited_at TIMESTAMPTZ`);
    // E2E DM: ciphertext zaten content'te saklanır (server hiç görmez); ek metadata
    await pool.query(`ALTER TABLE direct_messages ADD COLUMN IF NOT EXISTS is_encrypted BOOLEAN NOT NULL DEFAULT FALSE`);
    await pool.query(`ALTER TABLE direct_messages ADD COLUMN IF NOT EXISTS nonce TEXT`);
    await pool.query(`ALTER TABLE direct_messages ADD COLUMN IF NOT EXISTS sender_pub TEXT`);
}

// ---------- Helpers ----------
const norm = (s) => String(s || '').trim().toLowerCase();
function pair(aId, bId) { return aId < bId ? [aId, bId] : [bId, aId]; }

// ---------- Users ----------
async function findUserById(id) {
    const r = await pool.query('SELECT * FROM users WHERE id = $1', [id]);
    return r.rows[0] || null;
}
async function findUserByUsername(username) {
    const r = await pool.query('SELECT * FROM users WHERE username_lower = $1', [norm(username)]);
    return r.rows[0] || null;
}
async function findUserByEmail(email) {
    const r = await pool.query('SELECT * FROM users WHERE email_lower = $1', [norm(email)]);
    return r.rows[0] || null;
}
async function findUserByLogin(usernameOrEmailOrPhone) {
    const raw = String(usernameOrEmailOrPhone || '').trim();
    if (!raw) return null;
    const v = norm(raw);
    // Telefon biçimi: + ile başlayan veya tamamen rakam (boşluk/tire/parantez ayraçları)
    const stripped = raw.replace(/[\s\-\(\)]/g, '');
    const isPhoneLike = /^\+?\d{6,16}$/.test(stripped);
    let phoneE164 = null;
    if (isPhoneLike) {
        phoneE164 = stripped.startsWith('+') ? stripped : ('+' + stripped);
    }
    if (phoneE164) {
        const r = await pool.query(
            `SELECT * FROM users
              WHERE username_lower = $1 OR email_lower = $1 OR phone_e164 = $2
              LIMIT 1`, [v, phoneE164]);
        return r.rows[0] || null;
    }
    const r = await pool.query(
        'SELECT * FROM users WHERE username_lower = $1 OR email_lower = $1 LIMIT 1', [v]);
    return r.rows[0] || null;
}
async function createUser({ username, email, passwordHash, passwordSalt }) {
    const r = await pool.query(
        `INSERT INTO users (username, username_lower, email, email_lower, password_hash, password_salt)
         VALUES ($1,$2,$3,$4,$5,$6) RETURNING *`,
        [username, norm(username), email, norm(email), passwordHash, passwordSalt]
    );
    return r.rows[0];
}
async function setEmailVerified(userId) {
    await pool.query('UPDATE users SET email_verified = TRUE WHERE id = $1', [userId]);
}
async function updatePassword(userId, hash, salt) {
    await pool.query(
        'UPDATE users SET password_hash = $1, password_salt = $2 WHERE id = $3',
        [hash, salt, userId]);
}

// ---------- Tokens ----------
async function createToken({ userId, type, token, expiresAt }) {
    await pool.query(
        `INSERT INTO email_tokens (token, user_id, type, expires_at) VALUES ($1,$2,$3,$4)`,
        [token, userId, type, expiresAt]
    );
}
async function consumeToken(token, type) {
    // Find first; if usable, mark used and return user
    const r = await pool.query(
        `SELECT et.*, u.id AS uid, u.username FROM email_tokens et
         JOIN users u ON u.id = et.user_id
         WHERE et.token = $1 AND et.type = $2`,
        [token, type]
    );
    const row = r.rows[0];
    if (!row) return { ok: false, reason: 'not_found' };
    if (row.used) return { ok: false, reason: 'used' };
    if (new Date(row.expires_at).getTime() < Date.now()) return { ok: false, reason: 'expired' };
    await pool.query('UPDATE email_tokens SET used = TRUE WHERE token = $1', [token]);
    return { ok: true, userId: row.uid, username: row.username };
}
async function purgeOldTokens() {
    await pool.query(`DELETE FROM email_tokens WHERE expires_at < NOW() - INTERVAL '7 days'`);
}

// ---------- Friend requests ----------
async function getOrError() { /* placeholder for consistency */ }

async function listIncomingRequests(userId) {
    const r = await pool.query(
        `SELECT u.username FROM friend_requests fr
         JOIN users u ON u.id = fr.from_user_id
         WHERE fr.to_user_id = $1
         ORDER BY fr.created_at DESC`, [userId]);
    return r.rows.map(x => x.username);
}
async function listOutgoingRequests(userId) {
    const r = await pool.query(
        `SELECT u.username FROM friend_requests fr
         JOIN users u ON u.id = fr.to_user_id
         WHERE fr.from_user_id = $1
         ORDER BY fr.created_at DESC`, [userId]);
    return r.rows.map(x => x.username);
}
async function hasOutgoingRequest(fromId, toId) {
    const r = await pool.query(
        'SELECT 1 FROM friend_requests WHERE from_user_id = $1 AND to_user_id = $2', [fromId, toId]);
    return r.rowCount > 0;
}
async function hasIncomingRequest(toId, fromId) {
    return hasOutgoingRequest(fromId, toId);
}
async function addFriendRequest(fromId, toId) {
    await pool.query(
        `INSERT INTO friend_requests (from_user_id, to_user_id) VALUES ($1,$2)
         ON CONFLICT DO NOTHING`, [fromId, toId]);
}
async function removeFriendRequest(fromId, toId) {
    await pool.query(
        'DELETE FROM friend_requests WHERE from_user_id = $1 AND to_user_id = $2',
        [fromId, toId]);
}
async function removeAnyRequestBetween(aId, bId) {
    await pool.query(
        `DELETE FROM friend_requests WHERE
         (from_user_id = $1 AND to_user_id = $2) OR
         (from_user_id = $2 AND to_user_id = $1)`, [aId, bId]);
}

// ---------- Friendships ----------
async function areFriends(aId, bId) {
    const [lo, hi] = pair(aId, bId);
    const r = await pool.query(
        'SELECT 1 FROM friendships WHERE user_a_id = $1 AND user_b_id = $2', [lo, hi]);
    return r.rowCount > 0;
}
async function addFriendship(aId, bId) {
    const [lo, hi] = pair(aId, bId);
    await pool.query(
        `INSERT INTO friendships (user_a_id, user_b_id) VALUES ($1,$2)
         ON CONFLICT DO NOTHING`, [lo, hi]);
}
async function removeFriendship(aId, bId) {
    const [lo, hi] = pair(aId, bId);
    await pool.query(
        'DELETE FROM friendships WHERE user_a_id = $1 AND user_b_id = $2', [lo, hi]);
}
async function listFriends(userId) {
    const r = await pool.query(
        `SELECT CASE WHEN f.user_a_id = $1 THEN ub.username ELSE ua.username END AS username
         FROM friendships f
         JOIN users ua ON ua.id = f.user_a_id
         JOIN users ub ON ub.id = f.user_b_id
         WHERE f.user_a_id = $1 OR f.user_b_id = $1
         ORDER BY 1`, [userId]);
    return r.rows.map(x => x.username);
}

// ==========================================================================
// Discord-benzeri yapı: servers / channels / messages / direct_messages
// ==========================================================================

function randomInviteCode(len = 8) {
    const alphabet = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789';
    let out = '';
    for (let i = 0; i < len; ++i) out += alphabet[Math.floor(Math.random() * alphabet.length)];
    return out;
}

// ---------- Servers ----------
async function createServer({ name, ownerId, iconUrl = null }) {
    const client = await pool.connect();
    try {
        await client.query('BEGIN');
        // Invite code çakışırsa bir kaç kez dene
        let code = randomInviteCode();
        for (let i = 0; i < 5; ++i) {
            const r = await client.query('SELECT 1 FROM servers WHERE invite_code = $1', [code]);
            if (r.rowCount === 0) break;
            code = randomInviteCode();
        }
        const srv = await client.query(
            `INSERT INTO servers (name, owner_user_id, invite_code, icon_url)
             VALUES ($1,$2,$3,$4) RETURNING *`,
            [name, ownerId, code, iconUrl]
        );
        const server = srv.rows[0];
        await client.query(
            `INSERT INTO server_members (server_id, user_id, role) VALUES ($1,$2,'owner')`,
            [server.id, ownerId]
        );
        // Varsayılan kanallar: #genel (text), Sesli Sohbet (voice)
        await client.query(
            `INSERT INTO channels (server_id, name, type, position) VALUES
             ($1,'genel','text',0),($1,'Sesli Sohbet','voice',1)`,
            [server.id]
        );
        await client.query('COMMIT');
        return server;
    } catch (e) {
        await client.query('ROLLBACK');
        throw e;
    } finally {
        client.release();
    }
}

async function getServerById(serverId) {
    const r = await pool.query('SELECT * FROM servers WHERE id = $1', [serverId]);
    return r.rows[0] || null;
}

async function getServerByInvite(inviteCode) {
    const r = await pool.query('SELECT * FROM servers WHERE invite_code = $1',
        [String(inviteCode || '').trim().toUpperCase()]);
    return r.rows[0] || null;
}

async function listUserServers(userId) {
    const r = await pool.query(
        `SELECT s.id, s.name, s.invite_code, s.icon_url, sm.role
           FROM server_members sm
           JOIN servers s ON s.id = sm.server_id
          WHERE sm.user_id = $1
          ORDER BY sm.joined_at ASC`, [userId]);
    return r.rows;
}

async function isServerMember(serverId, userId) {
    const r = await pool.query(
        'SELECT 1 FROM server_members WHERE server_id = $1 AND user_id = $2',
        [serverId, userId]);
    return r.rowCount > 0;
}

async function getServerMemberRole(serverId, userId) {
    const r = await pool.query(
        'SELECT role FROM server_members WHERE server_id = $1 AND user_id = $2',
        [serverId, userId]);
    return r.rows[0]?.role || null;
}

async function joinServerByInvite(userId, inviteCode) {
    const srv = await getServerByInvite(inviteCode);
    if (!srv) return { ok: false, reason: 'not_found' };
    await pool.query(
        `INSERT INTO server_members (server_id, user_id, role) VALUES ($1,$2,'member')
         ON CONFLICT DO NOTHING`, [srv.id, userId]);
    return { ok: true, server: srv };
}

async function leaveServer(userId, serverId) {
    const role = await getServerMemberRole(serverId, userId);
    if (role === 'owner') return { ok: false, reason: 'owner_cannot_leave' };
    await pool.query(
        'DELETE FROM server_members WHERE server_id = $1 AND user_id = $2',
        [serverId, userId]);
    return { ok: true };
}

async function deleteServer(serverId, userId) {
    const srv = await getServerById(serverId);
    if (!srv) return { ok: false, reason: 'not_found' };
    if (srv.owner_user_id !== userId) return { ok: false, reason: 'not_owner' };
    await pool.query('DELETE FROM servers WHERE id = $1', [serverId]);
    return { ok: true };
}

async function renameServer(serverId, userId, newName) {
    const role = await getServerMemberRole(serverId, userId);
    if (role !== 'owner' && role !== 'admin') return { ok: false, reason: 'forbidden' };
    await pool.query('UPDATE servers SET name = $1 WHERE id = $2', [newName, serverId]);
    return { ok: true };
}

async function listServerMembers(serverId) {
    const r = await pool.query(
        `SELECT u.id, u.username, sm.role, sm.joined_at
           FROM server_members sm
           JOIN users u ON u.id = sm.user_id
          WHERE sm.server_id = $1
          ORDER BY CASE sm.role WHEN 'owner' THEN 0 WHEN 'admin' THEN 1 ELSE 2 END,
                   u.username_lower`, [serverId]);
    return r.rows;
}

// ---------- Channels ----------
async function addChannel(serverId, name, type = 'text') {
    const pos = await pool.query(
        'SELECT COALESCE(MAX(position),-1)+1 AS p FROM channels WHERE server_id = $1', [serverId]);
    const r = await pool.query(
        `INSERT INTO channels (server_id, name, type, position)
         VALUES ($1,$2,$3,$4) RETURNING *`,
        [serverId, name, type, pos.rows[0].p]);
    return r.rows[0];
}

async function listChannels(serverId) {
    const r = await pool.query(
        'SELECT * FROM channels WHERE server_id = $1 ORDER BY position, id', [serverId]);
    return r.rows;
}

async function getChannelById(channelId) {
    const r = await pool.query('SELECT * FROM channels WHERE id = $1', [channelId]);
    return r.rows[0] || null;
}

async function deleteChannel(channelId) {
    await pool.query('DELETE FROM channels WHERE id = $1', [channelId]);
}

async function renameChannel(channelId, newName) {
    await pool.query('UPDATE channels SET name = $1 WHERE id = $2', [newName, channelId]);
}

// ---------- Messages (channel) ----------
async function addMessage(channelId, userId, content) {
    const r = await pool.query(
        `INSERT INTO messages (channel_id, user_id, content)
         VALUES ($1,$2,$3)
         RETURNING id, channel_id, user_id, content, created_at`,
        [channelId, userId, content]);
    const msg = r.rows[0];
    const u = await pool.query('SELECT username FROM users WHERE id = $1', [userId]);
    msg.username = u.rows[0]?.username || '';
    return msg;
}

async function listMessages(channelId, { beforeId = null, limit = 50 } = {}) {
    limit = Math.min(Math.max(1, limit | 0), 200);
    const params = [channelId];
    let where = 'm.channel_id = $1 AND m.deleted_at IS NULL';
    if (beforeId) { params.push(beforeId); where += ` AND m.id < $${params.length}`; }
    params.push(limit);
    const r = await pool.query(
        `SELECT m.id, m.channel_id, m.user_id, m.content, m.created_at, m.edited_at,
                u.username
           FROM messages m JOIN users u ON u.id = m.user_id
          WHERE ${where}
          ORDER BY m.id DESC
          LIMIT $${params.length}`, params);
    return r.rows.reverse(); // eski -> yeni
}

async function deleteMessage(messageId, userId) {
    // Sadece sahibi silebilir (server rolü kontrolü server.js'te)
    const r = await pool.query(
        `UPDATE messages SET deleted_at = NOW()
           WHERE id = $1 AND user_id = $2 AND deleted_at IS NULL
           RETURNING id, channel_id`, [messageId, userId]);
    return r.rows[0] || null;
}

async function editMessage(messageId, userId, newContent) {
    const r = await pool.query(
        `UPDATE messages SET content = $3, edited_at = NOW()
           WHERE id = $1 AND user_id = $2 AND deleted_at IS NULL
           RETURNING id, channel_id, content, edited_at`,
        [messageId, userId, newContent]);
    return r.rows[0] || null;
}

// ---------- Direct Messages ----------
async function sendDirectMessage(senderId, recipientId, content,
                                 { isEncrypted = false, nonce = null, senderPub = null } = {}) {
    const r = await pool.query(
        `INSERT INTO direct_messages (sender_id, recipient_id, content, is_encrypted, nonce, sender_pub)
         VALUES ($1,$2,$3,$4,$5,$6)
         RETURNING id, sender_id, recipient_id, content, created_at, is_encrypted, nonce, sender_pub`,
        [senderId, recipientId, content, !!isEncrypted, nonce, senderPub]);
    const msg = r.rows[0];
    const u = await pool.query(
        'SELECT id, username FROM users WHERE id IN ($1,$2)',
        [senderId, recipientId]);
    const byId = Object.fromEntries(u.rows.map(x => [x.id, x.username]));
    msg.sender_username    = byId[senderId]    || '';
    msg.recipient_username = byId[recipientId] || '';
    return msg;
}

async function listDirectMessages(userIdA, userIdB, { beforeId = null, limit = 50 } = {}) {
    limit = Math.min(Math.max(1, limit | 0), 200);
    const params = [userIdA, userIdB];
    let where = `((sender_id = $1 AND recipient_id = $2) OR (sender_id = $2 AND recipient_id = $1))
                 AND deleted_at IS NULL`;
    if (beforeId) { params.push(beforeId); where += ` AND id < $${params.length}`; }
    params.push(limit);
    const r = await pool.query(
        `SELECT dm.id, dm.sender_id, dm.recipient_id, dm.content, dm.created_at, dm.read_at, dm.edited_at,
                dm.is_encrypted, dm.nonce, dm.sender_pub,
                us.username AS sender_username,
                ur.username AS recipient_username
           FROM direct_messages dm
           JOIN users us ON us.id = dm.sender_id
           JOIN users ur ON ur.id = dm.recipient_id
          WHERE ${where}
          ORDER BY dm.id DESC
          LIMIT $${params.length}`, params);
    return r.rows.reverse();
}

async function listDMThreads(userId) {
    // Son mesaj zamanına göre sohbet ettiği kullanıcılar
    const r = await pool.query(
        `WITH pairs AS (
            SELECT
                CASE WHEN sender_id = $1 THEN recipient_id ELSE sender_id END AS peer_id,
                MAX(created_at) AS last_at
              FROM direct_messages
             WHERE (sender_id = $1 OR recipient_id = $1) AND deleted_at IS NULL
             GROUP BY peer_id
         )
         SELECT u.id, u.username, p.last_at,
                (SELECT COUNT(*) FROM direct_messages dm
                   WHERE dm.sender_id = p.peer_id
                     AND dm.recipient_id = $1
                     AND dm.read_at IS NULL
                     AND dm.deleted_at IS NULL) AS unread_count
           FROM pairs p JOIN users u ON u.id = p.peer_id
          ORDER BY p.last_at DESC`, [userId]);
    return r.rows;
}

async function markDMRead(userId, peerId) {
    await pool.query(
        `UPDATE direct_messages SET read_at = NOW()
           WHERE recipient_id = $1 AND sender_id = $2 AND read_at IS NULL`,
        [userId, peerId]);
}

async function getDirectMessage(messageId) {
    const r = await pool.query(
        `SELECT id, sender_id, recipient_id, content, created_at, deleted_at
           FROM direct_messages WHERE id = $1`, [messageId]);
    return r.rows[0] || null;
}

async function deleteDirectMessage(messageId, requesterId) {
    const msg = await getDirectMessage(messageId);
    if (!msg) return { ok: false, reason: 'not_found' };
    if (msg.deleted_at) return { ok: false, reason: 'already_deleted' };
    if (msg.sender_id !== requesterId) return { ok: false, reason: 'forbidden' };
    await pool.query(
        `UPDATE direct_messages SET deleted_at = NOW() WHERE id = $1`, [messageId]);
    return { ok: true, senderId: msg.sender_id, recipientId: msg.recipient_id };
}

async function editDirectMessage(messageId, requesterId, newContent) {
    const msg = await getDirectMessage(messageId);
    if (!msg) return { ok: false, reason: 'not_found' };
    if (msg.deleted_at) return { ok: false, reason: 'deleted' };
    if (msg.sender_id !== requesterId) return { ok: false, reason: 'forbidden' };
    const c = String(newContent || '').trim();
    if (c.length < 1 || c.length > 4000) return { ok: false, reason: 'invalid_content' };
    await pool.query(
        `UPDATE direct_messages SET content = $1, edited_at = NOW() WHERE id = $2`,
        [c, messageId]);
    return { ok: true, senderId: msg.sender_id, recipientId: msg.recipient_id, content: c };
}

// ======================= 2FA helpers =======================

async function getUserSecurity(userId) {
    const r = await pool.query(
        `SELECT id, username, email, email_verified,
                totp_secret, totp_enabled,
                phone_e164, phone_verified, sms_2fa_enabled,
                email_2fa_enabled
           FROM users WHERE id = $1`, [userId]);
    return r.rows[0] || null;
}
async function setEmail2fa(userId, enabled) {
    await pool.query(`UPDATE users SET email_2fa_enabled = $1 WHERE id = $2`, [!!enabled, userId]);
}

async function setTotpSecret(userId, secret) {
    await pool.query(`UPDATE users SET totp_secret = $1, totp_enabled = FALSE WHERE id = $2`,
        [secret, userId]);
}
async function enableTotp(userId) {
    await pool.query(`UPDATE users SET totp_enabled = TRUE WHERE id = $1`, [userId]);
}
async function disableTotp(userId) {
    await pool.query(`UPDATE users SET totp_enabled = FALSE, totp_secret = NULL WHERE id = $1`, [userId]);
}

async function setPhoneNumber(userId, e164) {
    await pool.query(`UPDATE users SET phone_e164 = $1, phone_verified = FALSE WHERE id = $2`,
        [e164, userId]);
}
async function setPhoneVerified(userId) {
    await pool.query(`UPDATE users SET phone_verified = TRUE WHERE id = $1`, [userId]);
}
async function setSms2fa(userId, enabled) {
    await pool.query(`UPDATE users SET sms_2fa_enabled = $1 WHERE id = $2`, [!!enabled, userId]);
}

async function createTwoFaCode(userId, kind, code, ttlSeconds = 300) {
    // Önce kullanıcının aynı kind'deki bekleyen kodlarını geçersiz kıl
    await pool.query(
        `UPDATE two_fa_codes SET used = TRUE WHERE user_id = $1 AND kind = $2 AND used = FALSE`,
        [userId, kind]);
    await pool.query(
        `INSERT INTO two_fa_codes (user_id, kind, code, expires_at)
         VALUES ($1, $2, $3, NOW() + ($4 || ' seconds')::INTERVAL)`,
        [userId, kind, code, String(ttlSeconds)]);
}

async function consumeTwoFaCode(userId, kind, code) {
    const r = await pool.query(
        `SELECT id FROM two_fa_codes
          WHERE user_id = $1 AND kind = $2 AND code = $3
            AND used = FALSE AND expires_at > NOW()
          ORDER BY id DESC LIMIT 1`,
        [userId, kind, code]);
    if (!r.rows[0]) return false;
    await pool.query(`UPDATE two_fa_codes SET used = TRUE WHERE id = $1`, [r.rows[0].id]);
    return true;
}

module.exports = {
    pool,
    initSchema,
    // 2FA
    getUserSecurity, setTotpSecret, enableTotp, disableTotp,
    setPhoneNumber, setPhoneVerified, setSms2fa, setEmail2fa,
    createTwoFaCode, consumeTwoFaCode,
    // users
    findUserById, findUserByUsername, findUserByEmail, findUserByLogin,
    createUser, setEmailVerified, updatePassword,
    // tokens
    createToken, consumeToken, purgeOldTokens,
    // friends
    listIncomingRequests, listOutgoingRequests,
    hasOutgoingRequest, hasIncomingRequest,
    addFriendRequest, removeFriendRequest, removeAnyRequestBetween,
    areFriends, addFriendship, removeFriendship, listFriends,
    // servers
    createServer, getServerById, getServerByInvite,
    listUserServers, isServerMember, getServerMemberRole,
    joinServerByInvite, leaveServer, deleteServer, renameServer,
    listServerMembers,
    // channels
    addChannel, listChannels, getChannelById, deleteChannel, renameChannel,
    // messages
    addMessage, listMessages, deleteMessage, editMessage,
    // DMs
    sendDirectMessage, listDirectMessages, listDMThreads, markDMRead,
    getDirectMessage, deleteDirectMessage, editDirectMessage,
    // Cihaz fingerprint
    findKnownDevice, registerKnownDevice, touchKnownDevice,
    hasRecentDeviceInSubnet, getLastNewDeviceAlertAt, touchLastNewDeviceAlert,
    // 2FA güvenilir cihaz tokenları
    addTrustedDevice2fa, findTrustedDevice2fa, touchTrustedDevice2fa,
    revokeTrustedDevice2fa, purgeExpiredTrustedDevices2fa,
    // Mesaj erişim onay sistemi
    createMessageAccessGrant, findMessageAccessGrant,
    decideMessageAccessGrant, consumeMessageAccessGrant,
    listPendingMessageAccessGrants, purgeExpiredMessageAccessGrants,
    // Parolasız giriş kodları
    createPasswordlessCode, consumePasswordlessCode, expirePasswordlessCodes,
    // E2E DM public key
    setUserPublicKey, getUserPublicKey, getUserPublicKeyByName,
};

// =================== E2E Public Keys =====================
async function setUserPublicKey(userId, pubKeyB64) {
    await pool.query('UPDATE users SET public_key = $1 WHERE id = $2',
        [pubKeyB64 || null, userId]);
}
async function getUserPublicKey(userId) {
    const r = await pool.query('SELECT public_key FROM users WHERE id = $1', [userId]);
    return (r.rows[0] && r.rows[0].public_key) || null;
}
async function getUserPublicKeyByName(username) {
    const r = await pool.query(
        'SELECT id, username, public_key FROM users WHERE username_lower = $1',
        [norm(username)]);
    return r.rows[0] || null;
}

// =================== Cihaz Fingerprint =====================
async function findKnownDevice(userId, fingerprint) {
    const r = await pool.query(
        'SELECT * FROM known_devices WHERE user_id = $1 AND fingerprint = $2 LIMIT 1',
        [userId, fingerprint]);
    return r.rows[0] || null;
}
async function registerKnownDevice(userId, fingerprint, ip, ua) {
    const r = await pool.query(
        `INSERT INTO known_devices (user_id, fingerprint, ip, user_agent)
         VALUES ($1, $2, $3, $4)
         ON CONFLICT (user_id, fingerprint) DO UPDATE
            SET last_seen = NOW(), ip = EXCLUDED.ip, user_agent = EXCLUDED.user_agent
         RETURNING *`,
        [userId, fingerprint, ip || null, ua || null]);
    return r.rows[0];
}
async function touchKnownDevice(userId, fingerprint) {
    await pool.query(
        'UPDATE known_devices SET last_seen = NOW() WHERE user_id = $1 AND fingerprint = $2',
        [userId, fingerprint]);
}
// Aynı /24 alt-ağdan son 30 gün içinde herhangi bir bilinen cihaz var mı?
async function hasRecentDeviceInSubnet(userId, ipPrefix) {
    const r = await pool.query(
        `SELECT 1 FROM known_devices
          WHERE user_id = $1 AND ip LIKE $2
            AND last_seen > NOW() - INTERVAL '30 days' LIMIT 1`,
        [userId, ipPrefix + '%']);
    return r.rowCount > 0;
}
// Yeni cihaz uyarısı throttling
async function getLastNewDeviceAlertAt(userId) {
    const r = await pool.query(
        'SELECT last_new_device_alert_at AS at FROM users WHERE id = $1', [userId]);
    return r.rows[0] && r.rows[0].at ? new Date(r.rows[0].at).getTime() : 0;
}
async function touchLastNewDeviceAlert(userId) {
    await pool.query(
        'UPDATE users SET last_new_device_alert_at = NOW() WHERE id = $1', [userId]);
}

// =================== 2FA Trusted Devices (30 gün) ===================
async function addTrustedDevice2fa(userId, tokenHash, fingerprint, ttlDays = 30) {
    const expiresAt = new Date(Date.now() + ttlDays * 24 * 60 * 60 * 1000);
    await pool.query(
        `INSERT INTO trusted_devices_2fa (user_id, token_hash, fingerprint, expires_at)
         VALUES ($1, $2, $3, $4)
         ON CONFLICT (token_hash) DO UPDATE SET expires_at = EXCLUDED.expires_at,
                                                 last_used_at = NOW()`,
        [userId, tokenHash, fingerprint || null, expiresAt]);
}
async function findTrustedDevice2fa(userId, tokenHash) {
    const r = await pool.query(
        `SELECT * FROM trusted_devices_2fa
          WHERE user_id = $1 AND token_hash = $2 AND expires_at > NOW()
          LIMIT 1`,
        [userId, tokenHash]);
    return r.rows[0] || null;
}
async function touchTrustedDevice2fa(tokenHash) {
    await pool.query(
        'UPDATE trusted_devices_2fa SET last_used_at = NOW() WHERE token_hash = $1',
        [tokenHash]);
}
async function revokeTrustedDevice2fa(tokenHash) {
    await pool.query('DELETE FROM trusted_devices_2fa WHERE token_hash = $1', [tokenHash]);
}
async function purgeExpiredTrustedDevices2fa() {
    await pool.query('DELETE FROM trusted_devices_2fa WHERE expires_at < NOW()');
}

// =================== Mesaj Erişim Onay Sistemi ===================
// Admin bir kullanıcının mesajlarına erişim talep eder. Kullanıcı e-postadan
// 'Onayla'yı tıklayana kadar admin .txt indiremez.
async function createMessageAccessGrant({ userId, token, requestedBy, ttlHours = 24 }) {
    const expiresAt = new Date(Date.now() + ttlHours * 60 * 60 * 1000);
    const r = await pool.query(
        `INSERT INTO message_access_grants (user_id, token, requested_by, expires_at)
         VALUES ($1, $2, $3, $4) RETURNING *`,
        [userId, token, requestedBy || null, expiresAt]);
    return r.rows[0];
}
async function findMessageAccessGrant(token) {
    const r = await pool.query(
        `SELECT g.*, u.username, u.email
           FROM message_access_grants g
           JOIN users u ON u.id = g.user_id
          WHERE g.token = $1 LIMIT 1`, [token]);
    return r.rows[0] || null;
}
async function decideMessageAccessGrant(token, decision /* 'approved'|'denied' */) {
    const r = await pool.query(
        `UPDATE message_access_grants
            SET status = $2, decided_at = NOW()
          WHERE token = $1 AND status = 'pending'
            AND expires_at > NOW()
          RETURNING *`,
        [token, decision]);
    return r.rows[0] || null;
}
// Tek seferlik kullan: status approved → used'a çevir, mesajları döndürmek için
async function consumeMessageAccessGrant(token) {
    const r = await pool.query(
        `UPDATE message_access_grants
            SET status = 'used'
          WHERE token = $1 AND status = 'approved'
            AND expires_at > NOW()
          RETURNING *`,
        [token]);
    return r.rows[0] || null;
}
async function listPendingMessageAccessGrants() {
    const r = await pool.query(
        `SELECT g.id, g.token, g.requested_by, g.created_at, g.expires_at,
                g.status, u.username, u.email
           FROM message_access_grants g
           JOIN users u ON u.id = g.user_id
          WHERE g.expires_at > NOW()
            AND g.status IN ('pending','approved','denied')
          ORDER BY g.created_at DESC
          LIMIT 50`);
    return r.rows;
}
async function purgeExpiredMessageAccessGrants() {
    await pool.query(
        `UPDATE message_access_grants SET status = 'expired'
          WHERE expires_at < NOW() AND status = 'pending'`);
    await pool.query(
        `DELETE FROM message_access_grants WHERE expires_at < NOW() - INTERVAL '7 days'`);
}

// =================== Parolasız Giriş Kodları ===================
async function createPasswordlessCode(userId, channel, codeHash, ttlSeconds) {
    const expiresAt = new Date(Date.now() + ttlSeconds * 1000);
    // Aynı kullanıcı/kanal için bekleyen eski kodları geçersiz kıl
    await pool.query(
        `UPDATE passwordless_login_codes SET used = TRUE
         WHERE user_id = $1 AND channel = $2 AND used = FALSE`,
        [userId, channel]);
    const r = await pool.query(
        `INSERT INTO passwordless_login_codes (user_id, channel, code_hash, expires_at)
         VALUES ($1, $2, $3, $4) RETURNING id`,
        [userId, channel, codeHash, expiresAt]);
    return r.rows[0].id;
}
async function consumePasswordlessCode(userId, channel, codeHash) {
    const r = await pool.query(
        `SELECT * FROM passwordless_login_codes
         WHERE user_id = $1 AND channel = $2 AND used = FALSE
         ORDER BY created_at DESC LIMIT 1`,
        [userId, channel]);
    const row = r.rows[0];
    if (!row) return { ok: false, reason: 'not_found' };
    if (new Date(row.expires_at).getTime() < Date.now())
        return { ok: false, reason: 'expired' };
    if (row.attempts >= 5) {
        await pool.query('UPDATE passwordless_login_codes SET used = TRUE WHERE id = $1', [row.id]);
        return { ok: false, reason: 'too_many_attempts' };
    }
    if (row.code_hash !== codeHash) {
        await pool.query(
            'UPDATE passwordless_login_codes SET attempts = attempts + 1 WHERE id = $1',
            [row.id]);
        return { ok: false, reason: 'wrong_code' };
    }
    await pool.query('UPDATE passwordless_login_codes SET used = TRUE WHERE id = $1', [row.id]);
    return { ok: true };
}
async function expirePasswordlessCodes() {
    await pool.query(
        `DELETE FROM passwordless_login_codes WHERE expires_at < NOW() - INTERVAL '1 day'`);
}
