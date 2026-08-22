#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;
class QListWidget;

//
// MessageListPanel — center-left pane showing the message list for a folder.
//
// Phase 4C: adds a search bar (FTS5-backed) and batch selection support.
//
class MessageListPanel : public QWidget
{
    Q_OBJECT

public:
    explicit MessageListPanel(QWidget* parent = nullptr);
    ~MessageListPanel() override;

    void loadMessages(qint64 folderId);
    qint64 currentFolderId() const { return m_currentFolderId; }

    // Returns ids of all currently selected messages.
    QList<qint64> selectedMessageIds() const;

    // Focuses the search box and selects all text.
    void focusSearch();

signals:
    void messageSelected(qint64 messageId);
    // Emitted when multi-selection changes (for batch action enabling)
    void selectionChanged(const QList<qint64>& selectedIds);

private slots:
    void onSelectionChanged();
    void onSearchTextChanged(const QString& text);

private:
    void setupUi();
    void populateList(const QList<struct Message>& messages);

    QWidget*     m_headerBar      = nullptr;
    QLabel*      m_folderLabel    = nullptr;
    QLineEdit*   m_searchBox      = nullptr;
    QLabel*      m_placeholderMsg = nullptr;
    QListWidget* m_messageList    = nullptr;
    
    qint64 m_currentFolderId = -1;
};
