#include "../src/torrent/StreamingRules.h"
#include <QtTest>
#include <algorithm>
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

    void matchesOnlyTheExpectedManagedPath() {
        QVERIFY(bitfly::sameManagedPath("cache/session/data", "cache/session/./data"));
        QVERIFY(!bitfly::sameManagedPath("cache/session-a/data", "cache/session-b/data"));
        QVERIFY(!bitfly::sameManagedPath(QString(), "cache/session/data"));
    }

    void schedulesRequestedPiecesFirstAndBoundsLookAhead() {
        QCOMPARE(bitfly::scheduledPieces(0, 1000, 130, 200, 100, 10), QList<int>({1, 2, 3}));
        QCOMPARE(bitfly::scheduledPieces(950, 100, 20, 10, 100, 11), QList<int>({9}));
        QCOMPARE(bitfly::scheduledPieces(0, 1000, 0, 1000, 100, 10, 4), QList<int>({0, 1, 2, 3}));
        QCOMPARE(bitfly::scheduledPieces(0, 1000, 0, 1000, 100, 10, 1), QList<int>({0}));
        const auto afterPlayhead = bitfly::scheduledPieces(0, 10000, 350, 1000, 100, 100, 20);
        QCOMPARE(afterPlayhead.first(), 3);
        QVERIFY(std::all_of(afterPlayhead.begin(), afterPlayhead.end(),
                            [](int piece) { return piece >= 3; }));
        QVERIFY(bitfly::scheduledPieces(0, 0, 0, 10, 100, 10).isEmpty());
        QVERIFY(bitfly::scheduledPieces(0, 100, 101, 10, 100, 10).isEmpty());
        QVERIFY(bitfly::scheduledPieces(std::numeric_limits<qint64>::max() - 2, 10, 3, 1, 100, 10)
                    .isEmpty());
        QVERIFY(bitfly::scheduledPieces(std::numeric_limits<qint64>::max() - 4, 10, 0, 10, 100, 10)
                    .isEmpty());
        const auto bounded = bitfly::scheduledPieces(0, 10000, 0, 10000, 1, 10000, 10000);
        QCOMPARE(bounded.size(), 4096);
        QCOMPARE(bounded.first(), 0);
        QCOMPARE(bounded.last(), 4095);
    }

    void advancesStartupBufferAcrossBoundedSchedulingWindows() {
        constexpr qint64 pieceSize = 16 * 1024;
        constexpr qint64 startupBytes = 2 * 1024 * 1024;
        constexpr int lookAhead = 24;
        constexpr int pieceCount = int(startupBytes / pieceSize);
        qint64 offset = 0;
        int windows = 0;
        while (offset < startupBytes) {
            const auto pieces =
                bitfly::scheduledPieces(0, startupBytes, offset, startupBytes - offset,
                                        int(pieceSize), pieceCount, lookAhead);
            QVERIFY(!pieces.isEmpty());
            QCOMPARE(pieces.first(), int(offset / pieceSize));
            QVERIFY(pieces.size() <= lookAhead);
            offset += qint64(pieces.size()) * pieceSize;
            ++windows;
        }
        QCOMPARE(offset, startupBytes);
        QCOMPARE(windows, 6);
    }
};

QTEST_APPLESS_MAIN(TorrentTests)
#include "torrent_tests.moc"
