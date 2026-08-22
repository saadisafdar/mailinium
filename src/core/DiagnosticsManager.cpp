#include "DiagnosticsManager.h"
#include "storage/Database.h"
#include "storage/AccountStore.h"
#include "storage/FolderStore.h"
#include "storage/MessageStore.h"
#include "storage/DraftStore.h"
#include "core/SyncManager.h"
#include "core/Logger.h"

#include <QDateTime>
#include <QFileInfo>
#include <QSysInfo>
#include <QSqlQuery>

QString DiagnosticsManager::maskEmail(const QString& email)
{
    int atIdx = email.indexOf('@');
    if (atIdx <= 0) return QStringLiteral("***@***");

    QString user = email.left(atIdx);
    QString domain = email.mid(atIdx + 1);

    if (user.length() <= 2) {
        user = user.left(1) + QStringLiteral("***");
    } else {
        user = user.left(2) + QStringLiteral("***");
    }

    return user + QStringLiteral("@") + domain;
}

QString DiagnosticsManager::generateReport()
{
    QString report;
    QTextStream out(&report);

    out << "# Mailinium System & Diagnostic Report\n\n";
    out << "**Generated at:** " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n\n";

    // Application & System Info
    out << "## 1. Application & Environment\n";
    out << "- **Mailinium Version:** 0.1.0\n";
    out << "- **Qt Version:** " << QT_VERSION_STR << "\n";
    out << "- **OS:** " << QSysInfo::prettyProductName() << " (" << QSysInfo::kernelType() << " " << QSysInfo::kernelVersion() << ")\n";
    out << "- **CPU Architecture:** " << QSysInfo::currentCpuArchitecture() << "\n";
    out << "- **Build ABI:** " << QSysInfo::buildAbi() << "\n\n";

    // Database & Storage
    out << "## 2. Storage & Database Metrics\n";
    out << "- **Database Open:** " << (Database::instance().isOpen() ? "Yes" : "No") << "\n";

    AccountStore accStore;
    const auto accounts = accStore.allAccounts();
    out << "- **Configured Accounts:** " << accounts.size() << "\n";

    int totalFolders = 0;
    int totalMessages = 0;
    FolderStore folderStore;
    MessageStore msgStore;

    for (const Account& acc : accounts) {
        auto folders = folderStore.foldersForAccount(acc.id);
        totalFolders += folders.size();
        for (const Folder& f : folders) {
            totalMessages += msgStore.messageCount(f.id);
        }
    }

    out << "- **Total Folders:** " << totalFolders << "\n";
    out << "- **Total Messages:** " << totalMessages << "\n";

    int totalDrafts = 0;
    DraftStore draftStore;
    for (const Account& acc : accounts) {
        totalDrafts += draftStore.draftsForAccount(acc.id).size();
    }
    out << "- **Total Local Drafts:** " << totalDrafts << "\n\n";

    // Active Accounts & Sync State
    out << "## 3. Accounts & Synchronization\n";
    if (accounts.isEmpty()) {
        out << "*No accounts configured.*\n\n";
    } else {
        for (const Account& acc : accounts) {
            QString masked = maskEmail(acc.email);
            QString authType = (acc.authType == AuthType::OAuth2) ? QStringLiteral("OAuth 2.0") : QStringLiteral("Password");
            auto* worker = SyncManager::instance().workerForAccount(acc.id);
            QString workerState = worker ? QStringLiteral("Active / Running") : QStringLiteral("Inactive");

            out << "### Account: " << masked << "\n";
            out << "- **ID:** " << acc.id << "\n";
            out << "- **Auth Type:** " << authType << "\n";
            out << "- **IMAP Host:** " << acc.imapHost << ":" << acc.imapPort << "\n";
            out << "- **SMTP Host:** " << acc.smtpHost << ":" << acc.smtpPort << "\n";
            out << "- **Sync Worker:** " << workerState << "\n";
            out << "- **OAuth Expiry:** " << (acc.tokenExpiry.isValid() ? acc.tokenExpiry.toString(Qt::ISODate) : QStringLiteral("N/A")) << "\n\n";
        }
    }

    // Log Summary
    out << "## 4. Diagnostics & Log Files\n";
    QStringList logFiles = Logger::instance().allLogFiles();
    out << "- **Active Log Directory:** " << Logger::instance().logDir() << "\n";
    out << "- **Log File Count:** " << logFiles.size() << "\n";
    for (const QString& logFile : logFiles) {
        QFileInfo fi(logFile);
        out << "  - `" << fi.fileName() << "` (" << fi.size() << " bytes)\n";
    }

    return report;
}
