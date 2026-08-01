#include "network/ImapClient.h"
#include "core/Logger.h"

#include <QSslError>
#include <QStringList>

ImapClient::ImapClient(QObject* parent)
    : QObject(parent)
{
    m_socket = new QSslSocket(this);

    connect(m_socket, &QSslSocket::connected, this, &ImapClient::onSocketConnected);
    connect(m_socket, &QSslSocket::encrypted, this, [this]() {
        Logger::instance().info(QStringLiteral("imap"), QStringLiteral("SSL handshake completed (encrypted)."));
    });
    connect(m_socket, &QSslSocket::disconnected, this, &ImapClient::onSocketDisconnected);
    connect(m_socket, &QSslSocket::errorOccurred, this, &ImapClient::onSocketError);
    connect(m_socket, &QSslSocket::sslErrors, this, &ImapClient::onSslErrors);
    connect(m_socket, &QSslSocket::readyRead, this, &ImapClient::onReadyRead);
}

ImapClient::~ImapClient()
{
    if (m_socket->isOpen()) {
        m_socket->close();
    }
}

void ImapClient::connectToServer(const QString& host, quint16 port)
{
    if (m_state != State::Disconnected) {
        disconnectFromServer();
    }

    m_state = State::Connecting;
    Logger::instance().info(QStringLiteral("imap"), QStringLiteral("Connecting to ") + host + ":" + QString::number(port));
    m_socket->connectToHostEncrypted(host, port);
}

void ImapClient::disconnectFromServer()
{
    m_state = State::Disconnected;
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->disconnectFromHost();
    }
}

void ImapClient::login(const QString& username, const QString& password)
{
    if (m_state != State::NotAuthenticated) {
        emit loginFailed(QStringLiteral("Not connected or already authenticated."));
        return;
    }

    m_state = State::Authenticating;
    m_currentTag = generateTag().toUtf8();
    
    // Quote credentials securely (simplified for Phase 3C)
    // Note: Do not log the password!
    QString cmd = QStringLiteral("%1 LOGIN \"%2\" \"%3\"\r\n")
                  .arg(QString::fromUtf8(m_currentTag))
                  .arg(username)
                  .arg(password);
                  
    Logger::instance().debug(QStringLiteral("imap"), QStringLiteral("Sending LOGIN for ") + username);
    m_socket->write(cmd.toUtf8());
}

void ImapClient::listFolders()
{
    if (m_state != State::Authenticated) {
        emit listFailed(QStringLiteral("Not authenticated."));
        return;
    }

    m_state = State::ListingFolders;
    m_parsedFolders.clear();
    m_currentTag = generateTag().toUtf8();
    
    QByteArray cmd = m_currentTag + " LIST \"\" \"*\"\r\n";
    Logger::instance().debug(QStringLiteral("imap"), QStringLiteral("Sending LIST command"));
    m_socket->write(cmd);
}

QString ImapClient::generateTag()
{
    ++m_tagCounter;
    return QStringLiteral("A%1").arg(m_tagCounter, 4, 10, QLatin1Char('0'));
}

void ImapClient::onSocketConnected()
{
    Logger::instance().info(QStringLiteral("imap"), QStringLiteral("Connected to IMAP server"));
    m_state = State::Greeting;
    // Wait for the greeting before emitting connected(), handled in processLine.
}

void ImapClient::onSocketDisconnected()
{
    Logger::instance().info(QStringLiteral("imap"), QStringLiteral("Disconnected from IMAP server"));
    m_state = State::Disconnected;
    emit disconnected();
}

void ImapClient::onSocketError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError)
    QString errorMsg = m_socket->errorString();
    Logger::instance().error(QStringLiteral("imap"), QStringLiteral("Socket error: ") + errorMsg);
    
    if (m_state == State::Connecting) {
        emit errorOccurred(errorMsg);
    }
    m_state = State::Disconnected;
}

void ImapClient::onSslErrors(const QList<QSslError>& errors)
{
    for (const auto& error : errors) {
        Logger::instance().error(QStringLiteral("imap"), QStringLiteral("SSL Error: ") + error.errorString());
    }
    // In production we might allow ignoring certain errors based on settings,
    // but for now we ignore them automatically just for easier testing with self-signed certs.
    m_socket->ignoreSslErrors();
}

void ImapClient::onReadyRead()
{
    while (m_socket->canReadLine()) {
        QByteArray line = m_socket->readLine();
        // Remove \r\n
        if (line.endsWith("\r\n")) {
            line.chop(2);
        } else if (line.endsWith("\n")) {
            line.chop(1);
        }
        processLine(line);
    }
}

void ImapClient::processLine(const QByteArray& line)
{
    Logger::instance().debug(QStringLiteral("imap"), QStringLiteral("S: ") + QString::fromUtf8(line));

    if (m_state == State::Greeting) {
        handleGreeting(line);
    } else if (m_state == State::Authenticating) {
        handleLoginResponse(line);
    } else if (m_state == State::ListingFolders) {
        handleListResponse(line);
    }
}

void ImapClient::handleGreeting(const QByteArray& line)
{
    if (line.startsWith("* OK") || line.startsWith("* PREAUTH")) {
        Logger::instance().info(QStringLiteral("imap"), QStringLiteral("Received valid greeting."));
        m_state = State::NotAuthenticated;
        emit connected();
    } else {
        m_state = State::Disconnected;
        emit errorOccurred(QStringLiteral("Unexpected greeting: ") + QString::fromUtf8(line));
        m_socket->disconnectFromHost();
    }
}

void ImapClient::handleLoginResponse(const QByteArray& line)
{
    if (line.startsWith(m_currentTag + " OK")) {
        Logger::instance().info(QStringLiteral("imap"), QStringLiteral("LOGIN succeeded."));
        m_state = State::Authenticated;
        emit loginSuccessful();
    } else if (line.startsWith(m_currentTag + " NO") || line.startsWith(m_currentTag + " BAD")) {
        Logger::instance().error(QStringLiteral("imap"), QStringLiteral("LOGIN failed: ") + QString::fromUtf8(line));
        m_state = State::NotAuthenticated;
        emit loginFailed(QString::fromUtf8(line));
    }
}

void ImapClient::handleListResponse(const QByteArray& line)
{
    if (line.startsWith("* LIST")) {
        // Example: * LIST (\HasNoChildren) "/" "INBOX"
        // Minimal parsing for Phase 3C
        QString strLine = QString::fromUtf8(line);
        
        // Find the last quotes to get the folder name
        int lastQuoteIdx = strLine.lastIndexOf('"');
        if (lastQuoteIdx > 0) {
            int secondToLastQuoteIdx = strLine.lastIndexOf('"', lastQuoteIdx - 1);
            if (secondToLastQuoteIdx > 0) {
                QString folderName = strLine.mid(secondToLastQuoteIdx + 1, lastQuoteIdx - secondToLastQuoteIdx - 1);
                
                Folder f;
                f.name = folderName;
                
                // Extremely basic role guessing based on standard names
                QString lowerName = folderName.toLower();
                if (lowerName == "inbox") f.role = "inbox";
                else if (lowerName.contains("sent")) f.role = "sent";
                else if (lowerName.contains("trash") || lowerName.contains("bin")) f.role = "trash";
                else if (lowerName.contains("draft")) f.role = "drafts";
                else if (lowerName.contains("spam") || lowerName.contains("junk")) f.role = "spam";
                else f.role = "archive"; // Default role
                
                m_parsedFolders.append(f);
            }
        }
    } else if (line.startsWith(m_currentTag + " OK")) {
        Logger::instance().info(QStringLiteral("imap"), QStringLiteral("LIST succeeded. Folders parsed: ") + QString::number(m_parsedFolders.size()));
        m_state = State::Authenticated;
        emit foldersListed(m_parsedFolders);
    } else if (line.startsWith(m_currentTag + " NO") || line.startsWith(m_currentTag + " BAD")) {
        Logger::instance().error(QStringLiteral("imap"), QStringLiteral("LIST failed: ") + QString::fromUtf8(line));
        m_state = State::Authenticated;
        emit listFailed(QString::fromUtf8(line));
    }
}
