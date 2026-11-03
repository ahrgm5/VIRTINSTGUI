#include <QApplication>
#include <ic4/ic4.h>
#include "mainwindow.h" // Replace with the actual header for your GUI class

int main(int argc, char *argv[])
{
    // 1. Initialize the IC4 library (Required before any IC4 calls)
    ic4::InitLibraryConfig ic4Config;
    ic4::initLibrary(ic4Config);

    // 2. Initialize the Qt Application
    QApplication a(argc, argv);

    // 3. Instantiate and display your existing GUI
    MainWindow w;
    w.show();

    // 4. Run the Qt event loop
    int exitCode = a.exec();

    // 5. Clean up the IC4 library upon exit
    ic4::exitLibrary();

    return exitCode;
}