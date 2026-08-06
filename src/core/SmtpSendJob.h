#pragma once

#include "storage/Account.h"
#include "protocols/MimeBuilder.h"

#include <QObject>
#include <QThread>

//
// SmtpSendJob — sends an email off the UI thread using SmtpClient.
//
// Usage:
//   SmtpSendJob* job = new SmtpSendJob(account, composeData, this);
//   connect(job, &SmtpSendJob::progress,   ...);
//   connect(job, &SmtpSendJob::succeeded,  ...);
//   connect(job, &SmtpSendJob::failed,     ...);
//   job->start();   // starts internal QThread; job deletes itself when done
//
class SmtpSendJob : public QObject
{
    Q_OBJECT

public:
    explicit SmtpSendJob(const Account& account,
                         const ComposeData& composeData,
                         QObject* parent = nullptr);
    ~SmtpSendJob() override;

    // Begin the send. Builds the MIME, refreshes token if needed, and
    // runs SmtpClient on a background thread.
    void start();

signals:
    void progress(int percent);
    void succeeded();
    void failed(const QString& errorMessage);

private slots:
    void onWorkerStarted();
    void onSmtpProgress(int percent);
    void onSmtpSucceeded();
    void onSmtpFailed(const QString& error);
    void onTokenRefreshed(const QString& email, const QString& accessToken,
                          const QString& refreshToken, int expiresInSecs);
    void onTokenRefreshFailed(const QString& error);

private:
    void beginSmtp(const QString& accessToken);

    Account       m_account;
    ComposeData   m_composeData;
    QByteArray    m_rawMime;
    QThread*      m_thread = nullptr;
};
