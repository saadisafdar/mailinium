#include "ui/AccountSetupWizard.h"

#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QVBoxLayout>

#include "storage/Account.h"
#include "storage/AccountStore.h"
#include "auth/GoogleOAuth.h"
#include "core/Logger.h"

#include <QPushButton>

// ===========================================================================
// AccountSetupWizard
// ===========================================================================

AccountSetupWizard::AccountSetupWizard(QWidget* parent)
    : QWizard(parent)
{
    setWindowTitle(tr("Add Email Account"));
    setWizardStyle(QWizard::ModernStyle);
    setMinimumSize(500, 400);

    setupPages();
}

void AccountSetupWizard::setupPages()
{
    setPage(Page_Intro, new IntroPage(this));
    setPage(Page_Details, new AccountDetailsPage(this));
    setPage(Page_Server, new ServerSettingsPage(this));
    
    setStartId(Page_Intro);
}

qint64 AccountSetupWizard::createdAccountId() const
{
    return m_createdAccountId;
}

void AccountSetupWizard::accept()
{
    // Accept should only happen manually or from Manual IMAP flow.
    // If we're here and m_createdAccountId > 0, it means Google OAuth succeeded and bypassed this.
    if (m_createdAccountId > 0) {
        QWizard::accept();
        return;
    }

    // Retrieve data from manual wizard fields
    Account acc;
    acc.email       = field(QStringLiteral("email")).toString().trimmed();
    acc.displayName = field(QStringLiteral("displayName")).toString().trimmed();
    acc.password    = field(QStringLiteral("password")).toString();
    acc.imapHost    = field(QStringLiteral("imapHost")).toString().trimmed();
    acc.imapPort    = field(QStringLiteral("imapPort")).toInt();
    acc.smtpHost    = field(QStringLiteral("smtpHost")).toString().trimmed();
    acc.smtpPort    = field(QStringLiteral("smtpPort")).toInt();
    acc.provider    = QStringLiteral("Custom"); // Simplification for now
    acc.authType    = AuthType::Password;

    // Validate (basic validation since pages also validate)
    if (acc.email.isEmpty() || acc.imapHost.isEmpty() || acc.smtpHost.isEmpty()) {
        QMessageBox::critical(this, tr("Error"), tr("Missing required fields."));
        return; // Do not accept
    }

    AccountStore store;
    
    if (store.accountExists(acc.email, acc.provider)) {
        QMessageBox::warning(this, tr("Duplicate Account"), tr("An account with this email and provider already exists."));
        return; // Do not accept
    }
    
    m_createdAccountId = store.insertAccount(acc);

    if (m_createdAccountId < 0) {
        QMessageBox::critical(this, tr("Database Error"), tr("Failed to save the account to the database."));
        return; // Do not accept
    }

    // Call base class implementation to close the dialog with Accepted result
    QWizard::accept();
}

// ===========================================================================
// IntroPage
// ===========================================================================

IntroPage::IntroPage(QWidget* parent)
    : QWizardPage(parent)
{
    setTitle(tr("Add Account"));
    setSubTitle(tr("Choose an account provider to get started."));

    m_btnGoogle = new QPushButton(tr("Sign in with Google"), this);
    m_btnGoogle->setMinimumHeight(40);
    
    m_btnManual = new QPushButton(tr("Manual IMAP (Advanced)"), this);
    m_btnManual->setFlat(true);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #666;"));
    m_statusLabel->hide();

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addStretch();
    layout->addWidget(m_btnGoogle);
    layout->addSpacing(20);
    layout->addWidget(m_btnManual);
    layout->addWidget(m_statusLabel);
    layout->addStretch();

    connect(m_btnGoogle, &QPushButton::clicked, this, &IntroPage::onGoogleClicked);
    connect(m_btnManual, &QPushButton::clicked, this, &IntroPage::onManualClicked);
}

int IntroPage::nextId() const
{
    if (m_manualSelected) {
        return AccountSetupWizard::Page_Details;
    }
    return -1; // No next page if waiting for Google Auth
}

bool IntroPage::isComplete() const
{
    // The page is only "complete" for clicking Next if manual is selected.
    return m_manualSelected;
}

void IntroPage::onGoogleClicked()
{
    m_btnGoogle->setEnabled(false);
    m_btnManual->setEnabled(false);
    
    m_statusLabel->setText(tr("Waiting for authentication in browser..."));
    m_statusLabel->show();

    if (!m_oauth) {
        m_oauth = new GoogleOAuth(this);
        connect(m_oauth, &GoogleOAuth::authSuccessful, this, &IntroPage::onGoogleAuthSuccessful);
        connect(m_oauth, &GoogleOAuth::authFailed, this, &IntroPage::onGoogleAuthFailed);
    }
    
    m_oauth->startFlow();
}

void IntroPage::onManualClicked()
{
    m_manualSelected = true;
    emit completeChanged();
    wizard()->next(); // Automatically move to the next page
}

void IntroPage::onGoogleAuthSuccessful(const QString& email, const QString& accessToken, const QString& refreshToken, int expiresInSecs)
{
    Logger::instance().info(QStringLiteral("ui"), QStringLiteral("Google OAuth successful, creating account."));
    
    Account acc;
    acc.email = email;
    acc.displayName = email; // Fallback, could fetch from user info
    acc.provider = QStringLiteral("google");
    acc.imapHost = QStringLiteral("imap.gmail.com");
    acc.imapPort = 993;
    acc.smtpHost = QStringLiteral("smtp.gmail.com");
    acc.smtpPort = 465;
    
    acc.authType = AuthType::OAuth2;
    acc.accessToken = accessToken;
    acc.refreshToken = refreshToken;
    acc.tokenExpiry = QDateTime::currentDateTimeUtc().addSecs(expiresInSecs);
    
    AccountStore store;
    
    if (store.accountExists(acc.email, acc.provider)) {
        onGoogleAuthFailed(tr("An account with this email already exists."));
        return;
    }
    
    qint64 id = store.insertAccount(acc);
    
    if (id > 0) {
        auto* mainWizard = qobject_cast<AccountSetupWizard*>(wizard());
        if (mainWizard) {
            mainWizard->setCreatedAccountId(id);
            mainWizard->accept();
        }
    } else {
        onGoogleAuthFailed(tr("Failed to save account to database."));
    }
}

void IntroPage::onGoogleAuthFailed(const QString& errorMsg)
{
    m_statusLabel->setText(tr("Authentication failed: ") + errorMsg);
    m_btnGoogle->setEnabled(true);
    m_btnManual->setEnabled(true);
}

// ===========================================================================
// AccountDetailsPage
// ===========================================================================

AccountDetailsPage::AccountDetailsPage(QWidget* parent)
    : QWizardPage(parent)
{
    setTitle(tr("Account Details"));
    setSubTitle(tr("Enter your email address and name."));

    m_emailEdit = new QLineEdit(this);
    m_emailEdit->setPlaceholderText(tr("you@example.com"));

    m_displayNameEdit = new QLineEdit(this);
    m_displayNameEdit->setPlaceholderText(tr("Your Name"));

    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText(tr("Password"));

    // Register fields so QWizard can store/retrieve their values
    registerField(QStringLiteral("email*"), m_emailEdit); // * means required
    registerField(QStringLiteral("displayName"), m_displayNameEdit);
    registerField(QStringLiteral("password*"), m_passwordEdit);

    QFormLayout* layout = new QFormLayout(this);
    layout->addRow(tr("&Email Address:"), m_emailEdit);
    layout->addRow(tr("&Display Name:"), m_displayNameEdit);
    layout->addRow(tr("&Password:"), m_passwordEdit);

    connect(m_emailEdit, &QLineEdit::textChanged, this, &AccountDetailsPage::onFieldChanged);
    connect(m_passwordEdit, &QLineEdit::textChanged, this, &AccountDetailsPage::onFieldChanged);
}

int AccountDetailsPage::nextId() const
{
    return AccountSetupWizard::Page_Server;
}

bool AccountDetailsPage::isComplete() const
{
    return !m_emailEdit->text().trimmed().isEmpty() && !m_passwordEdit->text().isEmpty();
}

void AccountDetailsPage::onFieldChanged()
{
    emit completeChanged();
}

// ===========================================================================
// ServerSettingsPage
// ===========================================================================

ServerSettingsPage::ServerSettingsPage(QWidget* parent)
    : QWizardPage(parent)
{
    setTitle(tr("Server Settings"));
    setSubTitle(tr("Configure IMAP and SMTP servers for this account."));

    m_imapHostEdit = new QLineEdit(this);
    m_imapHostEdit->setPlaceholderText(tr("imap.example.com"));

    m_imapPortEdit = new QLineEdit(QStringLiteral("993"), this);

    m_smtpHostEdit = new QLineEdit(this);
    m_smtpHostEdit->setPlaceholderText(tr("smtp.example.com"));

    m_smtpPortEdit = new QLineEdit(QStringLiteral("587"), this);

    registerField(QStringLiteral("imapHost*"), m_imapHostEdit);
    registerField(QStringLiteral("imapPort"), m_imapPortEdit);
    registerField(QStringLiteral("smtpHost*"), m_smtpHostEdit);
    registerField(QStringLiteral("smtpPort"), m_smtpPortEdit);

    QFormLayout* layout = new QFormLayout(this);
    layout->addRow(tr("IMAP &Host:"), m_imapHostEdit);
    layout->addRow(tr("IMAP &Port:"), m_imapPortEdit);
    layout->addRow(tr("SMTP H&ost:"), m_smtpHostEdit);
    layout->addRow(tr("SMTP P&ort:"), m_smtpPortEdit);

    connect(m_imapHostEdit, &QLineEdit::textChanged, this, &ServerSettingsPage::onFieldChanged);
    connect(m_smtpHostEdit, &QLineEdit::textChanged, this, &ServerSettingsPage::onFieldChanged);
}

int ServerSettingsPage::nextId() const
{
    return -1; // Final page
}

bool ServerSettingsPage::isComplete() const
{
    return !m_imapHostEdit->text().trimmed().isEmpty() &&
           !m_smtpHostEdit->text().trimmed().isEmpty();
}

void ServerSettingsPage::onFieldChanged()
{
    emit completeChanged();
}
