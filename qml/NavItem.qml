import QtQuick
import QtQuick.Controls

ItemDelegate {
    id: root
    required property string label
    required property string iconName
    property bool selected: false
    implicitHeight: 48
    hoverEnabled: true
    leftPadding: 12
    rightPadding: 10
    Accessible.name: root.label
    Accessible.selected: root.selected
    background: Rectangle {
        radius: Theme.radius
        color: root.selected ? Theme.accentWash : (root.hovered ? Theme.raised : "transparent")
        border.width: root.activeFocus ? 1 : 0
        border.color: Theme.accent
        Rectangle {
            visible: root.selected
            width: 3
            height: parent.height - 14
            radius: 2
            color: Theme.accent
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
        }
    }
    contentItem: Row {
        spacing: 9
        NavIcon {
            name: root.iconName
            color: root.selected ? Theme.accent : Theme.faint
            anchors.verticalCenter: parent.verticalCenter
            rotation: root.down ? 5 : (root.hovered && !Theme.reducedMotion ? -6 : 0)
            scale: root.down ? 0.92 : (root.hovered && !Theme.reducedMotion ? 1.08 : 1)
            Behavior on rotation { NumberAnimation { duration: Theme.fastMotion; easing.type: Easing.OutCubic } }
            Behavior on scale { NumberAnimation { duration: Theme.fastMotion; easing.type: Easing.OutCubic } }
        }
        Text {
            text: root.label
            color: root.selected ? Theme.ink : Theme.secondary
            font.family: Theme.fontFamily
            font.pixelSize: 14
            font.weight: root.selected ? Font.DemiBold : Font.Normal
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
