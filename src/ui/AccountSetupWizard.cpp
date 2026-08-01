#include "ui/AccountSetupWizard.h"

#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QVBoxLayout>

#include "storage/Account.h"
#include "storage/AccountStore.h"

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
    addPage(new AccountDetailsPage(this));
    addPage(new ServerSettingsPage(this));
}

qint64 AccountSetupWizard::createdAccountId() const
{
    return m_createdAccountId;
}

void AccountSetupWizard::accept()
{
    // Retrieve data from wizard fields
    Account acc;
    acc.email       = field(QStringLiteral("email")).toString().trimmed();
    acc.displayName = field(QStringLiteral("displayName")).toString().trimmed();
    acc.password    = field(QStringLiteral("password")).toString();
    acc.imapHost    = field(QStringLiteral("imapHost")).toString().trimmed();
    acc.imapPort    = field(QStringLiteral("imapPort")).toInt();
    acc.smtpHost    = field(QStringLiteral("smtpHost")).toString().trimmed();
    acc.smtpPort    = field(QStringLiteral("smtpPort")).toInt();
    acc.provider    = QStringLiteral("Custom"); // Simplification for now

    // Validate (basic validation since pages also validate)
    if (acc.email.isEmpty() || acc.imapHost.isEmpty() || acc.smtpHost.isEmpty()) {
        QMessageBox::critical(this, tr("Error"), tr("Missing required fields."));
        return; // Do not accept
    }

    AccountStore store;
    m_createdAccountId = store.insertAccount(acc);

    if (m_createdAccountId < 0) {
        QMessageBox::critical(this, tr("Database Error"), tr("Failed to save the account to the database."));
        return; // Do not accept
    }

    // Call base class implementation to close the dialog with Accepted result
    QWizard::accept();
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

bool ServerSettingsPage::isComplete() const
{
    return !m_imapHostEdit->text().trimmed().isEmpty() &&
           !m_smtpHostEdit->text().trimmed().isEmpty();
}

void ServerSettingsPage::onFieldChanged()
{
    emit completeChanged();
}
