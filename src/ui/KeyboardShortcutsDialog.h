#pragma once

#include <QDialog>

class QTableWidget;

//
// KeyboardShortcutsDialog — displays a clean, searchable cheat sheet of all application keyboard shortcuts.
//
class KeyboardShortcutsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit KeyboardShortcutsDialog(QWidget* parent = nullptr);
    ~KeyboardShortcutsDialog() override = default;

private:
    void setupUi();
    void addShortcutRow(QTableWidget* table, const QString& section, const QString& action, const QString& shortcut);
};
