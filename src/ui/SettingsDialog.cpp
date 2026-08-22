#include "SettingsDialog.h"
#include "AccountSetupWizard.h"
#include "../core/Settings.h"
#include "../core/ThemeManager.h"
#include "../core/SyncManager.h"
#include "../storage/AccountStore.h"
#include "../auth/GoogleOAuth.h"

#include <QApplication>
#include <QButtonGroup>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
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
    m_tabs->addTab(buildSyncTab(),     tr("Sync"));
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
                   QApplication::palette().color(QPalette::Disabled, QPalette::WindowText));
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
    layout->setSpacing(12);

    auto* listWidget = new QListWidget(page);
    layout->addWidget(listWidget, 1);

    auto reloadAccounts = [listWidget]() {
        listWidget->clear();
        AccountStore store;
        const auto accounts = store.allAccounts();
        for (const Account& acc : accounts) {
            QString typeStr = (acc.authType == AuthType::OAuth2) ? QStringLiteral("OAuth 2.0") : QStringLiteral("Password");
            QString label = QStringLiteral("%1 (%2) — %3").arg(acc.email, acc.displayName.isEmpty() ? tr("No Name") : acc.displayName, typeStr);
            auto* item = new QListWidgetItem(label, listWidget);
            item->setData(Qt::UserRole, acc.id);
        }
    };

    reloadAccounts();

    auto* btnLayout = new QHBoxLayout;
    auto* btnAdd = new QPushButton(tr("Add Account..."), page);
    auto* btnReauth = new QPushButton(tr("Re-authenticate..."), page);
    auto* btnRemove = new QPushButton(tr("Remove"), page);

    btnLayout->addWidget(btnAdd);
    btnLayout->addWidget(btnReauth);
    btnLayout->addWidget(btnRemove);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    connect(btnAdd, &QPushButton::clicked, this, [this, reloadAccounts]() {
        AccountSetupWizard wizard(this);
        if (wizard.exec() == QDialog::Accepted) {
            qint64 newId = wizard.createdAccountId();
            if (newId > 0) {
                AccountStore store;
                auto opt = store.accountById(newId);
                if (opt.has_value()) {
                    SyncManager::instance().addAccount(*opt);
                }
            }
            reloadAccounts();
        }
    });

    connect(btnReauth, &QPushButton::clicked, this, [this, listWidget, reloadAccounts]() {
        auto items = listWidget->selectedItems();
        if (items.isEmpty()) return;

        qint64 accountId = items.first()->data(Qt::UserRole).toLongLong();
        AccountStore store;
        auto opt = store.accountById(accountId);
        if (!opt.has_value()) return;

        Account acc = *opt;
        if (acc.authType == AuthType::OAuth2) {
            auto* oauth = new GoogleOAuth(this);
            connect(oauth, &GoogleOAuth::authSuccessful, this, [this, oauth, accountId, reloadAccounts](const QString& email, const QString& accessToken, const QString& refreshToken, int expiresInSecs) {
                oauth->deleteLater();
                AccountStore s;
                s.updateOAuthTokens(accountId, accessToken, refreshToken, QDateTime::currentDateTimeUtc().addSecs(expiresInSecs));
                SyncManager::instance().reauthenticateAccount(accountId);
                reloadAccounts();
                QMessageBox::information(this, tr("Authentication Succeeded"), tr("Successfully re-authenticated %1.").arg(email));
            });
            connect(oauth, &GoogleOAuth::authFailed, this, [this, oauth](const QString& errorMsg, bool /*isPermanent*/) {
                oauth->deleteLater();
                QMessageBox::warning(this, tr("Authentication Failed"), errorMsg);
            });
            oauth->startReauthFlow(acc.email);
        } else {
            bool ok = false;
            QString newPass = QInputDialog::getText(this, tr("Re-authenticate Account"),
                tr("Enter password for %1:").arg(acc.email), QLineEdit::Password, QString(), &ok);
            if (ok && !newPass.isEmpty()) {
                AccountStore s;
                s.updatePassword(accountId, newPass);
                SyncManager::instance().reauthenticateAccount(accountId);
                reloadAccounts();
            }
        }
    });

    connect(btnRemove, &QPushButton::clicked, this, [this, listWidget, reloadAccounts]() {
        auto items = listWidget->selectedItems();
        if (items.isEmpty()) return;

        qint64 accountId = items.first()->data(Qt::UserRole).toLongLong();
        if (QMessageBox::question(this, tr("Remove Account"), tr("Are you sure you want to remove this account? This will delete all downloaded messages.")) == QMessageBox::Yes) {
            SyncManager::instance().removeAccount(accountId);
            reloadAccounts();
        }
    });

    return page;
}

// ---------------------------------------------------------------------------
// Sync tab
// ---------------------------------------------------------------------------

QWidget* SettingsDialog::buildSyncTab()
{
    auto* page = new QWidget;
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(16, 16, 16, 16);
    outer->setSpacing(16);

    // -- Schedule group ----------------------------------------------------
    auto* scheduleGroup = new QGroupBox(tr("Synchronization Schedule"), page);
    auto* scheduleForm  = new QFormLayout(scheduleGroup);
    scheduleForm->setContentsMargins(12, 12, 12, 12);
    scheduleForm->setRowWrapPolicy(QFormLayout::WrapLongRows);

    // Interval spinbox (minutes)
    auto* intervalSpin = new QSpinBox(scheduleGroup);
    intervalSpin->setRange(1, 1440); // 1 minute to 24 hours
    intervalSpin->setSuffix(tr(" minutes"));
    int currentSecs = Settings::instance().intValue(Settings::Key::SyncIntervalSeconds);
    intervalSpin->setValue(qMax(1, currentSecs / 60));

    connect(intervalSpin, &QSpinBox::valueChanged, this, [](int mins) {
        Settings::instance().setValue(Settings::Key::SyncIntervalSeconds, mins * 60);
    });
    scheduleForm->addRow(tr("Check for new mail every:"), intervalSpin);

    // Header Fetch Limit
    auto* fetchLimitSpin = new QSpinBox(scheduleGroup);
    fetchLimitSpin->setRange(10, 1000);
    fetchLimitSpin->setSingleStep(10);
    fetchLimitSpin->setSuffix(tr(" messages"));
    fetchLimitSpin->setValue(Settings::instance().intValue(Settings::Key::SyncFetchLimit));

    connect(fetchLimitSpin, &QSpinBox::valueChanged, this, [](int limit) {
        Settings::instance().setValue(Settings::Key::SyncFetchLimit, limit);
    });
    scheduleForm->addRow(tr("Fetch limit per folder:"), fetchLimitSpin);

    outer->addWidget(scheduleGroup);

    // -- Storage & Cache group ---------------------------------------------
    auto* cacheGroup = new QGroupBox(tr("Storage & Cache"), page);
    auto* cacheForm  = new QFormLayout(cacheGroup);
    cacheForm->setContentsMargins(12, 12, 12, 12);
    cacheForm->setRowWrapPolicy(QFormLayout::WrapLongRows);

    // Cache limit spinbox (MB)
    auto* cacheLimitSpin = new QSpinBox(cacheGroup);
    cacheLimitSpin->setRange(50, 10000); // 50MB to 10GB
    cacheLimitSpin->setSingleStep(50);
    cacheLimitSpin->setSuffix(tr(" MB"));
    cacheLimitSpin->setValue(Settings::instance().intValue(Settings::Key::SyncOfflineCacheMB));

    connect(cacheLimitSpin, &QSpinBox::valueChanged, this, [](int limitMb) {
        Settings::instance().setValue(Settings::Key::SyncOfflineCacheMB, limitMb);
    });
    cacheForm->addRow(tr("Offline storage limit:"), cacheLimitSpin);

    outer->addWidget(cacheGroup);
    outer->addStretch();

    return page;
}

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
                   QApplication::palette().color(QPalette::Disabled, QPalette::WindowText));
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
