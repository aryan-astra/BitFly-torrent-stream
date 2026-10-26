import QtQuick
import QtQuick.Controls

Button {
    id: root
    property bool destructive: false
    implicitHeight: 32
    leftPadding: 13
    rightPadding: 13
    hoverEnabled: true
    opacity: enabled ? 1 : 0.48
    font.family: Theme.fontFamily
    font.pixelSize: 12
    font.weight: Font.DemiBold
    contentItem: Text {
        text: root.text
        color: root.destructive ? Theme.danger
                                : (root.highlighted ? Theme.accentInk : Theme.ink)
        font: root.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    scale: root.down ? 0.97 : 1
    Behavior on scale { NumberAnimation { duration: Theme.fastMotion; easing.type: Easing.OutCubic } }
    background: Rectangle {
        Behavior on color { ColorAnimation { duration: Theme.fastMotion } }
        radius: Theme.radiusSmall
        color: root.highlighted ? (root.hovered ? Theme.accentHover : Theme.accent)
                                : (root.destructive ? Theme.dangerWash
                                                    : (root.hovered ? Theme.raisedHover : Theme.raised))
        border.width: root.activeFocus ? 2 : (root.highlighted ? 0 : 1)
        border.color: root.activeFocus && root.highlighted ? Theme.ink
                      : root.activeFocus ? Theme.accent : Theme.line
    }
}
