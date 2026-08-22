#include "NotificationBanner.h"
#include "../core/ThemeManager.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>

NotificationBanner::NotificationBanner(QWidget* parent)
    : QFrame(parent)
{
    setupUi();
    hide();
}

void NotificationBanner::setupUi()
{
    setFrameShape(QFrame::StyledPanel);
    setAutoFillBackground(true);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 6, 12, 6);
    layout->setSpacing(10);

    m_iconLabel = new QLabel(this);
    {
        QFont f = m_iconLabel->font();
        f.setBold(true);
        m_iconLabel->setFont(f);
    }
    layout->addWidget(m_iconLabel);

    m_textLabel = new QLabel(this);
    m_textLabel->setWordWrap(true);
    layout->addWidget(m_textLabel, 1);

    m_actionBtn = new QPushButton(this);
    m_actionBtn->setCursor(Qt::PointingHandCursor);
    m_actionBtn->hide();
    layout->addWidget(m_actionBtn);

    m_closeBtn = new QPushButton(QStringLiteral("✕"), this);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setFlat(true);
    m_closeBtn->setFixedSize(20, 20);
    layout->addWidget(m_closeBtn);

    m_autoHideTimer = new QTimer(this);
    m_autoHideTimer->setSingleShot(true);
    connect(m_autoHideTimer, &QTimer::timeout, this, &NotificationBanner::dismiss);

    connect(m_actionBtn, &QPushButton::clicked, this, [this]() {
        if (m_actionCallback) {
            m_actionCallback();
        }
        emit actionTriggered();
    });

    connect(m_closeBtn, &QPushButton::clicked, this, &NotificationBanner::dismiss);
}

void NotificationBanner::showNotification(Type type,
                                          const QString& message,
                                          const QString& actionText,
                                          std::function<void()> actionCallback,
                                          int autoHideMs)
{
    m_autoHideTimer->stop();
    m_actionCallback = actionCallback;

    m_textLabel->setText(message);

    if (!actionText.isEmpty() && actionCallback) {
        m_actionBtn->setText(actionText);
        m_actionBtn->show();
    } else {
        m_actionBtn->hide();
    }

    applyTheme(type);
    show();

    if (autoHideMs > 0) {
        m_autoHideTimer->start(autoHideMs);
    }
}

void NotificationBanner::dismiss()
{
    m_autoHideTimer->stop();
    hide();
    emit dismissed();
}

void NotificationBanner::applyTheme(Type type)
{
    bool isDark = ThemeManager::instance().isDark();

    QString bgColor;
    QString borderColor;
    QString textColor;
    QString icon;

    switch (type) {
    case Type::Info:
        icon = QStringLiteral("ℹ");
        bgColor = isDark ? QStringLiteral("#1a2b3c") : QStringLiteral("#e6f4ff");
        borderColor = isDark ? QStringLiteral("#1890ff") : QStringLiteral("#91d5ff");
        textColor = isDark ? QStringLiteral("#ffffff") : QStringLiteral("#002766");
        break;
    case Type::Warning:
        icon = QStringLiteral("⚠");
        bgColor = isDark ? QStringLiteral("#3a2a10") : QStringLiteral("#fffbe6");
        borderColor = isDark ? QStringLiteral("#faad14") : QStringLiteral("#ffe58f");
        textColor = isDark ? QStringLiteral("#ffffff") : QStringLiteral("#613400");
        break;
    case Type::Error:
        icon = QStringLiteral("✕");
        bgColor = isDark ? QStringLiteral("#3a1518") : QStringLiteral("#fff1f0");
        borderColor = isDark ? QStringLiteral("#ff4d4f") : QStringLiteral("#ffa39e");
        textColor = isDark ? QStringLiteral("#ffffff") : QStringLiteral("#5c0011");
        break;
    }

    m_iconLabel->setText(icon);
    setStyleSheet(QStringLiteral(
        "NotificationBanner {"
        "    background-color: %1;"
        "    border-bottom: 1px solid %2;"
        "    color: %3;"
        "}"
        "QLabel { color: %3; }"
        "QPushButton { color: %3; }"
    ).arg(bgColor, borderColor, textColor));
}

#include "moc_NotificationBanner.cpp"
