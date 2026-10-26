import QtQuick

Item {
    id: root
    property url source
    property bool dimmed: false
    property color surround: Theme.surface
    readonly property bool ready: artwork.status === Image.Ready
    readonly property real currentCornerRadius: frame.radius
    readonly property real currentCoverWidth: frame.width
    readonly property real imageRatio: artwork.implicitHeight > 0
        ? artwork.implicitWidth / artwork.implicitHeight : 1
    readonly property real aspect: ready ? Math.max(1, Math.min(1.85, imageRatio)) : 1
    implicitWidth: 180
    implicitHeight: ready ? width / aspect : 72
    Behavior on implicitHeight { NumberAnimation { duration: Theme.coverMotion; easing.type: Easing.OutCubic } }

    Rectangle {
        id: frame
        anchors.centerIn: parent
        width: root.ready ? root.width : Math.min(72, root.width)
        height: root.ready ? root.height : Math.min(72, root.height)
        radius: root.ready ? Theme.radius : width / 2
        color: Theme.sky
        clip: true
        opacity: root.dimmed ? 0.48 : 1
        Behavior on width { NumberAnimation { duration: Theme.coverMotion; easing.type: Easing.OutCubic } }
        Behavior on radius { NumberAnimation { duration: Theme.coverMotion; easing.type: Easing.OutCubic } }
        Image {
            anchors.centerIn: parent
            width: 48
            height: 48
            source: "assets/bitfly-mark.png"
            sourceSize.width: 84
            sourceSize.height: 84
            fillMode: Image.PreserveAspectFit
            visible: !root.ready
            Accessible.ignored: true
        }
        Image {
            id: artwork
            anchors.fill: parent
            source: root.source
            sourceSize.width: 512
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            cache: true
            opacity: root.ready ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.fastMotion } }
        }
        // Paint only the corners; avoids an extra image-sized shader texture.
        Canvas {
            anchors.fill: parent
            property real corner: frame.radius
            onCornerChanged: requestPaint()
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPaint: {
                const c = getContext("2d"), r = Math.min(corner, width / 2, height / 2)
                c.clearRect(0, 0, width, height)
                c.fillStyle = root.surround
                c.beginPath(); c.moveTo(0, 0); c.lineTo(r, 0); c.arc(r, r, r, -Math.PI / 2, -Math.PI, true); c.closePath(); c.fill()
                c.beginPath(); c.moveTo(width, 0); c.lineTo(width, r); c.arc(width-r, r, r, 0, -Math.PI/2, true); c.closePath(); c.fill()
                c.beginPath(); c.moveTo(width, height); c.lineTo(width-r, height); c.arc(width-r, height-r, r, Math.PI/2, 0, true); c.closePath(); c.fill()
                c.beginPath(); c.moveTo(0, height); c.lineTo(0, height-r); c.arc(r, height-r, r, Math.PI, Math.PI/2, true); c.closePath(); c.fill()
            }
        }
    }
}
