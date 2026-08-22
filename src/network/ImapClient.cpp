#include "network/ImapClient.h"
#include "core/Logger.h"
#include "storage/Folder.h"
#include "storage/Message.h"
#include "core/Settings.h"
#include "protocols/MimeParser.h"
#include <QTimeZone>
#include <QLocale>
#include <QRegularExpression>
#include <QDateTime>

#include <QSslError>
#include <QStringList>

ImapClient::ImapClient(QObject* parent)
    : QObject(parent)
{
    m_socket = new QSslSocket(this);
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);

    connect(m_socket, &QSslSocket::connected, this, &ImapClient::onSocketConnected);
    connect(m_socket, &QSslSocket::encrypted, this, [this]() {
        Logger::instance().info(QStringLiteral("imap"), QStringLiteral("SSL handshake completed (encrypted)."));
    });
    connect(m_socket, &QSslSocket::disconnected, this, &ImapClient::onSocketDisconnected);
    connect(m_socket, &QSslSocket::errorOccurred, this, &ImapClient::onSocketError);
    connect(m_socket, &QSslSocket::sslErrors, this, &ImapClient::onSslErrors);
    connect(m_socket, &QSslSocket::readyRead, this, &ImapClient::onReadyRead);
    connect(m_timeoutTimer, &QTimer::timeout, this, &ImapClient::onCommandTimeout);
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
    if (host == QStringLiteral("127.0.0.1") || host == QStringLiteral("localhost")) {
        m_socket->connectToHost(host, port);
    } else {
        m_socket->connectToHostEncrypted(host, port);
    }
}

void ImapClient::disconnectFromServer()
{
    m_state = State::Disconnected;
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->disconnectFromHost();
    }
    clearParserState();
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
    writeCommand(cmd.toUtf8());
}

void ImapClient::loginOAuth2(const QString& username, const QString& accessToken)
{
    if (m_state != State::NotAuthenticated) {
        emit loginFailed(QStringLiteral("Not connected or already authenticated."));
        return;
    }

    m_state = State::Authenticating;
    m_currentTag = generateTag().toUtf8();

    // XOAUTH2 payload: base64("user=" + username + "^Aauth=Bearer " + accessToken + "^A^A")
    // where ^A is \x01
    QString payloadStr = QStringLiteral("user=") + username + QStringLiteral("\x01") +
                         QStringLiteral("auth=Bearer ") + accessToken + QStringLiteral("\x01\x01");

    QByteArray payloadBase64 = payloadStr.toUtf8().toBase64();

    QString cmd = QStringLiteral("%1 AUTHENTICATE XOAUTH2 %2\r\n")
                  .arg(QString::fromUtf8(m_currentTag))
                  .arg(QString::fromUtf8(payloadBase64));

    Logger::instance().debug(QStringLiteral("imap"), QStringLiteral("Sending AUTHENTICATE XOAUTH2 for ") + username);
    writeCommand(cmd.toUtf8());
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
    writeCommand(cmd);
}

void ImapClient::selectFolder(const QString& folderName)
{
    if (m_state == State::Disconnected) return;
    m_state = State::SelectingFolder;
    m_expectedMessageCount = 0;
    m_uidvalidity = 0;
    m_uidnext = 0;
    m_currentTag = generateTag().toUtf8();

    // Escape quotes in folder name
    QString safeFolder = folderName;
    safeFolder.replace("\"", "\\\"");

    QByteArray cmd = m_currentTag + " SELECT \"" + safeFolder.toUtf8() + "\"\r\n";
    Logger::instance().debug(QStringLiteral("imap_pipeline"), QStringLiteral("[PIPELINE] 1. Selected folder: ") + safeFolder);
    writeCommand(cmd);
}

void ImapClient::searchAllUids()
{
    if (m_state == State::Disconnected) return;
    m_state = State::SearchingUids;
    m_parsedUids.clear();
    m_currentTag = generateTag().toUtf8();
    QByteArray cmd = m_currentTag + " UID SEARCH ALL\r\n";
    Logger::instance().debug(QStringLiteral("imap_pipeline"), QStringLiteral("[PIPELINE] 2. SEARCH command sent: ") + cmd.trimmed());
    writeCommand(cmd);
}

void ImapClient::fetchMessageHeaders(const QList<qint64>& uids)
{
    if (uids.isEmpty()) {
        emit headersFetched(QList<Message>());
        return;
    }
    m_state = State::FetchingHeaders;
    m_parsedMessages.clear();

    // Chunk UIDs according to SyncFetchLimit setting to avoid "Command line too long"
    int limit = Settings::instance().intValue(Settings::Key::SyncFetchLimit);
    if (limit <= 0) limit = 100;
    QList<qint64> chunk = uids.mid(0, limit);

    QStringList uidStrs;
    for (qint64 u : chunk) uidStrs << QString::number(u);
    QString sequence = uidStrs.join(",");

    m_currentTag = generateTag().toUtf8();
    QByteArray cmd = m_currentTag + " UID FETCH " + sequence.toUtf8() + " (UID ENVELOPE INTERNALDATE RFC822.SIZE FLAGS)\r\n";
    Logger::instance().debug(QStringLiteral("imap_pipeline"), QStringLiteral("[PIPELINE] 5. FETCH command sent: ") + cmd.trimmed());
    writeCommand(cmd);
}

void ImapClient::fetchMessageBody(qint64 uid)
{
    m_state = State::FetchingBody;
    m_currentFetchUid = uid;
    m_currentTag = generateTag().toUtf8();
    QByteArray cmd = m_currentTag + " UID FETCH " + QByteArray::number(uid) + " (BODY.PEEK[])\r\n";
    Logger::instance().debug(QStringLiteral("imap"), QStringLiteral("Sending UID FETCH body command for UID ") + QString::number(uid));
    writeCommand(cmd);
}

QString ImapClient::generateTag()
{
    ++m_tagCounter;
    return QStringLiteral("A%1").arg(m_tagCounter, 4, 10, QLatin1Char('0'));
}

void ImapClient::clearParserState()
{
    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
    }
    m_buffer.clear();
    m_currentLogicalLine.clear();
    m_inLiteralData = false;
    m_literalBytesExpected = 0;
    m_expectedMessageCount = 0;
    m_parsedUids.clear();
    m_parsedMessages.clear();
    m_parsedFlags.clear();
    m_currentFetchUid = 0;
    m_capabilities.clear();
    m_moveFallbackStep = MoveFallbackStep::None;
    m_pendingMoveUids.clear();
    m_pendingMoveDest.clear();
}

void ImapClient::fetchMessageFlags(const QList<qint64>& uids)
{
    if (uids.isEmpty()) {
        emit flagsFetched({});
        return;
    }
    m_state = State::FetchingFlags;
    m_parsedFlags.clear();

    // Chunking to avoid command line limits
    int limit = Settings::instance().intValue(Settings::Key::SyncFetchLimit);
    if (limit <= 0) limit = 100;
    QList<qint64> chunk = uids.mid(0, limit);

    QStringList uidStrs;
    for (qint64 u : chunk) uidStrs << QString::number(u);
    QString sequence = uidStrs.join(",");

    m_currentTag = generateTag().toUtf8();
    QByteArray cmd = m_currentTag + " UID FETCH " + sequence.toUtf8() + " (UID FLAGS)\r\n";
    Logger::instance().debug(QStringLiteral("imap"), QStringLiteral("Sending FETCH flags command for UIDs: ") + sequence);
    writeCommand(cmd);
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
    clearParserState();
    emit disconnected();
}

void ImapClient::onSocketError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError)
    QString errorMsg = m_socket->errorString();
    Logger::instance().error(QStringLiteral("imap"), QStringLiteral("Socket error: ") + errorMsg);

    bool wasConnected = (m_state != State::Disconnected);
    m_state = State::Disconnected;
    if (wasConnected) {
        emit errorOccurred(errorMsg);
    }
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
    m_buffer.append(m_socket->readAll());

    while (!m_buffer.isEmpty()) {
        if (m_inLiteralData) {
            if (m_buffer.size() >= m_literalBytesExpected) {
                m_currentLogicalLine.append(m_buffer.left(m_literalBytesExpected));
                m_buffer.remove(0, m_literalBytesExpected);
                m_inLiteralData = false;
            } else {
                break; // Need more data for the literal
            }
        } else {
            int crlfIdx = m_buffer.indexOf("\r\n");
            if (crlfIdx == -1) break; // Need more data for a full line

            QByteArray line = m_buffer.left(crlfIdx);
            m_buffer.remove(0, crlfIdx + 2);

            if (line.endsWith('}')) {
                int braceIdx = line.lastIndexOf('{');
                if (braceIdx != -1) {
                    bool ok;
                    int size = line.mid(braceIdx + 1, line.length() - braceIdx - 2).toInt(&ok);
                    if (ok) {
                        m_literalBytesExpected = size;
                        m_inLiteralData = true;

                        m_currentLogicalLine.append(line.left(braceIdx));
                        continue;
                    }
                }
            }

            m_currentLogicalLine.append(line);

            processLine(m_currentLogicalLine);
            m_currentLogicalLine.clear();
        }
    }
}

void ImapClient::processLine(const QByteArray& line)
{
    if (!m_currentTag.isEmpty() && (line.startsWith(m_currentTag + " OK") ||
                                    line.startsWith(m_currentTag + " NO") ||
                                    line.startsWith(m_currentTag + " BAD"))) {
        if (m_timeoutTimer) {
            m_timeoutTimer->stop();
        }
    }

    if (line.startsWith("* CAPABILITY ")) {
        QString capLine = QString::fromUtf8(line).mid(13).trimmed();
        m_capabilities = capLine.toUpper().split(' ', Qt::SkipEmptyParts);
        Logger::instance().info(QStringLiteral("imap"), QStringLiteral("Server capabilities: ") + capLine);
    }

    if (m_state == State::SearchingUids && line.startsWith("* SEARCH")) {
        Logger::instance().debug(QStringLiteral("imap_pipeline"), QStringLiteral("[PIPELINE] 3. Raw SEARCH response: ") + QString::fromUtf8(line));
    } else {
        Logger::instance().debug(QStringLiteral("imap"), QStringLiteral("S: ") + QString::fromUtf8(line));
    }

    if (m_state == State::Greeting) {
        handleGreeting(line);
    } else if (m_state == State::Authenticating) {
        handleLoginResponse(line);
    } else if (m_state == State::ListingFolders) {
        handleListResponse(line);
    } else if (m_state == State::SelectingFolder) {
        handleSelectResponse(line);
    } else if (m_state == State::SearchingUids) {
        handleSearchResponse(line);
    } else if (m_state == State::FetchingHeaders || m_state == State::FetchingBody || m_state == State::FetchingFlags) {
        handleFetchResponse(line);
    } else if (m_state == State::StoringFlags) {
        handleStoreResponse(line);
    } else if (m_state == State::CopyingMessages) {
        handleCopyResponse(line);
    } else if (m_state == State::MovingMessages) {
        handleMoveResponse(line);
    } else if (m_state == State::Expunging) {
        handleExpungeResponse(line);
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

                if (strLine.contains(QStringLiteral("\\Noselect"), Qt::CaseInsensitive)) {
                    f.role = QStringLiteral("noselect");
                } else if (lowerName == "inbox") f.role = "inbox";
                else if (lowerName.contains("sent")) f.role = "sent";
                else if (lowerName.contains("trash") || lowerName.contains("bin")) f.role = "trash";
                else if (lowerName.contains("draft")) f.role = "drafts";
                else if (lowerName.contains("spam") || lowerName.contains("junk")) f.role = "spam";
                else if (lowerName.contains("archive") || lowerName.contains("all mail")) f.role = "archive";
                else f.role = QStringLiteral(""); // custom folder

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

void ImapClient::handleSelectResponse(const QByteArray& line)
{
    if (line.startsWith("* ") && line.endsWith(" EXISTS")) {
        int spaceIdx = line.indexOf(' ', 2);
        if (spaceIdx != -1) {
            bool ok;
            int count = line.mid(2, spaceIdx - 2).toInt(&ok);
            if (ok) m_expectedMessageCount = count;
        }
        Logger::instance().debug(QStringLiteral("imap"), QStringLiteral("SELECT found EXISTS: ") + QString::number(m_expectedMessageCount));
    } else if (line.startsWith("* OK [UIDVALIDITY ")) {
        int start = 18; // "* OK [UIDVALIDITY "
        int end = line.indexOf(']', start);
        if (end != -1) {
            bool ok;
            qint64 uv = line.mid(start, end - start).toLongLong(&ok);
            if (ok) m_uidvalidity = uv;
        }
    } else if (line.startsWith("* OK [UIDNEXT ")) {
        int start = 14; // "* OK [UIDNEXT "
        int end = line.indexOf(']', start);
        if (end != -1) {
            bool ok;
            qint64 un = line.mid(start, end - start).toLongLong(&ok);
            if (ok) m_uidnext = un;
        }
    } else if (line.startsWith(m_currentTag + " OK")) {
        Logger::instance().info(QStringLiteral("imap"), QStringLiteral("SELECT succeeded. Messages: ") + QString::number(m_expectedMessageCount)
            + ", UIDVALIDITY: " + QString::number(m_uidvalidity) + ", UIDNEXT: " + QString::number(m_uidnext));
        m_state = State::Authenticated;
        emit folderSelected(m_expectedMessageCount, m_uidvalidity, m_uidnext);
    } else if (line.startsWith(m_currentTag + " NO") || line.startsWith(m_currentTag + " BAD")) {
        Logger::instance().error(QStringLiteral("imap"), QStringLiteral("SELECT failed: ") + QString::fromUtf8(line));
        m_state = State::Authenticated;
        emit errorOccurred(QString::fromUtf8(line));
    }
}

void ImapClient::handleSearchResponse(const QByteArray& line)
{
    if (line.startsWith("* SEARCH")) {
        QList<QByteArray> parts = line.mid(9).split(' ');
        for (const QByteArray& part : parts) {
            if (part.isEmpty()) continue;
            bool ok;
            qint64 uid = part.toLongLong(&ok);
            if (ok) m_parsedUids.append(uid);
        }
    } else if (line.startsWith(m_currentTag + " OK")) {
        Logger::instance().debug(QStringLiteral("imap_pipeline"), QStringLiteral("[PIPELINE] 4. Number of UIDs returned: ") + QString::number(m_parsedUids.size()));
        if (m_parsedUids.isEmpty()) {
            Logger::instance().error(QStringLiteral("imap_pipeline"), QStringLiteral("[PIPELINE] STOP: 0 UIDs returned from SEARCH!"));
        }
        m_state = State::Authenticated;
        emit uidsFound(m_parsedUids);
    } else if (line.startsWith(m_currentTag + " NO") || line.startsWith(m_currentTag + " BAD")) {
        Logger::instance().error(QStringLiteral("imap_pipeline"), QStringLiteral("[PIPELINE] SEARCH failed: ") + QString::fromUtf8(line));
        m_state = State::Authenticated;
        emit errorOccurred(QString::fromUtf8(line));
    }
}

enum class TokenType {
    Open,
    Close,
    String,
    Nil,
    Atom
};

struct Token {
    TokenType type;
    QString value;
};

static QList<Token> tokenizeSExpr(const QString& str, int startIdx) {
    QList<Token> tokens;
    int i = startIdx;
    int len = str.length();
    while (i < len) {
        QChar c = str.at(i);
        if (c.isSpace()) {
            i++;
            continue;
        }
        if (c == '(') {
            tokens.append({TokenType::Open, QStringLiteral("(")});
            i++;
        } else if (c == ')') {
            tokens.append({TokenType::Close, QStringLiteral(")")});
            i++;
        } else if (c == '"') {
            // Read quoted string
            QString s;
            i++; // skip open quote
            while (i < len) {
                QChar qc = str.at(i);
                if (qc == '"') {
                    i++; // skip close quote
                    break;
                } else if (qc == '\\' && i + 1 < len) {
                    s.append(str.at(i + 1));
                    i += 2;
                } else {
                    s.append(qc);
                    i++;
                }
            }
            tokens.append({TokenType::String, s});
        } else {
            // Read atom (NIL, 123, etc.)
            int start = i;
            while (i < len) {
                QChar qc = str.at(i);
                if (qc.isSpace() || qc == '(' || qc == ')' || qc == '"') {
                    break;
                }
                i++;
            }
            QString atom = str.mid(start, i - start);
            if (atom.toUpper() == QStringLiteral("NIL")) {
                tokens.append({TokenType::Nil, QString()});
            } else {
                tokens.append({TokenType::Atom, atom});
            }
        }
    }
    return tokens;
}

struct SNode {
    TokenType type = TokenType::Nil;
    QString value; // if String, Atom
    QList<SNode> children; // if Open (list)
};

static SNode parseSExpr(const QList<Token>& tokens, int& index) {
    SNode node;
    if (index >= tokens.size()) {
        node.type = TokenType::Nil;
        return node;
    }
    const Token& t = tokens.at(index);
    if (t.type == TokenType::Open) {
        node.type = TokenType::Open;
        index++; // consume '('
        while (index < tokens.size() && tokens.at(index).type != TokenType::Close) {
            node.children.append(parseSExpr(tokens, index));
        }
        if (index < tokens.size()) {
            index++; // consume ')'
        }
    } else {
        node.type = t.type;
        node.value = t.value;
        index++;
    }
    return node;
}

static QString formatAddressList(const SNode& listNode) {
    if (listNode.type != TokenType::Open) return QString();
    QStringList parts;
    for (const SNode& addr : listNode.children) {
        if (addr.type == TokenType::Open && addr.children.size() >= 4) {
            QString name = addr.children[0].value;
            QString mailbox = addr.children[2].value;
            QString host = addr.children[3].value;
            if (mailbox.isEmpty()) continue;

            QString email = mailbox;
            if (!host.isEmpty()) {
                email += QStringLiteral("@") + host;
            }
            // Decode display name if encoded using RFC 2047
            name = MimeParser::decodeHeaderValue(name);

            if (!name.isEmpty()) {
                parts.append(QStringLiteral("\"%1\" <%2>").arg(name, email));
            } else {
                parts.append(email);
            }
        }
    }
    return parts.join(QStringLiteral(", "));
}

static void extractFromAddress(const SNode& fromList, QString& fromName, QString& fromAddress) {
    if (fromList.type != TokenType::Open || fromList.children.isEmpty()) return;
    const SNode& addr = fromList.children.first();
    if (addr.type == TokenType::Open && addr.children.size() >= 4) {
        fromName = MimeParser::decodeHeaderValue(addr.children[0].value);
        QString mailbox = addr.children[2].value;
        QString host = addr.children[3].value;
        if (!mailbox.isEmpty()) {
            fromAddress = mailbox;
            if (!host.isEmpty()) {
                fromAddress += QStringLiteral("@") + host;
            }
        }
    }
}

static QDateTime parseRfc2822Date(const QString& str) {
    QString cleaned = str.simplified();
    QDateTime dt = QDateTime::fromString(cleaned, Qt::RFC2822Date);
    if (!dt.isValid()) {
        dt = QDateTime::currentDateTimeUtc();
    } else {
        dt = dt.toTimeZone(QTimeZone::UTC);
    }
    return dt;
}

void ImapClient::handleFetchResponse(const QByteArray& line)
{
    if (line.startsWith("* ") && line.contains(" FETCH ")) {
        if (m_state == State::FetchingHeaders) {
            Message msg;

            int uidIdx = line.indexOf("UID ");
            if (uidIdx != -1) {
                int uidEndIdx = line.indexOf(' ', uidIdx + 4);
                if (uidEndIdx == -1) uidEndIdx = line.indexOf(')', uidIdx + 4);
                if (uidEndIdx != -1) {
                    msg.uid = line.mid(uidIdx + 4, uidEndIdx - uidIdx - 4).toLongLong();
                }
            }

            int sizeIdx = line.indexOf("RFC822.SIZE ");
            if (sizeIdx != -1) {
                int sizeEndIdx = line.indexOf(' ', sizeIdx + 12);
                if (sizeEndIdx == -1) sizeEndIdx = line.indexOf(')', sizeIdx + 12);
                if (sizeEndIdx != -1) {
                    msg.size = line.mid(sizeIdx + 12, sizeEndIdx - sizeIdx - 12).toLongLong();
                }
            }

            // Parse FLAGS
            int flagsIdx = line.indexOf("FLAGS (");
            if (flagsIdx != -1) {
                int flagsEndIdx = line.indexOf(')', flagsIdx + 7);
                if (flagsEndIdx != -1) {
                    QByteArray flagsStr = line.mid(flagsIdx + 7, flagsEndIdx - flagsIdx - 7);
                    msg.flags = 0;
                    if (flagsStr.contains("\\Seen")) msg.flags |= MessageFlags::Seen;
                    if (flagsStr.contains("\\Flagged")) msg.flags |= MessageFlags::Flagged;
                    if (flagsStr.contains("\\Answered")) msg.flags |= MessageFlags::Answered;
                    if (flagsStr.contains("\\Deleted")) msg.flags |= MessageFlags::Deleted;
                    if (flagsStr.contains("\\Draft")) msg.flags |= MessageFlags::Draft;
                }
            }

            int envIdx = line.indexOf("ENVELOPE (");
            if (envIdx != -1) {
                QString envStr = QString::fromUtf8(line.mid(envIdx + 9));
                QList<Token> tokens = tokenizeSExpr(envStr, 0);
                int tokensIndex = 0;
                SNode root = parseSExpr(tokens, tokensIndex);
                if (root.type == TokenType::Open && root.children.size() >= 10) {
                    msg.date = parseRfc2822Date(root.children[0].value);
                    msg.subject = MimeParser::decodeHeaderValue(root.children[1].value);
                    extractFromAddress(root.children[2], msg.fromName, msg.fromAddress);
                    msg.replyTo = formatAddressList(root.children[4]);
                    msg.toList = formatAddressList(root.children[5]);
                    msg.ccList = formatAddressList(root.children[6]);
                    msg.bccList = formatAddressList(root.children[7]);
                    msg.messageId = root.children[9].value;
                }
            }

            if (msg.uid > 0) {
                Logger::instance().debug(QStringLiteral("imap"), QStringLiteral("FETCH parsed header UID: ") + QString::number(msg.uid) + ", FLAGS: " + QString::number(msg.flags));
                m_parsedMessages.append(msg);
            } else {
                Logger::instance().debug(QStringLiteral("imap"), QStringLiteral("FETCH parsed header UID failed, UID was 0. Line: ") + QString::fromUtf8(line));
            }
        } else if (m_state == State::FetchingFlags) {
            qint64 uid = 0;
            int flags = 0;

            int uidIdx = line.indexOf("UID ");
            if (uidIdx != -1) {
                int uidEndIdx = line.indexOf(' ', uidIdx + 4);
                if (uidEndIdx == -1) uidEndIdx = line.indexOf(')', uidIdx + 4);
                if (uidEndIdx != -1) {
                    uid = line.mid(uidIdx + 4, uidEndIdx - uidIdx - 4).toLongLong();
                }
            }

            int flagsIdx = line.indexOf("FLAGS (");
            if (flagsIdx != -1) {
                int flagsEndIdx = line.indexOf(')', flagsIdx + 7);
                if (flagsEndIdx != -1) {
                    QByteArray flagsStr = line.mid(flagsIdx + 7, flagsEndIdx - flagsIdx - 7);
                    if (flagsStr.contains("\\Seen")) flags |= MessageFlags::Seen;
                    if (flagsStr.contains("\\Flagged")) flags |= MessageFlags::Flagged;
                    if (flagsStr.contains("\\Answered")) flags |= MessageFlags::Answered;
                    if (flagsStr.contains("\\Deleted")) flags |= MessageFlags::Deleted;
                    if (flagsStr.contains("\\Draft")) flags |= MessageFlags::Draft;
                }
            }

            if (uid > 0) {
                m_parsedFlags.insert(uid, flags);
            }
        } else if (m_state == State::FetchingBody) {
            int bodyIdx = line.indexOf("BODY");
            if (bodyIdx != -1) {
                int bracketEnd = line.indexOf(']', bodyIdx);
                if (bracketEnd != -1) {
                    QString bodyText = QString::fromUtf8(line.mid(bracketEnd + 2));
                    if (bodyText.endsWith(')')) {
                        bodyText.chop(1);
                    }
                    Logger::instance().debug(QStringLiteral("imap"), QStringLiteral("FETCH parsed body for UID: ") + QString::number(m_currentFetchUid));
                    emit bodyFetched(m_currentFetchUid, bodyText);
                }
            }
        }
    } else if (line.startsWith(m_currentTag + " OK")) {
        if (m_state == State::FetchingHeaders) {
            Logger::instance().debug(QStringLiteral("imap_pipeline"), QStringLiteral("[PIPELINE] 6. Number of headers parsed: ") + QString::number(m_parsedMessages.size()));
            if (m_parsedMessages.isEmpty()) {
                Logger::instance().error(QStringLiteral("imap_pipeline"), QStringLiteral("[PIPELINE] STOP: 0 headers parsed!"));
            }
            m_state = State::Authenticated;
            emit headersFetched(m_parsedMessages);
        } else if (m_state == State::FetchingFlags) {
            Logger::instance().debug(QStringLiteral("imap"), QStringLiteral("FETCH FLAGS succeeded. Parsed count: ") + QString::number(m_parsedFlags.size()));
            m_state = State::Authenticated;
            emit flagsFetched(m_parsedFlags);
        } else {
            Logger::instance().info(QStringLiteral("imap"), QStringLiteral("FETCH BODY succeeded."));
            m_state = State::Authenticated;
        }
    } else if (line.startsWith(m_currentTag + " NO") || line.startsWith(m_currentTag + " BAD")) {
        Logger::instance().error(QStringLiteral("imap"), QStringLiteral("FETCH failed: ") + QString::fromUtf8(line));
        m_state = State::Authenticated;
        emit errorOccurred(QString::fromUtf8(line));
    }
}

void ImapClient::storeFlags(const QList<qint64>& uids, const QString& flags, bool add)
{
    if (uids.isEmpty()) {
        emit flagsStored(uids);
        return;
    }
    m_state = State::StoringFlags;
    m_parsedUids = uids;
    m_currentTag = generateTag().toUtf8();
    QStringList uidStrs;
    for (qint64 u : uids) uidStrs << QString::number(u);
    QString sequence = uidStrs.join(",");
    QString sign = add ? QStringLiteral("+") : QStringLiteral("-");
    QByteArray cmd = m_currentTag + " UID STORE " + sequence.toUtf8() + " " + sign.toUtf8() + "FLAGS (" + flags.toUtf8() + ")\r\n";
    Logger::instance().debug(QStringLiteral("imap"), QStringLiteral("Sending STORE: ") + cmd.trimmed());
    writeCommand(cmd);
}

void ImapClient::copyMessages(const QList<qint64>& uids, const QString& destFolder)
{
    if (uids.isEmpty()) {
        emit messagesCopied(uids);
        return;
    }
    m_state = State::CopyingMessages;
    m_parsedUids = uids;
    m_currentTag = generateTag().toUtf8();
    QStringList uidStrs;
    for (qint64 u : uids) uidStrs << QString::number(u);
    QString sequence = uidStrs.join(",");
    QString safeDest = destFolder;
    safeDest.replace("\"", "\\\"");
    QByteArray cmd = m_currentTag + " UID COPY " + sequence.toUtf8() + " \"" + safeDest.toUtf8() + "\"\r\n";
    Logger::instance().debug(QStringLiteral("imap"), QStringLiteral("Sending COPY: ") + cmd.trimmed());
    writeCommand(cmd);
}

void ImapClient::moveMessages(const QList<qint64>& uids, const QString& destFolder)
{
    if (uids.isEmpty()) {
        emit messagesMoved(uids);
        return;
    }

    if (m_capabilities.contains(QStringLiteral("MOVE"))) {
        m_state = State::MovingMessages;
        m_parsedUids = uids;
        m_moveFallbackStep = MoveFallbackStep::None;
        m_currentTag = generateTag().toUtf8();
        QStringList uidStrs;
        for (qint64 u : uids) uidStrs << QString::number(u);
        QString sequence = uidStrs.join(",");
        QString safeDest = destFolder;
        safeDest.replace("\"", "\\\"");
        QByteArray cmd = m_currentTag + " UID MOVE " + sequence.toUtf8() + " \"" + safeDest.toUtf8() + "\"\r\n";
        Logger::instance().debug(QStringLiteral("imap"), QStringLiteral("Sending MOVE: ") + cmd.trimmed());
        writeCommand(cmd);
    } else {
        Logger::instance().info(QStringLiteral("imap"), QStringLiteral("MOVE capability not supported. Using fallback COPY -> STORE +Deleted -> EXPUNGE"));
        m_moveFallbackStep = MoveFallbackStep::Copy;
        m_pendingMoveUids = uids;
        m_pendingMoveDest = destFolder;

        copyMessages(uids, destFolder);
    }
}

void ImapClient::expunge()
{
    m_state = State::Expunging;
    m_currentTag = generateTag().toUtf8();
    QByteArray cmd = m_currentTag + " EXPUNGE\r\n";
    Logger::instance().debug(QStringLiteral("imap"), QStringLiteral("Sending EXPUNGE"));
    writeCommand(cmd);
}

void ImapClient::handleStoreResponse(const QByteArray& line)
{
    if (line.startsWith(m_currentTag + " OK")) {
        Logger::instance().info(QStringLiteral("imap"), QStringLiteral("STORE succeeded."));
        if (m_moveFallbackStep == MoveFallbackStep::StoreDeleted) {
            m_moveFallbackStep = MoveFallbackStep::Expunge;
            expunge();
        } else {
            m_state = State::Authenticated;
            emit flagsStored(m_parsedUids);
        }
    } else if (line.startsWith(m_currentTag + " NO") || line.startsWith(m_currentTag + " BAD")) {
        Logger::instance().error(QStringLiteral("imap"), QStringLiteral("STORE failed: ") + QString::fromUtf8(line));
        m_state = State::Authenticated;
        m_moveFallbackStep = MoveFallbackStep::None;
        emit errorOccurred(QString::fromUtf8(line));
    }
}

void ImapClient::handleCopyResponse(const QByteArray& line)
{
    if (line.startsWith(m_currentTag + " OK")) {
        Logger::instance().info(QStringLiteral("imap"), QStringLiteral("COPY succeeded."));
        if (m_moveFallbackStep == MoveFallbackStep::Copy) {
            m_moveFallbackStep = MoveFallbackStep::StoreDeleted;
            storeFlags(m_pendingMoveUids, QStringLiteral("\\Deleted"), true);
        } else {
            m_state = State::Authenticated;
            emit messagesCopied(m_parsedUids);
        }
    } else if (line.startsWith(m_currentTag + " NO") || line.startsWith(m_currentTag + " BAD")) {
        Logger::instance().error(QStringLiteral("imap"), QStringLiteral("COPY failed: ") + QString::fromUtf8(line));
        m_state = State::Authenticated;
        m_moveFallbackStep = MoveFallbackStep::None;
        emit errorOccurred(QString::fromUtf8(line));
    }
}

void ImapClient::handleMoveResponse(const QByteArray& line)
{
    if (line.startsWith(m_currentTag + " OK")) {
        Logger::instance().info(QStringLiteral("imap"), QStringLiteral("MOVE succeeded."));
        m_state = State::Authenticated;
        emit messagesMoved(m_parsedUids);
    } else if (line.startsWith(m_currentTag + " NO") || line.startsWith(m_currentTag + " BAD")) {
        Logger::instance().error(QStringLiteral("imap"), QStringLiteral("MOVE failed: ") + QString::fromUtf8(line));
        m_state = State::Authenticated;
        emit errorOccurred(QString::fromUtf8(line));
    }
}

void ImapClient::handleExpungeResponse(const QByteArray& line)
{
    if (line.startsWith(m_currentTag + " OK")) {
        Logger::instance().info(QStringLiteral("imap"), QStringLiteral("EXPUNGE succeeded."));
        m_state = State::Authenticated;
        if (m_moveFallbackStep == MoveFallbackStep::Expunge) {
            m_moveFallbackStep = MoveFallbackStep::None;
            emit messagesMoved(m_pendingMoveUids);
        } else {
            emit expunged();
        }
    } else if (line.startsWith(m_currentTag + " NO") || line.startsWith(m_currentTag + " BAD")) {
        Logger::instance().error(QStringLiteral("imap"), QStringLiteral("EXPUNGE failed: ") + QString::fromUtf8(line));
        m_state = State::Authenticated;
        m_moveFallbackStep = MoveFallbackStep::None;
        emit errorOccurred(QString::fromUtf8(line));
    }
}

void ImapClient::writeCommand(const QByteArray& cmd)
{
    m_socket->write(cmd);
    if (m_timeoutTimer) {
        m_timeoutTimer->start(30000); // 30 seconds
    }
}

void ImapClient::onCommandTimeout()
{
    Logger::instance().warning(QStringLiteral("imap"), QStringLiteral("Command timed out. Tag: ") + QString::fromUtf8(m_currentTag));
    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
    }
    m_socket->disconnectFromHost();
    emit errorOccurred(QStringLiteral("Command timed out"));
}
