# VoLaura Admin & Update Server

Tek-dosya Node.js sunucu — `update.volaura.xyz` üzerinde çalışır:

- **`/api/version`** → istemci uygulaması her açılışta + 6 saatte bir çağırır
- **`/admin`** → web tabanlı admin panel (token ile giriş)
- **`/downloads/*`** → installer dosyaları
- **`/api/admin/*`** → sürüm yükleme, bildirim yayını, kullanıcı/mesaj listeleme

## Hızlı kurulum (sıfırdan)

```bash
# 1) Repo'yu sunucuya kopyala
scp -r server/admin user@volaura.xyz:/opt/volaura-admin

# 2) Sunucuda
cd /opt/volaura-admin
cp .env.example .env
# .env'i düzenle — özellikle ADMIN_TOKEN'ı uzun rastgele dize yap:
#   openssl rand -hex 32
nano .env

# 3) Bağımlılıkları kur
npm install --production

# 4) Test çalıştır
npm start    # → http://localhost:3000

# 5) systemd servisi olarak kur (önerilen)
sudo cp deploy/volaura-admin.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now volaura-admin
```

## DNS + reverse proxy

`update.volaura.xyz` için A kaydı sunucu IP'sine işaret etmeli.

### Caddy (önerilen — otomatik HTTPS)

`/etc/caddy/Caddyfile`:

```caddyfile
update.volaura.xyz {
    reverse_proxy localhost:3000
    encode gzip
}
```

Sonra: `sudo systemctl reload caddy`. HTTPS sertifikası otomatik alınır.

### Nginx alternatifi

```nginx
server {
    listen 443 ssl http2;
    server_name update.volaura.xyz;
    ssl_certificate     /etc/letsencrypt/live/update.volaura.xyz/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/update.volaura.xyz/privkey.pem;

    client_max_body_size 600M;   # Setup yüklemeleri için

    location / {
        proxy_pass http://127.0.0.1:3000;
        proxy_set_header Host $host;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }
}
```

## Kullanım

### 1. Admin paneline giriş
- Tarayıcıdan `https://update.volaura.xyz/admin`
- `.env` içindeki `ADMIN_TOKEN` ile giriş yap.

### 2. Yeni sürüm yayınlamak
1. Geliştirici makinesinde:
   - `mainwindow.cpp` ve `installer/installer.iss` içinde versiyonu artır (`1.1.0 → 1.2.0`).
   - `cmake --build build --target VoLaura -j 8`
   - `iscc installer/installer.iss` → `dist/VoLaura-Setup-1.2.0.exe`
2. Admin panelde **📦 Sürümler** sekmesi → "Yeni Sürüm Yükle":
   - Versiyon: `1.2.0`
   - Notlar: değişiklik listesi
   - Dosya: `VoLaura-Setup-1.2.0.exe`
   - "Latest olarak işaretle" ✓
3. Yükle → tüm istemciler bir sonraki yoklamada bildirim alır.

### 3. Kullanıcı tarafı (otomatik)
- İstemci `update_checker.cpp` her 6 saatte bir `/api/version` çağırır.
- Yeni sürüm varsa **çan ikonunda kırmızı badge** belirir.
- Kullanıcı bildirime tıklar → "Şimdi güncelle" butonuna basar:
  - Setup `%TEMP%`'e indirilir
  - SHA256 doğrulanır
  - Inno Setup `/VERYSILENT` ile çalıştırılır
  - Mevcut uygulama otomatik kapanır, yeni sürüm açılır.

## Veri yapısı

Tüm veri sunucuda dosya tabanlıdır:

- `releases.json` → sürüm metadata
- `releases/` → installer .exe dosyaları (yedeklenmesi öneriIir)
- `notifications.json` → broadcast bildirim listesi

## Signaling backend entegrasyonu (gelecekte)

`/api/admin/users` ve `/api/admin/messages` şu an placeholder. Ana
`wss://volaura.xyz:8444` sunucusunda admin REST API açılınca:

1. `.env`'e `SIGNALING_API=https://api.volaura.xyz` ve
   `SIGNALING_ADMIN_KEY=...` ekle.
2. `server.js` içindeki `/api/admin/users` ve `/api/admin/messages`
   handler'larındaki TODO yorumlarını gerçek `fetch()` çağrılarıyla
   değiştir.

## Güvenlik notları

- `ADMIN_TOKEN` mutlaka uzun ve rastgele olmalı (`openssl rand -hex 32`).
- Token istemci tarafında **sadece HttpOnly cookie**'de saklanır.
- SHA256 hash istemci tarafında doğrulandığı için MITM saldırılarına
  karşı dirençli (HTTPS + hash çift güvence).
- Üretim için: rate limiting (`express-rate-limit`) eklemen önerilir.
