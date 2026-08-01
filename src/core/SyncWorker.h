#pragma once

#include <QObject>
#include <QTimer>
#include <QList>
#include "storage/Account.h"
#include "storage/Folder.h"

class ImapClient;

//
// SyncWorker — Manages the synchronization lifecycle for a single account.
//
class SyncWorker : public QObject
{
    Q_OBJECT

public:
    explicit SyncWorker(const Account& account, QObject* parent = nullptr);
    ~SyncWorker() override;

    void start();
    void stop();

    qint64 accountId() const { return m_account.id; }

signals:
    void statusChanged(const QString& status);
    void foldersUpdated();

private slots:
    void onClientConnected();
    void onClientDisconnected();
    void onClientError(const QString& errorMsg);
    void onLoginSuccessful();
    void onLoginFailed(const QString& reason);
    void onFoldersListed(const QList<Folder>& folders);
    void onListFailed(const QString& reason);
    
    void doSync();

private:
    Account m_account;
    ImapClient* m_imapClient = nullptr;
    QTimer m_syncTimer;
};
