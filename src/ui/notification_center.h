#pragma once

// =====================================================================
//  NotificationCenter — top bar bell icon + dropdown panel
// ---------------------------------------------------------------------
//  • Top bar'a yerleştirilen QToolButton bell + kırmızı badge
//  • Tıklanınca dropdown panel açılır (frameless popup)
//  • Bildirim ekleme: addNotification(emoji, title, body, [actionLabel, callback])
//  • "Tümünü temizle" butonu
//  • Okunmamış sayacı bell üzerinde
// =====================================================================

#include <QWidget>
#include <QString>
#include <QList>
#include <functional>

class QToolButton;
class QLabel;
class QFrame;
class QVBoxLayout;
class QScrollArea;

class NotificationCenter : public QWidget {
    Q_OBJECT
public:
    explicit NotificationCenter(QWidget *anchorParent, QWidget *toplevel = nullptr);

    // Top bar'a yerleştirmek için widget döndürür (bell + badge container)
    QToolButton *bellWidget() const { return bell_; }

    // Bildirim ekle. actionLabel boş değilse alt köşede primary buton görünür.
    void addNotification(const QString &emoji,
                         const QString &title,
                         const QString &body,
                         const QString &actionLabel = QString(),
                         std::function<void()> onAction = nullptr,
                         const QString &id = QString());  // id verilirse aynı id'li mevcutsa replace

    void clearAll();
    int  unreadCount() const { return unread_; }

public slots:
    void togglePanel();
    void hidePanel();

private:
    void rebuildList();
    void updateBadge();
    void positionPanel();

    struct Item {
        QString id;
        QString emoji;
        QString title;
        QString body;
        QString actionLabel;
        std::function<void()> onAction;
        qint64  ts;
    };

    QWidget       *toplevel_ = nullptr;
    QToolButton   *bell_ = nullptr;
    QLabel        *badge_ = nullptr;
    QFrame        *panel_ = nullptr;
    QVBoxLayout   *itemsLay_ = nullptr;
    QScrollArea   *scroll_ = nullptr;
    QLabel        *emptyLabel_ = nullptr;
    QList<Item>    items_;
    int            unread_ = 0;
};
