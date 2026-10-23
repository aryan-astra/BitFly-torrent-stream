import QtQuick
import QtQuick.Controls

SpinBox {
    id: root
    implicitWidth: 112
    implicitHeight: 32
    font.family: Theme.fontFamily
    font.pixelSize: 11

    contentItem: TextInput {
        text: root.displayText
        font: root.font
        color: Theme.ink
        selectionColor: Theme.accent
        selectedTextColor: "#ffffff"
        horizontalAlignment: TextInput.AlignHCenter
        verticalAlignment: TextInput.AlignVCenter
        readOnly: !root.editable
        validator: root.validator
        inputMethodHints: root.inputMethodHints
    }
    background: Rectangle {
        radius: Theme.radius
        color: Theme.surface
        border.color: root.activeFocus ? Theme.accent : Theme.line
        border.width: root.activeFocus ? 2 : 1
    }
    down.indicator: Rectangle {
        x: 1
        y: 1
        width: 25
        height: root.height - 2
        radius: Theme.radius
        color: "transparent"
        Label { anchors.centerIn: parent; text: "−"; color: Theme.secondary; font.pixelSize: 14 }
    }
    up.indicator: Rectangle {
        x: root.width - width - 1
        y: 1
        width: 25
        height: root.height - 2
        radius: Theme.radius
        color: "transparent"
        Label { anchors.centerIn: parent; text: "+"; color: Theme.secondary; font.pixelSize: 13 }
    }
}
