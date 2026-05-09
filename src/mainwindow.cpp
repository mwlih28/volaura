#include "mainwindow.h"
#include "third_party/qrcodegen.hpp"
#include "crypto/e2e_crypto.h"

#include <cmath>
#include <QBuffer>
#include <QCamera>
#include <QCameraDevice>
#include <QDateTime>
#include <QDialog>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QScreen>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QVideoFrame>
#include <QVideoSink>
#include <QWidget>
#include <QFrame>
#include <QGraphicsBlurEffect>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QGridLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QToolButton>
#include <QAbstractButton>
#include <QPainter>
#include <QPainterPath>
#include <QEvent>
#include <QComboBox>
#include <QSettings>
#include <QSlider>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QAudioInput>
#include <QAudioOutput>
#include <QAudioDevice>
#include <QAudioSink>
#include <QAudioSource>
#include <QAudioFormat>
#include <QSystemTrayIcon>
#include <QSoundEffect>
#include <QStandardPaths>
#include <QDir>
#include <QApplication>
#include <QIODevice>
#include <QFileDialog>
#include <QFileInfo>
#include <QShortcut>
#include <QSpinBox>
#include <QMouseEvent>
#include <QStyle>
#include <QStackedWidget>
#include <QAction>
#include <QSizeGrip>
#include <QMenu>
#include <QWidgetAction>
#include <QClipboard>
#include <QApplication>
#include <QInputDialog>
#include <QPropertyAnimation>
#include <QAbstractAnimation>
#include <QPointer>
#include <QtConcurrent>
#include <QMessageBox>
#include <QKeyEvent>
#include <QEnterEvent>

// ============================================================================
//  AnimatedCheckBox — tik çizimi scale+opacity animasyonlu
// ============================================================================
AnimatedCheckBox::AnimatedCheckBox(QWidget *parent) : QAbstractButton(parent) {
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFixedSize(22, 22);
    setAttribute(Qt::WA_Hover, true);
    connect(this, &QAbstractButton::toggled, this, [this](bool on) {
        auto *anim = new QPropertyAnimation(this, "tickProgress", this);
        anim->setDuration(on ? 260 : 160);
        anim->setStartValue(m_tick);
        anim->setEndValue(on ? 1.0 : 0.0);
        anim->setEasingCurve(on ? QEasingCurve::OutBack : QEasingCurve::InCubic);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    });
}

void AnimatedCheckBox::setTickProgress(qreal v) {
    m_tick = v; update();
}

void AnimatedCheckBox::enterEvent(QEnterEvent *e) {
    m_hover = true; update();
    QAbstractButton::enterEvent(e);
}

void AnimatedCheckBox::leaveEvent(QEvent *e) {
    m_hover = false; update();
    QAbstractButton::leaveEvent(e);
}

void AnimatedCheckBox::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r(1.5, 1.5, width() - 3, height() - 3);

    // Kutu: unchecked → koyu / checked → gradient mavi-mor
    const bool on = m_tick > 0.01;
    if (on) {
        // Arka plan: checked oranıyla karışım (animasyon boyunca yumuşak geçiş)
        QLinearGradient g(r.topLeft(), r.bottomRight());
        g.setColorAt(0, QColor(43, 109, 245));
        g.setColorAt(1, QColor(123, 63, 228));
        QColor base(26, 31, 44);
        QColor blended;
        blended.setRedF(base.redF()   * (1 - m_tick) + 0.30 * m_tick);
        blended.setGreenF(base.greenF() * (1 - m_tick) + 0.45 * m_tick);
        blended.setBlueF(base.blueF() * (1 - m_tick) + 0.85 * m_tick);
        p.setBrush(QBrush(g));
    } else {
        p.setBrush(QColor(16, 20, 30));
    }
    p.setPen(QPen(m_hover ? QColor(90, 155, 255) : QColor(58, 69, 94), 1.6));
    p.drawRoundedRect(r, 5, 5);

    // Tik çizimi: polyline (3,11)→(9,17)→(18,6), stroke-dashoffset gibi
    if (m_tick > 0.02) {
        const qreal t = std::min<qreal>(1.0, m_tick);
        QPainterPath path;
        path.moveTo(5.5, 11.5);
        path.lineTo(9.5, 15.5);
        path.lineTo(16.5, 7.5);
        // Scale animasyonu: merkezden ölçekle (küçükten büyüğe)
        const qreal scale = 0.7 + 0.3 * t;
        p.save();
        p.translate(width() / 2.0, height() / 2.0);
        p.scale(scale, scale);
        p.translate(-width() / 2.0, -height() / 2.0);

        QPen pen(Qt::white);
        pen.setWidthF(2.4);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        p.setPen(pen);
        // Path'i kısmi çizmek için stroke progress (0..t)
        QPainterPath partial;
        qreal len = path.length();
        // Qt'de pathslice yok; yaklaşık: pointAtPercent ile polyline noktalarını üret
        const int steps = 28;
        for (int i = 0; i <= steps; ++i) {
            const qreal pct = (qreal(i) / steps) * t;
            QPointF pt = path.pointAtPercent(std::min<qreal>(pct, 1.0));
            if (i == 0) partial.moveTo(pt);
            else        partial.lineTo(pt);
        }
        p.setOpacity(std::min<qreal>(1.0, t * 1.6));
        p.drawPath(partial);
        p.restore();
    }
}

// ============================================================================
//  VoLauraSplash — açılış ekranı (logo + pulse + gradient aura)
// ============================================================================
VoLauraSplash::VoLauraSplash(const QPixmap &logo, QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::SplashScreen | Qt::WindowStaysOnTopHint),
      m_logo(logo) {
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(320, 320);
    if (auto *scr = QGuiApplication::primaryScreen()) {
        QRect g = scr->geometry();
        move(g.center() - QPoint(width()/2, height()/2));
    }

    // Pulse — yumuşak halo nabızı
    auto *aPulse = new QPropertyAnimation(this, "pulse", this);
    aPulse->setDuration(4000);
    aPulse->setStartValue(0.0);
    aPulse->setKeyValueAt(0.5, 1.0);
    aPulse->setEndValue(0.0);
    aPulse->setLoopCount(-1);
    aPulse->setEasingCurve(QEasingCurve::InOutSine);
    aPulse->start(QAbstractAnimation::DeleteWhenStopped);

    // Orbit — bokeh + yörünge
    auto *aOrbit = new QPropertyAnimation(this, "orbit", this);
    aOrbit->setDuration(14000);
    aOrbit->setStartValue(0.0);
    aOrbit->setEndValue(1.0);
    aOrbit->setLoopCount(-1);
    aOrbit->setEasingCurve(QEasingCurve::Linear);
    aOrbit->start(QAbstractAnimation::DeleteWhenStopped);

    // Logo intro — tek seferlik 0→1 (fade-in)
    auto *aLogo = new QPropertyAnimation(this, "logoIn", this);
    aLogo->setDuration(1100);
    aLogo->setStartValue(0.0);
    aLogo->setEndValue(1.0);
    aLogo->setEasingCurve(QEasingCurve::OutCubic);
    aLogo->start(QAbstractAnimation::DeleteWhenStopped);

    // Sweep — light-sweep shimmer (2.8s ilerler, durur, tekrar)
    auto *aSweep = new QPropertyAnimation(this, "sweep", this);
    aSweep->setDuration(2800);
    aSweep->setStartValue(-0.2);
    aSweep->setKeyValueAt(0.6, 1.2);
    aSweep->setEndValue(1.2);
    aSweep->setLoopCount(-1);
    aSweep->setEasingCurve(QEasingCurve::InOutQuad);
    aSweep->start(QAbstractAnimation::DeleteWhenStopped);
}

// Yardımcı: otpauth URL'inden QR kod pixmap üretir.
// pxSize toplam pixmap boyutu; quietZone = beyaz kenar boşluğu (modül cinsinden).
static QPixmap makeQrPixmap(const QString &text, int pxSize = 220, int quietZone = 4) {
    using qrcodegen::QrCode;
    try {
        const QByteArray utf8 = text.toUtf8();
        const QrCode qr = QrCode::encodeText(utf8.constData(), QrCode::Ecc::MEDIUM);
        const int n = qr.getSize();
        const int total = n + 2 * quietZone;
        // Modül boyutu için tam piksel — sürtmesin diye total'a böl, geri kalan kenarda
        const int scale = std::max(1, pxSize / total);
        const int finalSize = scale * total;
        QImage img(finalSize, finalSize, QImage::Format_RGB32);
        img.fill(Qt::white);
        QPainter p(&img);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(15, 18, 28)); // marka koyu lacivert (beyaz arkada okunabilir)
        for (int y = 0; y < n; ++y) {
            for (int x = 0; x < n; ++x) {
                if (qr.getModule(x, y)) {
                    p.drawRect((x + quietZone) * scale, (y + quietZone) * scale, scale, scale);
                }
            }
        }
        p.end();
        return QPixmap::fromImage(img);
    } catch (...) {
        // QR encoding sırasında bir hata olursa boş pixmap döndür
        return QPixmap();
    }
}

// Yardımcı: stylized V ribbon shape (iç + dış ribbon yayları)
static QPainterPath buildVRibbon(const QPointF &center, qreal size) {
    // V harfinin iki bacağını iki ribbon şeridi olarak çiziyoruz
    // Bunları iki ayrı path'in birleşimi olarak döndürmek yerine tek dolgulu path döndür
    QPainterPath path;
    const qreal w = size;          // genel genişlik
    const qreal h = size * 0.82;   // yükseklik
    const qreal thick = size * 0.19; // ribbon kalınlığı
    const qreal topOffset = size * 0.06;

    const qreal cx = center.x();
    const qreal cy = center.y();

    // Sol bacak (dış çizgi: sol üst → dip, iç çizgi: dip → biraz sağ/üst)
    // Kontrol noktaları ribbon akışı için hafif kavis
    {
        QPainterPath leg;
        const QPointF topL (cx - w * 0.46, cy - h * 0.44 + topOffset);
        const QPointF topLi(cx - w * 0.46 + thick * 0.9, cy - h * 0.44 + topOffset);
        const QPointF bot  (cx - w * 0.04, cy + h * 0.44);
        const QPointF boti (cx - w * 0.04 + thick * 0.35, cy + h * 0.44 - thick * 0.9);
        leg.moveTo(topL);
        leg.cubicTo(cx - w * 0.38, cy - h * 0.10,
                    cx - w * 0.22, cy + h * 0.20,
                    bot.x(), bot.y());
        leg.lineTo(boti);
        leg.cubicTo(cx - w * 0.16, cy + h * 0.10,
                    cx - w * 0.30, cy - h * 0.10,
                    topLi.x(), topLi.y());
        leg.closeSubpath();
        path.addPath(leg);
    }
    // Sağ bacak (ayna) — ribbon bitmiyor, üst ucu hafif yukarı uzanıp kıvrık bitiyor
    {
        QPainterPath leg;
        const QPointF topR (cx + w * 0.46, cy - h * 0.44 + topOffset);
        const QPointF topRi(cx + w * 0.46 - thick * 0.9, cy - h * 0.44 + topOffset);
        const QPointF bot  (cx + w * 0.04, cy + h * 0.44);
        const QPointF boti (cx + w * 0.04 - thick * 0.35, cy + h * 0.44 - thick * 0.9);
        leg.moveTo(topR);
        leg.cubicTo(cx + w * 0.38, cy - h * 0.10,
                    cx + w * 0.22, cy + h * 0.20,
                    bot.x(), bot.y());
        leg.lineTo(boti);
        leg.cubicTo(cx + w * 0.16, cy + h * 0.10,
                    cx + w * 0.30, cy - h * 0.10,
                    topRi.x(), topRi.y());
        leg.closeSubpath();
        path.addPath(leg);
    }
    // Hafif ek kıvrım — sağ üst ucu ribbon "swish"
    {
        QPainterPath swish;
        const QPointF a(cx + w * 0.46, cy - h * 0.44 + topOffset);
        const QPointF b(cx + w * 0.52, cy - h * 0.52);
        const QPointF c1(cx + w * 0.58, cy - h * 0.30);
        const QPointF c2(cx + w * 0.40, cy - h * 0.30);
        swish.moveTo(a);
        swish.cubicTo(cx + w * 0.58, cy - h * 0.38,
                      cx + w * 0.60, cy - h * 0.52,
                      b.x(), b.y());
        const QPointF endP(cx + w * 0.46 - thick * 0.9, cy - h * 0.44 + topOffset);
        swish.cubicTo(c1, c2, endP);
        swish.closeSubpath();
        path.addPath(swish);
    }
    return path;
}

void VoLauraSplash::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    const QRectF r = rect();
    const QPointF c = r.center();

    // 1) Sade yuvarlak arka plan — düz koyu, ince kenarlık
    {
        QPainterPath card;
        card.addRoundedRect(r, 18, 18);
        p.setClipPath(card);
        p.fillRect(r, QColor(0x0a, 0x0a, 0x0b));
        p.setClipping(false);
        QPen border(QColor(0x26, 0x26, 0x2b));
        border.setWidthF(1.0);
        p.setPen(border);
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 18, 18);
    }

    // 2) Logo — fade-in
    if (!m_logo.isNull()) {
        const int targetH = 96;
        QPixmap scaled = m_logo.scaledToHeight(targetH, Qt::SmoothTransformation);
        QPointF lp(c.x() - scaled.width()  / 2.0,
                   c.y() - scaled.height() / 2.0 - 28);
        p.save();
        p.setOpacity(m_logoIn);
        p.drawPixmap(lp, scaled);
        p.restore();
    }

    // 3) Marka ismi — sade beyaz, letter-spacing yok
    {
        const qreal nameAlpha = qBound(0.0, (m_logoIn - 0.3) / 0.7, 1.0);
        p.setPen(QColor(0xfa, 0xfa, 0xfa, int(255 * nameAlpha)));
        QFont f = p.font();
        f.setPointSize(20);
        f.setWeight(QFont::DemiBold);
        p.setFont(f);
        p.drawText(QRect(0, int(c.y() + 38), width(), 30),
                   Qt::AlignHCenter | Qt::AlignVCenter, "VoLaura");
    }

    // 4) İnce indeterminate çizgi — indigo
    {
        const qreal barAlpha = qBound(0.0, (m_logoIn - 0.4) / 0.6, 1.0);
        const qreal w = 140;
        const qreal x0 = c.x() - w / 2;
        const qreal y  = c.y() + 86;
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x26, 0x26, 0x2b, int(255 * barAlpha)));
        p.drawRoundedRect(QRectF(x0, y, w, 2), 1, 1);
        const qreal segW = 50;
        const qreal sx = x0 - segW + std::fmod(m_orbit * 2.0, 1.0) * (w + segW);
        p.setBrush(QColor(0x63, 0x66, 0xf1, int(255 * barAlpha)));
        p.drawRoundedRect(QRectF(sx, y, segW, 2), 1, 1);
    }
}

void VoLauraSplash::finish(QWidget *mainWin) {
    // Fade-out, ardından ana pencereyi göster
    auto *eff = new QGraphicsOpacityEffect(this);
    setGraphicsEffect(eff);
    eff->setOpacity(1.0);
    auto *fade = new QPropertyAnimation(eff, "opacity", this);
    fade->setDuration(400);
    fade->setStartValue(1.0);
    fade->setEndValue(0.0);
    fade->setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(fade, &QPropertyAnimation::finished, this, [this, mainWin]() {
        if (mainWin) { mainWin->show(); mainWin->raise(); mainWin->activateWindow(); }
        close();
        deleteLater();
    });
    fade->start(QAbstractAnimation::DeleteWhenStopped);
}

// ============================================================================
//  NetworkBackground — slate-blue gradient + ağ düğümleri
// ============================================================================
NetworkBackground::NetworkBackground(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground, false);
    // Performans için ana ekran arka planını opaque işaretle: alttaki widget'lar
    // gereksiz yere repaint olmasın. WA_OpaquePaintEvent paintEvent'ten önce arka
    // planı temizlemeyi atlatır, böylece daha hızlı blit yaparız.
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    rebuildNodes();
    // ESKİ: phase QPropertyAnimation (30s loop) — sürekli repaint tetikliyordu,
    // tüm UI'da fark edilen yavaşlamanın temel nedeniydi. Statik bırakıyoruz;
    // arkaplan cache zaten gradient + edge'leri içeriyor, tek blit yeterli.
    setPhase(0.5); // tek seferlik fixed phase (nokta parlaklığı orta seviye)
}

void NetworkBackground::resizeEvent(QResizeEvent *e) {
    rebuildNodes();
    rebuildBackgroundCache();
    QWidget::resizeEvent(e);
}

void NetworkBackground::rebuildNodes() {
    m_nodes.clear();
    m_edges.clear();
    if (width() < 10 || height() < 10) return;
    const int N = 28; // daha az düğüm, daha akıcı
    quint32 s = 0x9E3779B1u;
    auto rnd = [&]() -> qreal {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (s & 0xFFFFFF) / qreal(0xFFFFFF);
    };
    for (int i = 0; i < N; ++i) {
        Node n;
        n.pos = QPointF(rnd() * width(), rnd() * height());
        n.r = 1.2 + rnd() * 1.4;
        n.phaseOffset = rnd();
        m_nodes.push_back(n);
    }
    // Edge'leri tek seferde hesapla
    const qreal maxDist = 180.0;
    for (int i = 0; i < m_nodes.size(); ++i) {
        for (int j = i + 1; j < m_nodes.size(); ++j) {
            const qreal dx = m_nodes[i].pos.x() - m_nodes[j].pos.x();
            const qreal dy = m_nodes[i].pos.y() - m_nodes[j].pos.y();
            const qreal d  = std::sqrt(dx*dx + dy*dy);
            if (d < maxDist) {
                Edge e;
                e.a = i; e.b = j;
                e.alpha = (1.0 - d / maxDist) * 0.20;
                m_edges.push_back(e);
            }
        }
    }
}

void NetworkBackground::rebuildBackgroundCache() {
    if (width() <= 0 || height() <= 0) { m_bgCache = QPixmap(); return; }
    const qreal dpr = devicePixelRatioF();
    m_bgCache = QPixmap(int(width() * dpr), int(height() * dpr));
    m_bgCache.setDevicePixelRatio(dpr);
    m_bgCache.fill(Qt::transparent);

    QPainter p(&m_bgCache);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r(0, 0, width(), height());

    // Sade düz arka plan — tek koyu renk
    p.fillRect(r, QColor(0x0a, 0x0a, 0x0b));

    p.end();
}

void NetworkBackground::paintEvent(QPaintEvent *) {
    if (m_bgCache.isNull()) rebuildBackgroundCache();
    QPainter p(this);
    if (!m_bgCache.isNull()) p.drawPixmap(0, 0, m_bgCache);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      welcomeModal(nullptr),
      roomTitleLabel(nullptr),
      chatHistory(nullptr),
      messageInput(nullptr),
      participantList(nullptr),
      remoteScreenLabel(nullptr),
      remoteCameraLabel(nullptr),
      createRoomBtn(nullptr),
      joinRoomBtn(nullptr),
      muteBtn(nullptr),
      screenShareBtn(nullptr),
      cameraBtn(nullptr),
      connectionStatusLabel(nullptr),
      signalingClient(new SignalingClient("wss://volaura.xyz:8444", this)),
      isMuted(false),
      isScreenSharing(false),
      isCameraOn(false),
      screenShareTimer(new QTimer(this)),
      voicePingTimer(new QTimer(this)),
      cameraShareTimer(new QTimer(this)),
      camera(nullptr),
      cameraVideoSink(nullptr),
      cameraCaptureSession(nullptr),
      backgroundBlurEffect(nullptr),
      participantGrid(nullptr),
      participantGridWidget(nullptr),
      settingsScreenIndex(-1),
      settingsScreenFps(30),
      settingsScreenHeight(1080),
      settingsJpegQuality(85),
      isLoggedIn(false),
      loginScreen(nullptr),
      loginUserInput(nullptr),
      loginEmailInput(nullptr),
      loginPassInput(nullptr),
      loginErrorLabel(nullptr),
      loginInfoLabel(nullptr),
      loginSubmitBtn(nullptr),
      loginToggleBtn(nullptr),
      loginForgotBtn(nullptr),
      loginResendBtn(nullptr),
      loginInRegisterMode(false),
      friendsSidebarLayout(nullptr),
      requestsBtn(nullptr),
      ringingDialog(nullptr),
      titleBar(nullptr),
      titleMaxBtn(nullptr),
      sizeGrip(nullptr),
      windowDragging(false) {
    setWindowTitle("VoLaura - Sesli Sohbet");
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    resize(1280, 760);
    setMinimumSize(1080, 640);

    loadSettings();

    connect(signalingClient, &SignalingClient::roomCreated, this, &MainWindow::onRoomCreated);
    connect(signalingClient, &SignalingClient::roomJoined, this, &MainWindow::onRoomJoined);
    connect(signalingClient, &SignalingClient::error, this, &MainWindow::onError);
    connect(signalingClient, &SignalingClient::chatMessageReceived, this, &MainWindow::onChatMessageReceived);
    connect(signalingClient, &SignalingClient::participantJoined, this, &MainWindow::onParticipantJoined);
    connect(signalingClient, &SignalingClient::participantLeft, this, &MainWindow::onParticipantLeft);
    connect(signalingClient, &SignalingClient::participantMediaStateChanged, this, &MainWindow::onParticipantMediaStateChanged);
    connect(signalingClient, &SignalingClient::participantsListed, this, &MainWindow::onParticipantsListed);
    connect(signalingClient, &SignalingClient::mediaChunkReceived, this, &MainWindow::onMediaChunkReceived);
    // İlk başarılı bağlantı sessiz; yalnızca yeniden bağlanmalarda kullanıcıya bilgi ver.
    connect(signalingClient, &SignalingClient::connected, this, [this]() {
        if (hasShownDisconnectToast) {
            showToast(QString::fromUtf8("Yeniden bağlandı"), "success", 1600);
            hasShownDisconnectToast = false;
        }
    });
    connect(signalingClient, &SignalingClient::disconnected, this, [this]() {
        if (!isLoggedIn) return; // henüz login olmadıysa sessiz geç
        showToast(QString::fromUtf8("Bağlantı koptu"), "warn", 2500);
        hasShownDisconnectToast = true;
    });

    // Auth & friends
    connect(signalingClient, &SignalingClient::loginResult, this, &MainWindow::onLoginResult);
    connect(signalingClient, &SignalingClient::login2faRequired, this,
            [this](const QString &uname, bool totp, bool sms, bool email,
                   const QString &phoneHint, const QString &emailHint) {
        Q_UNUSED(uname);
        if (loginSubmitBtn) loginSubmitBtn->setEnabled(true);
        // Auto-login sırasında 2FA gerekirse: kullanıcıdan kod iste,
        // ama remember-me'yi SİLME — kod doğrulanırsa 30-gün remember devam etsin.
        // Bu şekilde TOTP açtıktan sonra her açılışta sadece TOTP kodu istenir,
        // kullanıcı adı/şifre tekrar girmek gerekmez.
        if (autoLoginSilent) {
            autoLoginSilent = false;
            // pendingAutoLoginPass tutuluyor — 2FA başarılıysa onLoginResult saveRememberMe çağıracak
            if (loginScreen) { loginScreen->show(); loginScreen->raise(); }
        }
        showLogin2faDialog(totp, sms, email, phoneHint, emailHint);
    });
    connect(signalingClient, &SignalingClient::registerResult, this, &MainWindow::onRegisterResult);
    connect(signalingClient, &SignalingClient::registerVerifyPending, this, &MainWindow::onRegisterVerifyPending);
    connect(signalingClient, &SignalingClient::loginNeedsVerification, this, &MainWindow::onLoginNeedsVerification);
    connect(signalingClient, &SignalingClient::passwordResetSent, this, &MainWindow::onPasswordResetSent);
    connect(signalingClient, &SignalingClient::verificationSent, this, &MainWindow::onVerificationSent);
    connect(signalingClient, &SignalingClient::friendsListUpdated, this, &MainWindow::onFriendsListUpdated);
    connect(signalingClient, &SignalingClient::friendRequestReceived, this, &MainWindow::onFriendRequestReceived);
    connect(signalingClient, &SignalingClient::friendAdded, this, &MainWindow::onFriendAdded);
    connect(signalingClient, &SignalingClient::friendRemoved, this, &MainWindow::onFriendRemoved);
    connect(signalingClient, &SignalingClient::friendStatusChanged, this, &MainWindow::onFriendStatusChanged);
    connect(signalingClient, &SignalingClient::friendOpResult, this, &MainWindow::onFriendOpResult);

    // Calling
    connect(signalingClient, &SignalingClient::incomingCall, this, &MainWindow::onIncomingCall);
    connect(signalingClient, &SignalingClient::callDeclined, this, &MainWindow::onCallDeclined);
    connect(signalingClient, &SignalingClient::callCancelled, this, &MainWindow::onCallCancelled);
    connect(signalingClient, &SignalingClient::callAccepted, this, &MainWindow::onCallAccepted);
    connect(signalingClient, &SignalingClient::callUnreachable, this, &MainWindow::onCallUnreachable);
    connect(signalingClient, &SignalingClient::callError, this, &MainWindow::onCallError);

    // Discord-benzeri
    connect(signalingClient, &SignalingClient::serversListed,   this, &MainWindow::onServersListed);
    connect(signalingClient, &SignalingClient::serverCreated,   this, &MainWindow::onServerCreated);
    connect(signalingClient, &SignalingClient::serverJoined,    this, &MainWindow::onServerJoined);
    connect(signalingClient, &SignalingClient::serverLeft,      this, &MainWindow::onServerLeft);
    connect(signalingClient, &SignalingClient::serverDeleted,   this, &MainWindow::onServerDeleted);
    connect(signalingClient, &SignalingClient::serverRenamed,   this, &MainWindow::onServerRenamed);
    connect(signalingClient, &SignalingClient::channelsListed,  this, &MainWindow::onChannelsListed);
    connect(signalingClient, &SignalingClient::channelCreated,  this, &MainWindow::onChannelCreated);
    connect(signalingClient, &SignalingClient::channelDeleted,  this, &MainWindow::onChannelDeleted);
    connect(signalingClient, &SignalingClient::channelRenamed,  this, &MainWindow::onChannelRenamed);
    connect(signalingClient, &SignalingClient::channelMessagesListed,  this, &MainWindow::onChannelMessagesListed);
    connect(signalingClient, &SignalingClient::channelMessageReceived, this, &MainWindow::onChannelMessageReceived);
    connect(signalingClient, &SignalingClient::channelMessageDeleted,  this, &MainWindow::onChannelMessageDeleted);
    connect(signalingClient, &SignalingClient::channelMessageEdited,   this, &MainWindow::onChannelMessageEdited);
    connect(signalingClient, &SignalingClient::membersListed,          this, &MainWindow::onMembersListed);
    connect(signalingClient, &SignalingClient::memberJoined,           this, &MainWindow::onMemberJoined);
    connect(signalingClient, &SignalingClient::memberLeft,             this, &MainWindow::onMemberLeft);
    connect(signalingClient, &SignalingClient::dmThreadsListed,        this, &MainWindow::onDmThreadsListed);
    connect(signalingClient, &SignalingClient::dmMessagesListed,       this, &MainWindow::onDmMessagesListed);
    connect(signalingClient, &SignalingClient::dmReceived,             this, &MainWindow::onDmReceived);
    // E2E: peer pub key cevabı geldiğinde cache'i güncelle ve queued mesajları gönder
    connect(signalingClient, &SignalingClient::publicKeyResult, this,
            [this](const QString &username, const QString &pubKeyB64, bool found) {
        Q_UNUSED(found);
        e2ePeerKeyCache[username] = pubKeyB64; // boş = E2E yok
        // Bu peer için bekleyen plaintext mesaj varsa şimdi şifreleyip gönder
        if (!e2ePendingPlaintextByPeer.contains(username)) return;
        const QStringList pending = e2ePendingPlaintextByPeer.take(username);
        for (const QString &text : pending) {
            QString ct, nonce;
            if (!pubKeyB64.isEmpty() &&
                E2E::encryptForPeer(pubKeyB64, text, &ct, &nonce)) {
                signalingClient->sendDmEncrypted(username, ct, nonce, E2E::publicKeyB64());
            } else {
                // Peer'in pub key'i yok — düz metin olarak gönder
                signalingClient->sendDm(username, text);
            }
        }
    });
    connect(signalingClient, &SignalingClient::dmDeleted, this, [this](const QString &peer, qint64 mid) {
        if (chatMode == ChatMode::Dm && currentDmPeer == peer) removeChatRow(mid);
    });
    connect(signalingClient, &SignalingClient::dmEdited, this, [this](const QString &peer, qint64 mid, const QString &content) {
        if (chatMode == ChatMode::Dm && currentDmPeer == peer) updateChatRow(mid, content, true);
    });
    connect(signalingClient, &SignalingClient::typingChannelReceived, this,
            [this](int /*serverId*/, int channelId, const QString &username) {
        if (chatMode != ChatMode::Channel || channelId != currentChannelId) return;
        if (username == currentUserName) return;
        activeTypers[username] = QDateTime::currentDateTime();
        refreshTypingLabel();
    });
    connect(signalingClient, &SignalingClient::typingDmReceived, this,
            [this](const QString &fromUsername) {
        if (chatMode != ChatMode::Dm || currentDmPeer != fromUsername) return;
        activeTypers[fromUsername] = QDateTime::currentDateTime();
        refreshTypingLabel();
    });

    // Voice channels
    connect(signalingClient, &SignalingClient::voiceJoined,        this, &MainWindow::onVoiceJoined);
    connect(signalingClient, &SignalingClient::voiceLeft,          this, &MainWindow::onVoiceLeft);
    connect(signalingClient, &SignalingClient::voiceMemberJoined,  this, &MainWindow::onVoiceMemberJoined);
    connect(signalingClient, &SignalingClient::voiceMemberLeft,    this, &MainWindow::onVoiceMemberLeft);
    // Sesli kanaldaki mevcut üyeleri listele (kullanıcı katılmadan da gösterilir)
    connect(signalingClient, &SignalingClient::voiceParticipantsListed, this,
            [this](int channelId, const QJsonArray &participants) {
        // Bu, açık olan kanal panelimize ait mi? (henüz katılmadıysak da panel açıksa)
        if (channelId != currentChannelId && channelId != currentVoiceChannelId) return;
        // Eğer bu kanala katılmışsak voiceMembers zaten dolu — ama liste daha güncel olabilir
        voiceMembers.clear();
        for (const auto &v : participants) {
            const QJsonObject o = v.toObject();
            voiceMembers.insert((qint64)o.value("userId").toDouble(), o);
        }
        if (voicePanelStatus) {
            const int n = voiceMembers.size();
            if (currentVoiceChannelId == channelId) {
                voicePanelStatus->setText(QString::fromUtf8("Bağlandı · %1 kişi").arg(n));
            } else {
                voicePanelStatus->setText(n == 0
                    ? QString::fromUtf8("Bu kanalda kimse yok")
                    : QString::fromUtf8("%1 kişi sesli kanalda").arg(n));
            }
        }
        rebuildVoiceMembersList();
    });
    connect(signalingClient, &SignalingClient::voiceStateChanged,  this, &MainWindow::onVoiceStateChanged);
    connect(signalingClient, &SignalingClient::voiceChunkReceived, this, &MainWindow::onVoiceChunkReceived);
    connect(signalingClient, &SignalingClient::voiceShareStarted,  this, &MainWindow::onVoiceShareStarted);
    connect(signalingClient, &SignalingClient::voiceShareStopped,  this, &MainWindow::onVoiceShareStopped);
    connect(signalingClient, &SignalingClient::voiceMediaChunkReceived, this, &MainWindow::onVoiceMediaChunk);

    applyScreenShareInterval();
    voicePingTimer->setInterval(1000);
    cameraShareTimer->setInterval(180);
    connect(screenShareTimer, &QTimer::timeout, this, &MainWindow::captureAndSendScreenFrame);
    connect(voicePingTimer, &QTimer::timeout, this, &MainWindow::sendVoiceActivityPing);
    connect(cameraShareTimer, &QTimer::timeout, this, &MainWindow::sendCameraFrame);

    applyCameraDevice();

    setupUi();
    initSystemTray();
    initUpdateChecker();
    signalingClient->connectToServer();
    showLoginScreen();
    // 30-gün remember-me: saklanan credential varsa otomatik giriş dene
    tryAutoLogin();
}

MainWindow::~MainWindow() {
    if (camera) {
        camera->stop();
    }
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    const int tbh = titleBar ? titleBar->height() : 0;
    QRect contentRect(0, tbh, width(), height() - tbh);
    // welcomeModal kaldırıldı (legacy); login overlay tüm content alanını kaplar.
    if (loginScreen)  loginScreen->setGeometry(contentRect);
    if (sizeGrip)     sizeGrip->move(width() - sizeGrip->width() - 2, height() - sizeGrip->height() - 2);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    if (obj == titleBar) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                windowDragging = true;
                windowDragOffset = me->globalPosition().toPoint() - frameGeometry().topLeft();
                return false;
            }
        } else if (event->type() == QEvent::MouseMove) {
            auto *me = static_cast<QMouseEvent*>(event);
            if (windowDragging && (me->buttons() & Qt::LeftButton)) {
                if (isMaximized()) {
                    // restore + cursoru yeni pencereye hizala
                    showNormal();
                    windowDragOffset = QPoint(width() / 2, titleBar->height() / 2);
                    if (titleMaxBtn) titleMaxBtn->setText(QString::fromUtf8("☐"));
                }
                move(me->globalPosition().toPoint() - windowDragOffset);
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            windowDragging = false;
        } else if (event->type() == QEvent::MouseButtonDblClick) {
            if (isMaximized()) {
                showNormal();
                if (titleMaxBtn) titleMaxBtn->setText(QString::fromUtf8("☐"));
            } else {
                showMaximized();
                if (titleMaxBtn) titleMaxBtn->setText(QString::fromUtf8("❐"));
            }
            return true;
        }
    }
    // Voice media tile double-click → fullscreen
    if (event->type() == QEvent::MouseButtonDblClick) {
        QLabel *lbl = qobject_cast<QLabel*>(obj);
        if (lbl) {
            const QString uname = lbl->property("voiceTileUser").toString();
            const QString kind  = lbl->property("voiceTileKind").toString();
            if (!uname.isEmpty() && !kind.isEmpty()) {
                openVoiceTileFullscreen(uname, kind);
                return true;
            }
        }
    }
    // Mesaj resim tıklaması → tam boyut viewer
    if (event->type() == QEvent::MouseButtonPress) {
        QLabel *lbl = qobject_cast<QLabel*>(obj);
        if (lbl) {
            QVariant v = lbl->property("fullPixmap");
            if (v.isValid()) {
                QPixmap pm = v.value<QPixmap>();
                if (!pm.isNull()) {
                    auto *dlg = new QDialog(this);
                    dlg->setAttribute(Qt::WA_DeleteOnClose);
                    dlg->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
                    dlg->setAttribute(Qt::WA_TranslucentBackground);
                    auto *outer = new QVBoxLayout(dlg);
                    outer->setContentsMargins(0,0,0,0);
                    auto *card = new QWidget(dlg);
                    card->setStyleSheet("background:#0b0e16; border:1px solid rgba(255,255,255,0.05);"
                                        " border-radius:14px;");
                    outer->addWidget(card);
                    auto *cl = new QVBoxLayout(card);
                    cl->setContentsMargins(8, 8, 8, 8);
                    auto *imgL = new QLabel();
                    QSize avail = QGuiApplication::primaryScreen()->availableGeometry().size();
                    QPixmap big = pm.scaled(avail * 0.9, Qt::KeepAspectRatio,
                                            Qt::SmoothTransformation);
                    imgL->setPixmap(big);
                    imgL->setCursor(Qt::PointingHandCursor);
                    cl->addWidget(imgL);
                    imgL->installEventFilter(dlg);
                    dlg->setFixedSize(big.size() + QSize(32, 32));
                    QObject::connect(imgL, &QObject::destroyed, dlg, &QObject::deleteLater);
                    // Kapatmak için ESC veya tıklama
                    dlg->show();
                    auto closeOnClick = new QShortcut(QKeySequence(Qt::Key_Escape), dlg);
                    QObject::connect(closeOnClick, &QShortcut::activated, dlg, &QDialog::close);
                    return true;
                }
            }
        }
    }
    // Mesaj satırı hover → toolbar göster/gizle + resize ile yeniden konumla
    {
        QWidget *w = qobject_cast<QWidget*>(obj);
        if (w) {
            QVariant v = w->property("hoverTools");
            if (v.isValid()) {
                QWidget *tools = v.value<QWidget*>();
                if (tools) {
                    if (event->type() == QEvent::Enter) {
                        tools->adjustSize();
                        tools->move(w->width() - tools->width() - 10, -10);
                        tools->show();
                        tools->raise();
                    } else if (event->type() == QEvent::Leave) {
                        tools->hide();
                    } else if (event->type() == QEvent::Resize) {
                        tools->adjustSize();
                        tools->move(w->width() - tools->width() - 10, -10);
                    }
                }
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

// ============================================================================
//  TEMA — tüm renkler burada, tutarlı ve modern koyu tema
// ============================================================================
namespace T {
    // Modern minimal dark — Linear/Notion tarzı
    constexpr auto Bg      = "#0a0a0b";
    constexpr auto Sidebar = "#0f0f11";
    constexpr auto ChatCol = "#0a0a0b";
    constexpr auto Panel   = "#141417";
    constexpr auto Header  = "#0a0a0b";
    constexpr auto Input   = "#17171a";
    constexpr auto Card    = "#141417";
    constexpr auto Border  = "#26262b";
    constexpr auto Text    = "#fafafa";
    constexpr auto Sub     = "#a1a1aa";
    constexpr auto Muted   = "#52525b";
    constexpr auto Accent  = "#6366f1";
    constexpr auto Accent2 = "#a855f7";
    constexpr auto Danger  = "#ef4444";
    constexpr auto Success = "#22c55e";
}

QWidget *MainWindow::buildTitleBar() {
    QWidget *bar = new QWidget();
    bar->setObjectName("vlTitleBar");
    bar->setFixedHeight(36);
    bar->setAttribute(Qt::WA_StyledBackground, true);
    bar->setStyleSheet(QString(
        "#vlTitleBar{background:%1; border-bottom:1px solid %2;}"
        "QLabel{background:transparent; color:%3; font-size:12px; border:none;}").arg(T::Header, T::Border, T::Sub));

    auto *h = new QHBoxLayout(bar);
    h->setContentsMargins(10, 0, 0, 0);
    h->setSpacing(8);

    auto *icon = new QLabel();
    icon->setFixedSize(20, 20);
    icon->setPixmap(QPixmap(":/icons/volaura-logo.png")
                    .scaled(20, 20, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    h->addWidget(icon);

    auto *name = new QLabel("VoLaura");
    name->setStyleSheet(QString("color:%1; font-weight:700; font-size:12px; background:transparent;").arg(T::Text));
    h->addWidget(name);

    h->addStretch();

    const QString winBtnCss = QString(
        "QToolButton{background:transparent; color:%1; border:none; font-size:13px;}"
        "QToolButton:hover{background:rgba(255,255,255,0.06); color:%2;}").arg(T::Sub, T::Text);
    const QString closeBtnCss = QString(
        "QToolButton{background:transparent; color:%1; border:none; font-size:14px;}"
        "QToolButton:hover{background:%2; color:#fff;}").arg(T::Sub, T::Danger);

    auto *minBtn = new QToolButton();
    minBtn->setText(QString::fromUtf8("–"));
    minBtn->setFixedSize(46, 36);
    minBtn->setCursor(Qt::ArrowCursor);
    minBtn->setStyleSheet(winBtnCss);
    connect(minBtn, &QToolButton::clicked, this, &QMainWindow::showMinimized);
    h->addWidget(minBtn);

    auto *maxBtn = new QToolButton();
    maxBtn->setText(QString::fromUtf8("☐"));
    maxBtn->setFixedSize(46, 36);
    maxBtn->setCursor(Qt::ArrowCursor);
    maxBtn->setStyleSheet(winBtnCss);
    connect(maxBtn, &QToolButton::clicked, this, [this, maxBtn]() {
        if (isMaximized()) { showNormal(); maxBtn->setText(QString::fromUtf8("☐")); }
        else               { showMaximized(); maxBtn->setText(QString::fromUtf8("❐")); }
    });
    titleMaxBtn = maxBtn;
    h->addWidget(maxBtn);

    auto *closeBtn = new QToolButton();
    closeBtn->setText(QString::fromUtf8("✕"));
    closeBtn->setFixedSize(46, 36);
    closeBtn->setCursor(Qt::ArrowCursor);
    closeBtn->setStyleSheet(closeBtnCss);
    connect(closeBtn, &QToolButton::clicked, this, &QMainWindow::close);
    h->addWidget(closeBtn);

    bar->installEventFilter(this);
    return bar;
}

// ============================================================================
//  Line-icon helper — net çizgili modern ikonlar (emoji yerine)
// ============================================================================
static QIcon makeLineIcon(const QString &kind, const QColor &color, int size = 22) {
    QPixmap pm(size * 2, size * 2); // hi-DPI için 2x
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(2.0, 2.0);
    QPen pen(color);
    pen.setWidthF(1.7);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    const qreal s = size;
    const qreal cx = s / 2.0, cy = s / 2.0;

    if (kind == "plus") {
        const qreal r = s * 0.32;
        p.drawLine(QPointF(cx - r, cy), QPointF(cx + r, cy));
        p.drawLine(QPointF(cx, cy - r), QPointF(cx, cy + r));
    } else if (kind == "envelope") {
        // Klasik zarf — dikdörtgen + iki üstten ortaya inen çizgi ('V' kapak)
        const qreal w = s * 0.66, h = s * 0.48;
        QRectF box(cx - w / 2, cy - h / 2, w, h);
        // Dış kutu
        QPainterPath body;
        body.addRoundedRect(box, 2.0, 2.0);
        p.drawPath(body);
        // Kapak: sol üst → iç orta → sağ üst
        QPainterPath flap;
        const qreal inset = 1.2;
        flap.moveTo(box.left() + inset,  box.top() + inset);
        flap.lineTo(box.center().x(),    box.top() + h * 0.52);
        flap.lineTo(box.right() - inset, box.top() + inset);
        p.drawPath(flap);
    } else if (kind == "close") {
        // X — iki çapraz çizgi
        const qreal r = s * 0.28;
        p.drawLine(QPointF(cx - r, cy - r), QPointF(cx + r, cy + r));
        p.drawLine(QPointF(cx + r, cy - r), QPointF(cx - r, cy + r));
    } else if (kind == "gear") {
        // 8 dişli sade gear — dış küçük çıkıntılar + iç delik
        const qreal rOuter = s * 0.36;
        const qreal rInner = s * 0.28;
        const qreal rHole  = s * 0.10;
        QPainterPath gear;
        const int teeth = 8;
        for (int i = 0; i < teeth * 2; ++i) {
            const qreal angle = (i / qreal(teeth * 2)) * 2 * M_PI - M_PI / 2;
            const qreal r = (i % 2 == 0) ? rOuter : rInner;
            const qreal x = cx + r * std::cos(angle);
            const qreal y = cy + r * std::sin(angle);
            if (i == 0) gear.moveTo(x, y);
            else        gear.lineTo(x, y);
        }
        gear.closeSubpath();
        p.drawPath(gear);
        p.drawEllipse(QPointF(cx, cy), rHole, rHole);
    } else if (kind == "user-plus") {
        // İnsan + sağ üstte küçük + işareti
        const qreal headR = s * 0.13;
        const qreal headY = cy - s * 0.14;
        p.drawEllipse(QPointF(cx - s * 0.06, headY), headR, headR);
        // omuz/torso (yarım daire)
        QRectF body(cx - s * 0.27, cy - s * 0.02, s * 0.42, s * 0.44);
        p.drawArc(body, 0 * 16, 180 * 16);
        // küçük + (sağ üst köşe)
        const qreal pr = s * 0.10;
        const qreal px = cx + s * 0.22;
        const qreal py = cy - s * 0.18;
        p.drawLine(QPointF(px - pr, py), QPointF(px + pr, py));
        p.drawLine(QPointF(px, py - pr), QPointF(px, py + pr));
    } else if (kind == "send") {
        // Paper plane (üçgen sağa bakan)
        QPainterPath plane;
        plane.moveTo(cx - s * 0.30, cy - s * 0.24);
        plane.lineTo(cx + s * 0.32, cy);
        plane.lineTo(cx - s * 0.30, cy + s * 0.24);
        plane.lineTo(cx - s * 0.18, cy);
        plane.closeSubpath();
        p.setBrush(color);
        p.drawPath(plane);
    } else if (kind == "more") {
        // 3 vertical dots
        const qreal dr = s * 0.06;
        p.setBrush(color);
        p.setPen(Qt::NoPen);
        for (int i = -1; i <= 1; ++i)
            p.drawEllipse(QPointF(cx, cy + i * s * 0.22), dr, dr);
    } else if (kind == "phone") {
        // Klasik telefon ahizesi — diagonal kavisli kapsül
        QPainterPath ph;
        const qreal r = s * 0.36;
        ph.moveTo(cx - r * 0.95, cy - r * 0.55);
        ph.quadTo(cx - r * 0.55, cy - r * 0.95, cx - r * 0.20, cy - r * 0.65);
        ph.lineTo(cx + r * 0.10, cy - r * 0.30);
        ph.quadTo(cx, cy, cx + r * 0.30, cy + r * 0.10);
        ph.lineTo(cx + r * 0.65, cy + r * 0.20);
        ph.quadTo(cx + r * 0.95, cy + r * 0.55, cx + r * 0.55, cy + r * 0.95);
        ph.quadTo(cx + r * 0.20, cy + r * 0.95, cx - r * 0.30, cy + r * 0.50);
        ph.quadTo(cx - r * 0.95, cy - r * 0.10, cx - r * 0.95, cy - r * 0.55);
        p.drawPath(ph);
    } else if (kind == "camera") {
        // Video kamera — küçük oval gövde + sağda üçgen lens çıkıntısı
        QRectF body(cx - s * 0.30, cy - s * 0.18, s * 0.50, s * 0.36);
        QPainterPath cm;
        cm.addRoundedRect(body, 3, 3);
        p.drawPath(cm);
        // Sağdaki üçgen "lens"
        QPainterPath tri;
        tri.moveTo(body.right(), cy - s * 0.10);
        tri.lineTo(cx + s * 0.36, cy - s * 0.18);
        tri.lineTo(cx + s * 0.36, cy + s * 0.18);
        tri.lineTo(body.right(), cy + s * 0.10);
        tri.closeSubpath();
        p.drawPath(tri);
    } else if (kind == "user") {
        // İnsan silüeti — baş + omuz yarım daire
        const qreal headR = s * 0.13;
        p.drawEllipse(QPointF(cx, cy - s * 0.16), headR, headR);
        QRectF body(cx - s * 0.22, cy - s * 0.04, s * 0.44, s * 0.42);
        p.drawArc(body, 0 * 16, 180 * 16);
    } else if (kind == "log-out") {
        // Açık kapı + sağa ok (→]): solda U-şekilli kutu, sağda ok ucu
        const qreal w = s * 0.40, h = s * 0.56;
        // Kutu (sağ tarafı açık)
        QPainterPath box;
        box.moveTo(cx + s * 0.02,           cy - h / 2);
        box.lineTo(cx - w + s * 0.02,       cy - h / 2);
        box.lineTo(cx - w + s * 0.02,       cy + h / 2);
        box.lineTo(cx + s * 0.02,           cy + h / 2);
        p.drawPath(box);
        // Ok gövdesi
        p.drawLine(QPointF(cx - s * 0.18, cy), QPointF(cx + s * 0.32, cy));
        // Ok ucu (>)
        p.drawLine(QPointF(cx + s * 0.32, cy), QPointF(cx + s * 0.18, cy - s * 0.14));
        p.drawLine(QPointF(cx + s * 0.32, cy), QPointF(cx + s * 0.18, cy + s * 0.14));
    }
    p.end();
    return QIcon(pm);
}

// Modern dialog temasi (frameless themed cards) - setupUi icinde ve dialoglarda
// kullanilir. Tanim ileride (Modern UI helpers bolumunde) yapilir; burada sadece
// forward declaration. Statik olmamalı ki tek tanım kullanılsın.
extern const char *kPrettyDialogQss;

void MainWindow::setupUi() {
    QWidget *root = new QWidget(this);
    setCentralWidget(root);
    setStyleSheet(QString(
        "QWidget{background:%1; color:%2; font-family:'Segoe UI','Inter',sans-serif; font-size:13.5px;}"
        "QPushButton{font-weight:600;}"
        "QScrollBar:vertical{background:transparent; width:7px; margin:3px;}"
        "QScrollBar::handle:vertical{background:%3; border-radius:3px; min-height:28px;}"
        "QScrollBar::handle:vertical:hover{background:%4;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}"
        "QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical{background:transparent;}"
    ).arg(T::Bg, T::Text, T::Muted, T::Sub));

    auto *vroot = new QVBoxLayout(root);
    vroot->setContentsMargins(0, 0, 0, 0);
    vroot->setSpacing(0);

    titleBar = buildTitleBar();
    vroot->addWidget(titleBar);

    QWidget *content = new NetworkBackground();
    vroot->addWidget(content, 1);

    QHBoxLayout *main = new QHBoxLayout(content);
    main->setContentsMargins(0, 0, 0, 0);
    main->setSpacing(0);

    sizeGrip = new QSizeGrip(this);
    sizeGrip->resize(16, 16);
    sizeGrip->setStyleSheet("background:transparent;");
    sizeGrip->raise();

    // ===== SIDEBAR — logo + arkadaşlar + sunucular + alt butonlar =====
    QWidget *sidebar = new QWidget();
    sidebar->setFixedWidth(78);
    sidebar->setAttribute(Qt::WA_StyledBackground, true);
    sidebar->setStyleSheet(QString("background:%1; border-right:1px solid %2;").arg(T::Sidebar, T::Border));
    QVBoxLayout *sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(8, 12, 8, 10);
    sideLayout->setSpacing(8);

    QLabel *brand = new QLabel();
    brand->setFixedSize(42, 42);
    brand->setAlignment(Qt::AlignCenter);
    brand->setAttribute(Qt::WA_StyledBackground, true);
    brand->setStyleSheet("background:transparent; border:none;");
    brand->setPixmap(QPixmap(":/icons/volaura-logo.png")
                     .scaled(42, 42, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    sideLayout->addWidget(brand, 0, Qt::AlignHCenter);

    // Ayraç
    QWidget *sep1 = new QWidget();
    sep1->setFixedHeight(1);
    sep1->setStyleSheet(QString("background:%1;").arg(T::Border));
    sideLayout->addWidget(sep1);

    auto mkSideBtn = [](const QString &iconKind, const QColor &clr, const QString &tip) {
        auto *b = new QToolButton();
        b->setIcon(makeLineIcon(iconKind, clr, 20));
        b->setIconSize(QSize(20, 20));
        b->setFixedSize(44, 44);
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(tip);
        b->setStyleSheet(QString(
            "QToolButton{background:transparent; border:none; border-radius:14px; color:%1;}"
            "QToolButton:hover{background:rgba(255,255,255,0.06); color:%2;}"
        ).arg(T::Sub, T::Text));
        return b;
    };

    // Arkadaş listesi (scroll)
    QScrollArea *friendsScroll = new QScrollArea();
    friendsScroll->setWidgetResizable(true);
    friendsScroll->setFrameShape(QFrame::NoFrame);
    friendsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    friendsScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    friendsScroll->setStyleSheet("background:transparent; border:none;");
    QWidget *friendsContainer = new QWidget();
    friendsContainer->setStyleSheet("background:transparent;");
    friendsSidebarLayout = new QVBoxLayout(friendsContainer);
    friendsSidebarLayout->setContentsMargins(0, 2, 0, 2);
    friendsSidebarLayout->setSpacing(6);
    friendsSidebarLayout->addStretch();
    friendsScroll->setWidget(friendsContainer);
    sideLayout->addWidget(friendsScroll, 1);

    // Sunucu listesi (scroll)
    QScrollArea *serversScroll = new QScrollArea();
    serversScroll->setWidgetResizable(true);
    serversScroll->setFrameShape(QFrame::NoFrame);
    serversScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    serversScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    serversScroll->setStyleSheet("background:transparent; border:none;");
    serversScroll->setMaximumHeight(240);
    QWidget *serversContainer = new QWidget();
    serversContainer->setStyleSheet("background:transparent;");
    serverSidebarLayout = new QVBoxLayout(serversContainer);
    serverSidebarLayout->setContentsMargins(0, 2, 0, 2);
    serverSidebarLayout->setSpacing(6);
    serverSidebarLayout->addStretch();
    serversScroll->setWidget(serversContainer);
    sideLayout->addWidget(serversScroll);

    // Alt butonlar
    QWidget *sep2 = new QWidget();
    sep2->setFixedHeight(1);
    sep2->setStyleSheet(QString("background:%1;").arg(T::Border));
    sideLayout->addWidget(sep2);

    auto *addServerBtn = mkSideBtn("plus", QColor(T::Sub), QString::fromUtf8("Sunucu oluştur / katıl"));
    connect(addServerBtn, &QToolButton::clicked, this, &MainWindow::showServerSetupDialog);
    sideLayout->addWidget(addServerBtn, 0, Qt::AlignHCenter);

    auto *reqBtn = mkSideBtn("envelope", QColor(T::Text), QString::fromUtf8("Arkadaşlık istekleri"));
    connect(reqBtn, &QToolButton::clicked, this, &MainWindow::showRequestsDialog);
    requestsBtn = reqBtn;
    sideLayout->addWidget(reqBtn, 0, Qt::AlignHCenter);

    auto *settingsBtn = mkSideBtn("gear", QColor(T::Sub), QString::fromUtf8("Ayarlar"));
    connect(settingsBtn, &QToolButton::clicked, this, &MainWindow::showSettingsDialog);
    sideLayout->addWidget(settingsBtn, 0, Qt::AlignHCenter);

    auto *logoutBtn = mkSideBtn("log-out", QColor("#ef4444"), QString::fromUtf8("Çıkış Yap"));
    connect(logoutBtn, &QToolButton::clicked, this, &MainWindow::performLogout);
    sideLayout->addWidget(logoutBtn, 0, Qt::AlignHCenter);

    auto *addFriendBtn = mkSideBtn("user-plus", QColor(T::Sub), QString::fromUtf8("Arkadaş ekle"));
    connect(addFriendBtn, &QToolButton::clicked, this, &MainWindow::showAddFriendDialog);
    sideLayout->addWidget(addFriendBtn, 0, Qt::AlignHCenter);
    main->addWidget(sidebar);

    // ===== CHAT COLUMN — header + kanal listesi + mesajlar + input =====
    QWidget *chatColumn = new QWidget();
    chatColumn->setFixedWidth(380);
    chatColumn->setAttribute(Qt::WA_StyledBackground, true);
    chatColumn->setStyleSheet(QString("background:%1; border-right:1px solid %2;").arg(T::ChatCol, T::Border));
    QVBoxLayout *chatColLayout = new QVBoxLayout(chatColumn);
    chatColLayout->setContentsMargins(0, 0, 0, 0);
    chatColLayout->setSpacing(0);

    // Header
    QWidget *chatHeader = new QWidget();
    chatHeader->setObjectName("chatHeader");
    chatHeader->setFixedHeight(52);
    chatHeader->setAttribute(Qt::WA_StyledBackground, true);
    chatHeader->setStyleSheet(QString(
        "QWidget#chatHeader{background:%1; border-bottom:1px solid %2;}").arg(T::Header, T::Border));
    QHBoxLayout *chatHeaderLayout = new QHBoxLayout(chatHeader);
    chatHeaderLayout->setContentsMargins(18, 0, 12, 0);
    chatHeaderLayout->setSpacing(8);
    roomTitleLabel = new QLabel(QString::fromUtf8("VoLaura"));
    roomTitleLabel->setStyleSheet(QString("font-size:16px; font-weight:700; color:%1; background:transparent;").arg(T::Text));
    chatHeaderLayout->addWidget(roomTitleLabel);
    chatHeaderLayout->addStretch();

    // DM action bar
    dmActionBar = new QWidget();
    dmActionBar->setStyleSheet("background:transparent;");
    auto *dmActLay = new QHBoxLayout(dmActionBar);
    dmActLay->setContentsMargins(0, 0, 0, 0);
    dmActLay->setSpacing(4);
    const QString dmBtnCss = QString(
        "QToolButton{background:rgba(255,255,255,0.04); color:%1; border:1px solid %2;"
        " border-radius:16px; min-width:32px; min-height:32px; max-width:32px; max-height:32px;}"
        "QToolButton:hover{background:rgba(120,150,210,0.15); color:%3; border-color:rgba(120,150,210,0.4);}"
    ).arg(T::Sub, T::Border, T::Text);

    dmCallBtn = new QToolButton();
    dmCallBtn->setIcon(makeLineIcon("phone", QColor(T::Success), 16));
    dmCallBtn->setIconSize(QSize(16, 16));
    dmCallBtn->setCursor(Qt::PointingHandCursor);
    dmCallBtn->setToolTip(QString::fromUtf8("Sesli ara"));
    dmCallBtn->setStyleSheet(dmBtnCss);
    connect(dmCallBtn, &QToolButton::clicked, this, [this]() {
        if (chatMode == ChatMode::Dm && !currentDmPeer.isEmpty())
            startCallToFriend(currentDmPeer);
    });
    dmActLay->addWidget(dmCallBtn);

    dmVideoBtn = new QToolButton();
    dmVideoBtn->setIcon(makeLineIcon("camera", QColor(T::Accent), 16));
    dmVideoBtn->setIconSize(QSize(16, 16));
    dmVideoBtn->setCursor(Qt::PointingHandCursor);
    dmVideoBtn->setToolTip(QString::fromUtf8("Görüntülü ara"));
    dmVideoBtn->setStyleSheet(dmBtnCss);
    connect(dmVideoBtn, &QToolButton::clicked, this, [this]() {
        if (chatMode == ChatMode::Dm && !currentDmPeer.isEmpty()) {
            startCallToFriend(currentDmPeer);
            QTimer::singleShot(800, this, [this]() { if (!isCameraOn) toggleCamera(); });
        }
    });
    dmActLay->addWidget(dmVideoBtn);

    dmInfoBtn = new QToolButton();
    dmInfoBtn->setIcon(makeLineIcon("user", QColor(T::Sub), 16));
    dmInfoBtn->setIconSize(QSize(16, 16));
    dmInfoBtn->setCursor(Qt::PointingHandCursor);
    dmInfoBtn->setToolTip(QString::fromUtf8("Profil"));
    dmInfoBtn->setStyleSheet(dmBtnCss);
    connect(dmInfoBtn, &QToolButton::clicked, this, [this]() {
        if (chatMode == ChatMode::Dm && !currentDmPeer.isEmpty())
            showToast(QString::fromUtf8("@%1").arg(currentDmPeer), "info", 1500);
    });
    dmActLay->addWidget(dmInfoBtn);
    dmActionBar->hide();
    chatHeaderLayout->addWidget(dmActionBar);

    connectionStatusLabel = new QLabel();
    connectionStatusLabel->hide();
    chatColLayout->addWidget(chatHeader);

    // Kanal listesi paneli
    channelListPanel = new QWidget();
    channelListPanel->setAttribute(Qt::WA_StyledBackground, true);
    channelListPanel->setStyleSheet(QString("background:%1; border-bottom:1px solid %2;").arg(T::Panel, T::Border));
    auto *chanPanelLay = new QVBoxLayout(channelListPanel);
    chanPanelLay->setContentsMargins(10, 8, 10, 8);
    chanPanelLay->setSpacing(4);
    auto *chanPanelHeader = new QHBoxLayout();
    auto *chanPanelTitle = new QLabel(QString::fromUtf8("KANALLAR"));
    chanPanelTitle->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700; letter-spacing:1px;").arg(T::Muted));
    auto *chanAddBtn = new QToolButton();
    chanAddBtn->setText("+");
    chanAddBtn->setFixedSize(20, 20);
    chanAddBtn->setCursor(Qt::PointingHandCursor);
    chanAddBtn->setToolTip(QString::fromUtf8("Yeni kanal"));
    chanAddBtn->setStyleSheet(QString(
        "QToolButton{background:rgba(255,255,255,0.04); color:%1; border:none; border-radius:10px; font-size:14px; font-weight:700;}"
        "QToolButton:hover{background:%2; color:white;}").arg(T::Sub, T::Accent));
    connect(chanAddBtn, &QToolButton::clicked, this, [this]() {
        if (currentServerId <= 0) return;
        const QString role = serversById.value(currentServerId).value("role").toString();
        if (role != "owner" && role != "admin") {
            showToast(QString::fromUtf8("Kanal oluşturmak için yetkin yok."), "warn");
            return;
        }
        QDialog d(this);
        d.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        d.setAttribute(Qt::WA_TranslucentBackground);
        d.setFixedSize(400, 240);
        auto *outer = new QVBoxLayout(&d); outer->setContentsMargins(0,0,0,0);
        auto *card = new QWidget(&d);
        card->setObjectName("card"); card->setStyleSheet(kPrettyDialogQss);
        outer->addWidget(card);
        auto *l = new QVBoxLayout(card);
        l->setContentsMargins(22, 18, 22, 18); l->setSpacing(8);
        auto *t = new QLabel(QString::fromUtf8("Yeni Kanal")); t->setObjectName("title"); l->addWidget(t);
        auto *inp = new QLineEdit();
        inp->setPlaceholderText(QString::fromUtf8("Kanal adı..."));
        inp->setStyleSheet(QString("QLineEdit{background:%1; color:%2; border:1px solid %3; border-radius:8px; padding:10px;}").arg(T::Card, T::Text, T::Border));
        l->addWidget(inp);
        auto *typeRow = new QHBoxLayout();
        auto *rbText  = new QPushButton(QString::fromUtf8("# Metin"));
        auto *rbVoice = new QPushButton(QString::fromUtf8("🔊 Sesli"));
        rbText->setCheckable(true); rbVoice->setCheckable(true); rbText->setChecked(true);
        const QString chipCss = QString(
            "QPushButton{background:%1; color:%2; border:1px solid %3; border-radius:8px; padding:8px 12px;}"
            "QPushButton:checked{background:%4; color:white; border-color:%4;}").arg(T::Card, T::Sub, T::Border, T::Accent);
        rbText->setStyleSheet(chipCss); rbVoice->setStyleSheet(chipCss);
        typeRow->addWidget(rbText); typeRow->addWidget(rbVoice); l->addLayout(typeRow);
        connect(rbText, &QPushButton::clicked, [&]{ rbText->setChecked(true); rbVoice->setChecked(false); });
        connect(rbVoice, &QPushButton::clicked, [&]{ rbVoice->setChecked(true); rbText->setChecked(false); });
        l->addStretch();
        auto *btnRow = new QHBoxLayout(); btnRow->addStretch();
        auto *cancel = new QPushButton(QString::fromUtf8("İptal")); cancel->setObjectName("ghost");
        auto *ok = new QPushButton(QString::fromUtf8("Oluştur"));
        btnRow->addWidget(cancel); btnRow->addWidget(ok); l->addLayout(btnRow);
        connect(cancel, &QPushButton::clicked, &d, &QDialog::reject);
        connect(ok, &QPushButton::clicked, &d, &QDialog::accept);
        inp->setFocus();
        if (d.exec() != QDialog::Accepted) return;
        const QString nm = inp->text().trimmed();
        if (nm.isEmpty()) return;
        signalingClient->createChannel(currentServerId, nm, rbVoice->isChecked() ? "voice" : "text");
        showToast(QString::fromUtf8("Kanal oluşturuluyor..."), "info", 1800);
    });
    chanPanelHeader->addWidget(chanPanelTitle);
    chanPanelHeader->addStretch();
    auto *chanInviteBtn = new QToolButton();
    chanInviteBtn->setText(QString::fromUtf8("⇪"));
    chanInviteBtn->setFixedSize(20, 20);
    chanInviteBtn->setCursor(Qt::PointingHandCursor);
    chanInviteBtn->setToolTip(QString::fromUtf8("Davet kodu"));
    chanInviteBtn->setStyleSheet(QString(
        "QToolButton{background:rgba(255,255,255,0.04); color:%1; border:none; border-radius:10px; font-size:12px; font-weight:700;}"
        "QToolButton:hover{background:%2; color:white;}").arg(T::Sub, T::Accent));
    connect(chanInviteBtn, &QToolButton::clicked, this, [this]() {
        if (currentServerId > 0) showInviteCodeDialog(currentServerId);
    });
    chanPanelHeader->addWidget(chanInviteBtn);
    chanPanelHeader->addWidget(chanAddBtn);
    chanPanelLay->addLayout(chanPanelHeader);
    channelListLayout = new QVBoxLayout();
    channelListLayout->setSpacing(1);
    chanPanelLay->addLayout(channelListLayout);
    channelListPanel->hide();
    chatColLayout->addWidget(channelListPanel);

    // Mesaj alanı
    chatScroll = new QScrollArea();
    chatScroll->setWidgetResizable(true);
    chatScroll->setFrameShape(QFrame::NoFrame);
    chatScroll->setStyleSheet("QScrollArea{background:transparent; border:none;}");
    chatScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QWidget *chatContainer = new QWidget();
    chatContainer->setStyleSheet(QString(
        "QWidget{background:transparent;}"
        "QWidget#msgRow{background:transparent; border-radius:6px;}"
        "QWidget#msgRow:hover{background:rgba(255,255,255,0.02);}"
        "QLabel#msgBody{color:%1; font-size:13px;}"
        "QWidget#msgTools{background:%2; border:1px solid %3; border-radius:8px;}"
        "QWidget#msgTools QToolButton{background:transparent; color:%4; border:none; padding:4px 7px; font-size:13px;}"
        "QWidget#msgTools QToolButton:hover{background:rgba(255,255,255,0.06); color:%1; border-radius:5px;}"
        "QLabel#msgImage{background:%2; border:1px solid %3; border-radius:8px; padding:3px;}"
    ).arg(T::Text, T::Card, T::Border, T::Sub));
    chatLayout = new QVBoxLayout(chatContainer);
    chatLayout->setContentsMargins(10, 10, 10, 10);
    chatLayout->setSpacing(8);
    chatLayout->addStretch();
    chatScroll->setWidget(chatContainer);
    chatColLayout->addWidget(chatScroll, 1);

    chatHistory = new QTextEdit();
    chatHistory->hide();

    // Input bar
    QWidget *chatInputWrap = new QWidget();
    chatInputWrap->setFixedHeight(72);
    chatInputWrap->setStyleSheet(QString("background:transparent; border-top:1px solid %1;").arg(T::Border));
    QHBoxLayout *chatInputLayout = new QHBoxLayout(chatInputWrap);
    chatInputLayout->setContentsMargins(14, 12, 14, 12);
    chatInputLayout->setSpacing(8);

    QWidget *inputPill = new QWidget();
    inputPill->setAttribute(Qt::WA_StyledBackground, true);
    inputPill->setStyleSheet(QString("background:%1; border:1px solid %2; border-radius:22px;").arg(T::Input, T::Border));
    QHBoxLayout *inputPillLayout = new QHBoxLayout(inputPill);
    inputPillLayout->setContentsMargins(6, 3, 4, 3);
    inputPillLayout->setSpacing(4);

    auto *attachBtn = new QToolButton();
    attachBtn->setFixedSize(34, 34);
    attachBtn->setCursor(Qt::PointingHandCursor);
    attachBtn->setText(QString::fromUtf8("📎"));
    attachBtn->setToolTip(QString::fromUtf8("Resim ekle"));
    attachBtn->setStyleSheet(QString(
        "QToolButton{background:transparent; border:none; color:%1; font-size:16px; border-radius:17px;}"
        "QToolButton:hover{background:rgba(255,255,255,0.06); color:%2;}").arg(T::Sub, T::Text));
    inputPillLayout->addWidget(attachBtn);
    connect(attachBtn, &QToolButton::clicked, this, &MainWindow::pickAndSendImageAttachment);

    messageInput = new QLineEdit();
    messageInput->setPlaceholderText(QString::fromUtf8("Mesaj gönder..."));
    messageInput->setStyleSheet(QString("QLineEdit{background:transparent; border:none; color:%1; font-size:13px; padding:6px 0;}").arg(T::Text));
    inputPillLayout->addWidget(messageInput, 1);

    QToolButton *sendBtn = new QToolButton();
    sendBtn->setFixedSize(36, 36);
    sendBtn->setCursor(Qt::PointingHandCursor);
    sendBtn->setIconSize(QSize(20, 20));
    sendBtn->setIcon(QIcon(":/icons/volaura-logo.png"));
    sendBtn->setToolTip(QString::fromUtf8("Gönder"));
    sendBtn->setStyleSheet(QString(
        "QToolButton{background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %1,stop:1 %2);"
        " border:none; border-radius:18px;}"
        "QToolButton:hover{background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %3,stop:1 %4);}"
    ).arg(T::Accent2, T::Accent, "#9a5af0", "#818cf8"));
    inputPillLayout->addWidget(sendBtn);
    chatInputLayout->addWidget(inputPill);

    typingLabel = new QLabel();
    typingLabel->setStyleSheet(QString("color:%1; font-size:11px; font-style:italic; padding:1px 16px;").arg(T::Muted));
    typingLabel->setFixedHeight(16);
    typingLabel->setText("");
    chatColLayout->addWidget(typingLabel);
    chatColLayout->addWidget(chatInputWrap);

    connect(sendBtn, &QToolButton::clicked, this, &MainWindow::sendCurrentMessage);
    connect(messageInput, &QLineEdit::returnPressed, this, &MainWindow::sendCurrentMessage);

    typingCooldownTimer = new QTimer(this);
    typingCooldownTimer->setSingleShot(true);
    typingCooldownTimer->setInterval(3000);
    typingExpireTimer = new QTimer(this);
    typingExpireTimer->setInterval(1000);
    connect(typingExpireTimer, &QTimer::timeout, this, &MainWindow::refreshTypingLabel);
    typingExpireTimer->start();
    connect(messageInput, &QLineEdit::textEdited, this, [this](const QString &t) {
        if (!t.trimmed().isEmpty()) onLocalTypingSignal();
    });
    main->addWidget(chatColumn);

    // ===== MEMBERS PANEL =====
    membersPanel = new QWidget();
    membersPanel->setFixedWidth(210);
    membersPanel->setAttribute(Qt::WA_StyledBackground, true);
    membersPanel->setStyleSheet(QString("background:%1; border-right:1px solid %2;").arg(T::Panel, T::Border));
    auto *mPanelWrap = new QVBoxLayout(membersPanel);
    mPanelWrap->setContentsMargins(0, 0, 0, 0);
    mPanelWrap->setSpacing(0);

    QWidget *mPanelHead = new QWidget();
    mPanelHead->setFixedHeight(52);
    mPanelHead->setStyleSheet(QString("background:transparent; border-bottom:1px solid %1;").arg(T::Border));
    auto *mHeadLay = new QHBoxLayout(mPanelHead);
    mHeadLay->setContentsMargins(16, 0, 12, 0);
    membersPanelTitle = new QLabel(QString::fromUtf8("ÜYELER"));
    membersPanelTitle->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700; letter-spacing:1px;").arg(T::Muted));
    mHeadLay->addWidget(membersPanelTitle);
    mHeadLay->addStretch();
    mPanelWrap->addWidget(mPanelHead);

    QScrollArea *mScroll = new QScrollArea();
    mScroll->setWidgetResizable(true);
    mScroll->setFrameShape(QFrame::NoFrame);
    mScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mScroll->setStyleSheet("QScrollArea{background:transparent; border:none;}");
    QWidget *mContent = new QWidget();
    mContent->setStyleSheet("background:transparent;");
    membersPanelLayout = new QVBoxLayout(mContent);
    membersPanelLayout->setContentsMargins(8, 8, 8, 8);
    membersPanelLayout->setSpacing(3);
    membersPanelLayout->addStretch();
    mScroll->setWidget(mContent);
    mPanelWrap->addWidget(mScroll, 1);
    membersPanel->hide();
    main->addWidget(membersPanel);

    // ===== MEDIA COLUMN =====
    mediaColumn = new QWidget();
    mediaColumn->setAttribute(Qt::WA_StyledBackground, true);
    mediaColumn->setStyleSheet("background:transparent;");
    QVBoxLayout *mediaLayout = new QVBoxLayout(mediaColumn);
    mediaLayout->setContentsMargins(0, 0, 0, 0);
    mediaLayout->setSpacing(0);

    QWidget *topBar = new QWidget();
    topBar->setFixedHeight(52);
    topBar->setStyleSheet(QString("background:transparent; border-bottom:1px solid %1;").arg(T::Border));
    QHBoxLayout *topBarLayout = new QHBoxLayout(topBar);
    topBarLayout->setContentsMargins(18, 0, 18, 0);
    topBarLayout->addStretch();

    // Bildirim merkezi (logo'nun yanında çan ikonu + badge)
    if (!notifCenter) notifCenter = new NotificationCenter(topBar, this);
    topBarLayout->addWidget(notifCenter->bellWidget());
    topBarLayout->addSpacing(10);

    QLabel *meBadge = new QLabel();
    meBadge->setFixedSize(34, 34);
    meBadge->setAlignment(Qt::AlignCenter);
    meBadge->setAttribute(Qt::WA_StyledBackground, true);
    meBadge->setStyleSheet("background:transparent; border:none;");
    meBadge->setPixmap(QPixmap(":/icons/volaura-logo.png")
                       .scaled(34, 34, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    topBarLayout->addWidget(meBadge);
    mediaLayout->addWidget(topBar);

    QScrollArea *scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("background:transparent;");
    participantGridWidget = new QWidget();
    participantGridWidget->setStyleSheet("background:transparent;");
    participantGrid = new QGridLayout(participantGridWidget);
    participantGrid->setContentsMargins(20, 20, 20, 20);
    participantGrid->setHorizontalSpacing(14);
    participantGrid->setVerticalSpacing(14);
    scroll->setWidget(participantGridWidget);
    mediaLayout->addWidget(scroll, 1);

    // Control bar
    QWidget *controlWrap = new QWidget();
    mediaControlBar = controlWrap;
    controlWrap->setFixedHeight(88);
    controlWrap->setStyleSheet("background:transparent;");
    QHBoxLayout *controlWrapLayout = new QHBoxLayout(controlWrap);
    controlWrapLayout->setContentsMargins(0, 8, 0, 14);
    controlWrapLayout->addStretch();

    QWidget *controlBar = new QWidget();
    controlBar->setFixedHeight(56);
    controlBar->setStyleSheet(QString("background:%1; border:1px solid %2; border-radius:28px;").arg(T::Card, T::Border));
    QHBoxLayout *ctrlLayout = new QHBoxLayout(controlBar);
    ctrlLayout->setContentsMargins(10, 6, 10, 6);
    ctrlLayout->setSpacing(8);

    auto mkCtrlBtn = [](const QString &g, const QString &bg, const QString &border, const QString &clr) {
        auto *b = new QToolButton();
        b->setText(g);
        b->setFixedSize(42, 42);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(QString(
            "QToolButton{background:%1; border:2px solid %2; border-radius:21px; color:%3; font-size:15px;}"
            "QToolButton:hover{background:%2;}").arg(bg, border, clr));
        return b;
    };

    muteBtn = mkCtrlBtn(QString::fromUtf8("Mic"), "#102519", T::Success, "#eaffea");
    QToolButton *spkBtn = mkCtrlBtn(QString::fromUtf8("Ses"), "#1a1a1d", T::Accent, "#e3eeff");
    QToolButton *shareBtn = mkCtrlBtn(QString::fromUtf8("Ekran"), T::Card, T::Border, T::Sub);
    screenShareBtn = shareBtn;
    cameraBtn = mkCtrlBtn(QString::fromUtf8("Kam"), "#3a1a22", T::Danger, "#ffd5dd");

    QPushButton *callEnd = new QPushButton(QString::fromUtf8("Kapat"));
    callEnd->setFixedHeight(42);
    callEnd->setMinimumWidth(100);
    callEnd->setCursor(Qt::PointingHandCursor);
    callEnd->setStyleSheet(QString(
        "QPushButton{background:%1; color:white; border:none; border-radius:21px; padding:0 20px; font-size:13px; font-weight:700;}"
        "QPushButton:hover{background:%2;}").arg(T::Danger, "#f87171"));

    ctrlLayout->addWidget(muteBtn);
    ctrlLayout->addWidget(spkBtn);
    ctrlLayout->addWidget(shareBtn);
    ctrlLayout->addWidget(cameraBtn);
    ctrlLayout->addSpacing(2);
    ctrlLayout->addWidget(callEnd);

    controlWrapLayout->addWidget(controlBar);
    controlWrapLayout->addStretch();
    mediaLayout->addWidget(controlWrap);

    connect(muteBtn, &QAbstractButton::clicked, this, &MainWindow::toggleMute);
    connect(screenShareBtn, &QAbstractButton::clicked, this, [this]() {
        if (isScreenSharing) toggleScreenShare();
        else showScreenSourcePicker();
    });
    connect(cameraBtn, &QAbstractButton::clicked, this, &MainWindow::toggleCamera);
    connect(callEnd, &QPushButton::clicked, this, [this]() {
        const QString peer = activeCallPeer;
        if (!peer.isEmpty()) signalingClient->declineCall(peer);
        activeCallPeer.clear();
        pendingCallTo.clear();
        updateMediaControlVisibility();
        if (isScreenSharing) toggleScreenShare();
        if (isCameraOn) toggleCamera();
        stopCallVoiceCapture();
        stopAllCallVoicePlayback();
        signalingClient->leaveRoom();
        currentRoomCode.clear();
        if (welcomeModal) welcomeModal->hide();
        hideVoicePanel();
        if (!peer.isEmpty()) {
            selectDm(peer);
        } else {
            chatMode = ChatMode::Idle;
            currentChannelId = 0;
            currentDmPeer.clear();
            if (roomTitleLabel) roomTitleLabel->setText("VoLaura");
            if (dmActionBar) dmActionBar->hide();
            clearChatArea();
            appendSystemMessage(QString::fromUtf8("Bir arkadaşına tıkla veya sunucu seç."));
        }
    });

    remoteScreenLabel = new QLabel();
    remoteScreenLabel->setMinimumHeight(1);
    remoteScreenLabel->hide();
    remoteCameraLabel = new QLabel();
    remoteCameraLabel->setMinimumHeight(1);
    remoteCameraLabel->hide();
    participantList = new QListWidget();
    participantList->hide();

    cameraBtn->setEnabled(camera != nullptr);
    main->addWidget(mediaColumn, 1);

    rebuildParticipantGrid();
    updateMediaControlVisibility();
}

// Aktif çağrı veya voice channel varsa media control bar'ı göster, yoksa gizle.
// Eski global control bar (Mic/Ses/Ekran/Kam/Kapat) — kullanıcı isteği üzerine
// her zaman gizli. Sesli kanal kontrolleri voice panel'de, 1:1 call kontrolleri
// arama ekranında. Bu bar artık kullanılmıyor.
void MainWindow::updateMediaControlVisibility() {
    if (!mediaControlBar) return;
    mediaControlBar->setVisible(false);
}

// LEGACY: Eski "Oda Kur / Odaya Katıl" modal'ı kaldırıldı. Artık kullanıcı
// arkadaş ya da sunucu/kanal seçerek sohbete başlıyor; ayrı oda overlay'i
// gerekmiyor. Geriye dönük çağrı uyumu için fonksiyonlar no-op olarak korundu
// (clearChatArea + idle empty-state ile boş ekranı zaten gösteriyoruz).
void MainWindow::showWelcomeModal() { /* no-op: legacy */ }
void MainWindow::hideWelcomeModal() { /* no-op: legacy */ }

void MainWindow::showCreateRoomDialog() {
    QDialog dialog(this);
    dialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dialog.setAttribute(Qt::WA_TranslucentBackground);
    QVBoxLayout outer(&dialog);
    outer.setContentsMargins(0, 0, 0, 0);

    QFrame panel;
    panel.setStyleSheet("QFrame{background:#1a2238; border:1px solid #445a8f; border-radius:14px;}");
    outer.addWidget(&panel);

    QVBoxLayout layout(&panel);
    QLabel title("Oda Kur");
    title.setStyleSheet("font-size:28px; font-weight:700; color:#fafafa;");
    QLineEdit roomName;
    QLineEdit userName;
    QLineEdit password;
    roomName.setPlaceholderText("Oda adi");
    userName.setPlaceholderText("Kullanici adi");
    password.setPlaceholderText("Sifre (opsiyonel)");
    password.setEchoMode(QLineEdit::Password);
    roomName.setStyleSheet("QLineEdit{background:#11192c; border:1px solid #38508a; border-radius:8px; padding:10px 12px; color:#e6ecff;}");
    userName.setStyleSheet(roomName.styleSheet());
    password.setStyleSheet(roomName.styleSheet());
    layout.addWidget(&title);
    layout.addSpacing(6);
    layout.addWidget(&roomName);
    layout.addWidget(&userName);
    layout.addWidget(&password);
    layout.addSpacing(4);
    QLabel errorLabel;
    errorLabel.setStyleSheet("color:#ff9da8; font-size:12px;");
    errorLabel.hide();
    layout.addWidget(&errorLabel);
    QPushButton submit("Olustur");
    submit.setEnabled(false);
    submit.setStyleSheet(
        "QPushButton{background:#6366f1; border:none; border-radius:8px; padding:10px 12px; color:white;}"
        "QPushButton:hover{background:#4f46e5;}"
        "QPushButton:disabled{background:#1a1a1d; color:#a1a1aa;}"
    );
    layout.addWidget(&submit);
    auto updateState = [&]() {
        const bool valid = !roomName.text().trimmed().isEmpty() && !userName.text().trimmed().isEmpty();
        submit.setEnabled(valid);
        if (valid) {
            errorLabel.hide();
        }
    };
    connect(&roomName, &QLineEdit::textChanged, &dialog, updateState);
    connect(&userName, &QLineEdit::textChanged, &dialog, updateState);
    roomName.setFocus();
    connect(&submit, &QPushButton::clicked, [&]() {
        if (roomName.text().trimmed().isEmpty() || userName.text().trimmed().isEmpty()) {
            errorLabel.setText("Oda adi ve kullanici adi zorunlu.");
            errorLabel.show();
            return;
        }
        currentRoomName = roomName.text().trimmed();
        currentUserName = userName.text().trimmed();
        currentPassword = password.text();
        dialog.accept();
        createRoom();
    });
    dialog.resize(420, 260);
    dialog.exec();
}

void MainWindow::showJoinRoomDialog() {
    QDialog dialog(this);
    dialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dialog.setAttribute(Qt::WA_TranslucentBackground);
    QVBoxLayout outer(&dialog);
    outer.setContentsMargins(0, 0, 0, 0);

    QFrame panel;
    panel.setStyleSheet("QFrame{background:#1a2238; border:1px solid #445a8f; border-radius:14px;}");
    outer.addWidget(&panel);

    QVBoxLayout layout(&panel);
    QLabel title("Odaya Katil");
    title.setStyleSheet("font-size:28px; font-weight:700; color:#fafafa;");
    QLineEdit code;
    QLineEdit userName;
    QLineEdit password;
    code.setPlaceholderText("6 haneli kod");
    userName.setPlaceholderText("Kullanici adi");
    password.setPlaceholderText("Sifre (varsa)");
    password.setEchoMode(QLineEdit::Password);
    code.setMaxLength(6);
    code.setStyleSheet("QLineEdit{background:#11192c; border:1px solid #38508a; border-radius:8px; padding:10px 12px; color:#e6ecff;}");
    userName.setStyleSheet(code.styleSheet());
    password.setStyleSheet(code.styleSheet());
    layout.addWidget(&title);
    layout.addSpacing(6);
    layout.addWidget(&code);
    layout.addWidget(&userName);
    layout.addWidget(&password);
    layout.addSpacing(4);
    QLabel errorLabel;
    errorLabel.setStyleSheet("color:#ff9da8; font-size:12px;");
    errorLabel.hide();
    layout.addWidget(&errorLabel);
    QPushButton submit("Katil");
    submit.setEnabled(false);
    submit.setStyleSheet(
        "QPushButton{background:#6366f1; border:none; border-radius:8px; padding:10px 12px; color:white;}"
        "QPushButton:hover{background:#4f46e5;}"
        "QPushButton:disabled{background:#1a1a1d; color:#a1a1aa;}"
    );
    layout.addWidget(&submit);
    auto updateState = [&]() {
        const bool valid = code.text().trimmed().size() == 6 && !userName.text().trimmed().isEmpty();
        submit.setEnabled(valid);
        if (valid) {
            errorLabel.hide();
        }
    };
    connect(&code, &QLineEdit::textChanged, &dialog, updateState);
    connect(&userName, &QLineEdit::textChanged, &dialog, updateState);
    code.setFocus();
    connect(&submit, &QPushButton::clicked, [&]() {
        if (code.text().trimmed().size() != 6 || userName.text().trimmed().isEmpty()) {
            errorLabel.setText("6 haneli kod ve kullanici adi gerekli.");
            errorLabel.show();
            return;
        }
        currentUserName = userName.text().trimmed();
        currentPassword = password.text();
        dialog.accept();
        joinRoom(code.text().trimmed());
    });
    dialog.resize(420, 260);
    dialog.exec();
}

void MainWindow::createRoom() {
    hideWelcomeModal();
    signalingClient->createRoom(currentRoomName, currentUserName, currentPassword);
}

void MainWindow::joinRoom(const QString &code) {
    hideWelcomeModal();
    signalingClient->joinRoom(code, currentUserName, currentPassword);
}

// ===================== DM resim eki =====================
//
// İçerik formatı: "[[img:<base64-jpeg>]]\n<isteğe bağlı text>"
// Hem DM (E2E ciphertext'in içine girer) hem kanal mesajları için aynı şema.
// Render: appendRichMessage() içinde başında [[img: olan içerik QPixmap'e dönüştürülür.
void MainWindow::pickAndSendImageAttachment() {
    if (chatMode == ChatMode::Idle) {
        showToast(QString::fromUtf8("Önce bir DM ya da kanal seç"), "warn", 2200);
        return;
    }
    const QString file = QFileDialog::getOpenFileName(this,
        QString::fromUtf8("Resim seç"),
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
        QString::fromUtf8("Resimler (*.png *.jpg *.jpeg *.bmp *.gif *.webp)"));
    if (file.isEmpty()) return;

    QFileInfo fi(file);
    if (fi.size() > 4 * 1024 * 1024) {
        showToast(QString::fromUtf8("Dosya 4 MB'dan büyük — daha küçük bir resim seç."), "warn");
        return;
    }
    QImage img(file);
    if (img.isNull()) {
        showToast(QString::fromUtf8("Resim okunamadı."), "error");
        return;
    }
    // Çok büyükse 1280px max boyuta küçült (chat kullanımı için yeter)
    const int maxSide = 1280;
    if (img.width() > maxSide || img.height() > maxSide) {
        img = img.scaled(maxSide, maxSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    // JPEG sıkıştır — kalite 75 (~ %85 boyut tasarrufu)
    QByteArray jpeg;
    QBuffer buf(&jpeg);
    buf.open(QIODevice::WriteOnly);
    if (!img.save(&buf, "JPG", 75)) {
        showToast(QString::fromUtf8("Resim sıkıştırılamadı."), "error");
        return;
    }
    if (jpeg.size() > 800 * 1024) {
        // Hala büyükse daha agresif kaliteye düşür
        jpeg.clear(); buf.close(); buf.open(QIODevice::WriteOnly);
        if (!img.save(&buf, "JPG", 55) || jpeg.size() > 1024 * 1024) {
            showToast(QString::fromUtf8("Resim çok büyük — daha küçük bir resim dene."), "warn");
            return;
        }
    }
    const QString b64 = QString::fromLatin1(jpeg.toBase64());
    QString caption = messageInput->text().trimmed();
    QString payload = QString("[[img:%1]]").arg(b64);
    if (!caption.isEmpty()) payload += "\n" + caption;

    switch (chatMode) {
        case ChatMode::Channel:
            if (currentChannelId > 0)
                signalingClient->sendChannelMessage(currentChannelId, payload);
            break;
        case ChatMode::Dm:
            if (!currentDmPeer.isEmpty()) {
                if (E2E::isAvailable() && e2ePeerKeyCache.contains(currentDmPeer)) {
                    const QString peerPub = e2ePeerKeyCache.value(currentDmPeer);
                    QString ct, nonce;
                    if (!peerPub.isEmpty() &&
                        E2E::encryptForPeer(peerPub, payload, &ct, &nonce)) {
                        signalingClient->sendDmEncrypted(currentDmPeer, ct, nonce,
                                                         E2E::publicKeyB64());
                        break;
                    }
                }
                signalingClient->sendDm(currentDmPeer, payload);
            }
            break;
        default: break;
    }
    messageInput->clear();
}

void MainWindow::sendCurrentMessage() {
    const QString text = messageInput->text().trimmed();
    if (text.isEmpty()) return;
    switch (chatMode) {
        case ChatMode::Channel:
            if (currentChannelId > 0) signalingClient->sendChannelMessage(currentChannelId, text);
            break;
        case ChatMode::Dm:
            if (!currentDmPeer.isEmpty()) {
                // E2E gönderim akışı:
                //  1) libsodium yoksa → plaintext
                //  2) peer pubkey cache'te + dolu ise → encrypt + sendDmEncrypted
                //  3) peer pubkey cache'te ama boş (peer E2E desteklemiyor) → plaintext
                //  4) cache'te yok → key iste, mesajı kuyruğa al, cevap gelince gönder
                if (!E2E::isAvailable()) {
                    signalingClient->sendDm(currentDmPeer, text);
                } else if (e2ePeerKeyCache.contains(currentDmPeer)) {
                    const QString peerPub = e2ePeerKeyCache.value(currentDmPeer);
                    QString ct, nonce;
                    if (!peerPub.isEmpty() &&
                        E2E::encryptForPeer(peerPub, text, &ct, &nonce)) {
                        signalingClient->sendDmEncrypted(currentDmPeer, ct, nonce,
                                                         E2E::publicKeyB64());
                    } else {
                        signalingClient->sendDm(currentDmPeer, text);
                    }
                } else {
                    e2ePendingPlaintextByPeer[currentDmPeer].append(text);
                    e2eRequestPeerKey(currentDmPeer);
                }
            }
            break;
        case ChatMode::Room:
            appendChatBubble(currentUserName.isEmpty() ? QString("Sen") : currentUserName,
                             createTimeStamp(), text);
            signalingClient->sendChatMessage(currentUserName, text);
            break;
        case ChatMode::Idle:
            showToast(QString::fromUtf8("Önce bir sunucu kanalı ya da DM seç"), "warn", 2200);
            return;
    }
    messageInput->clear();
}

void MainWindow::toggleMute() {
    isMuted = !isMuted;
    muteBtn->setStyleSheet(QString(
        "QToolButton{background:%1; border:2px solid %2; border-radius:23px; color:#eaffea; font-size:16px;}"
        "QToolButton:hover{background:%2;}"
    ).arg(isMuted ? "#3a1a22" : "#102519", isMuted ? T::Danger : T::Success));
    signalingClient->sendMediaState(isMuted, isScreenSharing);
    if (isMuted) {
        voicePingTimer->stop();
    } else if (!voicePingTimer->isActive()) {
        voicePingTimer->start();
    }
}

void MainWindow::toggleScreenShare() {
    isScreenSharing = !isScreenSharing;
    screenShareBtn->setStyleSheet(QString(
        "QToolButton{background:%1; border:2px solid %2; border-radius:23px; color:#fafafa; font-size:16px;}"
        "QToolButton:hover{background:%2;}"
    ).arg(isScreenSharing ? "#1a1a1d" : T::Card, isScreenSharing ? T::Accent : T::Border));
    signalingClient->sendMediaState(isMuted, isScreenSharing);
    if (isScreenSharing) {
        screenShareTimer->start();
        captureAndSendScreenFrame();
    } else {
        screenShareTimer->stop();
    }
}

void MainWindow::toggleCamera() {
    if (!camera) {
        showToast(QString::fromUtf8("Kamera bulunamadı"), "error", 2500);
        return;
    }
    isCameraOn = !isCameraOn;
    cameraBtn->setStyleSheet(QString(
        "QToolButton{background:%1; border:2px solid %2; border-radius:23px; color:#ffd5dd; font-size:16px;}"
        "QToolButton:hover{background:%2;}"
    ).arg(isCameraOn ? "#102519" : "#3a1a22", isCameraOn ? T::Success : T::Danger));
    if (isCameraOn) {
        camera->start();
        cameraShareTimer->start();
        sendCameraFrame();
    } else {
        cameraShareTimer->stop();
        camera->stop();
    }
}

void MainWindow::onRoomCreated(const QString &roomCode, const QString &roomName) {
    currentRoomCode = roomCode;
    currentRoomName = roomName;
    showRoomInterface(roomCode);
    // If this room was created as part of a call, send the call_friend signal now
    if (!pendingCallTo.isEmpty()) {
        signalingClient->callFriend(pendingCallTo, roomCode);
    }
    // 1:1 call: mikrofon yakalamayı başlat
    if (!pendingCallTo.isEmpty() || !activeCallPeer.isEmpty()) {
        startCallVoiceCapture();
    }
}

void MainWindow::onRoomJoined(const QString &roomCode, const QString &roomName) {
    currentRoomCode = roomCode;
    currentRoomName = roomName;
    showRoomInterface(roomCode);
    // 1:1 call: mikrofon yakalamayı başlat
    if (!activeCallPeer.isEmpty() || !pendingCallTo.isEmpty()) {
        startCallVoiceCapture();
    }
}

void MainWindow::onError(const QString &error) {
    if (error.contains("Bilinmeyen mesaj tipi", Qt::CaseInsensitive)) {
        return;
    }
    // Sadece toast — chat history'i kirletme
    showToast(error, "error", 3500);
}

void MainWindow::onChatMessageReceived(const QString &userName, const QString &message, const QString &timestamp) {
    if (userName == currentUserName) {
        return;
    }
    const QString ts = timestamp.isEmpty() ? createTimeStamp() : timestamp;
    appendChatBubble(userName, ts, message);
}

void MainWindow::onParticipantJoined(const QString &participantId, const QString &userName) {
    updateParticipantLabel(participantId, userName);
}

void MainWindow::onParticipantLeft(const QString &participantId) {
    removeParticipantLabel(participantId);
}

void MainWindow::onParticipantMediaStateChanged(const QString &participantId, bool audioMuted, bool screenSharing) {
    const QString name = participantNames.value(participantId, participantId);
    updateParticipantLabel(participantId, name, audioMuted, screenSharing);
}

void MainWindow::onParticipantsListed(const QJsonArray &participants) {
    for (const QJsonValue &value : participants) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject obj = value.toObject();
        updateParticipantLabel(obj.value("participantId").toString(), obj.value("userName").toString());
    }
}

void MainWindow::onMediaChunkReceived(const QString &participantId, const QString &mediaKind, const QByteArray &payload) {
    if (mediaKind == "audio_pcm") {
        if (currentRoomCode.isEmpty() || payload.isEmpty()) return;
        QAudioSink *sink = getOrCreateCallVoiceSinkFor(participantId);
        QIODevice *io = callVoiceSinkIO.value(participantId, nullptr);
        if (sink && io) io->write(payload);
        // Remote speaker voice activity
        if (computeVoiceActive(payload)) {
            const QString uname = participantNames.value(participantId);
            if (!uname.isEmpty()) setUserSpeaking(uname, true);
        }
        return;
    }
    QImage image;
    if ((mediaKind == "screen_frame" || mediaKind == "camera_frame") && image.loadFromData(payload, "JPG")) {
        QLabel *target = mediaKind == "screen_frame" ? remoteScreenLabel : remoteCameraLabel;
        target->setPixmap(QPixmap::fromImage(image).scaled(target->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

void MainWindow::captureAndSendScreenFrame() {
    if (!isScreenSharing) {
        return;
    }
    const QList<QScreen*> screens = QGuiApplication::screens();
    QScreen *screen = nullptr;
    if (settingsScreenIndex >= 0 && settingsScreenIndex < screens.size()) {
        screen = screens.at(settingsScreenIndex);
    }
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    if (!screen) {
        return;
    }
    const QPixmap frame = screen->grabWindow(0);
    if (frame.isNull()) {
        return;
    }
    const int targetH = qBound(240, settingsScreenHeight, 1440);
    const int targetW = static_cast<int>(targetH * 16.0 / 9.0);
    QImage scaled = frame.toImage().scaled(targetW, targetH, Qt::KeepAspectRatio, Qt::FastTransformation);
    QByteArray encoded;
    QBuffer buffer(&encoded);
    buffer.open(QIODevice::WriteOnly);
    scaled.save(&buffer, "JPG", qBound(10, settingsJpegQuality, 95));
    signalingClient->sendMediaChunk("screen_frame", encoded);
}

void MainWindow::sendVoiceActivityPing() {
    if (!isMuted) {
        signalingClient->sendMediaChunk("audio_ping", QByteArray("1"));
    }
}

void MainWindow::sendCameraFrame() {
    if (isCameraOn && !lastCameraFramePayload.isEmpty()) {
        signalingClient->sendMediaChunk("camera_frame", lastCameraFramePayload);
    }
}

void MainWindow::onCameraFrameChanged(const QVideoFrame &frame) {
    if (!isCameraOn || !frame.isValid()) {
        return;
    }
    QImage image = frame.toImage();
    if (image.isNull()) {
        return;
    }
    QImage scaled = image.scaled(640, 360, Qt::KeepAspectRatio, Qt::FastTransformation);
    QByteArray encoded;
    QBuffer buffer(&encoded);
    buffer.open(QIODevice::WriteOnly);
    scaled.save(&buffer, "JPG", 70);
    lastCameraFramePayload = encoded;
}

void MainWindow::showRoomInterface(const QString &roomCode) {
    // Sadece aktif bir cagri/olusturma niyetiyle odaya girildiyse tam gorunume ge
    // Yoksa server reconnect kaynakli otomatik oda girisini gormezden gel.
    if (activeCallPeer.isEmpty() && pendingCallTo.isEmpty()) {
        // Idle'da: oda UI'ini acma, sadece state'i kaydet
        return;
    }
    roomTitleLabel->setText(QString::fromUtf8("Arama"));
    connectionStatusLabel->setText("Kod: " + roomCode);
    appendSystemMessage("Baglandin. Davet kodu: " + roomCode);
    participantList->clear();
    participantList->addItem(currentUserName + " (Sen) [self]");
    const QStringList seedKeys = {"__seed_1","__seed_2","__seed_3","__seed_4","__seed_5","__seed_6"};
    for (const QString &k : seedKeys) participantNames.remove(k);
    updateParticipantLabel("self", currentUserName + " (Sen)", isMuted, isScreenSharing);
}

void MainWindow::appendSystemMessage(const QString &message) {
    if (!chatLayout) return;
    QLabel *sys = new QLabel(QString("[Sistem %1] %2").arg(createTimeStamp(), message));
    sys->setWordWrap(true);
    sys->setStyleSheet(QString("color:%1; background:transparent; font-size:12px; padding:2px 4px;").arg(T::Accent));
    const int insertAt = chatLayout->count() - 1;
    chatLayout->insertWidget(insertAt < 0 ? 0 : insertAt, sys);
}

QString MainWindow::createTimeStamp() const {
    return QDateTime::currentDateTime().toString("HH:mm:ss");
}

void MainWindow::updateParticipantLabel(const QString &participantId, const QString &userName, bool muted, bool sharing) {
    if (participantId.isEmpty()) {
        return;
    }
    participantNames[participantId] = userName;
    participantMuted[participantId] = muted;
    participantSharing[participantId] = sharing;
    rebuildParticipantGrid();
}

void MainWindow::removeParticipantLabel(const QString &participantId) {
    participantNames.remove(participantId);
    participantMuted.remove(participantId);
    participantSharing.remove(participantId);
    rebuildParticipantGrid();
}

QString MainWindow::avatarInitials(const QString &name) const {
    QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) return QString("?");
    const QStringList parts = trimmed.split(' ', Qt::SkipEmptyParts);
    QString out;
    for (int i = 0; i < parts.size() && out.size() < 2; ++i) {
        out += parts[i].left(1).toUpper();
    }
    return out.isEmpty() ? trimmed.left(1).toUpper() : out;
}

QString MainWindow::avatarColor(const QString &name) const {
    static const QStringList palette = {
        "#4a6cf7", "#f7a84a", "#22c55e", "#ef4444", "#9b59f7", "#17c3b2", "#f77e4a", "#6c7ae0"
    };
    uint h = 0;
    for (const QChar &c : name) h = h * 131 + c.unicode();
    return palette[h % palette.size()];
}

QWidget *MainWindow::buildParticipantCard(const QString &name, const QString &status, bool speaking, bool muted, bool sharing) {
    QFrame *card = new QFrame();
    card->setMinimumSize(240, 200);
    card->setAttribute(Qt::WA_StyledBackground, true);
    const QString borderColor = speaking ? "#6366f1" : "rgba(255,255,255,0.06)";
    const QString bg = speaking ? "#1a1a1d" : T::Card;
    card->setObjectName("pcard");
    card->setStyleSheet(QString(
        "QFrame#pcard{background:%1; border:1px solid %2; border-radius:14px;}"
    ).arg(bg, borderColor));
    if (speaking) {
        auto *glow = new QGraphicsDropShadowEffect(card);
        glow->setBlurRadius(24);
        glow->setOffset(0, 0);
        glow->setColor(QColor(77, 140, 255, 160));
        card->setGraphicsEffect(glow);
    }

    QVBoxLayout *lay = new QVBoxLayout(card);
    lay->setContentsMargins(16, 18, 16, 16);
    lay->setSpacing(8);
    lay->addStretch();

    // Avatar with status dot
    QWidget *avWrap = new QWidget();
    avWrap->setFixedSize(86, 86);
    avWrap->setStyleSheet("background:transparent;");
    QLabel *avatar = new QLabel(avWrap);
    avatar->setGeometry(0, 0, 86, 86);
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setAttribute(Qt::WA_StyledBackground, true);
    avatar->setText(avatarInitials(name));
    avatar->setStyleSheet(QString(
        "background:%1; color:white; border-radius:43px; font-size:28px; font-weight:800; border:3px solid rgba(255,255,255,30);"
    ).arg(avatarColor(name)));
    QLabel *dot = new QLabel(avWrap);
    dot->setFixedSize(16, 16);
    dot->move(66, 66);
    dot->setStyleSheet(QString("background:%1; border:3px solid %2; border-radius:8px;").arg(T::Success, T::Card));
    lay->addWidget(avWrap, 0, Qt::AlignHCenter);

    QLabel *nameLbl = new QLabel(name);
    nameLbl->setAlignment(Qt::AlignCenter);
    nameLbl->setStyleSheet(QString("color:%1; font-size:15px; font-weight:700; background:transparent; border:none;").arg(T::Text));
    lay->addWidget(nameLbl);

    QLabel *statusLbl = new QLabel(status);
    statusLbl->setAlignment(Qt::AlignCenter);
    statusLbl->setStyleSheet(QString("color:%1; font-size:12px; background:transparent; border:none;").arg(T::Sub));
    lay->addWidget(statusLbl);

    lay->addStretch();

    // Bottom-right badge (mic/speaker)
    QString badgeGlyph = muted ? QString::fromUtf8("\U0001F507") : QString::fromUtf8("\U0001F3A4");
    QString badgeColor = muted ? T::Danger : T::Success;
    if (sharing) { badgeGlyph = QString::fromUtf8("\U0001F5A5"); badgeColor = T::Accent; }
    QWidget *badgeWrap = new QWidget(card);
    badgeWrap->setStyleSheet("background:transparent;");
    badgeWrap->setFixedSize(34, 34);
    QLabel *badge = new QLabel(badgeWrap);
    badge->setText(badgeGlyph);
    badge->setAlignment(Qt::AlignCenter);
    badge->setGeometry(3, 3, 28, 28);
    badge->setStyleSheet(QString("background:rgba(10,13,20,220); color:%1; border-radius:14px; font-size:13px; border:none;").arg(badgeColor));
    badgeWrap->move(card->width() - 42, card->height() - 42);
    class BadgeMover : public QObject {
    public:
        QWidget *badgeWrap;
        BadgeMover(QWidget *bw, QObject *parent) : QObject(parent), badgeWrap(bw) {}
        bool eventFilter(QObject *obj, QEvent *ev) override {
            if (ev->type() == QEvent::Resize) {
                QWidget *w = qobject_cast<QWidget*>(obj);
                if (w && badgeWrap) badgeWrap->move(w->width() - 42, w->height() - 42);
            }
            return false;
        }
    };
    card->installEventFilter(new BadgeMover(badgeWrap, card));

    return card;
}

void MainWindow::rebuildParticipantGrid() {
    if (!participantGrid) return;
    // Clear existing
    QLayoutItem *child;
    while ((child = participantGrid->takeAt(0)) != nullptr) {
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }
    participantCards.clear();

    const QStringList ids = participantNames.keys();
    if (ids.isEmpty()) {
        QWidget *emptyWrap = new QWidget();
        emptyWrap->setStyleSheet("background:transparent;");
        auto *lay = new QVBoxLayout(emptyWrap);
        lay->setAlignment(Qt::AlignCenter);
        lay->setSpacing(18);

        // Logo wrap — etrafında soft mor halo
        QWidget *logoBox = new QWidget();
        logoBox->setFixedSize(180, 180);
        logoBox->setAttribute(Qt::WA_StyledBackground, true);
        logoBox->setStyleSheet(
            "background:qradialgradient(cx:0.5,cy:0.5,radius:0.5,"
            " stop:0 rgba(123,63,228,0.28),"
            " stop:0.6 rgba(43,109,245,0.10),"
            " stop:1 transparent); border-radius:90px; border:none;");
        auto *lboxLay = new QVBoxLayout(logoBox);
        lboxLay->setContentsMargins(0,0,0,0);
        QLabel *logo = new QLabel();
        logo->setAlignment(Qt::AlignCenter);
        logo->setStyleSheet("background:transparent; border:none;");
        logo->setPixmap(QPixmap(":/icons/volaura-logo.png")
                        .scaled(140, 140, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        auto *lglow = new QGraphicsDropShadowEffect(logo);
        lglow->setBlurRadius(40); lglow->setOffset(0, 0);
        lglow->setColor(QColor(123, 63, 228, 180));
        logo->setGraphicsEffect(lglow);
        lboxLay->addWidget(logo, 0, Qt::AlignCenter);
        lay->addWidget(logoBox, 0, Qt::AlignHCenter);

        QLabel *title = new QLabel(QString::fromUtf8("VoLaura'ya hoş geldin"));
        title->setAlignment(Qt::AlignCenter);
        title->setStyleSheet("color:#fafafa; font-size:28px; font-weight:800;"
                             " background:transparent; letter-spacing:0.4px;");
        lay->addWidget(title);

        QLabel *subtitle = new QLabel(QString::fromUtf8("Sesin, görüntün, sohbetin — hepsi bir arada"));
        subtitle->setAlignment(Qt::AlignCenter);
        subtitle->setStyleSheet("color:#9aa6c4; font-size:13.5px; background:transparent;");
        lay->addWidget(subtitle);

        lay->addSpacing(10);

        // Tıklanabilir hızlı eylem chip'leri — relevant dialog'ları açar
        auto makeChip = [this](const QString &emoji, const QString &text,
                               std::function<void()> onClick) {
            auto *chip = new QPushButton();
            chip->setCursor(Qt::PointingHandCursor);
            chip->setText(emoji.isEmpty() ? text : QString::fromUtf8("  %1   %2").arg(emoji, text));
            chip->setStyleSheet(
                "QPushButton{background:rgba(255,255,255,0.04);"
                " border:1px solid rgba(255,255,255,0.08);"
                " color:#fafafa; border-radius:18px; padding:10px 22px;"
                " font-size:13px; font-weight:600; text-align:center;}"
                "QPushButton:hover{background:rgba(123,63,228,0.18);"
                " border:1px solid rgba(180,140,255,0.35); color:#fafafa;}");
            connect(chip, &QPushButton::clicked, this, [onClick]() { if (onClick) onClick(); });
            return chip;
        };
        auto *chipsRow = new QHBoxLayout();
        chipsRow->setSpacing(10);
        chipsRow->addStretch();
        chipsRow->addWidget(makeChip(QString(),
            QString::fromUtf8("Arkadaş ekle"),
            [this]() { showAddFriendDialog(); }));
        chipsRow->addWidget(makeChip(QString(),
            QString::fromUtf8("İstekler"),
            [this]() { showRequestsDialog(); }));
        chipsRow->addWidget(makeChip(QString(),
            QString::fromUtf8("Ayarlar"),
            [this]() { showSettingsDialog(); }));
        chipsRow->addStretch();
        lay->addLayout(chipsRow);

        lay->addSpacing(28);

        // Feature kartları — boşluğu doldurup sık kullanılan eylemleri öne çıkarır
        auto makeFeatureCard = [this](const QString &emoji, const QString &title,
                                       const QString &desc, std::function<void()> onClick) {
            auto *card = new QPushButton();
            card->setCursor(Qt::PointingHandCursor);
            card->setFixedSize(220, 120);
            card->setStyleSheet(
                "QPushButton{background:rgba(255,255,255,0.03);"
                " border:1px solid rgba(255,255,255,0.06);"
                " border-radius:14px; padding:14px; text-align:left;}"
                "QPushButton:hover{background:rgba(123,63,228,0.14);"
                " border:1px solid rgba(180,140,255,0.4);}");
            auto *cl = new QVBoxLayout(card);
            cl->setSpacing(6);
            cl->setContentsMargins(14, 12, 14, 12);
            if (!emoji.isEmpty()) {
                auto *e = new QLabel(emoji);
                e->setStyleSheet("font-size:24px; background:transparent; border:none;");
                cl->addWidget(e);
            }
            auto *t = new QLabel(title);
            t->setStyleSheet("color:#fafafa; font-size:14px; font-weight:700;"
                             " background:transparent; border:none;");
            cl->addWidget(t);
            auto *desc_l = new QLabel(desc);
            desc_l->setWordWrap(true);
            desc_l->setStyleSheet("color:#8b97b4; font-size:11.5px;"
                                  " background:transparent; border:none;");
            cl->addWidget(desc_l);
            cl->addStretch();
            connect(card, &QPushButton::clicked, this, [onClick]() { if (onClick) onClick(); });
            return card;
        };
        auto *featRow = new QHBoxLayout();
        featRow->setSpacing(14);
        featRow->addStretch();
        featRow->addWidget(makeFeatureCard(QString(),
            QString::fromUtf8("Sunucu Oluştur / Katıl"),
            QString::fromUtf8("Yeni bir sunucu kur veya davet kodu ile katıl."),
            [this]() { showServerSetupDialog(); }));
        featRow->addWidget(makeFeatureCard(QString(),
            QString::fromUtf8("Hesap Güvenliği"),
            QString::fromUtf8("İki adımlı doğrulama, telefon ve e-posta kodu ayarları."),
            [this]() { showSecurityDialog(); }));
        featRow->addWidget(makeFeatureCard(QString(),
            QString::fromUtf8("Mesajlarım"),
            QString::fromUtf8("Arkadaşlarınla doğrudan mesajlaşma — çevrimdışıyken bile."),
            [this]() { showAddFriendDialog(); }));
        featRow->addStretch();
        lay->addLayout(featRow);

        lay->addSpacing(18);

        // Alt ipucu satırı
        auto *tip = new QLabel(QString::fromUtf8(
            "💡  Avatara <b>sol tık</b>: çevrimiçi arkadaşı ara · "
            "çevrimdışıysa DM aç &nbsp;|&nbsp; <b>sağ tık</b>: menü"));
        tip->setTextFormat(Qt::RichText);
        tip->setAlignment(Qt::AlignCenter);
        tip->setStyleSheet("color:#7d89a8; font-size:11.5px;"
                           " background:transparent; border:none;");
        lay->addWidget(tip);

        participantGrid->addWidget(emptyWrap, 0, 0, 1, 3, Qt::AlignCenter);
        return;
    }
    const int columns = 3;
    int row = 0, col = 0;
    int idx = 0;
    for (const QString &id : ids) {
        const QString name = participantNames.value(id);
        const bool muted = participantMuted.value(id, false);
        const bool sharing = participantSharing.value(id, false);
        const bool speaking = (idx == 0) && !muted;
        QString status;
        if (speaking) status = QString::fromUtf8("Konuşuyor...");
        else if (sharing) status = QString::fromUtf8("Ekran paylaşıyor");
        else status = createTimeStamp().left(5);
        QWidget *card = buildParticipantCard(name, status, speaking, muted, sharing);
        participantCards[id] = card;
        participantGrid->addWidget(card, row, col);
        col++;
        if (col >= columns) { col = 0; row++; }
        idx++;
    }
    // Fill remaining columns to keep equal widths
    for (int c = 0; c < columns; ++c) participantGrid->setColumnStretch(c, 1);
    participantGrid->setRowStretch(row + 1, 1);
}

void MainWindow::appendChatBubble(const QString &userName, const QString &timestamp, const QString &message) {
    if (!chatLayout) return;

    QWidget *row = new QWidget();
    row->setObjectName("msgRow");  // chatContainer QSS'inden hover/bg gelir
    row->setAttribute(Qt::WA_StyledBackground, true);
    QHBoxLayout *rowLay = new QHBoxLayout(row);
    rowLay->setContentsMargins(6, 4, 6, 4);
    rowLay->setSpacing(10);
    rowLay->setAlignment(Qt::AlignTop);

    // Avatar — drop shadow yok (perf), inline minimal stil.
    QLabel *avatar = new QLabel(avatarInitials(userName));
    avatar->setFixedSize(34, 34);
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setAttribute(Qt::WA_StyledBackground, true);
    avatar->setStyleSheet(QString(
        "background:%1; color:#fff; border-radius:17px; font-weight:700; font-size:13px;"
    ).arg(avatarColor(userName)));
    rowLay->addWidget(avatar, 0, Qt::AlignTop);

    // Text column — stil container'dan, setStyleSheet yok.
    QWidget *textCol = new QWidget();
    QVBoxLayout *textLay = new QVBoxLayout(textCol);
    textLay->setContentsMargins(0, 0, 0, 0);
    textLay->setSpacing(2);

    QLabel *header = new QLabel(QString("<span style='color:#fafafa;font-weight:700;'>%1</span>  "
                                        "<span style='color:#a1a1aa;font-size:11px;'>%2</span>")
                                .arg(userName.toHtmlEscaped(), timestamp.toHtmlEscaped()));
    header->setTextFormat(Qt::RichText);
    textLay->addWidget(header);

    QLabel *body = new QLabel(message);
    body->setObjectName("msgBody");  // stil container'dan
    body->setWordWrap(true);
    textLay->addWidget(body);

    rowLay->addWidget(textCol, 1);

    // Insert before the stretch (last item)
    const int insertAt = chatLayout->count() - 1;
    chatLayout->insertWidget(insertAt < 0 ? 0 : insertAt, row);

    // Auto-scroll to bottom
    if (chatScroll && chatScroll->verticalScrollBar()) {
        QTimer::singleShot(0, chatScroll, [this]() {
            chatScroll->verticalScrollBar()->setValue(chatScroll->verticalScrollBar()->maximum());
        });
    }
}

// ===================== SETTINGS =====================

void MainWindow::loadSettings() {
    QSettings s("VoLaura", "VoLaura");
    settingsMicId       = s.value("audio/mic").toString();
    settingsSpeakerId   = s.value("audio/speaker").toString();
    settingsCameraId    = s.value("video/camera").toString();
    settingsScreenIndex = s.value("screen/index", -1).toInt();
    settingsScreenFps   = qBound(1, s.value("screen/fps", 30).toInt(), 120);
    settingsScreenHeight= qBound(240, s.value("screen/height", 1080).toInt(), 2160);
    settingsJpegQuality = qBound(10, s.value("screen/quality", 85).toInt(), 98);
    settingsAec         = s.value("audio/aec", true).toBool();
    settingsAgc         = s.value("audio/agc", true).toBool();
    settingsNs          = s.value("audio/ns", true).toBool();
    settingsMicGain     = qBound(0.3f, float(s.value("audio/micGain", 1.0).toDouble()), 3.0f);
    // Yüksek varsayılan: 96 kbps speech-HD; max 256 kbps (FullBand stereo benzeri kalite)
    settingsAudioBitrate= qBound(16000, s.value("audio/bitrate", 96000).toInt(), 256000);
}

void MainWindow::saveSettings() {
    QSettings s("VoLaura", "VoLaura");
    s.setValue("audio/mic", settingsMicId);
    s.setValue("audio/speaker", settingsSpeakerId);
    s.setValue("video/camera", settingsCameraId);
    s.setValue("screen/index", settingsScreenIndex);
    s.setValue("screen/fps", settingsScreenFps);
    s.setValue("screen/height", settingsScreenHeight);
    s.setValue("screen/quality", settingsJpegQuality);
    s.setValue("audio/aec", settingsAec);
    s.setValue("audio/agc", settingsAgc);
    s.setValue("audio/ns", settingsNs);
    s.setValue("audio/micGain", double(settingsMicGain));
    s.setValue("audio/bitrate", settingsAudioBitrate);
}

// Ayarlardaki ses işleme tercihlerini aktif processor'lara uygula.
void MainWindow::applyAudioSettings() {
    voiceAudioProcessor.setAecEnabled(settingsAec);
    voiceAudioProcessor.setAgcEnabled(settingsAgc);
    voiceNoiseSuppressor.setEnabled(settingsNs);
    if (voiceEncoder.isReady()) {
        // Bitrate runtime değiştirme: encoder reinit gerekmez, libopus ctl yeterli ama
        // wrapper'da yok — yine de yeni init yaparsak kullanıcı kanaldan çıkıp girince
        // yeni bitrate uygulanır. Şimdilik flag yeter.
    }
}

void MainWindow::applyScreenShareInterval() {
    const int fps = qBound(1, settingsScreenFps, 120);
    const int interval = qMax(8, 1000 / fps);  // 8ms = 125 FPS tavan
    if (screenShareTimer) screenShareTimer->setInterval(interval);
    if (voiceScreenShareTimer) voiceScreenShareTimer->setInterval(interval);
}

void MainWindow::applyCameraDevice() {
    const bool wasRunning = (camera && isCameraOn);
    if (camera) {
        camera->stop();
        camera->deleteLater();
        camera = nullptr;
    }

    QCameraDevice chosen;
    const QList<QCameraDevice> devices = QMediaDevices::videoInputs();
    if (!settingsCameraId.isEmpty()) {
        for (const QCameraDevice &d : devices) {
            if (QString::fromUtf8(d.id()) == settingsCameraId) { chosen = d; break; }
        }
    }
    if (chosen.isNull()) chosen = QMediaDevices::defaultVideoInput();

    if (!chosen.isNull()) {
        camera = new QCamera(chosen, this);
        if (!cameraVideoSink) {
            cameraVideoSink = new QVideoSink(this);
            connect(cameraVideoSink, &QVideoSink::videoFrameChanged, this, &MainWindow::onCameraFrameChanged);
        }
        if (!cameraCaptureSession) {
            cameraCaptureSession = new QMediaCaptureSession(this);
            cameraCaptureSession->setVideoSink(cameraVideoSink);
        }
        cameraCaptureSession->setCamera(camera);
        if (wasRunning) camera->start();
    }
    if (cameraBtn) cameraBtn->setEnabled(camera != nullptr);
}

void MainWindow::showSettingsDialog() {
    QDialog dialog(this);
    dialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dialog.setAttribute(Qt::WA_TranslucentBackground);
    dialog.setMinimumWidth(520);

    auto *outer = new QVBoxLayout(&dialog);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *card = new QWidget(&dialog);
    card->setObjectName("card");
    card->setStyleSheet((QString(kPrettyDialogQss) +
        "QLabel{color:%1; font-size:13px;}"
        "QComboBox, QSpinBox{background:%2; color:%3;"
        " border:1px solid %4; border-radius:8px; padding:6px 10px; min-height:22px;}"
        "QComboBox QAbstractItemView{background:%2; color:%3;"
        " selection-background-color:%5;}"
        "QSlider::groove:horizontal{height:4px; background:%4; border-radius:2px;}"
        "QSlider::handle:horizontal{background:%5; width:14px; height:14px;"
        " border-radius:7px; margin:-5px 0;}"
        "QSlider::sub-page:horizontal{background:%5; border-radius:2px;}"
        "QCheckBox{color:%1;}"
        "QPushButton#closeX{background:transparent; color:%1; border:none;"
        " font-size:18px; font-weight:700; min-width:28px; max-width:28px;"
        " min-height:28px; max-height:28px; border-radius:14px;}"
        "QPushButton#closeX:hover{background:rgba(255,255,255,0.08); color:%3;}"
    ).arg(T::Sub, T::Card, T::Text, T::Border, T::Accent));
    outer->addWidget(card);
    auto *root = new QVBoxLayout(card);
    root->setContentsMargins(22, 14, 22, 18);
    root->setSpacing(14);

    // Custom header — X kapatma butonu + sürüklenebilir başlık
    auto *headerRow = new QHBoxLayout();
    QLabel *title = new QLabel(QString::fromUtf8("Ayarlar"));
    title->setStyleSheet(QString("color:%1; font-size:20px; font-weight:700;"
                         " background:transparent; border:none;").arg(T::Text));
    headerRow->addWidget(title);
    headerRow->addStretch();
    QPushButton *closeBtn = new QPushButton(QString::fromUtf8("✕"));
    closeBtn->setObjectName("closeX");
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setToolTip(QString::fromUtf8("Kapat"));
    headerRow->addWidget(closeBtn);
    root->addLayout(headerRow);
    connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::reject);

    auto *form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignLeft);
    form->setFormAlignment(Qt::AlignTop);
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(10);

    // Mic
    QComboBox *micCombo = new QComboBox();
    for (const QAudioDevice &d : QMediaDevices::audioInputs()) {
        micCombo->addItem(d.description(), QString::fromUtf8(d.id()));
    }
    if (micCombo->count() == 0) micCombo->addItem("(cihaz bulunamadı)", QString());
    int micIdx = micCombo->findData(settingsMicId);
    if (micIdx >= 0) micCombo->setCurrentIndex(micIdx);
    form->addRow(QString::fromUtf8("Mikrofon (giriş)"), micCombo);

    // Speaker
    QComboBox *spkCombo = new QComboBox();
    for (const QAudioDevice &d : QMediaDevices::audioOutputs()) {
        spkCombo->addItem(d.description(), QString::fromUtf8(d.id()));
    }
    if (spkCombo->count() == 0) spkCombo->addItem("(cihaz bulunamadı)", QString());
    int spkIdx = spkCombo->findData(settingsSpeakerId);
    if (spkIdx >= 0) spkCombo->setCurrentIndex(spkIdx);
    form->addRow(QString::fromUtf8("Hoparlör (çıkış)"), spkCombo);

    // ===== Ses İşleme (DSP) =====
    QLabel *audioHeader = new QLabel(QString::fromUtf8("🎙  SES İŞLEME"));
    audioHeader->setStyleSheet("color:#a78bfa; font-weight:700; font-size:12px; letter-spacing:1px; margin-top:8px;");
    form->addRow(audioHeader);

    // Mic gain slider
    QWidget *gainWrap = new QWidget();
    auto *gainLay = new QHBoxLayout(gainWrap);
    gainLay->setContentsMargins(0, 0, 0, 0);
    QSlider *gainSlider = new QSlider(Qt::Horizontal);
    gainSlider->setRange(30, 300);
    gainSlider->setValue(int(settingsMicGain * 100.0f));
    QLabel *gainValue = new QLabel(QString::number(int(settingsMicGain * 100.0f)) + "%");
    gainValue->setMinimumWidth(60);
    gainValue->setStyleSheet("color:#fafafa; font-weight:700;");
    connect(gainSlider, &QSlider::valueChanged, gainValue, [gainValue](int v){ gainValue->setText(QString::number(v) + "%"); });
    gainLay->addWidget(gainSlider, 1);
    gainLay->addWidget(gainValue);
    form->addRow(QString::fromUtf8("Mikrofon kazancı"), gainWrap);

    // Bitrate
    QComboBox *bitrateCombo = new QComboBox();
    const QList<QPair<int, QString>> bitrates = {
        {32000,  QString::fromUtf8("32 kbps · düşük")},
        {48000,  QString::fromUtf8("48 kbps · ekonomik")},
        {64000,  QString::fromUtf8("64 kbps · standart (önerilen)")},
        {96000,  QString::fromUtf8("96 kbps · yüksek")},
        {128000, QString::fromUtf8("128 kbps · çok yüksek")},
        {192000, QString::fromUtf8("192 kbps · stüdyo")}
    };
    for (const auto &p : bitrates) bitrateCombo->addItem(p.second, p.first);
    int bIdx = bitrateCombo->findData(settingsAudioBitrate);
    if (bIdx >= 0) bitrateCombo->setCurrentIndex(bIdx);
    form->addRow(QString::fromUtf8("Ses bit hızı"), bitrateCombo);

    // AEC checkbox
    QCheckBox *aecCheck = new QCheckBox(QString::fromUtf8("Yankı bastırma (AEC) — hoparlör kullanıyorsan aç"));
    aecCheck->setChecked(settingsAec);
    aecCheck->setStyleSheet("color:#fafafa;");
    form->addRow(QString(), aecCheck);

    // AGC checkbox
    QCheckBox *agcCheck = new QCheckBox(QString::fromUtf8("Otomatik kazanç kontrolü (AGC) — sesi sabit tutar"));
    agcCheck->setChecked(settingsAgc);
    agcCheck->setStyleSheet("color:#fafafa;");
    form->addRow(QString(), agcCheck);

    // NS checkbox
    QCheckBox *nsCheck = new QCheckBox(QString::fromUtf8("Gürültü bastırma (RNNoise ML) — klavye, fan, klima"));
    nsCheck->setChecked(settingsNs);
    nsCheck->setStyleSheet("color:#fafafa;");
    form->addRow(QString(), nsCheck);

    // Camera
    QComboBox *camCombo = new QComboBox();
    for (const QCameraDevice &d : QMediaDevices::videoInputs()) {
        camCombo->addItem(d.description(), QString::fromUtf8(d.id()));
    }
    if (camCombo->count() == 0) camCombo->addItem("(kamera bulunamadı)", QString());
    int camIdx = camCombo->findData(settingsCameraId);
    if (camIdx >= 0) camCombo->setCurrentIndex(camIdx);
    form->addRow("Kamera", camCombo);

    // Screen
    QComboBox *screenCombo = new QComboBox();
    screenCombo->addItem(QString::fromUtf8("Birincil ekran (otomatik)"), -1);
    const QList<QScreen*> screens = QGuiApplication::screens();
    for (int i = 0; i < screens.size(); ++i) {
        QScreen *sc = screens.at(i);
        const QRect g = sc->geometry();
        screenCombo->addItem(QString("%1 — %2 (%3×%4)").arg(i + 1).arg(sc->name()).arg(g.width()).arg(g.height()), i);
    }
    int scIdx = screenCombo->findData(settingsScreenIndex);
    if (scIdx >= 0) screenCombo->setCurrentIndex(scIdx);
    form->addRow(QString::fromUtf8("Paylaşılacak ekran"), screenCombo);

    // FPS
    QWidget *fpsWrap = new QWidget();
    auto *fpsLay = new QHBoxLayout(fpsWrap);
    fpsLay->setContentsMargins(0, 0, 0, 0);
    QSlider *fpsSlider = new QSlider(Qt::Horizontal);
    fpsSlider->setRange(1, 120);
    fpsSlider->setValue(settingsScreenFps);
    QLabel *fpsValue = new QLabel(QString::number(settingsScreenFps) + " fps");
    fpsValue->setMinimumWidth(60);
    fpsValue->setStyleSheet("color:#fafafa; font-weight:700;");
    connect(fpsSlider, &QSlider::valueChanged, fpsValue, [fpsValue](int v){ fpsValue->setText(QString::number(v) + " fps"); });
    fpsLay->addWidget(fpsSlider, 1);
    fpsLay->addWidget(fpsValue);
    form->addRow(QString::fromUtf8("Ekran paylaşım FPS"), fpsWrap);

    // Resolution
    QComboBox *resCombo = new QComboBox();
    const QList<int> heights = {360, 480, 720, 900, 1080, 1440, 2160};
    for (int h : heights) {
        const int w = static_cast<int>(h * 16.0 / 9.0);
        resCombo->addItem(QString("%1p  (%2×%1)").arg(h).arg(w), h);
    }
    int rIdx = resCombo->findData(settingsScreenHeight);
    if (rIdx < 0) rIdx = resCombo->findData(720);
    resCombo->setCurrentIndex(rIdx);
    form->addRow(QString::fromUtf8("Çözünürlük"), resCombo);

    // Quality
    QWidget *qWrap = new QWidget();
    auto *qLay = new QHBoxLayout(qWrap);
    qLay->setContentsMargins(0, 0, 0, 0);
    QSlider *qSlider = new QSlider(Qt::Horizontal);
    qSlider->setRange(10, 95);
    qSlider->setValue(settingsJpegQuality);
    QLabel *qValue = new QLabel(QString::number(settingsJpegQuality) + "%");
    qValue->setMinimumWidth(60);
    qValue->setStyleSheet("color:#fafafa; font-weight:700;");
    connect(qSlider, &QSlider::valueChanged, qValue, [qValue](int v){ qValue->setText(QString::number(v) + "%"); });
    qLay->addWidget(qSlider, 1);
    qLay->addWidget(qValue);
    form->addRow(QString::fromUtf8("JPEG kalitesi"), qWrap);

    root->addLayout(form);

    // Hint
    QLabel *hint = new QLabel(QString::fromUtf8(
        "Yüksek FPS/çözünürlük daha yumuşak ekran paylaşımı sağlar\n"
        "ancak daha fazla bant genişliği ve CPU kullanır."));
    hint->setStyleSheet("color:#a1a1aa; font-size:11px;");
    hint->setWordWrap(true);
    root->addWidget(hint);

    // Buttons
    auto *btnRow = new QHBoxLayout();
    QPushButton *securityBtn = new QPushButton(QString::fromUtf8("🔐  Hesap Güvenliği (2FA)"));
    securityBtn->setObjectName("ghost");
    securityBtn->setCursor(Qt::PointingHandCursor);
    btnRow->addWidget(securityBtn);
    btnRow->addStretch();
    QPushButton *cancelBtn = new QPushButton("Vazgeç");
    cancelBtn->setObjectName("ghost");
    cancelBtn->setCursor(Qt::PointingHandCursor);
    QPushButton *saveBtn = new QPushButton("Kaydet");
    saveBtn->setCursor(Qt::PointingHandCursor);
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(saveBtn);
    root->addLayout(btnRow);

    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(saveBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(securityBtn, &QPushButton::clicked, this, [this, &dialog]() {
        dialog.accept();
        showSecurityDialog();
    });

    if (dialog.exec() == QDialog::Accepted) {
        const QString oldCamera = settingsCameraId;
        settingsMicId        = micCombo->currentData().toString();
        settingsSpeakerId    = spkCombo->currentData().toString();
        settingsCameraId     = camCombo->currentData().toString();
        settingsScreenIndex  = screenCombo->currentData().toInt();
        settingsScreenFps    = fpsSlider->value();
        settingsScreenHeight = resCombo->currentData().toInt();
        settingsJpegQuality  = qSlider->value();
        // Ses işleme ayarları
        settingsMicGain      = float(gainSlider->value()) / 100.0f;
        settingsAudioBitrate = bitrateCombo->currentData().toInt();
        settingsAec          = aecCheck->isChecked();
        settingsAgc          = agcCheck->isChecked();
        settingsNs           = nsCheck->isChecked();
        saveSettings();
        applyScreenShareInterval();
        applyAudioSettings();
        if (oldCamera != settingsCameraId) {
            applyCameraDevice();
        }
        showToast(QString::fromUtf8("Ayarlar kaydedildi"), "success", 2200);
    }
}

// ===================== LOGIN / REGISTER =====================

void MainWindow::showLoginScreen() {
    if (loginScreen) {
        loginScreen->show();
        loginScreen->raise();
        return;
    }

    loginScreen = new QWidget(this);
    // Title bar'ın altından başla (üst 36px). resizeEvent bunu güncel tutuyor.
    const int tbh = titleBar ? titleBar->height() : 0;
    loginScreen->setGeometry(0, tbh, width(), height() - tbh);
    loginScreen->setAttribute(Qt::WA_StyledBackground, true);
    // Sade koyu arka plan — tek düz renk
    loginScreen->setStyleSheet(QString("background:%1;").arg(T::Bg));

    auto *outer = new QVBoxLayout(loginScreen);
    outer->setAlignment(Qt::AlignCenter);

    QWidget *card = new QWidget();
    card->setFixedWidth(440);
    card->setAttribute(Qt::WA_StyledBackground, true);
    card->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Maximum);
    // Sade card — düz koyu arka plan + ince kenar
    card->setStyleSheet(QString(
        "background:%1; border:1px solid %2; border-radius:14px;").arg(T::Card, T::Border));

    auto *lay = new QVBoxLayout(card);
    lay->setContentsMargins(40, 40, 40, 36);
    lay->setSpacing(14);
    lay->setSizeConstraint(QLayout::SetFixedSize);

    // Logo — sade, ring/glow yok
    QLabel *logo = new QLabel();
    logo->setAlignment(Qt::AlignCenter);
    logo->setStyleSheet("background:transparent; border:none;");
    logo->setPixmap(QPixmap(":/icons/volaura-logo.png")
                    .scaled(72, 72, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    lay->addWidget(logo, 0, Qt::AlignHCenter);

    QLabel *title = new QLabel(QString::fromUtf8("VoLaura"));
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QString("color:%1; font-size:26px; font-weight:700;"
                         " background:transparent; border:none;").arg(T::Text));
    lay->addWidget(title);

    QLabel *subtitle = new QLabel(QString::fromUtf8("Hesabınla giriş yap veya yeni hesap oluştur"));
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setStyleSheet(QString("color:%1; font-size:13px; background:transparent; border:none;").arg(T::Sub));
    lay->addWidget(subtitle);
    lay->addSpacing(8);

    const QString inputCss = QString(
        "QLineEdit{background:%1; color:%2;"
        " border:1px solid %3;"
        " border-radius:10px; padding:0 14px; font-size:13px; min-height:44px;"
        " selection-background-color:%4;}"
        "QLineEdit:hover{border:1px solid %5;"
        " background:%6;}"
        "QLineEdit:focus{border:1px solid %4;"
        " background:%7;}").arg(T::Card, T::Text, T::Border, T::Accent,
            "rgba(120,150,210,0.28)", "rgba(20,24,36,0.92)", "rgba(22,28,44,0.96)");

    // Authentication stack: 0=login/register, 1=forgot password (uygulama içi inline)
    auto *authStack = new QStackedWidget();
    authStack->setObjectName("loginAuthStack");
    authStack->setStyleSheet("background:transparent;");
    authStack->setMinimumWidth(400); // card 480 - 2*40 padding

    // ----- Login pane -----
    QWidget *loginPane = new QWidget();
    auto *loginLay = new QVBoxLayout(loginPane);
    loginLay->setContentsMargins(0, 0, 0, 0);
    loginLay->setSpacing(18);

    loginUserInput = new QLineEdit();
    loginUserInput->setPlaceholderText(QString::fromUtf8("Kullanıcı adı veya e-posta"));
    loginUserInput->setStyleSheet(inputCss);
    loginLay->addWidget(loginUserInput);

    loginEmailInput = new QLineEdit();
    loginEmailInput->setPlaceholderText(QString::fromUtf8("E-posta adresi"));
    loginEmailInput->setStyleSheet(inputCss);
    loginEmailInput->hide();
    loginLay->addWidget(loginEmailInput);

    loginPassInput = new QLineEdit();
    loginPassInput->setPlaceholderText(QString::fromUtf8("Şifre"));
    loginPassInput->setEchoMode(QLineEdit::Password);
    loginPassInput->setStyleSheet(inputCss);
    // Şifre görünürlük toggle — sade text "Göster"/"Gizle"
    {
        QAction *eye = loginPassInput->addAction(QIcon(), QLineEdit::TrailingPosition);
        eye->setText(QString::fromUtf8("Göster"));
        eye->setToolTip(QString::fromUtf8("Şifreyi göster/gizle"));
        connect(eye, &QAction::triggered, this, [this, eye]() {
            const bool show = loginPassInput->echoMode() == QLineEdit::Password;
            loginPassInput->setEchoMode(show ? QLineEdit::Normal : QLineEdit::Password);
            eye->setText(show ? QString::fromUtf8("Gizle") : QString::fromUtf8("Göster"));
        });
    }
    loginLay->addWidget(loginPassInput);

    QWidget *termsRow = new QWidget();
    termsRow->setStyleSheet("background:transparent; border:none;");
    auto *termsLay = new QHBoxLayout(termsRow);
    termsLay->setContentsMargins(2, 4, 2, 4);
    termsLay->setSpacing(10);
    loginTermsCheck = new AnimatedCheckBox();
    // (stil artık custom painter ile — CSS yok)
    QLabel *termsLabel = new QLabel(QString::fromUtf8(
        "<span style=\"color:%1;\">"
        "<a href=\"https://volaura.xyz/terms\" style=\"color:%2;text-decoration:none;font-weight:600;\">Hizmet Şartları</a>"
        "<span> ve </span>"
        "<a href=\"https://volaura.xyz/privacy\" style=\"color:%2;text-decoration:none;font-weight:600;\">Gizlilik Politikası</a>"
        "<span>'nı okudum ve kabul ediyorum.</span></span>").arg(T::Sub, T::Accent));
    termsLabel->setStyleSheet("background:transparent; border:none; font-size:12px;");
    termsLabel->setTextFormat(Qt::RichText);
    termsLabel->setOpenExternalLinks(true);
    termsLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
    termsLabel->setWordWrap(true);
    termsLabel->setCursor(Qt::ArrowCursor);
    // Tıklama: checkbox'ı toggle et (link tıklamaları zaten browser açıyor — onlar bu slot'u
    // tetiklemez çünkü href varsa default olarak link aktivasyonu yapılır)
    termsLay->addWidget(loginTermsCheck, 0, Qt::AlignTop);
    termsLay->addWidget(termsLabel, 1);
    termsRow->hide();
    loginLay->addWidget(termsRow);
    loginTermsCheck->setProperty("rowWidget", QVariant::fromValue<QObject*>(termsRow));

    loginErrorLabel = new QLabel();
    loginErrorLabel->setStyleSheet(QString("color:%1; font-size:12px; background:transparent; border:none;").arg(T::Danger));
    loginErrorLabel->setWordWrap(true);
    loginErrorLabel->hide();
    loginLay->addWidget(loginErrorLabel);

    loginInfoLabel = new QLabel();
    loginInfoLabel->setStyleSheet(QString("color:%1; font-size:12px; background:transparent; border:none;").arg(T::Success));
    loginInfoLabel->setWordWrap(true);
    loginInfoLabel->hide();
    loginLay->addWidget(loginInfoLabel);

    loginLay->addSpacing(4);
    loginSubmitBtn = new QPushButton(QString::fromUtf8("Giriş Yap"));
    loginSubmitBtn->setCursor(Qt::PointingHandCursor);
    loginSubmitBtn->setFixedHeight(44);
    loginSubmitBtn->setStyleSheet(QString(
        "QPushButton{background:%1; color:#ffffff; border:none;"
        "  border-radius:10px; font-size:14px; font-weight:700;}"
        "QPushButton:hover{background:#818cf8;}"
        "QPushButton:pressed{background:#4f46e5;}"
        "QPushButton:disabled{background:%2; color:%3;}"
    ).arg(T::Accent, T::Card, T::Muted));
    loginLay->addWidget(loginSubmitBtn);

    loginLay->addSpacing(2);

    loginResendBtn = new QPushButton(QString::fromUtf8("Doğrulama e-postasını yeniden gönder"));
    loginResendBtn->setCursor(Qt::PointingHandCursor);
    loginResendBtn->setFlat(true);
    loginResendBtn->setStyleSheet(QString(
        "QPushButton{background:transparent; color:%1; border:none; font-size:12px;}"
        "QPushButton:hover{color:%2;}"
    ).arg(T::Accent, T::Text));
    loginResendBtn->hide();
    loginResendBtn->setFocusPolicy(Qt::NoFocus);
    loginLay->addWidget(loginResendBtn);

    loginToggleBtn = new QPushButton(QString::fromUtf8("Hesabın yok mu? Kayıt ol"));
    loginToggleBtn->setCursor(Qt::PointingHandCursor);
    loginToggleBtn->setFlat(true);
    loginToggleBtn->setStyleSheet(QString(
        "QPushButton{background:transparent; color:%1; border:none; font-size:12px;}"
        "QPushButton:hover{color:%2;}"
    ).arg(T::Sub, T::Text));
    loginToggleBtn->setFocusPolicy(Qt::NoFocus);
    loginLay->addWidget(loginToggleBtn);

    loginForgotBtn = new QPushButton(QString::fromUtf8("Şifremi unuttum"));
    loginForgotBtn->setCursor(Qt::PointingHandCursor);
    loginForgotBtn->setFlat(true);
    loginForgotBtn->setStyleSheet(QString(
        "QPushButton{background:transparent; color:%1; border:none; font-size:12px;}"
        "QPushButton:hover{color:%2;}"
    ).arg(T::Sub, T::Text));
    loginForgotBtn->setFocusPolicy(Qt::NoFocus);
    loginLay->addWidget(loginForgotBtn);

    // Parolasız giriş — telefon/e-posta + tek kullanımlık kod
    auto *codeLoginBtn = new QPushButton(QString::fromUtf8("Kod ile giriş yap (parolasız)"));
    codeLoginBtn->setCursor(Qt::PointingHandCursor);
    codeLoginBtn->setFlat(true);
    codeLoginBtn->setStyleSheet(QString(
        "QPushButton{background:transparent; color:%1; border:none; font-size:12.5px;"
        " font-weight:600;}"
        "QPushButton:hover{color:%2;}"
    ).arg(T::Accent, T::Text));
    codeLoginBtn->setFocusPolicy(Qt::NoFocus);
    loginLay->addWidget(codeLoginBtn);
    connect(codeLoginBtn, &QPushButton::clicked, this, [this]() {
        showPasswordlessLoginDialog();
    });

    authStack->addWidget(loginPane);

    // ----- Forgot password pane (uygulama içi inline) -----
    QWidget *forgotPane = new QWidget();
    auto *forgotLay = new QVBoxLayout(forgotPane);
    forgotLay->setContentsMargins(0, 0, 0, 0);
    forgotLay->setSpacing(10);

    auto *forgotInfo = new QLabel(QString::fromUtf8(
        "Kullanıcı adını veya e-posta adresini gir; şifre sıfırlama bağlantısı e-postana gönderilecek."));
    forgotInfo->setWordWrap(true);
    forgotInfo->setStyleSheet(QString("color:%1; background:transparent; font-size:12px; border:none;").arg(T::Sub));
    forgotLay->addWidget(forgotInfo);

    auto *forgotInput = new QLineEdit();
    forgotInput->setObjectName("forgotInput");
    forgotInput->setPlaceholderText(QString::fromUtf8("Kullanıcı adı veya e-posta"));
    forgotInput->setStyleSheet(inputCss);
    forgotLay->addWidget(forgotInput);

    auto *forgotStatus = new QLabel();
    forgotStatus->setObjectName("forgotStatus");
    forgotStatus->setWordWrap(true);
    forgotStatus->setStyleSheet(QString("color:%1; font-size:12px; background:transparent; border:none;").arg(T::Success));
    forgotStatus->hide();
    forgotLay->addWidget(forgotStatus);

    auto *forgotSendBtn = new QPushButton(QString::fromUtf8("Bağlantıyı Gönder"));
    forgotSendBtn->setCursor(Qt::PointingHandCursor);
    forgotSendBtn->setFixedHeight(42);
    forgotSendBtn->setStyleSheet(QString(
        "QPushButton{background:%1; color:white; border:none; border-radius:10px; font-size:14px; font-weight:700;}"
        "QPushButton:hover{background:#818cf8;}"
        "QPushButton:disabled{background:%2; color:%3;}"
    ).arg(T::Accent, T::Card, T::Muted));
    forgotLay->addWidget(forgotSendBtn);

    auto *forgotBackBtn = new QPushButton(QString::fromUtf8("← Giriş ekranına dön"));
    forgotBackBtn->setCursor(Qt::PointingHandCursor);
    forgotBackBtn->setFlat(true);
    forgotBackBtn->setStyleSheet(QString(
        "QPushButton{background:transparent; color:%1; border:none; font-size:12px;}"
        "QPushButton:hover{color:%2;}"
    ).arg(T::Sub, T::Text));
    forgotBackBtn->setFocusPolicy(Qt::NoFocus);
    forgotLay->addWidget(forgotBackBtn);

    authStack->addWidget(forgotPane);
    authStack->setCurrentIndex(0);

    lay->addWidget(authStack);

    // Forgot pane connections
    connect(forgotSendBtn, &QPushButton::clicked, this, [this, forgotInput, forgotStatus, forgotSendBtn]() {
        const QString id = forgotInput->text().trimmed();
        if (id.isEmpty()) {
            forgotStatus->setStyleSheet(QString("color:%1; font-size:12px; background:transparent; border:none;").arg(T::Danger));
            forgotStatus->setText(QString::fromUtf8("Kullanıcı adı veya e-posta gerekli."));
            forgotStatus->show();
            return;
        }
        signalingClient->requestPasswordReset(id);
        forgotSendBtn->setEnabled(false);
        forgotStatus->setStyleSheet(QString("color:%1; font-size:12px; background:transparent; border:none;").arg(T::Success));
        forgotStatus->setText(QString::fromUtf8("Şifre sıfırlama isteği gönderildi. E-postanı kontrol et."));
        forgotStatus->show();
    });
    connect(forgotBackBtn, &QPushButton::clicked, this, [authStack, forgotInput, forgotStatus, forgotSendBtn, title, subtitle]() {
        forgotInput->clear();
        forgotStatus->hide();
        forgotSendBtn->setEnabled(true);
        title->setText(QString::fromUtf8("VoLaura'ya Giriş Yap"));
        subtitle->setText(QString::fromUtf8("Hesabınla giriş yap veya yeni hesap oluştur"));
        authStack->setCurrentIndex(0);
    });

    outer->addWidget(card);

    auto doSubmit = [this]() {
        const QString u = loginUserInput->text().trimmed();
        const QString e = loginEmailInput ? loginEmailInput->text().trimmed() : QString();
        const QString p = loginPassInput->text();
        if (u.isEmpty() || p.isEmpty()) {
            loginErrorLabel->setText(QString::fromUtf8("Kullanıcı adı ve şifre zorunlu."));
            loginErrorLabel->show();
            return;
        }
        if (loginInRegisterMode && e.isEmpty()) {
            loginErrorLabel->setText(QString::fromUtf8("E-posta adresi zorunlu."));
            loginErrorLabel->show();
            return;
        }
        if (loginInRegisterMode && loginTermsCheck && !loginTermsCheck->isChecked()) {
            loginErrorLabel->setText(QString::fromUtf8(
                "Devam etmek için Hizmet Şartları ve Gizlilik Politikası'nı kabul etmelisin."));
            loginErrorLabel->show();
            return;
        }
        loginErrorLabel->hide();
        if (loginResendBtn) loginResendBtn->hide();
        loginSubmitBtn->setEnabled(false);
        // Kullanıcıya anlık geri bildirim — yönlendiriliyor mesajı
        if (loginInfoLabel) {
            loginInfoLabel->setText(loginInRegisterMode
                ? QString::fromUtf8("Hesabın oluşturuluyor, lütfen bekleyin...")
                : QString::fromUtf8("Giriş yapılıyor, yönlendiriliyorsun..."));
            loginInfoLabel->show();
        }
        // Login başarılı olursa 30 gün saklansın diye ön hafızaya al
        if (!loginInRegisterMode) pendingAutoLoginPass = p;
        autoLoginSilent = false; // manuel submit: 2FA dialogu gösterilsin
        if (loginInRegisterMode) signalingClient->registerAccount(u, e, p);
        else signalingClient->login(u, p);
    };
    connect(loginSubmitBtn, &QPushButton::clicked, this, doSubmit);
    connect(loginUserInput, &QLineEdit::returnPressed, this, doSubmit);
    connect(loginEmailInput, &QLineEdit::returnPressed, this, doSubmit);
    connect(loginPassInput, &QLineEdit::returnPressed, this, doSubmit);

    connect(loginToggleBtn, &QPushButton::clicked, this, [this]() {
        loginInRegisterMode = !loginInRegisterMode;
        loginSubmitBtn->setText(loginInRegisterMode ? QString::fromUtf8("Kayıt Ol") : QString::fromUtf8("Giriş Yap"));
        loginToggleBtn->setText(loginInRegisterMode
                                ? QString::fromUtf8("Zaten hesabın var mı? Giriş yap")
                                : QString::fromUtf8("Hesabın yok mu? Kayıt ol"));
        if (loginEmailInput) loginEmailInput->setVisible(loginInRegisterMode);
        if (loginForgotBtn)  loginForgotBtn->setVisible(!loginInRegisterMode);
        if (loginResendBtn)  loginResendBtn->hide();
        if (loginTermsCheck) {
            QObject *row = loginTermsCheck->property("rowWidget").value<QObject*>();
            if (auto *w = qobject_cast<QWidget*>(row)) w->setVisible(loginInRegisterMode);
        }
        loginErrorLabel->hide();
        if (loginInfoLabel) loginInfoLabel->hide();
    });

    connect(loginForgotBtn, &QPushButton::clicked, this,
            [this, authStack, forgotInput, title, subtitle]() {
        title->setText(QString::fromUtf8("Şifremi Unuttum"));
        subtitle->setText(QString::fromUtf8("Yeni şifre belirlemek için e-posta bağlantısı gönderelim"));
        if (loginUserInput) forgotInput->setText(loginUserInput->text().trimmed());
        authStack->setCurrentIndex(1);
        forgotInput->setFocus();
    });
    connect(loginResendBtn, &QPushButton::clicked, this, [this]() {
        const QString id = !lastUnverifiedUser.isEmpty() ? lastUnverifiedUser
                                                          : loginUserInput->text().trimmed();
        if (id.isEmpty()) return;
        signalingClient->resendVerification(id);
        loginResendBtn->setEnabled(false);
        if (loginInfoLabel) {
            loginInfoLabel->setText(QString::fromUtf8("Doğrulama e-postası gönderiliyor..."));
            loginInfoLabel->show();
        }
    });

    loginUserInput->setFocus();
}

void MainWindow::hideLoginScreen() {
    if (loginScreen) loginScreen->hide();
}

void MainWindow::onLoginResult(bool ok, const QString &userNameOrError) {
    if (loginSubmitBtn) loginSubmitBtn->setEnabled(true);
    if (!ok) {
        // Auto-login başarısız oldu → saklanan credential artık geçersiz, temizle ki
        // kullanıcı normal login ekranında tekrar deneyebilsin.
        if (autoLoginAttempted) clearRememberMe();
        if (loginErrorLabel) { loginErrorLabel->setText(userNameOrError); loginErrorLabel->show(); }
        if (loginScreen) { loginScreen->show(); loginScreen->raise(); }
        return;
    }
    const bool wasAutoLogin = autoLoginAttempted && autoLoginSilent;
    authUserName = userNameOrError;
    currentUserName = authUserName;
    isLoggedIn = true;
    autoLoginSilent = false;
    // Başarılı giriş — şifreyi 30 gün sakla
    if (!pendingAutoLoginPass.isEmpty()) {
        saveRememberMe(authUserName, pendingAutoLoginPass);
        pendingAutoLoginPass.clear();
    }
    // E2E DM hazırlığı — cihaz keypair'ini hazırla ve sunucuya pub key bildir
    e2eEnsureAndAnnounce();
    hideLoginScreen();
    showMainUI();
    // Otomatik giriş ise kullanıcıya bildir (30-gün remember-me)
    if (wasAutoLogin) {
        showToast(QString::fromUtf8("✓ Otomatik giriş yapıldı — Hoş geldin %1!").arg(authUserName), "success", 3500);
    } else {
        showToast(QString::fromUtf8("✓ Giriş başarılı — Hoş geldin %1!").arg(authUserName), "success", 2800);
    }
}

void MainWindow::performLogout() {
    // ===== Themed onay dialog'u (uygulama paleti ile uyumlu) =====
    QDialog dlg(this);
    dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dlg.setAttribute(Qt::WA_TranslucentBackground);
    dlg.setMinimumWidth(440);
    auto *outer = new QVBoxLayout(&dlg);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *card = new QWidget(&dlg);
    card->setObjectName("card");
    card->setStyleSheet(QString(kPrettyDialogQss) +
        QString("QLabel#t{color:%1; font-size:18px; font-weight:700;"
                " background:transparent; border:none;}"
                "QLabel#m{color:%2; font-size:13px; line-height:1.5;"
                " background:transparent; border:none;}"
                "QPushButton#danger{background:#ef4444; color:#ffffff; border:none;"
                " border-radius:10px; padding:10px 18px; font-weight:700; font-size:13px;}"
                "QPushButton#danger:hover{background:#dc2626;}"
                "QPushButton#danger:pressed{background:#b91c1c;}"
                "QPushButton#ghost{background:transparent; color:%2; border:1px solid %3;"
                " border-radius:10px; padding:10px 16px; font-weight:600;}"
                "QPushButton#ghost:hover{color:%1; border:1px solid %4;}")
        .arg(T::Text, T::Sub, T::Border, T::Accent));
    outer->addWidget(card);
    auto *cl = new QVBoxLayout(card);
    cl->setContentsMargins(26, 22, 26, 22);
    cl->setSpacing(12);

    // Header — kırmızı küre + başlık
    auto *headRow = new QHBoxLayout();
    auto *iconBubble = new QLabel(QString::fromUtf8("🚪"));
    iconBubble->setFixedSize(44, 44);
    iconBubble->setAlignment(Qt::AlignCenter);
    iconBubble->setAttribute(Qt::WA_StyledBackground, true);
    iconBubble->setStyleSheet(
        "background:qlineargradient(x1:0,y1:0,x2:1,y2:1,"
        " stop:0 #ef4444, stop:1 #b91c1c);"
        " color:white; border-radius:22px; font-size:20px;");
    headRow->addWidget(iconBubble);
    auto *titleCol = new QVBoxLayout();
    titleCol->setSpacing(2);
    auto *t = new QLabel(QString::fromUtf8("Çıkış Yap"));
    t->setObjectName("t");
    auto *sub = new QLabel(QString::fromUtf8("Hesabından ayrılmak üzeresin"));
    sub->setStyleSheet(QString("color:%1; font-size:12px; background:transparent;").arg(T::Sub));
    titleCol->addWidget(t);
    titleCol->addWidget(sub);
    headRow->addLayout(titleCol);
    headRow->addStretch();
    cl->addLayout(headRow);

    // Açıklama
    auto *m = new QLabel(QString::fromUtf8(
        "Bu cihazda kayıtlı <b>30 günlük otomatik giriş</b> bilgilerin silinecek.<br>"
        "Tekrar girmek için kullanıcı adı ve şifren gerekecek."));
    m->setObjectName("m");
    m->setTextFormat(Qt::RichText);
    m->setWordWrap(true);
    cl->addWidget(m);
    cl->addSpacing(4);

    // Butonlar
    auto *btnRow = new QHBoxLayout();
    auto *cancelBtn = new QPushButton(QString::fromUtf8("Vazgeç"));
    cancelBtn->setObjectName("ghost");
    cancelBtn->setCursor(Qt::PointingHandCursor);
    auto *confirmBtn = new QPushButton(QString::fromUtf8("🚪  Evet, çıkış yap"));
    confirmBtn->setObjectName("danger");
    confirmBtn->setCursor(Qt::PointingHandCursor);
    confirmBtn->setMinimumHeight(42);
    btnRow->addWidget(cancelBtn);
    btnRow->addStretch();
    btnRow->addWidget(confirmBtn);
    cl->addLayout(btnRow);

    bool confirmed = false;
    connect(cancelBtn,  &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(confirmBtn, &QPushButton::clicked, &dlg, [&dlg, &confirmed]() {
        confirmed = true; dlg.accept();
    });

    showDialogBackdrop();
    dlg.exec();
    hideDialogBackdrop();
    if (!confirmed) return;

    // ===== Çıkış işlemi =====
    const QString outgoingUser = authUserName;
    clearRememberMe();
    pendingAutoLoginPass.clear();
    autoLoginAttempted = false;
    autoLoginSilent = false;
    isLoggedIn = false;
    if (signalingClient) signalingClient->logout();
    stopVoiceCapture();
    stopCallVoiceCapture();
    stopAllVoicePlayback();
    stopAllCallVoicePlayback();
    if (loginUserInput)  loginUserInput->clear();
    if (loginPassInput)  loginPassInput->clear();
    if (loginErrorLabel) loginErrorLabel->hide();
    if (loginInfoLabel) {
        loginInfoLabel->setText(QString::fromUtf8("✓ Çıkış yapıldı. Tekrar giriş yapmak için bilgilerini gir."));
        loginInfoLabel->show();
    }
    showLoginScreen();

    // Belirgin başarı toast'u
    const QString msg = outgoingUser.isEmpty()
        ? QString::fromUtf8("✓ Çıkış yapıldı")
        : QString::fromUtf8("✓ Çıkış yapıldı — Görüşmek üzere %1!").arg(outgoingUser);
    showToast(msg, "success", 4000);
}

// ===================== Update Checker =====================

void MainWindow::initUpdateChecker() {
    if (updateChecker) return;
    updateChecker = new UpdateChecker(this);
    updateChecker->setCurrentVersion(qApp->applicationVersion());
    updateChecker->setCheckUrl(QUrl("https://update.volaura.xyz/api/version"));

    connect(updateChecker, &UpdateChecker::updateAvailable,
            this, &MainWindow::onUpdateAvailable);
    connect(updateChecker, &UpdateChecker::checkFailed, this,
            [](const QString &e) { qDebug() << "[update] check failed:" << e; });
    connect(updateChecker, &UpdateChecker::downloadProgress, this,
            [this](qint64 r, qint64 t) {
        if (t > 0 && notifCenter) {
            const int pct = int(100.0 * double(r) / double(t));
            // Aynı id ile replace ediyoruz → tek satır canlı progress
            notifCenter->addNotification(
                QStringLiteral("⬇️"),
                QStringLiteral("Güncelleme indiriliyor — %%1").arg(pct),
                QStringLiteral("%1 / %2 MB").arg(r / 1024 / 1024).arg(t / 1024 / 1024),
                QString(), nullptr, "update_progress");
        }
    });
    connect(updateChecker, &UpdateChecker::downloadFinished, this,
            [this](const QString &p) {
        Q_UNUSED(p);
        if (notifCenter) notifCenter->addNotification(
            QStringLiteral("✅"),
            QStringLiteral("İndirme tamamlandı"),
            QStringLiteral("Kurulum başlatılıyor — uygulama otomatik yeniden açılacak."),
            QString(), nullptr, "update_progress");
    });
    connect(updateChecker, &UpdateChecker::downloadFailed, this,
            [this](const QString &err) {
        if (notifCenter) notifCenter->addNotification(
            QStringLiteral("⚠️"),
            QStringLiteral("Güncelleme başarısız"),
            err, QStringLiteral("Tekrar dene"),
            [this]() { updateChecker->checkNow(); }, "update_progress");
    });

    // İlk kontrol: 8 saniye sonra (UI hazır olsun); sonra 6 saatte bir
    QTimer::singleShot(8000, updateChecker, &UpdateChecker::checkNow);
    updateChecker->setPollIntervalSec(6 * 60 * 60);
}

void MainWindow::onUpdateAvailable(const QString &version, const QString &notes,
                                    const QUrl &url, const QString &sha, qint64 size) {
    if (!notifCenter) return;
    const QString sizeText = size > 0
        ? QString::fromUtf8(" · %1 MB").arg(size / 1024 / 1024)
        : QString();
    const QString body = QString::fromUtf8(
        "<b>Sürüm %1</b> çıktı (şu an %2)%3<br><br>%4")
        .arg(version, qApp->applicationVersion(), sizeText, notes.toHtmlEscaped().replace("\n", "<br>"));
    notifCenter->addNotification(
        QString::fromUtf8("🚀"),
        QString::fromUtf8("Yeni güncelleme mevcut"),
        body,
        QString::fromUtf8("Şimdi güncelle"),
        [this, url, sha]() { updateChecker->downloadAndInstall(url, sha); },
        "update_available");
}

// ===================== E2E DM yardımcıları =====================
void MainWindow::e2eEnsureAndAnnounce() {
    if (!E2E::isAvailable()) return; // libsodium yok — silent skip
    QString pub;
    if (!E2E::ensureLocalKeypair(authUserName, &pub)) return;
    if (!pub.isEmpty()) signalingClient->announcePublicKey(pub);
}

void MainWindow::e2eRequestPeerKey(const QString &username) {
    if (!E2E::isAvailable()) return;
    if (username.isEmpty()) return;
    if (e2ePeerKeyCache.contains(username)) return; // zaten istenmiş / cevap beklemede
    e2ePeerKeyCache.insert(username, QString()); // pending = boş string
    signalingClient->requestPublicKey(username);
}

// ===================== Remember-Me (30 gün) =====================
void MainWindow::saveRememberMe(const QString &user, const QString &pass) {
    QSettings s("VoLaura", "VoLaura");
    s.setValue("auth/rememberUser", user);
    // Base64 — güvenli değil ama düz metin değil (Windows'ta DPAPI ileride eklenebilir)
    s.setValue("auth/rememberPass", QString::fromLatin1(pass.toUtf8().toBase64()));
    s.setValue("auth/rememberExpireMs",
               QDateTime::currentDateTimeUtc().addDays(30).toMSecsSinceEpoch());
}

void MainWindow::clearRememberMe() {
    QSettings s("VoLaura", "VoLaura");
    s.remove("auth/rememberUser");
    s.remove("auth/rememberPass");
    s.remove("auth/rememberExpireMs");
}

bool MainWindow::tryAutoLogin() {
    QSettings s("VoLaura", "VoLaura");
    const QString user = s.value("auth/rememberUser").toString();
    const QString passB64 = s.value("auth/rememberPass").toString();
    const qint64  exp = s.value("auth/rememberExpireMs").toLongLong();
    if (user.isEmpty() || passB64.isEmpty() || exp <= 0) return false;
    if (QDateTime::currentDateTimeUtc().toMSecsSinceEpoch() >= exp) {
        clearRememberMe(); return false;
    }
    const QString pass = QString::fromUtf8(QByteArray::fromBase64(passB64.toLatin1()));
    if (pass.isEmpty()) return false;

    autoLoginAttempted = true;
    autoLoginSilent = true;
    pendingAutoLoginPass = pass;
    // Socket zaten bağlanmış olabilir, olmayabilir — iki duruma da hazır ol
    // Connected sinyaline tek seferlik bağlan; socket zaten hazırsa setupUi
    // sonrası çağrılacak tryAutoLogin anında bağlantı mevcut olsa bile
    // sinyal connect'ten önce gelmiş olabilir — bu yüzden hem doSend'i
    // doğrudan dene, hem de sinyale abone ol (sinyal yeniden bağlanmada da tetiklenir).
    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(signalingClient, &SignalingClient::connected, this,
                    [this, user, pass, conn]() {
        signalingClient->login(user, pass);
        QObject::disconnect(*conn);
    });
    // Socket şimdiden bağlıysa connected sinyali gelmeyebilir — küçük gecikmeyle deneme
    QTimer::singleShot(150, this, [this, user, pass, conn]() {
        if (pendingAutoLoginPass.isEmpty()) return; // zaten login gönderilmiş
        signalingClient->login(user, pass);
        QObject::disconnect(*conn);
    });
    return true;
}

void MainWindow::onRegisterResult(bool ok, const QString &userNameOrError) {
    if (loginSubmitBtn) loginSubmitBtn->setEnabled(true);
    if (!ok) {
        if (loginErrorLabel) { loginErrorLabel->setText(userNameOrError); loginErrorLabel->show(); }
        return;
    }
    // Otomatik giriş YOK; e-posta doğrulaması gerekiyor.
    lastUnverifiedUser = userNameOrError;
    // Detaylı mesaj registerVerifyPending sinyalinde geliyor.
}

void MainWindow::onRegisterVerifyPending(const QString &email, const QString &message) {
    // Önce mod değiştir (toggle, info label'ı temizler) sonra mesajı göster
    if (loginInRegisterMode && loginToggleBtn) loginToggleBtn->click();
    if (loginErrorLabel) loginErrorLabel->hide();
    const QString text = !message.isEmpty()
        ? message
        : QString::fromUtf8("✓ Doğrulama bağlantısı %1 adresine gönderildi. E-postanı kontrol et.").arg(email);
    if (loginInfoLabel) {
        loginInfoLabel->setText(text);
        loginInfoLabel->show();
    }
    // Kullanıcıya popup ile de bilgilendir
    QMessageBox box(this);
    box.setWindowTitle(QString::fromUtf8("Kayıt Başarılı"));
    box.setIcon(QMessageBox::Information);
    box.setText(QString::fromUtf8("Hesabın oluşturuldu."));
    box.setInformativeText(QString::fromUtf8(
        "<p><b>%1</b> adresine bir doğrulama e-postası gönderildi.</p>"
        "<p>E-postandaki bağlantıya tıklayarak hesabını aktive et, "
        "ardından buradan giriş yapabilirsin.</p>"
        "<p style='color:%2;font-size:12px;'>Posta gelmediyse spam klasörünü kontrol et "
        "veya '<i>Doğrulama e-postasını yeniden gönder</i>' butonunu kullan.</p>").arg(email, T::Sub));
    box.setStandardButtons(QMessageBox::Ok);
    box.setStyleSheet(QString(
        "QMessageBox{background:%1;}"
        "QLabel{color:%2;}"
        "QPushButton{background:%3; color:white; border:none; border-radius:8px; "
                    "padding:8px 22px; font-weight:700;}"
        "QPushButton:hover{background:#818cf8;}").arg(T::Bg, T::Text, T::Accent));
    box.exec();
    if (loginResendBtn) { loginResendBtn->setEnabled(true); loginResendBtn->show(); }
    if (loginPassInput) loginPassInput->clear();
}

void MainWindow::onLoginNeedsVerification(const QString &userName, const QString &email, const QString &message) {
    if (loginSubmitBtn) loginSubmitBtn->setEnabled(true);
    lastUnverifiedUser = !userName.isEmpty() ? userName : loginUserInput->text().trimmed();
    if (loginErrorLabel) {
        const QString text = !message.isEmpty()
            ? message
            : QString::fromUtf8("E-postan henüz doğrulanmamış. Lütfen e-postandaki bağlantıya tıkla.");
        loginErrorLabel->setText(text);
        loginErrorLabel->show();
    }
    if (loginInfoLabel && !email.isEmpty()) {
        loginInfoLabel->setText(QString::fromUtf8("E-posta adresi: %1").arg(email));
        loginInfoLabel->show();
    }
    if (loginResendBtn) { loginResendBtn->setEnabled(true); loginResendBtn->show(); }
}

void MainWindow::onPasswordResetSent(bool /*ok*/, const QString &message) {
    if (loginInfoLabel) {
        loginInfoLabel->setText(message.isEmpty()
            ? QString::fromUtf8("Şifre sıfırlama bağlantısı (varsa) e-postanıza gönderildi.")
            : message);
        loginInfoLabel->show();
    }
    if (loginErrorLabel) loginErrorLabel->hide();
}

void MainWindow::onVerificationSent(bool /*ok*/, const QString &message) {
    if (loginResendBtn) loginResendBtn->setEnabled(true);
    if (loginInfoLabel) {
        loginInfoLabel->setText(message.isEmpty()
            ? QString::fromUtf8("Doğrulama e-postası gönderildi (varsa).")
            : message);
        loginInfoLabel->show();
    }
}

void MainWindow::showForgotPasswordDialog() {
    // Inline panel - kart içindeki stack'in forgot sayfasını göster
    if (loginScreen) {
        auto *stack = loginScreen->findChild<QStackedWidget*>("loginAuthStack");
        if (stack && stack->count() >= 2) stack->setCurrentIndex(1);
    }
}

void MainWindow::showMainUI() {
    // Oto oda davranisini kaldir: login sonrasi bos/idle ekran
    participantNames.clear();
    participantMuted.clear();
    participantSharing.clear();
    rebuildParticipantGrid();
    if (roomTitleLabel) roomTitleLabel->setText(QString::fromUtf8("VoLaura"));
    if (connectionStatusLabel) connectionStatusLabel->setText(QString());
    clearChatArea();
    chatMode = ChatMode::Idle;
    currentServerId = 0;
    currentChannelId = 0;
    currentDmPeer.clear();

    // Sunucu / DM listelerini cek
    signalingClient->listServers();
    signalingClient->listDmThreads();
}

// ===================== FRIENDS UI =====================

QWidget *MainWindow::buildFriendAvatar(const QString &userName, bool online) {
    QToolButton *wrap = new QToolButton();
    wrap->setFixedSize(48, 48);
    wrap->setCursor(Qt::PointingHandCursor);
    wrap->setToolTip(userName + (online
        ? QString::fromUtf8(" (çevrimiçi) — tıkla: DM aç · sağ tık: menü")
        : QString::fromUtf8(" (çevrimdışı) — tıkla: DM aç · sağ tık: menü")));
    wrap->setStyleSheet(QString("QToolButton{background:transparent; border:none;} QToolButton:hover{background:rgba(255,255,255,0.06); border-radius:22px;}"));
    wrap->setContextMenuPolicy(Qt::CustomContextMenu);
    wrap->setProperty("friendUserName", userName);
    const bool onlineFlag = online;
    Q_UNUSED(onlineFlag);
    connect(wrap, &QToolButton::clicked, this, [this, userName]() {
        // Tıklayınca her zaman DM aç. Aramak için DM header'daki sesli/görüntülü
        // arama butonlarını ya da sağ tık menüsündeki "Sesli Ara" seçeneğini kullan.
        selectDm(userName);
    });

    // Speaking pulse ring (yesil glow) - varsayilan gizli
    QWidget *speakingRing = new QWidget(wrap);
    speakingRing->setObjectName("speakingRing");
    speakingRing->setAttribute(Qt::WA_TransparentForMouseEvents);
    speakingRing->setGeometry(0, 0, 48, 48);
    speakingRing->setStyleSheet(QString(
        "background:transparent; border:2px solid %1; border-radius:24px;").arg(T::Success));
    auto *ringGlow = new QGraphicsDropShadowEffect(speakingRing);
    ringGlow->setBlurRadius(18);
    ringGlow->setOffset(0, 0);
    ringGlow->setColor(QColor(46, 204, 113, 220));
    speakingRing->setGraphicsEffect(ringGlow);
    speakingRing->setVisible(speakingUsers.contains(userName));
    // Pulse timer: glow blur+alpha degisir
    auto *pulseTimer = new QTimer(speakingRing);
    int *pulsePhase = new int(0);
    connect(pulseTimer, &QTimer::timeout, speakingRing, [speakingRing, pulsePhase, ringGlow]() {
        if (!speakingRing->isVisible()) return;
        *pulsePhase = (*pulsePhase + 1) % 60;
        const double t = *pulsePhase / 60.0;
        const double pulse = 0.6 + 0.4 * std::sin(t * 6.2831853);
        ringGlow->setBlurRadius(14 + pulse * 10);
        ringGlow->setColor(QColor(46, 204, 113, int(150 + 80 * pulse)));
    });
    pulseTimer->start(50);

    QLabel *avatar = new QLabel(wrap);
    avatar->setGeometry(4, 4, 40, 40);
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setAttribute(Qt::WA_StyledBackground, true);
    avatar->setText(avatarInitials(userName));
    const QString bg = online ? avatarColor(userName) : QString("#3a4258");
    const QString color = online ? QString("#ffffff") : QString("#8b97b4");
    avatar->setStyleSheet(QString(
        "background:%1; color:%2; border-radius:20px; font-weight:700; font-size:14px;"
    ).arg(bg, color));

    QLabel *dot = new QLabel(wrap);
    dot->setFixedSize(12, 12);
    dot->move(32, 32);
    dot->setAttribute(Qt::WA_StyledBackground, true);
    dot->setStyleSheet(QString(
        "background:%1; border:2px solid #0a0a0b; border-radius:6px;"
    ).arg(online ? "#22c55e" : "#52525b"));

    // DM okunmamış rozeti (sağ üst)
    const int dmUnread = unreadByDmPeer.value(userName, 0);
    if (dmUnread > 0) {
        QLabel *ubadge = new QLabel(wrap);
        ubadge->setText(dmUnread > 99 ? QString("99+") : QString::number(dmUnread));
        ubadge->setAlignment(Qt::AlignCenter);
        ubadge->setAttribute(Qt::WA_StyledBackground, true);
        ubadge->setStyleSheet(
            "background:#ef4444; color:white; border:2px solid #0a0a0b;"
            " border-radius:9px; font-size:10px; font-weight:800;");
        const int bw = dmUnread > 9 ? 24 : 18;
        ubadge->setGeometry(48 - bw, -2, bw, 18);
    }

    connect(wrap, &QWidget::customContextMenuRequested, this, [this, userName](const QPoint &){
        QDialog d(this);
        d.setWindowTitle(userName);
        d.setStyleSheet("QDialog{background:#141417;} QPushButton{background:#26262b; color:#fafafa; border:none; border-radius:8px; padding:8px 14px; font-weight:700;} QPushButton:hover{background:#3a3a40;} QPushButton#call{background:#22c55e; color:#0a0a0b;} QPushButton#call:hover{background:#4ade80;} QPushButton#danger{background:#ef4444;} QPushButton#danger:hover{background:#f87171;} QLabel{color:#fafafa;}");
        auto *l = new QVBoxLayout(&d);
        QLabel *n = new QLabel(userName);
        n->setStyleSheet("font-size:16px; font-weight:700;");
        l->addWidget(n);
        QPushButton *callBtn = new QPushButton(QString::fromUtf8("Sesli Ara"));
        callBtn->setObjectName("call");
        l->addWidget(callBtn);
        QPushButton *dmBtn = new QPushButton(QString::fromUtf8("Mesaj Gönder (DM)"));
        l->addWidget(dmBtn);
        QPushButton *rm = new QPushButton(QString::fromUtf8("Arkadaşlıktan çıkar"));
        rm->setObjectName("danger");
        l->addWidget(rm);
        QPushButton *close = new QPushButton("Kapat");
        l->addWidget(close);
        connect(callBtn, &QPushButton::clicked, &d, [this, userName, &d]() {
            d.accept();
            startCallToFriend(userName);
        });
        connect(dmBtn, &QPushButton::clicked, &d, [this, userName, &d]() {
            d.accept();
            selectDm(userName);
        });
        connect(rm, &QPushButton::clicked, &d, [this, userName, &d](){
            d.accept();
            if (promptConfirm(
                    QString::fromUtf8("Arkadaşlıktan Çıkar"),
                    QString::fromUtf8("%1 ile arkadaşlığını sonlandırmak istediğine emin misin?").arg(userName),
                    QString::fromUtf8("Çıkar"),
                    QString::fromUtf8("Vazgeç"),
                    true)) {
                signalingClient->removeFriend(userName);
                showToast(QString::fromUtf8("%1 arkadaş listenden çıkarıldı").arg(userName), "info");
            }
        });
        connect(close, &QPushButton::clicked, &d, &QDialog::reject);
        d.exec();
    });
    return wrap;
}

void MainWindow::rebuildFriendsSidebar() {
    if (!friendsSidebarLayout) return;
    QLayoutItem *child;
    while ((child = friendsSidebarLayout->takeAt(0)) != nullptr) {
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }
    QList<QString> names = friendsOnline.keys();
    std::sort(names.begin(), names.end(), [this](const QString &a, const QString &b){
        const bool oa = friendsOnline.value(a, false);
        const bool ob = friendsOnline.value(b, false);
        if (oa != ob) return oa;
        return a.toLower() < b.toLower();
    });
    for (const QString &n : names) {
        friendsSidebarLayout->addWidget(buildFriendAvatar(n, friendsOnline.value(n, false)), 0, Qt::AlignHCenter);
    }
    friendsSidebarLayout->addStretch();
}

void MainWindow::updateRequestsBadge() {
    if (!requestsBtn) return;
    const int n = pendingInRequests.size();
    if (n > 0) {
        requestsBtn->setStyleSheet(QString(
            "QToolButton{background:#3a1a22; border:2px solid %1; border-radius:20px; color:#ffd5dd; font-size:13px; font-weight:700;}"
            "QToolButton:hover{background:#4c222d;}").arg(T::Danger));
        requestsBtn->setText(QString::number(n));
        requestsBtn->setToolTip(QString::fromUtf8("%1 bekleyen arkadaşlık isteği").arg(n));
    } else {
        requestsBtn->setStyleSheet(QString(
            "QToolButton{background:transparent; border:none; border-radius:14px; color:%1;}"
            "QToolButton:hover{background:rgba(255,255,255,0.06); color:%2;}").arg(T::Sub, T::Text));
        requestsBtn->setIcon(makeLineIcon("envelope", QColor(T::Text), 20));
        requestsBtn->setIconSize(QSize(20, 20));
        requestsBtn->setText("");
        requestsBtn->setToolTip(QString::fromUtf8("Arkadaşlık istekleri"));
    }
}

void MainWindow::onFriendsListUpdated(const QJsonArray &friends, const QJsonArray &pendingIn, const QJsonArray &pendingOut) {
    friendsOnline.clear();
    for (const QJsonValue &v : friends) {
        const QJsonObject o = v.toObject();
        friendsOnline.insert(o.value("userName").toString(), o.value("online").toBool());
    }
    pendingInRequests.clear();
    for (const QJsonValue &v : pendingIn) pendingInRequests.insert(v.toString());
    pendingOutRequests.clear();
    for (const QJsonValue &v : pendingOut) pendingOutRequests.insert(v.toString());
    rebuildFriendsSidebar();
    updateRequestsBadge();
}

void MainWindow::onFriendRequestReceived(const QString &fromUserName) {
    pendingInRequests.insert(fromUserName);
    updateRequestsBadge();
    appendSystemMessage(QString::fromUtf8("Arkadaşlık isteği: %1").arg(fromUserName));
    notifyUser(QString::fromUtf8("Arkadaşlık isteği"),
               QString::fromUtf8("%1 sana arkadaşlık isteği gönderdi").arg(fromUserName),
               "friend");
}

void MainWindow::onFriendAdded(const QString &userName, bool online) {
    friendsOnline.insert(userName, online);
    pendingOutRequests.remove(userName);
    pendingInRequests.remove(userName);
    rebuildFriendsSidebar();
    updateRequestsBadge();
    appendSystemMessage(QString::fromUtf8("Arkadaş eklendi: %1").arg(userName));
}

void MainWindow::onFriendRemoved(const QString &userName) {
    friendsOnline.remove(userName);
    rebuildFriendsSidebar();
    appendSystemMessage(QString::fromUtf8("%1 artık arkadaşın değil.").arg(userName));
}

void MainWindow::onFriendStatusChanged(const QString &userName, bool online) {
    if (friendsOnline.contains(userName)) {
        friendsOnline[userName] = online;
        rebuildFriendsSidebar();
    }
}

void MainWindow::onFriendOpResult(bool ok, const QString &op, const QString &userNameOrError) {
    Q_UNUSED(op);
    if (!ok) showToast(QString::fromUtf8("Arkadaşlık hatası: ") + userNameOrError, "error", 3000);
}

void MainWindow::showAddFriendDialog() {
    if (!isLoggedIn) {
        showToast(QString::fromUtf8("Önce giriş yapmalısın"), "warn", 2200);
        return;
    }
    QDialog d(this);
    d.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    d.setAttribute(Qt::WA_TranslucentBackground);
    d.setMinimumSize(460, 440);
    auto *outer = new QVBoxLayout(&d);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *cardW = new QWidget(&d);
    cardW->setObjectName("card");
    cardW->setStyleSheet((QString(kPrettyDialogQss) +
        "QListWidget{background:%1; border:1px solid %2;"
        " border-radius:11px; color:%3; padding:4px;}"
        "QListWidget::item{padding:8px; border-radius:6px;}"
        "QListWidget::item:selected{background:%4;}"
        "QListWidget::item:hover{background:%5;}"
        "QPushButton#closeX{background:transparent; color:%6; border:none;"
        " font-size:18px; font-weight:700; min-width:28px; max-width:28px;"
        " min-height:28px; max-height:28px; border-radius:14px;}"
        "QPushButton#closeX:hover{background:rgba(255,255,255,0.08); color:%3;}"
    ).arg(T::Card, T::Border, T::Text, T::Accent, T::Input, T::Sub));
    outer->addWidget(cardW);
    auto *lay = new QVBoxLayout(cardW);
    lay->setContentsMargins(22, 14, 22, 18);
    lay->setSpacing(10);

    // Header (title + X)
    auto *hdr = new QHBoxLayout();
    QLabel *title = new QLabel(QString::fromUtf8("Arkadaşlık İsteği Gönder"));
    title->setStyleSheet(QString("color:%1; font-size:18px; font-weight:700;"
                         " background:transparent; border:none;").arg(T::Text));
    hdr->addWidget(title);
    hdr->addStretch();
    QPushButton *closeX = new QPushButton(QString::fromUtf8("✕"));
    closeX->setObjectName("closeX");
    closeX->setCursor(Qt::PointingHandCursor);
    hdr->addWidget(closeX);
    lay->addLayout(hdr);
    connect(closeX, &QPushButton::clicked, &d, &QDialog::reject);

    QLabel *hint = new QLabel(QString::fromUtf8("Kullanıcı adı yaz ve istek gönder. Mevcut arkadaşlarında arama da yapabilirsin."));
    hint->setStyleSheet("color:#8b97b4; font-size:12px; background:transparent; border:none;");
    hint->setWordWrap(true);
    lay->addWidget(hint);

    QLineEdit *input = new QLineEdit();
    input->setPlaceholderText(QString::fromUtf8("🔍  Kullanıcı ara..."));
    lay->addWidget(input);

    QLabel *sectionLbl = new QLabel(QString::fromUtf8("Arkadaşların"));
    sectionLbl->setStyleSheet("color:#a1a1aa; font-size:11px; margin-top:4px;");
    lay->addWidget(sectionLbl);

    QListWidget *list = new QListWidget();
    lay->addWidget(list, 1);

    auto refresh = [this, list, input, sectionLbl]() {
        list->clear();
        const QString q = input->text().trimmed().toLower();
        int matches = 0;
        QList<QString> names = friendsOnline.keys();
        std::sort(names.begin(), names.end(), [this](const QString &a, const QString &b){
            const bool oa = friendsOnline.value(a, false);
            const bool ob = friendsOnline.value(b, false);
            if (oa != ob) return oa;
            return a.toLower() < b.toLower();
        });
        for (const QString &n : names) {
            if (!q.isEmpty() && !n.toLower().contains(q)) continue;
            const bool online = friendsOnline.value(n, false);
            QListWidgetItem *it = new QListWidgetItem(
                QString("%1 %2").arg(online ? QString::fromUtf8("🟢") : QString::fromUtf8("⚫"), n));
            list->addItem(it);
            matches++;
        }
        sectionLbl->setText(q.isEmpty()
            ? QString::fromUtf8("Arkadaşların (%1)").arg(friendsOnline.size())
            : QString::fromUtf8("Eşleşen (%1)").arg(matches));
    };
    refresh();
    connect(input, &QLineEdit::textChanged, &d, refresh);

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    QPushButton *cancel = new QPushButton("Kapat");
    cancel->setObjectName("ghost");
    QPushButton *send = new QPushButton(QString::fromUtf8("İstek Gönder"));
    btnRow->addWidget(cancel);
    btnRow->addWidget(send);
    lay->addLayout(btnRow);

    connect(cancel, &QPushButton::clicked, &d, &QDialog::reject);
    connect(send, &QPushButton::clicked, &d, [this, input, &d]() {
        const QString u = input->text().trimmed();
        if (!u.isEmpty()) signalingClient->sendFriendRequest(u);
        d.accept();
    });
    connect(input, &QLineEdit::returnPressed, send, &QPushButton::click);
    input->setFocus();
    d.exec();
}

void MainWindow::showRequestsDialog() {
    if (!isLoggedIn) return;
    QDialog d(this);
    d.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    d.setAttribute(Qt::WA_TranslucentBackground);
    d.setMinimumWidth(460);

    auto *outerL = new QVBoxLayout(&d);
    outerL->setContentsMargins(0, 0, 0, 0);
    auto *cardW = new QWidget(&d);
    cardW->setObjectName("card");
    cardW->setStyleSheet((QString(kPrettyDialogQss) +
        "QLabel#name{font-weight:700; font-size:14px;}"
        "QLabel#muted{color:%1; font-size:11px;}"
        "QPushButton#accept{background:qlineargradient(x1:0,y1:0,x2:1,y2:1,"
        " stop:0 %2, stop:1 #4ade80); color:#0a0a0b;}"
        "QPushButton#accept:hover{background:qlineargradient(x1:0,y1:0,x2:1,y2:1,"
        " stop:0 #4ade80, stop:1 #86efac);}"
        "QPushButton#reject{background:qlineargradient(x1:0,y1:0,x2:1,y2:1,"
        " stop:0 %3, stop:1 #f87171);}"
        "QPushButton#reject:hover{background:qlineargradient(x1:0,y1:0,x2:1,y2:1,"
        " stop:0 #f87171, stop:1 #fca5a5);}"
        "QPushButton#closeX{background:transparent; color:%1; border:none;"
        " font-size:18px; font-weight:700; min-width:28px; max-width:28px;"
        " min-height:28px; max-height:28px; border-radius:14px;}"
        "QPushButton#closeX:hover{background:rgba(255,255,255,0.08); color:%4;}"
    ).arg(T::Sub, T::Success, T::Danger, T::Text));
    outerL->addWidget(cardW);
    auto *root = new QVBoxLayout(cardW);
    root->setContentsMargins(22, 14, 22, 18);
    root->setSpacing(10);

    auto *hdr = new QHBoxLayout();
    QLabel *title = new QLabel(QString::fromUtf8("Arkadaşlık İstekleri"));
    title->setStyleSheet(QString("color:%1; font-size:20px; font-weight:700;"
                         " background:transparent; border:none;").arg(T::Text));
    hdr->addWidget(title);
    hdr->addStretch();
    QPushButton *closeXBtn = new QPushButton(QString::fromUtf8("✕"));
    closeXBtn->setObjectName("closeX");
    closeXBtn->setCursor(Qt::PointingHandCursor);
    hdr->addWidget(closeXBtn);
    root->addLayout(hdr);
    connect(closeXBtn, &QPushButton::clicked, &d, &QDialog::reject);

    QLabel *inLbl = new QLabel(QString::fromUtf8("Gelen istekler"));
    inLbl->setObjectName("muted");
    root->addWidget(inLbl);

    if (pendingInRequests.isEmpty()) {
        QLabel *none = new QLabel(QString::fromUtf8("Bekleyen istek yok."));
        none->setStyleSheet("color:#4a5670;");
        root->addWidget(none);
    } else {
        for (const QString &name : pendingInRequests) {
            QWidget *row = new QWidget();
            auto *rl = new QHBoxLayout(row);
            rl->setContentsMargins(0, 0, 0, 0);
            QLabel *n = new QLabel(name);
            n->setObjectName("name");
            rl->addWidget(n, 1);
            QPushButton *acc = new QPushButton(QString::fromUtf8("Kabul"));
            acc->setObjectName("accept");
            QPushButton *rej = new QPushButton(QString::fromUtf8("Reddet"));
            rej->setObjectName("reject");
            rl->addWidget(acc);
            rl->addWidget(rej);
            connect(acc, &QPushButton::clicked, this, [this, name, row, inLbl]() {
                signalingClient->acceptFriendRequest(name);
                pendingInRequests.remove(name);
                row->hide();
                updateRequestsBadge();
            });
            connect(rej, &QPushButton::clicked, this, [this, name, row]() {
                signalingClient->rejectFriendRequest(name);
                pendingInRequests.remove(name);
                row->hide();
                updateRequestsBadge();
            });
            root->addWidget(row);
        }
    }

    QLabel *outLbl = new QLabel(QString::fromUtf8("Gönderilen istekler"));
    outLbl->setObjectName("muted");
    root->addWidget(outLbl);
    if (pendingOutRequests.isEmpty()) {
        QLabel *none = new QLabel(QString::fromUtf8("Bekleyen giden istek yok."));
        none->setStyleSheet("color:#4a5670;");
        root->addWidget(none);
    } else {
        for (const QString &name : pendingOutRequests) {
            QWidget *row = new QWidget();
            auto *rl = new QHBoxLayout(row);
            rl->setContentsMargins(0, 0, 0, 0);
            QLabel *n = new QLabel(name + QString::fromUtf8("  — bekliyor"));
            n->setObjectName("name");
            rl->addWidget(n, 1);
            QPushButton *cancel = new QPushButton(QString::fromUtf8("İptal"));
            cancel->setObjectName("ghost");
            rl->addWidget(cancel);
            connect(cancel, &QPushButton::clicked, this, [this, name, row]() {
                signalingClient->cancelFriendRequest(name);
                pendingOutRequests.remove(name);
                row->hide();
            });
            root->addWidget(row);
        }
    }

    auto *close = new QPushButton("Kapat");
    close->setObjectName("ghost");
    root->addSpacing(6);
    root->addWidget(close, 0, Qt::AlignRight);
    connect(close, &QPushButton::clicked, &d, &QDialog::accept);
    d.exec();
}

// ===================== CALL FLOW =====================

void MainWindow::startCallToFriend(const QString &userName) {
    if (!isLoggedIn) return;
    // Çevrimdışı arkadaşı aramayı ENGELLEMİYORUZ. Sunucu offline durumunda
    // onCallUnreachable ile yanıtlar; o zaman kullanıcıya geri bildirim veririz.
    // (Kullanıcı talebi: "çevrimdışı diye aramamazlık yapma, çaldır".)
    if (!activeCallPeer.isEmpty() || !pendingCallTo.isEmpty()) {
        showToast(QString::fromUtf8("Zaten aktif bir çağrı var"), "warn", 2200);
        return;
    }
    pendingCallTo = userName;
    updateMediaControlVisibility();

    // Create a room; on roomCreated we'll send call_friend
    signalingClient->createRoom(QString::fromUtf8("Çağrı: %1").arg(userName), currentUserName, QString());

    // Outgoing ringing dialog
    if (ringingDialog) { ringingDialog->close(); ringingDialog->deleteLater(); ringingDialog = nullptr; }
    ringingDialog = new QDialog(this);
    ringingDialog->setWindowTitle(QString::fromUtf8("Aranıyor..."));
    ringingDialog->setMinimumWidth(360);
    ringingDialog->setStyleSheet(
        "QDialog{background:#141417;}"
        "QLabel{color:#fafafa;}"
        "QLabel#title{font-size:20px; font-weight:700;}"
        "QLabel#muted{color:#a1a1aa; font-size:12px;}"
        "QPushButton{background:#ef4444; color:white; border:none; border-radius:10px; padding:10px 18px; font-weight:700;}"
        "QPushButton:hover{background:#f87171;}"
    );
    auto *lay = new QVBoxLayout(ringingDialog);
    lay->setContentsMargins(26, 24, 26, 20);
    lay->setSpacing(14);
    lay->setAlignment(Qt::AlignCenter);

    QLabel *av = new QLabel(avatarInitials(userName));
    av->setFixedSize(86, 86);
    av->setAlignment(Qt::AlignCenter);
    av->setAttribute(Qt::WA_StyledBackground, true);
    av->setStyleSheet(QString("background:%1; color:white; border-radius:43px; font-size:30px; font-weight:800;").arg(avatarColor(userName)));
    lay->addWidget(av, 0, Qt::AlignHCenter);

    QLabel *t = new QLabel(userName);
    t->setObjectName("title");
    t->setAlignment(Qt::AlignCenter);
    lay->addWidget(t);

    QLabel *s = new QLabel(QString::fromUtf8("Aranıyor..."));
    s->setObjectName("muted");
    s->setAlignment(Qt::AlignCenter);
    lay->addWidget(s);

    QPushButton *cancel = new QPushButton(QString::fromUtf8("İptal"));
    cancel->setCursor(Qt::PointingHandCursor);
    cancel->setFixedHeight(44);
    lay->addWidget(cancel);

    connect(cancel, &QPushButton::clicked, this, [this]() {
        if (!pendingCallTo.isEmpty()) {
            signalingClient->cancelCall(pendingCallTo);
            pendingCallTo.clear();
        }
        if (ringingDialog) { ringingDialog->close(); ringingDialog->deleteLater(); ringingDialog = nullptr; }
        signalingClient->leaveRoom();
    });

    ringingDialog->show();
}

void MainWindow::onIncomingCall(const QString &fromUserName, const QString &roomCode) {
    // Sistem bildirimi + ses (pencere arka plandaysa toast + multi-beep)
    notifyUser(QString::fromUtf8("Gelen arama"),
               QString::fromUtf8("%1 seni arıyor").arg(fromUserName), "call");
    if (ringingDialog) { ringingDialog->close(); ringingDialog->deleteLater(); ringingDialog = nullptr; }
    ringingDialog = new QDialog(this);
    ringingDialog->setWindowTitle(QString::fromUtf8("Gelen Arama"));
    ringingDialog->setMinimumWidth(380);
    ringingDialog->setStyleSheet(
        "QDialog{background:#141417;}"
        "QLabel{color:#fafafa;}"
        "QLabel#title{font-size:20px; font-weight:700;}"
        "QLabel#muted{color:#a1a1aa; font-size:12px;}"
        "QPushButton{color:white; border:none; border-radius:10px; padding:10px 18px; font-weight:700;}"
        "QPushButton#accept{background:#22c55e; color:#0a0a0b;} QPushButton#accept:hover{background:#4ade80;}"
        "QPushButton#decline{background:#ef4444;} QPushButton#decline:hover{background:#f87171;}"
    );
    auto *lay = new QVBoxLayout(ringingDialog);
    lay->setContentsMargins(26, 24, 26, 20);
    lay->setSpacing(14);
    lay->setAlignment(Qt::AlignCenter);

    QLabel *av = new QLabel(avatarInitials(fromUserName));
    av->setFixedSize(86, 86);
    av->setAlignment(Qt::AlignCenter);
    av->setAttribute(Qt::WA_StyledBackground, true);
    av->setStyleSheet(QString("background:%1; color:white; border-radius:43px; font-size:30px; font-weight:800;").arg(avatarColor(fromUserName)));
    lay->addWidget(av, 0, Qt::AlignHCenter);

    QLabel *t = new QLabel(fromUserName);
    t->setObjectName("title");
    t->setAlignment(Qt::AlignCenter);
    lay->addWidget(t);

    QLabel *s = new QLabel(QString::fromUtf8("Seni arıyor..."));
    s->setObjectName("muted");
    s->setAlignment(Qt::AlignCenter);
    lay->addWidget(s);

    auto *btnRow = new QHBoxLayout();
    QPushButton *decline = new QPushButton(QString::fromUtf8("Reddet"));
    decline->setObjectName("decline");
    decline->setCursor(Qt::PointingHandCursor);
    decline->setFixedHeight(44);
    QPushButton *accept = new QPushButton(QString::fromUtf8("Kabul Et"));
    accept->setObjectName("accept");
    accept->setCursor(Qt::PointingHandCursor);
    accept->setFixedHeight(44);
    btnRow->addWidget(decline);
    btnRow->addWidget(accept);
    lay->addLayout(btnRow);

    const QString caller = fromUserName;
    const QString code = roomCode;
    connect(decline, &QPushButton::clicked, this, [this, caller]() {
        signalingClient->declineCall(caller);
        if (ringingDialog) { ringingDialog->close(); ringingDialog->deleteLater(); ringingDialog = nullptr; }
    });
    connect(accept, &QPushButton::clicked, this, [this, caller, code]() {
        signalingClient->acceptCall(caller);
        activeCallPeer = caller;
        updateMediaControlVisibility();
        if (ringingDialog) { ringingDialog->close(); ringingDialog->deleteLater(); ringingDialog = nullptr; }
        hideWelcomeModal();
        signalingClient->joinRoom(code, currentUserName, QString());
    });

    ringingDialog->show();
}

void MainWindow::onCallDeclined(const QString &fromUserName) {
    if (pendingCallTo == fromUserName || activeCallPeer == fromUserName) {
        appendSystemMessage(fromUserName + QString::fromUtf8(" aramayı reddetti."));
        pendingCallTo.clear();
        activeCallPeer.clear();
        updateMediaControlVisibility();
        if (ringingDialog) { ringingDialog->close(); ringingDialog->deleteLater(); ringingDialog = nullptr; }
        stopCallVoiceCapture();
        stopAllCallVoicePlayback();
        signalingClient->leaveRoom();
    }
}

void MainWindow::onCallCancelled(const QString &fromUserName) {
    if (ringingDialog) { ringingDialog->close(); ringingDialog->deleteLater(); ringingDialog = nullptr; }
    appendSystemMessage(fromUserName + QString::fromUtf8(" aramayı iptal etti."));
    pendingCallTo.clear();
    activeCallPeer.clear();
    updateMediaControlVisibility();
    stopCallVoiceCapture();
    stopAllCallVoicePlayback();
}

void MainWindow::onCallAccepted(const QString &fromUserName) {
    activeCallPeer = fromUserName;
    pendingCallTo.clear();
    updateMediaControlVisibility();
    if (ringingDialog) { ringingDialog->close(); ringingDialog->deleteLater(); ringingDialog = nullptr; }
    appendSystemMessage(fromUserName + QString::fromUtf8(" aramayı kabul etti."));
}

void MainWindow::onCallUnreachable(const QString &userName) {
    pendingCallTo.clear();
    activeCallPeer.clear();
    updateMediaControlVisibility();
    if (ringingDialog) { ringingDialog->close(); ringingDialog->deleteLater(); ringingDialog = nullptr; }
    signalingClient->leaveRoom();
    appendSystemMessage(userName + QString::fromUtf8(" ulaşılamıyor (çevrimdışı)."));
}

void MainWindow::onCallError(const QString &userName, const QString &error) {
    pendingCallTo.clear();
    activeCallPeer.clear();
    updateMediaControlVisibility();
    if (ringingDialog) { ringingDialog->close(); ringingDialog->deleteLater(); ringingDialog = nullptr; }
    signalingClient->leaveRoom();
    appendSystemMessage(QString::fromUtf8("Arama hatası (%1): %2").arg(userName, error));
}

// ===================== SCREEN SOURCE PICKER =====================

void MainWindow::showScreenSourcePicker() {
    QDialog d(this);
    d.setWindowTitle(QString::fromUtf8("Ekran Paylaşımı Kaynağı"));
    d.setMinimumSize(640, 440);
    d.setStyleSheet(
        "QDialog{background:#141417;}"
        "QLabel{color:#fafafa;}"
        "QLabel#title{font-size:20px; font-weight:700;}"
        "QLabel#muted{color:#a1a1aa; font-size:12px;}"
        "QPushButton{background:#26262b; color:#fafafa; border:none; border-radius:8px; padding:8px 14px; font-weight:700;}"
        "QPushButton:hover{background:#3a3a40;}"
        "QPushButton#primary{background:#6366f1;} QPushButton#primary:hover{background:#818cf8;}"
        "QPushButton#cancel{background:#26262b;}"
        "QFrame#thumb{background:#17171a; border:2px solid #26262b; border-radius:12px;}"
        "QFrame#thumb[selected=\"true\"]{border-color:#6366f1;}"
    );
    auto *root = new QVBoxLayout(&d);
    root->setContentsMargins(22, 20, 22, 18);
    root->setSpacing(12);
    QLabel *title = new QLabel(QString::fromUtf8("Paylaşılacak Ekranı Seç"));
    title->setObjectName("title");
    root->addWidget(title);
    QLabel *hint = new QLabel(QString::fromUtf8("Birden fazla monitörün varsa hangi ekranı paylaşacağını seç."));
    hint->setObjectName("muted");
    root->addWidget(hint);

    auto *grid = new QGridLayout();
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(12);

    const QList<QScreen*> screens = QGuiApplication::screens();
    int selectedIndex = settingsScreenIndex >= 0 && settingsScreenIndex < screens.size() ? settingsScreenIndex : 0;
    QList<QFrame*> thumbs;
    for (int i = 0; i < screens.size(); ++i) {
        QScreen *sc = screens.at(i);
        QFrame *thumb = new QFrame();
        thumb->setObjectName("thumb");
        thumb->setAttribute(Qt::WA_StyledBackground, true);
        thumb->setProperty("selected", i == selectedIndex);
        thumb->setCursor(Qt::PointingHandCursor);
        thumb->setMinimumSize(260, 180);
        auto *tl = new QVBoxLayout(thumb);
        tl->setContentsMargins(10, 10, 10, 10);
        tl->setSpacing(6);
        QLabel *preview = new QLabel();
        preview->setAlignment(Qt::AlignCenter);
        preview->setStyleSheet("background:#0a0a0b; border-radius:8px;");
        preview->setMinimumHeight(130);
        QPixmap pm = sc->grabWindow(0);
        if (!pm.isNull()) preview->setPixmap(pm.scaled(240, 130, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        tl->addWidget(preview, 1);
        const QRect g = sc->geometry();
        QLabel *name = new QLabel(QString("%1 — %2×%3").arg(sc->name().isEmpty() ? QString("Ekran %1").arg(i+1) : sc->name()).arg(g.width()).arg(g.height()));
        name->setStyleSheet("font-weight:700;");
        tl->addWidget(name);
        thumb->installEventFilter(&d);
        grid->addWidget(thumb, i / 2, i % 2);
        thumbs.append(thumb);
    }
    root->addLayout(grid, 1);

    // Selection logic
    class Picker : public QObject {
    public:
        QList<QFrame*> thumbs;
        int *selected;
        Picker(QList<QFrame*> t, int *s, QObject *p) : QObject(p), thumbs(t), selected(s) {}
        bool eventFilter(QObject *obj, QEvent *ev) override {
            if (ev->type() == QEvent::MouseButtonPress) {
                for (int i = 0; i < thumbs.size(); ++i) {
                    if (thumbs[i] == obj) {
                        *selected = i;
                        for (int j = 0; j < thumbs.size(); ++j) {
                            thumbs[j]->setProperty("selected", j == i);
                            thumbs[j]->style()->unpolish(thumbs[j]);
                            thumbs[j]->style()->polish(thumbs[j]);
                        }
                        return true;
                    }
                }
            }
            return false;
        }
    };
    Picker *picker = new Picker(thumbs, &selectedIndex, &d);
    for (QFrame *t : thumbs) t->installEventFilter(picker);

    // FPS control inline
    auto *fpsRow = new QHBoxLayout();
    QLabel *fpsLbl = new QLabel(QString::fromUtf8("FPS: %1").arg(settingsScreenFps));
    QSlider *fps = new QSlider(Qt::Horizontal);
    fps->setRange(1, 120);
    fps->setValue(settingsScreenFps);
    connect(fps, &QSlider::valueChanged, fpsLbl, [fpsLbl](int v){ fpsLbl->setText(QString::fromUtf8("FPS: %1").arg(v)); });
    fpsRow->addWidget(fpsLbl);
    fpsRow->addWidget(fps, 1);
    root->addLayout(fpsRow);

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    QPushButton *cancel = new QPushButton("Vazgeç");
    cancel->setObjectName("cancel");
    QPushButton *startBtn = new QPushButton(QString::fromUtf8("Paylaşımı Başlat"));
    startBtn->setObjectName("primary");
    btnRow->addWidget(cancel);
    btnRow->addWidget(startBtn);
    root->addLayout(btnRow);

    connect(cancel, &QPushButton::clicked, &d, &QDialog::reject);
    connect(startBtn, &QPushButton::clicked, &d, &QDialog::accept);

    if (d.exec() == QDialog::Accepted) {
        settingsScreenIndex = selectedIndex;
        settingsScreenFps = fps->value();
        saveSettings();
        applyScreenShareInterval();
        // Start sharing if not already
        if (!isScreenSharing) toggleScreenShare();
    }
}

bool MainWindow::pickScreenSourceForVoice() {
    const QList<QScreen*> screens = QGuiApplication::screens();
    if (screens.size() <= 1) return true; // tek ekran, direkt başlat
    QDialog d(this);
    d.setWindowTitle(QString::fromUtf8("Sesli Kanalda Ekran Paylaşımı"));
    d.setMinimumSize(620, 420);
    d.setStyleSheet(
        "QDialog{background:#141417;}"
        "QLabel{color:#fafafa;}"
        "QLabel#title{font-size:18px; font-weight:700;}"
        "QLabel#muted{color:#a1a1aa; font-size:12px;}"
        "QPushButton{background:#26262b; color:#fafafa; border:none; border-radius:8px; padding:8px 14px; font-weight:700;}"
        "QPushButton:hover{background:#3a3a40;}"
        "QPushButton#primary{background:#6366f1;} QPushButton#primary:hover{background:#818cf8;}"
        "QFrame#thumb{background:#17171a; border:2px solid #26262b; border-radius:12px;}"
        "QFrame#thumb[selected=\"true\"]{border-color:#6366f1;}"
    );
    auto *root = new QVBoxLayout(&d);
    root->setContentsMargins(20, 18, 20, 16);
    root->setSpacing(10);
    auto *title = new QLabel(QString::fromUtf8("Paylaşılacak Ekranı Seç"));
    title->setObjectName("title");
    root->addWidget(title);
    auto *hint = new QLabel(QString::fromUtf8("Hangi monitör kanaldaki diğer kişilerce görüntülensin?"));
    hint->setObjectName("muted");
    root->addWidget(hint);

    auto *grid = new QGridLayout();
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(12);
    int selectedIndex = (settingsScreenIndex >= 0 && settingsScreenIndex < screens.size())
        ? settingsScreenIndex : 0;
    QList<QFrame*> thumbs;
    for (int i = 0; i < screens.size(); ++i) {
        QScreen *sc = screens.at(i);
        QFrame *thumb = new QFrame();
        thumb->setObjectName("thumb");
        thumb->setAttribute(Qt::WA_StyledBackground, true);
        thumb->setProperty("selected", i == selectedIndex);
        thumb->setCursor(Qt::PointingHandCursor);
        thumb->setMinimumSize(260, 170);
        auto *tl = new QVBoxLayout(thumb);
        tl->setContentsMargins(10, 10, 10, 10);
        tl->setSpacing(6);
        auto *preview = new QLabel();
        preview->setAlignment(Qt::AlignCenter);
        preview->setStyleSheet("background:#0a0a0b; border-radius:8px;");
        preview->setMinimumHeight(120);
        QPixmap pm = sc->grabWindow(0);
        if (!pm.isNull()) preview->setPixmap(pm.scaled(240, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        tl->addWidget(preview, 1);
        const QRect g = sc->geometry();
        auto *nm = new QLabel(QString("%1 — %2×%3").arg(
            sc->name().isEmpty() ? QString("Ekran %1").arg(i+1) : sc->name()).arg(g.width()).arg(g.height()));
        nm->setStyleSheet("font-weight:700;");
        tl->addWidget(nm);
        grid->addWidget(thumb, i / 2, i % 2);
        thumbs.append(thumb);
    }
    root->addLayout(grid, 1);

    class Picker : public QObject {
    public:
        QList<QFrame*> thumbs;
        int *selected;
        Picker(QList<QFrame*> t, int *s, QObject *p) : QObject(p), thumbs(t), selected(s) {}
        bool eventFilter(QObject *obj, QEvent *ev) override {
            if (ev->type() == QEvent::MouseButtonPress) {
                for (int i = 0; i < thumbs.size(); ++i) {
                    if (thumbs[i] == obj) {
                        *selected = i;
                        for (int j = 0; j < thumbs.size(); ++j) {
                            thumbs[j]->setProperty("selected", j == i);
                            thumbs[j]->style()->unpolish(thumbs[j]);
                            thumbs[j]->style()->polish(thumbs[j]);
                        }
                        return true;
                    }
                }
            }
            return false;
        }
    };
    Picker *picker = new Picker(thumbs, &selectedIndex, &d);
    for (QFrame *t : thumbs) t->installEventFilter(picker);

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto *cancel = new QPushButton(QString::fromUtf8("Vazgeç"));
    auto *startBtn = new QPushButton(QString::fromUtf8("Paylaşımı Başlat"));
    startBtn->setObjectName("primary");
    btnRow->addWidget(cancel);
    btnRow->addWidget(startBtn);
    root->addLayout(btnRow);
    connect(cancel, &QPushButton::clicked, &d, &QDialog::reject);
    connect(startBtn, &QPushButton::clicked, &d, &QDialog::accept);

    if (d.exec() != QDialog::Accepted) return false;
    settingsScreenIndex = selectedIndex;
    saveSettings();
    return true;
}

// ============================================================
//          Discord-benzeri: sunucu / kanal / DM UI
// ============================================================

void MainWindow::clearChatArea() {
    if (!chatLayout) return;
    QLayoutItem *it;
    while ((it = chatLayout->takeAt(0)) != nullptr) {
        if (it->widget()) it->widget()->deleteLater();
        delete it;
    }
    chatLayout->addStretch();
    chatRowsByMessageId.clear();
    chatBodyByMessageId.clear();
}

// Mesaj içeriğini zengin HTML'e çevir: code block (```...```), inline code (`...`),
// URL'leri tıklanabilir link, @mention'ları vurgu rengi.
static QString formatMessageHtml(const QString &content, const QString &currentUser) {
    // Önce HTML escape
    QString s = content.toHtmlEscaped();

    // 1) Triple-backtick code blocks
    {
        QRegularExpression re(QStringLiteral("```([\\s\\S]*?)```"));
        QString out;
        int last = 0;
        auto it = re.globalMatch(s);
        while (it.hasNext()) {
            auto m = it.next();
            out += s.mid(last, m.capturedStart() - last);
            QString code = m.captured(1);
            out += QStringLiteral("<pre style='background:#0a0a0b;border:1px solid #26262b;"
                                  "border-radius:8px;padding:10px 12px;margin:6px 0;"
                                  "color:#cde0ff;font-family:Consolas,monospace;font-size:12.5px;"
                                  "white-space:pre-wrap;'>") + code + QStringLiteral("</pre>");
            last = m.capturedEnd();
        }
        out += s.mid(last);
        s = out;
    }

    // 2) Inline `code`
    {
        QRegularExpression re(QStringLiteral("`([^`\\n]+?)`"));
        s.replace(re, QStringLiteral("<code style='background:#0a0a0b;color:#ffd07a;"
                                     "padding:1px 6px;border-radius:4px;border:1px solid #26262b;"
                                     "font-family:Consolas,monospace;font-size:12.5px;'>\\1</code>"));
    }

    // 3) URLs → tıklanabilir link
    {
        QRegularExpression re(QStringLiteral("\\b(https?://[^\\s<]+)"));
        s.replace(re, QStringLiteral("<a href='\\1' style='color:#7fb3ff;text-decoration:none;'>\\1</a>"));
    }

    // 4) @mentions
    {
        QRegularExpression re(QStringLiteral("@([A-Za-z0-9_]{2,32})"));
        const QString meEsc = currentUser.toHtmlEscaped();
        QString out;
        int last = 0;
        auto it = re.globalMatch(s);
        while (it.hasNext()) {
            auto m = it.next();
            out += s.mid(last, m.capturedStart() - last);
            const QString user = m.captured(1);
            const bool isMe = (!meEsc.isEmpty() && user.compare(meEsc, Qt::CaseInsensitive) == 0);
            const QString bg = isMe ? "rgba(255,180,80,0.18)" : "rgba(43,109,245,0.20)";
            const QString fg = isMe ? "#ffd07a" : "#9ec5ff";
            out += QStringLiteral("<span style='background:%1;color:%2;padding:1px 6px;"
                                  "border-radius:4px;font-weight:600;'>@%3</span>")
                       .arg(bg, fg, user);
            last = m.capturedEnd();
        }
        out += s.mid(last);
        s = out;
    }

    // 5) Newline → <br>
    s.replace(QStringLiteral("\n"), QStringLiteral("<br>"));
    return s;
}

void MainWindow::appendRichMessage(const QString &userName, const QDateTime &when,
                                   const QString &content, bool edited,
                                   qint64 messageId, bool canManage, bool isDm) {
    if (!chatLayout) return;
    const QString ts = when.isValid() ? when.toLocalTime().toString("HH:mm") : createTimeStamp().left(5);

    QWidget *row = new QWidget();
    row->setObjectName("msgRow");
    row->setAttribute(Qt::WA_StyledBackground, true);
    // Stil chatContainer QSS'inden geliyor; per-message setStyleSheet kaldırıldı.
    QHBoxLayout *rowLay = new QHBoxLayout(row);
    rowLay->setContentsMargins(6, 4, 6, 4);
    rowLay->setSpacing(10);
    rowLay->setAlignment(Qt::AlignTop);

    // Avatar — sade renkli daire. Drop shadow kaldırıldı (her mesaj için
    // QGraphicsDropShadowEffect = software repaint, listeyi yavaşlatıyordu).
    QLabel *avatar = new QLabel(avatarInitials(userName));
    avatar->setFixedSize(38, 38);
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setAttribute(Qt::WA_StyledBackground, true);
    // Renk dinamik (kullanıcı adına göre) → tek inline tanım, ek pseudo-state yok.
    avatar->setStyleSheet(QString(
        "background:%1; color:#fff; border-radius:19px;"
        " font-weight:800; font-size:13.5px;"
        " border:1px solid rgba(255,255,255,0.10);"
    ).arg(avatarColor(userName)));
    rowLay->addWidget(avatar, 0, Qt::AlignTop);

    // Text column — stil container'dan geliyor, setStyleSheet yok.
    QWidget *textCol = new QWidget();
    QVBoxLayout *textLay = new QVBoxLayout(textCol);
    textLay->setContentsMargins(0, 0, 0, 0);
    textLay->setSpacing(3);

    QLabel *header = new QLabel(QString(
        "<span style='color:#fafafa;font-weight:800;font-size:13.5px;'>%1</span>"
        "  <span style='color:#a1a1aa;font-size:11px;'>%2</span>")
        .arg(userName.toHtmlEscaped(), ts.toHtmlEscaped()));
    header->setTextFormat(Qt::RichText);
    textLay->addWidget(header);

    // Resim eki desteği — içerik "[[img:b64]]" ile başlıyorsa QPixmap olarak göster
    QString textBody = content;
    if (content.startsWith(QStringLiteral("[[img:"))) {
        const int endIdx = content.indexOf(QStringLiteral("]]"));
        if (endIdx > 6) {
            const QString b64 = content.mid(6, endIdx - 6);
            const QByteArray jpegBytes = QByteArray::fromBase64(b64.toLatin1());
            QPixmap pm;
            if (pm.loadFromData(jpegBytes, "JPG") && !pm.isNull()) {
                // Max 360x360 thumbnail (gerçek resmi tıklayınca aç)
                QPixmap scaled = pm.scaled(360, 360, Qt::KeepAspectRatio,
                                           Qt::SmoothTransformation);
                QLabel *imgLbl = new QLabel();
                imgLbl->setObjectName("msgImage");  // stil container'dan gelir
                imgLbl->setPixmap(scaled);
                imgLbl->setCursor(Qt::PointingHandCursor);
                imgLbl->setToolTip(QString::fromUtf8("Tam boyut için tıkla"));
                imgLbl->installEventFilter(this);
                imgLbl->setProperty("fullPixmap", QVariant::fromValue(pm));
                textLay->addWidget(imgLbl);
                textBody = content.mid(endIdx + 2).trimmed();
            }
        }
    }

    QLabel *body = new QLabel();
    body->setObjectName("msgBody");  // stil container'dan gelir
    body->setWordWrap(true);
    body->setTextFormat(Qt::RichText);
    body->setOpenExternalLinks(true);
    body->setTextInteractionFlags(Qt::TextBrowserInteraction);
    QString bodyHtml = formatMessageHtml(textBody, currentUserName);
    if (edited) bodyHtml += QString::fromUtf8("  <span style='color:#6d7a99;font-size:10.5px;font-style:italic;'>(düzenlendi)</span>");
    body->setText(bodyHtml);
    if (!textBody.isEmpty() || !bodyHtml.isEmpty()) textLay->addWidget(body);
    else body->hide();

    rowLay->addWidget(textCol, 1);

    // Hover toolbar — sağda. messageId>0 ise göster.
    if (messageId > 0) {
        const bool isOwn = (userName == currentUserName);
        QWidget *tools = new QWidget(row);
        tools->setObjectName("msgTools");  // stil container'dan gelir
        tools->setAttribute(Qt::WA_StyledBackground, true);
        auto *tLay = new QHBoxLayout(tools);
        tLay->setContentsMargins(4, 2, 4, 2);
        tLay->setSpacing(0);

        auto mkBtn = [&](const QString &gly, const QString &tip){
            auto *b = new QToolButton();
            b->setText(gly);
            b->setToolTip(tip);
            b->setCursor(Qt::PointingHandCursor);
            tLay->addWidget(b);
            return b;
        };
        const QString contentCopy = content;
        QToolButton *bCopy = mkBtn(QString::fromUtf8("⧉"), QString::fromUtf8("Kopyala"));
        connect(bCopy, &QToolButton::clicked, this, [this, contentCopy](){
            QApplication::clipboard()->setText(contentCopy);
            showToast(QString::fromUtf8("Mesaj kopyalandı"), "info", 1400);
        });
        if (isOwn) {
            QToolButton *bEdit = mkBtn(QString::fromUtf8("✎"), QString::fromUtf8("Düzenle"));
            connect(bEdit, &QToolButton::clicked, this, [this, messageId, contentCopy, isDm](){
                const QString nv = promptInput(
                    QString::fromUtf8("Mesajı Düzenle"),
                    QString::fromUtf8("Yeni içeriği gir."),
                    contentCopy, QString::fromUtf8("Mesaj..."), QString::fromUtf8("✎"));
                if (nv.isEmpty() || nv == contentCopy) return;
                if (isDm) signalingClient->editDm(messageId, nv);
                else     signalingClient->editChannelMessage(messageId, nv);
            });
        }
        if (isOwn || canManage) {
            QToolButton *bDel = mkBtn(QString::fromUtf8("🗑"), QString::fromUtf8("Sil"));
            connect(bDel, &QToolButton::clicked, this, [this, messageId, isDm](){
                if (promptConfirm(QString::fromUtf8("Mesajı Sil"),
                                  QString::fromUtf8("Bu mesaj silinsin mi?"),
                                  QString::fromUtf8("Sil"), QString::fromUtf8("Vazgeç"), true)) {
                    if (isDm) signalingClient->deleteDm(messageId);
                    else     signalingClient->deleteChannelMessage(messageId);
                }
            });
        }

        // Toolbar varsayılan gizli, hover'da göster.
        tools->hide();
        tools->setProperty("isMsgTools", true);
        // Sağ-üst köşeye floating konumlandır
        auto reposition = [tools, row]() {
            tools->adjustSize();
            tools->move(row->width() - tools->width() - 10, -10);
            tools->raise();
        };
        // Initial size
        QTimer::singleShot(0, tools, reposition);
        // Hover handling: row üzerine eventFilter bağlamak yerine basit yaklaşım
        row->installEventFilter(this);
        row->setProperty("hoverTools", QVariant::fromValue<QWidget*>(tools));
        // Yeniden konumlandırma için rowResize
        row->setProperty("toolsReposFn", QVariant());
        // Toolbar dinamik konumlandırma için ayrı bir helper; resize'de tools'ı tekrar yerleştir.
        row->connect(row, &QWidget::destroyed, [](){});  // no-op (tools row'un child'ı, otomatik silinir)
        // Resize signal yok; basit timer ile uyarla
        // (Esas hover/leave eventFilter MainWindow::eventFilter'da işleniyor — aşağıda eklenir.)
    }

    // Context menu (sağ tık): own ise kopyala/düzenle/sil; admin ise kopyala/sil
    if (messageId > 0) {
        const bool isOwn = (userName == currentUserName);
        row->setContextMenuPolicy(Qt::CustomContextMenu);
        const QString contentCopy = content;
        connect(row, &QWidget::customContextMenuRequested, this,
                [this, messageId, isOwn, canManage, contentCopy, isDm](const QPoint &) {
            QMenu m(this);
            m.setStyleSheet("QMenu{background:#141823;color:#fafafa;border:1px solid #26262b;padding:4px;}"
                            "QMenu::item{padding:6px 12px;border-radius:4px;}"
                            "QMenu::item:selected{background:#6366f1;}");
            QAction *aCopy = m.addAction(QString::fromUtf8("Kopyala"));
            QAction *aEdit = nullptr, *aDel = nullptr;
            if (isOwn) aEdit = m.addAction(QString::fromUtf8("Düzenle"));
            if (isOwn || canManage) aDel = m.addAction(QString::fromUtf8("Sil"));
            QAction *sel = m.exec(QCursor::pos());
            if (!sel) return;
            if (sel == aCopy) {
                QApplication::clipboard()->setText(contentCopy);
                showToast(QString::fromUtf8("Mesaj kopyalandı"), "info", 1500);
            } else if (aEdit && sel == aEdit) {
                const QString nv = promptInput(
                    QString::fromUtf8("Mesajı Düzenle"),
                    QString::fromUtf8("Yeni içeriği gir."),
                    contentCopy, QString::fromUtf8("Mesaj..."), QString::fromUtf8("✎"));
                if (nv.isEmpty() || nv == contentCopy) return;
                if (isDm) signalingClient->editDm(messageId, nv);
                else     signalingClient->editChannelMessage(messageId, nv);
            } else if (aDel && sel == aDel) {
                if (promptConfirm(QString::fromUtf8("Mesajı Sil"),
                                  QString::fromUtf8("Bu mesaj silinsin mi?"),
                                  QString::fromUtf8("Sil"), QString::fromUtf8("Vazgeç"), true)) {
                    if (isDm) signalingClient->deleteDm(messageId);
                    else     signalingClient->deleteChannelMessage(messageId);
                }
            }
        });
        chatRowsByMessageId[messageId] = row;
        chatBodyByMessageId[messageId] = body;
    }

    // Insert before the stretch (last item)
    const int insertAt = chatLayout->count() - 1;
    chatLayout->insertWidget(insertAt < 0 ? 0 : insertAt, row);

    // Auto-scroll to bottom
    if (chatScroll && chatScroll->verticalScrollBar()) {
        QTimer::singleShot(0, chatScroll, [this]() {
            chatScroll->verticalScrollBar()->setValue(chatScroll->verticalScrollBar()->maximum());
        });
    }
}

void MainWindow::removeChatRow(qint64 messageId) {
    auto it = chatRowsByMessageId.find(messageId);
    if (it == chatRowsByMessageId.end()) return;
    if (it.value()) it.value()->deleteLater();
    chatRowsByMessageId.erase(it);
    chatBodyByMessageId.remove(messageId);
}

void MainWindow::updateChatRow(qint64 messageId, const QString &newContent, bool edited) {
    auto it = chatBodyByMessageId.find(messageId);
    if (it == chatBodyByMessageId.end()) return;
    QString html = formatMessageHtml(newContent, currentUserName);
    if (edited) html += QString::fromUtf8("  <span style='color:#6d7a99;font-size:10.5px;font-style:italic;'>(düzenlendi)</span>");
    if (it.value()) it.value()->setText(html);
}

void MainWindow::editChannelMessagePrompt(qint64 messageId, const QString &currentContent) {
    const QString nv = promptInput(
        QString::fromUtf8("Mesajı Düzenle"),
        QString::fromUtf8("Yeni içeriği gir."),
        currentContent,
        QString::fromUtf8("Mesaj..."),
        QString::fromUtf8("✎"));
    if (nv.isEmpty() || nv == currentContent) return;
    signalingClient->editChannelMessage(messageId, nv);
}

void MainWindow::rebuildServerSidebar() {
    if (!serverSidebarLayout) return;
    // Mevcut öğeleri sil
    QLayoutItem *it;
    while ((it = serverSidebarLayout->takeAt(0)) != nullptr) {
        if (it->widget()) it->widget()->deleteLater();
        delete it;
    }
    // Sunucu butonları
    for (auto it2 = serversById.constBegin(); it2 != serversById.constEnd(); ++it2) {
        const QJsonObject &s = it2.value();
        const int sid = s.value("id").toInt();
        const QString name = s.value("name").toString();
        const bool active = (sid == currentServerId);
        const int unread = unreadForServer(sid);

        // Sarmalayıcı: solunda pill indicator (active/hover göstergesi) için yer.
        QWidget *wrap = new QWidget();
        wrap->setFixedSize(64, 56);
        wrap->setStyleSheet("background:transparent;");
        wrap->setProperty("serverId", sid);

        // Pill indicator (sol kenarda) — active=12px, hover=8px, idle=0px
        QWidget *pill = new QWidget(wrap);
        pill->setObjectName("pill");
        pill->setStyleSheet(
            "background:qlineargradient(x1:0,y1:0,x2:0,y2:1,"
            " stop:0 #ffffff, stop:1 #cfd9f3); border-top-right-radius:4px;"
            " border-bottom-right-radius:4px;");
        const int pillH = active ? 28 : (unread > 0 ? 16 : 0);
        pill->setGeometry(0, (56 - pillH) / 2, 4, pillH);

        QToolButton *btn = new QToolButton(wrap);
        btn->setFixedSize(48, 48);
        btn->move(8, 4);
        btn->setCursor(Qt::PointingHandCursor);
        QString initials = name.isEmpty() ? QString("?") : name.left(2).toUpper();
        btn->setText(initials);
        btn->setToolTip(name + QString::fromUtf8("\nDavet: ") + s.value("inviteCode").toString());
        if (active) {
            btn->setStyleSheet(
                "QToolButton{background:qlineargradient(x1:0,y1:0,x2:1,y2:1,"
                " stop:0 #6366f1, stop:1 #a855f7); color:#ffffff;"
                " border:1px solid rgba(255,255,255,0.20); border-radius:16px;"
                " font-weight:800; font-size:14px;}"
                "QToolButton:hover{border-radius:14px;}");
            auto *glow = new QGraphicsDropShadowEffect(btn);
            glow->setBlurRadius(22); glow->setOffset(0, 4);
            glow->setColor(QColor(43, 109, 245, 160));
            btn->setGraphicsEffect(glow);
        } else {
            btn->setStyleSheet(
                "QToolButton{background:#1a1a1d; color:#dde5f5;"
                " border:1px solid rgba(255,255,255,0.06); border-radius:24px;"
                " font-weight:800; font-size:14px;}"
                "QToolButton:hover{background:qlineargradient(x1:0,y1:0,x2:1,y2:1,"
                " stop:0 #6366f1, stop:1 #818cf8); color:#ffffff;"
                " border-radius:16px; border:1px solid rgba(255,255,255,0.15);}");
        }
        connect(btn, &QToolButton::clicked, this, [this, sid]() { selectServer(sid); });
        // Sağ tık menüsü: kopyala/ayrıl/sil
        btn->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(btn, &QToolButton::customContextMenuRequested, this, [this, sid, s](const QPoint &) {
            QMenu m(this);
            m.setStyleSheet("QMenu{background:#141823;color:#fafafa;border:1px solid #26262b;padding:4px;}"
                            "QMenu::item{padding:6px 12px;border-radius:4px;}"
                            "QMenu::item:selected{background:#6366f1;}");
            QAction *actCopy  = m.addAction(QString::fromUtf8("Davet kodunu kopyala"));
            QAction *actInv   = m.addAction(QString::fromUtf8("Davet kodunu göster"));
            m.addSeparator();
            const QString role = s.value("role").toString();
            QAction *actLeave = nullptr, *actDelete = nullptr, *actRename = nullptr;
            if (role == "owner") {
                actRename = m.addAction(QString::fromUtf8("Sunucuyu yeniden adlandır"));
                actDelete = m.addAction(QString::fromUtf8("Sunucuyu sil"));
            } else {
                actLeave  = m.addAction(QString::fromUtf8("Sunucudan ayrıl"));
            }
            QAction *sel = m.exec(QCursor::pos());
            if (!sel) return;
            if (sel == actCopy) {
                QApplication::clipboard()->setText(s.value("inviteCode").toString());
                showToast(QString::fromUtf8("Davet kodu panoya kopyalandı"), "success", 1800);
            } else if (sel == actInv) {
                showInviteCodeDialog(sid);
            } else if (sel == actLeave) {
                if (promptConfirm(
                        QString::fromUtf8("Sunucudan Ayrıl"),
                        QString::fromUtf8("%1 sunucusundan ayrılmak istediğine emin misin? Mesaj geçmişine erişimin sona erer.").arg(s.value("name").toString()),
                        QString::fromUtf8("Ayrıl"),
                        QString::fromUtf8("İptal"),
                        true)) {
                    signalingClient->leaveServer(sid);
                }
            } else if (sel == actDelete) {
                if (promptConfirm(
                        QString::fromUtf8("Sunucuyu Sil"),
                        QString::fromUtf8("%1 sunucusunu kalıcı olarak silmek üzeresin. Bu işlem geri alınamaz; tüm kanallar ve mesajlar kaybolur.").arg(s.value("name").toString()),
                        QString::fromUtf8("Kalıcı Olarak Sil"),
                        QString::fromUtf8("Vazgeç"),
                        true)) {
                    signalingClient->deleteServer(sid);
                }
            } else if (sel == actRename) {
                const QString nm = promptInput(
                    QString::fromUtf8("Sunucuyu Yeniden Adlandır"),
                    QString::fromUtf8("Sunucuya yeni bir isim ver. 2–40 karakter arasında olmalı."),
                    s.value("name").toString(),
                    QString::fromUtf8("Örn: Arkadaşlar"),
                    QString::fromUtf8("✎"));
                if (!nm.isEmpty()) signalingClient->renameServer(sid, nm);
            }
        });
        // Unread badge — sağ-üstte, gradient+glow
        if (unread > 0 && !active) {
            QLabel *badge = new QLabel(wrap);
            badge->setText(unread > 99 ? QString("99+") : QString::number(unread));
            badge->setAlignment(Qt::AlignCenter);
            badge->setAttribute(Qt::WA_StyledBackground, true);
            badge->setStyleSheet(
                "QLabel{background:qlineargradient(x1:0,y1:0,x2:1,y2:1,"
                " stop:0 #ff5577, stop:1 #ff8aa3); color:white;"
                " border:2px solid #0c0f16; border-radius:9px;"
                " font-size:10px; font-weight:800; padding:0;}");
            const int w = unread > 9 ? 24 : 18;
            // btn rect: (8, 4) - 48x48 → top-right ≈ (54, 0)
            badge->setGeometry(56 - w, 0, w, 18);
            auto *bGlow = new QGraphicsDropShadowEffect(badge);
            bGlow->setBlurRadius(14); bGlow->setOffset(0, 2);
            bGlow->setColor(QColor(231, 76, 109, 200));
            badge->setGraphicsEffect(bGlow);
        }

        serverSidebarLayout->addWidget(wrap, 0, Qt::AlignHCenter);
    }
    serverSidebarLayout->addStretch();
}

void MainWindow::rebuildChannelList() {
    if (!channelListLayout || !channelListPanel) return;
    // Eski öğeleri temizle
    QLayoutItem *it;
    while ((it = channelListLayout->takeAt(0)) != nullptr) {
        if (it->widget()) it->widget()->deleteLater();
        delete it;
    }
    if (currentServerId <= 0 || !channelsByServer.contains(currentServerId)) {
        channelListPanel->hide();
        return;
    }
    const auto &list = channelsByServer.value(currentServerId);
    if (list.isEmpty()) {
        auto *empty = new QLabel(QString::fromUtf8("Henüz kanal yok."));
        empty->setStyleSheet("color:#6b7690; font-size:12px; font-style:italic; padding:6px 8px;");
        channelListLayout->addWidget(empty);
        channelListPanel->show();
        return;
    }
    const QString srvRole = serversById.value(currentServerId).value("role").toString();
    const bool canManage = (srvRole == "owner" || srvRole == "admin");

    for (const auto &c : list) {
        const int chId = c.value("id").toInt();
        const QString nm = c.value("name").toString();
        const QString ty = c.value("type").toString();
        const bool isSelected = (chId == currentChannelId);
        const QString prefix = (ty == "voice") ? QString::fromUtf8("🔊  ") : QString("#  ");

        const int unread = unreadByChannel.value(chId, 0);
        const bool unreadBold = (unread > 0 && !isSelected);

        auto *rowW = new QWidget();
        rowW->setAttribute(Qt::WA_StyledBackground, true);
        rowW->setFixedHeight(32);
        auto *rowLay = new QHBoxLayout(rowW);
        rowLay->setContentsMargins(0, 0, 0, 0);
        rowLay->setSpacing(6);

        auto *btn = new QPushButton(prefix + nm);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedHeight(32);
        btn->setContextMenuPolicy(Qt::CustomContextMenu);
        const QString baseCss =
            "QPushButton{background:transparent; color:#a6b2cf; border:none;"
            " padding:4px 10px; text-align:left; border-radius:6px; font-weight:600;}"
            "QPushButton:hover{background:#17171a; color:#fafafa;}";
        const QString selCss =
            "QPushButton{background:#1f2738; color:#ffffff; border:none;"
            " padding:4px 10px; text-align:left; border-radius:6px; font-weight:700;}";
        const QString unreadCss =
            "QPushButton{background:transparent; color:#ffffff; border:none;"
            " padding:4px 10px; text-align:left; border-radius:6px; font-weight:800;}"
            "QPushButton:hover{background:#17171a;}";
        btn->setStyleSheet(isSelected ? selCss : (unreadBold ? unreadCss : baseCss));
        connect(btn, &QPushButton::clicked, this, [this, chId]() { selectChannel(chId); });
        rowLay->addWidget(btn, 1);

        if (unreadBold) {
            auto *badge = new QLabel(unread > 99 ? QString("99+") : QString::number(unread));
            badge->setAlignment(Qt::AlignCenter);
            badge->setAttribute(Qt::WA_StyledBackground, true);
            badge->setStyleSheet(
                "background:#ef4444; color:white; border-radius:9px;"
                " font-size:10px; font-weight:800; padding:0 6px;");
            badge->setFixedHeight(18);
            badge->setMinimumWidth(unread > 9 ? 26 : 18);
            rowLay->addWidget(badge, 0, Qt::AlignVCenter);
            rowLay->addSpacing(4);
        }

        // Kanal bağlam menüsü (rename / delete - yetkili ise)
        if (canManage) {
            connect(btn, &QPushButton::customContextMenuRequested, this, [this, chId, nm](const QPoint &) {
                QMenu m(this);
                m.setStyleSheet("QMenu{background:#141823;color:#fafafa;border:1px solid #26262b;padding:4px;}"
                                "QMenu::item{padding:6px 12px;border-radius:4px;}"
                                "QMenu::item:selected{background:#6366f1;}");
                QAction *aRen = m.addAction(QString::fromUtf8("Yeniden adlandır"));
                QAction *aDel = m.addAction(QString::fromUtf8("Sil"));
                QAction *sel = m.exec(QCursor::pos());
                if (!sel) return;
                if (sel == aRen) {
                    const QString nv = promptInput(
                        QString::fromUtf8("Kanalı Yeniden Adlandır"),
                        QString::fromUtf8("Kanala yeni bir isim ver."),
                        nm, QString::fromUtf8("yeni-isim"), QString::fromUtf8("✎"));
                    if (!nv.isEmpty()) signalingClient->renameChannel(chId, nv);
                } else if (sel == aDel) {
                    if (promptConfirm(
                            QString::fromUtf8("Kanalı Sil"),
                            QString::fromUtf8("%1 kanalını silmek istiyor musun? Bu işlem geri alınamaz.").arg(nm),
                            QString::fromUtf8("Sil"), QString::fromUtf8("Vazgeç"), true)) {
                        signalingClient->deleteChannel(chId);
                    }
                }
            });
        }
        channelListLayout->addWidget(rowW);
    }
    channelListPanel->show();
}

void MainWindow::selectServer(int serverId) {
    if (!serversById.contains(serverId)) return;
    currentServerId = serverId;
    currentChannelId = 0;
    currentDmPeer.clear();
    chatMode = ChatMode::Channel;
    // Başlığı güncelle
    const QString name = serversById.value(serverId).value("name").toString();
    if (roomTitleLabel) roomTitleLabel->setText(name);
    if (dmActionBar) dmActionBar->hide();
    hideVoicePanel();
    clearChatArea();
    appendSystemMessage(QString::fromUtf8("Bir kanal seç") + QString::fromUtf8("erek sohbete başlayabilirsin."));
    // Kanalları çek
    signalingClient->listChannels(serverId);
    signalingClient->listMembers(serverId);
    rebuildServerSidebar();
    rebuildChannelList();
    if (membersPanel) membersPanel->show();
    rebuildMembersPanel();
}

void MainWindow::selectChannel(int channelId) {
    if (!channelsById.contains(channelId)) return;
    const QJsonObject &ch = channelsById.value(channelId);
    currentChannelId = channelId;
    chatMode = ChatMode::Channel;
    currentDmPeer.clear();
    if (dmActionBar) dmActionBar->hide();
    const QString chName = ch.value("name").toString();
    const QString type = ch.value("type").toString();
    if (roomTitleLabel) {
        const QString srvName = serversById.value(currentServerId).value("name").toString();
        roomTitleLabel->setText(srvName + "  >  " + (type == "voice" ? QString::fromUtf8("🔊 ") : "#") + chName);
    }
    clearChatArea();
    clearUnreadChannel(channelId);
    activeTypers.clear();
    refreshTypingLabel();
    if (type == "voice") {
        showVoicePanel(channelId);
        rebuildChannelList();
        return;
    }
    hideVoicePanel();
    // Mesajları yükle
    signalingClient->listChannelMessages(channelId);
    rebuildChannelList();
}

void MainWindow::selectDm(const QString &peerUsername) {
    currentDmPeer = peerUsername;
    currentServerId = 0;
    currentChannelId = 0;
    chatMode = ChatMode::Dm;
    if (roomTitleLabel) roomTitleLabel->setText("@" + peerUsername);
    hideVoicePanel();
    if (channelListPanel) channelListPanel->hide();
    if (membersPanel) membersPanel->hide();
    if (dmActionBar) dmActionBar->show();
    clearChatArea();
    clearUnreadDm(peerUsername);
    activeTypers.clear();
    refreshTypingLabel();
    signalingClient->listDmMessages(peerUsername);
    signalingClient->markDmRead(peerUsername);
}

// ===================== Server / Channel / DM slots =====================

void MainWindow::onServersListed(const QJsonArray &servers) {
    serversById.clear();
    for (const auto &v : servers) {
        const QJsonObject s = v.toObject();
        serversById[s.value("id").toInt()] = s;
    }
    rebuildServerSidebar();
}

void MainWindow::onServerCreated(const QJsonObject &server) {
    const int sid = server.value("id").toInt();
    serversById[sid] = server;
    rebuildServerSidebar();
    const QString name = server.value("name").toString();
    const QString code = server.value("inviteCode").toString();
    appendSystemMessage(QString::fromUtf8("Sunucu oluşturuldu: ") + name
        + QString::fromUtf8("  (davet kodu: ") + code + ")");
    showToast(QString::fromUtf8("Sunucu oluşturuldu: %1").arg(name), "success", 2500);
    selectServer(sid);
    // Davet kodunu hemen göster — kullanıcı kopyalayıp arkadaşına gönderebilsin.
    if (!code.isEmpty()) {
        QTimer::singleShot(250, this, [this, sid]() { showInviteCodeDialog(sid); });
    }
}

void MainWindow::onServerJoined(const QJsonObject &server) {
    const int sid = server.value("id").toInt();
    QJsonObject enriched = server;
    if (!enriched.contains("role")) enriched["role"] = "member";
    serversById[sid] = enriched;
    rebuildServerSidebar();
    const QString name = server.value("name").toString();
    appendSystemMessage(QString::fromUtf8("Sunucuya katıldın: ") + name);
    showToast(QString::fromUtf8("Sunucuya katıldın: %1").arg(name), "success", 2500);
    selectServer(sid);
}

void MainWindow::onServerLeft(int serverId) {
    serversById.remove(serverId);
    channelsByServer.remove(serverId);
    if (currentServerId == serverId) { currentServerId = 0; currentChannelId = 0; chatMode = ChatMode::Idle; clearChatArea(); }
    rebuildServerSidebar();
}

void MainWindow::onServerDeleted(int serverId) {
    onServerLeft(serverId);
    appendSystemMessage(QString::fromUtf8("Sunucu silindi."));
}

void MainWindow::onServerRenamed(int serverId, const QString &name) {
    if (!serversById.contains(serverId)) return;
    QJsonObject s = serversById[serverId];
    s["name"] = name;
    serversById[serverId] = s;
    rebuildServerSidebar();
    if (currentServerId == serverId && roomTitleLabel) roomTitleLabel->setText(name);
}

void MainWindow::onChannelsListed(int serverId, const QJsonArray &channels) {
    QList<QJsonObject> list;
    // Bu sunucuya ait eski kanalları channelsById'den temizle
    if (channelsByServer.contains(serverId)) {
        for (const auto &c : channelsByServer.value(serverId))
            channelsById.remove(c.value("id").toInt());
    }
    for (const auto &v : channels) {
        const QJsonObject c = v.toObject();
        list.append(c);
        channelsById[c.value("id").toInt()] = c;
    }
    channelsByServer[serverId] = list;
    if (currentServerId == serverId) {
        rebuildChannelList();
        // İlk text kanalı otomatik seç (zaten kanal yoksa veya sçme yapilmadıysa)
        if (currentChannelId <= 0) {
            for (const auto &c : list) {
                if (c.value("type").toString() == "text") {
                    selectChannel(c.value("id").toInt());
                    break;
                }
            }
        }
    }
}

void MainWindow::onChannelCreated(int serverId, const QJsonObject &channel) {
    channelsByServer[serverId].append(channel);
    channelsById[channel.value("id").toInt()] = channel;
    if (currentServerId == serverId) {
        rebuildChannelList();
        const QString t = channel.value("type").toString();
        showToast(QString::fromUtf8("Yeni kanal: ")
            + (t == "voice" ? QString::fromUtf8("🔊 ") : "#")
            + channel.value("name").toString(), "success", 2400);
    }
}

void MainWindow::onChannelDeleted(int serverId, int channelId) {
    auto &list = channelsByServer[serverId];
    for (int i = 0; i < list.size(); ++i)
        if (list[i].value("id").toInt() == channelId) { list.removeAt(i); break; }
    channelsById.remove(channelId);
    if (currentChannelId == channelId) {
        currentChannelId = 0;
        clearChatArea();
        hideVoicePanel();
        showToast(QString::fromUtf8("Kanal silindi."), "info");
    }
    if (currentServerId == serverId) rebuildChannelList();
}

void MainWindow::onChannelRenamed(int serverId, int channelId, const QString &name) {
    if (channelsById.contains(channelId)) {
        QJsonObject c = channelsById[channelId];
        c["name"] = name;
        channelsById[channelId] = c;
    }
    auto &list = channelsByServer[serverId];
    for (auto &c : list) if (c.value("id").toInt() == channelId) c["name"] = name;
    if (currentServerId == serverId) rebuildChannelList();
    if (currentChannelId == channelId) selectChannel(channelId);
}

void MainWindow::onChannelMessagesListed(int channelId, const QJsonArray &messages) {
    if (channelId != currentChannelId) return;
    clearChatArea();
    const QString role = serversById.value(currentServerId).value("role").toString();
    const bool canManage = (role == "owner" || role == "admin");
    for (const auto &v : messages) {
        const QJsonObject m = v.toObject();
        appendRichMessage(
            m.value("username").toString(),
            QDateTime::fromString(m.value("created_at").toString(), Qt::ISODate),
            m.value("content").toString(),
            !m.value("edited_at").isNull() && !m.value("edited_at").toString().isEmpty(),
            (qint64)m.value("id").toDouble(),
            canManage);
    }
}

void MainWindow::onChannelMessageReceived(int serverId, int channelId, const QJsonObject &message) {
    Q_UNUSED(serverId);
    const QString sender = message.value("username").toString();
    if (channelId != currentChannelId) {
        if (sender != currentUserName) bumpUnreadChannel(channelId);
        return;
    }
    const QString role = serversById.value(currentServerId).value("role").toString();
    const bool canManage = (role == "owner" || role == "admin");
    appendRichMessage(
        sender,
        QDateTime::fromString(message.value("created_at").toString(), Qt::ISODate),
        message.value("content").toString(),
        false,
        (qint64)message.value("id").toDouble(),
        canManage);
}

void MainWindow::onChannelMessageDeleted(int serverId, int channelId, qint64 messageId) {
    Q_UNUSED(serverId);
    if (channelId != currentChannelId) return;
    removeChatRow(messageId);
}

void MainWindow::onChannelMessageEdited(int serverId, int channelId, qint64 messageId, const QString &content) {
    Q_UNUSED(serverId);
    if (channelId != currentChannelId) return;
    updateChatRow(messageId, content, true);
}

void MainWindow::onDmThreadsListed(const QJsonArray &threads) {
    // Boot/reconnect senkronu: her peer için unread sayısını state'e yaz
    for (const auto &v : threads) {
        const QJsonObject o = v.toObject();
        const QString peer = o.value("username").toString();
        const int unread = o.value("unreadCount").toInt();
        if (peer.isEmpty()) continue;
        // Şu an açık DM ise sıfırla (sunucuya da mark_dm_read gitmiş olur)
        if (chatMode == ChatMode::Dm && currentDmPeer == peer) {
            unreadByDmPeer.remove(peer);
        } else if (unread > 0) {
            unreadByDmPeer[peer] = unread;
        } else {
            unreadByDmPeer.remove(peer);
        }
    }
    rebuildFriendsSidebar();
}

// E2E DM içerik çözümü — şifreliyse decrypt et, değilse içeriği aynen döndür.
// Çözülemiyorsa (anahtar yok / başka cihaz) açıklayıcı bir placeholder döner.
static QString resolveDmContent(const QJsonObject &m) {
    const bool encrypted = m.value("is_encrypted").toBool();
    const QString content = m.value("content").toString();
    if (!encrypted) return content;
    if (!E2E::isAvailable())
        return QString::fromUtf8("🔒 Şifreli mesaj (bu sürümde okunamıyor)");
    const QString senderPub = m.value("sender_pub").toString();
    const QString nonce     = m.value("nonce").toString();
    QString plaintext;
    if (E2E::decryptFromPeer(senderPub, content, nonce, &plaintext))
        return plaintext;
    return QString::fromUtf8("🔒 Şifreli mesaj (bu cihazda çözülemiyor)");
}

void MainWindow::onDmMessagesListed(const QString &peerUsername, const QJsonArray &messages) {
    if (peerUsername != currentDmPeer) return;
    clearChatArea();
    appendSystemMessage(QString::fromUtf8("DM: @") + peerUsername);
    // Bu konuşma için peer'in pubkey'i henüz cache'te değilse iste
    e2eRequestPeerKey(peerUsername);
    for (const auto &v : messages) {
        const QJsonObject m = v.toObject();
        appendRichMessage(
            m.value("sender_username").toString(),
            QDateTime::fromString(m.value("created_at").toString(), Qt::ISODate),
            resolveDmContent(m),
            !m.value("edited_at").isNull() && !m.value("edited_at").toString().isEmpty(),
            (qint64)m.value("id").toDouble(),
            false,
            true /* isDm */);
    }
}

void MainWindow::onDmReceived(const QJsonObject &message) {
    const QString from = message.value("sender_username").toString();
    const QString to   = message.value("recipient_username").toString();
    const QString peer = (from == currentUserName) ? to : from;
    const QString resolved = resolveDmContent(message);
    if (chatMode == ChatMode::Dm && currentDmPeer == peer) {
        appendRichMessage(from,
            QDateTime::fromString(message.value("created_at").toString(), Qt::ISODate),
            resolved,
            false,
            (qint64)message.value("id").toDouble(),
            false,
            true /* isDm */);
        if (from != currentUserName) signalingClient->markDmRead(peer);
    } else if (from != currentUserName) {
        bumpUnreadDm(from);
        notifyUser(QString::fromUtf8("Yeni mesaj — ") + from,
                   resolved.left(120), "dm");
    }
}

// ===================== Dialoglar =====================

void MainWindow::showServerSetupDialog() {
    if (!isLoggedIn) { appendSystemMessage(QString::fromUtf8("Önce giriş yapmalısın.")); return; }
    QDialog d(this);
    d.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    d.setAttribute(Qt::WA_TranslucentBackground);
    d.setFixedSize(420, 210);  // 460x300 → kompakt: boş alan yok

    QVBoxLayout outer(&d);
    outer.setContentsMargins(0, 0, 0, 0);
    QWidget card(&d);
    card.setObjectName("card");
    card.setStyleSheet(kPrettyDialogQss);
    outer.addWidget(&card);
    QVBoxLayout lay(&card);
    lay.setContentsMargins(22, 14, 16, 16);
    lay.setSpacing(10);

    // Header with close X
    auto *hdrRow = new QHBoxLayout();
    hdrRow->setContentsMargins(0, 0, 0, 0);
    hdrRow->setSpacing(8);
    QLabel *title = new QLabel(QString::fromUtf8("Sunucu"));
    title->setStyleSheet("color:#fafafa; font-size:18px; font-weight:700;");
    hdrRow->addWidget(title);
    hdrRow->addStretch();
    QToolButton *closeX = new QToolButton();
    closeX->setFixedSize(26, 26);
    closeX->setCursor(Qt::PointingHandCursor);
    closeX->setIcon(makeLineIcon("close", QColor("#9aa6c4"), 14));
    closeX->setIconSize(QSize(14, 14));
    closeX->setStyleSheet("QToolButton{background:rgba(255,255,255,0.04); border:none;"
                          " border-radius:13px;}"
                          "QToolButton:hover{background:rgba(231,76,109,0.18);}");
    hdrRow->addWidget(closeX);
    lay.addLayout(hdrRow);

    QLabel *sub = new QLabel(QString::fromUtf8("Yeni bir sunucu oluştur ya da davet koduyla katıl."));
    sub->setWordWrap(true);
    sub->setStyleSheet("color:#8b97b4; font-size:12.5px;");
    lay.addWidget(sub);
    // stretch KALDIRILDI — boş alan yok, butonlar hemen altta

    QHBoxLayout row;
    QPushButton cancelBtn(QString::fromUtf8("Vazgeç"));
    cancelBtn.setObjectName("cancel");
    QPushButton joinBtn(QString::fromUtf8("Katıl"));
    joinBtn.setObjectName("cancel");
    QPushButton createBtn(QString::fromUtf8("Oluştur"));
    row.addStretch();
    row.addWidget(&cancelBtn);
    row.addWidget(&joinBtn);
    row.addWidget(&createBtn);
    lay.addLayout(&row);

    connect(closeX,     &QToolButton::clicked, &d, [&](){ d.done(0); });
    connect(&cancelBtn, &QPushButton::clicked, &d, [&](){ d.done(0); });
    connect(&createBtn, &QPushButton::clicked, &d, [&](){ d.done(1); });
    connect(&joinBtn,   &QPushButton::clicked, &d, [&](){ d.done(2); });

    const int rc = d.exec();
    if (rc == 1) showCreateServerDialog();
    else if (rc == 2) showJoinServerDialog();
}

void MainWindow::showCreateServerDialog() {
    showDialogBackdrop();
    QDialog d(this);
    d.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    d.setAttribute(Qt::WA_TranslucentBackground);
    d.setFixedSize(560, 520);

    auto *outer = new QVBoxLayout(&d);
    outer->setContentsMargins(0, 0, 0, 0);
    QWidget *card = new QWidget(&d);
    card->setObjectName("card");
    card->setStyleSheet((QString(kPrettyDialogQss) +
        "QPushButton#tplBtn{background:rgba(255,255,255,0.04); color:%1;"
        " border:1px solid rgba(120,150,210,0.18); border-radius:14px;"
        " padding:14px; font-weight:600; text-align:left; min-width:0;}"
        "QPushButton#tplBtn:hover{background:rgba(255,255,255,0.08);"
        " border:1px solid rgba(120,150,210,0.35);}"
        "QPushButton#tplBtn[selected=\"true\"]{background:qlineargradient("
        " x1:0,y1:0,x2:1,y2:1, stop:0 rgba(43,109,245,0.25),"
        " stop:1 rgba(123,63,228,0.25));"
        " border:2px solid #818cf8;}"
        "QLabel#step{color:%2; font-size:11px; font-weight:700;"
        " letter-spacing:1.5px; text-transform:uppercase;}"
        "QLabel#wtitle{font-size:22px; font-weight:800; color:%1;}"
        "QPushButton#ghost{min-width:0;}"
    ).arg(T::Text, T::Sub));
    outer->addWidget(card);

    auto *cl = new QVBoxLayout(card);
    cl->setContentsMargins(30, 26, 30, 24);
    cl->setSpacing(14);

    // Step progress dots
    auto *dotsRow = new QHBoxLayout();
    dotsRow->setSpacing(8);
    dotsRow->addStretch();
    QList<QLabel*> dots;
    for (int i = 0; i < 2; ++i) {
        QLabel *dot = new QLabel();
        dot->setFixedSize(28, 4);
        dot->setStyleSheet("background:#26262b; border-radius:2px;");
        dots.append(dot);
        dotsRow->addWidget(dot);
    }
    dotsRow->addStretch();
    cl->addLayout(dotsRow);

    auto *stepLbl = new QLabel(QString::fromUtf8("ADIM 1 / 2"));
    stepLbl->setObjectName("step");
    stepLbl->setAlignment(Qt::AlignCenter);
    cl->addWidget(stepLbl);

    auto *titleLbl = new QLabel(QString::fromUtf8("Bir şablon seç"));
    titleLbl->setObjectName("wtitle");
    titleLbl->setAlignment(Qt::AlignCenter);
    cl->addWidget(titleLbl);

    auto *subLbl = new QLabel(QString::fromUtf8(
        "Sunucunu bir başlangıç noktasıyla oluştur. Daha sonra her şeyi değiştirebilirsin."));
    subLbl->setObjectName("subtitle");
    subLbl->setWordWrap(true);
    subLbl->setAlignment(Qt::AlignCenter);
    cl->addWidget(subLbl);

    // Stacked content
    auto *stack = new QStackedWidget();
    cl->addWidget(stack, 1);

    // --- Step 1: template selection ---
    QWidget *page1 = new QWidget();
    auto *p1l = new QGridLayout(page1);
    p1l->setContentsMargins(0, 8, 0, 0);
    p1l->setSpacing(12);

    struct Template { QString icon; QString name; QString desc; };
    QList<Template> templates = {
        {QString::fromUtf8("✦"), QString::fromUtf8("Arkadaşlar"),      QString::fromUtf8("Küçük bir sohbet ve ses kanalı")},
        {QString::fromUtf8("🎮"), QString::fromUtf8("Oyun"),            QString::fromUtf8("Oyun ve sesli sohbet kanalları")},
        {QString::fromUtf8("📚"), QString::fromUtf8("Ders / Çalışma"),  QString::fromUtf8("Konu bazlı metin kanalları")},
        {QString::fromUtf8("🎨"), QString::fromUtf8("Topluluk"),        QString::fromUtf8("Duyurular + sohbet + ses")},
    };

    QList<QPushButton*> tplBtns;
    int selectedIdx = 0;
    for (int i = 0; i < templates.size(); ++i) {
        const Template &t = templates[i];
        QPushButton *btn = new QPushButton();
        btn->setObjectName("tplBtn");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setMinimumHeight(82);
        btn->setText(QString::fromUtf8("  %1   %2\n      %3").arg(t.icon, t.name, t.desc));
        btn->setProperty("selected", i == 0);
        btn->style()->unpolish(btn); btn->style()->polish(btn);
        tplBtns.append(btn);
        p1l->addWidget(btn, i / 2, i % 2);
    }
    int *selIdx = new int(0);
    for (int i = 0; i < tplBtns.size(); ++i) {
        QPushButton *btn = tplBtns[i];
        connect(btn, &QPushButton::clicked, &d, [tplBtns, selIdx, i]() {
            *selIdx = i;
            for (int k = 0; k < tplBtns.size(); ++k) {
                tplBtns[k]->setProperty("selected", k == i);
                tplBtns[k]->style()->unpolish(tplBtns[k]);
                tplBtns[k]->style()->polish(tplBtns[k]);
            }
        });
    }
    Q_UNUSED(selectedIdx);
    stack->addWidget(page1);

    // --- Step 2: name ---
    QWidget *page2 = new QWidget();
    auto *p2l = new QVBoxLayout(page2);
    p2l->setContentsMargins(0, 20, 0, 0);
    p2l->setSpacing(14);

    QLabel *previewWrap = new QLabel();
    previewWrap->setFixedSize(82, 82);
    previewWrap->setAlignment(Qt::AlignCenter);
    previewWrap->setStyleSheet(
        "background:qlineargradient(x1:0,y1:0,x2:1,y2:1,"
        " stop:0 #6366f1, stop:1 #a855f7); color:#fff; border-radius:24px;"
        " font-size:36px; font-weight:800;");
    previewWrap->setText(QString::fromUtf8("✦"));
    p2l->addWidget(previewWrap, 0, Qt::AlignHCenter);

    QLabel *nameLbl = new QLabel(QString::fromUtf8("Sunucu ismi"));
    nameLbl->setStyleSheet("color:#a1a1aa; font-size:12px; font-weight:600;");
    p2l->addWidget(nameLbl);

    QLineEdit *nameEdit = new QLineEdit();
    nameEdit->setPlaceholderText(QString::fromUtf8("Sunucunun adı..."));
    nameEdit->setMinimumHeight(44);
    p2l->addWidget(nameEdit);

    QLabel *hint = new QLabel(QString::fromUtf8(
        "İstediğin zaman değiştirebilirsin. 2-40 karakter."));
    hint->setStyleSheet("color:#6d7a99; font-size:11px;");
    p2l->addWidget(hint);
    p2l->addStretch();

    stack->addWidget(page2);

    // Button row
    auto *btnRow = new QHBoxLayout();
    btnRow->setSpacing(8);
    QPushButton *backBtn = new QPushButton(QString::fromUtf8("Geri"));
    backBtn->setObjectName("ghost");
    backBtn->setVisible(false);
    QPushButton *cancelBtn = new QPushButton(QString::fromUtf8("Vazgeç"));
    cancelBtn->setObjectName("ghost");
    QPushButton *nextBtn = new QPushButton(QString::fromUtf8("İleri"));
    btnRow->addWidget(backBtn);
    btnRow->addStretch();
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(nextBtn);
    cl->addLayout(btnRow);

    auto updateDots = [&](int step) {
        for (int i = 0; i < dots.size(); ++i) {
            const bool active = i <= step;
            dots[i]->setStyleSheet(active
                ? "background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                  " stop:0 #6366f1, stop:1 #a855f7); border-radius:2px;"
                : "background:#26262b; border-radius:2px;");
        }
    };
    updateDots(0);

    // Template seçimi değişince preview güncelle
    auto syncPreview = [selIdx, templates, previewWrap]() {
        const Template &t = templates[*selIdx];
        previewWrap->setText(t.icon);
    };
    for (QPushButton *btn : tplBtns) {
        connect(btn, &QPushButton::clicked, &d, syncPreview);
    }

    connect(cancelBtn, &QPushButton::clicked, &d, &QDialog::reject);
    connect(backBtn, &QPushButton::clicked, &d, [&]() {
        stack->setCurrentIndex(0);
        stepLbl->setText(QString::fromUtf8("ADIM 1 / 2"));
        titleLbl->setText(QString::fromUtf8("Bir şablon seç"));
        subLbl->setText(QString::fromUtf8(
            "Sunucunu bir başlangıç noktasıyla oluştur. Daha sonra her şeyi değiştirebilirsin."));
        nextBtn->setText(QString::fromUtf8("İleri"));
        backBtn->setVisible(false);
        updateDots(0);
    });
    connect(nextBtn, &QPushButton::clicked, &d, [&]() {
        if (stack->currentIndex() == 0) {
            stack->setCurrentIndex(1);
            stepLbl->setText(QString::fromUtf8("ADIM 2 / 2"));
            titleLbl->setText(QString::fromUtf8("Sunucuna isim ver"));
            subLbl->setText(QString::fromUtf8(
                "Bu isim sunucu listende görünecek. Dilediğin zaman değiştirebilirsin."));
            // Default isim önerisi
            if (nameEdit->text().isEmpty()) {
                nameEdit->setText(templates[*selIdx].name);
                nameEdit->selectAll();
            }
            nameEdit->setFocus();
            nextBtn->setText(QString::fromUtf8("Oluştur"));
            backBtn->setVisible(true);
            updateDots(1);
        } else {
            const QString name = nameEdit->text().trimmed();
            if (name.length() < 2) return;
            d.accept();
        }
    });
    connect(nameEdit, &QLineEdit::returnPressed, nextBtn, &QPushButton::click);

    const bool accepted = d.exec() == QDialog::Accepted;
    const QString finalName = nameEdit->text().trimmed();
    hideDialogBackdrop();
    delete selIdx;
    if (!accepted || finalName.isEmpty()) return;
    signalingClient->createServer(finalName);
    showToast(QString::fromUtf8("Sunucu oluşturuluyor..."), "info", 2000);
}

void MainWindow::showJoinServerDialog() {
    const QString code = promptInput(
        QString::fromUtf8("Sunucuya Katıl"),
        QString::fromUtf8("Arkadaşından aldığın 8 haneli davet kodunu gir."),
        QString(),
        QString::fromUtf8("Örn: K3X7PQ9A"),
        QString::fromUtf8("⇢"));
    if (code.isEmpty()) return;
    signalingClient->joinServerByInvite(code.toUpper());
    showToast(QString::fromUtf8("Sunucuya katılım gönderildi..."), "info", 2000);
}

// ===================== Modern UI helpers =====================

// Modern minimal dialog teması — sade, düz renkler, yeni T:: paleti
const char *kPrettyDialogQss =
    "#card{background:#141417; border:1px solid #26262b; border-radius:14px;}"
    "QLabel{color:#fafafa; background:transparent; border:none;}"
    "#iconBubble{background:#6366f1; color:#fff;"
    " border:none; border-radius:22px;"
    " font-size:20px; font-weight:700;}"
    "#title{font-size:18px; font-weight:700; color:#fafafa;}"
    "#subtitle{color:#a1a1aa; font-size:12px;}"
    "QLineEdit,QTextEdit,QPlainTextEdit{background:#17171a; color:#fafafa;"
    " border:1px solid #26262b; border-radius:9px; padding:10px 12px;"
    " font-size:13px; selection-background-color:#6366f1;}"
    "QLineEdit:hover,QTextEdit:hover,QPlainTextEdit:hover{border:1px solid #3a3a40;}"
    "QLineEdit:focus,QTextEdit:focus,QPlainTextEdit:focus{"
    " border:1px solid #6366f1;}"
    "QLineEdit:disabled{background:#0f0f11; color:#52525b;}"
    "QComboBox{background:#17171a; color:#fafafa; border:1px solid #26262b;"
    " border-radius:9px; padding:8px 12px; min-height:20px;}"
    "QComboBox:hover{border:1px solid #3a3a40;}"
    "QComboBox:focus{border:1px solid #6366f1;}"
    "QComboBox::drop-down{border:none; width:22px;}"
    "QComboBox QAbstractItemView{background:#17171a; color:#fafafa;"
    " border:1px solid #26262b; border-radius:8px; selection-background-color:#6366f1;"
    " padding:4px;}"
    // CheckBox
    "QCheckBox{color:#fafafa; spacing:8px;}"
    "QCheckBox::indicator{width:18px; height:18px; border-radius:5px;"
    " background:#17171a; border:1px solid #3a3a40;}"
    "QCheckBox::indicator:hover{border:1px solid #6366f1;}"
    "QCheckBox::indicator:checked{background:#6366f1; border:1px solid #6366f1;}"
    // Primary button — düz indigo
    "QPushButton{background:#6366f1; color:#ffffff; border:none;"
    " border-radius:9px; padding:10px 18px; font-weight:600; min-width:88px;}"
    "QPushButton:hover{background:#818cf8;}"
    "QPushButton:pressed{background:#4f46e5;}"
    "QPushButton:disabled{background:#1a1a1d; color:#52525b;}"
    // Ghost button
    "QPushButton#ghost{background:transparent; color:#a1a1aa;"
    " border:1px solid #26262b;}"
    "QPushButton#ghost:hover{background:#1a1a1d; color:#fafafa;"
    " border:1px solid #3a3a40;}"
    // Danger button
    "QPushButton#danger{background:#ef4444; color:#ffffff;}"
    "QPushButton#danger:hover{background:#f87171;}"
    // Success button
    "QPushButton#success{background:#22c55e; color:#ffffff;}"
    "QPushButton#success:hover{background:#4ade80;}";

void MainWindow::showDialogBackdrop() {
    if (dialogBackdrop) return;
    dialogBackdrop = new QWidget(this);
    dialogBackdrop->setAttribute(Qt::WA_TransparentForMouseEvents);
    dialogBackdrop->setStyleSheet("background:rgba(6, 8, 16, 0.65); border:none;");
    dialogBackdrop->setGeometry(rect());
    dialogBackdrop->lower();
    dialogBackdrop->show();
}

void MainWindow::hideDialogBackdrop() {
    if (dialogBackdrop) {
        dialogBackdrop->hide();
        dialogBackdrop->deleteLater();
        dialogBackdrop = nullptr;
    }
}

// ===================== Voice activity indicator =====================

bool MainWindow::computeVoiceActive(const QByteArray &pcm16) {
    if (pcm16.size() < 2) return false;
    const qint16 *samples = reinterpret_cast<const qint16*>(pcm16.constData());
    const int n = pcm16.size() / 2;
    if (n <= 0) return false;
    // Hizli RMS
    double sumSq = 0.0;
    const int step = qMax(1, n / 512);
    int count = 0;
    for (int i = 0; i < n; i += step) {
        const double s = samples[i] / 32768.0;
        sumSq += s * s;
        ++count;
    }
    if (count == 0) return false;
    const double rms = std::sqrt(sumSq / count);
    return rms > 0.022;  // esik
}

void MainWindow::setUserSpeaking(const QString &userName, bool active) {
    if (userName.isEmpty()) return;
    if (active) {
        const bool wasSpeaking = speakingUsers.contains(userName);
        speakingUsers.insert(userName);
        // Timer'i resetle (sessizlikten sonra 400ms'de kapansin)
        QTimer *t = speakingTimers.value(userName, nullptr);
        if (!t) {
            t = new QTimer(this);
            t->setSingleShot(true);
            connect(t, &QTimer::timeout, this, [this, userName]() {
                speakingUsers.remove(userName);
                refreshFriendAvatarSpeaking(userName);
            });
            speakingTimers.insert(userName, t);
        }
        t->start(400);
        if (!wasSpeaking) refreshFriendAvatarSpeaking(userName);
    } else {
        if (speakingUsers.remove(userName)) refreshFriendAvatarSpeaking(userName);
        if (auto *t = speakingTimers.value(userName, nullptr)) t->stop();
    }
}

void MainWindow::refreshFriendAvatarSpeaking(const QString &userName) {
    if (!friendsSidebarLayout) return;
    for (int i = 0; i < friendsSidebarLayout->count(); ++i) {
        QLayoutItem *item = friendsSidebarLayout->itemAt(i);
        if (!item || !item->widget()) continue;
        QWidget *w = item->widget();
        if (w->property("friendUserName").toString() == userName) {
            QWidget *ring = w->findChild<QWidget*>("speakingRing");
            if (ring) ring->setVisible(speakingUsers.contains(userName));
            break;
        }
    }
}

QString MainWindow::promptInput(const QString &title, const QString &label,
                                const QString &initial, const QString &placeholder,
                                const QString &icon) {
    showDialogBackdrop();
    QDialog d(this);
    d.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    d.setAttribute(Qt::WA_TranslucentBackground);
    d.setFixedSize(440, icon.isEmpty() ? 220 : 260);

    auto *outer = new QVBoxLayout(&d);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *card = new QWidget(&d);
    card->setObjectName("card");
    card->setStyleSheet(kPrettyDialogQss);
    outer->addWidget(card);
    auto *lay = new QVBoxLayout(card);
    lay->setContentsMargins(26, 22, 26, 22);
    lay->setSpacing(10);

    if (!icon.isEmpty()) {
        auto *bub = new QLabel(icon);
        bub->setObjectName("iconBubble");
        bub->setFixedSize(44, 44);
        bub->setAlignment(Qt::AlignCenter);
        auto *row = new QHBoxLayout();
        row->addWidget(bub);
        row->addStretch();
        lay->addLayout(row);
    }
    auto *titleLbl = new QLabel(title);
    titleLbl->setObjectName("title");
    lay->addWidget(titleLbl);
    auto *subLbl = new QLabel(label);
    subLbl->setObjectName("subtitle");
    subLbl->setWordWrap(true);
    lay->addWidget(subLbl);

    auto *input = new QLineEdit();
    input->setText(initial);
    if (!placeholder.isEmpty()) input->setPlaceholderText(placeholder);
    input->setClearButtonEnabled(true);
    lay->addWidget(input);
    lay->addStretch();

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto *cancel = new QPushButton(QString::fromUtf8("Vazgeç"));
    cancel->setObjectName("ghost");
    auto *okBtn = new QPushButton(QString::fromUtf8("Tamam"));
    btnRow->addWidget(cancel);
    btnRow->addWidget(okBtn);
    lay->addLayout(btnRow);

    connect(cancel, &QPushButton::clicked, &d, &QDialog::reject);
    connect(okBtn,  &QPushButton::clicked, &d, &QDialog::accept);
    connect(input,  &QLineEdit::returnPressed, &d, &QDialog::accept);

    input->setFocus();
    if (d.exec() != QDialog::Accepted) {
        hideDialogBackdrop();
        return QString();
    }
    hideDialogBackdrop();
    return input->text().trimmed();
}

bool MainWindow::promptConfirm(const QString &title, const QString &message,
                               const QString &confirmText, const QString &cancelText, bool danger) {
    showDialogBackdrop();
    QDialog d(this);
    d.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    d.setAttribute(Qt::WA_TranslucentBackground);
    d.setFixedSize(420, 220);

    auto *outer = new QVBoxLayout(&d);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *card = new QWidget(&d);
    card->setObjectName("card");
    card->setStyleSheet(kPrettyDialogQss);
    outer->addWidget(card);
    auto *lay = new QVBoxLayout(card);
    lay->setContentsMargins(26, 22, 26, 22);
    lay->setSpacing(10);

    auto *bub = new QLabel(danger ? QString::fromUtf8("⚠") : QString::fromUtf8("?"));
    bub->setObjectName("iconBubble");
    if (danger) bub->setStyleSheet("background:qradialgradient(cx:0.5,cy:0.5,radius:0.8,"
                                   " stop:0 #ff6b84, stop:1 #ef4444); color:#fff;"
                                   " border-radius:22px; font-size:20px; font-weight:800;");
    bub->setFixedSize(44, 44);
    bub->setAlignment(Qt::AlignCenter);
    auto *row = new QHBoxLayout();
    row->addWidget(bub);
    row->addStretch();
    lay->addLayout(row);

    auto *titleLbl = new QLabel(title);
    titleLbl->setObjectName("title");
    lay->addWidget(titleLbl);
    auto *msg = new QLabel(message);
    msg->setObjectName("subtitle");
    msg->setWordWrap(true);
    lay->addWidget(msg);
    lay->addStretch();

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto *cancel  = new QPushButton(cancelText);
    cancel->setObjectName("ghost");
    auto *confirm = new QPushButton(confirmText);
    if (danger) confirm->setObjectName("danger");
    btnRow->addWidget(cancel);
    btnRow->addWidget(confirm);
    lay->addLayout(btnRow);

    connect(cancel,  &QPushButton::clicked, &d, &QDialog::reject);
    connect(confirm, &QPushButton::clicked, &d, &QDialog::accept);

    const bool accepted = d.exec() == QDialog::Accepted;
    hideDialogBackdrop();
    return accepted;
}

// ===================== Sistem tepsisi + bildirim =====================

void MainWindow::initSystemTray() {
    if (trayIcon) return;
    if (!QSystemTrayIcon::isSystemTrayAvailable()) return;
    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(QIcon(":/icons/volaura-logo.png"));
    trayIcon->setToolTip("VoLaura");
    auto *menu = new QMenu(this);
    auto *showAct = menu->addAction(QString::fromUtf8("VoLaura'yı göster"));
    menu->addSeparator();
    auto *quitAct = menu->addAction(QString::fromUtf8("Çıkış"));
    connect(showAct, &QAction::triggered, this, [this]() {
        showNormal(); raise(); activateWindow();
    });
    connect(quitAct, &QAction::triggered, qApp, &QApplication::quit);
    trayIcon->setContextMenu(menu);
    connect(trayIcon, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::Trigger || r == QSystemTrayIcon::DoubleClick) {
            showNormal(); raise(); activateWindow();
        }
    });
    trayIcon->show();
}

bool MainWindow::isWindowFocused() const {
    if (isMinimized() || !isVisible()) return false;
    return isActiveWindow();
}

void MainWindow::playNotificationSound(const QString &type) {
    // Windows'ta tray showMessage zaten ses çalar; çağrı için ekstra emphasis
    if (type == "call") {
        // Üç kısa beep ile çağrı vurgusu
        QApplication::beep();
        QTimer::singleShot(220, this, []() { QApplication::beep(); });
        QTimer::singleShot(440, this, []() { QApplication::beep(); });
    } else {
        QApplication::beep();
    }
}

void MainWindow::notifyUser(const QString &title, const QString &body, const QString &type) {
    // Pencere odaktaysa zaten kullanıcı görüyor — sadece in-app toast yeter
    if (isWindowFocused()) {
        showToast(body, type == "call" ? "warn" : "info", 3500);
        return;
    }
    // Arka planda → tray balonu (Windows toast) + ses
    if (trayIcon && trayIcon->isVisible()) {
        QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information;
        if (type == "call")   icon = QSystemTrayIcon::Warning;
        if (type == "friend") icon = QSystemTrayIcon::Information;
        trayIcon->showMessage(title, body, icon, 5000);
    }
    playNotificationSound(type);
}

// Toast: sağ-üst köşede beliren modern, glassmorphism efektli bildirim.
// Slide-in animasyon + drop-shadow + countdown bar. Tıklanabilir (kapatma).
void MainWindow::showToast(const QString &message, const QString &type, int durationMs) {
    static QList<QPointer<QWidget>> sToasts;
    sToasts.erase(std::remove_if(sToasts.begin(), sToasts.end(),
        [](const QPointer<QWidget> &w){ return w.isNull(); }), sToasts.end());

    // Tip → accent renk (sade tek satır toast)
    QString accent = T::Accent;
    if (type == "success")   accent = T::Success;
    else if (type == "error") accent = T::Danger;
    else if (type == "warn")  accent = "#f59e0b";

    // Sade tek satır toast — sol kenar accent şerit + tek satır mesaj + X
    auto *toast = new QWidget(this);
    toast->setAttribute(Qt::WA_DeleteOnClose);
    toast->setObjectName("toastRoot");
    toast->setStyleSheet(QString(
        "#toastRoot{background:%1; border:1px solid %2; border-radius:10px;}"
        "QLabel{background:transparent; border:none; color:%3; font-size:12.5px;}"
        "#tDot{background:%4; border-radius:4px;}"
        "#tBody{color:%3; font-size:12.5px;}"
        "#tClose{color:%5; background:transparent; border:none; font-size:13px; padding:0;}"
        "#tClose:hover{color:%3;}"
    ).arg(T::Card, T::Border, T::Text, accent, T::Sub));

    auto *shadow = new QGraphicsDropShadowEffect(toast);
    shadow->setBlurRadius(20);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 140));
    toast->setGraphicsEffect(shadow);

    auto *row = new QHBoxLayout(toast);
    row->setContentsMargins(12, 9, 10, 9);
    row->setSpacing(10);

    // Sol noktası — küçük renk dot
    auto *dot = new QLabel();
    dot->setObjectName("tDot");
    dot->setFixedSize(8, 8);
    row->addWidget(dot, 0, Qt::AlignVCenter);

    auto *bLbl = new QLabel(message);
    bLbl->setObjectName("tBody");
    bLbl->setWordWrap(false);
    bLbl->setMaximumWidth(360);
    bLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    row->addWidget(bLbl, 1, Qt::AlignVCenter);

    auto *closeBtn = new QToolButton();
    closeBtn->setObjectName("tClose");
    closeBtn->setText(QString::fromUtf8("✕"));
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setFixedSize(18, 18);
    row->addWidget(closeBtn, 0, Qt::AlignVCenter);

    // Countdown progress (toast altında 2px ince çubuk)
    auto *bar = new QWidget(toast);
    bar->setStyleSheet(QString("background:%1; border-bottom-left-radius:10px;").arg(accent));
    bar->setFixedHeight(2);

    toast->adjustSize();
    toast->setMinimumWidth(280);
    toast->setMaximumWidth(420);
    toast->adjustSize();
    bar->setGeometry(0, toast->height() - 2, toast->width(), 2);

    // Konumlandır: sağ-alt köşe, üst üste yığılma
    const int margin = 18;
    const int spacing = 10;
    int y = height() - margin - toast->sizeHint().height();
    for (const auto &t : sToasts) if (t) y -= (t->height() + spacing);
    const int finalX = width() - toast->sizeHint().width() - margin;
    const int startX = width(); // ekran dışından kayar
    toast->move(startX, y);
    toast->show();
    toast->raise();
    sToasts.append(toast);

    // Slide-in (sağdan kayma) + ease out
    auto *slideIn = new QPropertyAnimation(toast, "pos", toast);
    slideIn->setDuration(320);
    slideIn->setStartValue(QPoint(startX, y));
    slideIn->setEndValue(QPoint(finalX, y));
    slideIn->setEasingCurve(QEasingCurve::OutCubic);
    slideIn->start(QAbstractAnimation::DeleteWhenStopped);

    // Countdown bar animasyonu — toast altındaki 2px şerit toast genişliği → 0
    auto *barAnim = new QPropertyAnimation(bar, "geometry", toast);
    barAnim->setDuration(durationMs);
    barAnim->setStartValue(QRect(0, toast->height() - 2, toast->width(), 2));
    barAnim->setEndValue(QRect(0, toast->height() - 2, 0, 2));
    barAnim->setEasingCurve(QEasingCurve::Linear);
    barAnim->start(QAbstractAnimation::DeleteWhenStopped);

    // Kapatma fonksiyonu
    auto closeToast = [toast]() {
        if (!toast) return;
        auto *fadeEff = new QGraphicsOpacityEffect(toast);
        // Mevcut shadow effect ile çakışmasın diye yeni opacity için ayrı sarma yerine
        // basit bir slide-out + fade kombinasyonu yapalım:
        toast->setGraphicsEffect(fadeEff);
        fadeEff->setOpacity(1.0);
        auto *fadeOut = new QPropertyAnimation(fadeEff, "opacity", toast);
        fadeOut->setDuration(220);
        fadeOut->setStartValue(1.0);
        fadeOut->setEndValue(0.0);
        auto *slideOut = new QPropertyAnimation(toast, "pos", toast);
        slideOut->setDuration(220);
        slideOut->setStartValue(toast->pos());
        slideOut->setEndValue(QPoint(toast->pos().x() + 40, toast->pos().y()));
        slideOut->setEasingCurve(QEasingCurve::InCubic);
        QObject::connect(fadeOut, &QPropertyAnimation::finished, toast, &QWidget::close);
        fadeOut->start(QAbstractAnimation::DeleteWhenStopped);
        slideOut->start(QAbstractAnimation::DeleteWhenStopped);
    };

    QObject::connect(closeBtn, &QToolButton::clicked, toast, closeToast);
    QTimer::singleShot(durationMs, toast, closeToast);
}

void MainWindow::showInviteCodeDialog(int serverId) {
    if (!serversById.contains(serverId)) return;
    const QJsonObject srv = serversById.value(serverId);
    const QString code = srv.value("inviteCode").toString();
    const QString name = srv.value("name").toString();

    QDialog d(this);
    d.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    d.setAttribute(Qt::WA_TranslucentBackground);
    d.setFixedSize(460, 300);

    auto *outer = new QVBoxLayout(&d);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *card = new QWidget(&d);
    card->setObjectName("card");
    card->setStyleSheet(kPrettyDialogQss);
    outer->addWidget(card);
    auto *lay = new QVBoxLayout(card);
    lay->setContentsMargins(26, 22, 26, 22);
    lay->setSpacing(12);

    auto *bub = new QLabel(QString::fromUtf8("🎫"));
    bub->setObjectName("iconBubble");
    bub->setFixedSize(44, 44);
    bub->setAlignment(Qt::AlignCenter);
    auto *topRow = new QHBoxLayout();
    topRow->addWidget(bub);
    topRow->addStretch();
    lay->addLayout(topRow);

    auto *titleLbl = new QLabel(QString::fromUtf8("Davet Kodu"));
    titleLbl->setObjectName("title");
    lay->addWidget(titleLbl);
    auto *subLbl = new QLabel(QString::fromUtf8("%1 sunucusuna davet için bu kodu arkadaşınla paylaş.").arg(name));
    subLbl->setObjectName("subtitle");
    subLbl->setWordWrap(true);
    lay->addWidget(subLbl);

    auto *codeField = new QLineEdit(code);
    codeField->setReadOnly(true);
    codeField->setAlignment(Qt::AlignCenter);
    codeField->setStyleSheet(
        "QLineEdit{background:#161b27; color:#fafafa; border:1px solid #26262b;"
        " border-radius:12px; padding:14px; font-size:22px; font-weight:800;"
        " letter-spacing:4px;}");
    lay->addWidget(codeField);
    lay->addStretch();

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto *closeBtn = new QPushButton(QString::fromUtf8("Kapat"));
    closeBtn->setObjectName("ghost");
    auto *copyBtn = new QPushButton(QString::fromUtf8("⎘ Kopyala"));
    btnRow->addWidget(closeBtn);
    btnRow->addWidget(copyBtn);
    lay->addLayout(btnRow);

    connect(copyBtn, &QPushButton::clicked, this, [this, code]() {
        QApplication::clipboard()->setText(code);
        showToast(QString::fromUtf8("Davet kodu panoya kopyalandı"), "success");
    });
    connect(closeBtn, &QPushButton::clicked, &d, &QDialog::accept);
    d.exec();
}

// =====================================================================
//                          VOICE CHANNELS
// =====================================================================
//
// Tasarım: Sesli kanala katılınca mikrofondan 16 kHz / mono / 16-bit PCM
// yakalanır, ~40 ms'lik parçalar base64 olarak sunucuya 'voice_chunk'
// mesajıyla gönderilir. Sunucu aynı kanaldaki diğer üyelere relay eder.
// Her peer için ayrı bir QAudioSink ile oynatılır (mikser gerekmiyor:
// Qt ses motoru karıştırır).

// PCM S16 mono için linear gain uygula. Saturation clipping.
static QByteArray applyPcmGain(const QByteArray &pcm, float gain) {
    if (gain == 1.0f || pcm.isEmpty()) return pcm;
    QByteArray out = pcm;
    int16_t *p = reinterpret_cast<int16_t*>(out.data());
    const int n = out.size() / 2;
    for (int i = 0; i < n; ++i) {
        int v = int(float(p[i]) * gain);
        if (v >  32767) v =  32767;
        else if (v < -32768) v = -32768;
        p[i] = int16_t(v);
    }
    return out;
}

float MainWindow::getUserVolume(const QString &username) const {
    return userVolume.value(username, 1.0f);
}

void MainWindow::setUserVolume(const QString &username, float v) {
    v = qBound(0.0f, v, 2.0f);
    if (qFuzzyCompare(v, 1.0f)) userVolume.remove(username);
    else userVolume.insert(username, v);
    // Sink->setVolume sadece 0..1 destekler; >1 PCM amplify ile sağlanır.
    // Channel sink'leri (userId → username çevirisi)
    for (auto it = voiceMembers.begin(); it != voiceMembers.end(); ++it) {
        if (it.value().value("username").toString() == username) {
            if (auto *sink = voiceSinks.value(it.key(), nullptr))
                sink->setVolume(qMin(1.0f, v));
        }
    }
    // Call sink'leri (participantId → username)
    for (auto it = participantNames.begin(); it != participantNames.end(); ++it) {
        if (it.value() == username) {
            if (auto *sink = callVoiceSinks.value(it.key(), nullptr))
                sink->setVolume(qMin(1.0f, v));
        }
    }
}

void MainWindow::showUserVolumeMenu(const QString &username, const QPoint &globalPos) {
    if (username.isEmpty() || username == currentUserName) return;
    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setStyleSheet(
        "QMenu{background:#0f1422; border:1px solid rgba(120,150,210,0.30); border-radius:10px; padding:8px;}"
        "QMenu::item{padding:6px 10px; color:#e4e7ee;}");
    QWidget *w = new QWidget();
    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(12, 8, 12, 10);
    lay->setSpacing(6);
    auto *title = new QLabel(QString::fromUtf8("🔊  %1 — ses seviyesi").arg(username));
    title->setStyleSheet("color:#a78bfa; font-weight:700; font-size:12px;");
    lay->addWidget(title);
    auto *row = new QHBoxLayout();
    auto *slider = new QSlider(Qt::Horizontal);
    slider->setRange(0, 200);
    const int initVal = int(getUserVolume(username) * 100.0f);
    slider->setValue(initVal);
    slider->setFixedWidth(220);
    auto *valLbl = new QLabel(QString::number(initVal) + "%");
    valLbl->setMinimumWidth(48);
    valLbl->setStyleSheet("color:#fafafa; font-weight:700;");
    row->addWidget(slider);
    row->addWidget(valLbl);
    lay->addLayout(row);
    auto *hint = new QLabel(QString::fromUtf8("Sürükle: 0% (sustur) · 100% (normal) · 200% (yükselt)"));
    hint->setStyleSheet("color:#8b94a8; font-size:10px;");
    lay->addWidget(hint);
    auto *muteRow = new QHBoxLayout();
    auto *muteBtn = new QPushButton(QString::fromUtf8("🔇 Sustur"));
    auto *resetBtn = new QPushButton(QString::fromUtf8("↺ Sıfırla"));
    muteBtn->setStyleSheet("QPushButton{background:#1f2333; color:#e4e7ee; border:1px solid #2c3144;"
                           " padding:5px 10px; border-radius:6px;}"
                           "QPushButton:hover{background:#272d40;}");
    resetBtn->setStyleSheet(muteBtn->styleSheet());
    muteRow->addWidget(muteBtn);
    muteRow->addWidget(resetBtn);
    muteRow->addStretch();
    lay->addLayout(muteRow);
    connect(slider, &QSlider::valueChanged, this, [this, username, valLbl](int v) {
        valLbl->setText(QString::number(v) + "%");
        setUserVolume(username, float(v) / 100.0f);
    });
    connect(muteBtn,  &QPushButton::clicked, this, [slider]() { slider->setValue(0); });
    connect(resetBtn, &QPushButton::clicked, this, [slider]() { slider->setValue(100); });
    auto *act = new QWidgetAction(menu);
    act->setDefaultWidget(w);
    menu->addAction(act);
    menu->popup(globalPos);
}

// Mic gain uygulayıcı — PCM S16 mono frame üzerinde inplace ölçek.
static QByteArray applyMicGain(const QByteArray &pcm, float gain) {
    if (gain == 1.0f || pcm.isEmpty()) return pcm;
    QByteArray out = pcm;
    int16_t *p = reinterpret_cast<int16_t*>(out.data());
    const int n = out.size() / 2;
    for (int i = 0; i < n; ++i) {
        int v = int(float(p[i]) * gain);
        if (v >  32767) v =  32767;
        else if (v < -32768) v = -32768;
        p[i] = int16_t(v);
    }
    return out;
}

static QAudioFormat kVoiceFormat() {
    QAudioFormat f;
    // 48 kHz mono S16 — Opus codec için optimal (Discord/Zoom standardı)
    // Mono: VoIP profilinde stereo'ya göre çok daha iyi kalite/bandwidth oranı
    f.setSampleRate(48000);
    f.setChannelCount(1);
    f.setSampleFormat(QAudioFormat::Int16);
    return f;
}

void MainWindow::showVoicePanel(int channelId) {
    // voicePanel chat alanı üzerine inline yerleşsin. chatLayout varsa onun
    // en üstüne kart olarak ekle.
    hideVoicePanel();

    if (!chatLayout) return;
    const QString chName = channelsById.value(channelId).value("name").toString();

    voicePanel = new QWidget();
    voicePanel->setObjectName("voiceCard");
    voicePanel->setAttribute(Qt::WA_StyledBackground, true);
    voicePanel->setStyleSheet(
        "#voiceCard{background:#141417; border:1px solid #26262b; border-radius:14px;}"
        "QLabel{color:#fafafa; background:transparent;}"
        "#voiceTitle{font-size:16px; font-weight:700;}"
        "#voiceStatus{color:#a1a1aa; font-size:12px;}"
        "QPushButton{background:#6366f1; color:white; border:none; border-radius:8px;"
        " padding:7px 12px; font-weight:600; font-size:12px; min-height:18px;}"
        "QPushButton:hover{background:#818cf8;}"
        "QPushButton#ghost{background:#1d1d22; color:#fafafa;}"
        "QPushButton#ghost:hover{background:#2a2a31;}"
        "QPushButton#danger{background:#ef4444;}"
        "QPushButton#danger:hover{background:#f87171;}"
    );
    voicePanelLayout = new QVBoxLayout(voicePanel);
    voicePanelLayout->setContentsMargins(18, 16, 18, 16);
    voicePanelLayout->setSpacing(10);

    // Kanal adı zaten üstteki kanal listesinde görünüyor; voice panel'da
    // tekrar göstermek görsel kirlilikti (kullanıcı şikayeti). Sadece kompakt
    // bir status satırı bırakıyoruz.
    auto *headerRow = new QHBoxLayout();
    voicePanelTitle = new QLabel();  // unused but kept for reference (geriye uyumluluk)
    voicePanelTitle->hide();
    voicePanelStatus = new QLabel(QString::fromUtf8("Sesli Bağlantı"));
    voicePanelStatus->setObjectName("voiceStatus");
    Q_UNUSED(chName);
    headerRow->addStretch();
    headerRow->addWidget(voicePanelStatus);
    voicePanelLayout->addLayout(headerRow);

    // Live media grid (ekran/kamera tiles)
    voiceMediaGrid = new QWidget();
    voiceMediaGrid->setAttribute(Qt::WA_StyledBackground, true);
    voiceMediaGrid->setStyleSheet("background:transparent;");
    voiceMediaGridLayout = new QGridLayout(voiceMediaGrid);
    voiceMediaGridLayout->setContentsMargins(0, 0, 0, 0);
    voiceMediaGridLayout->setHorizontalSpacing(10);
    voiceMediaGridLayout->setVerticalSpacing(10);
    voiceMediaGrid->hide();
    voicePanelLayout->addWidget(voiceMediaGrid);

    // Members listesi (içeride dikey kutu)
    voiceMembersWidget = new QWidget();
    voiceMembersWidget->setAttribute(Qt::WA_StyledBackground, true);
    voiceMembersWidget->setStyleSheet("background:#0d1018; border-radius:10px;");
    voiceMembersLayout = new QVBoxLayout(voiceMembersWidget);
    voiceMembersLayout->setContentsMargins(10, 10, 10, 10);
    voiceMembersLayout->setSpacing(6);
    voicePanelLayout->addWidget(voiceMembersWidget);

    // Butonlar — 2 satırlı; dar chat kolonunda sığsın
    voiceJoinBtn   = new QPushButton(QString::fromUtf8("Sesli Kanala Bağlan"));
    voiceLeaveBtn  = new QPushButton(QString::fromUtf8("Ayrıl"));
    voiceLeaveBtn->setObjectName("danger");
    voiceMuteBtn   = new QPushButton(QString::fromUtf8("Mikrofon"));
    voiceMuteBtn->setObjectName("ghost");
    voiceDeafenBtn = new QPushButton(QString::fromUtf8("Sağır"));
    voiceDeafenBtn->setObjectName("ghost");
    voiceScreenShareBtn = new QPushButton(QString::fromUtf8("Ekran Paylaş"));
    voiceScreenShareBtn->setObjectName("ghost");
    voiceCameraShareBtn = new QPushButton(QString::fromUtf8("Kamera"));
    voiceCameraShareBtn->setObjectName("ghost");
    for (auto *b : {voiceJoinBtn, voiceLeaveBtn, voiceMuteBtn, voiceDeafenBtn,
                    voiceScreenShareBtn, voiceCameraShareBtn}) {
        b->setCursor(Qt::PointingHandCursor);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    // Üst satır: state — Mikrofon / Sağır
    auto *btnRow1 = new QHBoxLayout();
    btnRow1->setSpacing(6);
    btnRow1->addWidget(voiceMuteBtn);
    btnRow1->addWidget(voiceDeafenBtn);
    voicePanelLayout->addLayout(btnRow1);
    // Alt satır: Ekran / Kamera / Bağlan-Ayrıl
    auto *btnRow2 = new QHBoxLayout();
    btnRow2->setSpacing(6);
    btnRow2->addWidget(voiceScreenShareBtn);
    btnRow2->addWidget(voiceCameraShareBtn);
    btnRow2->addWidget(voiceJoinBtn);
    btnRow2->addWidget(voiceLeaveBtn);
    voicePanelLayout->addLayout(btnRow2);

    connect(voiceJoinBtn,   &QPushButton::clicked, this, &MainWindow::joinVoiceChannelClicked);
    connect(voiceLeaveBtn,  &QPushButton::clicked, this, &MainWindow::leaveVoiceChannelClicked);
    connect(voiceMuteBtn,   &QPushButton::clicked, this, &MainWindow::toggleVoiceMute);
    connect(voiceDeafenBtn, &QPushButton::clicked, this, &MainWindow::toggleVoiceDeafen);
    connect(voiceScreenShareBtn, &QPushButton::clicked, this, &MainWindow::toggleVoiceScreenShare);
    connect(voiceCameraShareBtn, &QPushButton::clicked, this, &MainWindow::toggleVoiceCameraShare);

    // Başlangıç: bağlı değilse Leave/Mute/Deafen pasif
    const bool joined = (currentVoiceChannelId == channelId);
    voiceLeaveBtn->setVisible(joined);
    voiceMuteBtn->setVisible(joined);
    voiceDeafenBtn->setVisible(joined);
    voiceScreenShareBtn->setVisible(joined);
    voiceCameraShareBtn->setVisible(joined);
    voiceJoinBtn->setVisible(!joined);

    // chatLayout'un en üstüne ekle (index 0)
    chatLayout->insertWidget(0, voicePanel);

    // Kanal listesini sor
    signalingClient->listVoiceParticipants(channelId);
    rebuildVoiceMembersList();
}

void MainWindow::hideVoicePanel() {
    if (voicePanel) {
        voicePanel->deleteLater();
        voicePanel = nullptr;
        voicePanelLayout = nullptr;
        voiceMembersWidget = nullptr;
        voiceMembersLayout = nullptr;
        voiceJoinBtn = voiceLeaveBtn = voiceMuteBtn = voiceDeafenBtn = nullptr;
        voiceScreenShareBtn = voiceCameraShareBtn = nullptr;
        voicePanelTitle = voicePanelStatus = nullptr;
        voiceMediaGrid = nullptr;
        voiceMediaGridLayout = nullptr;
        voiceMediaTiles.clear();
        voiceMediaTileLabels.clear();
    }
}

void MainWindow::rebuildVoiceMembersList() {
    if (!voiceMembersLayout) return;
    QLayoutItem *it;
    while ((it = voiceMembersLayout->takeAt(0)) != nullptr) {
        if (it->widget()) it->widget()->deleteLater();
        delete it;
    }
    if (voiceMembers.isEmpty()) {
        auto *empty = new QLabel(QString::fromUtf8("Bu kanalda henüz kimse yok."));
        empty->setStyleSheet("color:#6b7690; font-style:italic;");
        voiceMembersLayout->addWidget(empty);
        return;
    }
    for (const auto &m : voiceMembers) {
        const QString uname = m.value("username").toString();
        const bool muted    = m.value("muted").toBool();
        const bool deafened = m.value("deafened").toBool();

        auto *row = new QWidget();
        row->setAttribute(Qt::WA_StyledBackground, true);
        row->setStyleSheet("background:#161b27; border-radius:8px;");
        auto *h = new QHBoxLayout(row);
        h->setContentsMargins(10, 6, 10, 6);
        h->setSpacing(10);

        auto *av = new QLabel(avatarInitials(uname));
        av->setFixedSize(28, 28);
        av->setAlignment(Qt::AlignCenter);
        av->setAttribute(Qt::WA_StyledBackground, true);
        av->setStyleSheet(QString("background:%1; color:white; border-radius:14px; font-size:11px; font-weight:800;").arg(avatarColor(uname)));
        h->addWidget(av);

        auto *nm = new QLabel(uname);
        nm->setStyleSheet("color:#fafafa; font-weight:600;");
        h->addWidget(nm);
        h->addStretch();

        if (muted) {
            auto *t = new QLabel(QString::fromUtf8("Sessiz"));
            t->setStyleSheet("color:#ff9da8; font-size:11px; font-weight:600;"
                             " padding:2px 8px; background:rgba(239,68,68,0.18); border-radius:8px;");
            h->addWidget(t);
        }
        if (deafened) {
            auto *t = new QLabel(QString::fromUtf8("Sağır"));
            t->setStyleSheet("color:#ff9da8; font-size:11px; font-weight:600;"
                             " padding:2px 8px; background:rgba(239,68,68,0.18); border-radius:8px;");
            h->addWidget(t);
        }
        // Diğer kullanıcılar için: 🔊 ses seviyesi butonu
        if (uname != currentUserName && !uname.isEmpty()) {
            auto *volBtn = new QToolButton();
            volBtn->setText(QString::fromUtf8("🔊"));
            volBtn->setCursor(Qt::PointingHandCursor);
            volBtn->setToolTip(QString::fromUtf8("Ses seviyesini ayarla"));
            volBtn->setStyleSheet(
                "QToolButton{background:transparent; border:none; color:#a78bfa;"
                " font-size:13px; padding:2px 6px; border-radius:6px;}"
                "QToolButton:hover{background:rgba(167,139,250,0.18); color:#e4e7ee;}");
            connect(volBtn, &QToolButton::clicked, this, [this, volBtn, uname]() {
                showUserVolumeMenu(uname, volBtn->mapToGlobal(QPoint(0, volBtn->height())));
            });
            h->addWidget(volBtn);
        }
        voiceMembersLayout->addWidget(row);
    }
}

void MainWindow::joinVoiceChannelClicked() {
    if (currentChannelId <= 0) return;
    const QString type = channelsById.value(currentChannelId).value("type").toString();
    if (type != "voice") return;
    // Zaten başka bir voice kanalındaysa ayrıl
    if (currentVoiceChannelId && currentVoiceChannelId != currentChannelId) {
        signalingClient->leaveVoiceChannel();
        stopVoiceCapture();
        stopAllVoicePlayback();
    }
    signalingClient->joinVoiceChannel(currentChannelId);
}

void MainWindow::leaveVoiceChannelClicked() {
    if (currentVoiceChannelId <= 0) return;
    signalingClient->leaveVoiceChannel();
    // onVoiceLeft içinde temizlik yapılıyor
}

void MainWindow::toggleVoiceMute() {
    if (currentVoiceChannelId <= 0) return;
    voiceMuted = !voiceMuted;
    if (voiceMuteBtn) voiceMuteBtn->setText(voiceMuted
        ? QString::fromUtf8("Mikrofon Kapalı") : QString::fromUtf8("Mikrofon Açık"));
    if (voiceMuteBtn) voiceMuteBtn->setStyleSheet(voiceMuted
        ? QString("background:%1; color:#ffffff;").arg(T::Danger)
        : QString("background:%1; color:#ffffff;").arg(T::Success));
    signalingClient->setVoiceState(currentVoiceChannelId, voiceMuted, voiceDeafened);
}

void MainWindow::toggleVoiceDeafen() {
    if (currentVoiceChannelId <= 0) return;
    voiceDeafened = !voiceDeafened;
    if (voiceDeafenBtn) voiceDeafenBtn->setText(voiceDeafened
        ? QString::fromUtf8("Sağır: Açık") : QString::fromUtf8("Sağır: Kapalı"));
    // Sağır modda tüm playback'leri durdur
    if (voiceDeafened) stopAllVoicePlayback();
    signalingClient->setVoiceState(currentVoiceChannelId, voiceMuted, voiceDeafened);
}

void MainWindow::startVoiceCapture() {
    stopVoiceCapture();
    QAudioFormat fmt = kVoiceFormat();  // 48 kHz mono S16 — Opus için zorunlu
    QAudioDevice dev = QMediaDevices::defaultAudioInput();
    if (!settingsMicId.isEmpty()) {
        for (const QAudioDevice &d : QMediaDevices::audioInputs()) {
            if (QString::fromUtf8(d.id()) == settingsMicId) { dev = d; break; }
        }
    }
    if (dev.isNull()) {
        appendSystemMessage(QString::fromUtf8("⚠ Mikrofon cihazı bulunamadı."));
        return;
    }
    // Format desteklenmiyorsa bile 48kHz mono S16 ile dene — WASAPI shared mode genelde
    // resample yapar; Qt'nin isFormatSupported() çok temkinli false dönüyor olabilir.
    const bool fmtNative = dev.isFormatSupported(fmt);
    qInfo() << "[voice] mic device:" << dev.description()
            << "fmtSupported(48k/mono/S16)=" << fmtNative;
    voiceSource = new QAudioSource(dev, fmt, this);
    // 40 ms capture buffer (2x frame) — düşük latency
    voiceSource->setBufferSize(int(fmt.sampleRate() * fmt.bytesPerFrame() * 0.04));
    voiceMicBuffer.clear();
    // Opus encoder'ı hazırla (kullanıcı bitrate ayarı; varsayılan 64 kbps VOIP, FEC, DTX)
    if (!voiceEncoder.isReady()) {
        if (!voiceEncoder.init(settingsAudioBitrate)) {
            appendSystemMessage(QString::fromUtf8("⚠ Opus encoder başlatılamadı."));
            return;
        }
    }
    // RNNoise gürültü bastırıcıyı hazırla (klavye, fan, klima vs.)
    if (!voiceNoiseSuppressor.isReady()) {
        if (!voiceNoiseSuppressor.init()) {
            qInfo() << "[voice] RNNoise yok — ham mic kullanılıyor";
        } else {
            qInfo() << "[voice] RNNoise aktif (gürültü bastırma açık)";
        }
    }
    // Speex DSP: AEC (yankı) + AGC (otomatik gain) + dereverb
    if (!voiceAudioProcessor.isReady()) {
        if (!voiceAudioProcessor.init(200)) {
            qInfo() << "[voice] SpeexDSP yok — AEC/AGC devre dışı";
        } else {
            qInfo() << "[voice] SpeexDSP aktif (AEC + AGC + dereverb açık)";
        }
    }
    applyAudioSettings();
    voiceSourceIO = voiceSource->start();
    qInfo() << "[voice] QAudioSource state:" << voiceSource->state()
            << "error:" << voiceSource->error()
            << "format ok:" << (voiceSource->format() == fmt);
    if (voiceSource->error() != QAudio::NoError || !voiceSourceIO) {
        // Cihaz formatı kabul etmedi — preferredFormat ile tekrar dene (kalite düşer ama ses gider)
        appendSystemMessage(QString::fromUtf8("⚠ Mic 48kHz mono'yu reddetti, cihaz formatına düşülüyor."));
        delete voiceSource;
        QAudioFormat pf = dev.preferredFormat();
        qInfo() << "[voice] fallback fmt:" << pf.sampleRate() << "Hz" << pf.channelCount() << "ch";
        voiceSource = new QAudioSource(dev, pf, this);
        voiceSource->setBufferSize(int(pf.sampleRate() * pf.bytesPerFrame() * 0.04));
        voiceSourceIO = voiceSource->start();
    }
    if (voiceSourceIO) {
        connect(voiceSourceIO, &QIODevice::readyRead, this, &MainWindow::onMicReadyRead);
    } else {
        appendSystemMessage(QString::fromUtf8("⚠ Mikrofon başlatılamadı."));
    }
}

void MainWindow::stopVoiceCapture() {
    if (voiceSource) {
        voiceSource->stop();
        voiceSource->deleteLater();
        voiceSource = nullptr;
    }
    voiceSourceIO = nullptr;
    voiceMicBuffer.clear();
}

void MainWindow::onMicReadyRead() {
    if (!voiceSourceIO || voiceMuted || currentVoiceChannelId <= 0) {
        if (voiceSourceIO) voiceSourceIO->readAll(); // drain
        voiceMicBuffer.clear();
        return;
    }
    const QByteArray data = voiceSourceIO->readAll();
    if (data.isEmpty()) return;
    voiceMicBuffer.append(data);

    // 20 ms mono S16 = 48000 * 2 * 0.02 = 1920 byte → Opus encode → ~80-160 byte
    const int kTargetChunk = OpusEncoderWrapper::kFrameBytes; // 1920
    while (voiceMicBuffer.size() >= kTargetChunk) {
        QByteArray pcmFrame = voiceMicBuffer.left(kTargetChunk);
        voiceMicBuffer.remove(0, kTargetChunk);
        // 0) Mic gain (kullanıcı ayarı)
        if (settingsMicGain != 1.0f) pcmFrame = applyMicGain(pcmFrame, settingsMicGain);
        // 1) Speex AEC + AGC + dereverb
        if (voiceAudioProcessor.isReady()) {
            pcmFrame = voiceAudioProcessor.processCapture(pcmFrame);
        }
        // 2) RNNoise: ML noise suppression
        if (voiceNoiseSuppressor.isReady() && voiceNoiseSuppressor.isEnabled()) {
            pcmFrame = voiceNoiseSuppressor.processFrame(pcmFrame);
        }
        if (computeVoiceActive(pcmFrame)) setUserSpeaking(currentUserName, true);
        const QByteArray opusPkt = voiceEncoder.encodePcm(pcmFrame);
        if (!opusPkt.isEmpty())
            signalingClient->sendVoiceChunk(currentVoiceChannelId, opusPkt);
    }
}

QAudioSink* MainWindow::getOrCreateVoiceSinkFor(qint64 userId) {
    auto it = voiceSinks.find(userId);
    if (it != voiceSinks.end()) return it.value();
    QAudioFormat fmt = kVoiceFormat();
    QAudioDevice dev = QMediaDevices::defaultAudioOutput();
    if (!settingsSpeakerId.isEmpty()) {
        for (const QAudioDevice &d : QMediaDevices::audioOutputs()) {
            if (QString::fromUtf8(d.id()) == settingsSpeakerId) { dev = d; break; }
        }
    }
    if (dev.isNull()) {
        appendSystemMessage(QString::fromUtf8("⚠ Ses çıkış cihazı bulunamadı."));
        return nullptr;
    }
    // Opus decoder her zaman 48kHz mono S16 PCM döner — sink'i bu formatta zorla aç.
    // isFormatSupported() false dönse bile WASAPI shared mode genelde resample yapar.
    qInfo() << "[voice] sink device:" << dev.description()
            << "fmtSupported(48k/mono/S16)=" << dev.isFormatSupported(fmt);
    QAudioSink *sink = new QAudioSink(dev, fmt, this);
    // 400ms jitter buffer — ağ jitter'inde underrun/cızırtıyı önler
    sink->setBufferSize(int(fmt.sampleRate() * fmt.bytesPerFrame() * 0.40));
    sink->setVolume(1.0);
    QIODevice *io = sink->start();
    if (!io || sink->error() != QAudio::NoError) {
        appendSystemMessage(QString::fromUtf8("⚠ Ses çıkışı başlatılamadı (err=%1).").arg(sink->error()));
        sink->deleteLater();
        return nullptr;
    }
    voiceSinks.insert(userId, sink);
    voiceSinkIO.insert(userId, io);
    return sink;
}

void MainWindow::stopAllVoicePlayback() {
    for (auto it = voiceSinks.begin(); it != voiceSinks.end(); ++it) {
        if (it.value()) { it.value()->stop(); it.value()->deleteLater(); }
    }
    voiceSinks.clear();
    voiceSinkIO.clear();
}

// ===================== 1:1 CALL AUDIO =====================
// 1:1 arkadaş aramaları room üzerinden çalışır. Mikrofondan 48 kHz mono PCM
// yakalanır → AEC/AGC/NS uygulanır → Opus encode → sendMediaChunk("audio_pcm", opusPkt).
// Aynı pipeline'ı sesli kanalla paylaşır (encoder, processor, suppressor singleton).
void MainWindow::startCallVoiceCapture() {
    stopCallVoiceCapture();
    QAudioFormat fmt = kVoiceFormat();
    QAudioDevice dev = QMediaDevices::defaultAudioInput();
    if (!settingsMicId.isEmpty()) {
        for (const QAudioDevice &d : QMediaDevices::audioInputs()) {
            if (QString::fromUtf8(d.id()) == settingsMicId) { dev = d; break; }
        }
    }
    if (dev.isNull()) {
        appendSystemMessage(QString::fromUtf8("Mikrofon bulunamadı."));
        return;
    }
    qInfo() << "[call] mic device:" << dev.description()
            << "fmtSupported(48k/mono/S16)=" << dev.isFormatSupported(fmt);
    callVoiceSource = new QAudioSource(dev, fmt, this);
    callVoiceSource->setBufferSize(int(fmt.sampleRate() * fmt.bytesPerFrame() * 0.04));
    callMicBuffer.clear();
    // DSP pipeline'ı paylaşımlı (channel ile aynı) — singleton init
    if (!voiceEncoder.isReady()) voiceEncoder.init(settingsAudioBitrate);
    if (!voiceNoiseSuppressor.isReady()) voiceNoiseSuppressor.init();
    if (!voiceAudioProcessor.isReady()) voiceAudioProcessor.init(200);
    applyAudioSettings();
    callVoiceSourceIO = callVoiceSource->start();
    if (callVoiceSource->error() != QAudio::NoError || !callVoiceSourceIO) {
        appendSystemMessage(QString::fromUtf8("⚠ Çağrı mic 48kHz mono'yu reddetti, cihaz formatına düşülüyor."));
        delete callVoiceSource;
        QAudioFormat pf = dev.preferredFormat();
        callVoiceSource = new QAudioSource(dev, pf, this);
        callVoiceSourceIO = callVoiceSource->start();
    }
    if (callVoiceSourceIO) {
        connect(callVoiceSourceIO, &QIODevice::readyRead, this, &MainWindow::onCallMicReadyRead);
    }
}

void MainWindow::stopCallVoiceCapture() {
    if (callVoiceSource) {
        callVoiceSource->stop();
        callVoiceSource->deleteLater();
        callVoiceSource = nullptr;
    }
    callVoiceSourceIO = nullptr;
    callMicBuffer.clear();
}

void MainWindow::onCallMicReadyRead() {
    if (!callVoiceSourceIO) return;
    if (isMuted || currentRoomCode.isEmpty()) {
        callVoiceSourceIO->readAll();
        callMicBuffer.clear();
        return;
    }
    const QByteArray data = callVoiceSourceIO->readAll();
    if (data.isEmpty()) return;
    callMicBuffer.append(data);
    const int kTargetChunk = OpusEncoderWrapper::kFrameBytes; // 1920 byte / 20ms
    while (callMicBuffer.size() >= kTargetChunk) {
        QByteArray pcmFrame = callMicBuffer.left(kTargetChunk);
        callMicBuffer.remove(0, kTargetChunk);
        // 0) Mic gain
        if (settingsMicGain != 1.0f) pcmFrame = applyMicGain(pcmFrame, settingsMicGain);
        // 1) AEC + AGC + dereverb
        if (voiceAudioProcessor.isReady()) {
            pcmFrame = voiceAudioProcessor.processCapture(pcmFrame);
        }
        // 2) RNNoise ML noise suppression
        if (voiceNoiseSuppressor.isReady() && voiceNoiseSuppressor.isEnabled()) {
            pcmFrame = voiceNoiseSuppressor.processFrame(pcmFrame);
        }
        if (computeVoiceActive(pcmFrame)) setUserSpeaking(currentUserName, true);
        // 3) Opus encode
        const QByteArray opusPkt = voiceEncoder.encodePcm(pcmFrame);
        if (!opusPkt.isEmpty())
            signalingClient->sendMediaChunk("audio_pcm", opusPkt);
    }
}

QAudioSink* MainWindow::getOrCreateCallVoiceSinkFor(const QString &participantId) {
    auto it = callVoiceSinks.find(participantId);
    if (it != callVoiceSinks.end()) return it.value();
    QAudioFormat fmt = kVoiceFormat();
    QAudioDevice dev = QMediaDevices::defaultAudioOutput();
    if (!settingsSpeakerId.isEmpty()) {
        for (const QAudioDevice &d : QMediaDevices::audioOutputs()) {
            if (QString::fromUtf8(d.id()) == settingsSpeakerId) { dev = d; break; }
        }
    }
    if (dev.isNull()) {
        appendSystemMessage(QString::fromUtf8("⚠ Çağrı ses çıkış cihazı bulunamadı."));
        return nullptr;
    }
    if (!dev.isFormatSupported(fmt)) {
        appendSystemMessage(QString::fromUtf8("⚠ Çağrı ses formatı desteklenmiyor, cihaz formatına düşülüyor."));
        fmt = dev.preferredFormat();
    }
    QAudioSink *sink = new QAudioSink(dev, fmt, this);
    sink->setBufferSize(int(fmt.sampleRate() * fmt.bytesPerFrame() * 0.40));
    sink->setVolume(1.0);
    QIODevice *io = sink->start();
    if (!io) {
        appendSystemMessage(QString::fromUtf8("⚠ Çağrı ses çıkışı başlatılamadı: %1").arg(sink->error()));
        sink->deleteLater();
        return nullptr;
    }
    callVoiceSinks.insert(participantId, sink);
    callVoiceSinkIO.insert(participantId, io);
    return sink;
}

void MainWindow::stopAllCallVoicePlayback() {
    for (auto it = callVoiceSinks.begin(); it != callVoiceSinks.end(); ++it) {
        if (it.value()) { it.value()->stop(); it.value()->deleteLater(); }
    }
    callVoiceSinks.clear();
    callVoiceSinkIO.clear();
    callVoiceDecoders.clear();
}

void MainWindow::onVoiceJoined(int channelId, const QJsonArray &participants) {
    currentVoiceChannelId = channelId;
    updateMediaControlVisibility();
    voiceMembers.clear();
    for (const auto &v : participants) {
        const QJsonObject o = v.toObject();
        voiceMembers.insert((qint64)o.value("userId").toDouble(), o);
    }

    if (voicePanelStatus) voicePanelStatus->setText(QString::fromUtf8("Bağlandı · %1 kişi").arg(voiceMembers.size()));
    if (voiceJoinBtn)   voiceJoinBtn->setVisible(false);
    if (voiceLeaveBtn)  voiceLeaveBtn->setVisible(true);
    if (voiceMuteBtn)   voiceMuteBtn->setVisible(true);
    if (voiceDeafenBtn) voiceDeafenBtn->setVisible(true);
    if (voiceScreenShareBtn) voiceScreenShareBtn->setVisible(true);
    if (voiceCameraShareBtn) voiceCameraShareBtn->setVisible(true);

    startVoiceCapture();
    rebuildVoiceMembersList();
    // Halihazırda paylaşım yapanlar için tile oluştur
    for (const auto &v : participants) {
        const QJsonObject o = v.toObject();
        const QString uname = o.value("username").toString();
        if (o.value("sharingScreen").toBool()) {
            makeVoiceMediaTile(uname, "screen");
        }
        if (o.value("sharingCamera").toBool()) {
            makeVoiceMediaTile(uname, "camera");
        }
    }
    rebuildVoiceMediaGrid();
    showToast(QString::fromUtf8("Sesli kanala bağlandın"), "success", 2200);
}

void MainWindow::onVoiceLeft() {
    currentVoiceChannelId = 0;
    updateMediaControlVisibility();
    voiceMembers.clear();
    stopVoiceCapture();
    stopAllVoicePlayback();
    stopAllVoiceShares();
    if (voicePanelStatus) voicePanelStatus->setText(QString::fromUtf8("Bağlanmadı"));
    if (voiceJoinBtn)   voiceJoinBtn->setVisible(true);
    if (voiceLeaveBtn)  voiceLeaveBtn->setVisible(false);
    if (voiceMuteBtn)   voiceMuteBtn->setVisible(false);
    if (voiceDeafenBtn) voiceDeafenBtn->setVisible(false);
    if (voiceScreenShareBtn) voiceScreenShareBtn->setVisible(false);
    if (voiceCameraShareBtn) voiceCameraShareBtn->setVisible(false);
    rebuildVoiceMembersList();
    showToast(QString::fromUtf8("Sesli kanaldan ayrıldın"), "info", 2000);
}

void MainWindow::onVoiceMemberJoined(int channelId, const QJsonObject &member) {
    if (channelId != currentVoiceChannelId && channelId != currentChannelId) return;
    const qint64 uid = (qint64)member.value("userId").toDouble();
    voiceMembers.insert(uid, member);
    if (voicePanelStatus && currentVoiceChannelId)
        voicePanelStatus->setText(QString::fromUtf8("Bağlandı · %1 kişi").arg(voiceMembers.size()));
    rebuildVoiceMembersList();
}

void MainWindow::onVoiceMemberLeft(int channelId, qint64 userId, const QString &username) {
    if (channelId != currentVoiceChannelId && channelId != currentChannelId) return;
    voiceMembers.remove(userId);
    // Playback'i durdur
    auto it = voiceSinks.find(userId);
    if (it != voiceSinks.end()) {
        if (it.value()) { it.value()->stop(); it.value()->deleteLater(); }
        voiceSinks.erase(it);
        voiceSinkIO.remove(userId);
    }
    voiceDecoders.remove(userId);
    // Bu kullanıcıya ait paylaşım tile'larını kaldır
    if (!username.isEmpty()) {
        removeVoiceMediaTile(username, "screen");
        removeVoiceMediaTile(username, "camera");
    }
    if (voicePanelStatus && currentVoiceChannelId)
        voicePanelStatus->setText(QString::fromUtf8("Bağlandı · %1 kişi").arg(voiceMembers.size()));
    rebuildVoiceMembersList();
}

void MainWindow::onVoiceStateChanged(int channelId, qint64 userId, bool muted, bool deafened) {
    if (channelId != currentVoiceChannelId && channelId != currentChannelId) return;
    auto it = voiceMembers.find(userId);
    if (it != voiceMembers.end()) {
        QJsonObject o = it.value();
        o["muted"] = muted;
        o["deafened"] = deafened;
        voiceMembers[userId] = o;
        rebuildVoiceMembersList();
    }
}

void MainWindow::onVoiceChunkReceived(int channelId, qint64 userId, const QByteArray &opusPkt) {
    if (channelId != currentVoiceChannelId) return;
    if (voiceDeafened) return;
    if (opusPkt.isEmpty()) return;
    // Per-peer Opus decoder lookup / lazy init
    auto it = voiceDecoders.find(userId);
    if (it == voiceDecoders.end()) {
        auto dec = std::make_shared<OpusDecoderWrapper>();
        if (!dec->init()) return;
        it = voiceDecoders.insert(userId, dec);
    }
    QByteArray pcm = it.value()->decodePacket(opusPkt);
    if (pcm.isEmpty()) return;
    QAudioSink *sink = getOrCreateVoiceSinkFor(userId);
    QIODevice *io = voiceSinkIO.value(userId, nullptr);
    if (!sink || !io) return;
    // Per-user volume: sink->setVolume(0..1); >1 PCM amplify
    const QString uname = voiceMembers.value(userId).value("username").toString();
    const float vol = getUserVolume(uname);
    if (vol > 1.0f) pcm = applyPcmGain(pcm, vol);
    io->write(pcm);
    // AEC için far-end referansı: hoparlöre giden PCM'i biriktir
    if (voiceAudioProcessor.isReady()) {
        voiceAudioProcessor.pushFarEnd(pcm);
    }
    if (computeVoiceActive(pcm)) {
        if (!uname.isEmpty()) setUserSpeaking(uname, true);
    }
}

// =====================================================================
//                          MEMBERS PANEL
// =====================================================================

static int roleRank(const QString &role) {
    if (role == "owner") return 0;
    if (role == "admin") return 1;
    return 2; // member
}

void MainWindow::rebuildMembersPanel() {
    if (!membersPanelLayout || !membersPanel) return;
    // Önceki öğeleri sil
    QLayoutItem *it;
    while ((it = membersPanelLayout->takeAt(0)) != nullptr) {
        if (it->widget()) it->widget()->deleteLater();
        delete it;
    }
    if (currentServerId <= 0 || !membersByServer.contains(currentServerId)) {
        // yükleniyor placeholder'i
        auto *empty = new QLabel(QString::fromUtf8("Yükleniyor..."));
        empty->setStyleSheet("color:#6b7690; font-style:italic; padding:6px 4px;");
        membersPanelLayout->addWidget(empty);
        membersPanelLayout->addStretch();
        return;
    }
    // üyeleri online > rol > isim önceliğine göre sırala
    QList<QJsonObject> list;
    for (const auto &v : membersByServer.value(currentServerId)) list.append(v.toObject());
    std::sort(list.begin(), list.end(), [](const QJsonObject &a, const QJsonObject &b) {
        const bool ao = a.value("online").toBool();
        const bool bo = b.value("online").toBool();
        if (ao != bo) return ao; // online önce
        const int ar = roleRank(a.value("role").toString());
        const int br = roleRank(b.value("role").toString());
        if (ar != br) return ar < br;
        return a.value("username").toString().compare(b.value("username").toString(), Qt::CaseInsensitive) < 0;
    });
    // Gruplar: online / offline
    int onlineCount = 0, offlineCount = 0;
    for (const auto &m : list) {
        if (m.value("online").toBool()) onlineCount++; else offlineCount++;
    }
    if (membersPanelTitle) {
        membersPanelTitle->setText(QString::fromUtf8("ÜYELER · %1").arg(list.size()));
    }

    auto addSection = [this](const QString &label, int count) {
        if (count <= 0) return;
        auto *hdr = new QLabel(QString("%1 — %2").arg(label).arg(count));
        hdr->setStyleSheet("color:#6b7690; font-size:10px; font-weight:800; letter-spacing:1px; padding:6px 4px 2px 4px;");
        membersPanelLayout->addWidget(hdr);
    };

    addSection(QString::fromUtf8("ÇEVRİMİÇİ"), onlineCount);
    bool offlineHeaderAdded = false;
    for (const auto &m : list) {
        const QString uname = m.value("username").toString();
        const QString role  = m.value("role").toString();
        const bool online   = m.value("online").toBool();
        if (!online && !offlineHeaderAdded) {
            addSection(QString::fromUtf8("ÇEVRİMDIŞI"), offlineCount);
            offlineHeaderAdded = true;
        }
        auto *row = new QPushButton();
        row->setCursor(Qt::PointingHandCursor);
        row->setFixedHeight(44);
        row->setText(""); // metin manuel layout
        row->setStyleSheet(
            "QPushButton{background:transparent; border:none; border-radius:8px; text-align:left;}"
            "QPushButton:hover{background:#161b27;}");
        auto *rowLay = new QHBoxLayout(row);
        rowLay->setContentsMargins(6, 4, 6, 4);
        rowLay->setSpacing(10);

        // Avatar + online dot
        QWidget *avWrap = new QWidget();
        avWrap->setFixedSize(32, 32);
        avWrap->setStyleSheet("background:transparent;");
        auto *av = new QLabel(avatarInitials(uname), avWrap);
        av->setGeometry(0, 0, 32, 32);
        av->setAlignment(Qt::AlignCenter);
        av->setAttribute(Qt::WA_StyledBackground, true);
        av->setStyleSheet(QString("background:%1; color:white; border-radius:16px; font-size:12px; font-weight:800;")
                          .arg(online ? avatarColor(uname) : "#3b4360"));
        auto *dot = new QLabel(avWrap);
        dot->setGeometry(20, 20, 12, 12);
        dot->setAttribute(Qt::WA_StyledBackground, true);
        dot->setStyleSheet(QString("background:%1; border:2px solid #0e1119; border-radius:6px;")
                           .arg(online ? "#22c55e" : "#6b7690"));
        rowLay->addWidget(avWrap);

        // Name + role
        auto *col = new QVBoxLayout();
        col->setSpacing(0);
        col->setContentsMargins(0, 0, 0, 0);
        auto *nm = new QLabel(uname);
        nm->setStyleSheet(QString("color:%1; font-weight:700; background:transparent;")
                          .arg(online ? "#fafafa" : "#7c8aa5"));
        col->addWidget(nm);
        if (role == "owner" || role == "admin") {
            auto *tag = new QLabel(role == "owner" ? QString::fromUtf8("👑 sahip") : QString::fromUtf8("⚡ yönetici"));
            tag->setStyleSheet("color:#ffc857; font-size:10px; background:transparent;");
            col->addWidget(tag);
        }
        rowLay->addLayout(col, 1);

        // Sağ tık menüsü (DM, arkadaş ekle, kopyala)
        const QString selfName = currentUserName;
        row->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(row, &QPushButton::customContextMenuRequested, this, [this, uname, selfName](const QPoint &) {
            if (uname == selfName) return; // kendine sağ tık anlamsız
            QMenu mm(this);
            mm.setStyleSheet("QMenu{background:#141823;color:#fafafa;border:1px solid #26262b;padding:4px;}"
                             "QMenu::item{padding:6px 12px;border-radius:4px;}"
                             "QMenu::item:selected{background:#6366f1;}");
            QAction *aDm   = mm.addAction(QString::fromUtf8("💬  DM aç"));
            QAction *aFr   = nullptr;
            if (!friendsOnline.contains(uname) && !pendingOutRequests.contains(uname))
                aFr = mm.addAction(QString::fromUtf8("➕  Arkadaş ekle"));
            QAction *aCall = mm.addAction(QString::fromUtf8("📞  Ara"));
            mm.addSeparator();
            QAction *aCopy = mm.addAction(QString::fromUtf8("Kullanıcı adını kopyala"));
            QAction *sel = mm.exec(QCursor::pos());
            if (!sel) return;
            if (sel == aDm)       selectDm(uname);
            else if (sel == aCopy){ QApplication::clipboard()->setText(uname); showToast(QString::fromUtf8("Kopyalandı"), "info", 1500); }
            else if (aFr && sel == aFr) { signalingClient->sendFriendRequest(uname); showToast(QString::fromUtf8("Arkadaşlık isteği gönderildi"), "info"); }
            else if (sel == aCall){ startCallToFriend(uname); }
        });
        // Sol tık da DM aç
        connect(row, &QPushButton::clicked, this, [this, uname, selfName]() {
            if (uname == selfName) return;
            selectDm(uname);
        });
        membersPanelLayout->addWidget(row);
    }
    membersPanelLayout->addStretch();
}

void MainWindow::onMembersListed(int serverId, const QJsonArray &members) {
    membersByServer[serverId] = members;
    if (currentServerId == serverId) rebuildMembersPanel();
}

void MainWindow::onMemberJoined(int serverId, const QJsonObject &member) {
    QJsonArray arr = membersByServer.value(serverId);
    // aynı kullanıcı varsa güncelle, yoksa ekle
    const QString uname = member.value("username").toString();
    bool found = false;
    for (int i = 0; i < arr.size(); ++i) {
        QJsonObject o = arr[i].toObject();
        if (o.value("username").toString() == uname) {
            arr[i] = member;
            found = true;
            break;
        }
    }
    if (!found) arr.append(member);
    membersByServer[serverId] = arr;
    if (currentServerId == serverId) {
        rebuildMembersPanel();
        showToast(QString::fromUtf8("%1 sunucuya katıldı").arg(uname), "info", 2200);
    }
}

void MainWindow::onMemberLeft(int serverId, const QString &username) {
    QJsonArray arr = membersByServer.value(serverId);
    for (int i = 0; i < arr.size(); ++i) {
        if (arr[i].toObject().value("username").toString() == username) {
            arr.removeAt(i);
            break;
        }
    }
    membersByServer[serverId] = arr;
    if (currentServerId == serverId) rebuildMembersPanel();
}

// =====================================================================
//                    UNREAD COUNTERS / BADGES
// =====================================================================

int MainWindow::unreadForServer(int serverId) const {
    int total = 0;
    if (!channelsByServer.contains(serverId)) return 0;
    for (const auto &c : channelsByServer.value(serverId)) {
        total += unreadByChannel.value(c.value("id").toInt(), 0);
    }
    return total;
}

void MainWindow::clearUnreadChannel(int channelId) {
    if (!unreadByChannel.contains(channelId)) return;
    unreadByChannel.remove(channelId);
    rebuildChannelList();
    rebuildServerSidebar();
}

void MainWindow::clearUnreadDm(const QString &peer) {
    if (!unreadByDmPeer.contains(peer)) return;
    unreadByDmPeer.remove(peer);
    rebuildFriendsSidebar();
}

void MainWindow::bumpUnreadChannel(int channelId) {
    unreadByChannel[channelId] = unreadByChannel.value(channelId, 0) + 1;
    if (currentServerId > 0) rebuildChannelList();
    rebuildServerSidebar();
}

void MainWindow::bumpUnreadDm(const QString &peer) {
    unreadByDmPeer[peer] = unreadByDmPeer.value(peer, 0) + 1;
    rebuildFriendsSidebar();
}

// =====================================================================
//                    TYPING INDICATORS
// =====================================================================

void MainWindow::onLocalTypingSignal() {
    if (!typingCooldownTimer) return;
    if (typingCooldownTimer->isActive()) return;
    typingCooldownTimer->start();
    if (chatMode == ChatMode::Channel && currentChannelId > 0) {
        signalingClient->sendTypingChannel(currentChannelId);
    } else if (chatMode == ChatMode::Dm && !currentDmPeer.isEmpty()) {
        signalingClient->sendTypingDm(currentDmPeer);
    }
}

// =====================================================================
//                    VOICE SCREEN/CAMERA SHARING
// =====================================================================

QString MainWindow::voiceTileKey(const QString &username, const QString &kind) {
    return username + ":" + kind;
}

QWidget *MainWindow::makeVoiceMediaTile(const QString &username, const QString &kind) {
    QWidget *tile = new QWidget();
    tile->setObjectName("voiceTile");
    tile->setAttribute(Qt::WA_StyledBackground, true);
    tile->setStyleSheet(
        "QWidget#voiceTile{background:qlineargradient(x1:0,y1:0,x2:1,y2:1,"
        " stop:0 #0a0a0b, stop:1 #050709);"
        " border:1px solid rgba(120,150,210,0.12); border-radius:14px;}"
        "QWidget#voiceTile:hover{border:1px solid rgba(120,150,210,0.35);}");
    tile->setMinimumSize(360, 220);

    auto *tileShadow = new QGraphicsDropShadowEffect(tile);
    tileShadow->setBlurRadius(24);
    tileShadow->setOffset(0, 6);
    tileShadow->setColor(QColor(0, 0, 0, 180));
    tile->setGraphicsEffect(tileShadow);

    auto *v = new QVBoxLayout(tile);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    // Video alanı (tam doldurur)
    auto *screen = new QLabel(tile);
    screen->setAlignment(Qt::AlignCenter);
    screen->setMinimumHeight(180);
    screen->setStyleSheet(
        "background:#05070d; border-radius:13px; color:#5c6680; font-size:12px;");
    screen->setText(QString::fromUtf8("Sinyal bekleniyor..."));
    screen->setCursor(Qt::PointingHandCursor);
    screen->setToolTip(QString::fromUtf8("Büyütmek için çift tıkla"));
    v->addWidget(screen, 1);

    // Double-click → fullscreen viewer
    screen->installEventFilter(this);
    screen->setProperty("voiceTileUser", username);
    screen->setProperty("voiceTileKind", kind);

    // Sağ tık → ses seviyesi menüsü (kendi tile'ında işlev yok)
    if (username != currentUserName) {
        tile->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(tile, &QWidget::customContextMenuRequested, this,
                [this, tile, username](const QPoint &pos) {
            showUserVolumeMenu(username, tile->mapToGlobal(pos));
        });
    }

    // Overlay: sol-alt kullanıcı badge (blur cam efekti görünümü)
    auto *badge = new QLabel(screen);
    badge->setText(QString::fromUtf8("%1  %2")
        .arg(kind == "screen" ? QString::fromUtf8("🖥") : QString::fromUtf8("📷"),
             username.toHtmlEscaped()));
    badge->setAttribute(Qt::WA_StyledBackground, true);
    badge->setStyleSheet(
        "background:rgba(5,7,13,0.72); color:#fafafa;"
        " border:1px solid rgba(255,255,255,0.08);"
        " border-radius:10px; padding:4px 10px;"
        " font-weight:700; font-size:11px;");
    badge->adjustSize();
    badge->move(10, 10);
    badge->raise();

    // Overlay: sağ-üst fullscreen (⛶) ikon
    auto *fsBtn = new QToolButton(screen);
    fsBtn->setText(QString::fromUtf8("⛶"));
    fsBtn->setCursor(Qt::PointingHandCursor);
    fsBtn->setToolTip(QString::fromUtf8("Tam ekran"));
    fsBtn->setStyleSheet(
        "QToolButton{background:rgba(5,7,13,0.72); color:#c9d3ea;"
        " border:1px solid rgba(255,255,255,0.08); border-radius:8px;"
        " font-size:13px; padding:3px 7px;}"
        "QToolButton:hover{background:rgba(120,150,210,0.30); color:#fff;}");
    fsBtn->adjustSize();
    connect(fsBtn, &QToolButton::clicked, this, [this, username, kind]() {
        openVoiceTileFullscreen(username, kind);
    });
    // Fullscreen butonu sağ-üste konumlandır (label boyut değiştirince)
    auto repositionFs = [screen, fsBtn]() {
        fsBtn->move(screen->width() - fsBtn->width() - 8, 8);
    };
    screen->installEventFilter(this);  // zaten kuruluydu; resize için farklı yol
    QObject::connect(screen, &QObject::destroyed, fsBtn, [](){});
    // Screen resize eventlerini yakalamak için timer-based
    auto *fsRepo = new QTimer(screen);
    fsRepo->setInterval(500);
    connect(fsRepo, &QTimer::timeout, screen, repositionFs);
    fsRepo->start();
    repositionFs();

    const QString key = voiceTileKey(username, kind);
    voiceMediaTiles[key] = tile;
    voiceMediaTileLabels[key] = screen;
    return tile;
}

void MainWindow::removeVoiceMediaTile(const QString &username, const QString &kind) {
    const QString key = voiceTileKey(username, kind);
    if (!voiceMediaTiles.contains(key)) return;
    QWidget *w = voiceMediaTiles.take(key);
    voiceMediaTileLabels.remove(key);
    if (w) w->deleteLater();
    rebuildVoiceMediaGrid();
}

void MainWindow::rebuildVoiceMediaGrid() {
    if (!voiceMediaGrid || !voiceMediaGridLayout) return;
    // Layout'tan hepsini çıkar (silmeden — widget'lar map'te tutulu)
    while (voiceMediaGridLayout->count() > 0) {
        QLayoutItem *li = voiceMediaGridLayout->takeAt(0);
        if (li) delete li;
    }
    if (voiceMediaTiles.isEmpty()) {
        voiceMediaGrid->hide();
        return;
    }
    voiceMediaGrid->show();
    const int cols = voiceMediaTiles.size() > 1 ? 2 : 1;
    int i = 0;
    for (auto it = voiceMediaTiles.begin(); it != voiceMediaTiles.end(); ++it) {
        const int r = i / cols, c = i % cols;
        voiceMediaGridLayout->addWidget(it.value(), r, c);
        ++i;
    }
}

void MainWindow::captureAndSendVoiceScreenFrame() {
    if (!voiceSharingScreen || currentVoiceChannelId <= 0) return;
    // Aynı anda iki encode yarışmasın — UI takılmasın
    if (voiceScreenEncodeBusy) return;
    const QList<QScreen*> screens = QGuiApplication::screens();
    QScreen *screen = (settingsScreenIndex >= 0 && settingsScreenIndex < screens.size())
        ? screens.at(settingsScreenIndex) : QGuiApplication::primaryScreen();
    if (!screen) return;
    const QPixmap frame = screen->grabWindow(0);
    if (frame.isNull()) return;
    const int targetH = qBound(240, settingsScreenHeight, 1440);
    const int targetW = static_cast<int>(targetH * 16.0 / 9.0);
    QImage scaled = frame.toImage().scaled(targetW, targetH, Qt::KeepAspectRatio, Qt::FastTransformation);

    // Kendi tile preview — re-decode yerine ham QImage'tan üret (daha hızlı)
    const QPixmap selfPm = QPixmap::fromImage(scaled);
    const QString key = voiceTileKey(currentUserName, "screen");
    if (voiceMediaTileLabels.contains(key)) {
        if (QLabel *lbl = voiceMediaTileLabels.value(key)) {
            const QSize sz = lbl->size().isValid() && lbl->width() > 4 ? lbl->size() : QSize(targetW/2, targetH/2);
            lbl->setPixmap(selfPm.scaled(sz, Qt::KeepAspectRatio, Qt::FastTransformation));
        }
    }
    // Tam ekran açık ve kendi paylaşımımızı izliyorsak — fullscreen label'ı da güncelle (donmasın)
    if (voiceFullscreenLabel && voiceFullscreenUser == currentUserName && voiceFullscreenKind == "screen") {
        voiceFullscreenLabel->setPixmap(selfPm.scaled(voiceFullscreenLabel->size(),
            Qt::KeepAspectRatio, Qt::FastTransformation));
    }

    // JPEG encode + ağ gönderimini worker thread'e at — UI thread bloklanmasın
    voiceScreenEncodeBusy = true;
    const int q = qBound(10, settingsJpegQuality, 95);
    const int chId = currentVoiceChannelId;
    QPointer<MainWindow> self(this);
    (void)QtConcurrent::run([self, scaled, q, chId]() {
        QByteArray encoded;
        QBuffer buffer(&encoded);
        buffer.open(QIODevice::WriteOnly);
        scaled.save(&buffer, "JPG", q);
        QMetaObject::invokeMethod(qApp, [self, encoded, chId]() {
            if (!self) return;
            self->voiceScreenEncodeBusy = false;
            if (!self->voiceSharingScreen || self->currentVoiceChannelId != chId) return;
            self->signalingClient->sendVoiceMediaChunk(chId, "screen", encoded);
        }, Qt::QueuedConnection);
    });
}

void MainWindow::sendVoiceCameraFrameTick() {
    if (!voiceSharingCamera || currentVoiceChannelId <= 0) return;
    if (lastCameraFramePayload.isEmpty()) return;
    signalingClient->sendVoiceMediaChunk(currentVoiceChannelId, "camera", lastCameraFramePayload);
    // Tek decode, hem tile hem fullscreen güncelleme — donmasın
    QPixmap pm;
    if (!pm.loadFromData(lastCameraFramePayload, "JPG")) return;
    const QString key = voiceTileKey(currentUserName, "camera");
    if (voiceMediaTileLabels.contains(key)) {
        if (QLabel *lbl = voiceMediaTileLabels.value(key)) {
            lbl->setPixmap(pm.scaled(lbl->size(), Qt::KeepAspectRatio, Qt::FastTransformation));
        }
    }
    if (voiceFullscreenLabel && voiceFullscreenUser == currentUserName && voiceFullscreenKind == "camera") {
        voiceFullscreenLabel->setPixmap(pm.scaled(voiceFullscreenLabel->size(),
            Qt::KeepAspectRatio, Qt::FastTransformation));
    }
}

void MainWindow::toggleVoiceScreenShare() {
    if (currentVoiceChannelId <= 0) return;
    if (!voiceScreenShareTimer) {
        voiceScreenShareTimer = new QTimer(this);
        voiceScreenShareTimer->setInterval(qMax(8, 1000 / qMax(1, settingsScreenFps)));
        connect(voiceScreenShareTimer, &QTimer::timeout, this, &MainWindow::captureAndSendVoiceScreenFrame);
    }
    if (voiceSharingScreen) {
        voiceSharingScreen = false;
        voiceScreenShareTimer->stop();
        signalingClient->stopVoiceShare(currentVoiceChannelId, "screen");
        removeVoiceMediaTile(currentUserName, "screen");
        if (voiceScreenShareBtn) {
            voiceScreenShareBtn->setText(QString::fromUtf8("🖥  Ekranı Paylaş"));
            voiceScreenShareBtn->setStyleSheet("");
        }
    } else {
        if (!pickScreenSourceForVoice()) return;  // iptal edildi
        voiceSharingScreen = true;
        signalingClient->startVoiceShare(currentVoiceChannelId, "screen");
        // Kendi tile'ımızı da ekle (anahtar: currentUserName)
        makeVoiceMediaTile(currentUserName, "screen");
        rebuildVoiceMediaGrid();
        voiceScreenShareTimer->start();
        captureAndSendVoiceScreenFrame();
        if (voiceScreenShareBtn) {
            voiceScreenShareBtn->setText(QString::fromUtf8("🖥  Durdur"));
            voiceScreenShareBtn->setStyleSheet("background:#1a1a1d; color:#b0c6ff;");
        }
    }
}

void MainWindow::toggleVoiceCameraShare() {
    if (currentVoiceChannelId <= 0) return;
    if (!voiceCameraShareTimer) {
        voiceCameraShareTimer = new QTimer(this);
        voiceCameraShareTimer->setInterval(180);
        connect(voiceCameraShareTimer, &QTimer::timeout, this, &MainWindow::sendVoiceCameraFrameTick);
    }
    if (voiceSharingCamera) {
        voiceSharingCamera = false;
        voiceCameraShareTimer->stop();
        if (isCameraOn) toggleCamera(); // kamerayı kapat
        signalingClient->stopVoiceShare(currentVoiceChannelId, "camera");
        removeVoiceMediaTile(currentUserName, "camera");
        if (voiceCameraShareBtn) {
            voiceCameraShareBtn->setText(QString::fromUtf8("📷  Kamera"));
            voiceCameraShareBtn->setStyleSheet("");
        }
    } else {
        voiceSharingCamera = true;
        if (!isCameraOn) toggleCamera(); // kamerayı aç → lastCameraFramePayload dolar
        signalingClient->startVoiceShare(currentVoiceChannelId, "camera");
        makeVoiceMediaTile(currentUserName, "camera");
        rebuildVoiceMediaGrid();
        voiceCameraShareTimer->start();
        if (voiceCameraShareBtn) {
            voiceCameraShareBtn->setText(QString::fromUtf8("📷  Durdur"));
            voiceCameraShareBtn->setStyleSheet("background:#3a1a22; color:#ffd5dd;");
        }
    }
}

void MainWindow::stopAllVoiceShares() {
    if (voiceSharingScreen) {
        voiceSharingScreen = false;
        if (voiceScreenShareTimer) voiceScreenShareTimer->stop();
        if (currentVoiceChannelId > 0) signalingClient->stopVoiceShare(currentVoiceChannelId, "screen");
    }
    if (voiceSharingCamera) {
        voiceSharingCamera = false;
        if (voiceCameraShareTimer) voiceCameraShareTimer->stop();
        if (currentVoiceChannelId > 0) signalingClient->stopVoiceShare(currentVoiceChannelId, "camera");
        if (isCameraOn) toggleCamera();
    }
    // Tüm tile'ları temizle
    for (const QString &k : voiceMediaTiles.keys()) {
        if (QWidget *w = voiceMediaTiles.value(k)) w->deleteLater();
    }
    voiceMediaTiles.clear();
    voiceMediaTileLabels.clear();
    rebuildVoiceMediaGrid();
}

void MainWindow::onVoiceShareStarted(int channelId, qint64 userId, const QString &username, const QString &kind) {
    Q_UNUSED(userId);
    if (channelId != currentVoiceChannelId) return;
    const QString key = voiceTileKey(username, kind);
    if (!voiceMediaTiles.contains(key)) {
        makeVoiceMediaTile(username, kind);
        rebuildVoiceMediaGrid();
    }
    if (username != currentUserName) {
        showToast(QString::fromUtf8("%1 %2 paylaşımına başladı").arg(
            username, kind == "screen" ? QString::fromUtf8("ekran") : QString::fromUtf8("kamera")),
            "info", 2500);
    }
}

void MainWindow::onVoiceShareStopped(int channelId, qint64 userId, const QString &username, const QString &kind) {
    Q_UNUSED(userId);
    if (channelId != currentVoiceChannelId) return;
    removeVoiceMediaTile(username, kind);
}

void MainWindow::onVoiceMediaChunk(int channelId, qint64 userId, const QString &username,
                                   const QString &kind, const QByteArray &jpeg) {
    Q_UNUSED(userId);
    if (channelId != currentVoiceChannelId) return;
    if (jpeg.isEmpty()) return;
    const QString key = voiceTileKey(username, kind);
    if (!voiceMediaTiles.contains(key)) {
        makeVoiceMediaTile(username, kind);
        rebuildVoiceMediaGrid();
    }
    QLabel *lbl = voiceMediaTileLabels.value(key);
    if (!lbl) return;
    QPixmap pm;
    if (!pm.loadFromData(jpeg, "JPG")) return;
    lbl->setPixmap(pm.scaled(lbl->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    // Fullscreen viewer açıksa onu da güncelle
    if (voiceFullscreenLabel && voiceFullscreenUser == username && voiceFullscreenKind == kind) {
        voiceFullscreenLabel->setPixmap(pm.scaled(voiceFullscreenLabel->size(),
            Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

// =====================================================================
//          Parolasız Giriş Dialog (telefon/e-posta + kod)
// =====================================================================

void MainWindow::showPasswordlessLoginDialog() {
    showDialogBackdrop();
    QDialog d(this);
    d.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    d.setAttribute(Qt::WA_TranslucentBackground);
    d.setMinimumSize(460, 540);

    auto *outerL = new QVBoxLayout(&d);
    outerL->setContentsMargins(0, 0, 0, 0);
    auto *cardW = new QWidget(&d);
    cardW->setObjectName("card");
    cardW->setStyleSheet((QString(kPrettyDialogQss) +
        "QLabel#t{color:%1; font-size:18px; font-weight:700;"
        " background:transparent; border:none;}"
        "QLabel#muted{color:%2; font-size:12.5px; line-height:1.5;"
        " background:transparent; border:none;}"
        "QPushButton#closeX{background:transparent; color:%2; border:none;"
        " font-size:18px; font-weight:700; min-width:28px; max-width:28px;"
        " min-height:28px; max-height:28px; border-radius:14px;}"
        "QPushButton#closeX:hover{background:rgba(255,255,255,0.08); color:%1;}"
    ).arg(T::Text, T::Sub));
    outerL->addWidget(cardW);
    auto *root = new QVBoxLayout(cardW);
    root->setContentsMargins(28, 22, 28, 24);
    root->setSpacing(14);

    // Header
    auto *hdr = new QHBoxLayout();
    auto *title = new QLabel(QString::fromUtf8("Kod ile Giriş"));
    title->setObjectName("t");
    hdr->addWidget(title);
    hdr->addStretch();
    auto *closeXBtn = new QPushButton(QString::fromUtf8("✕"));
    closeXBtn->setObjectName("closeX");
    closeXBtn->setCursor(Qt::PointingHandCursor);
    hdr->addWidget(closeXBtn);
    root->addLayout(hdr);
    connect(closeXBtn, &QPushButton::clicked, &d, &QDialog::reject);

    auto *info = new QLabel(QString::fromUtf8(
        "E-posta adresini gir, sana 6 haneli tek kullanımlık bir kod gönderelim."));
    info->setObjectName("muted");
    info->setWordWrap(true);
    root->addWidget(info);
    root->addSpacing(6);

    // Aşama 1: identifier (sadece e-posta)
    const QString plainInputCss = QString(
        "QLineEdit{background:%1; color:%2; border:1px solid %3;"
        " border-radius:10px; padding:0 14px; font-size:13px; min-height:44px;}"
        "QLineEdit:focus{border:1px solid %4;}").arg(T::Input, T::Text, T::Border, T::Accent);

    auto *idEdit = new QLineEdit();
    idEdit->setPlaceholderText(QString::fromUtf8("e-posta@ornek.com"));
    idEdit->setStyleSheet(plainInputCss);
    root->addWidget(idEdit);

    auto *sendBtn = new QPushButton(QString::fromUtf8("Kod Gönder"));
    sendBtn->setCursor(Qt::PointingHandCursor);
    sendBtn->setMinimumHeight(44);
    root->addWidget(sendBtn);

    // Aşama 2: code (gizli, kod gönderildikten sonra görünür)
    auto *codeWrap = new QWidget();
    codeWrap->setStyleSheet("background:transparent;");
    auto *codeLay = new QVBoxLayout(codeWrap);
    codeLay->setContentsMargins(0, 10, 0, 0);
    codeLay->setSpacing(10);

    auto *targetLbl = new QLabel();
    targetLbl->setObjectName("muted");
    targetLbl->setWordWrap(true);
    targetLbl->setAlignment(Qt::AlignHCenter);
    codeLay->addWidget(targetLbl);

    auto *codeEdit = new QLineEdit();
    codeEdit->setPlaceholderText(QString::fromUtf8("000 000"));
    codeEdit->setAlignment(Qt::AlignCenter);
    codeEdit->setMaxLength(8);
    codeEdit->setStyleSheet(QString(
        "QLineEdit{background:%1; color:%2; border:1px solid %3;"
        " border-radius:10px; padding:0 14px; min-height:50px;"
        " font-size:22px; font-weight:700; letter-spacing:10px;}"
        "QLineEdit:focus{border:1px solid %4;}").arg(T::Input, T::Text, T::Border, T::Accent));
    codeLay->addWidget(codeEdit);

    auto *verifyBtn = new QPushButton(QString::fromUtf8("Giriş Yap"));
    verifyBtn->setCursor(Qt::PointingHandCursor);
    verifyBtn->setMinimumHeight(44);
    codeLay->addWidget(verifyBtn);

    auto *resendLink = new QPushButton(QString::fromUtf8("Kodu yeniden gönder"));
    resendLink->setFlat(true);
    resendLink->setCursor(Qt::PointingHandCursor);
    resendLink->setStyleSheet(QString(
        "QPushButton{background:transparent; border:none; color:%1; font-size:12px;}"
        "QPushButton:hover{color:%2; text-decoration:underline;}").arg(T::Sub, T::Accent));
    codeLay->addWidget(resendLink, 0, Qt::AlignHCenter);

    codeWrap->hide();
    root->addWidget(codeWrap);

    // Status / error
    auto *status = new QLabel();
    status->setObjectName("muted");
    status->setWordWrap(true);
    status->setAlignment(Qt::AlignHCenter);
    status->hide();
    root->addWidget(status);

    auto setError = [status](const QString &m, bool isErr = true) {
        status->setText(m);
        status->setStyleSheet(QString("color:%1; font-size:12px; background:transparent; border:none;")
            .arg(isErr ? T::Danger : T::Success));
        status->show();
    };

    auto doSend = [this, idEdit, sendBtn, status]() {
        const QString id = idEdit->text().trimmed();
        if (id.isEmpty()) return;
        sendBtn->setEnabled(false);
        status->hide();
        signalingClient->requestLoginCode(id);
    };
    connect(sendBtn, &QPushButton::clicked, &d, doSend);
    connect(idEdit, &QLineEdit::returnPressed, &d, doSend);
    connect(resendLink, &QPushButton::clicked, &d, [this, idEdit]() {
        const QString id = idEdit->text().trimmed();
        if (!id.isEmpty()) signalingClient->requestLoginCode(id);
    });

    auto doVerify = [this, idEdit, codeEdit, verifyBtn, status]() {
        const QString id = idEdit->text().trimmed();
        const QString c = codeEdit->text().trimmed();
        if (id.isEmpty() || c.length() < 4) return;
        verifyBtn->setEnabled(false);
        status->hide();
        signalingClient->verifyLoginCode(id, c);
    };
    connect(verifyBtn, &QPushButton::clicked, &d, doVerify);
    connect(codeEdit, &QLineEdit::returnPressed, &d, doVerify);

    // Server cevapları
    auto rConn = connect(signalingClient, &SignalingClient::requestLoginCodeResult, &d,
        [=](bool ok, const QString &channel, const QString &target,
            bool dev, const QString &userName, const QString &error) {
        Q_UNUSED(userName);
        sendBtn->setEnabled(true);
        if (!ok) { setError(error.isEmpty() ? QString::fromUtf8("Kod gönderilemedi.") : error); return; }
        const QString chTr = (channel == "email")
            ? QString::fromUtf8("e-posta") : QString::fromUtf8("SMS");
        targetLbl->setText(QString::fromUtf8(
            "Kod %1 ile <b>%2</b> adresine gönderildi.").arg(chTr, target));
        targetLbl->setTextFormat(Qt::RichText);
        codeWrap->show();
        codeEdit->setFocus();
        if (dev) setError(QString::fromUtf8("Geliştirici modu — SMS yerine sunucu konsoluna yazıldı."), false);
        else status->hide();
    });
    auto vConn = connect(signalingClient, &SignalingClient::verifyLoginCodeResult, &d,
        [=, &d](bool ok, const QString &userName, const QString &error) {
        verifyBtn->setEnabled(true);
        if (!ok) { setError(error.isEmpty() ? QString::fromUtf8("Kod doğrulanamadı.") : error); return; }
        // Login finalize akışı: sunucu zaten login_result gönderdi → onLoginResult çağrılır
        // Burada sadece dialog'u kapat
        Q_UNUSED(userName);
        d.accept();
    });

    idEdit->setFocus();
    d.exec();
    disconnect(rConn);
    disconnect(vConn);
    hideDialogBackdrop();
}

// =====================================================================
//                    2FA: Login challenge + Security dialog
// =====================================================================

void MainWindow::showLogin2faDialog(bool totp, bool sms, bool email,
                                    const QString &phoneHint, const QString &emailHint) {
    showDialogBackdrop();
    QDialog d(this);
    d.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    d.setAttribute(Qt::WA_TranslucentBackground);
    d.setMinimumWidth(420);

    auto *outerL = new QVBoxLayout(&d);
    outerL->setContentsMargins(0, 0, 0, 0);
    auto *cardW = new QWidget(&d);
    cardW->setObjectName("card");
    cardW->setStyleSheet((QString(kPrettyDialogQss) +
        "QLabel#t{color:%1; font-size:18px; font-weight:700;"
        " background:transparent; border:none;}"
        "QLineEdit{font-size:18px; letter-spacing:4px;}"
        "QPushButton#closeX{background:transparent; color:%2; border:none;"
        " font-size:18px; font-weight:700; min-width:28px; max-width:28px;"
        " min-height:28px; max-height:28px; border-radius:14px;}"
        "QPushButton#closeX:hover{background:rgba(255,255,255,0.08); color:%1;}"
    ).arg(T::Text, T::Sub));
    outerL->addWidget(cardW);
    auto *root = new QVBoxLayout(cardW);
    root->setContentsMargins(22, 14, 22, 18);
    root->setSpacing(12);

    auto *hdr = new QHBoxLayout();
    auto *t = new QLabel(QString::fromUtf8("Doğrulama kodu"));
    t->setObjectName("t");
    hdr->addWidget(t);
    hdr->addStretch();
    auto *closeX = new QPushButton(QString::fromUtf8("✕"));
    closeX->setObjectName("closeX");
    closeX->setCursor(Qt::PointingHandCursor);
    hdr->addWidget(closeX);
    root->addLayout(hdr);
    connect(closeX, &QPushButton::clicked, &d, &QDialog::reject);

    QStringList opts;
    if (totp)  opts << QString::fromUtf8("Authenticator uygulamandaki 6 haneli kod");
    if (sms)   opts << QString::fromUtf8("%1 numarasına SMS ile gelen kod").arg(phoneHint.isEmpty() ? QString::fromUtf8("telefon") : phoneHint);
    if (email) opts << QString::fromUtf8("%1 adresine gönderilen e-posta kodu").arg(emailHint.isEmpty() ? QString::fromUtf8("e-posta") : emailHint);
    QString hint;
    if (opts.size() == 1) hint = opts.first() + QString::fromUtf8("u gir.");
    else if (opts.size() > 1) hint = QString::fromUtf8("Herhangi birini kullanabilirsin:\n• ") + opts.join(QString::fromUtf8("\n• "));
    auto *hintLbl = new QLabel(hint);
    hintLbl->setWordWrap(true);
    hintLbl->setStyleSheet(QString("color:%1; font-size:12px;").arg(T::Sub));
    root->addWidget(hintLbl);

    auto *edit = new QLineEdit();
    edit->setPlaceholderText("123 456");
    edit->setAlignment(Qt::AlignCenter);
    edit->setMaxLength(8);
    root->addWidget(edit);

    auto *err = new QLabel();
    err->setStyleSheet(QString("color:%1; font-size:12px;").arg(T::Danger));
    err->hide();
    root->addWidget(err);

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto *cancel = new QPushButton(QString::fromUtf8("Vazgeç"));
    cancel->setObjectName("ghost");
    auto *okBtn = new QPushButton(QString::fromUtf8("Doğrula"));
    btnRow->addWidget(cancel);
    btnRow->addWidget(okBtn);
    root->addLayout(btnRow);

    okBtn->setDefault(true);
    okBtn->setAutoDefault(true);
    auto submit = [this, edit, err, okBtn]() {
        const QString code = edit->text().trimmed();
        if (code.length() < 4) { err->setText(QString::fromUtf8("Kod eksik.")); err->show(); return; }
        // Button'u disable etme — server yanıtı gecikirse user ikinci kez deneyebilsin.
        // Sadece görsel feedback için text değişsin.
        okBtn->setText(QString::fromUtf8("Doğrulanıyor..."));
        signalingClient->verifyLogin2fa(code);
    };
    connect(okBtn, &QPushButton::clicked, &d, submit);
    connect(edit, &QLineEdit::returnPressed, &d, submit);
    connect(cancel, &QPushButton::clicked, &d, &QDialog::reject);

    auto conn = connect(signalingClient, &SignalingClient::login2faResult, &d,
        [&d, err, okBtn](bool ok, const QString &, const QString &, const QString &error) {
        if (ok) { d.accept(); return; }
        err->setText(error.isEmpty() ? QString::fromUtf8("Kod hatalı.") : error);
        err->show();
        okBtn->setText(QString::fromUtf8("Doğrula"));
    });
    edit->setFocus();
    d.exec();
    disconnect(conn);
    hideDialogBackdrop();
}

void MainWindow::showSecurityDialog() {
    showDialogBackdrop();
    QDialog d(this);
    d.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    d.setAttribute(Qt::WA_TranslucentBackground);
    d.setMinimumSize(560, 700);
    if (auto *parent = parentWidget() ? parentWidget() : this) {
        const int maxH = int(parent->height() * 0.92);
        if (maxH > 0) d.setMaximumHeight(maxH);
    }

    auto *outerL = new QVBoxLayout(&d);
    outerL->setContentsMargins(0, 0, 0, 0);
    auto *cardW = new QWidget(&d);
    cardW->setObjectName("card");
    cardW->setStyleSheet((QString(kPrettyDialogQss) +
        "QLabel#t{color:%1; font-size:20px; font-weight:700;}"
        "QLabel#sec{color:%1; font-size:14px; font-weight:700;}"
        "QLabel#muted{color:%2; font-size:12px;}"
        "QLabel#ok{color:%3; font-weight:700;}"
        "QLabel#warn{color:%4; font-weight:700;}"
        "QFrame#subcard{background:%5; border:1px solid %6; border-radius:12px;}"
        "QPushButton#closeX{background:transparent; color:%2; border:none;"
        " font-size:18px; font-weight:700; min-width:28px; max-width:28px;"
        " min-height:28px; max-height:28px; border-radius:14px;}"
        "QPushButton#closeX:hover{background:rgba(255,255,255,0.08); color:%1;}"
        "QScrollArea{background:transparent; border:none;}"
        "QScrollArea > QWidget{background:transparent;}"
        "QScrollArea > QWidget > QWidget{background:transparent;}"
    ).arg(T::Text, T::Sub, T::Success, T::Danger, T::Card, T::Border));
    outerL->addWidget(cardW);
    auto *cardL = new QVBoxLayout(cardW);
    cardL->setContentsMargins(0, 0, 0, 0);
    cardL->setSpacing(0);

    // İçerik scrollable olsun ki TOTP setup açılınca alta sıkışmasın
    auto *scroll = new QScrollArea(cardW);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *scrollHost = new QWidget();
    scroll->setWidget(scrollHost);
    cardL->addWidget(scroll);
    auto *root = new QVBoxLayout(scrollHost);
    root->setContentsMargins(22, 14, 22, 18);
    root->setSpacing(14);

    // Header — başlık + sağ üstte X
    auto *hdr = new QHBoxLayout();
    auto *title = new QLabel(QString::fromUtf8("Hesap Güvenliği"));
    title->setObjectName("t");
    hdr->addWidget(title);
    hdr->addStretch();
    auto *closeXBtn = new QPushButton(QString::fromUtf8("✕"));
    closeXBtn->setObjectName("closeX");
    closeXBtn->setCursor(Qt::PointingHandCursor);
    closeXBtn->setToolTip(QString::fromUtf8("Kapat"));
    hdr->addWidget(closeXBtn);
    root->addLayout(hdr);
    connect(closeXBtn, &QPushButton::clicked, &d, &QDialog::reject);

    // --- TOTP kartı ---
    auto *totpCard = new QFrame();
    totpCard->setObjectName("subcard");
    auto *tcL = new QVBoxLayout(totpCard);
    tcL->setContentsMargins(14, 12, 14, 12);
    tcL->setSpacing(8);
    auto *totpHeader = new QHBoxLayout();
    auto *totpTitle = new QLabel(QString::fromUtf8("Authenticator (TOTP)"));
    totpTitle->setObjectName("sec");
    auto *totpStatus = new QLabel(QString::fromUtf8("Yükleniyor..."));
    totpStatus->setObjectName("muted");
    totpHeader->addWidget(totpTitle);
    totpHeader->addStretch();
    totpHeader->addWidget(totpStatus);
    tcL->addLayout(totpHeader);
    auto *totpHint = new QLabel(QString::fromUtf8(
        "Google Authenticator, Authy, 1Password gibi bir uygulama ile kullan."));
    totpHint->setObjectName("muted");
    totpHint->setWordWrap(true);
    tcL->addWidget(totpHint);
    // ----- TOTP setup container (gizli; Etkinleştir'e basınca açılır) -----
    auto *totpSetupBox = new QWidget();
    totpSetupBox->setStyleSheet("background:transparent;");
    auto *setupL = new QVBoxLayout(totpSetupBox);
    setupL->setContentsMargins(0, 6, 0, 0);
    setupL->setSpacing(12);
    setupL->setAlignment(Qt::AlignHCenter);

    // 1) Adım yazısı
    auto *step1 = new QLabel(QString::fromUtf8(
        "1. Authenticator uygulamana QR kodu tarat veya gizli anahtarı manuel gir."));
    step1->setObjectName("muted");
    step1->setWordWrap(true);
    step1->setAlignment(Qt::AlignHCenter);
    setupL->addWidget(step1);

    // 2) QR kod
    auto *totpQrLabel = new QLabel();
    totpQrLabel->setAlignment(Qt::AlignCenter);
    totpQrLabel->setStyleSheet(
        "QLabel{background:#ffffff; border-radius:14px; padding:12px;}");
    {
        auto *qrRow = new QHBoxLayout();
        qrRow->addStretch();
        qrRow->addWidget(totpQrLabel);
        qrRow->addStretch();
        setupL->addLayout(qrRow);
    }

    // 3) Secret + kopyala
    auto *secretRow = new QHBoxLayout();
    secretRow->setSpacing(8);
    auto *totpSecretLbl = new QLineEdit();
    totpSecretLbl->setReadOnly(true);
    totpSecretLbl->setAlignment(Qt::AlignCenter);
    totpSecretLbl->setStyleSheet(QString(
        "QLineEdit{background:%1; color:%2; border:1px solid %3;"
        " border-radius:10px; padding:10px 14px;"
        " font-family:'Consolas','Cascadia Code',monospace;"
        " font-size:13px; letter-spacing:2px; font-weight:600;}").arg(T::Input, T::Text, T::Border));
    auto *totpCopyBtn = new QPushButton(QString::fromUtf8("Kopyala"));
    totpCopyBtn->setObjectName("ghost");
    totpCopyBtn->setCursor(Qt::PointingHandCursor);
    secretRow->addWidget(totpSecretLbl, 1);
    secretRow->addWidget(totpCopyBtn);
    setupL->addLayout(secretRow);

    // 4) Adım 2
    auto *step2 = new QLabel(QString::fromUtf8(
        "2. Uygulamadaki 6 haneli kodu aşağıya gir."));
    step2->setObjectName("muted");
    step2->setWordWrap(true);
    step2->setAlignment(Qt::AlignHCenter);
    setupL->addSpacing(4);
    setupL->addWidget(step2);

    auto *totpCodeEdit = new QLineEdit();
    totpCodeEdit->setPlaceholderText(QString::fromUtf8("000 000"));
    totpCodeEdit->setMaxLength(8);
    totpCodeEdit->setAlignment(Qt::AlignCenter);
    totpCodeEdit->setStyleSheet(QString(
        "QLineEdit{background:%1; color:%2; border:1px solid %3;"
        " border-radius:10px; padding:0 14px; min-height:46px;"
        " font-size:20px; font-weight:700; letter-spacing:8px;}"
        "QLineEdit:focus{border:1px solid %4;}").arg(T::Input, T::Text, T::Border, T::Accent));
    setupL->addWidget(totpCodeEdit);

    auto *totpConfirmBtn = new QPushButton(QString::fromUtf8("Doğrula ve Aç"));
    totpConfirmBtn->setCursor(Qt::PointingHandCursor);
    totpConfirmBtn->setMinimumHeight(44);
    setupL->addWidget(totpConfirmBtn);

    totpSetupBox->hide();
    tcL->addWidget(totpSetupBox);

    // Enable / Disable buton satırı (setup açılmadığında görünür)
    auto *totpRow = new QHBoxLayout();
    auto *totpEnableBtn = new QPushButton(QString::fromUtf8("Etkinleştir"));
    totpEnableBtn->setCursor(Qt::PointingHandCursor);
    auto *totpDisableBtn = new QPushButton(QString::fromUtf8("Devre Dışı Bırak"));
    totpDisableBtn->setObjectName("danger");
    totpDisableBtn->setCursor(Qt::PointingHandCursor);
    totpDisableBtn->hide();
    totpRow->addWidget(totpEnableBtn);
    totpRow->addStretch();
    totpRow->addWidget(totpDisableBtn);
    tcL->addLayout(totpRow);

    // Kopyala
    connect(totpCopyBtn, &QPushButton::clicked, this, [this, totpSecretLbl]() {
        QGuiApplication::clipboard()->setText(totpSecretLbl->text());
        showToast(QString::fromUtf8("Gizli anahtar kopyalandı"), "info", 1400);
    });

    root->addWidget(totpCard);

    // --- SMS kartı ---
    auto *smsCard = new QFrame();
    smsCard->setObjectName("subcard");
    auto *sL = new QVBoxLayout(smsCard);
    sL->setContentsMargins(14, 12, 14, 12);
    sL->setSpacing(8);
    auto *smsHeader = new QHBoxLayout();
    auto *smsTitle = new QLabel(QString::fromUtf8("SMS (Twilio)"));
    smsTitle->setObjectName("sec");
    auto *smsStatus = new QLabel(QString::fromUtf8("Yükleniyor..."));
    smsStatus->setObjectName("muted");
    smsHeader->addWidget(smsTitle);
    smsHeader->addStretch();
    smsHeader->addWidget(smsStatus);
    sL->addLayout(smsHeader);
    auto *phoneEdit = new QLineEdit();
    phoneEdit->setPlaceholderText(QString::fromUtf8("Telefon (+905551234567)"));
    sL->addWidget(phoneEdit);
    auto *phoneRow = new QHBoxLayout();
    auto *savePhoneBtn = new QPushButton(QString::fromUtf8("Numarayı Kaydet"));
    savePhoneBtn->setObjectName("ghost");
    auto *sendCodeBtn = new QPushButton(QString::fromUtf8("Kod Gönder"));
    phoneRow->addWidget(savePhoneBtn);
    phoneRow->addWidget(sendCodeBtn);
    phoneRow->addStretch();
    sL->addLayout(phoneRow);
    auto *smsCodeEdit = new QLineEdit();
    smsCodeEdit->setPlaceholderText(QString::fromUtf8("SMS ile gelen kod"));
    smsCodeEdit->setMaxLength(8);
    sL->addWidget(smsCodeEdit);
    auto *verifyCodeBtn = new QPushButton(QString::fromUtf8("Kodu Doğrula"));
    sL->addWidget(verifyCodeBtn);
    auto *smsToggleBtn = new QPushButton(QString::fromUtf8("SMS 2FA Etkinleştir"));
    smsToggleBtn->setObjectName("ghost");
    sL->addWidget(smsToggleBtn);
    root->addWidget(smsCard);
    // SMS desteği kaldırıldı — kart gizleniyor (yalnızca e-posta/TOTP).
    smsCard->hide();

    // msg label (diğer kartların result handler'ları burayı kullanır)
    auto *msg = new QLabel();
    msg->setObjectName("muted");
    msg->setWordWrap(true);
    // Hata için kırmızı stil; helper'lar
    auto setMsgError = [msg](const QString &t) {
        msg->setStyleSheet(QString(
            "background:rgba(239,68,68,0.10); border:1px solid rgba(239,68,68,0.35);"
            "border-radius:8px; padding:8px 12px; color:%1; font-size:12.5px; font-weight:600;"
        ).arg(T::Danger));
        msg->setText(t);
        msg->show();
    };
    auto clearMsg = [msg]() {
        msg->setStyleSheet("");
        msg->setText("");
    };

    // forward-declared email state setter (gerçek hali email card oluşturulunca atanır)
    auto *emailStatusSetter = new std::function<void(bool)>([](bool){});

    // --- E-posta kartı ---
    auto *emailCard = new QFrame();
    emailCard->setObjectName("subcard");
    auto *eL = new QVBoxLayout(emailCard);
    eL->setContentsMargins(14, 12, 14, 12);
    eL->setSpacing(8);
    auto *emailHeader = new QHBoxLayout();
    auto *emailTitle = new QLabel(QString::fromUtf8("E-posta Kodu"));
    emailTitle->setObjectName("sec");
    auto *emailStatus = new QLabel(QString::fromUtf8("Yükleniyor..."));
    emailStatus->setObjectName("muted");
    emailHeader->addWidget(emailTitle);
    emailHeader->addStretch();
    emailHeader->addWidget(emailStatus);
    eL->addLayout(emailHeader);
    auto *emailHint = new QLabel(QString::fromUtf8(
        "Giriş sırasında hesabına kayıtlı e-posta adresine 6 haneli kod gönderilir."));
    emailHint->setObjectName("muted");
    emailHint->setWordWrap(true);
    eL->addWidget(emailHint);
    auto *emailToggleBtn = new QPushButton(QString::fromUtf8("E-posta 2FA Etkinleştir"));
    emailToggleBtn->setObjectName("ghost");
    eL->addWidget(emailToggleBtn);
    root->addWidget(emailCard);

    // Email state setter (yukarıda forward-declared)
    bool *emailEnabledCache = new bool(false);
    *emailStatusSetter = [=](bool enabled) {
        *emailEnabledCache = enabled;
        emailStatus->setText(enabled ? QString::fromUtf8("Etkin ✓") : QString::fromUtf8("Kapalı"));
        emailStatus->setObjectName(enabled ? "ok" : "muted");
        emailStatus->style()->unpolish(emailStatus);
        emailStatus->style()->polish(emailStatus);
        emailToggleBtn->setText(enabled
            ? QString::fromUtf8("E-posta 2FA Kapat")
            : QString::fromUtf8("E-posta 2FA Etkinleştir"));
    };
    connect(&d, &QDialog::finished, [emailEnabledCache]() { delete emailEnabledCache; });
    // TOTP enabled cache — securityStatus signal handler ileride bunu güncelleyecek
    bool *totpEnabledCache = new bool(false);
    connect(emailToggleBtn, &QPushButton::clicked, this,
            [this, &d, emailEnabledCache, totpEnabledCache]() {
        const bool currentlyOn = *emailEnabledCache;
        if (!currentlyOn) {
            // Açma: doğrudan
            signalingClient->toggleEmail2fa(true);
            return;
        }
        // Kapatma: TOTP açıksa authenticator kodu, değilse onay dialog
        if (*totpEnabledCache) {
            QDialog dlg(&d);
            dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
            dlg.setAttribute(Qt::WA_TranslucentBackground);
            dlg.setMinimumWidth(420);
            auto *o = new QVBoxLayout(&dlg);
            o->setContentsMargins(0, 0, 0, 0);
            auto *card = new QWidget(&dlg);
            card->setObjectName("card");
            card->setStyleSheet(QString(kPrettyDialogQss) +
                QString("QLabel#t{color:%1; font-size:17px; font-weight:700; background:transparent; border:none;}"
                        "QLabel#m{color:%2; font-size:12.5px; background:transparent; border:none;}")
                .arg(T::Text, T::Sub));
            o->addWidget(card);
            auto *cl = new QVBoxLayout(card);
            cl->setContentsMargins(24, 22, 24, 22);
            cl->setSpacing(12);
            auto *t = new QLabel(QString::fromUtf8("E-posta 2FA'yı Kapat"));
            t->setObjectName("t");
            cl->addWidget(t);
            auto *m = new QLabel(QString::fromUtf8(
                "Hesabında authenticator açık. Onaylamak için <b>6 haneli kodu</b> gir."));
            m->setObjectName("m");
            m->setWordWrap(true);
            m->setTextFormat(Qt::RichText);
            cl->addWidget(m);
            auto *codeIn = new QLineEdit();
            codeIn->setPlaceholderText(QString::fromUtf8("000 000"));
            codeIn->setMaxLength(8);
            codeIn->setAlignment(Qt::AlignCenter);
            codeIn->setStyleSheet(QString(
                "QLineEdit{background:%1; color:%2; border:1px solid %3;"
                " border-radius:10px; padding:0 14px; min-height:48px;"
                " font-size:20px; font-weight:700; letter-spacing:8px;}"
                "QLineEdit:focus{border:1px solid %4;}").arg(T::Input, T::Text, T::Border, T::Accent));
            cl->addWidget(codeIn);
            auto *btnRow = new QHBoxLayout();
            auto *cancel = new QPushButton(QString::fromUtf8("Vazgeç"));
            cancel->setObjectName("ghost"); cancel->setCursor(Qt::PointingHandCursor);
            auto *confirm = new QPushButton(QString::fromUtf8("Devre Dışı Bırak"));
            confirm->setObjectName("danger"); confirm->setCursor(Qt::PointingHandCursor);
            confirm->setMinimumHeight(40);
            btnRow->addWidget(cancel); btnRow->addStretch(); btnRow->addWidget(confirm);
            cl->addLayout(btnRow);
            connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
            auto submit = [this, codeIn, &dlg]() {
                const QString c = codeIn->text().trimmed().remove(' ');
                if (c.length() < 6) return;
                signalingClient->toggleEmail2fa(false, c);
                dlg.accept();
            };
            connect(confirm, &QPushButton::clicked, &dlg, submit);
            connect(codeIn, &QLineEdit::returnPressed, &dlg, submit);
            codeIn->setFocus();
            dlg.exec();
        } else {
            // TOTP kapalı — basit onay yeterli
            QDialog dlg(&d);
            dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
            dlg.setAttribute(Qt::WA_TranslucentBackground);
            dlg.setMinimumWidth(400);
            auto *o = new QVBoxLayout(&dlg);
            o->setContentsMargins(0, 0, 0, 0);
            auto *card = new QWidget(&dlg);
            card->setObjectName("card");
            card->setStyleSheet(QString(kPrettyDialogQss) +
                QString("QLabel#t{color:%1; font-size:17px; font-weight:700; background:transparent; border:none;}"
                        "QLabel#m{color:%2; font-size:12.5px; background:transparent; border:none;}")
                .arg(T::Text, T::Sub));
            o->addWidget(card);
            auto *cl = new QVBoxLayout(card);
            cl->setContentsMargins(24, 22, 24, 22);
            cl->setSpacing(12);
            auto *t = new QLabel(QString::fromUtf8("E-posta 2FA'yı Kapat"));
            t->setObjectName("t");
            cl->addWidget(t);
            auto *m = new QLabel(QString::fromUtf8(
                "İki adımlı doğrulama kapatılacak. Hesabın yalnızca şifre ile korunacak. Devam edilsin mi?"));
            m->setObjectName("m");
            m->setWordWrap(true);
            cl->addWidget(m);
            auto *btnRow = new QHBoxLayout();
            auto *cancel = new QPushButton(QString::fromUtf8("Vazgeç"));
            cancel->setObjectName("ghost"); cancel->setCursor(Qt::PointingHandCursor);
            auto *confirm = new QPushButton(QString::fromUtf8("Evet, Kapat"));
            confirm->setObjectName("danger"); confirm->setCursor(Qt::PointingHandCursor);
            confirm->setMinimumHeight(40);
            btnRow->addWidget(cancel); btnRow->addStretch(); btnRow->addWidget(confirm);
            cl->addLayout(btnRow);
            connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
            connect(confirm, &QPushButton::clicked, this, [this, &dlg]() {
                signalingClient->toggleEmail2fa(false);
                dlg.accept();
            });
            dlg.exec();
        }
    });
    connect(signalingClient, &SignalingClient::toggleEmail2faResult, &d,
        [=](bool ok, bool enabled, const QString &error) {
            if (ok) {
                showToast(enabled
                    ? QString::fromUtf8("E-posta 2FA etkinleştirildi")
                    : QString::fromUtf8("E-posta 2FA kapatıldı"), "success", 1800);
                clearMsg();
            } else {
                setMsgError(error.isEmpty() ? QString::fromUtf8("İşlem başarısız.") : error);
            }
        });

    root->addWidget(msg);

    auto *closeRow = new QHBoxLayout();
    closeRow->addStretch();
    auto *closeBtn = new QPushButton(QString::fromUtf8("Kapat"));
    closeBtn->setObjectName("ghost");
    closeRow->addWidget(closeBtn);
    root->addLayout(closeRow);
    connect(closeBtn, &QPushButton::clicked, &d, &QDialog::accept);

    // --- State + signals ---
    auto setTotpState = [=](bool enabled) {
        if (enabled) {
            totpStatus->setText(QString::fromUtf8("Etkin"));
            totpStatus->setObjectName("ok");
            totpEnableBtn->hide();
            totpSetupBox->hide();
            totpDisableBtn->show();
        } else {
            totpStatus->setText(QString::fromUtf8("Kapalı"));
            totpStatus->setObjectName("muted");
            totpEnableBtn->show();
            totpSetupBox->hide();
            totpDisableBtn->hide();
        }
        totpStatus->style()->unpolish(totpStatus);
        totpStatus->style()->polish(totpStatus);
    };
    auto setSmsState = [=](bool phoneVerified, bool smsEnabled, const QString &masked, bool twilioOk) {
        QString st;
        if (smsEnabled) st = QString::fromUtf8("Etkin ✓  ") + masked;
        else if (phoneVerified) st = QString::fromUtf8("Numara doğrulandı  ") + masked;
        else st = QString::fromUtf8("Kapalı");
        if (!twilioOk) st += QString::fromUtf8("  (Twilio yok — dev modda konsola yazıyor)");
        smsStatus->setText(st);
        smsToggleBtn->setText(smsEnabled ? QString::fromUtf8("SMS 2FA Kapat") : QString::fromUtf8("SMS 2FA Etkinleştir"));
        smsToggleBtn->setEnabled(phoneVerified || smsEnabled);
    };

    connect(signalingClient, &SignalingClient::securityStatus, &d,
        [=](bool totpEn, bool smsEn, bool emailEn, bool pVer,
            const QString &phone, const QString &masked, const QString &emailMasked, bool twOk) {
            Q_UNUSED(emailMasked);
            *totpEnabledCache = totpEn;
            setTotpState(totpEn);
            setSmsState(pVer, smsEn, masked, twOk);
            (*emailStatusSetter)(emailEn);
            if (!phone.isEmpty() && phoneEdit->text().isEmpty()) phoneEdit->setText(phone);
        });
    connect(&d, &QDialog::finished, [emailStatusSetter, totpEnabledCache]() {
        delete emailStatusSetter; delete totpEnabledCache;
    });
    connect(signalingClient, &SignalingClient::totpSetup, &d,
        [=](const QString &secret, const QString &otpauth) {
            const QPixmap qr = makeQrPixmap(otpauth, 240, 4);
            if (!qr.isNull()) {
                totpQrLabel->setPixmap(qr);
                totpQrLabel->setFixedSize(qr.width() + 24, qr.height() + 24);
            }
            totpSecretLbl->setText(secret);
            totpSetupBox->show();
            totpEnableBtn->hide();
            totpCodeEdit->setFocus();
            clearMsg();
        });
    connect(signalingClient, &SignalingClient::totpConfirmResult, &d,
        [=](bool ok, const QString &error) {
            if (ok) {
                totpCodeEdit->clear();
                setTotpState(true);              // server push beklemeden hemen UI güncelle
                showToast(QString::fromUtf8("TOTP etkinleştirildi"), "success", 1800);
                signalingClient->getSecurityStatus(); // sunucu state'i de yenile
                clearMsg();
            } else {
                setMsgError(error.isEmpty() ? QString::fromUtf8("Hatalı kod girdiniz. Lütfen authenticator uygulamandaki güncel kodu tekrar dene.") : error);
            }
        });
    connect(signalingClient, &SignalingClient::totpDisableResult, &d,
        [=](bool ok, const QString &error) {
            if (ok) {
                setTotpState(false);
                showToast(QString::fromUtf8("TOTP devre dışı bırakıldı"), "info", 1800);
                signalingClient->getSecurityStatus();
                clearMsg();
            } else {
                setMsgError(error.isEmpty() ? QString::fromUtf8("Hatalı kod girdiniz. Authenticator uygulamandaki güncel 6 haneli kodu tekrar dene.") : error);
            }
        });
    connect(signalingClient, &SignalingClient::setPhoneResult, &d,
        [=](bool ok, const QString &error) {
            if (ok) clearMsg(); else setMsgError(error);
        });
    connect(signalingClient, &SignalingClient::sendPhoneCodeResult, &d,
        [=](bool ok, bool dev, const QString &error) {
            if (ok) clearMsg();
            else setMsgError(error);
        });
    connect(signalingClient, &SignalingClient::verifyPhoneCodeResult, &d,
        [=](bool ok, const QString &error) {
            if (ok) { clearMsg(); smsCodeEdit->clear(); }
            else setMsgError(error);
        });
    connect(signalingClient, &SignalingClient::toggleSms2faResult, &d,
        [=](bool ok, bool enabled, const QString &error) {
            if (ok) clearMsg();
            else setMsgError(error);
        });

    // --- Button actions ---
    connect(totpEnableBtn, &QPushButton::clicked, this, [this]() {
        signalingClient->startTotpSetup();
    });
    connect(totpConfirmBtn, &QPushButton::clicked, this, [this, totpCodeEdit]() {
        const QString c = totpCodeEdit->text().trimmed();
        if (c.length() >= 6) signalingClient->confirmTotpSetup(c);
    });
    connect(totpDisableBtn, &QPushButton::clicked, this, [this, &d]() {
        // Authenticator kodu ile onaylı kapatma — pretty modal
        QDialog dlg(&d);
        dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        dlg.setAttribute(Qt::WA_TranslucentBackground);
        dlg.setMinimumWidth(420);
        auto *o = new QVBoxLayout(&dlg);
        o->setContentsMargins(0, 0, 0, 0);
        auto *card = new QWidget(&dlg);
        card->setObjectName("card");
        card->setStyleSheet(QString(kPrettyDialogQss) +
            QString("QLabel#t{color:%1; font-size:17px; font-weight:700; background:transparent; border:none;}"
                    "QLabel#m{color:%2; font-size:12.5px; background:transparent; border:none;}")
            .arg(T::Text, T::Sub));
        o->addWidget(card);
        auto *cl = new QVBoxLayout(card);
        cl->setContentsMargins(24, 22, 24, 22);
        cl->setSpacing(12);
        auto *t = new QLabel(QString::fromUtf8("İki Adımlı Doğrulamayı Kapat"));
        t->setObjectName("t");
        cl->addWidget(t);
        auto *m = new QLabel(QString::fromUtf8(
            "Devre dışı bırakmak için authenticator uygulamandaki <b>6 haneli kodu</b> gir."));
        m->setObjectName("m");
        m->setWordWrap(true);
        m->setTextFormat(Qt::RichText);
        cl->addWidget(m);
        auto *codeIn = new QLineEdit();
        codeIn->setPlaceholderText(QString::fromUtf8("000 000"));
        codeIn->setMaxLength(8);
        codeIn->setAlignment(Qt::AlignCenter);
        codeIn->setStyleSheet(QString(
            "QLineEdit{background:%1; color:%2; border:1px solid %3;"
            " border-radius:10px; padding:0 14px; min-height:48px;"
            " font-size:20px; font-weight:700; letter-spacing:8px;}"
            "QLineEdit:focus{border:1px solid %4;}").arg(T::Input, T::Text, T::Border, T::Accent));
        cl->addWidget(codeIn);
        auto *btnRow = new QHBoxLayout();
        auto *cancel = new QPushButton(QString::fromUtf8("Vazgeç"));
        cancel->setObjectName("ghost");
        cancel->setCursor(Qt::PointingHandCursor);
        auto *confirm = new QPushButton(QString::fromUtf8("Devre Dışı Bırak"));
        confirm->setObjectName("danger");
        confirm->setCursor(Qt::PointingHandCursor);
        confirm->setMinimumHeight(40);
        btnRow->addWidget(cancel);
        btnRow->addStretch();
        btnRow->addWidget(confirm);
        cl->addLayout(btnRow);

        connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
        auto submit = [this, codeIn, &dlg]() {
            const QString c = codeIn->text().trimmed().remove(' ');
            if (c.length() < 6) return;
            signalingClient->disableTotp(c);
            dlg.accept();
        };
        connect(confirm, &QPushButton::clicked, &dlg, submit);
        connect(codeIn, &QLineEdit::returnPressed, &dlg, submit);
        codeIn->setFocus();
        dlg.exec();
    });
    connect(savePhoneBtn, &QPushButton::clicked, this, [this, phoneEdit]() {
        signalingClient->setPhoneNumber(phoneEdit->text().trimmed());
    });
    connect(sendCodeBtn, &QPushButton::clicked, this, [this]() {
        signalingClient->sendPhoneCode();
    });
    connect(verifyCodeBtn, &QPushButton::clicked, this, [this, smsCodeEdit]() {
        const QString c = smsCodeEdit->text().trimmed();
        if (c.length() >= 4) signalingClient->verifyPhoneCode(c);
    });
    // smsToggleBtn: mevcut status'a göre toggle
    bool *smsEnabledCache = new bool(false);
    connect(signalingClient, &SignalingClient::securityStatus, &d,
        [smsEnabledCache](bool, bool smsEn, bool, bool, const QString&, const QString&, const QString&, bool) {
            *smsEnabledCache = smsEn;
        });
    connect(smsToggleBtn, &QPushButton::clicked, this, [this, smsEnabledCache]() {
        signalingClient->toggleSms2fa(!*smsEnabledCache);
    });
    connect(&d, &QDialog::finished, [smsEnabledCache]() { delete smsEnabledCache; });

    // Status isteyelim
    signalingClient->getSecurityStatus();
    d.exec();
    hideDialogBackdrop();
}

void MainWindow::openVoiceTileFullscreen(const QString &username, const QString &kind) {
    if (voiceFullscreenDlg) { voiceFullscreenDlg->raise(); voiceFullscreenDlg->activateWindow(); return; }
    voiceFullscreenDlg = new QDialog(this);
    voiceFullscreenDlg->setAttribute(Qt::WA_DeleteOnClose);
    voiceFullscreenDlg->setWindowTitle(QString("%1 · %2").arg(username,
        kind == "screen" ? QString::fromUtf8("Ekran") : QString::fromUtf8("Kamera")));
    voiceFullscreenDlg->setStyleSheet("background:#05070d;");
    voiceFullscreenDlg->resize(1280, 720);

    auto *vl = new QVBoxLayout(voiceFullscreenDlg);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Image (tam ekran arka plan)
    voiceFullscreenLabel = new QLabel();
    voiceFullscreenLabel->setAlignment(Qt::AlignCenter);
    voiceFullscreenLabel->setStyleSheet("background:#05070d; color:#5c6680;");
    voiceFullscreenLabel->setText(QString::fromUtf8("Sinyal bekleniyor..."));
    vl->addWidget(voiceFullscreenLabel, 1);

    // Overlay: başlık (sol-üst)
    auto *titleBadge = new QLabel(voiceFullscreenLabel);
    titleBadge->setText(QString::fromUtf8("%1  %2")
        .arg(kind == "screen" ? QString::fromUtf8("🖥") : QString::fromUtf8("📷"), username));
    titleBadge->setAttribute(Qt::WA_StyledBackground, true);
    titleBadge->setStyleSheet(
        "background:rgba(5,7,13,0.72); color:#fafafa;"
        " border:1px solid rgba(255,255,255,0.10);"
        " border-radius:12px; padding:8px 14px;"
        " font-weight:700; font-size:13px;");
    titleBadge->adjustSize();
    titleBadge->move(20, 20);
    titleBadge->raise();

    // Overlay: alt kontrol çubuğu
    auto *controls = new QWidget(voiceFullscreenLabel);
    controls->setAttribute(Qt::WA_StyledBackground, true);
    controls->setStyleSheet(
        "QWidget{background:rgba(10,13,20,0.82);"
        " border:1px solid rgba(255,255,255,0.10); border-radius:26px;}"
        "QPushButton{background:rgba(255,255,255,0.06); color:#fafafa;"
        " border:1px solid rgba(255,255,255,0.08); border-radius:20px;"
        " min-width:44px; min-height:44px; max-width:44px; max-height:44px;"
        " font-size:16px;}"
        "QPushButton:hover{background:rgba(120,150,210,0.22);"
        " border:1px solid rgba(120,150,210,0.40);}"
        "QPushButton#hang{background:#ef4444; border:1px solid rgba(255,255,255,0.10);}"
        "QPushButton#hang:hover{background:#f87171;}");
    auto *cl = new QHBoxLayout(controls);
    cl->setContentsMargins(14, 8, 14, 8);
    cl->setSpacing(8);

    auto *muteBtnFs = new QPushButton(QString::fromUtf8("🎤"));
    muteBtnFs->setCursor(Qt::PointingHandCursor);
    muteBtnFs->setToolTip(QString::fromUtf8("Mikrofon aç/kapat"));
    connect(muteBtnFs, &QPushButton::clicked, this, [this, muteBtnFs]() {
        toggleMute();
        muteBtnFs->setText(isMuted ? QString::fromUtf8("🔇") : QString::fromUtf8("🎤"));
    });
    cl->addWidget(muteBtnFs);

    auto *camBtnFs = new QPushButton(QString::fromUtf8("📷"));
    camBtnFs->setCursor(Qt::PointingHandCursor);
    camBtnFs->setToolTip(QString::fromUtf8("Kamerayı aç/kapat"));
    connect(camBtnFs, &QPushButton::clicked, this, [this, camBtnFs]() {
        toggleCamera();
        camBtnFs->setText(isCameraOn ? QString::fromUtf8("📷") : QString::fromUtf8("🚫"));
    });
    cl->addWidget(camBtnFs);

    auto *closeBtn = new QPushButton(QString::fromUtf8("✕"));
    closeBtn->setObjectName("hang");
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setToolTip(QString::fromUtf8("Tam ekranı kapat"));
    connect(closeBtn, &QPushButton::clicked, voiceFullscreenDlg, &QDialog::accept);
    cl->addWidget(closeBtn);

    controls->adjustSize();
    auto repositionControls = [voiceFullscreenLabel = voiceFullscreenLabel, controls]() {
        const int w = controls->sizeHint().width();
        const int h = controls->sizeHint().height();
        controls->resize(w, h);
        controls->move((voiceFullscreenLabel->width() - w) / 2,
                       voiceFullscreenLabel->height() - h - 20);
    };
    auto *repoTimer = new QTimer(voiceFullscreenLabel);
    repoTimer->setInterval(300);
    connect(repoTimer, &QTimer::timeout, voiceFullscreenLabel, repositionControls);
    repoTimer->start();
    repositionControls();

    voiceFullscreenUser = username;
    voiceFullscreenKind = kind;

    // Mevcut pixmap'i hemen kopyala
    const QString key = voiceTileKey(username, kind);
    if (QLabel *src = voiceMediaTileLabels.value(key, nullptr)) {
        const QPixmap pm = src->pixmap(Qt::ReturnByValue);
        if (!pm.isNull()) voiceFullscreenLabel->setPixmap(pm.scaled(voiceFullscreenLabel->size(),
            Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    connect(voiceFullscreenDlg, &QDialog::finished, this, [this]() {
        voiceFullscreenDlg = nullptr;
        voiceFullscreenLabel = nullptr;
        voiceFullscreenUser.clear();
        voiceFullscreenKind.clear();
    });
    voiceFullscreenDlg->show();
}

void MainWindow::refreshTypingLabel() {
    if (!typingLabel) return;
    // 4 saniyeden eski olanları sil
    const QDateTime now = QDateTime::currentDateTime();
    QMutableMapIterator<QString, QDateTime> it(activeTypers);
    while (it.hasNext()) {
        it.next();
        if (it.value().msecsTo(now) > 4000) it.remove();
    }
    if (activeTypers.isEmpty()) {
        typingLabel->setText("");
        return;
    }
    const QStringList names = activeTypers.keys();
    QString text;
    if (names.size() == 1) {
        text = QString::fromUtf8("%1 yazıyor...").arg(names.first());
    } else if (names.size() == 2) {
        text = QString::fromUtf8("%1 ve %2 yazıyor...").arg(names[0], names[1]);
    } else {
        text = QString::fromUtf8("Birkaç kişi yazıyor...");
    }
    typingLabel->setText(text);
}
