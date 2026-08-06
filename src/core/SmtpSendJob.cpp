#include "core/SmtpSendJob.h"
#include "protocols/MimeBuilder.h"
#include "protocols/smtp/SmtpClient.h"
#include "auth/GoogleOAuth.h"
#include "core/Logger.h"
#include "storage/AccountStore.h"

#include <QDateTime>

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

SmtpSendJob::SmtpSendJob(const Account& account,
                          const ComposeData& composeData,
                          QObject* parent)
    : QObject(parent)
    , m_account(account)
    , m_composeData(composeData)
    , m_thread(new QThread(this))
{
}

SmtpSendJob::~SmtpSendJob() = default;

// ---------------------------------------------------------------------------
// Public: start
// ---------------------------------------------------------------------------

void SmtpSendJob::start()
{
    // Build the MIME message synchronously — it's fast
    m_rawMime = MimeBuilder::build(m_composeData);
    if (m_rawMime.isEmpty()) {
        emit failed(QStringLiteral("Failed to build MIME message"));
        return;
    }

    // Decide whether we need to refresh the OAuth token
    if (m_account.authType == AuthType::OAuth2
        && m_account.tokenExpiry.isValid()
        && m_account.tokenExpiry <= QDateTime::currentDateTimeUtc()) {

        Logger::instance().info(QStringLiteral("smtp"),
            QStringLiteral("Access token expired — refreshing before sending"));

        auto* oauth = new GoogleOAuth(this);
        connect(oauth, &GoogleOAuth::authSuccessful,
                this,  &SmtpSendJob::onTokenRefreshed);
        connect(oauth, &GoogleOAuth::authFailed,
                this,  &SmtpSendJob::onTokenRefreshFailed);
        oauth->refreshAccessToken(m_account.refreshToken);
        return;
    }

    // Token is still valid — proceed directly
    beginSmtp(m_account.accessToken);
}

// ---------------------------------------------------------------------------
// Private: beginSmtp — moves SmtpClient to background thread
// ---------------------------------------------------------------------------

void SmtpSendJob::beginSmtp(const QString& accessToken)
{
    // Extract bare "from" email address from the composed From field
    QString fromAddr = m_composeData.from.trimmed();
    const int lt = fromAddr.lastIndexOf('<');
    const int gt = fromAddr.lastIndexOf('>');
    if (lt != -1 && gt != -1 && gt > lt) {
        fromAddr = fromAddr.mid(lt + 1, gt - lt - 1).trimmed();
    }

    // Build recipient list (To + Cc + Bcc)
    QStringList recipients;
    auto appendAddrs = [&](const QString& csv) {
        const QStringList parts = csv.split(',', Qt::SkipEmptyParts);
        for (const QString& p : parts) {
            recipients.append(p.trimmed());
        }
    };
    appendAddrs(m_composeData.to);
    appendAddrs(m_composeData.cc);
    appendAddrs(m_composeData.bcc);

    if (recipients.isEmpty()) {
        emit failed(QStringLiteral("No recipients specified"));
        return;
    }

    auto* client = new SmtpClient(accessToken);
    client->moveToThread(m_thread);

    connect(m_thread, &QThread::started,   this,   &SmtpSendJob::onWorkerStarted);
    connect(client,   &SmtpClient::progress,  this, &SmtpSendJob::onSmtpProgress);
    connect(client,   &SmtpClient::succeeded, this, &SmtpSendJob::onSmtpSucceeded);
    connect(client,   &SmtpClient::failed,    this, &SmtpSendJob::onSmtpFailed);

    // Lambda captures for the thread-start: call client->send on the worker thread
    const QString from    = fromAddr;
    const QStringList rcp = recipients;
    const QByteArray  raw = m_rawMime;

    connect(m_thread, &QThread::started, client, [client, from, rcp, raw]() {
        client->send(from, rcp, raw);
    });

    // Cleanup: stop thread and delete client when done
    connect(client, &SmtpClient::succeeded, m_thread, &QThread::quit);
    connect(client, &SmtpClient::failed,    m_thread, &QThread::quit);
    connect(m_thread, &QThread::finished,   client,   &QObject::deleteLater);

    m_thread->start();
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void SmtpSendJob::onWorkerStarted()
{
    Logger::instance().info(QStringLiteral("smtp"), QStringLiteral("SMTP worker thread started"));
}

void SmtpSendJob::onSmtpProgress(int percent)
{
    emit progress(percent);
}

void SmtpSendJob::onSmtpSucceeded()
{
    Logger::instance().info(QStringLiteral("smtp"), QStringLiteral("SmtpSendJob: message sent"));
    emit succeeded();
    deleteLater();
}

void SmtpSendJob::onSmtpFailed(const QString& error)
{
    Logger::instance().error(QStringLiteral("smtp"), QStringLiteral("SmtpSendJob: ") + error);
    emit failed(error);
    deleteLater();
}

void SmtpSendJob::onTokenRefreshed(const QString& /*email*/,
                                    const QString& accessToken,
                                    const QString& refreshToken,
                                    int expiresInSecs)
{
    m_account.accessToken  = accessToken;
    m_account.refreshToken = refreshToken;
    m_account.tokenExpiry  = QDateTime::currentDateTimeUtc().addSecs(expiresInSecs);

    // Persist the updated tokens to the database
    AccountStore store;
    store.updateAccount(m_account);

    Logger::instance().info(QStringLiteral("smtp"),
        QStringLiteral("Token refreshed — proceeding with send"));
    beginSmtp(accessToken);
}

void SmtpSendJob::onTokenRefreshFailed(const QString& error)
{
    emit failed(QStringLiteral("Token refresh failed: ") + error);
    deleteLater();
}
