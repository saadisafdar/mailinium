#pragma once

#include <QApplication>

//
// Application — subclass of QApplication that owns the application lifecycle.
//
// Responsibilities:
//   - Set application metadata (name, version, organisation)
//   - Initialize Logger and Settings before anything else
//   - Create and show the main window
//   - Handle graceful shutdown (aboutToQuit)
//
// There is exactly one Application instance for the lifetime of the process.
//
class MainWindow;

class Application : public QApplication
{
    Q_OBJECT

public:
    Application(int& argc, char** argv);
    ~Application() override;

private slots:
    void onAboutToQuit();

private:
    void initializeCore();
    void initializeUi();

    QString dataDirectory() const;

    MainWindow* m_mainWindow = nullptr;
};
