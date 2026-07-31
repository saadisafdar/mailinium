#pragma once

#include <QWidget>

class QLabel;

//
// MessageViewPanel — center-right pane displaying the selected message.
//
// Phase 3: structural shell with:
//   - A header area (will show From, Subject, Date, action buttons in Phase 3E)
//   - An empty state placeholder (replaced by QWebEngineView in Phase 3E)
//
class MessageViewPanel : public QWidget
{
    Q_OBJECT

public:
    explicit MessageViewPanel(QWidget* parent = nullptr);
    ~MessageViewPanel() override;

private:
    void setupUi();

    QWidget* m_headerArea     = nullptr;
    QLabel*  m_placeholderMsg = nullptr;
};
