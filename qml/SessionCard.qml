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

    property var mediaFiles: (session.files || []).filter(function (file) { return file.playable })
    property int selectedFileIndex: session.selected === undefined ? -1 : session.selected
    readonly property var selectedFile: {
        for (let i = 0; i < mediaFiles.length; ++i)
            if (mediaFiles[i].index === selectedFileIndex) return mediaFiles[i]
        return mediaFiles.length ? mediaFiles[0] : null
    }
    readonly property bool isPaused: String(session.state || "").toLowerCase().indexOf("pause") >= 0
    implicitHeight: body.implicitHeight + 32
    radius: Theme.radius + 1
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
        return bytesPerSecond > 0 ? humanSize(bytesPerSecond) + "/s" : "Waiting for peers"
    }
    function statusColor(state) {
        const value = String(state || "").toLowerCase()
        if (value.indexOf("error") >= 0) return Theme.danger
        if (value.indexOf("pause") >= 0) return Theme.secondary
        if (value.indexOf("seed") >= 0 || value.indexOf("complete") >= 0) return Theme.accent
        return Theme.warning
    }

    ColumnLayout {
        id: body
        anchors.fill: parent
        anchors.margins: 16
        spacing: 11

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3
                Label {
                    Layout.fillWidth: true
                    text: root.session.name || "Resolving torrent…"
                    color: Theme.ink
                    font.family: Theme.fontFamily
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    elide: Text.ElideMiddle
                }
                Label {
                    text: root.session.error ? root.session.error : ((root.session.peers || 0) + " peers  ·  " + root.humanSize(root.session.downloaded) + " of " + root.humanSize(root.session.total))
                    color: root.session.error ? Theme.danger : Theme.secondary
                    font.family: Theme.fontFamily
                    font.pixelSize: 11
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
            Rectangle {
                radius: 10
                color: Qt.rgba(root.statusColor(root.session.state).r, root.statusColor(root.session.state).g, root.statusColor(root.session.state).b, 0.12)
                implicitWidth: stateText.implicitWidth + 16
                implicitHeight: 24
                Label {
                    id: stateText
                    anchors.centerIn: parent
                    text: root.session.state || "Resolving"
                    color: root.statusColor(root.session.state)
                    font.family: Theme.fontFamily
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                }
            }
        }

        ProgressBar {
            Layout.fillWidth: true
            implicitHeight: 5
            from: 0
            to: 1
            id: progressBar
            value: Math.max(0, Math.min(1, Number(root.session.progress || 0)))
            background: Rectangle { radius: 3; color: "#edf0ed" }
            contentItem: Item {
                Rectangle { width: parent.width * progressBar.visualPosition; height: parent.height; radius: 3; color: Theme.accent }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Label {
                text: Math.round(Number(root.session.progress || 0) * 100) + "%"
                color: Theme.secondary
                font.family: Theme.fontFamily
                font.pixelSize: 11
                Layout.preferredWidth: 38
            }
            Rectangle {
                Layout.preferredWidth: 5; Layout.preferredHeight: 5; radius: 3; color: Theme.line
            }
            Label {
                text: root.humanRate(Number(root.session.rate || 0))
                color: Theme.secondary
                font.family: Theme.fontFamily
                font.pixelSize: 11
                Layout.fillWidth: true
            }
            Label {
                text: "Buffer " + Math.round(Number(root.session.buffer || 0) * 100) + "%"
                color: Theme.secondary
                font.family: Theme.fontFamily
                font.pixelSize: 11
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Label {
                text: "Media"
                color: Theme.secondary
                font.family: Theme.fontFamily
                font.pixelSize: 11
            }
            ComboBox {
                id: fileChoice
                Layout.fillWidth: true
                Layout.minimumWidth: 100
                Layout.maximumWidth: root.width < 620 ? 185 : 300
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
                font.pixelSize: 11
                delegate: ItemDelegate {
                    id: fileDelegate
                    required property string name
                    required property var size
                    width: fileChoice.width
                    text: name
                    font.family: Theme.fontFamily
                    font.pixelSize: 11
                    contentItem: RowLayout {
                        Label { text: fileDelegate.name; elide: Text.ElideMiddle; Layout.fillWidth: true; color: Theme.ink; font.family: Theme.fontFamily; font.pixelSize: 11 }
                        Label { text: root.humanSize(fileDelegate.size); color: Theme.secondary; font.family: Theme.fontFamily; font.pixelSize: 10 }
                    }
                }
                contentItem: Text {
                    leftPadding: 10; rightPadding: fileChoice.indicator.width + 8
                    text: fileChoice.displayText || (root.mediaFiles.length ? root.mediaFiles[0].name : "No playable media found")
                    color: root.mediaFiles.length ? Theme.ink : Theme.faint
                    font: fileChoice.font
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideMiddle
                }
                background: Rectangle { radius: Theme.radius; color: "#ffffff"; border.color: Theme.line }
            }
            Item { Layout.fillWidth: true }
            QuietButton {
                text: root.isPaused ? "Resume" : "Pause"
                enabled: !!root.session.id
                onClicked: root.pauseRequested(root.session.id, !root.isPaused)
            }
            QuietButton {
                text: "Queue"
                enabled: !!root.selectedFile && !!root.session.id
                onClicked: root.queueRequested(root.session.id, root.selectedFile ? root.selectedFile.index : -1)
            }
            QuietButton {
                text: "Play in VLC"
                highlighted: true
                enabled: !!root.selectedFile && !!root.session.id
                onClicked: root.playRequested(root.session.id, root.selectedFile ? root.selectedFile.index : -1)
            }
            ToolButton {
                text: "⋯"
                Accessible.name: "Remove torrent"
                implicitWidth: 32
                implicitHeight: 32
                font.pixelSize: 19
                onClicked: removeMenu.open()
                Menu {
                    id: removeMenu
                    MenuItem { text: "Remove torrent…"; onTriggered: root.removeRequested(root.session.id) }
                }
            }
        }
    }
}
