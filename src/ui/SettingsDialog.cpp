#include "SettingsDialog.h"
#include "../core/Settings.h"
#include "../core/ThemeManager.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QRadioButton>
#include <QTabWidget>
#include <QVBoxLayout>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Settings"));
    setModal(true);
    setMinimumSize(520, 400);
    setupUi();
}

SettingsDialog::~SettingsDialog() = default;

// ---------------------------------------------------------------------------
// UI setup
// ---------------------------------------------------------------------------

void SettingsDialog::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 12);

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(buildGeneralTab(),  tr("General"));
    m_tabs->addTab(buildAccountsTab(), tr("Accounts"));
    m_tabs->addTab(buildAdvancedTab(), tr("Advanced"));
    layout->addWidget(m_tabs);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    layout->addWidget(buttons);
    layout->setContentsMargins(12, 8, 12, 12);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
}

// ---------------------------------------------------------------------------
// General tab
// ---------------------------------------------------------------------------

QWidget* SettingsDialog::buildGeneralTab()
{
    auto* page = new QWidget;
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(16, 16, 16, 16);
    outer->setSpacing(16);

    // -- Appearance group --------------------------------------------------
    auto* appearanceGroup = new QGroupBox(tr("Appearance"), page);
    auto* appearanceForm  = new QFormLayout(appearanceGroup);
    appearanceForm->setContentsMargins(12, 12, 12, 12);
    appearanceForm->setRowWrapPolicy(QFormLayout::WrapLongRows);

    // Theme
    auto* themeWidget = new QWidget(appearanceGroup);
    auto* themeLayout = new QVBoxLayout(themeWidget);
    themeLayout->setContentsMargins(0, 0, 0, 0);
    themeLayout->setSpacing(4);

    auto* themeGroup  = new QButtonGroup(themeWidget);
    auto* rbSystem    = new QRadioButton(tr("Follow system"), themeWidget);
    auto* rbLight     = new QRadioButton(tr("Light"),         themeWidget);
    auto* rbDark      = new QRadioButton(tr("Dark"),          themeWidget);
    themeGroup->addButton(rbSystem, 0);
    themeGroup->addButton(rbLight,  1);
    themeGroup->addButton(rbDark,   2);
    themeLayout->addWidget(rbSystem);
    themeLayout->addWidget(rbLight);
    themeLayout->addWidget(rbDark);

    // Reflect current theme
    switch (ThemeManager::instance().currentTheme()) {
    case ThemeManager::Theme::Light:  rbLight->setChecked(true);  break;
    case ThemeManager::Theme::Dark:   rbDark->setChecked(true);   break;
    default:                          rbSystem->setChecked(true); break;
    }

    connect(themeGroup, &QButtonGroup::idClicked, this, [](int id) {
        const ThemeManager::Theme t = (id == 1) ? ThemeManager::Theme::Light
                                    : (id == 2) ? ThemeManager::Theme::Dark
                                                : ThemeManager::Theme::System;
        ThemeManager::instance().apply(t);
    });

    appearanceForm->addRow(tr("Theme:"), themeWidget);

    // Message density
    auto* densityCombo = new QComboBox(appearanceGroup);
    densityCombo->addItem(tr("Compact"),     QStringLiteral("compact"));
    densityCombo->addItem(tr("Normal"),      QStringLiteral("normal"));
    densityCombo->addItem(tr("Comfortable"), QStringLiteral("comfortable"));
    {
        const QString saved = Settings::instance().stringValue(Settings::Key::UiMessageDensity);
        const int idx = densityCombo->findData(saved);
        densityCombo->setCurrentIndex(idx >= 0 ? idx : 1);
    }
    connect(densityCombo, &QComboBox::currentIndexChanged, this, [densityCombo](int idx) {
        Settings::instance().setValue(Settings::Key::UiMessageDensity,
                                      densityCombo->itemData(idx));
    });
    appearanceForm->addRow(tr("Message density:"), densityCombo);

    outer->addWidget(appearanceGroup);

    // -- Privacy group -------------------------------------------------------
    auto* privacyGroup = new QGroupBox(tr("Privacy"), page);
    auto* privacyForm  = new QFormLayout(privacyGroup);
    privacyForm->setContentsMargins(12, 12, 12, 12);

    auto* remoteImagesWidget = new QWidget(privacyGroup);
    auto* remoteLayout       = new QVBoxLayout(remoteImagesWidget);
    remoteLayout->setContentsMargins(0, 0, 0, 0);
    auto* remoteGroup  = new QButtonGroup(remoteImagesWidget);
    auto* rbBlock      = new QRadioButton(tr("Block (recommended)"), remoteImagesWidget);
    auto* rbAllow      = new QRadioButton(tr("Allow"),               remoteImagesWidget);
    remoteGroup->addButton(rbBlock, 0);
    remoteGroup->addButton(rbAllow, 1);
    remoteLayout->addWidget(rbBlock);
    remoteLayout->addWidget(rbAllow);

    const bool loadRemote = Settings::instance().boolValue(Settings::Key::PrivacyLoadRemoteImages);
    (loadRemote ? rbAllow : rbBlock)->setChecked(true);

    connect(remoteGroup, &QButtonGroup::idClicked, this, [](int id) {
        Settings::instance().setValue(Settings::Key::PrivacyLoadRemoteImages, id == 1);
    });

    privacyForm->addRow(tr("Remote images:"), remoteImagesWidget);

    // Placeholder note for features coming later
    auto* privacyNote = new QLabel(
        tr("Additional privacy settings will be added as features are implemented."),
        privacyGroup);
    privacyNote->setWordWrap(true);
    {
        QPalette p = privacyNote->palette();
        p.setColor(QPalette::WindowText,
                   qApp->palette().color(QPalette::Disabled, QPalette::WindowText));
        privacyNote->setPalette(p);
        QFont f = privacyNote->font();
        f.setPointSizeF(f.pointSizeF() * 0.88);
        privacyNote->setFont(f);
    }
    privacyForm->addRow(privacyNote);

    outer->addWidget(privacyGroup);
    outer->addStretch();

    return page;
}

// ---------------------------------------------------------------------------
// Accounts tab
// ---------------------------------------------------------------------------

QWidget* SettingsDialog::buildAccountsTab()
{
    auto* page  = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 16, 16, 16);

    auto* note = new QLabel(
        tr("Account management will be available in the next phase.\n\n"
           "To add an account, use File \u2192 Add Account."),
        page);
    note->setAlignment(Qt::AlignCenter);
    note->setWordWrap(true);
    {
        QPalette p = note->palette();
        p.setColor(QPalette::WindowText,
                   qApp->palette().color(QPalette::Disabled, QPalette::WindowText));
        note->setPalette(p);
    }
    layout->addWidget(note, 1, Qt::AlignCenter);

    return page;
}

// ---------------------------------------------------------------------------
// Advanced tab
// ---------------------------------------------------------------------------

QWidget* SettingsDialog::buildAdvancedTab()
{
    auto* page = new QWidget;
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(16, 16, 16, 16);
    outer->setSpacing(16);

    auto* loggingGroup = new QGroupBox(tr("Logging"), page);
    auto* loggingForm  = new QFormLayout(loggingGroup);
    loggingForm->setContentsMargins(12, 12, 12, 12);

    auto* logLevelCombo = new QComboBox(loggingGroup);
    logLevelCombo->addItem(tr("Info (default)"), QStringLiteral("info"));
    logLevelCombo->addItem(tr("Debug (verbose)"), QStringLiteral("debug"));
    logLevelCombo->addItem(tr("Warning"),          QStringLiteral("warning"));
    logLevelCombo->addItem(tr("Error only"),       QStringLiteral("error"));
    {
        const QString saved = Settings::instance().stringValue(Settings::Key::AdvancedLogLevel);
        const int idx = logLevelCombo->findData(saved);
        logLevelCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    connect(logLevelCombo, &QComboBox::currentIndexChanged, this, [logLevelCombo](int idx) {
        Settings::instance().setValue(Settings::Key::AdvancedLogLevel,
                                      logLevelCombo->itemData(idx));
    });
    loggingForm->addRow(tr("Log level:"), logLevelCombo);

    auto* logNote = new QLabel(
        tr("Log level changes take effect after restarting Mailinium."),
        loggingGroup);
    logNote->setWordWrap(true);
    {
        QPalette p = logNote->palette();
        p.setColor(QPalette::WindowText,
                   qApp->palette().color(QPalette::Disabled, QPalette::WindowText));
        logNote->setPalette(p);
        QFont f = logNote->font();
        f.setPointSizeF(f.pointSizeF() * 0.88);
        logNote->setFont(f);
    }
    loggingForm->addRow(logNote);

    outer->addWidget(loggingGroup);
    outer->addStretch();

    return page;
}
