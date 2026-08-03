#pragma once

#include <QDialog>

class QTabWidget;

//
// SettingsDialog — modal tabbed settings dialog.
//
// Tabs are created now as structural shells. Fields are added as each
// feature that requires configuration is implemented:
//
//   General   — Language, Theme, Startup behaviour (Phase 3 — partial)
//   Accounts  — Add/remove/configure email accounts (Phase 3B)
//   Sync      — Fetch interval, offline cache limit (Phase 3D)
//   Compose   — Default reply behaviour, signatures (Phase 3F)
//   Privacy   — Remote image loading policy (Phase 3E)
//   Advanced  — Log level, connection timeout (Phase 3)
//
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    ~SettingsDialog() override;

private:
    void setupUi();
    QWidget* buildGeneralTab();
    QWidget* buildAccountsTab();
    QWidget* buildSyncTab();
    QWidget* buildAdvancedTab();

    QTabWidget* m_tabs = nullptr;
};
