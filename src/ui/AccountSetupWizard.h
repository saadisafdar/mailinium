#pragma once

#include <QWizard>
#include <QString>

class QLineEdit;
class AccountStore;

//
// AccountSetupWizard — guides the user through setting up a new email account.
//
// Currently implements a basic flow:
// 1. Account Details (Email, Display Name)
// 2. Server Settings (IMAP/SMTP Host & Port)
//
class AccountSetupWizard : public QWizard
{
    Q_OBJECT

public:
    explicit AccountSetupWizard(QWidget* parent = nullptr);
    ~AccountSetupWizard() override = default;

    // Call after the wizard accepts to get the ID of the newly created account.
    // Returns -1 if the setup failed or was canceled.
    qint64 createdAccountId() const;

    // Overridden to intercept the accept action and perform database insertion.
    void accept() override;

private:
    void setupPages();

    qint64 m_createdAccountId = -1;
};

// ---------------------------------------------------------------------------
// Wizard Pages
// ---------------------------------------------------------------------------

class AccountDetailsPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit AccountDetailsPage(QWidget* parent = nullptr);

    bool isComplete() const override;

private slots:
    void onFieldChanged();

private:
    QLineEdit* m_emailEdit = nullptr;
    QLineEdit* m_displayNameEdit = nullptr;
};

class ServerSettingsPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit ServerSettingsPage(QWidget* parent = nullptr);

    bool isComplete() const override;

private slots:
    void onFieldChanged();

private:
    QLineEdit* m_imapHostEdit = nullptr;
    QLineEdit* m_imapPortEdit = nullptr;
    QLineEdit* m_smtpHostEdit = nullptr;
    QLineEdit* m_smtpPortEdit = nullptr;
};
