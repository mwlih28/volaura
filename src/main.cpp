#include <QApplication>
#include <QIcon>
#include <QPixmap>
#include <QTimer>
#include "mainwindow.h"
#include <QSettings>
#include "crypto/e2e_crypto.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("VoLaura");
    // libsodium başlat — DM uçtan uca şifreleme için
    E2E::initialize();
    app.setApplicationVersion("1.1.0");
    app.setOrganizationName("VoLaura");
    app.setWindowIcon(QIcon(":/icons/volaura-logo.png"));

    QSettings settings("VoLaura", "VoLaura");
    bool installed = settings.value("installed", false).toBool();
    if (!installed) {
        settings.setValue("installed", true);
        settings.sync();
    }

    // Splash screen — logo + pulse animasyonu
    QPixmap logo(":/icons/volaura-logo.png");
    auto *splash = new VoLauraSplash(logo);
    splash->show();
    app.processEvents();

    // Ana pencereyi hazırla ama henüz gösterme
    auto *window = new MainWindow();
    window->setAttribute(Qt::WA_DeleteOnClose);

    // 1.6 saniye sonra splash'i fade-out ile kapat ve ana pencereyi göster
    QTimer::singleShot(1600, splash, [splash, window]() {
        splash->finish(window);
    });

    return app.exec();
}
