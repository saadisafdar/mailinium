#pragma once

#include <QWidget>

class QLabel;
class QTextBrowser;
struct Message;

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

    void displayMessage(const Message& msg, const QString& body);

private:
    void setupUi();

    QWidget*      m_headerArea     = nullptr;
    QLabel*       m_placeholderMsg = nullptr;
    QTextBrowser* m_textBrowser    = nullptr;
    QLabel*       m_subjectLabel   = nullptr;
    QLabel*       m_fromLabel      = nullptr;
    QLabel*       m_dateLabel      = nullptr;
};
