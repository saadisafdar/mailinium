#pragma once

#include <QWizard>
#include <QString>

class QLineEdit;
class QPushButton;
class QLabel;
class AccountStore;
class GoogleOAuth;

//
// AccountSetupWizard — guides the user through setting up a new email account.
//
//
// Currently implements two flows:
// 1. Google OAuth (default) -> Instantly creates account on success
// 2. Manual IMAP (Advanced) -> Account Details -> Server Settings
//
class AccountSetupWizard : public QWizard
{
    Q_OBJECT

public:
    enum {
        Page_Intro,
        Page_Details,
        Page_Server
    };

    explicit AccountSetupWizard(QWidget* parent = nullptr);
    ~AccountSetupWizard() override = default;

    // Call after the wizard accepts to get the ID of the newly created account.
    // Returns -1 if the setup failed or was canceled.
    qint64 createdAccountId() const;

    // Overridden to intercept the accept action and perform database insertion.
    void accept() override;

    void setCreatedAccountId(qint64 id) { m_createdAccountId = id; }

private:
    void setupPages();

    qint64 m_createdAccountId = -1;
};

// ---------------------------------------------------------------------------
// Wizard Pages
// ---------------------------------------------------------------------------

class IntroPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit IntroPage(QWidget* parent = nullptr);

    int nextId() const override;
    bool isComplete() const override;

private slots:
    void onGoogleClicked();
    void onManualClicked();
    
    void onGoogleAuthSuccessful(const QString& email, const QString& accessToken, const QString& refreshToken, int expiresInSecs);
    void onGoogleAuthFailed(const QString& errorMsg);

private:
    QPushButton* m_btnGoogle = nullptr;
    QPushButton* m_btnManual = nullptr;
    QLabel* m_statusLabel = nullptr;
    
    GoogleOAuth* m_oauth = nullptr;
    bool m_manualSelected = false;
};

class AccountDetailsPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit AccountDetailsPage(QWidget* parent = nullptr);

    int nextId() const override;
    bool isComplete() const override;

private slots:
    void onFieldChanged();

private:
    QLineEdit* m_emailEdit = nullptr;
    QLineEdit* m_displayNameEdit = nullptr;
    QLineEdit* m_passwordEdit = nullptr;
};

class ServerSettingsPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit ServerSettingsPage(QWidget* parent = nullptr);

    int nextId() const override;
    bool isComplete() const override;

private slots:
    void onFieldChanged();

private:
    QLineEdit* m_imapHostEdit = nullptr;
    QLineEdit* m_imapPortEdit = nullptr;
    QLineEdit* m_smtpHostEdit = nullptr;
    QLineEdit* m_smtpPortEdit = nullptr;
};
