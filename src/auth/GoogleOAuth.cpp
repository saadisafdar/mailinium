#include "auth/GoogleOAuth.h"
#include "core/Logger.h"

#include <QCryptographicHash>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>
#include <QRandomGenerator>

#include <QCoreApplication>
#include <QDir>
#include <QFile>

static const QString GOOGLE_AUTH_URI = QStringLiteral("https://accounts.google.com/o/oauth2/v2/auth");
static const QString GOOGLE_TOKEN_URI = QStringLiteral("https://oauth2.googleapis.com/token");
static const QString GOOGLE_USERINFO_URI = QStringLiteral("https://www.googleapis.com/oauth2/v3/userinfo");

GoogleOAuth::GoogleOAuth(QObject* parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
    , m_networkManager(new QNetworkAccessManager(this))
{
    loadCredentials();
    connect(m_server, &QTcpServer::newConnection, this, &GoogleOAuth::onNewConnection);
}

GoogleOAuth::~GoogleOAuth()
{
    if (m_server->isListening()) {
        m_server->close();
    }
}

void GoogleOAuth::startFlow()
{
    if (m_clientId.isEmpty()) {
        emit authFailed(QStringLiteral("OAuth Client ID is missing. Please configure config/oauth.json."));
        return;
    }

    // Start listening on a random available port on the loopback interface
    if (!m_server->listen(QHostAddress::LocalHost, 0)) {
        emit authFailed(QStringLiteral("Failed to start local web server for OAuth redirect."));
        return;
    }

    m_port = m_server->serverPort();
    Logger::instance().info(QStringLiteral("oauth"), QStringLiteral("Listening on local port: ") + QString::number(m_port));

    // Generate random state to prevent CSRF
    m_state = QString::number(QRandomGenerator::global()->generate64(), 16);
    
    // Generate PKCE verifier and challenge
    m_codeVerifier = generateCodeVerifier();
    QString codeChallenge = generateCodeChallenge(m_codeVerifier);

    QUrl url(GOOGLE_AUTH_URI);
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("client_id"), m_clientId);
    query.addQueryItem(QStringLiteral("redirect_uri"), QStringLiteral("http://127.0.0.1:") + QString::number(m_port));
    query.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
    query.addQueryItem(QStringLiteral("scope"), QStringLiteral("https://mail.google.com/ https://www.googleapis.com/auth/userinfo.email"));
    query.addQueryItem(QStringLiteral("access_type"), QStringLiteral("offline"));
    query.addQueryItem(QStringLiteral("state"), m_state);
    query.addQueryItem(QStringLiteral("code_challenge"), codeChallenge);
    query.addQueryItem(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
    query.addQueryItem(QStringLiteral("prompt"), QStringLiteral("consent")); // Force consent to ensure refresh token is issued

    url.setQuery(query);

    Logger::instance().info(QStringLiteral("oauth"), QStringLiteral("Opening browser to Google OAuth..."));
    
    if (!QDesktopServices::openUrl(url)) {
        m_server->close();
        emit authFailed(QStringLiteral("Failed to open the system browser."));
    }
}

void GoogleOAuth::startReauthFlow(const QString& loginHint)
{
    if (m_clientId.isEmpty()) {
        emit authFailed(QStringLiteral("OAuth Client ID is missing. Please configure config/oauth.json."), true);
        return;
    }

    if (!m_server->listen(QHostAddress::LocalHost, 0)) {
        emit authFailed(QStringLiteral("Failed to start local web server for OAuth redirect."), false);
        return;
    }

    m_port = m_server->serverPort();
    Logger::instance().info(QStringLiteral("oauth"), QStringLiteral("Listening on local port for re-auth: ") + QString::number(m_port));

    m_state = QString::number(QRandomGenerator::global()->generate64(), 16);
    m_codeVerifier = generateCodeVerifier();
    QString codeChallenge = generateCodeChallenge(m_codeVerifier);

    QUrl url(GOOGLE_AUTH_URI);
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("client_id"), m_clientId);
    query.addQueryItem(QStringLiteral("redirect_uri"), QStringLiteral("http://127.0.0.1:") + QString::number(m_port));
    query.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
    query.addQueryItem(QStringLiteral("scope"), QStringLiteral("https://mail.google.com/ https://www.googleapis.com/auth/userinfo.email"));
    query.addQueryItem(QStringLiteral("access_type"), QStringLiteral("offline"));
    query.addQueryItem(QStringLiteral("state"), m_state);
    query.addQueryItem(QStringLiteral("code_challenge"), codeChallenge);
    query.addQueryItem(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
    query.addQueryItem(QStringLiteral("prompt"), QStringLiteral("consent"));
    if (!loginHint.isEmpty()) {
        query.addQueryItem(QStringLiteral("login_hint"), loginHint);
    }

    url.setQuery(query);

    Logger::instance().info(QStringLiteral("oauth"), QStringLiteral("Opening browser for re-authentication: ") + loginHint);
    
    if (!QDesktopServices::openUrl(url)) {
        m_server->close();
        emit authFailed(QStringLiteral("Failed to open the system browser."), false);
    }
}

void GoogleOAuth::onNewConnection()
{
    QTcpSocket* socket = m_server->nextPendingConnection();
    connect(socket, &QTcpSocket::readyRead, this, &GoogleOAuth::onReadyRead);
    connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
}

void GoogleOAuth::onReadyRead()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    QByteArray data = socket->readAll();
    QString request(data);

    // Parse the GET request (simplified HTTP parser)
    int firstSpace = request.indexOf(' ');
    int secondSpace = request.indexOf(' ', firstSpace + 1);
    
    if (firstSpace != -1 && secondSpace != -1) {
        QString path = request.mid(firstSpace + 1, secondSpace - firstSpace - 1);
        QUrl url(QStringLiteral("http://localhost") + path);
        QUrlQuery query(url);

        QString code = query.queryItemValue(QStringLiteral("code"));
        QString state = query.queryItemValue(QStringLiteral("state"));
        QString error = query.queryItemValue(QStringLiteral("error"));

        if (!error.isEmpty()) {
            emit authFailed(QStringLiteral("OAuth Error: ") + error);
        } else if (state != m_state) {
            emit authFailed(QStringLiteral("OAuth Error: Invalid state parameter."));
        } else if (!code.isEmpty()) {
            exchangeCodeForTokens(code);
        } else {
            // Ignore requests (like favicon.ico) that don't have code/error
            return;
        }

        // Send a simple HTML response
        QString html = QStringLiteral(
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Connection: close\r\n"
            "\r\n"
            "<html><head><title>Authentication Complete</title></head>"
            "<body style='font-family: sans-serif; text-align: center; margin-top: 100px;'>"
            "<h2>Authentication Complete</h2>"
            "<p>You can close this window and return to Mailinium.</p>"
            "</body></html>"
        );
        socket->write(html.toUtf8());
        socket->flush();
    }
    
    socket->disconnectFromHost();
    
    // Stop listening once we've processed a valid response
    if (m_server->isListening()) {
        m_server->close();
    }
}

void GoogleOAuth::exchangeCodeForTokens(const QString& code)
{
    Logger::instance().info(QStringLiteral("oauth"), QStringLiteral("Exchanging authorization code for tokens..."));

    QUrl url(GOOGLE_TOKEN_URI);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("client_id"), m_clientId);
    query.addQueryItem(QStringLiteral("client_secret"), m_clientSecret);
    query.addQueryItem(QStringLiteral("code"), code);
    query.addQueryItem(QStringLiteral("code_verifier"), m_codeVerifier);
    query.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("authorization_code"));
    query.addQueryItem(QStringLiteral("redirect_uri"), QStringLiteral("http://127.0.0.1:") + QString::number(m_port));

    QByteArray postData = query.toString(QUrl::FullyEncoded).toUtf8();
    
    QNetworkReply* reply = m_networkManager->post(request, postData);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onTokenRequestFinished(reply);
    });
}

void GoogleOAuth::onTokenRequestFinished(QNetworkReply* reply)
{
    reply->deleteLater();
    
    if (reply->error() != QNetworkReply::NoError) {
        emit authFailed(QStringLiteral("Failed to retrieve tokens: ") + reply->errorString());
        return;
    }

    QByteArray response = reply->readAll();
    QJsonDocument json = QJsonDocument::fromJson(response);
    if (!json.isObject()) {
        emit authFailed(QStringLiteral("Invalid token response format."));
        return;
    }

    QJsonObject obj = json.object();
    QString accessToken = obj.value(QStringLiteral("access_token")).toString();
    QString refreshToken = obj.value(QStringLiteral("refresh_token")).toString();
    int expiresIn = obj.value(QStringLiteral("expires_in")).toInt(3600);

    if (accessToken.isEmpty()) {
        emit authFailed(QStringLiteral("Access token missing from response."));
        return;
    }

    // Now we need the user's email address
    fetchUserInfo(accessToken, refreshToken, expiresIn);
}

void GoogleOAuth::fetchUserInfo(const QString& accessToken, const QString& refreshToken, int expiresInSecs)
{
    Logger::instance().info(QStringLiteral("oauth"), QStringLiteral("Fetching user profile..."));

    QUrl url(GOOGLE_USERINFO_URI);
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", ("Bearer " + accessToken).toUtf8());

    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, accessToken, refreshToken, expiresInSecs]() {
        reply->deleteLater();
        
        if (reply->error() != QNetworkReply::NoError) {
            emit authFailed(QStringLiteral("Failed to fetch user info: ") + reply->errorString());
            return;
        }

        QByteArray response = reply->readAll();
        QJsonDocument json = QJsonDocument::fromJson(response);
        if (!json.isObject()) {
            emit authFailed(QStringLiteral("Invalid user info response format."));
            return;
        }

        QString email = json.object().value(QStringLiteral("email")).toString();
        if (email.isEmpty()) {
            emit authFailed(QStringLiteral("Failed to extract email address from profile."));
            return;
        }

        Logger::instance().info(QStringLiteral("oauth"), QStringLiteral("OAuth flow successful for: ") + email);
        emit authSuccessful(email, accessToken, refreshToken, expiresInSecs);
    });
}

void GoogleOAuth::refreshAccessToken(const QString& refreshToken)
{
    if (refreshToken.isEmpty()) {
        emit authFailed(QStringLiteral("No refresh token available."));
        return;
    }

    Logger::instance().info(QStringLiteral("oauth"), QStringLiteral("Refreshing access token..."));

    QUrl url(GOOGLE_TOKEN_URI);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("client_id"), m_clientId);
    query.addQueryItem(QStringLiteral("client_secret"), m_clientSecret);
    query.addQueryItem(QStringLiteral("refresh_token"), refreshToken);
    query.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));

    QByteArray postData = query.toString(QUrl::FullyEncoded).toUtf8();
    
    QNetworkReply* reply = m_networkManager->post(request, postData);
    connect(reply, &QNetworkReply::finished, this, [this, reply, refreshToken]() {
        reply->deleteLater();
        
        QByteArray response = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            bool isPermanent = false;
            QString errorDetails = reply->errorString();

            // Inspect response for OAuth error tokens (e.g. invalid_grant on token revocation/expiration)
            QJsonDocument json = QJsonDocument::fromJson(response);
            if (json.isObject()) {
                QString err = json.object().value(QStringLiteral("error")).toString();
                QString desc = json.object().value(QStringLiteral("error_description")).toString();
                if (!err.isEmpty()) {
                    errorDetails = err + (desc.isEmpty() ? QStringLiteral("") : (QStringLiteral(": ") + desc));
                }
                if (err == QStringLiteral("invalid_grant") ||
                    err == QStringLiteral("invalid_client") ||
                    err == QStringLiteral("unauthorized_client")) {
                    isPermanent = true;
                }
            } else {
                int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                if (httpStatus == 400 || httpStatus == 401) {
                    isPermanent = true;
                }
            }

            Logger::instance().error(QStringLiteral("oauth"),
                QStringLiteral("Failed to refresh token (permanent=%1): %2").arg(isPermanent).arg(errorDetails));
            emit authFailed(QStringLiteral("Failed to refresh token: ") + errorDetails, isPermanent);
            return;
        }

        QJsonDocument json = QJsonDocument::fromJson(response);
        QJsonObject obj = json.object();
        
        QString accessToken = obj.value(QStringLiteral("access_token")).toString();
        int expiresIn = obj.value(QStringLiteral("expires_in")).toInt(3600);
        
        // Sometimes Google issues a new refresh token, otherwise we keep the old one
        QString newRefreshToken = obj.value(QStringLiteral("refresh_token")).toString();
        if (newRefreshToken.isEmpty()) {
            newRefreshToken = refreshToken;
        }

        if (accessToken.isEmpty()) {
            emit authFailed(QStringLiteral("Failed to parse new access token."), false);
            return;
        }

        // For refresh, we don't fetch user info again, just emit success
        emit authSuccessful(QStringLiteral(""), accessToken, newRefreshToken, expiresIn);
    });
}

QString GoogleOAuth::generateCodeVerifier()
{
    // A 43-128 character cryptographically random string (unreserved characters)
    const QString charset = QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~");
    QString verifier;
    for (int i = 0; i < 64; ++i) {
        verifier.append(charset.at(QRandomGenerator::global()->bounded(charset.length())));
    }
    return verifier;
}

QString GoogleOAuth::generateCodeChallenge(const QString& verifier)
{
    // BASE64URL-ENCODE(SHA256(ASCII(code_verifier)))
    QByteArray hash = QCryptographicHash::hash(verifier.toUtf8(), QCryptographicHash::Sha256);
    
    // Base64Url encode without padding
    return QString::fromUtf8(hash.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

void GoogleOAuth::loadCredentials()
{
    // Try to load from the executable's config folder first, then the current working directory's config folder
    QStringList paths = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/config/oauth.json"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../config/oauth.json"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../../config/oauth.json"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../../../config/oauth.json"), // When running from build/bin/Debug or similar
        QDir::currentPath() + QStringLiteral("/config/oauth.json")
    };
    
    for (const QString& path : paths) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray data = file.readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                if (obj.contains(QStringLiteral("google")) && obj.value(QStringLiteral("google")).isObject()) {
                    QJsonObject googleObj = obj.value(QStringLiteral("google")).toObject();
                    m_clientId = googleObj.value(QStringLiteral("client_id")).toString();
                    m_clientSecret = googleObj.value(QStringLiteral("client_secret")).toString();
                    Logger::instance().info(QStringLiteral("oauth"), QStringLiteral("Loaded OAuth credentials from ") + path);
                    return;
                }
            }
        }
    }
    
    Logger::instance().error(QStringLiteral("oauth"), QStringLiteral("Failed to load OAuth credentials. oauth.json not found."));
}
