#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QDir>
#include <QCoreApplication>
#include "app/AppController.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    engine.addImportPath("C:/Qt/6.8.3/mingw_64/qml");
    engine.addImportPath(QCoreApplication::applicationDirPath() + "/qml");
    engine.addImportPath(QDir::currentPath() + "/qml");
    QCoreApplication::addLibraryPath(QCoreApplication::applicationDirPath());
    QCoreApplication::addLibraryPath(QCoreApplication::applicationDirPath() + "/platforms");
    QCoreApplication::addLibraryPath("C:/Qt/6.8.3/mingw_64/plugins");

    AppController controller;
    controller.registerContextProperties(&engine);

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("schedulix_v1", "Main");

    return QGuiApplication::exec();
}
