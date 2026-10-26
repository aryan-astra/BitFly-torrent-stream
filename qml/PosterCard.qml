pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    required property var entry
    signal playRequested(string id)
    signal removeRequested(string id)
    signal revealRequested(string id)
    signal artworkRequested(string id)
    signal refreshRequested(string id)
    readonly property var item: root.entry || ({})
    readonly property string itemId: String(root.item.id || "")
    readonly property string title: String(root.item.name || "Untitled torrent")
    readonly property bool available: root.item.active === true || root.item.available === true
    readonly property bool hovered: hover.hovered
    implicitWidth: 200
    implicitHeight: width + 94
    radius: Theme.radiusLarge
    color: Theme.surface
    border.color: root.hovered ? Theme.accent : Theme.line
    Behavior on border.color { ColorAnimation { duration: Theme.fastMotion } }
    scale: root.hovered && !Theme.reducedMotion ? 1.012 : 1
    Behavior on scale { NumberAnimation { duration: Theme.fastMotion; easing.type: Easing.OutCubic } }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 7
        MediaCover {
            Layout.fillWidth: true
            Layout.preferredHeight: root.width - 30
            source: String(root.item.thumbnail || "")
            dimmed: !root.available
        }
        Label {
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            text: root.title
            color: Theme.ink
            font.family: Theme.fontFamily
            font.pixelSize: 14
            font.weight: Font.DemiBold
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideRight
        }
        Label {
            Layout.fillWidth: true
            text: root.available ? "Files on this device" : "Saved link · download again"
            color: Theme.secondary
            font.family: Theme.fontFamily
            font.pixelSize: 12
            elide: Text.ElideRight
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 4
            QuietButton {
                Layout.fillWidth: true
                text: "Stream again"
                implicitHeight: 30
                onClicked: root.playRequested(root.itemId)
            }
            IconButton {
                glyph: "⋯"
                hint: "Artwork and files"
                onClicked: artworkMenu.popup()
            }
            IconButton {
                glyph: "×"
                hint: "Remove this library entry"
                destructive: true
                onClicked: root.removeRequested(root.itemId)
            }
        }
    }
    HoverHandler { id: hover }
    Menu {
        id: artworkMenu
        Action { text: "Choose artwork…"; onTriggered: root.artworkRequested(root.itemId) }
        Action { text: "Refresh video preview"; enabled: root.item.active === true; onTriggered: root.refreshRequested(root.itemId) }
        Action { text: "Show downloaded files"; enabled: root.available; onTriggered: root.revealRequested(root.itemId) }
    }
    Accessible.role: Accessible.Grouping
    Accessible.name: root.title + (root.available ? ", files on device" : ", saved link")
}
