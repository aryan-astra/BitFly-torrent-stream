import QtQuick
import QtQuick.Controls

ItemDelegate {
    id: root
    required property string label
    required property string iconName
    property bool selected: false
    implicitHeight: 38
    hoverEnabled: true
    leftPadding: 11
    rightPadding: 10
    background: Rectangle {
        radius: 5
        color: root.selected ? "#343a36" : (root.hovered ? "#2b302d" : "transparent")
        Rectangle {
            visible: root.selected
            width: 2
            radius: 1
            color: "#d7e4dc"
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
        }
    }
    contentItem: Row {
        spacing: 11
        NavIcon {
            name: root.iconName
            color: root.selected ? "#f3f5f3" : "#a5aca7"
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            text: root.label
            color: root.selected ? "#ffffff" : "#c9ceca"
            font.family: Theme.fontFamily
            font.pixelSize: 13
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
