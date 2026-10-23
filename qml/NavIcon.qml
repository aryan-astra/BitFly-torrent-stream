import QtQuick

Canvas {
    id: root
    required property string name
    property color color: Theme.railMuted
    implicitWidth: 16
    implicitHeight: 16
    onColorChanged: requestPaint()
    onNameChanged: requestPaint()
    onPaint: {
        const context = getContext("2d")
        context.clearRect(0, 0, width, height)
        context.lineWidth = 1.5
        context.lineCap = "round"
        context.lineJoin = "round"
        context.strokeStyle = color
        context.fillStyle = color
        if (name === "stream") {
            context.beginPath()
            context.moveTo(4, 2.5)
            context.lineTo(13, 8)
            context.lineTo(4, 13.5)
            context.closePath()
            context.stroke()
        } else if (name === "queue") {
            for (let y = 4; y <= 12; y += 4) {
                context.beginPath()
                context.arc(3, y, 0.8, 0, Math.PI * 2)
                context.fill()
                context.beginPath()
                context.moveTo(6, y)
                context.lineTo(13, y)
                context.stroke()
            }
        } else if (name === "history") {
            context.beginPath()
            context.arc(8, 8, 6, -Math.PI * 0.78, Math.PI * 1.65)
            context.stroke()
            context.beginPath()
            context.moveTo(3, 3)
            context.lineTo(3, 6.5)
            context.lineTo(6.5, 6.5)
            context.stroke()
            context.beginPath()
            context.moveTo(8, 4.5)
            context.lineTo(8, 8)
            context.lineTo(10.5, 9.5)
            context.stroke()
        } else if (name === "settings") {
            context.beginPath()
            context.arc(8, 8, 2.2, 0, Math.PI * 2)
            context.stroke()
            for (let angle = 0; angle < Math.PI * 2; angle += Math.PI / 4) {
                const innerX = 8 + Math.cos(angle) * 4.2
                const innerY = 8 + Math.sin(angle) * 4.2
                const outerX = 8 + Math.cos(angle) * 6.2
                const outerY = 8 + Math.sin(angle) * 6.2
                context.beginPath()
                context.moveTo(innerX, innerY)
                context.lineTo(outerX, outerY)
                context.stroke()
            }
        }
    }
}
