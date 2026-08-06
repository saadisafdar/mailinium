#pragma once

#include "storage/Account.h"
#include "storage/Message.h"
#include "storage/Draft.h"
#include "protocols/MimeBuilder.h"

#include <QDialog>
#include <optional>

class QLabel;
class QLineEdit;
class QTextEdit;
class QListWidget;
class QPushButton;
class QProgressBar;
class QTimer;
class QToolBar;

//
// ComposeWindow — native Qt Widgets email compose dialog.
//
// Modes:
//   New      — blank compose window
//   Reply    — To pre-filled with sender; Subject = "Re: ..."
//   ReplyAll — To + Cc pre-filled; Subject = "Re: ..."
//   Forward  — Subject = "Fwd: ..."; body quoted below separator
//
// Auto-save: fires every 30 seconds of idle after any edit.
//
class ComposeWindow : public QDialog
{
    Q_OBJECT

public:
    enum class Mode { New, Reply, ReplyAll, Forward };

    // Construct a compose window with a pre-selected account.
    // For New mode, call directly. For Reply/ReplyAll/Forward, use the
    // static factory methods below.
    explicit ComposeWindow(const Account& account,
                           QWidget* parent = nullptr,
                           Mode mode = Mode::New);
    ~ComposeWindow() override;

    // Factory methods — populate window fields from an existing message.
    static ComposeWindow* createReply   (const Account& account, const Message& original,
                                          const QString& quotedBody, QWidget* parent = nullptr);
    static ComposeWindow* createReplyAll(const Account& account, const Message& original,
                                          const QString& quotedBody, QWidget* parent = nullptr);
    static ComposeWindow* createForward (const Account& account, const Message& original,
                                          const QString& quotedBody, QWidget* parent = nullptr);

    // Restore a previously saved draft into this window.
    void loadDraft(const Draft& draft);

private slots:
    void onSend();
    void onSaveDraft();
    void onAttachFile();
    void onRemoveAttachment();
    void onAutoSave();
    void onBodyChanged();
    void onToggleBcc();

    void onSendProgress(int percent);
    void onSendSucceeded();
    void onSendFailed(const QString& error);

private:
    void setupUi();
    void setupToolBar();
    void setupFields();
    void setupBody();
    void setupAttachmentList();
    void setupProgressBar();

    ComposeData buildComposeData() const;
    bool        saveDraftNow();
    void        populateReplyHeaders(const Message& original);
    static QString buildQuoteBlock(const QString& body, const Message& original);

    Account       m_account;
    Mode          m_mode;
    Draft         m_draft;         // current draft (id == -1 if not yet saved)
    bool          m_draftDirty = false;
    bool          m_sending    = false;

    // Reply context
    QString       m_inReplyTo;
    QString       m_references;

    // Attachments
    QList<ComposeAttachment> m_attachments;

    // --- Widgets ---
    QLabel*       m_fromLabel         = nullptr;
    QLineEdit*    m_toEdit            = nullptr;
    QLineEdit*    m_ccEdit            = nullptr;
    QLineEdit*    m_bccEdit           = nullptr;
    QLineEdit*    m_subjectEdit       = nullptr;
    QTextEdit*    m_bodyEdit          = nullptr;
    QListWidget*  m_attachmentList    = nullptr;
    QProgressBar* m_progressBar       = nullptr;
    QLabel*       m_statusLabel       = nullptr;
    QPushButton*  m_bccToggle         = nullptr;

    // --- Timer ---
    QTimer*       m_autoSaveTimer     = nullptr;
};
