import QtQuick

Item {
    id: root
    width: 16
    height: 16
    property string iconType: "overview"
    property color color: "#6b7280"

    Canvas {
        anchors.fill: parent
        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0,0,width,height)
            ctx.fillStyle = root.color
            ctx.strokeStyle = root.color
            ctx.lineWidth = 1.2
            ctx.lineCap = "round"
            ctx.lineJoin = "round"

            if (root.iconType === "overview") {
                // 2x2 grid
                var s=5, g=2
                ctx.fillRect(1,1,s,s)
                ctx.fillRect(1+s+g,1,s,s)
                ctx.fillRect(1,1+s+g,s,s)
                ctx.fillRect(1+s+g,1+s+g,s,s)
            } else if (root.iconType === "workloads") {
                // stacked layers
                ctx.fillRect(2,2,12,3)
                ctx.fillRect(2,7,12,3)
                ctx.fillRect(2,12,12,3)
            } else if (root.iconType === "timeline") {
                // chart line
                ctx.beginPath()
                ctx.moveTo(1,13); ctx.lineTo(1,1); ctx.lineTo(15,1)
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(2,10); ctx.lineTo(6,6); ctx.lineTo(10,8); ctx.lineTo(14,4)
                ctx.stroke()
            } else if (root.iconType === "can") {
                // 3 vertical bars
                ctx.fillRect(2,8,3,6)
                ctx.fillRect(7,5,3,9)
                ctx.fillRect(12,3,3,11)
            } else if (root.iconType === "scheduling") {
                // clock/pie
                ctx.beginPath(); ctx.arc(8,8,6,0,Math.PI*2); ctx.stroke()
                ctx.beginPath(); ctx.moveTo(8,8); ctx.lineTo(8,3); ctx.lineTo(12,8); ctx.closePath(); ctx.fill()
            } else if (root.iconType === "jitter") {
                // wave
                ctx.beginPath()
                ctx.moveTo(1,8); ctx.quadraticCurveTo(4,2,7,8); ctx.quadraticCurveTo(10,14,13,8); ctx.quadraticCurveTo(14,6,15,8)
                ctx.stroke()
            } else if (root.iconType === "rootcause") {
                // magnifier
                ctx.beginPath(); ctx.arc(6,6,4,0,Math.PI*2); ctx.stroke()
                ctx.beginPath(); ctx.moveTo(9,9); ctx.lineTo(14,14); ctx.stroke()
                ctx.fillRect(11,11,2,2)
            } else if (root.iconType === "experiments") {
                // flask
                ctx.beginPath()
                ctx.moveTo(5,2); ctx.lineTo(11,2); ctx.lineTo(11,6); ctx.lineTo(13,9); ctx.lineTo(13,13); ctx.lineTo(3,13); ctx.lineTo(3,9); ctx.lineTo(5,6)
                ctx.closePath(); ctx.stroke()
                ctx.fillRect(5,10,6,2)
            } else if (root.iconType === "hardware") {
                // chip
                ctx.strokeRect(4,4,8,8)
                ctx.fillRect(2,5,2,2); ctx.fillRect(2,9,2,2)
                ctx.fillRect(12,5,2,2); ctx.fillRect(12,9,2,2)
                ctx.fillRect(6,2,2,2); ctx.fillRect(10,2,2,2)
            } else if (root.iconType === "tracehealth") {
                // shield
                ctx.beginPath()
                ctx.moveTo(8,1); ctx.lineTo(13,4); ctx.lineTo(13,10); ctx.quadraticCurveTo(8,14,3,10); ctx.lineTo(3,4)
                ctx.closePath(); ctx.stroke()
                ctx.beginPath(); ctx.moveTo(5,8); ctx.lineTo(7,10); ctx.lineTo(11,6); ctx.stroke()
            }
        }
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        Component.onCompleted: requestPaint()
    }
}
