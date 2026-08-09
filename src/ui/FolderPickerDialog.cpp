#include "FolderPickerDialog.h"
#include "storage/FolderStore.h"
#include <QVBoxLayout>
#include <QListWidget>
#include <QDialogButtonBox>
#include <QLabel>

FolderPickerDialog::FolderPickerDialog(qint64 accountId, qint64 excludeFolderId, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Select Folder"));
    setMinimumSize(300, 400);

    auto* layout = new QVBoxLayout(this);

    auto* label = new QLabel(tr("Choose a destination folder:"), this);
    layout->addWidget(label);

    m_listWidget = new QListWidget(this);
    layout->addWidget(m_listWidget);

    FolderStore store;
    const auto all = store.foldersForAccount(accountId);
    for (const Folder& f : all) {
        if (f.id != excludeFolderId && f.role != QStringLiteral("noselect")) {
            m_folders.append(f);
            m_listWidget->addItem(f.name);
        }
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    // Double-click accepts the dialog directly
    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, &QDialog::accept);
}

qint64 FolderPickerDialog::selectedFolderId() const
{
    int row = m_listWidget->currentRow();
    if (row >= 0 && row < m_folders.size()) {
        return m_folders.at(row).id;
    }
    return -1;
}

QString FolderPickerDialog::selectedFolderName() const
{
    int row = m_listWidget->currentRow();
    if (row >= 0 && row < m_folders.size()) {
        return m_folders.at(row).name;
    }
    return QString();
}
