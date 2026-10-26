import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    property string title
    property string hint
    property string detail
    default property alias content: slot.data
    implicitHeight: Math.max(labels.implicitHeight, slot.implicitHeight) + 8
    Layout.fillWidth: true

    ColumnLayout {
        id: labels
        anchors.left: parent.left
        anchors.right: slot.left
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 1
        RowLayout {
            spacing: 7
            Label {
                text: root.title
                color: Theme.ink
                font.family: Theme.fontFamily
                font.pixelSize: 13
                font.weight: Font.DemiBold
            }
            HelpDot {
                objectName: "help_" + root.title
                visible: root.hint.length > 0
                hint: root.hint
            }
        }
        Label {
            visible: root.detail.length > 0
            Layout.fillWidth: true
            text: root.detail
            color: Theme.secondary
            font.family: Theme.fontFamily
            font.pixelSize: 11
            wrapMode: Text.Wrap
        }
    }
    RowLayout {
        id: slot
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: 6
    }
}
