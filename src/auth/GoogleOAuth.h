#pragma once

#include <QObject>
#include <QString>
#include <QTcpServer>
#include <QNetworkAccessManager>
#include <QNetworkReply>

//
// GoogleOAuth
// Handles the official Google OAuth 2.0 Desktop Application flow (Loopback IP).
//
class GoogleOAuth : public QObject
{
    Q_OBJECT

public:
    explicit GoogleOAuth(QObject* parent = nullptr);
    ~GoogleOAuth() override;

    // Starts the OAuth 2.0 flow by opening the system browser and listening on a local port.
    void startFlow();

    // Starts re-authentication flow with an optional login hint (email).
    void startReauthFlow(const QString& loginHint = QStringLiteral(""));

    // Refreshes the access token using the provided refresh token.
    void refreshAccessToken(const QString& refreshToken);

    // Configuration checks
    bool isConfigured() const { return !m_clientId.isEmpty(); }
    QString clientId() const { return m_clientId; }
    QString clientSecret() const { return m_clientSecret; }

signals:
    // Emitted when the flow succeeds (either initial login or refresh)
    void authSuccessful(const QString& email, const QString& accessToken, const QString& refreshToken, int expiresInSecs);
    
    // Emitted when an error occurs during the flow. isPermanent indicates if refresh token was revoked/rejected (e.g. invalid_grant).
    void authFailed(const QString& errorMsg, bool isPermanent = false);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onTokenRequestFinished(QNetworkReply* reply);

private:
    void loadCredentials();
    void exchangeCodeForTokens(const QString& code);
    void fetchUserInfo(const QString& accessToken, const QString& refreshToken, int expiresInSecs);
    
    QString generateCodeVerifier();
    QString generateCodeChallenge(const QString& verifier);

    QTcpServer* m_server = nullptr;
    QNetworkAccessManager* m_networkManager = nullptr;
    
    QString m_state;
    QString m_codeVerifier;
    QString m_clientId;
    QString m_clientSecret;
    int m_port = 0;
};
