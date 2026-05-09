#pragma once

// =====================================================================
//  UpdateChecker — VoLaura otomatik güncelleme kontrolcüsü
// ---------------------------------------------------------------------
//  • Periyodik olarak `https://update.volaura.xyz/api/version` polller
//  • Yeni sürüm bulursa `updateAvailable` sinyali verir
//  • Kullanıcı onayı ile installer'ı %TEMP%'e indirir, SHA256 doğrular
//    ve çalıştırır (mevcut uygulama kapanır, kurulum tamamlanır,
//    yeni sürüm otomatik açılır).
//
//  Server tarafı için: server/admin (Node.js Express) bakın.
// =====================================================================

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QJsonArray>
#include <QSet>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

class UpdateChecker : public QObject {
    Q_OBJECT
public:
    explicit UpdateChecker(QObject *parent = nullptr);

    // Konfigurasyon
    void setCurrentVersion(const QString &v) { currentVersion_ = v; }
    void setCheckUrl(const QUrl &u)          { checkUrl_ = u; }
    void setPollIntervalSec(int s);            // 0 = sadece manuel

    // Aksiyonlar
    void checkNow();
    void fetchNotificationsNow();
    void downloadAndInstall(const QUrl &url, const QString &sha256Hex);
    void cancelDownload();

    // Duyuru endpoint'i (default: https://update.volaura.xyz/api/notifications)
    void setNotificationsUrl(const QUrl &u) { notifUrl_ = u; }

    // Versiyon karşılaştırma helper (1.2.0 vs 1.10.0 doğru)
    static int compareVersions(const QString &a, const QString &b);

signals:
    void updateAvailable(const QString &version, const QString &notes,
                         const QUrl &downloadUrl, const QString &sha256Hex,
                         qint64 sizeBytes);
    void noUpdateAvailable();
    void checkFailed(const QString &error);

    void downloadStarted();
    void downloadProgress(qint64 received, qint64 total);
    void downloadFinished(const QString &localPath);
    void downloadFailed(const QString &error);

    // Yeni duyuru geldi (admin paneli üzerinden push edilen).
    // İstemci, bunu NotificationCenter'a basar; aynı id ikinci defa
    // gelirse merkez kendi içinde dedupe eder.
    void notificationReceived(const QString &id,
                              const QString &type,
                              const QString &title,
                              const QString &body);

private:
    void onCheckFinished(QNetworkReply *r);
    void onNotificationsFinished(QNetworkReply *r);
    void onDownloadFinished(QNetworkReply *r, const QString &path,
                            const QString &expectedSha256);
    static QString sha256Hex(const QString &filePath);
    void launchInstaller(const QString &path);

    QNetworkAccessManager *net_;
    QTimer                *poll_;
    QTimer                *notifPoll_ = nullptr;
    QString                currentVersion_;
    QUrl                   checkUrl_;
    QUrl                   notifUrl_{QStringLiteral("https://update.volaura.xyz/api/notifications")};
    QSet<QString>          seenNotifIds_;  // dedupe across polls
    QNetworkReply         *activeDownload_ = nullptr;
};
