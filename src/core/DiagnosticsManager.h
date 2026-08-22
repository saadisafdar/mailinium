#pragma once

#include <QString>

//
// DiagnosticsManager — collects and generates sanitized application & system health diagnostics.
// PII is automatically redacted (e.g. emails masked as us***@domain.com).
//
class DiagnosticsManager
{
public:
    DiagnosticsManager() = default;

    // Generates a sanitized markdown report of application state, database stats, and environment.
    static QString generateReport();

    // Redacts email addresses for safe logging/export.
    static QString maskEmail(const QString& email);
};
