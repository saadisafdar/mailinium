#pragma once

#include <QWidget>

class QLabel;

//
// MessageListPanel — center-left pane showing the message list for a folder.
//
// Phase 3: structural shell with:
//   - A header bar (will show folder name and sort/filter controls in Phase 3E)
//   - An empty state placeholder (replaced by QListView + MessageListModel in Phase 3E)
//
// The header bar is created now so the three-pane layout has consistent
// visual weight even before messages are shown.
//
class MessageListPanel : public QWidget
{
    Q_OBJECT

public:
    explicit MessageListPanel(QWidget* parent = nullptr);
    ~MessageListPanel() override;

private:
    void setupUi();

    QWidget* m_headerBar      = nullptr;
    QLabel*  m_folderLabel    = nullptr; // shows selected folder name
    QLabel*  m_placeholderMsg = nullptr; // empty state
};
