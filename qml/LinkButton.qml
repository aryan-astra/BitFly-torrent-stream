import QtQuick
import QtQuick.Controls

Button {
    id: root
    implicitHeight: 26
    leftPadding: 4
    rightPadding: 4
    topPadding: 0
    bottomPadding: 0
    hoverEnabled: true
    font.family: Theme.fontFamily
    font.pixelSize: 12
    Accessible.role: Accessible.Link
    contentItem: Text {
        text: root.text
        color: root.hovered ? Theme.accentHover : Theme.accent
        font.family: Theme.fontFamily
        font.pixelSize: 12
        font.underline: true
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        color: Theme.surface
        border.color: root.activeFocus ? Theme.accent : "transparent"
        border.width: root.activeFocus ? 1 : 0
        radius: 2
    }
}
