#include "e2e_crypto.h"

#include <QSettings>
#include <QMutex>
#include <QByteArray>

#ifdef VOLAURA_HAVE_SODIUM
#  include <sodium.h>
#endif

namespace E2E {

namespace {
QMutex             g_mutex;
bool               g_initialized = false;
bool               g_available   = false;
QByteArray         g_pub;       // 32 byte
QByteArray         g_priv;      // 32 byte
QString            g_publicB64;
QString            g_currentAccount;

QString settingsKeyPub(const QString &acc)  { return QString("e2e/%1/pub").arg(acc); }
QString settingsKeyPriv(const QString &acc) { return QString("e2e/%1/priv").arg(acc); }
} // namespace

bool initialize() {
#ifdef VOLAURA_HAVE_SODIUM
    QMutexLocker lock(&g_mutex);
    if (g_initialized) return g_available;
    g_initialized = true;
    if (sodium_init() < 0) {
        g_available = false;
        return false;
    }
    g_available = true;
    return true;
#else
    g_initialized = true;
    g_available = false;
    return false;
#endif
}

bool isAvailable() {
#ifdef VOLAURA_HAVE_SODIUM
    QMutexLocker lock(&g_mutex);
    return g_initialized ? g_available : initialize();
#else
    return false;
#endif
}

bool ensureLocalKeypair(const QString &accountName, QString *outPublicKeyB64) {
#ifdef VOLAURA_HAVE_SODIUM
    if (!isAvailable()) return false;
    QMutexLocker lock(&g_mutex);
    g_currentAccount = accountName;

    QSettings s;
    const QByteArray pubB64  = s.value(settingsKeyPub(accountName)).toByteArray();
    const QByteArray privB64 = s.value(settingsKeyPriv(accountName)).toByteArray();
    if (!pubB64.isEmpty() && !privB64.isEmpty()) {
        const QByteArray pub  = QByteArray::fromBase64(pubB64);
        const QByteArray priv = QByteArray::fromBase64(privB64);
        if (pub.size()  == crypto_box_PUBLICKEYBYTES &&
            priv.size() == crypto_box_SECRETKEYBYTES) {
            g_pub = pub; g_priv = priv;
            g_publicB64 = QString::fromLatin1(pubB64);
            if (outPublicKeyB64) *outPublicKeyB64 = g_publicB64;
            return true;
        }
    }
    // Yeni keypair üret
    g_pub.resize(crypto_box_PUBLICKEYBYTES);
    g_priv.resize(crypto_box_SECRETKEYBYTES);
    crypto_box_keypair(reinterpret_cast<unsigned char*>(g_pub.data()),
                       reinterpret_cast<unsigned char*>(g_priv.data()));
    const QByteArray newPubB64  = g_pub.toBase64();
    const QByteArray newPrivB64 = g_priv.toBase64();
    s.setValue(settingsKeyPub(accountName),  newPubB64);
    s.setValue(settingsKeyPriv(accountName), newPrivB64);
    g_publicB64 = QString::fromLatin1(newPubB64);
    if (outPublicKeyB64) *outPublicKeyB64 = g_publicB64;
    return true;
#else
    Q_UNUSED(accountName); Q_UNUSED(outPublicKeyB64);
    return false;
#endif
}

QString publicKeyB64() {
    QMutexLocker lock(&g_mutex);
    return g_publicB64;
}

bool encryptForPeer(const QString &peerPubKeyB64,
                    const QString &plaintext,
                    QString *outCiphertextB64,
                    QString *outNonceB64) {
#ifdef VOLAURA_HAVE_SODIUM
    if (!isAvailable()) return false;
    if (peerPubKeyB64.isEmpty() || g_priv.size() != crypto_box_SECRETKEYBYTES) return false;
    const QByteArray peerPub = QByteArray::fromBase64(peerPubKeyB64.toLatin1());
    if (peerPub.size() != crypto_box_PUBLICKEYBYTES) return false;

    const QByteArray msg = plaintext.toUtf8();
    QByteArray nonce(crypto_box_NONCEBYTES, '\0');
    randombytes_buf(nonce.data(), nonce.size());

    QByteArray ct(msg.size() + crypto_box_MACBYTES, '\0');
    QMutexLocker lock(&g_mutex);
    if (crypto_box_easy(reinterpret_cast<unsigned char*>(ct.data()),
                        reinterpret_cast<const unsigned char*>(msg.constData()),
                        msg.size(),
                        reinterpret_cast<const unsigned char*>(nonce.constData()),
                        reinterpret_cast<const unsigned char*>(peerPub.constData()),
                        reinterpret_cast<const unsigned char*>(g_priv.constData())) != 0) {
        return false;
    }
    if (outCiphertextB64) *outCiphertextB64 = QString::fromLatin1(ct.toBase64());
    if (outNonceB64)      *outNonceB64      = QString::fromLatin1(nonce.toBase64());
    return true;
#else
    Q_UNUSED(peerPubKeyB64); Q_UNUSED(plaintext);
    Q_UNUSED(outCiphertextB64); Q_UNUSED(outNonceB64);
    return false;
#endif
}

bool decryptFromPeer(const QString &senderPubKeyB64,
                     const QString &ciphertextB64,
                     const QString &nonceB64,
                     QString *outPlaintext) {
#ifdef VOLAURA_HAVE_SODIUM
    if (!isAvailable()) return false;
    if (g_priv.size() != crypto_box_SECRETKEYBYTES) return false;
    const QByteArray senderPub = QByteArray::fromBase64(senderPubKeyB64.toLatin1());
    const QByteArray ct        = QByteArray::fromBase64(ciphertextB64.toLatin1());
    const QByteArray nonce     = QByteArray::fromBase64(nonceB64.toLatin1());
    if (senderPub.size() != crypto_box_PUBLICKEYBYTES) return false;
    if (nonce.size()     != crypto_box_NONCEBYTES) return false;
    if (ct.size() < (int)crypto_box_MACBYTES) return false;

    QByteArray pt(ct.size() - crypto_box_MACBYTES, '\0');
    QMutexLocker lock(&g_mutex);
    if (crypto_box_open_easy(reinterpret_cast<unsigned char*>(pt.data()),
                             reinterpret_cast<const unsigned char*>(ct.constData()),
                             ct.size(),
                             reinterpret_cast<const unsigned char*>(nonce.constData()),
                             reinterpret_cast<const unsigned char*>(senderPub.constData()),
                             reinterpret_cast<const unsigned char*>(g_priv.constData())) != 0) {
        return false; // Doğrulama hatası — anahtar uyuşmuyor veya bozuk veri
    }
    if (outPlaintext) *outPlaintext = QString::fromUtf8(pt);
    return true;
#else
    Q_UNUSED(senderPubKeyB64); Q_UNUSED(ciphertextB64); Q_UNUSED(nonceB64);
    Q_UNUSED(outPlaintext);
    return false;
#endif
}

void resetLocalKeypair(const QString &accountName) {
    QMutexLocker lock(&g_mutex);
    QSettings s;
    s.remove(settingsKeyPub(accountName));
    s.remove(settingsKeyPriv(accountName));
    g_pub.clear(); g_priv.clear(); g_publicB64.clear();
}

} // namespace E2E
