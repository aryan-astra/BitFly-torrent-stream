import QtQuick

Image {
    required property string name
    property color color: Theme.faint
    width: 32
    height: 32
    source: "assets/nav-" + name + ".png"
    sourceSize.width: 64
    sourceSize.height: 64
    fillMode: Image.PreserveAspectFit
    asynchronous: true
    Accessible.ignored: true
}
