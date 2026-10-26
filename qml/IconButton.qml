import QtQuick
import QtQuick.Controls

ToolButton {
    id: root
    property string glyph
    property string hint
    property bool destructive: false
    implicitWidth: 30
    implicitHeight: 30
    hoverEnabled: true
    opacity: enabled ? 1 : 0.48
    Accessible.name: root.hint
    scale: root.down ? 0.9 : (root.hovered ? 1.06 : 1)
    Behavior on scale { NumberAnimation { duration: Theme.fastMotion; easing.type: Easing.OutCubic } }
    contentItem: Text {
        anchors.centerIn: parent
        text: root.glyph
        color: root.destructive ? Theme.danger : Theme.secondary
        font.family: Theme.fontFamily
        font.pixelSize: 13
        font.weight: Font.DemiBold
    }
    background: Rectangle {
        radius: Theme.radiusSmall
        color: root.hovered || root.activeFocus
               ? (root.destructive ? Theme.dangerWash : Theme.raisedHover) : "transparent"
        border.width: root.activeFocus || root.hovered ? 1 : 0
        border.color: root.activeFocus ? Theme.accent : (root.destructive ? Theme.line : Theme.line)
    }
    ToolTip {
        visible: root.hovered || root.activeFocus
        text: root.hint
        delay: 450
        font.family: Theme.fontFamily
        font.pixelSize: 11
    }
}
