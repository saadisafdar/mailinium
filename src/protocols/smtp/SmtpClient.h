#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QSslSocket>
#include <QByteArray>
#include <QStringList>

//
// SmtpClient — non-blocking SMTP client implementing the Gmail SMTP flow.
//
// State machine:
//   Disconnected → Greeting → Ehlo → StartTls → EhloTls → Auth → Ready
//   → Sending → Done / Failed
//
// The SMTP conversation is:
//   [connect to smtp.gmail.com:587]
//   ← 220 smtp.gmail.com ESMTP greeting
//   → EHLO mailinium
//   ← 250 capabilities (must include STARTTLS)
//   → STARTTLS
//   ← 220 ready
//   [upgrade socket to TLS]
//   → EHLO mailinium   (re-send after TLS upgrade)
//   ← 250 capabilities
//   → AUTH XOAUTH2 <base64(user=X\001auth=Bearer Y\001\001)>
//   ← 235 Authentication successful
//   → MAIL FROM:<sender>
//   ← 250
//   → RCPT TO:<recipient1>
//   ← 250
//   → DATA
//   ← 354 send data
//   → [raw MIME bytes]\r\n.\r\n
//   ← 250 OK
//   → QUIT
//   ← 221
//   [disconnect]
//
// Usage:
//   SmtpClient* client = new SmtpClient(account, parent);
//   connect(client, &SmtpClient::succeeded, ...);
//   connect(client, &SmtpClient::failed, ...);
//   connect(client, &SmtpClient::progress, ...);
//   client->send(from, recipients, rawMime);
//
class SmtpClient : public QObject
{
    Q_OBJECT

public:
    explicit SmtpClient(const QString& accessToken, QObject* parent = nullptr);
    ~SmtpClient() override;

    // Begin the SMTP session and send the message.
    //   from        — envelope sender (MAIL FROM address, no angle brackets)
    //   recipients  — all envelope recipients (To + Cc + Bcc, no angle brackets)
    //   rawMime     — complete MIME message bytes (built by MimeBuilder)
    void send(const QString& from,
              const QStringList& recipients,
              const QByteArray& rawMime);

signals:
    // Emitted periodically during DATA upload (0–100)
    void progress(int percent);

    // Emitted on success
    void succeeded();

    // Emitted on any failure; the connection is closed before this fires
    void failed(const QString& errorMessage);

private slots:
    void onConnected();
    void onEncrypted();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError err);
    void onDisconnected();

private:
    enum class State {
        Disconnected,
        Greeting,
        Ehlo,
        StartTls,
        EhloTls,
        Auth,
        MailFrom,
        RcptTo,
        Data,
        DataSend,
        Quit,
        Done,
        Failed
    };

    void processServerResponse(const QString& line);
    void sendCommand(const QByteArray& cmd);
    void sendEhlo();
    void sendAuth();
    void sendMailFrom();
    void sendNextRcptTo();
    void sendDataCommand();
    void sendRawData();
    void sendQuit();
    void fail(const QString& reason);

    QSslSocket* m_socket   = nullptr;
    State       m_state    = State::Disconnected;
    QString     m_accessToken;
    QString     m_from;
    QStringList m_recipients;
    int         m_rcptIndex = 0;
    QByteArray  m_rawMime;

    static constexpr int   k_smtpPort = 587;
    static constexpr char  k_smtpHost[] = "smtp.gmail.com";
};
