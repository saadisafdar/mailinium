#pragma once

#include <QObject>
#include <QSslSocket>
#include <QString>
#include <QStringList>
#include <QList>

#include <QTimer>

#include "storage/Folder.h"
#include "storage/Message.h"

//
// ImapClient — lightweight native IMAP client over QSslSocket.
// Handles connection, authentication, and parsing IMAP responses.
//
class ImapClient : public QObject
{
    Q_OBJECT

public:
    explicit ImapClient(QObject* parent = nullptr);
    ~ImapClient() override;

    void connectToServer(const QString& host, quint16 port);
    void disconnectFromServer();

    void login(const QString& username, const QString& password);

    // Authenticate using OAuth 2.0 (XOAUTH2)
    void loginOAuth2(const QString& username, const QString& accessToken);

    void listFolders();
    void selectFolder(const QString& folderName);
    void searchAllUids();
    void fetchMessageHeaders(const QList<qint64>& uids);
    void fetchMessageBody(qint64 uid);
    void fetchMessageFlags(const QList<qint64>& uids);

    // Message operations
    void storeFlags(const QList<qint64>& uids, const QString& flags, bool add);
    void copyMessages(const QList<qint64>& uids, const QString& destFolder);
    void moveMessages(const QList<qint64>& uids, const QString& destFolder);
    void expunge();

    enum class State {
        Disconnected,
        Connecting,
        Greeting,
        NotAuthenticated,
        Authenticating,
        Authenticated,
        ListingFolders,
        SelectingFolder,
        SearchingUids,
        FetchingHeaders,
        FetchingBody,
        FetchingFlags,
        StoringFlags,
        CopyingMessages,
        MovingMessages,
        Expunging
    };

    State state() const { return m_state; }
    bool isIdle() const { return m_state == State::Authenticated; }

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString& errorMsg);

    void loginSuccessful();
    void loginFailed(const QString& reason);

    void foldersListed(const QList<Folder>& folders);
    void listFailed(const QString& reason);

    void folderSelected(int messageCount, qint64 uidvalidity, qint64 uidnext);
    void uidsFound(const QList<qint64>& uids);
    void headersFetched(const QList<Message>& messages);
    void flagsFetched(const QMap<qint64, int>& flags);
    void bodyFetched(qint64 uid, const QString& body);

    void flagsStored(const QList<qint64>& uids);
    void messagesCopied(const QList<qint64>& uids);
    void messagesMoved(const QList<qint64>& uids);
    void expunged();

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError socketError);
    void onSslErrors(const QList<QSslError>& errors);
    void onReadyRead();
    void onCommandTimeout();

private:
    void processLine(const QByteArray& line);
    QString generateTag();
    void clearParserState();
    void writeCommand(const QByteArray& cmd);

    QSslSocket* m_socket = nullptr;
    QTimer* m_timeoutTimer = nullptr;
    State m_state = State::Disconnected;

    int m_tagCounter = 0;
    QByteArray m_currentTag;

    QList<Folder> m_parsedFolders;

    // Simple IMAP parser helpers
    void handleGreeting(const QByteArray& line);
    void handleLoginResponse(const QByteArray& line);
    void handleListResponse(const QByteArray& line);
    void handleSelectResponse(const QByteArray& line);
    void handleSearchResponse(const QByteArray& line);
    void handleFetchResponse(const QByteArray& line);
    void handleStoreResponse(const QByteArray& line);
    void handleCopyResponse(const QByteArray& line);
    void handleMoveResponse(const QByteArray& line);
    void handleExpungeResponse(const QByteArray& line);

    // Parser state
    QByteArray m_buffer;
    QByteArray m_currentLogicalLine;
    bool m_inLiteralData = false;
    int m_literalBytesExpected = 0;

    // Command state
    int m_expectedMessageCount = 0;
    QList<qint64> m_parsedUids;
    QList<Message> m_parsedMessages;
    qint64 m_currentFetchUid = 0;
    qint64 m_uidvalidity = 0;
    qint64 m_uidnext = 0;
    QMap<qint64, int> m_parsedFlags;

    // Capability & Fallback state
    QStringList m_capabilities;
    enum class MoveFallbackStep {
        None,
        Copy,
        StoreDeleted,
        Expunge
    };
    MoveFallbackStep m_moveFallbackStep = MoveFallbackStep::None;
    QList<qint64> m_pendingMoveUids;
    QString m_pendingMoveDest;
};
