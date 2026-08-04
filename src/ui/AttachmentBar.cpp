#include "ui/AttachmentBar.h"
#include "core/Logger.h"
#include "storage/MessageStore.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>


AttachmentBar::AttachmentBar(QWidget* parent)
    : QWidget(parent)
{
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(12, 6, 12, 6);
    m_layout->setSpacing(8);
    hide();
}

AttachmentBar::~AttachmentBar() = default;

void AttachmentBar::clear()
{
    QLayoutItem* child;
    while ((child = m_layout->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }
    m_attachments.clear();
    hide();
}

static QString formatSize(qint64 bytes)
{
    if (bytes < 1024) return QString::number(bytes) + " B";
    if (bytes < 1024 * 1024) return QString::number(bytes / 1024) + " KB";
    return QString::number(bytes / (1024 * 1024)) + " MB";
}

void AttachmentBar::setAttachments(const QList<AttachmentInfo>& attachments)
{
    clear();
    m_attachments = attachments;

    if (m_attachments.isEmpty()) {
        hide();
        return;
    }

    auto* titleLabel = new QLabel(tr("Attachments (%1):").arg(m_attachments.size()), this);
    {
        QFont f = titleLabel->font();
        f.setBold(true);
        titleLabel->setFont(f);
    }
    m_layout->addWidget(titleLabel);

    for (const AttachmentInfo& att : m_attachments) {
        QString text = QStringLiteral("📎 %1 (%2)").arg(att.filename, formatSize(att.size));
        auto* btn = new QPushButton(text, this);

        connect(btn, &QPushButton::clicked, this, [this, att]() {
            QString path = QFileDialog::getSaveFileName(this, tr("Save Attachment"), att.filename);
            if (!path.isEmpty()) {
                QFile file(path);
                if (file.open(QIODevice::WriteOnly)) {
                    MessageStore store;
                    QByteArray lazyData = store.getAttachmentData(att.id);
                    file.write(lazyData);
                    file.close();
                    Logger::instance().info(QStringLiteral("ui"), QStringLiteral("Saved attachment to ") + path);
                } else {
                    QMessageBox::warning(this, tr("Error"), tr("Could not save file to %1").arg(path));
                }
            }
        });

        m_layout->addWidget(btn);
    }

    m_layout->addStretch();
    show();
}
