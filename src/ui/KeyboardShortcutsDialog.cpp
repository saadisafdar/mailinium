#include "KeyboardShortcutsDialog.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

KeyboardShortcutsDialog::KeyboardShortcutsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Keyboard Shortcuts"));
    resize(540, 480);
    setupUi();
}

void KeyboardShortcutsDialog::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto* titleLabel = new QLabel(tr("<h2>Keyboard Shortcuts</h2>"), this);
    layout->addWidget(titleLabel);

    auto* table = new QTableWidget(this);
    table->setColumnCount(3);
    table->setHorizontalHeaderLabels({ tr("Category"), tr("Action"), tr("Shortcut") });
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setAlternatingRowColors(true);

    // Navigation & Mail
    addShortcutRow(table, tr("Mail"), tr("New Message"), QStringLiteral("Ctrl+N"));
    addShortcutRow(table, tr("Mail"), tr("Drafts"), QStringLiteral("Ctrl+Shift+D"));
    addShortcutRow(table, tr("Mail"), tr("Check Mail / Sync"), QStringLiteral("F5 / Ctrl+Shift+S"));
    addShortcutRow(table, tr("Mail"), tr("Search Messages"), QStringLiteral("Ctrl+K / Ctrl+F"));

    // Message Actions
    addShortcutRow(table, tr("Message"), tr("Reply"), QStringLiteral("Ctrl+R"));
    addShortcutRow(table, tr("Message"), tr("Reply All"), QStringLiteral("Ctrl+Shift+R"));
    addShortcutRow(table, tr("Message"), tr("Forward"), QStringLiteral("Ctrl+Shift+F"));
    addShortcutRow(table, tr("Message"), tr("Archive Message"), QStringLiteral("E"));
    addShortcutRow(table, tr("Message"), tr("Delete Message"), QStringLiteral("Del / Backspace"));
    addShortcutRow(table, tr("Message"), tr("Mark as Read"), QStringLiteral("Shift+I"));
    addShortcutRow(table, tr("Message"), tr("Mark as Unread"), QStringLiteral("Shift+U"));
    addShortcutRow(table, tr("Message"), tr("Star / Unstar"), QStringLiteral("S"));
    addShortcutRow(table, tr("Message"), tr("Move to Folder"), QStringLiteral("M"));
    addShortcutRow(table, tr("Message"), tr("Copy to Folder"), QStringLiteral("C"));

    // Compose Window
    addShortcutRow(table, tr("Compose"), tr("Send Email"), QStringLiteral("Ctrl+Enter"));
    addShortcutRow(table, tr("Compose"), tr("Attach File"), QStringLiteral("Ctrl+Shift+A"));
    addShortcutRow(table, tr("Compose"), tr("Save Draft"), QStringLiteral("Ctrl+S"));

    // Application
    addShortcutRow(table, tr("App"), tr("Open Settings"), QStringLiteral("Ctrl+,"));
    addShortcutRow(table, tr("App"), tr("Keyboard Shortcuts Sheet"), QStringLiteral("F1"));
    addShortcutRow(table, tr("App"), tr("Quit Mailinium"), QStringLiteral("Ctrl+Q"));

    layout->addWidget(table, 1);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::accept);
    layout->addWidget(buttonBox);
}

void KeyboardShortcutsDialog::addShortcutRow(QTableWidget* table, const QString& section, const QString& action, const QString& shortcut)
{
    int row = table->rowCount();
    table->insertRow(row);

    auto* secItem = new QTableWidgetItem(section);
    auto* actItem = new QTableWidgetItem(action);
    auto* keyItem = new QTableWidgetItem(shortcut);

    QFont keyFont = keyItem->font();
    keyFont.setBold(true);
    keyItem->setFont(keyFont);

    table->setItem(row, 0, secItem);
    table->setItem(row, 1, actItem);
    table->setItem(row, 2, keyItem);
}

#include "moc_KeyboardShortcutsDialog.cpp"
