#pragma once

#include <QWidget>

class QLabel;
class QListWidget;
class QTreeWidget;

//
// AccountSidebar — left navigation panel.
//
// Phase 3: structural shell with two sections:
//   ACCOUNTS — QListWidget (populated in Phase 3B by AccountListModel)
//   FOLDERS  — QTreeWidget (populated in Phase 3B by FolderTreeModel)
//
// Has a slightly different background color than the main content area
// to communicate structural separation, following the convention of
// every major email client.
//
// Updates its background on theme changes via onThemeChanged().
//
class AccountSidebar : public QWidget
{
    Q_OBJECT

public:
    explicit AccountSidebar(QWidget* parent = nullptr);
    ~AccountSidebar() override;

    void loadAccounts();

public slots:
    void onThemeChanged(bool dark);
    void onFoldersUpdated(qint64 accountId);

private slots:
    void onAccountSelected();

private:
    void setupUi();
    void applyBackground(bool dark);

    QLabel*      m_accountsHeader = nullptr;
    QListWidget* m_accountList    = nullptr;
    QLabel*      m_emptyHint      = nullptr; // shown when account list is empty
    QLabel*      m_foldersHeader  = nullptr;
    QTreeWidget* m_folderTree     = nullptr;
};
