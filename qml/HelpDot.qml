import QtQuick
import QtQuick.Controls

ToolButton {
    id: root
    required property string hint
    implicitWidth: 26
    implicitHeight: 26
    hoverEnabled: true
    Accessible.name: "Help: " + root.hint
    Accessible.description: root.hint
    background: Rectangle {
        radius: width / 2
        color: root.hovered || root.activeFocus ? Theme.accentWash : "transparent"
        border.width: 1
        border.color: root.hovered || root.activeFocus ? Theme.accent : Theme.line
    }
    contentItem: Text {
        text: "?"
        color: root.hovered || root.activeFocus ? Theme.accent : Theme.secondary
        font.family: Theme.fontFamily
        font.pixelSize: 12
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    ToolTip {
        objectName: "helpTooltip"
        visible: root.hovered || root.activeFocus
        text: root.hint
        delay: 250
        timeout: 20000
        contentItem: Text {
            text: root.hint
            color: Theme.ink
            font.family: Theme.fontFamily
            font.pixelSize: 12
            wrapMode: Text.Wrap
            width: 280
        }
        background: Rectangle {
            color: Theme.raisedHover
            radius: Theme.radiusSmall
            border.color: Theme.line
            implicitWidth: 300
        }
    }
}
