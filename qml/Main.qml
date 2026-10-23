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
    width: 1080
    height: 720
    minimumWidth: 780
    minimumHeight: 540
    visible: true
    title: "BitFly"
    color: Theme.window
    font.family: Theme.fontFamily

    property string page: "Stream"
    property string pendingRemovalId: ""
    property string endedSessionId: ""

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
    function syncSessions() {
        const incoming = app.sessions
        const incomingIds = new Set()
        for (let i = 0; i < incoming.length; ++i)
            incomingIds.add(String(incoming[i].id || ""))

        // Keep each ListModel row keyed by torrent id. Replacing app.sessions directly
        // destroys every delegate on each backend refresh, stealing focus and resetting
        // the user's selected media file.
        for (let i = activeSessions.count - 1; i >= 0; --i) {
            if (!incomingIds.has(activeSessions.get(i).sessionId))
                activeSessions.remove(i)
        }
        for (let target = 0; target < incoming.length; ++target) {
            const item = incoming[target]
            const key = String(item.id || "")
            let existing = -1
            for (let i = target; i < activeSessions.count; ++i) {
                if (activeSessions.get(i).sessionId === key) { existing = i; break }
            }
            if (existing < 0) {
                activeSessions.insert(target, { sessionId: key, sessionData: item })
            } else {
                if (existing !== target) activeSessions.move(existing, target, 1)
                activeSessions.setProperty(target, "sessionData", item)
            }
        }
    }

    ListModel {
        id: activeSessions
        objectName: "activeSessions"
        dynamicRoles: true
    }
    Component.onCompleted: window.syncSessions()
    Connections {
        target: app
        function onChanged() { window.syncSessions() }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: 194
            Layout.fillHeight: true
            color: Theme.rail
            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: 13
                anchors.rightMargin: 13
                anchors.topMargin: 20
                anchors.bottomMargin: 13
                spacing: 0
                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 7
                    Layout.bottomMargin: 30
                    spacing: 9
                    Rectangle {
                        Layout.preferredWidth: 27; Layout.preferredHeight: 27; radius: 7; color: "#e5ece7"
                        Label { anchors.centerIn: parent; text: "B"; color: Theme.rail; font.pixelSize: 16; font.weight: Font.Bold }
                    }
                    Label { text: "BitFly"; color: "#f4f6f4"; font.pixelSize: 17; font.weight: Font.DemiBold }
                }
                Label {
                    text: "LIBRARY"
                    color: "#777f79"
                    font.family: Theme.fontFamily
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                    font.letterSpacing: 1.2
                    Layout.leftMargin: 9
                    Layout.bottomMargin: 9
                }
                NavItem { objectName: "navStream"; label: "Stream"; iconName: "stream"; selected: window.page === "Stream"; Layout.fillWidth: true; onClicked: window.page = "Stream" }
                NavItem { objectName: "navQueue"; label: "Queue"; iconName: "queue"; selected: window.page === "Queue"; Layout.fillWidth: true; onClicked: window.page = "Queue" }
                NavItem { objectName: "navHistory"; label: "History"; iconName: "history"; selected: window.page === "History"; Layout.fillWidth: true; onClicked: window.page = "History" }
                Item { Layout.fillHeight: true }
                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#373d39"; Layout.bottomMargin: 12 }
                NavItem { objectName: "navSettings"; label: "Settings"; iconName: "settings"; selected: window.page === "Settings"; Layout.fillWidth: true; onClicked: window.page = "Settings" }
                Label {
                    Layout.leftMargin: 10
                    Layout.topMargin: 10
                    text: "LOCAL STREAMING"
                    color: "#777f79"
                    font.pixelSize: 9
                    font.weight: Font.DemiBold
                    font.letterSpacing: 0.8
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 57
                color: Theme.surface
                border.color: Theme.line
                border.width: 1
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 27
                    anchors.rightMargin: 24
                    Label {
                        text: window.page
                        color: Theme.ink
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                    }
                    Item { Layout.fillWidth: true }
                    Label {
                        visible: app.vlcPath.length > 0
                        text: "VLC ready"
                        color: Theme.secondary
                        font.pixelSize: 11
                    }
                    Rectangle { visible: app.vlcPath.length > 0; Layout.preferredWidth: 6; Layout.preferredHeight: 6; radius: 3; color: Theme.accent }
                }
            }

            StackLayout {
                id: pages
                currentIndex: window.page === "Stream" ? 0 : window.page === "Queue" ? 1 : window.page === "History" ? 2 : 3
                Layout.fillWidth: true
                Layout.fillHeight: true

                // Stream page
                Item {
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 27
                        anchors.rightMargin: 27
                        anchors.topMargin: 21
                        anchors.bottomMargin: 18
                        spacing: 14

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            TextField {
                                id: input
                                Layout.fillWidth: true
                                implicitHeight: 39
                                placeholderText: "Paste a magnet link"
                                selectByMouse: true
                                font.pixelSize: 12
                                onAccepted: addButton.clicked()
                                background: Rectangle { radius: Theme.radius; color: Theme.surface; border.color: input.activeFocus ? Theme.accent : Theme.line; border.width: input.activeFocus ? 2 : 1 }
                            }
                            QuietButton { text: "Open .torrent"; onClicked: torrentDialog.open() }
                            QuietButton {
                                id: addButton
                                text: "Add stream"
                                highlighted: true
                                enabled: input.text.trim().length > 0
                                onClicked: {
                                    app.openInput(input.text.trim())
                                    input.clear()
                                }
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: "BitTorrent shares your IP address with peers and can upload downloaded pieces."
                            color: Theme.secondary
                            font.pixelSize: 11
                            wrapMode: Text.Wrap
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Label {
                                text: "Active sessions"
                                color: Theme.ink
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                            }
                            Label {
                                text: app.sessions.length ? app.sessions.length + " active" : ""
                                color: Theme.secondary
                                font.pixelSize: 11
                            }
                            Item { Layout.fillWidth: true }
                            ToolButton {
                                text: "Diagnostics  " + (app.advanced ? "⌃" : "⌄")
                                font.pixelSize: 11
                                onClicked: app.advanced = !app.advanced
                                Accessible.name: app.advanced ? "Hide diagnostics" : "Show diagnostics"
                            }
                        }

                        ScrollView {
                            id: sessionScroll
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                            ScrollBar.vertical: ScrollBar {
                                policy: ScrollBar.AsNeeded
                                background: Rectangle { color: "transparent" }
                                contentItem: Rectangle { implicitWidth: 6; radius: 3; color: "#c9cfcb" }
                            }
                            ColumnLayout {
                                width: sessionScroll.availableWidth
                                spacing: 10
                                Repeater {
                                    objectName: "sessionRepeater"
                                    model: activeSessions
                                    delegate: SessionCard {
                                        required property string sessionId
                                        required property var sessionData
                                        objectName: "sessionCard_" + sessionId
                                        Layout.fillWidth: true
                                        session: sessionData
                                        onPlayRequested: (id, fileIndex) => app.play(id, fileIndex)
                                        onQueueRequested: (id, fileIndex) => app.enqueue(id, fileIndex)
                                        onPauseRequested: (id, paused) => app.pause(id, paused)
                                        onRemoveRequested: id => { window.pendingRemovalId = id; removeDialog.open() }
                                    }
                                }
                                Rectangle {
                                    visible: app.sessions.length === 0
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 182
                                    color: Theme.surface
                                    border.color: Theme.line
                                    radius: Theme.radius + 1
                                    Column {
                                        anchors.centerIn: parent
                                        spacing: 7
                                        Label { anchors.horizontalCenter: parent.horizontalCenter; text: "No active streams"; color: Theme.ink; font.pixelSize: 14; font.weight: Font.DemiBold }
                                        Label { anchors.horizontalCenter: parent.horizontalCenter; text: "Paste a magnet link above or open a .torrent file to begin."; color: Theme.secondary; font.pixelSize: 11 }
                                    }
                                }
                                Rectangle {
                                    visible: app.advanced
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 40
                                    color: "#eef1ee"
                                    radius: Theme.radius
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 12
                                        anchors.rightMargin: 12
                                        spacing: 14
                                        Label { text: "Diagnostics"; color: Theme.ink; font.pixelSize: 11; font.weight: Font.DemiBold }
                                        Label { text: "Cache  " + window.humanSize(app.cacheBytes) + " / " + app.cacheLimitGiB + " GiB"; color: Theme.secondary; font.pixelSize: 11 }
                                        Label { visible: app.updateMessage.length > 0; text: app.updateMessage; color: Theme.secondary; font.pixelSize: 11; elide: Text.ElideRight; Layout.fillWidth: true; Layout.minimumWidth: 0 }
                                    }
                                }
                            }
                        }

                        RowLayout {
                            visible: app.error.length > 0
                            Layout.fillWidth: true
                            Label {
                                Layout.fillWidth: true
                                text: app.error
                                color: Theme.danger
                                font.pixelSize: 11
                                wrapMode: Text.Wrap
                            }
                            QuietButton { text: "Dismiss"; onClicked: app.clearError() }
                        }
                    }
                }

                // Queue page
                Item {
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 27
                        spacing: 14
                        Label { text: "Play next"; color: Theme.ink; font.pixelSize: 13; font.weight: Font.DemiBold }
                        ScrollView {
                            id: queueScroll
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                            ScrollBar.vertical: ScrollBar {
                                policy: ScrollBar.AsNeeded
                                background: Rectangle { color: "transparent" }
                                contentItem: Rectangle { implicitWidth: 6; radius: 3; color: "#c9cfcb" }
                            }
                            ColumnLayout {
                                width: queueScroll.availableWidth
                                spacing: 8
                                Repeater {
                                    model: app.queue
                                    delegate: Rectangle {
                                        required property int index
                                        required property var modelData
                                        Layout.fillWidth: true
                                        implicitHeight: 58
                                        color: Theme.surface
                                        border.color: Theme.line
                                        radius: Theme.radius
                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: 13
                                            anchors.rightMargin: 8
                                            Label { text: (index + 1).toString().padStart(2, "0"); color: Theme.faint; font.pixelSize: 11; Layout.preferredWidth: 22 }
                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                spacing: 2
                                                Label { text: window.displayName(modelData); color: Theme.ink; font.pixelSize: 12; elide: Text.ElideMiddle; Layout.fillWidth: true; Layout.minimumWidth: 0 }
                                                Label { visible: window.displayFile(modelData).length > 0; text: window.displayFile(modelData); color: Theme.secondary; font.pixelSize: 10; elide: Text.ElideMiddle; Layout.fillWidth: true; Layout.minimumWidth: 0 }
                                            }
                                            ToolButton { text: "↑"; enabled: index > 0; Accessible.name: "Move up"; onClicked: app.moveQueue(index, index - 1) }
                                            ToolButton { text: "↓"; enabled: index < app.queue.length - 1; Accessible.name: "Move down"; onClicked: app.moveQueue(index, index + 1) }
                                            ToolButton { text: "×"; Accessible.name: "Remove from queue"; onClicked: app.removeQueue(index) }
                                            QuietButton {
                                                text: "Play"
                                                highlighted: true
                                                enabled: modelData.id !== undefined && modelData.fileIndex !== undefined
                                                onClicked: app.playQueued(index)
                                            }
                                        }
                                    }
                                }
                                Rectangle {
                                    visible: app.queue.length === 0
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 150
                                    color: Theme.surface
                                    border.color: Theme.line
                                    radius: Theme.radius + 1
                                    Column {
                                        anchors.centerIn: parent
                                        spacing: 6
                                        Label { anchors.horizontalCenter: parent.horizontalCenter; text: "Your queue is empty"; color: Theme.ink; font.pixelSize: 13; font.weight: Font.DemiBold }
                                        Label { anchors.horizontalCenter: parent.horizontalCenter; text: "Add media from an active stream with Queue."; color: Theme.secondary; font.pixelSize: 11 }
                                    }
                                }
                            }
                        }
                    }
                }

                // History page
                Item {
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 27
                        spacing: 12
                        Label { text: "Recently streamed"; color: Theme.ink; font.pixelSize: 13; font.weight: Font.DemiBold }
                        ScrollView {
                            id: historyScroll
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                            ScrollBar.vertical: ScrollBar {
                                policy: ScrollBar.AsNeeded
                                background: Rectangle { color: "transparent" }
                                contentItem: Rectangle { implicitWidth: 6; radius: 3; color: "#c9cfcb" }
                            }
                            ColumnLayout {
                                width: historyScroll.availableWidth
                                spacing: 8
                                Repeater {
                                    model: app.history
                                    delegate: Rectangle {
                                        required property var modelData
                                        Layout.fillWidth: true
                                        implicitHeight: 58
                                        color: Theme.surface
                                        border.color: Theme.line
                                        radius: Theme.radius
                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: 13
                                            anchors.rightMargin: 12
                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                spacing: 2
                                                Label { text: window.displayName(modelData); color: Theme.ink; font.pixelSize: 12; elide: Text.ElideMiddle; Layout.fillWidth: true; Layout.minimumWidth: 0 }
                                                Label { text: window.displayFile(modelData); color: Theme.secondary; font.pixelSize: 10; elide: Text.ElideMiddle; Layout.fillWidth: true; Layout.minimumWidth: 0 }
                                            }
                                            QuietButton { text: "Stream again"; enabled: modelData.id !== undefined; onClicked: app.relaunch(modelData.id) }
                                        }
                                    }
                                }
                                Rectangle {
                                    visible: app.history.length === 0
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 150
                                    color: Theme.surface
                                    border.color: Theme.line
                                    radius: Theme.radius + 1
                                    Column {
                                        anchors.centerIn: parent
                                        spacing: 6
                                        Label { anchors.horizontalCenter: parent.horizontalCenter; text: "Nothing here yet"; color: Theme.ink; font.pixelSize: 13; font.weight: Font.DemiBold }
                                        Label { anchors.horizontalCenter: parent.horizontalCenter; text: "Streams you play will appear here."; color: Theme.secondary; font.pixelSize: 11 }
                                    }
                                }
                            }
                        }
                    }
                }

                // Settings page
                Item {
                    ScrollView {
                        id: settingsScroll
                        anchors.fill: parent
                        anchors.leftMargin: 27
                        anchors.rightMargin: 27
                        anchors.topMargin: 23
                        anchors.bottomMargin: 20
                        clip: true
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                            background: Rectangle { color: "transparent" }
                            contentItem: Rectangle { implicitWidth: 6; radius: 3; color: "#c9cfcb" }
                        }
                        ColumnLayout {
                            width: settingsScroll.availableWidth
                            spacing: 20
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 5
                                Label { text: "Playback"; color: Theme.ink; font.pixelSize: 13; font.weight: Font.DemiBold }
                                Label { text: "BitFly hands playback to the VLC installation on this PC."; color: Theme.secondary; font.pixelSize: 11; wrapMode: Text.Wrap; Layout.fillWidth: true }
                                RowLayout {
                                    Layout.fillWidth: true
                                    Label { text: app.vlcPath.length ? app.vlcPath : "VLC executable not selected"; color: app.vlcPath.length ? Theme.secondary : Theme.warning; font.pixelSize: 11; elide: Text.ElideMiddle; Layout.fillWidth: true; Layout.minimumWidth: 0 }
                                    QuietButton { text: "Choose VLC…"; onClicked: vlcDialog.open() }
                                }
                                LinkButton {
                                    text: "Download VLC from VideoLAN"
                                    Layout.alignment: Qt.AlignLeft
                                    Accessible.name: "Download VLC from VideoLAN"
                                    onClicked: Qt.openUrlExternally("https://www.videolan.org/vlc/")
                                }
                            }
                            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.line }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 5
                                Label { text: "Cache"; color: Theme.ink; font.pixelSize: 13; font.weight: Font.DemiBold }
                                Label { text: "Downloaded data is stored locally and bounded by the limit below."; color: Theme.secondary; font.pixelSize: 11; wrapMode: Text.Wrap; Layout.fillWidth: true }
                                RowLayout {
                                    Layout.fillWidth: true
                                    Label { text: app.cachePath || "Default cache location"; color: Theme.secondary; font.pixelSize: 11; elide: Text.ElideMiddle; Layout.fillWidth: true; Layout.minimumWidth: 0 }
                                    QuietButton { text: "Choose folder…"; onClicked: cacheDialog.open() }
                                }
                                RowLayout {
                                    spacing: 10
                                    Label { text: "Cache limit"; color: Theme.ink; font.pixelSize: 11 }
                                    SettingSpinBox {
                                        from: 1; to: 512; stepSize: 1
                                        value: app.cacheLimitGiB
                                        editable: true
                                        implicitHeight: 32
                                        onValueModified: app.cacheLimitGiB = value
                                        textFromValue: value => value + " GiB"
                                        valueFromText: text => parseInt(text)
                                    }
                                    Label { text: "In use: " + window.humanSize(app.cacheBytes); color: Theme.secondary; font.pixelSize: 11 }
                                }
                            }
                            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.line }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 6
                                Label { text: "Seeding"; color: Theme.ink; font.pixelSize: 13; font.weight: Font.DemiBold }
                                Label { text: "After playback, BitFly can seed briefly to help other peers. BitTorrent shares your IP address with the swarm."; color: Theme.secondary; font.pixelSize: 11; wrapMode: Text.Wrap; Layout.fillWidth: true }
                                RowLayout {
                                    Label { text: "Seed for"; color: Theme.ink; font.pixelSize: 11 }
                                    SettingSpinBox {
                                        from: 1; to: 1440; stepSize: 5
                                        value: app.seedMinutes
                                        editable: true
                                        implicitHeight: 32
                                        onValueModified: app.seedMinutes = value
                                        textFromValue: value => value + " min"
                                        valueFromText: text => parseInt(text)
                                    }
                                }
                            }
                            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.line }
                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: "Updates"; color: Theme.ink; font.pixelSize: 13; font.weight: Font.DemiBold }
                                Item { Layout.fillWidth: true }
                                Label { visible: app.updateMessage.length > 0; text: app.updateMessage; color: Theme.secondary; font.pixelSize: 11; elide: Text.ElideRight; Layout.maximumWidth: 240 }
                                QuietButton { text: "Check for updates"; onClicked: app.checkUpdates() }
                            }
                        }
                    }
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
        id: vlcDialog
        title: "Select VLC executable"
        nameFilters: ["VLC executable (vlc.exe)", "Applications (*.exe)"]
        fileMode: FileDialog.OpenFile
        onAccepted: app.setVlcPath(selectedFile)
    }
    FolderDialog {
        id: cacheDialog
        title: "Choose cache folder"
        onAccepted: app.setCachePath(selectedFolder)
    }
    Dialog {
        id: removeDialog
        title: "Remove torrent?"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Cancel
        width: 390
        contentItem: ColumnLayout {
            spacing: 12
            Label { text: "Remove this torrent from BitFly?"; color: Theme.ink; font.pixelSize: 13; wrapMode: Text.Wrap; Layout.fillWidth: true }
            CheckBox { id: deleteData; text: "Also delete downloaded data"; checked: false; font.pixelSize: 11 }
            QuietButton { text: "Remove"; destructive: true; Layout.alignment: Qt.AlignRight; onClicked: { app.remove(window.pendingRemovalId, deleteData.checked); removeDialog.close(); deleteData.checked = false } }
        }
        onClosed: deleteData.checked = false
    }
    Dialog {
        id: endedDialog
        title: "Playback finished"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.NoButton
        width: 410
        contentItem: ColumnLayout {
            spacing: 12
            Label { text: "What would you like to do with this torrent?"; color: Theme.ink; font.pixelSize: 13; wrapMode: Text.Wrap; Layout.fillWidth: true }
            Label { text: "Keep it available, remove its data, or seed for " + app.seedMinutes + " minutes."; color: Theme.secondary; font.pixelSize: 11; wrapMode: Text.Wrap; Layout.fillWidth: true }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                QuietButton { text: "Keep"; onClicked: { app.finishSession(window.endedSessionId, "keep"); endedDialog.close() } }
                QuietButton { text: "Delete data"; destructive: true; onClicked: { app.finishSession(window.endedSessionId, "delete"); endedDialog.close() } }
                QuietButton { text: "Seed"; highlighted: true; onClicked: { app.finishSession(window.endedSessionId, "seed"); endedDialog.close() } }
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
