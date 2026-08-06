#pragma once

#include <QDialog>
#include <QListWidget>
#include <QPushButton>

#include "storage/Account.h"
#include "storage/Draft.h"

//
// DraftsDialog — displays a list of saved drafts for a given account.
// Allows the user to select one to resume or delete.
//
class DraftsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DraftsDialog(const Account& account, QWidget* parent = nullptr);
    ~DraftsDialog() override;

signals:
    // Emitted when the user chooses to resume a draft
    void draftResumed(const Draft& draft);

private slots:
    void reloadDrafts();
    void onResumeClicked();
    void onDeleteClicked();
    void onSelectionChanged();

private:
    void setupUi();

    Account m_account;
    QList<Draft> m_drafts;

    QListWidget* m_listWidget;
    QPushButton* m_btnResume;
    QPushButton* m_btnDelete;
    QPushButton* m_btnClose;
};
