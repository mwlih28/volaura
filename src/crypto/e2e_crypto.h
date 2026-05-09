// E2E DM şifreleme yardımcısı — X25519 + XSalsa20-Poly1305 (libsodium crypto_box)
//
// Tasarım:
//   • Her kullanıcının cihazında kalıcı bir X25519 keypair vardır (QSettings).
//   • Public key login sonrası sunucuya 'announce_pubkey' ile yüklenir.
//   • DM göndermeden önce alıcının public key'i sunucudan alınır.
//   • Mesaj: nonce(24) + ciphertext = crypto_box_easy(plain, nonce, peer_pub, my_priv)
//   • Hem ciphertext hem nonce base64 olarak DB'ye yazılır.
//   • Server hiçbir zaman plaintext görmez.
//
// Kullanıcı yeni cihazda oturum açtığında yeni keypair oluşur ve eski cihazda
// gönderilmiş mesajlar bu cihazda okunamaz — bu kabul edilen bir uzlaşmadır.

#pragma once

#include <QString>
#include <QByteArray>

namespace E2E {

// Sodium başlatır — uygulama başında bir kez çağırılmalı.
// false dönerse libsodium yok demektir; E2E özellikleri kapalı kalır.
bool initialize();
bool isAvailable();

// Bu cihazda kayıtlı keypair varsa yükler; yoksa yeni üretip QSettings'e kaydeder.
// hesapAdi argümanı: aynı cihazdan farklı hesaplara giriliyorsa ayrı keypair.
// Başarılıysa true ve outPublicKeyB64 doldurulur.
bool ensureLocalKeypair(const QString &accountName, QString *outPublicKeyB64);

// Mevcut public key'i döndürür (ensureLocalKeypair çağrılmış olmalı).
QString publicKeyB64();

// Gönderim: plaintext'i alıcının pub key'i ile şifrele.
// Çıkış: ciphertextB64 + nonceB64. peerPubKeyB64 boş veya geçersizse false döner.
bool encryptForPeer(const QString &peerPubKeyB64,
                    const QString &plaintext,
                    QString *outCiphertextB64,
                    QString *outNonceB64);

// Alış: senderPub + nonce ile ciphertext'i çöz.
// Çözüm başarısızsa (anahtar uyumsuz, bozuk paket vb.) false döner.
bool decryptFromPeer(const QString &senderPubKeyB64,
                     const QString &ciphertextB64,
                     const QString &nonceB64,
                     QString *outPlaintext);

// Bu cihazın özel anahtarı sıfırlanır (logout veya hesap sıfırlama için).
void resetLocalKeypair(const QString &accountName);

} // namespace E2E
