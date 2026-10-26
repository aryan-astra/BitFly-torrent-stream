pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    required property var session
    signal playRequested(string id, int fileIndex)
    signal queueRequested(string id, int fileIndex)
    signal pauseRequested(string id, bool paused)
    signal removeRequested(string id)
    signal revealRequested(string id)
    signal artworkRequested(string id)
    signal refreshRequested(string id)

    property var mediaFiles: (session.files || []).filter(function (file) { return file.playable })
    property int selectedFileIndex: session.selected === undefined ? -1 : session.selected
    readonly property var selectedFile: {
        for (let i = 0; i < mediaFiles.length; ++i)
            if (mediaFiles[i].index === selectedFileIndex) return mediaFiles[i]
        return mediaFiles.length ? mediaFiles[0] : null
    }
    readonly property bool isPaused: String(session.state || "").toLowerCase().indexOf("pause") >= 0
    readonly property bool compact: width < 430
    implicitHeight: body.implicitHeight + 30
    radius: Theme.radiusLarge
    color: Theme.surface
    border.color: Theme.line
    border.width: 1

    function humanSize(bytes) {
        if (!bytes || bytes <= 0) return "—"
        const units = ["B", "KB", "MB", "GB", "TB"]
        let value = bytes, unit = 0
        while (value >= 1024 && unit < units.length - 1) { value /= 1024; ++unit }
        return (unit === 0 ? Math.round(value) : value.toFixed(1)) + " " + units[unit]
    }
    function humanRate(bytesPerSecond) {
        return bytesPerSecond > 0 ? humanSize(bytesPerSecond) + "/s" : "waiting for peers"
    }
    function statusColor(state) {
        const value = String(state || "").toLowerCase()
        if (value.indexOf("error") >= 0) return Theme.danger
        if (value.indexOf("pause") >= 0) return Theme.secondary
        if (value.indexOf("seed") >= 0 || value.indexOf("complete") >= 0) return Theme.success
        return Theme.accent
    }

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 3
        radius: 2
        color: root.statusColor(root.session.state)
        opacity: 0.75
    }

    ColumnLayout {
        id: body
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        anchors.topMargin: 14
        anchors.bottomMargin: 14
        spacing: 8

        MediaCover {
            objectName: "cover_" + root.session.id
            Layout.fillWidth: true
            Layout.preferredHeight: 152
            source: String(root.session.thumbnail || "")
        }

        Label {
            Layout.fillWidth: true
            visible: text.length > 0
            objectName: "previewStatus_" + root.session.id
            text: root.session.previewStatus || ""
            wrapMode: Text.Wrap
            color: Theme.secondary
            font.family: Theme.fontFamily
            font.pixelSize: 12
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: 7

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    spacing: 3
                    Label {
                        Layout.fillWidth: true
                        text: root.session.name || "Resolving torrent…"
                        color: Theme.ink
                        font.family: Theme.fontFamily
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }
                    Label {
                        Layout.fillWidth: true
                        text: root.session.error ? root.session.error
                                                 : ((root.session.peers || 0) + " peers  ·  "
                                                    + root.humanSize(root.session.downloaded) + " of "
                                                    + root.humanSize(root.session.total))
                        color: root.session.error ? Theme.danger : Theme.secondary
                        font.family: Theme.fontFamily
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }
                Rectangle {
                    radius: 9
                    color: Qt.rgba(root.statusColor(root.session.state).r, root.statusColor(root.session.state).g,
                                   root.statusColor(root.session.state).b, 0.14)
                    implicitWidth: stateText.implicitWidth + 18
                    implicitHeight: 23
                    Label {
                        id: stateText
                        anchors.centerIn: parent
                        text: root.session.state || "Resolving"
                        color: root.statusColor(root.session.state)
                        font.family: Theme.fontFamily
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                implicitHeight: 5
                Rectangle {
                    anchors.fill: parent
                    radius: 3
                    color: Theme.line
                }
                Rectangle {
                    width: parent.width * Math.max(0, Math.min(1, Number(root.session.progress || 0)))
                    height: parent.height
                    radius: 3
                    color: Theme.accent
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Label {
                    text: Math.round(Number(root.session.progress || 0) * 100) + "%"
                    color: Theme.ink
                    font.family: Theme.fontFamily
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                }
                Label {
                    text: "·"
                    color: Theme.faint
                    font.pixelSize: 12
                }
                Label {
                    text: root.humanRate(Number(root.session.rate || 0))
                    color: Theme.secondary
                    font.family: Theme.fontFamily
                    font.pixelSize: 12
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: "buffer " + Math.round(Number(root.session.buffer || 0) * 100) + "%"
                    color: Theme.faint
                    font.family: Theme.fontFamily
                    font.pixelSize: 12
                }
            }

            ComboBox {
                id: fileChoice
                objectName: "fileChoice_" + root.session.id
                Accessible.name: "Media file in " + (root.session.name || "torrent")
                Layout.fillWidth: true
                implicitHeight: 32
                model: root.mediaFiles
                textRole: "name"
                currentIndex: {
                    for (let i = 0; i < root.mediaFiles.length; ++i)
                        if (root.mediaFiles[i].index === root.selectedFileIndex) return i
                    return root.mediaFiles.length ? 0 : -1
                }
                onActivated: index => { if (index >= 0) root.selectedFileIndex = root.mediaFiles[index].index }
                enabled: root.mediaFiles.length > 0
                font.family: Theme.fontFamily
                font.pixelSize: 12
                delegate: ItemDelegate {
                    id: fileDelegate
                    required property var modelData
                    width: fileChoice.width
                    hoverEnabled: true
                    font.family: Theme.fontFamily
                    font.pixelSize: 12
                    contentItem: RowLayout {
                        Label {
                            text: fileDelegate.modelData ? fileDelegate.modelData.name : ""
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                            color: Theme.ink
                            font.family: Theme.fontFamily
                            font.pixelSize: 12
                        }
                        Label {
                            text: root.humanSize(fileDelegate.modelData ? fileDelegate.modelData.size : 0)
                            color: Theme.faint
                            font.family: Theme.fontFamily
                            font.pixelSize: 12
                        }
                    }
                    background: Rectangle {
                        color: fileDelegate.hovered ? Theme.raisedHover : "transparent"
                        radius: Theme.radiusSmall
                    }
                }
                contentItem: Text {
                    leftPadding: 11
                    rightPadding: fileChoice.indicator.width + 8
                    text: fileChoice.displayText || (root.mediaFiles.length ? root.mediaFiles[0].name : "No playable media found")
                    color: root.mediaFiles.length ? Theme.ink : Theme.faint
                    font: fileChoice.font
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideMiddle
                }
                indicator: Text {
                    x: fileChoice.width - width - 10
                    y: fileChoice.topPadding + (fileChoice.availableHeight - height) / 2
                    text: "⌄"
                    color: Theme.faint
                    font.pixelSize: 14
                }
                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: Theme.raised
                    border.color: fileChoice.activeFocus ? Theme.accent : Theme.line
                }
                popup: Popup {
                    y: fileChoice.height + 4
                    width: fileChoice.width
                    implicitHeight: Math.min(fileList.contentHeight + 8, 280)
                    padding: 4
                    contentItem: ListView {
                        id: fileList
                        clip: true
                        implicitHeight: contentHeight
                        model: fileChoice.delegateModel
                        currentIndex: fileChoice.highlightedIndex
                        boundsBehavior: Flickable.StopAtBounds
                    }
                    background: Rectangle {
                        color: Theme.raised
                        radius: Theme.radiusSmall
                        border.color: Theme.line
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 7
                QuietButton {
                    text: root.session.vlcRunning ? "Player open" : "Play"
                    highlighted: true
                    implicitHeight: 30
                    enabled: !!root.selectedFile && !!root.session.id && !root.session.vlcRunning
                    onClicked: root.playRequested(root.session.id, root.selectedFile ? root.selectedFile.index : -1)
                }
                QuietButton {
                    text: root.isPaused ? "Continue torrent" : "Pause"
                    implicitHeight: 30
                    enabled: !!root.session.id && !root.session.vlcRunning
                    onClicked: root.pauseRequested(root.session.id, !root.isPaused)
                }
                QuietButton {
                    visible: !root.compact
                    text: "Queue"
                    implicitHeight: 30
                    enabled: !!root.selectedFile && !!root.session.id
                    onClicked: root.queueRequested(root.session.id, root.selectedFile ? root.selectedFile.index : -1)
                }
                Item { Layout.fillWidth: true }
                IconButton {
                    glyph: "⋯"
                    hint: "Artwork and files"
                    onClicked: mediaMenu.popup()
                }
                IconButton {
                    glyph: "✕"
                    hint: "Remove this session"
                    destructive: true
                    onClicked: root.removeRequested(root.session.id)
                }
            }
        }
    }

    Accessible.role: Accessible.Grouping
    Accessible.name: root.session.name || "Torrent session"
    Menu {
        id: mediaMenu
        Action { text: "Choose artwork…"; onTriggered: root.artworkRequested(root.session.id) }
        Action { text: "Refresh video preview"; onTriggered: root.refreshRequested(root.session.id) }
        Action { text: "Show downloaded files"; onTriggered: root.revealRequested(root.session.id) }
        Action { text: "Add to queue"; enabled: !!root.selectedFile; onTriggered: root.queueRequested(root.session.id, root.selectedFile ? root.selectedFile.index : -1) }
    }
}
