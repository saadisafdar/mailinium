#include "protocols/smtp/SmtpClient.h"
#include "core/Logger.h"

#include <QSslConfiguration>

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

SmtpClient::SmtpClient(const QString& accessToken, QObject* parent)
    : QObject(parent)
    , m_socket(new QSslSocket(this))
    , m_accessToken(accessToken)
{
    connect(m_socket, &QSslSocket::connected,   this, &SmtpClient::onConnected);
    connect(m_socket, &QSslSocket::encrypted,   this, &SmtpClient::onEncrypted);
    connect(m_socket, &QSslSocket::readyRead,   this, &SmtpClient::onReadyRead);
    connect(m_socket, &QAbstractSocket::errorOccurred,
            this, &SmtpClient::onSocketError);
    connect(m_socket, &QAbstractSocket::disconnected,
            this, &SmtpClient::onDisconnected);
}

SmtpClient::~SmtpClient() = default;

// ---------------------------------------------------------------------------
// Public: send
// ---------------------------------------------------------------------------

void SmtpClient::send(const QString& from,
                       const QStringList& recipients,
                       const QByteArray& rawMime)
{
    if (m_state != State::Disconnected) {
        fail(QStringLiteral("SmtpClient is already in use"));
        return;
    }
    if (from.isEmpty() || recipients.isEmpty() || rawMime.isEmpty()) {
        fail(QStringLiteral("Invalid send parameters (empty from/recipients/mime)"));
        return;
    }

    m_from       = from;
    m_recipients = recipients;
    m_rawMime    = rawMime;
    m_rcptIndex  = 0;
    m_state      = State::Greeting;

    Logger::instance().info(QStringLiteral("smtp"),
        QStringLiteral("Connecting to ") + QLatin1String(k_smtpHost)
        + QStringLiteral(":") + QString::number(k_smtpPort));

    m_socket->connectToHost(QLatin1String(k_smtpHost), k_smtpPort);
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void SmtpClient::sendCommand(const QByteArray& cmd)
{
    // Log the command but redact the AUTH token
    if (cmd.startsWith("AUTH ")) {
        Logger::instance().debug(QStringLiteral("smtp"), QStringLiteral("→ AUTH XOAUTH2 [redacted]"));
    } else {
        Logger::instance().debug(QStringLiteral("smtp"),
            QStringLiteral("→ ") + QString::fromUtf8(cmd.trimmed()));
    }
    m_socket->write(cmd + "\r\n");
}

void SmtpClient::fail(const QString& reason)
{
    m_state = State::Failed;
    Logger::instance().error(QStringLiteral("smtp"), QStringLiteral("SMTP error: ") + reason);
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }
    emit failed(reason);
}

void SmtpClient::sendEhlo()
{
    sendCommand("EHLO mailinium");
}

void SmtpClient::sendAuth()
{
    // XOAUTH2 mechanism: base64("user=<email>\001auth=Bearer <token>\001\001")
    // We use the from address as the user.
    const QByteArray sasl = "user=" + m_from.toUtf8()
                          + "\x01" + "auth=Bearer " + m_accessToken.toUtf8()
                          + "\x01\x01";
    sendCommand("AUTH XOAUTH2 " + sasl.toBase64());
}

void SmtpClient::sendMailFrom()
{
    sendCommand("MAIL FROM:<" + m_from.toUtf8() + ">");
}

void SmtpClient::sendNextRcptTo()
{
    if (m_rcptIndex >= m_recipients.size()) {
        sendDataCommand();
        return;
    }
    const QString rcpt = m_recipients.at(m_rcptIndex).trimmed();
    // Strip display-name if present ("Name <email@host>")
    QString addr = rcpt;
    const int lt = rcpt.lastIndexOf('<');
    const int gt = rcpt.lastIndexOf('>');
    if (lt != -1 && gt != -1 && gt > lt) {
        addr = rcpt.mid(lt + 1, gt - lt - 1).trimmed();
    }
    sendCommand("RCPT TO:<" + addr.toUtf8() + ">");
    ++m_rcptIndex;
}

void SmtpClient::sendDataCommand()
{
    m_state = State::Data;
    sendCommand("DATA");
}

void SmtpClient::sendRawData()
{
    m_state = State::DataSend;
    Logger::instance().debug(QStringLiteral("smtp"),
        QStringLiteral("→ [DATA: ") + QString::number(m_rawMime.size()) + QStringLiteral(" bytes]"));

    // Write data in 64KiB chunks followed by the terminating dot-crlf
    constexpr int kChunk = 65536;
    int offset = 0;
    const int total = m_rawMime.size();
    while (offset < total) {
        const int len = qMin(kChunk, total - offset);
        m_socket->write(m_rawMime.constData() + offset, len);
        offset += len;
        emit progress(static_cast<int>((offset * 100LL) / total));
    }

    // End-of-data marker (a line with only ".")
    m_socket->write("\r\n.\r\n");
}

void SmtpClient::sendQuit()
{
    m_state = State::Quit;
    sendCommand("QUIT");
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void SmtpClient::onConnected()
{
    Logger::instance().info(QStringLiteral("smtp"), QStringLiteral("TCP connected, waiting for greeting"));
    // State stays Greeting — wait for 220 from server
}

void SmtpClient::onEncrypted()
{
    Logger::instance().info(QStringLiteral("smtp"), QStringLiteral("TLS established"));
    // After STARTTLS completes, re-send EHLO
    m_state = State::EhloTls;
    sendEhlo();
}

void SmtpClient::onReadyRead()
{
    // Read all available lines
    while (m_socket->canReadLine()) {
        const QString line = QString::fromUtf8(m_socket->readLine()).trimmed();
        Logger::instance().debug(QStringLiteral("smtp"), QStringLiteral("← ") + line);
        processServerResponse(line);
        if (m_state == State::Failed || m_state == State::Done) {
            return;
        }
    }
}

void SmtpClient::processServerResponse(const QString& line)
{
    if (line.size() < 3) {
        fail(QStringLiteral("Malformed server response: ") + line);
        return;
    }

    const int code    = line.left(3).toInt();
    const bool isFinal = (line.size() < 4 || line[3] != '-'); // continuation lines have '-'

    if (!isFinal) {
        // Multi-line response continuation — we only act on the final line
        return;
    }

    switch (m_state) {
    case State::Greeting:
        if (code == 220) {
            m_state = State::Ehlo;
            sendEhlo();
        } else {
            fail(QStringLiteral("Expected 220 greeting, got: ") + line);
        }
        break;

    case State::Ehlo:
        if (code == 250) {
            m_state = State::StartTls;
            sendCommand("STARTTLS");
        } else {
            fail(QStringLiteral("EHLO failed: ") + line);
        }
        break;

    case State::StartTls:
        if (code == 220) {
            // Upgrade to TLS — onEncrypted will fire on success
            m_socket->startClientEncryption();
        } else {
            fail(QStringLiteral("STARTTLS rejected: ") + line);
        }
        break;

    case State::EhloTls:
        if (code == 250) {
            m_state = State::Auth;
            sendAuth();
        } else {
            fail(QStringLiteral("EHLO (post-TLS) failed: ") + line);
        }
        break;

    case State::Auth:
        if (code == 235) {
            m_state = State::MailFrom;
            sendMailFrom();
        } else {
            fail(QStringLiteral("AUTH XOAUTH2 rejected: ") + line);
        }
        break;

    case State::MailFrom:
        if (code == 250) {
            m_state = State::RcptTo;
            sendNextRcptTo();
        } else {
            fail(QStringLiteral("MAIL FROM rejected: ") + line);
        }
        break;

    case State::RcptTo:
        if (code == 250) {
            sendNextRcptTo(); // sends next, or transitions to DATA
        } else {
            fail(QStringLiteral("RCPT TO rejected: ") + line);
        }
        break;

    case State::Data:
        if (code == 354) {
            sendRawData();
        } else {
            fail(QStringLiteral("DATA command rejected: ") + line);
        }
        break;

    case State::DataSend:
        if (code == 250) {
            sendQuit();
        } else {
            fail(QStringLiteral("Message submission rejected: ") + line);
        }
        break;

    case State::Quit:
        // 221 = bye; we don't care about error codes here, just disconnect
        m_socket->disconnectFromHost();
        break;

    default:
        break;
    }
}

void SmtpClient::onSocketError(QAbstractSocket::SocketError err)
{
    if (m_state == State::Done || m_state == State::Failed) return;
    fail(QStringLiteral("Socket error %1: ").arg(static_cast<int>(err))
         + m_socket->errorString());
}

void SmtpClient::onDisconnected()
{
    if (m_state == State::Quit || m_state == State::Done) {
        m_state = State::Done;
        Logger::instance().info(QStringLiteral("smtp"), QStringLiteral("Message sent successfully"));
        emit succeeded();
    } else if (m_state != State::Failed) {
        fail(QStringLiteral("Disconnected unexpectedly during SMTP session"));
    }
}
