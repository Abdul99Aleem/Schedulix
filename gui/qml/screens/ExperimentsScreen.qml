import QtQuick
import "../theme" as Theme
import "../components" as Comp

Rectangle {
    id: root
    color: Theme.Colors.contentBg

    Flickable {
        anchors.fill: parent
        anchors.margins: 16
        contentHeight: col.height
        clip: true
        Column {
            id: col
            width: parent.width
            spacing: 12

            // Header
            Column {
                width: parent.width
                spacing: 6
                Row {
                    width: parent.width
                    spacing: 12
                    Column {
                        spacing: 3
                        Text { text: "Experiment Comparison"; font.family: Theme.Typography.fontFamily; font.pixelSize: 16; font.weight: Font.Bold; color: Theme.Colors.textPrimary }
                        Text { text: "Cross-scenario performance analysis (S0 - S6 Stress Testing)"; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; color: Theme.Colors.textMuted }
                    }
                    Item { width: parent.width - 520 - 160; height: 1 }
                    Row {
                        spacing: 12; anchors.verticalCenter: parent.verticalCenter
                        Row { spacing: 6; Rectangle{width:8; height:8; radius:4; color:Theme.Colors.accentBlue; anchors.verticalCenter:parent.verticalCenter} Text{text:"NOMINAL"; font.family:Theme.Typography.fontFamily; font.pixelSize:9; color:Theme.Colors.textMuted; font.weight:Font.Medium; anchors.verticalCenter:parent.verticalCenter} }
                        Row { spacing: 6; Rectangle{width:8; height:8; radius:4; color:Theme.Colors.red; anchors.verticalCenter:parent.verticalCenter} Text{text:"VIOLATION"; font.family:Theme.Typography.fontFamily; font.pixelSize:9; color:Theme.Colors.textMuted; font.weight:Font.Medium; anchors.verticalCenter:parent.verticalCenter} }
                    }
                }
            }

            // Charts row
            Row {
                width: parent.width
                spacing: 12
                height: 220

                // P99 Response Time
                Rectangle {
                    width: (parent.width - 24)/3
                    height: parent.height
                    radius: Theme.Metrics.cardRadius
                    color: Theme.Colors.cardBg
                    border.color: Theme.Colors.border; border.width: 1
                    clip: true
                    Column {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 8
                        Text { text: "P99 Response Time (ms)"; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; font.weight: Font.Medium; color: Theme.Colors.textPrimary }
                        Item {
                            width: parent.width; height: 160
                            // Y axis labels
                            Column {
                                anchors.left: parent.left; width: 28; height: 130
                                anchors.top: parent.top; anchors.topMargin: 6
                                spacing: 32
                                Repeater { model: ["150","100","50","0"]; delegate: Text{ text:modelData; font.family:Theme.Typography.fontFamily; font.pixelSize:8; color:Theme.Colors.textLight; horizontalAlignment:Text.AlignRight; width:24 } }
                            }
                            // Grid
                            Item {
                                x: 32; y: 6; width: parent.width-40; height: 130
                                Column { anchors.fill: parent; spacing: 32
                                    Repeater { model:4; delegate: Rectangle{ width:parent.width; height:1; color:Theme.Colors.borderLight; opacity:0.7 } }
                                }
                                // Bars
                                Row {
                                    anchors.bottom: parent.bottom; width: parent.width; height: 130
                                    spacing: 8
                                    property var vals: experimentModel ? experimentModel.p99ResponseVals : [0,0,0,0,0,0,0]
                                    property var labels: ["S0","S1","S2","S3","S4","S5","S6"]
                                    Repeater {
                                        model: 7
                                        delegate: Item {
                                            width: (parent.width - 48)/7; height: parent.height
                                            Rectangle {
                                                width: parent.width; height: parent.parent.vals[index]/150 * 110
                                                anchors.bottom: parent.bottom
                                                anchors.bottomMargin: 14
                                                radius: 2
                                                color: index===4 ? "#dc2626" : "#1e40af"
                                                opacity: 0.92
                                            }
                                            // peak label for S4
                                            Rectangle {
                                                visible: index===4
                                                x: parent.width/2 - 30; y: parent.height - parent.parent.vals[index]/150*110 - 22
                                                width: 60; height: 14; radius: 3; color: "#fef2f2"; border.color: Theme.Colors.redBorder; border.width: 1
                                                Text { anchors.centerIn: parent; text: "112 ms (S4)"; font.family: Theme.Typography.fontFamily; font.pixelSize: 7; color: Theme.Colors.red; font.weight: Font.Bold }
                                            }
                                            Text { text: parent.parent.labels[index]; font.family: Theme.Typography.fontFamily; font.pixelSize: 8; color: index===4 ? Theme.Colors.red : Theme.Colors.textLight; font.weight: index===4?Font.Bold:Font.Normal; anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // Deadline Miss Rate
                Rectangle {
                    width: (parent.width - 24)/3
                    height: parent.height
                    radius: Theme.Metrics.cardRadius
                    color: Theme.Colors.cardBg
                    border.color: Theme.Colors.border; border.width: 1
                    clip: true
                    Column {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 8
                        Text { text: "Deadline Miss Rate (%)"; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; font.weight: Font.Medium; color: Theme.Colors.textPrimary }
                        Item {
                            width: parent.width; height: 160
                            Item {
                                anchors.left: parent.left; width: 28; height: 116
                                anchors.top: parent.top; anchors.topMargin: 6
                                Repeater {
                                    model: ["12%","9%","6%","3%","0%"]
                                    delegate: Text {
                                        width: 28
                                        y: index * (parent.height / 4) - height/2
                                        text: modelData
                                        font.family: Theme.Typography.fontFamily
                                        font.pixelSize: 8
                                        color: Theme.Colors.textLight
                                        horizontalAlignment: Text.AlignRight
                                    }
                                }
                            }
                            Item {
                                x: 32; y: 6; width: parent.width - 40; height: 130
                                // Grid Lines - aligned with Y labels
                                Item {
                                    anchors.fill: parent
                                    anchors.bottomMargin: 14
                                    Repeater {
                                        model: 5
                                        delegate: Rectangle {
                                            width: parent.width
                                            height: 1
                                            y: index * (parent.height / 4)
                                            color: Theme.Colors.borderLight
                                            opacity: 0.7
                                        }
                                    }
                                }
                                // Dots Canvas
                                Canvas {
                                    id: dotsCanvas
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.bottom: parent.bottom
                                    anchors.bottomMargin: 14
                                    renderTarget: Canvas.FramebufferObject
                                    onPaint: {
                                        var ctx = getContext("2d"); ctx.clearRect(0,0,width,height)
                                        var xs = [0.08,0.20,0.33,0.44,0.55,0.68,0.82]
                                        var ys = experimentModel ? experimentModel.deadlineMissYs : [0,0,0,0,0,0,0]
                                        for(var i=0;i<xs.length;i++){
                                            var x = xs[i]*width; var y = ys[i]*height
                                            ctx.beginPath(); ctx.arc(x,y,4,0,Math.PI*2)
                                            if(i>=4) ctx.fillStyle="#dc2626"; else if(i>=3) ctx.fillStyle="#f59e0b"; else ctx.fillStyle= i<=1?"#3b82f6":"#16a34a"
                                            if(i==1) ctx.fillStyle="#2563eb"
                                            ctx.fill()
                                            ctx.strokeStyle="white"; ctx.lineWidth=1; ctx.stroke()
                                        }
                                        ctx.fillStyle="#fef2f2"; ctx.strokeStyle="#fecaca"
                                        var lx = xs[4]*width-28; var ly = ys[4]*height-18
                                        ctx.fillRect(lx,ly,56,14); ctx.strokeRect(lx,ly,56,14)
                                        ctx.fillStyle="#dc2626"; ctx.font="7px 'Segoe UI'"; ctx.textAlign="center"
                                        ctx.fillText("8.4% (S4)", xs[4]*width, ly+10)
                                    }
                                    Component.onCompleted: requestPaint()
                                    onWidthChanged: requestPaint()
                                    onHeightChanged: requestPaint()
                                }
                                Row {
                                    anchors.bottom: parent.bottom; width: parent.width; height: 14
                                    Repeater { model: ["S0","S1","S2","S3","S4","S5","S6"]; delegate: Text { width: parent.width/7; text: modelData; font.family: Theme.Typography.fontFamily; font.pixelSize: 8; color: Theme.Colors.textLight; horizontalAlignment: Text.AlignHCenter } }
                                }
                            }
                        }
                    }
                }

                // Ctx Ready Wait
                Rectangle {
                    width: (parent.width - 24)/3
                    height: parent.height
                    radius: Theme.Metrics.cardRadius
                    color: Theme.Colors.cardBg
                    border.color: Theme.Colors.border; border.width: 1
                    clip: true
                    Column{
                        anchors.fill:parent; anchors.margins:14; spacing:8
                        Text{ text:"Ctx Ready Wait (ms)"; font.family:Theme.Typography.fontFamily; font.pixelSize:11; font.weight:Font.Medium; color:Theme.Colors.textPrimary}
                        Item{
                            width:parent.width; height:160
                            Column{ anchors.left:parent.left; width:28; height:130; anchors.top:parent.top; anchors.topMargin:6; spacing:32; Repeater{model:["100%","50%","0%"]; delegate: Text{text:modelData; font.family:Theme.Typography.fontFamily; font.pixelSize:8; color:Theme.Colors.textLight; horizontalAlignment:Text.AlignRight; width:28}}}
                            Item{
                                x:32; y:6; width:parent.width-40; height:130
                                Column{anchors.fill:parent; spacing:42; Repeater{model:3; delegate: Rectangle{width:parent.width; height:1; color:Theme.Colors.borderLight; opacity:0.7}}}
                                Canvas{
                                    anchors.fill:parent
                                    renderTarget: Canvas.FramebufferObject
                                    onPaint:{
                                        var ctx=getContext("2d"); ctx.clearRect(0,0,width,height)
                                        var cpus = experimentModel ? experimentModel.ctxReadyWaitCpus : []
                                        var xs=[0.08,0.20,0.33,0.44,0.55,0.68,0.82]
                                        for(var c=0;c<cpus.length;c++){
                                            ctx.beginPath(); ctx.strokeStyle=cpus[c].col; ctx.lineWidth=1.7
                                            for(var i=0;i<xs.length;i++){
                                                var x=xs[i]*width
                                                var y= (1 - cpus[c].pts[i])*height*0.85 + height*0.05
                                                if(i===0) ctx.moveTo(x,y); else ctx.lineTo(x,y)
                                            }
                                            ctx.stroke()
                                            for(var i=0;i<xs.length;i++){
                                                var x=xs[i]*width
                                                var y= (1 - cpus[c].pts[i])*height*0.85 + height*0.05
                                                ctx.beginPath(); ctx.arc(x,y,2.5,0,Math.PI*2); ctx.fillStyle=cpus[c].col; ctx.fill(); ctx.fillStyle="white"; ctx.beginPath(); ctx.arc(x,y,1,0,Math.PI*2); ctx.fill()
                                            }
                                        }
                                    }
                                    Component.onCompleted: requestPaint()
                                    onWidthChanged: requestPaint()
                                    onHeightChanged: requestPaint()
                                }
                                // legend on right inside chart?
                                Column{
                                    anchors.right:parent.right; anchors.top:parent.top; anchors.topMargin:4; spacing:4
                                    Repeater{ model: [{c:"#3b82f6", l:"CPU 0"},{c:"#ef4444", l:"CPU 1"},{c:"#22c55e", l:"CPU 2"},{c:"#f59e0b", l:"CPU 3"}]
                                        delegate: Row{ spacing:4; Rectangle{width:10; height:3; radius:1; color:modelData.c; anchors.verticalCenter:parent.verticalCenter} Text{text:modelData.l; font.family:Theme.Typography.fontFamily; font.pixelSize:8; color:Theme.Colors.textMuted} }
                                    }
                                }
                                Row{ anchors.bottom:parent.bottom; width:parent.width; height:14; Repeater{model:["S0","S1","S2","S3","S4","S5","S6"]; delegate: Text{width:parent.width/7; text:modelData; font.family:Theme.Typography.fontFamily; font.pixelSize:8; color:Theme.Colors.textLight; horizontalAlignment:Text.AlignHCenter}}}
                            }
                        }
                    }
                }
            }

            // KPI Matrix
            Rectangle {
                width: parent.width
                height: 360
                radius: Theme.Metrics.cardRadius
                color: Theme.Colors.cardBg
                border.color: Theme.Colors.border; border.width: 1
                clip: true
                Column {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10
                    Row {
                        width: parent.width
                        Text { text: "KPI Matrix"; font.family: Theme.Typography.fontFamily; font.pixelSize: 13; font.weight: Font.DemiBold; color: Theme.Colors.textPrimary }
                        Item { width: parent.width - 220; height: 1 }
                        Row {
                            spacing: 4
                            Text { text: "v"; font.pixelSize: 10; color: Theme.Colors.accentBlue }
                            Text { text: "Download CSV"; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; color: Theme.Colors.accentBlue; font.weight: Font.Medium }
                        }
                    }
                    Rectangle { width: parent.width; height: 1; color: Theme.Colors.borderLight }
                    // Header row
                    Row {
                        width: parent.width; height: 26
                        Text{ width: parent.width*0.22; text:"SCENARIO"; font.family:Theme.Typography.fontFamily; font.pixelSize:9; color:Theme.Colors.textLight; font.weight:Font.Medium }
                        Text{ width: parent.width*0.10; text:"STATUS"; font.family:Theme.Typography.fontFamily; font.pixelSize:9; color:Theme.Colors.textLight; font.weight:Font.Medium; horizontalAlignment:Text.AlignHCenter }
                        Text{ width: parent.width*0.12; text:"P99 RESP (MS)"; font.family:Theme.Typography.fontFamily; font.pixelSize:9; color:Theme.Colors.textLight; font.weight:Font.Medium; horizontalAlignment:Text.AlignHCenter }
                        Text{ width: parent.width*0.12; text:"MISS RATE (%)"; font.family:Theme.Typography.fontFamily; font.pixelSize:9; color:Theme.Colors.textLight; font.weight:Font.Medium; horizontalAlignment:Text.AlignHCenter }
                        Text{ width: parent.width*0.13; text:"MAX READY WAIT (MS)"; font.family:Theme.Typography.fontFamily; font.pixelSize:9; color:Theme.Colors.textLight; font.weight:Font.Medium; horizontalAlignment:Text.AlignHCenter }
                        Text{ width: parent.width*0.11; text:"MAX JITTER (MS)"; font.family:Theme.Typography.fontFamily; font.pixelSize:9; color:Theme.Colors.textLight; font.weight:Font.Medium; horizontalAlignment:Text.AlignHCenter }
                        Text{ width: parent.width*0.10; text:"CPU UTIL (%)"; font.family:Theme.Typography.fontFamily; font.pixelSize:9; color:Theme.Colors.textLight; font.weight:Font.Medium; horizontalAlignment:Text.AlignHCenter }
                        Text{ width: parent.width*0.10; text:"CAN -> RESP (MS)"; font.family:Theme.Typography.fontFamily; font.pixelSize:9; color:Theme.Colors.textLight; font.weight:Font.Medium; horizontalAlignment:Text.AlignHCenter }
                    }
                    Rectangle { width: parent.width; height: 1; color: Theme.Colors.borderLight }
                    Column {
                        width: parent.width; spacing: 0
                        Repeater {
                            model: experimentModel ? experimentModel.kpiMatrix : []
                            delegate: Rectangle {
                                width: parent.width; height: 34; color: index%2===0? "#f9fafb":"white"
                                Row {
                                    width: parent.width; anchors.verticalCenter: parent.verticalCenter
                                    Text{ width:parent.width*0.22; text:modelData.name; font.family:Theme.Typography.fontFamily; font.pixelSize:11; color:Theme.Colors.textPrimary }
                                    Item{ width:parent.width*0.10; height:18
                                        Rectangle{ anchors.centerIn:parent; width:52; height:18; radius:4; color: modelData.status==="PASS"?"#ecfdf5":modelData.status==="WARN"?"#fffbeb":"#fef2f2"; border.color: modelData.status==="PASS"?"#a7f3d0":modelData.status==="WARN"?"#fde68a":"#fecaca"; border.width:1
                                            Text{ anchors.centerIn:parent; text:modelData.status; font.family:Theme.Typography.fontFamily; font.pixelSize:9; font.weight:Font.DemiBold; color: modelData.status === "PASS" ? "#16a34a" : (modelData.status === "WARN" ? "#d97706" : "#dc2626") }
                                        }
                                    }
                                    Text{ width:parent.width*0.12; text:modelData.p99; font.family:Theme.Typography.fontFamily; font.pixelSize:11; color: modelData.status==="CRITICAL"?Theme.Colors.red:Theme.Colors.textSecondary; horizontalAlignment:Text.AlignHCenter; font.weight: modelData.status==="CRITICAL"?Font.Medium:Font.Normal; }
                                    Text{ width:parent.width*0.12; text:modelData.miss; font.family:Theme.Typography.fontFamily; font.pixelSize:11; color: modelData.status==="CRITICAL" || modelData.status==="WARN"?Theme.Colors.red:Theme.Colors.textSecondary; horizontalAlignment:Text.AlignHCenter }
                                    Text{ width:parent.width*0.13; text:modelData.wait; font.family:Theme.Typography.fontFamily; font.pixelSize:11; color: Theme.Colors.textSecondary; horizontalAlignment:Text.AlignHCenter }
                                    Text{ width:parent.width*0.11; text:modelData.jitter; font.family:Theme.Typography.fontFamily; font.pixelSize:11; color: Theme.Colors.textSecondary; horizontalAlignment:Text.AlignHCenter }
                                    Text{ width:parent.width*0.10; text:modelData.cpu; font.family:Theme.Typography.fontFamily; font.pixelSize:11; color: Theme.Colors.textSecondary; horizontalAlignment:Text.AlignHCenter }
                                    Text{ width:parent.width*0.10; text:modelData.can; font.family:Theme.Typography.fontFamily; font.pixelSize:11; color: modelData.status==="CRITICAL"?Theme.Colors.red:Theme.Colors.textSecondary; horizontalAlignment:Text.AlignHCenter }
                                }
                                Rectangle{ anchors.bottom:parent.bottom; width:parent.width; height:1; color:Theme.Colors.borderLight; opacity:0.5 }
                            }
                        }
                    }
                }
            }
        }
    }
}
