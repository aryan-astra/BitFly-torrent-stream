// qmllint disable unqualified
// The app object is supplied by the C++ context property contract.
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: window
    width: 1240
    height: 800
    minimumWidth: 940
    minimumHeight: 640
    visible: true
    title: Qt.application.version.length ? "BitFly " + Qt.application.version : "BitFly"
    color: Theme.window
    font.family: Theme.fontFamily
    palette.window: Theme.window
    palette.windowText: Theme.ink
    palette.text: Theme.ink
    palette.buttonText: Theme.ink
    palette.button: Theme.raised
    palette.highlight: Theme.accent
    palette.highlightedText: Theme.accentInk
    palette.base: Theme.surface

    property string page: "Library"
    property string pendingRemovalId: ""
    property bool pendingRemovalIsLibrary: false
    property string endedSessionId: ""
    property string pendingArtworkId: ""

    function displayName(item) {
        return item.name || item.title || item.fileName || item.id || "Torrent"
    }
    function displayFile(item) {
        return item.fileName || item.media || ""
    }
    function humanSize(bytes) {
        if (!bytes || bytes <= 0) return "0 B"
        const units = ["B", "KB", "MB", "GB", "TB"]
        let value = bytes, unit = 0
        while (value >= 1024 && unit < units.length - 1) { value /= 1024; ++unit }
        return (unit === 0 ? Math.round(value) : value.toFixed(1)) + " " + units[unit]
    }
    function pageSubtitle() {
        if (window.page === "Queue")
            return "Line up files from any active session."
        if (window.page === "Settings")
            return "A few preferences. Make yourself at home."
        if (window.page === "Search")
            return "Find a stream through your connected indexers."
        return "A little home for everything you want to watch."
    }
    function syncRows(model, incoming) {
        const ids = new Set()
        for (let i = 0; i < incoming.length; ++i)
            ids.add(String(incoming[i].id || ""))
        for (let i = model.count - 1; i >= 0; --i)
            if (!ids.has(String(model.get(i).rowId)))
                model.remove(i)
        for (let target = 0; target < incoming.length; ++target) {
            const item = incoming[target]
            const key = String(item.id || "")
            let existing = -1
            for (let i = target; i < model.count; ++i) {
                if (String(model.get(i).rowId) === key) { existing = i; break }
            }
            if (existing < 0)
                model.insert(target, { rowId: key, payload: item })
            else {
                if (existing !== target) model.move(existing, target, 1)
                model.setProperty(target, "payload", item)
            }
        }
    }
    function syncSessions() {
        window.syncRows(activeSessions, app.sessions || [])
    }
    function syncLibrary() {
        const all = app.library || []
        const saved = []
        for (let i = 0; i < all.length; ++i)
            if (all[i].active !== true)
                saved.push(all[i])
        window.syncRows(libraryEntries, saved)
    }
    function syncAll() {
        window.syncSessions()
        window.syncLibrary()
    }

    ListModel {
        id: activeSessions
        objectName: "activeSessions"
        dynamicRoles: true
    }
    ListModel {
        id: libraryEntries
        objectName: "libraryEntries"
        dynamicRoles: true
    }
    Component.onCompleted: window.syncAll()
    Connections {
        target: app
        function onChanged() { window.syncAll() }
    }

    Binding { target: Theme; property: "reducedMotion"; value: app.reducedMotion }
    Image {
        anchors.fill: parent
        source: "assets/sky-clouds.png"
        sourceSize.width: 1280
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        opacity: 1
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // Left rail ------------------------------------------------------------
        Rectangle {
            Layout.preferredWidth: 190
            Layout.fillHeight: true
            color: Theme.rail

            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                anchors.topMargin: 22
                anchors.bottomMargin: 18
                spacing: 0

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 6
                    Layout.bottomMargin: 26
                    spacing: 10
                    Image {
                        Layout.preferredWidth: 42
                        Layout.preferredHeight: 42
                        source: "assets/bitfly-mark.png"
                        sourceSize.width: 84
                        sourceSize.height: 84
                        fillMode: Image.PreserveAspectFit
                    }
                    ColumnLayout {
                        spacing: 0
                        Label {
                            text: "BitFly"
                            color: Theme.ink
                            font.family: Theme.displayFamily
                            font.pixelSize: 23
                            font.weight: Font.Normal
                        }
                        Label {
                            text: "a little less waiting"
                            color: Theme.secondary
                            font.pixelSize: 12
                        }
                    }
                }

                Label {
                    text: "YOUR SPACE"
                    color: Theme.faint
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    font.letterSpacing: 1.4
                    Layout.leftMargin: 12
                    Layout.bottomMargin: 8
                }
                NavItem {
                    objectName: "navLibrary"
                    label: "Library"
                    iconName: "library"
                    selected: window.page === "Library"
                    Layout.fillWidth: true
                    onClicked: window.page = "Library"
                }
                NavItem {
                    objectName: "navQueue"
                    label: "Queue"
                    iconName: "queue"
                    selected: window.page === "Queue"
                    Layout.fillWidth: true
                    Layout.topMargin: 3
                    onClicked: window.page = "Queue"
                }
                NavItem {
                    objectName: "navSearch"
                    label: "Search"
                    iconName: "search"
                    selected: window.page === "Search"
                    Layout.fillWidth: true
                    Layout.topMargin: 3
                    onClicked: window.page = "Search"
                }
                NavItem {
                    objectName: "navSettings"
                    label: "Settings"
                    iconName: "settings"
                    selected: window.page === "Settings"
                    Layout.fillWidth: true
                    Layout.topMargin: 3
                    onClicked: window.page = "Settings"
                }

                Item { Layout.fillHeight: true }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 96
                    radius: Theme.radius
                    color: Theme.surface
                    border.color: Theme.line
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 6
                        RowLayout {
                            Layout.fillWidth: true
                            Label {
                                text: "CACHE"
                                color: Theme.faint
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                font.letterSpacing: 1.2
                            }
                            Item { Layout.fillWidth: true }
                            Label {
                                text: Math.round(Number(app.cacheLimitGiB) > 0
                                                ? (Number(app.cacheBytes) / (Number(app.cacheLimitGiB) * 1073741824)) * 100
                                                : 0) + "%"
                                color: Theme.secondary
                                font.pixelSize: 12
                            }
                        }
                        Label {
                            text: window.humanSize(app.cacheBytes) + " of " + app.cacheLimitGiB + " GiB"
                            color: Theme.ink
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                        }
                        Item {
                            Layout.fillWidth: true
                            implicitHeight: 4
                            Rectangle { anchors.fill: parent; radius: 2; color: Theme.line }
                            Rectangle {
                                width: parent.width * Math.max(0, Math.min(1,
                                    Number(app.cacheBytes) / Math.max(1, Number(app.cacheLimitGiB) * 1073741824)))
                                height: parent.height
                                radius: 2
                                color: Theme.accent
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Rectangle {
                                implicitWidth: 6; implicitHeight: 6; radius: 3
                                Layout.alignment: Qt.AlignVCenter
                                color: app.playerPath.length > 0 ? Theme.success : Theme.warning
                            }
                            Label {
                                text: app.playerPath.length > 0 ? app.playerName + " ready" : "Choose a player"
                                color: Theme.secondary
                                font.pixelSize: 12
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                            LinkButton {
                                text: "Folder"
                                onClicked: app.openStorageFolder()
                            }
                        }
                    }
                }
            }
        }

        // Main column ---------------------------------------------------------
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 98
                color: Theme.window
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 30
                    anchors.rightMargin: 30
                    spacing: 12
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3
                        Label {
                            text: window.page
                            color: Theme.ink
                            font.family: Theme.displayFamily
                            font.pixelSize: 32
                            font.weight: Font.Normal
                        }
                        Label {
                            Layout.fillWidth: true
                            text: window.pageSubtitle()
                            color: Theme.secondary
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }
                    }
                    Rectangle {
                        radius: 11
                        color: Theme.surface
                        border.color: Theme.line
                        implicitWidth: activeChip.implicitWidth + 22
                        implicitHeight: 32
                        RowLayout {
                            id: activeChip
                            anchors.centerIn: parent
                            spacing: 7
                            Rectangle {
                                implicitWidth: 6; implicitHeight: 6; radius: 3
                                Layout.alignment: Qt.AlignVCenter
                                color: Theme.accent
                            }
                            Label {
                                text: (app.sessions ? app.sessions.length : 0) + " active"
                                color: Theme.secondary
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                            }
                        }
                    }
                    Rectangle {
                        radius: 11
                        color: Theme.surface
                        border.color: Theme.line
                        implicitWidth: cacheChip.implicitWidth + 22
                        implicitHeight: 32
                        RowLayout {
                            id: cacheChip
                            anchors.centerIn: parent
                            spacing: 7
                            Label {
                                text: "LOCAL"
                                color: Theme.accent
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                font.letterSpacing: 1
                            }
                            Label {
                                text: window.humanSize(app.cacheBytes)
                                color: Theme.secondary
                                font.pixelSize: 12
                            }
                        }
                    }
                }
            }

            // Inline status banners
            Rectangle {
                visible: app.error.length > 0
                Layout.fillWidth: true
                Layout.leftMargin: 30
                Layout.rightMargin: 30
                Layout.bottomMargin: 8
                implicitHeight: errorRow.implicitHeight + 18
                radius: Theme.radius
                color: Theme.dangerWash
                border.color: Theme.line
                RowLayout {
                    id: errorRow
                    anchors.fill: parent
                    anchors.leftMargin: 13
                    anchors.rightMargin: 9
                    spacing: 10
                    Label {
                        Layout.fillWidth: true
                        text: app.error
                        color: Theme.danger
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                    }
                    QuietButton { text: "Dismiss"; onClicked: app.clearError() }
                }
            }
            Rectangle {
                visible: app.error.length === 0 && app.notice.length > 0
                Layout.fillWidth: true
                Layout.leftMargin: 30
                Layout.rightMargin: 30
                Layout.bottomMargin: 8
                implicitHeight: noticeRow.implicitHeight + 18
                radius: Theme.radius
                color: Theme.accentWash
                border.color: Theme.line
                RowLayout {
                    id: noticeRow
                    anchors.fill: parent
                    anchors.leftMargin: 13
                    anchors.rightMargin: 9
                    spacing: 10
                    Label {
                        Layout.fillWidth: true
                        text: app.notice
                        color: Theme.accent
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                    }
                    QuietButton { text: "Dismiss"; onClicked: app.clearNotice() }
                }
            }

            StackLayout {
                id: pages
                currentIndex: window.page === "Queue" ? 1 : window.page === "Settings" ? 2 : window.page === "Search" ? 3 : 0
                Layout.fillWidth: true
                Layout.fillHeight: true

                // Library -----------------------------------------------------
                Item {
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 30
                        anchors.rightMargin: 30
                        anchors.topMargin: 8
                        anchors.bottomMargin: 16
                        spacing: 12

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 58
                            radius: Theme.radiusLarge
                            color: Theme.surface
                            border.color: magnetInput.activeFocus ? Theme.accent : Theme.line

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                spacing: 8
                                Rectangle {
                                    Layout.leftMargin: 4
                                    implicitWidth: 74
                                    implicitHeight: 30
                                    radius: Theme.radiusSmall
                                    color: Theme.accentWash
                                    Label {
                                        anchors.centerIn: parent
                                        text: "MAGNET"
                                        color: Theme.accent
                                        font.pixelSize: 12
                                        font.weight: Font.DemiBold
                                        font.letterSpacing: 1
                                    }
                                }
                                TextField {
                                    id: magnetInput
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 120
                                    implicitHeight: 40
                                    placeholderText: "Paste a magnet link or info hash…"
                                    Accessible.name: "Magnet link or info hash"
                                    placeholderTextColor: Theme.faint
                                    color: Theme.ink
                                    selectByMouse: true
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 14
                                    background: Rectangle { color: "transparent" }
                                    onAccepted: addButton.clicked()
                                }
                                QuietButton {
                                    text: "Open .torrent"
                                    onClicked: torrentDialog.open()
                                }
                                QuietButton {
                                    id: addButton
                                    text: "Add stream"
                                    highlighted: true
                                    enabled: magnetInput.text.trim().length > 0
                                    onClicked: {
                                        app.openInput(magnetInput.text.trim())
                                        magnetInput.clear()
                                    }
                                }
                            }
                        }

                        GridView {
                            id: libraryScroll
                            objectName: "libraryRepeater"
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: libraryEntries
                            readonly property int columns: Math.max(2, Math.min(5, Math.floor(width / 218)))
                            cellWidth: width / columns
                            cellHeight: cellWidth + 110
                            cacheBuffer: cellHeight
                            reuseItems: true
                            property bool userHasScrolled: false
                            onMovementStarted: userHasScrolled = true
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                            header: ColumnLayout {
                                width: libraryScroll.width
                                spacing: 14
                                onHeightChanged: {
                                    if (!libraryScroll.userHasScrolled)
                                        Qt.callLater(function () { libraryScroll.positionViewAtBeginning() })
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.topMargin: 6
                                    spacing: 9
                                    Label {
                                        text: "Active sessions"
                                        color: Theme.ink
                                        font.family: Theme.displayFamily
                                        font.pixelSize: 21
                                        font.weight: Font.Normal
                                    }
                                    Rectangle {
                                        visible: activeSessions.count > 0
                                        implicitWidth: activeCount.implicitWidth + 14
                                        implicitHeight: 20
                                        radius: 10
                                        color: Theme.accentWash
                                        Label {
                                            id: activeCount
                                            anchors.centerIn: parent
                                            text: activeSessions.count
                                            color: Theme.accent
                                            font.pixelSize: 12
                                            font.weight: Font.DemiBold
                                        }
                                    }
                                    Item { Layout.fillWidth: true }
                                    LinkButton {
                                        text: app.advanced ? "Hide diagnostics" : "Diagnostics"
                                        onClicked: app.advanced = !app.advanced
                                    }
                                }

                                Item {
                                    Layout.fillWidth: true
                                    implicitHeight: activeFlow.childrenRect.height
                                    Flow {
                                        id: activeFlow
                                        width: parent.width
                                        spacing: 14
                                        readonly property int columns: Math.max(2, Math.min(3, Math.floor(width / 400)))
                                        Repeater {
                                            objectName: "sessionRepeater"
                                            model: activeSessions
                                            delegate: SessionCard {
                                                required property string rowId
                                                required property var payload
                                                objectName: "sessionCard_" + rowId
                                                width: (activeFlow.width - (activeFlow.columns - 1) * activeFlow.spacing) / activeFlow.columns
                                                session: payload
                                                onPlayRequested: (id, fileIndex) => app.play(id, fileIndex)
                                                onQueueRequested: (id, fileIndex) => app.enqueue(id, fileIndex)
                                                onPauseRequested: (id, paused) => app.pause(id, paused)
                                                onArtworkRequested: id => { window.pendingArtworkId = id; artworkDialog.open() }
                                                onRefreshRequested: id => app.refreshPreview(id)
                                                onRevealRequested: id => app.revealSession(id)
                                                onRemoveRequested: id => {
                                                    window.pendingRemovalId = id
                                                    window.pendingRemovalIsLibrary = false
                                                    removeDialog.open()
                                                }
                                            }
                                        }
                                    }
                                }

                                Rectangle {
                                    visible: activeSessions.count === 0
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 150
                                    radius: Theme.radiusLarge
                                    color: Theme.surface
                                    border.color: Theme.line
                                    ColumnLayout {
                                        anchors.centerIn: parent
                                        spacing: 5
                                        Label {
                                            Layout.alignment: Qt.AlignHCenter
                                            text: "What shall we watch?"
                                            color: Theme.ink
                                            font.pixelSize: 15
                                            font.weight: Font.DemiBold
                                        }
                                        Label {
                                            text: "Paste a magnet above or open a .torrent file to start buffering."
                                            color: Theme.secondary
                                            font.pixelSize: 12
                                        }
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.topMargin: 8
                                    spacing: 9
                                    Label {
                                        text: "Library"
                                        color: Theme.ink
                                        font.family: Theme.displayFamily
                                        font.pixelSize: 21
                                        font.weight: Font.Normal
                                    }
                                    Label {
                                        text: libraryEntries.count + (libraryEntries.count === 1 ? " saved stream" : " saved streams")
                                        color: Theme.faint
                                        font.pixelSize: 12
                                    }
                                    Item { Layout.fillWidth: true }
                                    Label {
                                        text: "Pick up where you left off"
                                        color: Theme.faint
                                        font.pixelSize: 12
                                    }
                                }

                            }
                            delegate: Item {
                                id: shelfCell
                                required property string rowId
                                required property var payload
                                width: libraryScroll.cellWidth
                                height: libraryScroll.cellHeight
                                PosterCard {
                                    objectName: "libraryCard_" + shelfCell.rowId
                                    anchors.top: parent.top
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.margins: 7
                                    entry: shelfCell.payload
                                    onPlayRequested: id => app.relaunch(id)
                                    onArtworkRequested: id => { window.pendingArtworkId = id; artworkDialog.open() }
                                    onRefreshRequested: id => app.refreshPreview(id)
                                    onRevealRequested: id => app.revealSession(id)
                                    onRemoveRequested: id => {
                                        window.pendingRemovalId = id
                                        window.pendingRemovalIsLibrary = true
                                        removeDialog.open()
                                    }
                                }
                            }
                            footer: ColumnLayout {
                                width: libraryScroll.width
                                spacing: 14
                                Rectangle {
                                    visible: libraryEntries.count === 0
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 76
                                    radius: Theme.radiusLarge
                                    color: Theme.surface
                                    border.color: Theme.line
                                    Label {
                                        anchors.centerIn: parent
                                        text: "Your saved streams will live here. Their links and covers stay until you remove them."
                                        color: Theme.faint
                                        font.pixelSize: 12
                                    }
                                }

                                Rectangle {
                                    visible: app.advanced
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 42
                                    radius: Theme.radius
                                    color: Theme.surface
                                    border.color: Theme.line
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 13
                                        anchors.rightMargin: 13
                                        spacing: 16
                                        Label {
                                            text: "DIAGNOSTICS"
                                            color: Theme.faint
                                            font.pixelSize: 12
                                            font.weight: Font.DemiBold
                                            font.letterSpacing: 1
                                        }
                                        Label {
                                            text: "Cache " + window.humanSize(app.cacheBytes) + " / " + app.cacheLimitGiB + " GiB"
                                            color: Theme.secondary
                                            font.pixelSize: 12
                                        }
                                        Label {
                                            text: app.connectionLimit + " peer slots · " + app.uploadLimitKiB + " KiB/s up · " + app.streamAheadMiB + " MiB read-ahead"
                                            color: Theme.secondary
                                            font.pixelSize: 12
                                        }
                                        Item { Layout.fillWidth: true }
                                        Label {
                                            visible: app.updateMessage.length > 0
                                            text: app.updateMessage
                                            color: Theme.faint
                                            font.pixelSize: 12
                                            elide: Text.ElideRight
                                            Layout.maximumWidth: 260
                                        }
                                    }
                                }
                                Item { Layout.preferredHeight: 4 }                            }
                        }
                    }
                }

                // Queue -------------------------------------------------------
                Item {
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 30
                        anchors.topMargin: 8
                        spacing: 12

                        RowLayout {
                            Layout.fillWidth: true
                            Label {
                                text: "Up next"
                                color: Theme.ink
                                font.pixelSize: 15
                                font.weight: Font.DemiBold
                            }
                            Label {
                                text: "Your next file opens when the current player closes."
                                color: Theme.faint
                                font.pixelSize: 12
                            }
                            Item { Layout.fillWidth: true }
                            Label {
                                text: (app.queue ? app.queue.length : 0) + " queued"
                                color: Theme.faint
                                font.pixelSize: 12
                            }
                        }

                        ScrollView {
                            id: queueScroll
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                            ScrollBar.vertical: ScrollBar {
                                policy: ScrollBar.AsNeeded
                                background: Rectangle { color: "transparent" }
                                contentItem: Rectangle { implicitWidth: 6; radius: 3; color: Theme.line }
                            }
                            ColumnLayout {
                                width: queueScroll.availableWidth
                                spacing: 8
                                Repeater {
                                    model: app.queue
                                    delegate: Rectangle {
                                        id: queueRow
                                        required property int index
                                        required property var modelData
                                        Layout.fillWidth: true
                                        implicitHeight: 64
                                        radius: Theme.radius
                                        color: Theme.surface
                                        border.color: queueHover.hovered ? Theme.accent : Theme.line
                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: 10
                                            anchors.rightMargin: 8
                                            spacing: 10
                                            Rectangle {
                                                implicitWidth: 38
                                                implicitHeight: 38
                                                radius: Theme.radiusSmall
                                                color: Theme.raised
                                                Label {
                                                    anchors.centerIn: parent
                                                    text: (queueRow.index + 1).toString().padStart(2, "0")
                                                    color: Theme.accent
                                                    font.pixelSize: 12
                                                    font.weight: Font.DemiBold
                                                }
                                            }
                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                Layout.minimumWidth: 0
                                                spacing: 2
                                                Label {
                                                    Layout.fillWidth: true
                                                    text: window.displayName(queueRow.modelData)
                                                    color: Theme.ink
                                                    font.pixelSize: 14
                                                    font.weight: Font.DemiBold
                                                    elide: Text.ElideMiddle
                                                }
                                                Label {
                                                    visible: window.displayFile(queueRow.modelData).length > 0
                                                    Layout.fillWidth: true
                                                    text: window.displayFile(queueRow.modelData)
                                                    color: Theme.secondary
                                                    font.pixelSize: 12
                                                    elide: Text.ElideMiddle
                                                }
                                            }
                                            IconButton {
                                                glyph: "↑"
                                                hint: "Move up"
                                                enabled: queueRow.index > 0
                                                onClicked: app.moveQueue(queueRow.index, queueRow.index - 1)
                                            }
                                            IconButton {
                                                glyph: "↓"
                                                hint: "Move down"
                                                enabled: queueRow.index < app.queue.length - 1
                                                onClicked: app.moveQueue(queueRow.index, queueRow.index + 1)
                                            }
                                            IconButton {
                                                glyph: "✕"
                                                hint: "Remove from queue"
                                                destructive: true
                                                onClicked: app.removeQueue(queueRow.index)
                                            }
                                            QuietButton {
                                                text: "Play"
                                                highlighted: true
                                                implicitHeight: 30
                                                enabled: queueRow.modelData.id !== undefined && queueRow.modelData.fileIndex !== undefined
                                                onClicked: app.playQueued(queueRow.index)
                                            }
                                        }
                                        HoverHandler { id: queueHover }
                                    }
                                }
                                Rectangle {
                                    visible: !app.queue || app.queue.length === 0
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 160
                                    radius: Theme.radiusLarge
                                    color: Theme.surface
                                    border.color: Theme.line
                                    ColumnLayout {
                                        anchors.centerIn: parent
                                        spacing: 5
                                        Label {
                                            Layout.alignment: Qt.AlignHCenter
                                            text: "Nothing queued"
                                            color: Theme.ink
                                            font.pixelSize: 14
                                            font.weight: Font.DemiBold
                                        }
                                        Label {
                                            text: "Use Queue on an active session to line up its next file."
                                            color: Theme.secondary
                                            font.pixelSize: 12
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // Settings ----------------------------------------------------
                Item {
                    ScrollView {
                        id: settingsScroll
                        anchors.fill: parent
                        anchors.leftMargin: 30
                        anchors.rightMargin: 30
                        anchors.topMargin: 8
                        anchors.bottomMargin: 16
                        clip: true
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                            background: Rectangle { color: "transparent" }
                            contentItem: Rectangle { implicitWidth: 6; radius: 3; color: Theme.line }
                        }
                        ColumnLayout {
                            width: Math.min(settingsScroll.availableWidth, 860)
                            spacing: 10

                            SettingsCard {
                                title: "Storage"
                                SettingRow {
                                    title: "Download folder"
                                    hint: "BitFly writes each session into a managed “torrents” subfolder here. Changing it applies to new sessions immediately; active sessions keep their original folder so playback and cleanup stay consistent."
                                    detail: app.cachePath
                                    QuietButton { text: "Choose folder…"; onClicked: cacheDialog.open() }
                                    IconButton { glyph: "↗"; hint: "Open download folder"; onClicked: app.openStorageFolder() }
                                }
                                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.line }
                                SettingRow {
                                    title: "Cache limit"
                                    hint: "Hard cap on managed data across every download folder. At the cap, background sessions pause and new downloads are refused until space is freed."
                                    detail: "In use: " + window.humanSize(app.cacheBytes)
                                    SettingSpinBox {
                                        Accessible.name: "Cache limit in GiB"
                                        from: 1; to: 1024; stepSize: 1
                                        value: app.cacheLimitGiB
                                        editable: true
                                        onValueModified: app.cacheLimitGiB = value
                                        textFromValue: value => value + " GiB"
                                        valueFromText: text => parseInt(text)
                                    }
                                }
                            }

                            SettingsCard {
                                title: "Streaming"
                                SettingRow {
                                    title: "Read-ahead buffer"
                                    hint: "How much data to prioritise ahead of the current read position. This is not a limit on total downloaded storage."
                                    SettingSpinBox {
                                        Accessible.name: "Read-ahead buffer in MiB"
                                        from: 8; to: 8192; stepSize: 8
                                        value: app.streamAheadMiB
                                        editable: true
                                        onValueModified: app.streamAheadMiB = value
                                        textFromValue: value => value + " MiB"
                                        valueFromText: text => parseInt(text)
                                    }
                                }
                                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.line }
                                SettingRow {
                                    title: "Optional seed duration"
                                    hint: "If you choose Seed after playback, upload for this many minutes before pausing. Zero disables that option. Uploads while downloading are controlled separately by the upload cap."
                                    SettingSpinBox {
                                        Accessible.name: "Seed duration in minutes"
                                        from: 0; to: 1440; stepSize: 5
                                        value: app.seedMinutes
                                        editable: true
                                        onValueModified: app.seedMinutes = value
                                        textFromValue: value => value === 0 ? "Off" : value + " min"
                                        valueFromText: text => text === "Off" ? 0 : parseInt(text)
                                    }
                                }
                            }

                            SettingsCard {
                                title: "Network"
                                SettingRow {
                                    title: "Peer connections"
                                    hint: "Maximum simultaneous peer connections across all sessions. More peers can raise throughput, but every connection costs memory and sockets."
                                    SettingSpinBox {
                                        Accessible.name: "Maximum peer connections"
                                        from: 20; to: 1000; stepSize: 10
                                        value: app.connectionLimit
                                        editable: true
                                        onValueModified: app.connectionLimit = value
                                        textFromValue: value => value.toString()
                                        valueFromText: text => parseInt(text)
                                    }
                                }
                                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.line }
                                SettingRow {
                                    title: "Download cap"
                                    hint: "Limit download bandwidth when you need to leave room for other apps. Zero means unlimited; actual speed depends on peers and your connection."
                                    SettingSpinBox {
                                        Accessible.name: "Download cap in KiB per second"
                                        from: 0; to: 102400; stepSize: 256
                                        value: app.downloadLimitKiB
                                        editable: true
                                        onValueModified: app.downloadLimitKiB = value
                                        textFromValue: value => value === 0 ? "Unlimited" : value + " KiB/s"
                                        valueFromText: text => text === "Unlimited" ? 0 : parseInt(text)
                                    }
                                }
                                SettingRow {
                                    title: "Upload cap"
                                    hint: "Limits upload bandwidth while downloading. A small cap keeps downstream traffic prioritised; some upload keeps peers reciprocating. 0 removes the cap."
                                    SettingSpinBox {
                                        Accessible.name: "Upload cap in KiB per second"
                                        from: 0; to: 102400; stepSize: 16
                                        value: app.uploadLimitKiB
                                        editable: true
                                        onValueModified: app.uploadLimitKiB = value
                                        textFromValue: value => value === 0 ? "Unlimited" : value + " KiB/s"
                                        valueFromText: text => text === "Unlimited" ? 0 : parseInt(text)
                                    }
                                }
                            }

                            SettingsCard {
                                title: "Discovery"
                                SettingRow {
                                    title: "Find peers with DHT"
                                    hint: "Find peers through the distributed hash table. Private torrents keep their own tracker rules."
                                    Switch { checked: app.dhtEnabled; onToggled: app.dhtEnabled = checked; Accessible.name: "DHT peer discovery" }
                                }
                                SettingRow {
                                    title: "Local peer discovery"
                                    hint: "Look for peers on your local network. Disable this on networks where you do not want to announce torrent activity."
                                    Switch { checked: app.localDiscoveryEnabled; onToggled: app.localDiscoveryEnabled = checked; Accessible.name: "Local peer discovery" }
                                }
                                SettingRow {
                                    title: "Automatic port mapping"
                                    hint: "Ask your router to open an incoming peer port using UPnP or NAT-PMP. This is off by default and depends on your router."
                                    Switch { checked: app.portMappingEnabled; onToggled: app.portMappingEnabled = checked; Accessible.name: "Automatic port mapping" }
                                }
                            }

                            SettingsCard {
                                title: "Playback"
                                SettingRow {
                                    title: "External player"
                                    hint: "Choose a media player that supports HTTP streams, such as VLC. BitFly opens it only when you press Play."
                                    detail: app.playerPath.length ? app.playerPath : "Not selected"
                                    QuietButton { text: "Choose player…"; onClicked: vlcDialog.open() }
                                    LinkButton {
                                        Layout.leftMargin: 4
                                        text: "Get VLC"
                                        onClicked: Qt.openUrlExternally("https://www.videolan.org/vlc/")
                                    }
                                }
                                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.line }
                                SettingRow {
                                    title: "Diagnostics"
                                    hint: "Shows cache, read-ahead, peer, and update details on the Library page. Useful when tuning speed or reporting a problem."
                                    detail: app.advanced ? "Visible on the Library page" : "Hidden"
                                    QuietButton {
                                        text: app.advanced ? "Hide" : "Show"
                                        onClicked: app.advanced = !app.advanced
                                    }
                                }
                            }

                            SettingsCard {
                                title: "Look & feel"
                                SettingRow {
                                    title: "Reduce motion"
                                    hint: "Replaces cover unfolding, hover movement, and button springs with immediate state changes."
                                    Switch { checked: app.reducedMotion; onToggled: app.reducedMotion = checked; Accessible.name: "Reduce motion" }
                                }
                                SettingRow {
                                    title: "Video previews"
                                    hint: "Use installed VLC to sample a later video frame, even if another player is selected. Blank frames are skipped; slow streams may need Refresh video preview. Turning this off keeps saved artwork."
                                    Switch { checked: app.generatePreviews; onToggled: app.generatePreviews = checked; Accessible.name: "Generate video previews" }
                                }
                            }

                            SettingsCard {
                                title: "Cloudflare connection"
                                SettingRow {
                                    title: "Cloudflare One Client"
                                    hint: "This separate client changes your device's network connection, including other apps. BitFly does not bundle it or enable it automatically."
                                    detail: app.cloudflareStatus
                                    QuietButton { text: "Refresh"; enabled: !app.cloudflareBusy; onClicked: app.refreshCloudflare() }
                                    QuietButton { text: app.cloudflareInstalled ? "Open client" : "Get client"; onClicked: app.cloudflareInstalled ? app.openCloudflareClient() : app.openCloudflareDownload() }
                                }
                                SettingRow {
                                    visible: app.cloudflareInstalled
                                    title: "Connection"
                                    hint: "Connect or disconnect the installed Cloudflare client. You may need to accept its terms or sign in in its own window first."
                                    QuietButton { text: "Connect"; enabled: !app.cloudflareBusy; onClicked: app.connectCloudflare() }
                                    QuietButton { text: "Disconnect"; enabled: !app.cloudflareBusy; onClicked: app.disconnectCloudflare() }
                                }
                                SettingRow {
                                    visible: app.cloudflareInstalled
                                    title: "Change mode"
                                    hint: "DNS only encrypts DNS lookups; it does not tunnel torrent traffic. Traffic + DNS routes supported device traffic through Cloudflare. Organisation policies can restrict changes."
                                    ComboBox {
                                        id: cloudflareMode
                                        implicitWidth: 210
                                        model: ["DNS · HTTPS", "DNS · TLS", "Traffic + DNS · HTTPS", "Traffic + DNS · TLS", "Traffic only"]
                                        currentIndex: -1
                                        displayText: currentIndex < 0 ? "Choose a mode…" : currentText
                                        enabled: !app.cloudflareBusy
                                        onActivated: index => {
                                            app.setCloudflareMode(["doh", "dot", "warp+doh", "warp+dot", "tunnel_only"][index])
                                            cloudflareMode.currentIndex = -1
                                        }
                                        Accessible.name: "Cloudflare connection mode"
                                    }
                                }
                            }

                            SettingsCard {
                                title: "Updates"
                                SettingRow {
                                    title: "Stable releases"
                                    hint: "BitFly checks at each app launch and downloads available updates in the background. Installers are verified first. Installing saves sessions, closes BitFly and its players, and updates this app folder. Windows may request permission."
                                    detail: app.updateMessage.length ? app.updateMessage : "Checks at each app launch"
                                    QuietButton { text: "Check now"; enabled: !app.updateBusy; onClicked: app.checkUpdates() }
                                    QuietButton { visible: app.availableVersion.length > 0 && !app.updateReady; text: "Download update"; enabled: !app.updateBusy; highlighted: true; onClicked: app.downloadUpdate() }
                                    QuietButton { visible: app.updateReady; text: "Install update"; enabled: !app.updateBusy; highlighted: true; onClicked: app.installUpdate() }
                                }
                                ProgressBar {
                                    Layout.fillWidth: true
                                    visible: app.updateBusy && app.availableVersion.length > 0
                                    value: app.updateProgress
                                    Accessible.name: "Update download progress"
                                }
                            }
                            Item { Layout.preferredHeight: 6 }
                        }
                    }
                }
                SearchPage {
                    controller: app
                    onStreamChosen: window.page = "Library"
                }
            }
        }
    }

    FileDialog {
        id: torrentDialog
        title: "Open torrent file"
        nameFilters: ["Torrent files (*.torrent)"]
        fileMode: FileDialog.OpenFile
        onAccepted: app.openTorrent(selectedFile)
    }
    FileDialog {
        id: artworkDialog
        title: "Choose library artwork"
        nameFilters: ["Images (*.png *.jpg *.jpeg *.webp)"]
        fileMode: FileDialog.OpenFile
        onAccepted: app.setLibraryArtwork(window.pendingArtworkId, selectedFile)
    }
    FileDialog {
        id: vlcDialog
        title: "Select media player executable"
        nameFilters: ["Applications (*.exe)"]
        fileMode: FileDialog.OpenFile
        onAccepted: app.setPlayerPath(selectedFile)
    }
    FolderDialog {
        id: cacheDialog
        title: "Choose download folder"
        onAccepted: app.setCachePath(selectedFolder)
    }

    Dialog {
        id: removeDialog
        title: window.pendingRemovalIsLibrary ? "Remove from library?" : "Remove torrent?"
        modal: true
        anchors.centerIn: parent
        width: 430
        padding: 20
        background: Rectangle {
            color: Theme.raised
            radius: Theme.radiusLarge
            border.color: Theme.line
        }
        contentItem: ColumnLayout {
            spacing: 12
            Label {
                Layout.fillWidth: true
                text: window.pendingRemovalIsLibrary
                      ? "This removes the saved poster and any retained files for this entry. You will need the magnet again to stream it."
                      : "This stops the live session. Cached media can be kept for the library or deleted from disk."
                color: Theme.secondary
                font.pixelSize: 12
                wrapMode: Text.Wrap
            }
            CheckBox {
                id: deleteData
                visible: !window.pendingRemovalIsLibrary
                text: "Also delete downloaded data"
                font.family: Theme.fontFamily
                font.pixelSize: 12
                contentItem: Text {
                    leftPadding: deleteData.indicator.width + 8
                    text: deleteData.text
                    color: Theme.secondary
                    font.family: Theme.fontFamily
                    font.pixelSize: 12
                    verticalAlignment: Text.AlignVCenter
                }
                indicator: Rectangle {
                    implicitWidth: 18; implicitHeight: 18
                    radius: 5
                    color: deleteData.checked ? Theme.accent : Theme.raised
                    border.color: deleteData.checked ? Theme.accent : Theme.line
                    Text {
                        anchors.centerIn: parent
                        visible: deleteData.checked
                        text: "✓"
                        color: Theme.accentInk
                        font.pixelSize: 12
                        font.weight: Font.Bold
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Item { Layout.fillWidth: true }
                QuietButton { text: "Cancel"; onClicked: removeDialog.close() }
                QuietButton {
                    text: "Remove"
                    destructive: true
                    onClicked: {
                        if (window.pendingRemovalIsLibrary)
                            app.removeLibraryEntry(window.pendingRemovalId)
                        else
                            app.remove(window.pendingRemovalId, deleteData.checked)
                        removeDialog.close()
                        deleteData.checked = false
                    }
                }
            }
        }
        onClosed: deleteData.checked = false
    }

    Dialog {
        id: endedDialog
        title: "Playback finished"
        modal: true
        anchors.centerIn: parent
        width: 450
        padding: 20
        background: Rectangle {
            color: Theme.raised
            radius: Theme.radiusLarge
            border.color: Theme.line
        }
        contentItem: ColumnLayout {
            spacing: 12
            Label {
                text: "What should BitFly do with this torrent?"
                color: Theme.ink
                font.pixelSize: 15
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            Label {
                text: "Keep it available in the library, delete its data, or seed for "
                      + (app.seedMinutes === 0 ? "the configured duration (currently off)" : app.seedMinutes + " minutes") + "."
                color: Theme.secondary
                font.pixelSize: 12
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Item { Layout.fillWidth: true }
                QuietButton { text: "Keep"; onClicked: { app.finishSession(window.endedSessionId, "keep"); endedDialog.close() } }
                QuietButton { text: "Delete data"; destructive: true; onClicked: { app.finishSession(window.endedSessionId, "delete"); endedDialog.close() } }
                QuietButton { text: "Seed"; enabled: app.seedMinutes > 0; onClicked: { app.finishSession(window.endedSessionId, "seed"); endedDialog.close() } }
            }
        }
    }

    Connections {
        target: app
        function onPlaybackEnded(id) {
            window.endedSessionId = id
            endedDialog.open()
        }
    }
}
