#include "../src/torrent/StreamingRules.h"
#include <QtTest>
#include <limits>

class TorrentTests final : public QObject {
    Q_OBJECT
  private slots:
    void rejectsUnsafeTorrentPaths() {
        QVERIFY(bitfly::isSafeTorrentPath("Season 1/Episode 01.mkv"));
        QVERIFY(!bitfly::isSafeTorrentPath("../outside.mkv"));
        QVERIFY(!bitfly::isSafeTorrentPath("C:/outside.mkv"));
        QVERIFY(!bitfly::isSafeTorrentPath("folder\\..\\outside.mkv"));
        QVERIFY(!bitfly::isSafeTorrentPath("folder/CON.txt"));
        QVERIFY(!bitfly::isSafeTorrentPath("folder/name. "));
        QVERIFY(!bitfly::isSafeTorrentPath("folder//file.mkv"));
    }

    void schedulesRequestedPiecesFirstAndBoundsLookAhead() {
        QCOMPARE(bitfly::scheduledPieces(0, 1000, 130, 200, 100, 10), QList<int>({1, 2, 3}));
        QCOMPARE(bitfly::scheduledPieces(950, 100, 20, 10, 100, 11), QList<int>({9}));
        QCOMPARE(bitfly::scheduledPieces(0, 1000, 0, 1000, 100, 10, 4), QList<int>({0, 1, 2, 3}));
        QVERIFY(bitfly::scheduledPieces(0, 0, 0, 10, 100, 10).isEmpty());
        QVERIFY(bitfly::scheduledPieces(0, 100, 101, 10, 100, 10).isEmpty());
        QVERIFY(bitfly::scheduledPieces(std::numeric_limits<qint64>::max() - 2, 10, 3, 1, 100, 10)
                    .isEmpty());
        QVERIFY(bitfly::scheduledPieces(std::numeric_limits<qint64>::max() - 4, 10, 0, 10, 100, 10)
                    .isEmpty());
    }
};

QTEST_APPLESS_MAIN(TorrentTests)
#include "torrent_tests.moc"
