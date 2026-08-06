#include "ui/DraftsDialog.h"
#include "storage/DraftStore.h"
#include "core/Logger.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>

DraftsDialog::DraftsDialog(const Account& account, QWidget* parent)
    : QDialog(parent)
    , m_account(account)
{
    setWindowTitle(tr("Drafts - %1").arg(m_account.displayName));
    setMinimumSize(400, 300);

    setupUi();
    reloadDrafts();
}

DraftsDialog::~DraftsDialog() = default;

void DraftsDialog::setupUi()
{
    auto* root = new QVBoxLayout(this);

    m_listWidget = new QListWidget(this);
    connect(m_listWidget, &QListWidget::itemSelectionChanged, this, &DraftsDialog::onSelectionChanged);
    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, &DraftsDialog::onResumeClicked);
    root->addWidget(m_listWidget);

    auto* buttonRow = new QHBoxLayout;
    
    m_btnResume = new QPushButton(tr("Resume"), this);
    m_btnResume->setEnabled(false);
    connect(m_btnResume, &QPushButton::clicked, this, &DraftsDialog::onResumeClicked);
    
    m_btnDelete = new QPushButton(tr("Delete"), this);
    m_btnDelete->setEnabled(false);
    connect(m_btnDelete, &QPushButton::clicked, this, &DraftsDialog::onDeleteClicked);
    
    m_btnClose = new QPushButton(tr("Close"), this);
    connect(m_btnClose, &QPushButton::clicked, this, &QDialog::reject);
    
    buttonRow->addWidget(m_btnResume);
    buttonRow->addWidget(m_btnDelete);
    buttonRow->addStretch();
    buttonRow->addWidget(m_btnClose);
    
    root->addLayout(buttonRow);
}

void DraftsDialog::reloadDrafts()
{
    m_listWidget->clear();
    
    DraftStore store;
    m_drafts = store.draftsForAccount(m_account.id);
    
    for (const auto& draft : m_drafts) {
        QString text = draft.subject.isEmpty() ? tr("(No Subject)") : draft.subject;
        text += QStringLiteral(" - ") + draft.updatedAt.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
        m_listWidget->addItem(text);
    }
}

void DraftsDialog::onSelectionChanged()
{
    bool hasSelection = m_listWidget->currentRow() >= 0;
    m_btnResume->setEnabled(hasSelection);
    m_btnDelete->setEnabled(hasSelection);
}

void DraftsDialog::onResumeClicked()
{
    int row = m_listWidget->currentRow();
    if (row < 0 || row >= m_drafts.size()) return;
    
    const Draft& draft = m_drafts.at(row);
    emit draftResumed(draft);
    accept(); // Close dialog when resuming
}

void DraftsDialog::onDeleteClicked()
{
    int row = m_listWidget->currentRow();
    if (row < 0 || row >= m_drafts.size()) return;
    
    const Draft& draft = m_drafts.at(row);
    
    auto reply = QMessageBox::question(this, tr("Delete Draft"), 
        tr("Are you sure you want to delete this draft?"),
        QMessageBox::Yes | QMessageBox::No);
        
    if (reply == QMessageBox::Yes) {
        DraftStore store;
        store.deleteDraft(draft.id);
        reloadDrafts();
    }
}
