// =====================================================================
//  NotificationCenter implementation
// =====================================================================

#include "notification_center.h"

#include <QToolButton>
#include <QLabel>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QPushButton>
#include <QGraphicsDropShadowEffect>
#include <QPainter>
#include <QPainterPath>
#include <QIcon>
#include <QDateTime>
#include <QApplication>
#include <QScreen>
#include <QEvent>
#include <QMouseEvent>

namespace {
constexpr int kPanelWidth   = 380;
constexpr int kPanelMaxHigh = 480;

QIcon makeBellIcon(const QColor &color, int size) {
    QPixmap pm(size * 2, size * 2);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(2, 2);
    QPen pen(color, 1.6);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    const qreal s = size;
    const qreal cx = s / 2.0, cy = s / 2.0;
    QPainterPath bell;
    bell.moveTo(cx - s * 0.30, cy + s * 0.18);
    bell.lineTo(cx + s * 0.30, cy + s * 0.18);
    bell.quadTo(cx + s * 0.30, cy - s * 0.18, cx, cy - s * 0.32);
    bell.quadTo(cx - s * 0.30, cy - s * 0.18, cx - s * 0.30, cy + s * 0.18);
    p.drawPath(bell);
    // Çıngırak (alt küçük yarım daire)
    QRectF tongue(cx - s * 0.08, cy + s * 0.18, s * 0.16, s * 0.10);
    p.drawArc(tongue, 0, -180 * 16);
    p.end();
    return QIcon(pm);
}
} // namespace

NotificationCenter::NotificationCenter(QWidget *anchorParent, QWidget *toplevel)
    : QWidget(anchorParent), toplevel_(toplevel ? toplevel : anchorParent->window()) {
    // Bell button
    bell_ = new QToolButton(anchorParent);
    bell_->setCursor(Qt::PointingHandCursor);
    bell_->setFixedSize(38, 38);
    bell_->setIcon(makeBellIcon(QColor("#cbd5e1"), 20));
    bell_->setIconSize(QSize(20, 20));
    bell_->setToolTip(QStringLiteral("Bildirimler"));
    bell_->setStyleSheet(
        "QToolButton{background:rgba(255,255,255,0.04); border:1px solid rgba(120,150,210,0.18);"
        " border-radius:19px;}"
        "QToolButton:hover{background:rgba(167,139,250,0.18); border:1px solid rgba(167,139,250,0.45);}");
    QObject::connect(bell_, &QToolButton::clicked, this, &NotificationCenter::togglePanel);

    // Badge (sağ üst kırmızı sayaç)
    badge_ = new QLabel(bell_);
    badge_->setAttribute(Qt::WA_TransparentForMouseEvents);
    badge_->setAlignment(Qt::AlignCenter);
    badge_->setStyleSheet(
        "background:#ef4444; color:white; font-size:10px; font-weight:800;"
        " border-radius:8px; padding:0 4px;");
    badge_->hide();
    badge_->setMinimumSize(16, 16);
    badge_->move(bell_->width() - 18, 2);

    // Dropdown panel — frameless popup
    panel_ = new QFrame(toplevel_, Qt::Popup | Qt::FramelessWindowHint);
    panel_->setAttribute(Qt::WA_TranslucentBackground);
    panel_->setFixedWidth(kPanelWidth);
    panel_->setObjectName("notifPanel");
    panel_->setStyleSheet(
        "QFrame#notifPanel{background:#0f1422; border:1px solid rgba(120,150,210,0.22);"
        " border-radius:14px;}"
        "QLabel{background:transparent; color:#e4e7ee;}"
        "QPushButton{background:transparent; color:#a78bfa; border:none; padding:6px 10px;}"
        "QPushButton:hover{color:#c4b5fd;}");
    auto *shadow = new QGraphicsDropShadowEffect(panel_);
    shadow->setBlurRadius(40);
    shadow->setOffset(0, 12);
    shadow->setColor(QColor(0, 0, 0, 200));
    panel_->setGraphicsEffect(shadow);

    auto *outer = new QVBoxLayout(panel_);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // Header
    auto *header = new QWidget();
    header->setStyleSheet("background:transparent;");
    auto *hl = new QHBoxLayout(header);
    hl->setContentsMargins(16, 12, 10, 8);
    auto *t = new QLabel(QStringLiteral("Bildirimler"));
    t->setStyleSheet("color:#fafafa; font-size:14px; font-weight:700;");
    hl->addWidget(t);
    hl->addStretch();
    auto *clearBtn = new QPushButton(QStringLiteral("Tümünü temizle"));
    clearBtn->setCursor(Qt::PointingHandCursor);
    clearBtn->setStyleSheet("color:#94a3b8; font-size:11px;");
    QObject::connect(clearBtn, &QPushButton::clicked, this, &NotificationCenter::clearAll);
    hl->addWidget(clearBtn);
    outer->addWidget(header);

    // Separator
    auto *sep = new QFrame();
    sep->setFixedHeight(1);
    sep->setStyleSheet("background:rgba(120,150,210,0.14);");
    outer->addWidget(sep);

    // Scroll area for items
    scroll_ = new QScrollArea();
    scroll_->setWidgetResizable(true);
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setStyleSheet(
        "QScrollArea{background:transparent;}"
        "QScrollBar:vertical{background:transparent; width:6px; margin:4px;}"
        "QScrollBar::handle:vertical{background:#2c3144; border-radius:3px; min-height:24px;}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical{height:0;}");
    scroll_->setMaximumHeight(kPanelMaxHigh - 90);
    auto *wrap = new QWidget();
    wrap->setStyleSheet("background:transparent;");
    itemsLay_ = new QVBoxLayout(wrap);
    itemsLay_->setContentsMargins(8, 8, 8, 12);
    itemsLay_->setSpacing(8);
    itemsLay_->addStretch();
    scroll_->setWidget(wrap);
    outer->addWidget(scroll_);

    emptyLabel_ = new QLabel(QStringLiteral("🔔  Henüz bildirimin yok"));
    emptyLabel_->setAlignment(Qt::AlignCenter);
    emptyLabel_->setStyleSheet("color:#6b7280; font-size:12px; padding:36px 0;");
    outer->addWidget(emptyLabel_);

    rebuildList();
}

void NotificationCenter::addNotification(const QString &emoji,
                                          const QString &title,
                                          const QString &body,
                                          const QString &actionLabel,
                                          std::function<void()> onAction,
                                          const QString &id) {
    Item it;
    it.emoji = emoji;
    it.title = title;
    it.body  = body;
    it.actionLabel = actionLabel;
    it.onAction = std::move(onAction);
    it.ts = QDateTime::currentMSecsSinceEpoch();
    it.id = id;

    bool replaced = false;
    if (!id.isEmpty()) {
        // Replace if same id exists — bu durumda yeni okunmamış sayma
        for (int i = 0; i < items_.size(); ++i) {
            if (items_[i].id == id) {
                items_.removeAt(i);
                replaced = true;
                break;
            }
        }
    }
    items_.prepend(it);
    if (items_.size() > 50) items_ = items_.mid(0, 50);
    if (!replaced) ++unread_;
    rebuildList();
    updateBadge();
}

void NotificationCenter::clearAll() {
    items_.clear();
    unread_ = 0;
    rebuildList();
    updateBadge();
}

void NotificationCenter::rebuildList() {
    // Remove old item widgets (but keep stretch at end)
    while (itemsLay_->count() > 1) {
        QLayoutItem *li = itemsLay_->takeAt(0);
        if (li->widget()) li->widget()->deleteLater();
        delete li;
    }
    if (items_.isEmpty()) {
        emptyLabel_->show();
        scroll_->hide();
        if (panel_) panel_->setMaximumHeight(180);
        return;
    }
    emptyLabel_->hide();
    scroll_->show();
    if (panel_) panel_->setMaximumHeight(kPanelMaxHigh);

    for (int i = 0; i < items_.size(); ++i) {
        const Item &it = items_[i];
        auto *card = new QFrame();
        card->setStyleSheet(
            "QFrame{background:rgba(167,139,250,0.06); border:1px solid rgba(120,150,210,0.14);"
            " border-radius:10px;}");
        auto *cl = new QVBoxLayout(card);
        cl->setContentsMargins(12, 10, 12, 10);
        cl->setSpacing(4);

        auto *titleRow = new QHBoxLayout();
        titleRow->setSpacing(8);
        auto *em = new QLabel(it.emoji);
        em->setStyleSheet("font-size:16px;");
        titleRow->addWidget(em);
        auto *tt = new QLabel(it.title);
        tt->setStyleSheet("color:#fafafa; font-weight:700; font-size:13px;");
        tt->setWordWrap(true);
        titleRow->addWidget(tt, 1);
        cl->addLayout(titleRow);

        if (!it.body.isEmpty()) {
            auto *bb = new QLabel(it.body);
            bb->setWordWrap(true);
            bb->setTextFormat(Qt::RichText);
            bb->setStyleSheet("color:#cbd5e1; font-size:12px; line-height:1.4;");
            cl->addWidget(bb);
        }

        if (!it.actionLabel.isEmpty() && it.onAction) {
            auto *btn = new QPushButton(it.actionLabel);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setStyleSheet(
                "QPushButton{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                " stop:0 #6366f1, stop:1 #a855f7); color:white;"
                " padding:7px 14px; border-radius:8px; font-weight:700; font-size:12px; border:none;}"
                "QPushButton:hover{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                " stop:0 #4f46e5, stop:1 #9333ea);}");
            const auto cb = it.onAction;
            QObject::connect(btn, &QPushButton::clicked, this, [this, cb]() {
                if (cb) cb();
                hidePanel();
            });
            auto *btnRow = new QHBoxLayout();
            btnRow->addStretch();
            btnRow->addWidget(btn);
            cl->addLayout(btnRow);
        }
        itemsLay_->insertWidget(itemsLay_->count() - 1, card);
    }
}

void NotificationCenter::updateBadge() {
    if (unread_ <= 0) {
        badge_->hide();
        return;
    }
    badge_->setText(unread_ > 99 ? QStringLiteral("99+") : QString::number(unread_));
    badge_->adjustSize();
    int w = qMax(16, badge_->sizeHint().width() + 6);
    badge_->setFixedSize(w, 16);
    badge_->move(bell_->width() - w + 2, 2);
    badge_->show();
    badge_->raise();
}

void NotificationCenter::positionPanel() {
    if (!bell_ || !panel_) return;
    const QPoint global = bell_->mapToGlobal(QPoint(bell_->width(), bell_->height() + 6));
    int x = global.x() - panel_->width();
    int y = global.y();
    // Ekran içinde tut
    if (auto *scr = QApplication::screenAt(global)) {
        const QRect g = scr->availableGeometry();
        x = qBound(g.left() + 8, x, g.right() - panel_->width() - 8);
        y = qBound(g.top()  + 8, y, g.bottom() - 220);
    }
    panel_->move(x, y);
}

void NotificationCenter::togglePanel() {
    if (panel_->isVisible()) {
        hidePanel();
        return;
    }
    positionPanel();
    panel_->show();
    panel_->raise();
    panel_->activateWindow();
    // Açılınca okunmamış sayacı sıfırla
    unread_ = 0;
    updateBadge();
}

void NotificationCenter::hidePanel() {
    if (panel_->isVisible()) panel_->hide();
}
