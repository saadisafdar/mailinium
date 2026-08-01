#pragma once

#include <QObject>
#include <QSslSocket>
#include <QString>
#include <QStringList>
#include <QList>

#include "storage/Folder.h"

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
    void listFolders();

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString& errorMsg);

    void loginSuccessful();
    void loginFailed(const QString& reason);

    void foldersListed(const QList<Folder>& folders);
    void listFailed(const QString& reason);

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
        ListingFolders
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
};
