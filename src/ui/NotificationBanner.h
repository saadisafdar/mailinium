#pragma once

#include <QFrame>
#include <functional>

class QLabel;
class QPushButton;
class QTimer;

//
// NotificationBanner — A sleek in-window banner for non-intrusive error, warning, and info notifications.
//
class NotificationBanner : public QFrame
{
    Q_OBJECT

public:
    enum class Type {
        Info,
        Warning,
        Error
    };

    explicit NotificationBanner(QWidget* parent = nullptr);
    ~NotificationBanner() override = default;

    // Shows a notification message with optional action callback and auto-hide timer.
    void showNotification(Type type,
                          const QString& message,
                          const QString& actionText = QString(),
                          std::function<void()> actionCallback = nullptr,
                          int autoHideMs = 0);

    void dismiss();

signals:
    void dismissed();
    void actionTriggered();

private:
    void setupUi();
    void applyTheme(Type type);

    QLabel*      m_iconLabel   = nullptr;
    QLabel*      m_textLabel   = nullptr;
    QPushButton* m_actionBtn   = nullptr;
    QPushButton* m_closeBtn    = nullptr;
    QTimer*      m_autoHideTimer = nullptr;

    std::function<void()> m_actionCallback;
};
