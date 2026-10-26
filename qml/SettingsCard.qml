import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property string title
    default property alias content: slot.data
    implicitHeight: shell.implicitHeight + 20
    Layout.fillWidth: true
    color: Theme.surface
    radius: Theme.radiusLarge
    border.color: Theme.line
    border.width: 1

    ColumnLayout {
        id: shell
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 10
        spacing: 3

        Label {
            Layout.bottomMargin: 2
            text: root.title
            color: Theme.faint
            font.family: Theme.displayFamily
            font.pixelSize: 19
            font.weight: Font.Normal
        }

        ColumnLayout {
            id: slot
            Layout.fillWidth: true
            spacing: 2
        }
    }
}
