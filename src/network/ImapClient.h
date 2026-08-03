#pragma once

#include <QObject>
#include <QSslSocket>
#include <QString>
#include <QStringList>
#include <QList>

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

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString& errorMsg);

    void loginSuccessful();
    void loginFailed(const QString& reason);

    void foldersListed(const QList<Folder>& folders);
    void listFailed(const QString& reason);

    void folderSelected(int messageCount);
    void uidsFound(const QList<qint64>& uids);
    void headersFetched(const QList<Message>& messages);
    void bodyFetched(qint64 uid, const QString& body);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError socketError);
    void onSslErrors(const QList<QSslError>& errors);
    void onReadyRead();

private:
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
        FetchingBody
    };

    void processLine(const QByteArray& line);
    QString generateTag();

    QSslSocket* m_socket = nullptr;
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
};
