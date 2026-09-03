import QtQuick
import "../theme" as Theme

Rectangle {
    id: root
    color: Theme.Colors.cardBg
    radius: Theme.Metrics.cardRadius
    border.color: Theme.Colors.border
    border.width: 1
    clip: true

    property string title: ""
    property string value: ""
    property string subtitle: ""
    property color valueColor: Theme.Colors.textPrimary
    property var sparkPoints: []  // array of normalized 0..1 values

    Column {
        id: textCol
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 12
        spacing: 2

        Text {
            text: root.title
            font.family: Theme.Typography.fontFamily
            font.pixelSize: 9
            font.weight: Font.DemiBold
            color: Theme.Colors.textMuted
            font.letterSpacing: 0.5
        }
        Text {
            text: root.value
            font.family: Theme.Typography.fontFamily
            font.pixelSize: 20
            font.weight: Font.Bold
            color: root.valueColor
            lineHeight: 1.0
        }
        Text {
            text: root.subtitle
            font.family: Theme.Typography.fontFamily
            font.pixelSize: 9
            color: Theme.Colors.textLight
            visible: subtitle !== ""
        }
    }

    // Sparkline
    Canvas {
        id: spark
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.bottomMargin: 12
        height: 20
        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0,0,width,height)
            if (root.sparkPoints.length < 2) return
            ctx.strokeStyle = root.valueColor
            ctx.lineWidth = 1.4
            ctx.lineJoin = "round"
            ctx.lineCap = "round"
            ctx.beginPath()
            for (var i=0;i<root.sparkPoints.length;i++) {
                var x = (i/(root.sparkPoints.length-1))*width
                var y = height - root.sparkPoints[i]*height*0.85 - height*0.07
                if (i===0) ctx.moveTo(x,y)
                else ctx.lineTo(x,y)
            }
            ctx.stroke()
            // faint fill under line
            ctx.globalAlpha = 0.08
            ctx.lineTo(width, height)
            ctx.lineTo(0, height)
            ctx.closePath()
            ctx.fillStyle = root.valueColor
            ctx.fill()
        }
        Component.onCompleted: requestPaint()
        onWidthChanged: requestPaint()
    }
}

