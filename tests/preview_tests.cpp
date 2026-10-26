#include "app/Platform.h"
#include "app/PreviewCapture.h"
#include "app/ThumbnailArt.h"
#include "core/ByteSource.h"
#include "stream/HttpServer.h"
#include <QFile>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace bitfly;
class PreviewSource final : public ByteSource {
  public:
    QByteArray data;
    std::optional<StreamFile> describe(const QString& id) const override {
        if (id != "preview/0")
            return {};
        return StreamFile{id, "test-pattern.avi", data.size(), "video/x-msvideo"};
    }
    void demand(const QString&, qint64, qint64) override {}
    QByteArray readVerified(const QString&, qint64 offset, qint64 length) override {
        return data.mid(offset, length);
    }
};
class PreviewTests final : public QObject {
    Q_OBJECT
  private slots:
    void capturesThroughHttpAndCancelsOwnedDecoder() {
        const auto vlc = findVlc(qEnvironmentVariable("BITFLY_TEST_VLC"));
        QVERIFY2(validVlc(vlc), "This explicit media test requires installed VLC.");
        QFile fixture(QStringLiteral(BITFLY_PREVIEW_FIXTURE));
        QVERIFY(fixture.open(QIODevice::ReadOnly));
        PreviewSource source;
        source.data = fixture.readAll();
        HttpServer server(&source);
        QVERIFY(server.start());
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        PreviewCapture capture;
        QSignalSpy ready(&capture, &PreviewCapture::ready);
        const auto cancelled = directory.filePath("cancelled.png");
        const auto saved = directory.filePath("saved.png");
        capture.submit("cancelled", server.url("preview/0"), vlc, cancelled);
        capture.cancel("cancelled");
        capture.submit("saved", server.url("preview/0"), vlc, saved);
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 25000);
        QCOMPARE(ready.first().first().toString(), "saved");
        QVERIFY(!QFile::exists(cancelled));
        const QImage frame(saved);
        QVERIFY(!frame.isNull());
        QCOMPARE(frame.width(), 512);
        QVERIFY(frame.pixelColor(20, 20) != frame.pixelColor(300, 150));
    }
    void rejectsUntrustedEndpointsAndPreservesArtworkOnFailure() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath("cover.png");
        QImage original(32, 32, QImage::Format_RGB32);
        original.fill(Qt::green);
        QVERIFY(writeArtworkImage(path, original));
        QVERIFY(!writeArtworkImage(path, QImage()));
        QCOMPARE(QImage(path), original);
        PreviewCapture capture;
        QSignalSpy ready(&capture, &PreviewCapture::ready);
        capture.submit("remote", QUrl("http://example.com/video"), findVlc(), path);
        QTest::qWait(350);
        QVERIFY(ready.isEmpty());
        QCOMPARE(QImage(path), original);
    }
};
QTEST_MAIN(PreviewTests)
#include "preview_tests.moc"
