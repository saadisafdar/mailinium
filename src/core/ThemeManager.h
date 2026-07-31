#pragma once

#include <QObject>

//
// ThemeManager — manages the application visual theme (light / dark / system).
//
// Responsibilities:
//   - Set QApplication style to Fusion on all platforms for consistent, controllable rendering
//   - Apply a tuned QPalette for light and dark modes
//   - Detect the OS color scheme via QStyleHints (Qt 6.5+)
//   - Follow live OS theme changes when mode is System
//   - Persist the user's choice via Settings::Key::GeneralTheme
//
// Usage:
//   // In Application::initializeCore():
//   ThemeManager::instance().initialize();
//
//   // To switch theme programmatically (e.g. from the View menu):
//   ThemeManager::instance().apply(ThemeManager::Theme::Dark);
//
//   // To check current effective mode:
//   bool dark = ThemeManager::instance().isDark();
//
class ThemeManager : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ThemeManager)

public:
    enum class Theme {
        System, // Follow the OS color scheme
        Light,
        Dark,
    };

    static ThemeManager& instance();

    // Call once during Application::initializeCore(), after Settings is ready.
    void initialize();

    // Apply and persist a theme choice. Emits themeChanged().
    void apply(Theme theme);

    Theme currentTheme() const { return m_theme; }

    // True if the effective (resolved) mode is dark.
    bool isDark() const;

signals:
    // Emitted whenever the effective dark/light mode changes — either because
    // the user switched the theme or because the OS theme changed while
    // mode == System.
    void themeChanged(bool dark);

private:
    explicit ThemeManager(QObject* parent = nullptr);

    void applyEffectiveTheme();
    void applyLightPalette();
    void applyDarkPalette();
    bool systemIsDark() const;

    Theme m_theme = Theme::System;
};
