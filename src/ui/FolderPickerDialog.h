#pragma once

#include <QDialog>
#include <QList>
#include "storage/Folder.h"

class QListWidget;

class FolderPickerDialog : public QDialog
{
    Q_OBJECT

public:
    FolderPickerDialog(qint64 accountId, qint64 excludeFolderId = -1, QWidget* parent = nullptr);
    ~FolderPickerDialog() override = default;

    qint64 selectedFolderId() const;
    QString selectedFolderName() const;

private:
    QListWidget* m_listWidget = nullptr;
    QList<Folder> m_folders;
};
