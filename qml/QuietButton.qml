import QtQuick
import QtQuick.Controls

Button {
    id: root
    property bool destructive: false
    implicitHeight: 32
    leftPadding: 12
    rightPadding: 12
    font.family: Theme.fontFamily
    font.pixelSize: 12
    contentItem: Text {
        text: root.text
        color: root.destructive ? Theme.danger : (root.highlighted ? "#ffffff" : Theme.ink)
        font: root.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: Theme.radius
        color: root.highlighted ? (root.hovered ? Theme.accentHover : Theme.accent)
                               : (root.destructive ? Theme.dangerWash : (root.hovered ? "#f0f2ef" : Theme.surface))
        border.width: root.highlighted ? 0 : 1
        border.color: root.destructive ? "#f0d7d4" : Theme.line
    }
}
