// VoLaura e-posta gönderim sarmalayıcısı + HTML şablonları
// Sağlayıcı öncelik sırası:
//   1) RESEND_API_KEY varsa → Resend (3000/ay free)
//   2) SENDGRID_API_KEY varsa → SendGrid (fallback)
let sg = null;
let resend = null;
let provider = null;  // 'resend' | 'sendgrid' | null

function init() {
    if (provider) return;
    if (process.env.RESEND_API_KEY) {
        try {
            const { Resend } = require('resend');
            resend = new Resend(process.env.RESEND_API_KEY);
            provider = 'resend';
            console.log('[email] Resend aktif.');
            return;
        } catch (e) {
            console.error('[email] Resend yüklenemedi:', e.message);
        }
    }
    if (process.env.SENDGRID_API_KEY) {
        try {
            sg = require('@sendgrid/mail');
            sg.setApiKey(process.env.SENDGRID_API_KEY);
            provider = 'sendgrid';
            console.log('[email] SendGrid aktif.');
            return;
        } catch (e) {
            console.error('[email] SendGrid yüklenemedi:', e.message);
        }
    }
    console.warn('[email] Hiçbir e-posta sağlayıcı API anahtarı bulunamadı; e-posta devre dışı.');
}

function escapeHtml(s) {
    return String(s || '')
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#39;');
}

function getFromAddress() {
    const email = process.env.MAIL_FROM
               || process.env.RESEND_FROM
               || process.env.SENDGRID_FROM
               || 'onboarding@resend.dev';
    const name = process.env.MAIL_FROM_NAME
              || process.env.SENDGRID_FROM_NAME
              || 'VoLaura';
    return { email, name };
}

async function sendMail({ to, subject, html, text }) {
    init();
    if (!provider) throw new Error('E-posta sağlayıcı yapılandırılmamış');
    const from = getFromAddress();
    console.log(`[email] gönderiliyor → to=${to} from=${from.email} provider=${provider}`);
    if (provider === 'resend') {
        const r = await resend.emails.send({
            from: `${from.name} <${from.email}>`,
            to: [to],
            subject, html, text,
        });
        if (r && r.error) {
            const msg = r.error.message || JSON.stringify(r.error);
            console.error('[email][resend] hata:', msg);
            throw new Error(msg);
        }
        console.log(`[email] OK (resend) id=${r && r.data && r.data.id}`);
        return;
    }
    // SendGrid
    try {
        const [resp] = await sg.send({ to, from, subject, html, text });
        console.log(`[email] OK (sendgrid) status=${resp && resp.statusCode}`);
    } catch (e) {
        // SendGrid hata gövdesini detaylıca logla — kök sebep buradan görünür
        const status = e && e.code;
        const body = e && e.response && e.response.body;
        console.error('[email][sendgrid] hata:', e && e.message,
            'status=', status,
            'body=', body ? JSON.stringify(body).slice(0, 500) : '(yok)');
        throw e;
    }
}

// E-POSTA şablonu — açık tema, hafif marka aksanlı (mor/mavi gradient çubuk).
// Sade ama VoLaura'ya özgü: logo + wordmark + renkli üst çizgi.
function emailShell(title, bodyInner) {
    const safeTitle = escapeHtml(title);
    return `<!doctype html>
<html lang="tr"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>${safeTitle}</title></head>
<body style="margin:0;padding:0;background:#f6f7fb;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Helvetica,Arial,sans-serif;color:#1a1a1a;">
<div style="display:none;max-height:0;overflow:hidden;mso-hide:all;">${safeTitle} - VoLaura</div>
<table role="presentation" width="100%" cellpadding="0" cellspacing="0" border="0" style="background:#f6f7fb;padding:40px 16px;">
<tr><td align="center">
  <table role="presentation" width="560" cellpadding="0" cellspacing="0" border="0"
         style="max-width:560px;background:#ffffff;border:1px solid #e7e9ee;border-radius:14px;overflow:hidden;">
    <tr><td style="height:4px;background:linear-gradient(90deg,#5b6cff 0%,#a259ff 100%);font-size:0;line-height:0;">&nbsp;</td></tr>
    <tr><td style="padding:28px 32px 4px 32px;">
      <table role="presentation" cellpadding="0" cellspacing="0" border="0"><tr>
        <td style="vertical-align:middle;padding-right:12px;">
          <img src="https://volaura.xyz/logo.png" alt="VoLaura"
               width="36" height="36"
               style="display:block;width:36px;height:36px;border:0;outline:none;text-decoration:none;border-radius:8px;">
        </td>
        <td style="vertical-align:middle;color:#1a1a1a;font-weight:700;letter-spacing:0.4px;font-size:17px;">
          VoLaura
        </td>
      </tr></table>
    </td></tr>
    <tr><td style="padding:18px 32px 28px 32px;color:#1a1a1a;font-size:15px;line-height:1.6;">
      ${bodyInner}
    </td></tr>
    <tr><td style="padding:14px 32px 22px 32px;border-top:1px solid #eef0f4;color:#8b93a7;font-size:12px;line-height:1.55;">
      <a href="https://volaura.xyz" style="color:#5b6cff;text-decoration:none;">volaura.xyz</a>
      &nbsp;·&nbsp;
      <a href="mailto:support@volaura.xyz" style="color:#5b6cff;text-decoration:none;">destek</a>
      <div style="margin-top:6px;">© ${new Date().getFullYear()} VoLaura</div>
    </td></tr>
  </table>
</td></tr></table></body></html>`;
}

// WEB sayfaları için (terms/privacy/verify/reset). E-posta şablonundan AYRI: 'Bu e-posta size...' metni YOK.
function shellHtml(title, bodyInner) {
    const safeTitle = escapeHtml(title);
    return `<!doctype html>
<html lang="tr"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>${safeTitle} · VoLaura</title>
<link rel="icon" type="image/png" href="/logo.png">
<link rel="apple-touch-icon" href="/logo.png">
<meta name="theme-color" content="#2b6df5">
<meta property="og:title" content="${safeTitle} · VoLaura">
<meta property="og:description" content="VoLaura — sesin, görüntün, tek yerde.">
<meta property="og:image" content="https://volaura.xyz/logo.png">
<meta property="og:url" content="https://volaura.xyz">
<meta property="og:type" content="website">
<style>
  *{box-sizing:border-box}
  body{margin:0;background:#070a11;font-family:'Segoe UI',Roboto,Arial,sans-serif;color:#e9eefc;
       background-image:radial-gradient(1200px 600px at 10% -10%, rgba(43,109,245,0.15), transparent),
                        radial-gradient(900px 500px at 110% 10%, rgba(123,63,228,0.12), transparent);
       background-attachment:fixed;}
  a{color:#7fb3ff}
  .wrap{max-width:880px;margin:0 auto;padding:32px 20px 60px 20px}
  .nav{display:flex;align-items:center;justify-content:space-between;padding:8px 0 22px 0;
       border-bottom:1px solid #1c2335;margin-bottom:24px}
  .brand{display:inline-flex;align-items:center;gap:10px;text-decoration:none;color:#fff;font-weight:800;letter-spacing:1.2px}
  .brand .logo-img{display:inline-block;width:38px;height:38px;object-fit:contain;
                   filter:drop-shadow(0 4px 12px rgba(123,63,228,0.45));
                   vertical-align:middle;margin-right:4px}
  .nav-links a{margin-left:18px;color:#9fb2dc;text-decoration:none;font-size:13px}
  .nav-links a:hover{color:#fff}
  .card{background:linear-gradient(180deg, rgba(17,20,28,0.92), rgba(13,16,24,0.92));
        border:1px solid #1f2636;border-radius:18px;padding:34px 36px;
        box-shadow:0 14px 40px rgba(0,0,0,0.45)}
  .card h2{margin-top:0;color:#f4f7ff;font-size:24px}
  .card h3{color:#cdd7ef;margin-top:22px;font-size:16px}
  .card p, .card li{color:#cdd7ef;line-height:1.65;font-size:14.2px}
  .card ul{padding-left:18px}
  .meta{color:#8a96b3;font-size:12px;margin-top:-6px;margin-bottom:14px}
  footer{margin-top:34px;color:#7d8ba7;font-size:12px;text-align:center}
  footer a{color:#9fb2dc;text-decoration:none;margin:0 6px}
  hr{border:none;border-top:1px solid #1f2636;margin:22px 0}
</style>
</head>
<body>
<div class="wrap">
  <nav class="nav">
    <a class="brand" href="/"><img src="/logo.png" alt="VoLaura" class="logo-img"> VoLaura</a>
    <div class="nav-links">
      <a href="/terms">Şartlar</a>
      <a href="/privacy">Gizlilik</a>
      <a href="mailto:support@volaura.xyz">İletişim</a>
    </div>
  </nav>
  <div class="card">${bodyInner}</div>
  <footer>© ${new Date().getFullYear()} VoLaura — <a href="/terms">Hizmet Şartları</a> · <a href="/privacy">Gizlilik Politikası</a></footer>
</div>
</body></html>`;
}

function bigButton(href, label) {
    return `<table role="presentation" cellpadding="0" cellspacing="0" border="0" style="margin:18px 0;">
      <tr><td style="border-radius:8px;background:linear-gradient(135deg,#5b6cff 0%,#a259ff 100%);">
        <a href="${href}" target="_blank" rel="noopener"
           style="display:inline-block;padding:11px 22px;color:#ffffff;text-decoration:none;
                  font-weight:600;font-size:14px;border-radius:8px;">${label}</a>
      </td></tr></table>`;
}
// İkinci derece (outline) buton — sade, marka renkli kenarlık.
function ghostButton(href, label) {
    return `<table role="presentation" cellpadding="0" cellspacing="0" border="0" style="margin:6px 0 18px 0;">
      <tr><td style="border-radius:8px;border:1px solid #d6dae6;background:#ffffff;">
        <a href="${href}" target="_blank" rel="noopener"
           style="display:inline-block;padding:10px 20px;color:#1a1a1a;text-decoration:none;
                  font-weight:600;font-size:14px;border-radius:8px;">${label}</a>
      </td></tr></table>`;
}

function verificationEmail({ userName, link }) {
    const safeName = escapeHtml(userName);
    const inner = `
        <h1 style="margin:0 0 12px 0;font-size:22px;font-weight:700;color:#1a1a1a;">Hesabını doğrula</h1>
        <p style="margin:0 0 8px 0;color:#374151;">Merhaba ${safeName},</p>
        <p style="margin:0 0 16px 0;color:#374151;">VoLaura'ya kaydolduğun için teşekkürler. Hesabını etkinleştirmek için aşağıdaki butona tıkla.</p>
        ${bigButton(link, 'Hesabımı Doğrula')}
        <p style="margin:18px 0 6px 0;color:#6b7280;font-size:13px;">Buton çalışmıyorsa bu bağlantıyı tarayıcına yapıştır:</p>
        <p style="margin:0 0 18px 0;"><a href="${escapeHtml(link)}" style="color:#1a1a1a;word-break:break-all;">${escapeHtml(link)}</a></p>
        <p style="margin:0;color:#6b7280;font-size:13px;">Bağlantı 24 saat geçerlidir. Sen kaydolmadıysan bu e-postayı görmezden gelebilirsin.</p>`;
    return {
        subject: '✓ VoLaura hesabını doğrula',
        text: `Merhaba ${userName},\n\nVoLaura hesabını doğrulamak için şu bağlantıya tıkla:\n${link}\n\nBağlantı 24 saat geçerlidir.`,
        html: emailShell('Hesabını Doğrula', inner),
    };
}

function passwordResetEmail({ userName, link }) {
    const safeName = escapeHtml(userName);
    const inner = `
        <h1 style="margin:0 0 12px 0;font-size:22px;font-weight:700;color:#1a1a1a;">Şifreni sıfırla</h1>
        <p style="margin:0 0 8px 0;color:#374151;">Merhaba ${safeName},</p>
        <p style="margin:0 0 16px 0;color:#374151;">VoLaura hesabın için bir şifre sıfırlama isteği aldık. Yeni bir şifre belirlemek için aşağıdaki butona tıkla.</p>
        ${bigButton(link, 'Yeni Şifre Belirle')}
        <p style="margin:18px 0 6px 0;color:#6b7280;font-size:13px;">Buton çalışmıyorsa bu bağlantıyı tarayıcına yapıştır:</p>
        <p style="margin:0 0 18px 0;"><a href="${escapeHtml(link)}" style="color:#1a1a1a;word-break:break-all;">${escapeHtml(link)}</a></p>
        <p style="margin:0;color:#6b7280;font-size:13px;">Bağlantı 1 saat geçerlidir. Sen istemediysen bu e-postayı yok say.</p>`;
    return {
        subject: '🔒 VoLaura şifre sıfırlama',
        text: `Merhaba ${userName},\n\nŞifreni sıfırlamak için: ${link}\n\nBağlantı 1 saat geçerlidir. Sen istemediysen bu mesajı yok say.`,
        html: emailShell('Şifre Sıfırlama', inner),
    };
}

function loginCodeEmail({ userName, code }) {
    const safeName = escapeHtml(userName);
    const safeCode = escapeHtml(code);
    const inner = `
        <h1 style="margin:0 0 12px 0;font-size:22px;font-weight:700;color:#1a1a1a;">Giriş kodun</h1>
        <p style="margin:0 0 8px 0;color:#374151;">Merhaba ${safeName},</p>
        <p style="margin:0 0 8px 0;color:#374151;">VoLaura hesabına giriş yapmak için aşağıdaki kodu kullan:</p>
        <p style="margin:24px 0 12px 0;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;font-size:32px;font-weight:700;color:#5b6cff;letter-spacing:4px;">${safeCode}</p>
        <p style="margin:0 0 18px 0;color:#6b7280;font-size:13px;">Bu kod 10 dakika geçerlidir.</p>
        <p style="margin:0;color:#6b7280;font-size:13px;">Bu girişi sen başlatmadıysan bu e-postayı yok sayabilirsin.</p>`;
    return {
        subject: `${code} — VoLaura giriş kodun`,
        text: `Merhaba ${userName},\n\nVoLaura giriş kodun: ${code}\n\nKod 10 dakika geçerlidir. Kimseyle paylaşma.`,
        html: emailShell('Giriş Kodun', inner),
    };
}

// Yeni cihazdan giriş yapıldığında gönderilen güvenlik bildirimi.
// resetLink/enable2faLink verilirse mailde tek tıkla hızlı eylem butonları gösterilir.
function newDeviceEmail({ userName, ip, userAgent, location, when, resetLink, enable2faLink }) {
    const safeName = escapeHtml(userName);
    const safeIp   = escapeHtml(ip || 'bilinmiyor');
    const safeUa   = escapeHtml(userAgent || 'bilinmiyor');
    const safeLoc  = escapeHtml(location || 'bilinmiyor');
    const safeWhen = escapeHtml(when || new Date().toLocaleString('tr-TR'));
    const actionsHtml = (resetLink || enable2faLink) ? `
        <p style="margin:18px 0 6px 0;color:#1a1a1a;font-weight:600;font-size:14.5px;">Sen değil misin?</p>
        <p style="margin:0 0 6px 0;color:#374151;font-size:14px;">Hesabını hemen güvene al:</p>
        ${resetLink     ? bigButton(resetLink,    'Şifreyi Sıfırla')          : ''}
        ${enable2faLink ? ghostButton(enable2faLink, '2 Adımlı Doğrulamayı Aç') : ''}
    ` : '';
    const inner = `
        <h1 style="margin:0 0 12px 0;font-size:22px;font-weight:700;color:#1a1a1a;">Yeni cihazdan giriş</h1>
        <p style="margin:0 0 8px 0;color:#374151;">Merhaba ${safeName},</p>
        <p style="margin:0 0 18px 0;color:#374151;">VoLaura hesabına yeni bir cihazdan giriş yapıldı. Bu sensen yok sayabilirsin.</p>
        <table role="presentation" cellpadding="0" cellspacing="0" border="0" width="100%" style="border:1px solid #e7e9ee;border-radius:8px;">
          <tr><td style="padding:14px 16px;color:#374151;font-size:13.5px;line-height:1.8;">
            <div><b style="color:#1a1a1a;">Zaman:</b> ${safeWhen}</div>
            <div><b style="color:#1a1a1a;">IP:</b> ${safeIp}</div>
            <div><b style="color:#1a1a1a;">Konum:</b> ${safeLoc}</div>
            <div><b style="color:#1a1a1a;">Cihaz:</b> ${safeUa}</div>
          </td></tr>
        </table>
        ${actionsHtml}
        <p style="margin:14px 0 0 0;color:#8b93a7;font-size:12.5px;">Linkler kişiseldir, başkalarıyla paylaşma. Şifre sıfırlama 1 saat, 2FA bağlantısı 24 saat geçerlidir.</p>`;
    return {
        subject: '🔔 VoLaura — yeni bir cihazdan giriş yapıldı',
        text: `Merhaba ${userName},\n\nHesabına yeni bir cihazdan giriş yapıldı.\nZaman: ${when}\nIP: ${ip}\nKonum: ${location}\nCihaz: ${userAgent}\n\n` +
              (resetLink ? `Şifreni sıfırla: ${resetLink}\n` : '') +
              (enable2faLink ? `2FA'yı aç: ${enable2faLink}\n` : ''),
        html: emailShell('Yeni Cihazdan Giriş', inner),
    };
}

// Admin'in mesaj erişim talebi → kullanıcıya gönderilen onay e-postası.
// Onayla / Reddet butonları aynı /approve-message-access sayfasına gider;
// sayfa kararı POST'la kaydeder.
function messageAccessRequestEmail({ userName, requestedBy, approveUrl, denyUrl, expiresIn }) {
    const safeName = escapeHtml(userName);
    const safeWho  = escapeHtml(requestedBy || 'Admin');
    const safeExp  = escapeHtml(expiresIn || '24 saat');
    const inner = `
        <h1 style="margin:0 0 12px 0;font-size:22px;font-weight:700;color:#1a1a1a;">
            Mesaj erişim onayı
        </h1>
        <p style="margin:0 0 8px 0;color:#374151;">Merhaba ${safeName},</p>
        <p style="margin:0 0 14px 0;color:#374151;">
            <b>${safeWho}</b> hesabınla ilişkili kanal mesajlarına ve doğrudan mesajlara
            erişim için sizin onayınızı talep ediyor.
        </p>
        <table role="presentation" cellpadding="0" cellspacing="0" border="0" width="100%"
               style="border:1px solid #e7e9ee;border-radius:8px;background:#f9fafb;">
          <tr><td style="padding:14px 16px;color:#374151;font-size:13.5px;line-height:1.7;">
            <div><b style="color:#1a1a1a;">Talep eden:</b> ${safeWho}</div>
            <div><b style="color:#1a1a1a;">Geçerlilik:</b> ${safeExp}</div>
            <div><b style="color:#1a1a1a;">Sadece tek seferlik kullanılabilir.</b></div>
          </td></tr>
        </table>
        <p style="margin:18px 0 6px 0;color:#1a1a1a;font-weight:600;font-size:14.5px;">
            Onaylıyor musun?
        </p>
        ${bigButton(approveUrl, 'Evet, onaylıyorum')}
        ${ghostButton(denyUrl, 'Hayır, reddet')}
        <p style="margin:14px 0 0 0;color:#8b93a7;font-size:12.5px;">
            Bu istek senden değilse <b>Reddet</b>'e tıkla; istek otomatik iptal edilir.
            Şüphelendiğin durumda hesabını koruma altına almak için şifreni değiştirebilir
            veya 2FA'yı açabilirsin.
        </p>`;
    return {
        subject: 'VoLaura — Mesajlarınıza erişim onayı talep edildi',
        text: `Merhaba ${userName},\n\n${requestedBy || 'Admin'} mesajlarınıza erişim için onay talep ediyor.\n\nOnayla: ${approveUrl}\nReddet: ${denyUrl}\n\nGeçerlilik: ${expiresIn || '24 saat'}.\nBu talep senden değilse Reddet'e tıkla.`,
        html: emailShell('Mesaj Erişim Onayı', inner),
    };
}

module.exports = { sendMail, verificationEmail, passwordResetEmail, loginCodeEmail,
                   newDeviceEmail, messageAccessRequestEmail,
                   shellHtml, emailShell, escapeHtml };
