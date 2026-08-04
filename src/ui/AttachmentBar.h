#pragma once

#include <QWidget>
#include <QList>
#include "protocols/MimeMessage.h"

class QHBoxLayout;

//
// AttachmentBar — Widget displaying list of email attachments with save action buttons.
//
class AttachmentBar : public QWidget
{
    Q_OBJECT

public:
    explicit AttachmentBar(QWidget* parent = nullptr);
    ~AttachmentBar() override;

    void setAttachments(const QList<AttachmentInfo>& attachments);
    void clear();

private:
    QHBoxLayout* m_layout = nullptr;
    QList<AttachmentInfo> m_attachments;
};
