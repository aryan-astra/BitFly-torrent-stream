import QtQuick
import QtQuick.Controls

Button {
    id: root
    implicitHeight: 22
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0
    hoverEnabled: true
    font.family: Theme.fontFamily
    font.pixelSize: 11
    Accessible.role: Accessible.Link
    contentItem: Text {
        text: root.text
        color: root.hovered ? Theme.accentHover : Theme.accent
        font.family: Theme.fontFamily
        font.pixelSize: 11
        font.underline: true
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        color: "transparent"
        border.color: root.activeFocus ? Theme.accent : "transparent"
        border.width: root.activeFocus ? 1 : 0
        radius: 2
    }
}
