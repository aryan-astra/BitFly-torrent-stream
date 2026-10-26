#include "app/ApplicationController.h"
#include <QElapsedTimer>
#include <QFile>
#include <QFont>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>

int main(int argc, char** argv) {
    QElapsedTimer startup;
    startup.start();
    // Raster artwork and short transitions use the existing software render path.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    QGuiApplication application(argc, argv);
    QQuickStyle::setStyle("Basic");
    application.setOrganizationName("BitFly");
    application.setApplicationName("BitFly");
    application.setApplicationVersion(BITFLY_VERSION);
    application.setWindowIcon(QIcon(":/branding/BitFly.ico"));
    application.setFont(QFont("Segoe UI", 10));
    const auto args = application.arguments();
    const int diagnostic = args.indexOf("--diagnostic-data-dir");
    const QString diagnosticDirectory =
        diagnostic >= 0 && diagnostic + 1 < args.size() ? args[diagnostic + 1] : QString();
    bitfly::ApplicationController controller(nullptr, diagnosticDirectory);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("app", &controller);
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &application,
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("BitFly", "Main");
    const int probe = args.indexOf("--startup-probe");
    if (probe >= 0 && probe + 1 < args.size() && !engine.rootObjects().isEmpty()) {
        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        if (window)
            QObject::connect(
                window, &QQuickWindow::frameSwapped, &application,
                [&, done = false]() mutable {
                    if (done)
                        return;
                    done = true;
                    QFile output(args[probe + 1]);
                    if (output.open(QIODevice::WriteOnly))
                        output.write(QJsonDocument(QJsonObject{{"firstFrameMs", startup.elapsed()}})
                                         .toJson());
                },
                Qt::QueuedConnection);
    }
    const int quit = args.indexOf("--quit-after");
    if (quit >= 0 && quit + 1 < args.size())
        QTimer::singleShot(qBound(100, args[quit + 1].toInt(), 60000), &application,
                           &QCoreApplication::quit);
    QObject::connect(&controller, &bitfly::ApplicationController::workerReady, &application, [&] {
        if (args.size() == 2 && !args[1].startsWith("--"))
            controller.openInput(args[1]);
    });
    const int shot = args.indexOf("--screenshot");
    if (shot >= 0 && shot + 1 < args.size()) {
        QTimer::singleShot(1800, &application, [&] {
            auto* window = engine.rootObjects().isEmpty()
                               ? nullptr
                               : qobject_cast<QQuickWindow*>(engine.rootObjects().first());
            const bool saved = window && window->grabWindow().save(args[shot + 1]);
            application.exit(saved ? 0 : 2);
        });
    }
    return application.exec();
}
