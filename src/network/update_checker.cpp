// =====================================================================
//  UpdateChecker implementation
// =====================================================================

#include "update_checker.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTimer>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QProcess>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QDebug>

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent),
      net_(new QNetworkAccessManager(this)),
      poll_(new QTimer(this)),
      checkUrl_(QStringLiteral("https://update.volaura.xyz/api/version")) {
    poll_->setSingleShot(false);
    connect(poll_, &QTimer::timeout, this, &UpdateChecker::checkNow);

    // Bildirim/duyuru polling — sürüm kontrolünden bağımsız ve sık
    notifPoll_ = new QTimer(this);
    notifPoll_->setSingleShot(false);
    notifPoll_->setInterval(60 * 1000); // dakikada bir
    connect(notifPoll_, &QTimer::timeout, this, &UpdateChecker::fetchNotificationsNow);
    notifPoll_->start();
    // İlk fetch 4 saniye sonra
    QTimer::singleShot(4000, this, &UpdateChecker::fetchNotificationsNow);
}

void UpdateChecker::setPollIntervalSec(int s) {
    if (s <= 0) { poll_->stop(); return; }
    poll_->setInterval(s * 1000);
    if (!poll_->isActive()) poll_->start();
}

// ---------- Notifications poll ----------
void UpdateChecker::fetchNotificationsNow() {
    QNetworkRequest req(notifUrl_);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QString("VoLaura/%1").arg(currentVersion_));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *r = net_->get(req);
    connect(r, &QNetworkReply::finished, this, [this, r]() { onNotificationsFinished(r); });
}

void UpdateChecker::onNotificationsFinished(QNetworkReply *r) {
    r->deleteLater();
    if (r->error() != QNetworkReply::NoError) {
        // Sessizce yut — sürekli loglamayı önler
        return;
    }
    const QByteArray body = r->readAll();
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(body, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isArray()) return;

    const QJsonArray arr = doc.array();
    for (const auto &v : arr) {
        const QJsonObject o = v.toObject();
        const QString id    = o.value("id").toString();
        const QString type  = o.value("type").toString();
        const QString title = o.value("title").toString();
        const QString body  = o.value("body").toString();
        if (id.isEmpty() || title.isEmpty()) continue;
        if (seenNotifIds_.contains(id)) continue;  // zaten gösterildi
        seenNotifIds_.insert(id);
        emit notificationReceived(id, type, title, body);
    }
}

// ---------- Version comparison ----------
int UpdateChecker::compareVersions(const QString &a, const QString &b) {
    static const QRegularExpression splitter("[.\\-+]");
    const QStringList sa = a.split(splitter, Qt::SkipEmptyParts);
    const QStringList sb = b.split(splitter, Qt::SkipEmptyParts);
    const int n = qMax(sa.size(), sb.size());
    for (int i = 0; i < n; ++i) {
        const int va = (i < sa.size()) ? sa[i].toInt() : 0;
        const int vb = (i < sb.size()) ? sb[i].toInt() : 0;
        if (va != vb) return (va < vb) ? -1 : 1;
    }
    return 0;
}

// ---------- Check ----------
void UpdateChecker::checkNow() {
    QNetworkRequest req(checkUrl_);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QString("VoLaura/%1").arg(currentVersion_));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *r = net_->get(req);
    connect(r, &QNetworkReply::finished, this, [this, r]() { onCheckFinished(r); });
}

void UpdateChecker::onCheckFinished(QNetworkReply *r) {
    r->deleteLater();
    if (r->error() != QNetworkReply::NoError) {
        emit checkFailed(r->errorString());
        return;
    }
    const QByteArray body = r->readAll();
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(body, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        emit checkFailed(QStringLiteral("Geçersiz sunucu yanıtı"));
        return;
    }
    const QJsonObject o = doc.object();
    const QString latest = o.value("latest").toString();
    const QString notes  = o.value("notes").toString();
    const QString url    = o.value("url").toString();
    const QString sha    = o.value("sha256").toString();
    const qint64  size   = qint64(o.value("size").toDouble(0));

    if (latest.isEmpty() || url.isEmpty()) {
        emit checkFailed(QStringLiteral("Eksik manifest alanları"));
        return;
    }
    if (compareVersions(latest, currentVersion_) > 0) {
        emit updateAvailable(latest, notes, QUrl(url), sha, size);
    } else {
        emit noUpdateAvailable();
    }
}

// ---------- Download ----------
void UpdateChecker::downloadAndInstall(const QUrl &url, const QString &sha) {
    if (activeDownload_) { activeDownload_->abort(); activeDownload_ = nullptr; }
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir().mkpath(dir);
    const QString fileName = url.fileName().isEmpty() ? QStringLiteral("VoLaura-Setup.exe")
                                                      : url.fileName();
    const QString path = dir + "/" + fileName;
    QFile::remove(path);

    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    activeDownload_ = net_->get(req);
    emit downloadStarted();
    connect(activeDownload_, &QNetworkReply::downloadProgress,
            this, &UpdateChecker::downloadProgress);
    QNetworkReply *r = activeDownload_;
    connect(r, &QNetworkReply::finished, this,
            [this, r, path, sha]() { onDownloadFinished(r, path, sha); });
}

void UpdateChecker::cancelDownload() {
    if (activeDownload_) {
        activeDownload_->abort();
        activeDownload_ = nullptr;
    }
}

void UpdateChecker::onDownloadFinished(QNetworkReply *r,
                                        const QString &path,
                                        const QString &expectedSha) {
    activeDownload_ = nullptr;
    r->deleteLater();
    if (r->error() != QNetworkReply::NoError) {
        emit downloadFailed(r->errorString());
        return;
    }
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        emit downloadFailed(QStringLiteral("Dosya yazılamadı: %1").arg(path));
        return;
    }
    f.write(r->readAll());
    f.close();

    if (!expectedSha.isEmpty()) {
        const QString got = sha256Hex(path);
        if (got.compare(expectedSha, Qt::CaseInsensitive) != 0) {
            QFile::remove(path);
            emit downloadFailed(QStringLiteral("Bütünlük doğrulaması başarısız (SHA256)"));
            return;
        }
    }
    emit downloadFinished(path);
    launchInstaller(path);
}

QString UpdateChecker::sha256Hex(const QString &filePath) {
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash h(QCryptographicHash::Sha256);
    if (!h.addData(&f)) return {};
    return QString::fromLatin1(h.result().toHex());
}

void UpdateChecker::launchInstaller(const QString &path) {
    // Inno Setup: /SILENT göstermez ama /VERYSILENT gösterir.
    // Kullanıcı dosya doğrulanmış güncelleme yaptığı için Wizard'ı
    // göstermek gereksiz — /VERYSILENT + /CLOSEAPPLICATIONS ile geçiyoruz.
    QStringList args = {
        "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART",
        "/CLOSEAPPLICATIONS", "/RESTARTAPPLICATIONS"
    };
    QProcess::startDetached(path, args);
    // Mevcut uygulama kapatılmalı ki installer overwrite edebilsin
    QTimer::singleShot(800, qApp, &QCoreApplication::quit);
}
