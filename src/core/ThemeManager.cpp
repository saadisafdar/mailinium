#include "ThemeManager.h"
#include "Logger.h"
#include "Settings.h"

#include <QApplication>
#include <QGuiApplication>
#include <QPalette>
#include <QStyleFactory>
#include <QStyleHints>

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------

ThemeManager& ThemeManager::instance()
{
    static ThemeManager s_instance;
    return s_instance;
}

ThemeManager::ThemeManager(QObject* parent)
    : QObject(parent)
{
}

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

void ThemeManager::initialize()
{
    // Apply Fusion style globally. This gives us full palette control on all
    // platforms and a consistent, modern flat appearance.
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    // React to OS-level theme changes (e.g. user switches Windows dark mode
    // while the app is running). Qt 6.5+ emits colorSchemeChanged for this.
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
            this, [this](Qt::ColorScheme /*scheme*/) {
                if (m_theme == Theme::System) {
                    Logger::instance().debug(
                        QStringLiteral("theme"),
                        QStringLiteral("OS color scheme changed — re-applying system theme"));
                    applyEffectiveTheme();
                }
            });

    // Load and apply the saved preference
    const QString saved = Settings::instance().stringValue(Settings::Key::GeneralTheme);

    Theme theme = Theme::System;
    if (saved == QLatin1String("light")) {
        theme = Theme::Light;
    } else if (saved == QLatin1String("dark")) {
        theme = Theme::Dark;
    }

    apply(theme);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void ThemeManager::apply(Theme theme)
{
    m_theme = theme;

    // Persist the choice
    const QString themeStr = (theme == Theme::Light)  ? QStringLiteral("light")
                           : (theme == Theme::Dark)   ? QStringLiteral("dark")
                                                      : QStringLiteral("system");
    Settings::instance().setValue(Settings::Key::GeneralTheme, themeStr);

    Logger::instance().info(
        QStringLiteral("theme"),
        QStringLiteral("Applying theme: ") + themeStr);

    applyEffectiveTheme();
}

bool ThemeManager::isDark() const
{
    if (m_theme == Theme::Dark)  return true;
    if (m_theme == Theme::Light) return false;
    return systemIsDark();
}

// ---------------------------------------------------------------------------
// Internal
// ---------------------------------------------------------------------------

void ThemeManager::applyEffectiveTheme()
{
    const bool dark = isDark();
    dark ? applyDarkPalette() : applyLightPalette();
    emit themeChanged(dark);
}

bool ThemeManager::systemIsDark() const
{
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
}

// ---------------------------------------------------------------------------
// Light palette
// Clean, neutral. #0078D4 (Windows 11 accent blue) for highlight.
// ---------------------------------------------------------------------------

void ThemeManager::applyLightPalette()
{
    QPalette p;

    const QColor windowBg     (245, 245, 245);
    const QColor baseBg       (255, 255, 255);
    const QColor altBase      (238, 238, 238);
    const QColor text         ( 15,  15,  15);
    const QColor textDisabled (160, 160, 160);
    const QColor buttonBg     (225, 225, 225);
    const QColor highlight    (  0, 120, 212); // #0078D4
    const QColor border       (200, 200, 200);

    p.setColor(QPalette::Window,          windowBg);
    p.setColor(QPalette::WindowText,      text);
    p.setColor(QPalette::Base,            baseBg);
    p.setColor(QPalette::AlternateBase,   altBase);
    p.setColor(QPalette::Text,            text);
    p.setColor(QPalette::Button,          buttonBg);
    p.setColor(QPalette::ButtonText,      text);
    p.setColor(QPalette::Highlight,       highlight);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Link,            highlight);
    p.setColor(QPalette::LinkVisited,     QColor(100, 65, 164));
    p.setColor(QPalette::ToolTipBase,     QColor(255, 255, 220));
    p.setColor(QPalette::ToolTipText,     text);
    p.setColor(QPalette::Mid,             border);
    p.setColor(QPalette::Midlight,        QColor(235, 235, 235));
    p.setColor(QPalette::Dark,            QColor(180, 180, 180));
    p.setColor(QPalette::Shadow,          QColor(120, 120, 120));
    p.setColor(QPalette::BrightText,      Qt::red);

    p.setColor(QPalette::Disabled, QPalette::WindowText,  textDisabled);
    p.setColor(QPalette::Disabled, QPalette::Text,        textDisabled);
    p.setColor(QPalette::Disabled, QPalette::ButtonText,  textDisabled);
    p.setColor(QPalette::Disabled, QPalette::Highlight,   QColor(160, 160, 160));
    p.setColor(QPalette::Disabled, QPalette::HighlightedText, Qt::white);

    QApplication::setPalette(p);
}

// ---------------------------------------------------------------------------
// Dark palette
// VS Code-inspired neutral dark. Not pure black — avoids eye strain.
// ---------------------------------------------------------------------------

void ThemeManager::applyDarkPalette()
{
    QPalette p;

    const QColor windowBg     ( 30,  30,  30); // #1E1E1E
    const QColor baseBg       ( 37,  37,  38); // #252526
    const QColor altBase      ( 45,  45,  48); // #2D2D30
    const QColor text         (212, 212, 212); // #D4D4D4
    const QColor textDisabled ( 96,  96,  96);
    const QColor buttonBg     ( 60,  60,  60); // #3C3C3C
    const QColor highlight    (  0, 120, 212); // #0078D4 — same accent for consistency
    const QColor border       ( 68,  68,  68);

    p.setColor(QPalette::Window,          windowBg);
    p.setColor(QPalette::WindowText,      text);
    p.setColor(QPalette::Base,            baseBg);
    p.setColor(QPalette::AlternateBase,   altBase);
    p.setColor(QPalette::Text,            text);
    p.setColor(QPalette::Button,          buttonBg);
    p.setColor(QPalette::ButtonText,      text);
    p.setColor(QPalette::Highlight,       highlight);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Link,            QColor( 78, 201, 176)); // teal — readable on dark
    p.setColor(QPalette::LinkVisited,     QColor(197, 134, 192));
    p.setColor(QPalette::ToolTipBase,     QColor( 50,  50,  50));
    p.setColor(QPalette::ToolTipText,     text);
    p.setColor(QPalette::Mid,             border);
    p.setColor(QPalette::Midlight,        QColor( 50,  50,  50));
    p.setColor(QPalette::Dark,            QColor( 20,  20,  20));
    p.setColor(QPalette::Shadow,          QColor(  0,   0,   0));
    p.setColor(QPalette::BrightText,      QColor(244, 135, 113));

    p.setColor(QPalette::Disabled, QPalette::WindowText,  textDisabled);
    p.setColor(QPalette::Disabled, QPalette::Text,        textDisabled);
    p.setColor(QPalette::Disabled, QPalette::ButtonText,  textDisabled);
    p.setColor(QPalette::Disabled, QPalette::Highlight,   QColor( 60,  60,  60));
    p.setColor(QPalette::Disabled, QPalette::HighlightedText, textDisabled);

    QApplication::setPalette(p);
}
