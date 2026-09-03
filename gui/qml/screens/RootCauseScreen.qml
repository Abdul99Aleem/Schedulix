import QtQuick
import "../theme" as Theme
import "../components" as Comp

Rectangle {
    id: root
    color: Theme.Colors.contentBg

    Flickable {
        anchors.fill: parent
        anchors.margins: 16
        contentHeight: col.height + 20
        clip: true
        Column {
            id: col
            width: parent.width
            spacing: 12

            // Header
            Column {
                width: parent.width
                spacing: 8

                Row {
                    spacing: 10
                    Text { text: "<- Back to Incidents"; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; color: Theme.Colors.accentBlue; font.weight: Font.Medium }
                }
                Row {
                    width: parent.width
                    spacing: 12
                    Text { text: "BRAKE #482 - DEADLINE MISS"; font.family: Theme.Typography.fontFamily; font.pixelSize: 16; font.weight: Font.Bold; color: Theme.Colors.textPrimary }
                    Comp.StatusBadge { text: "CRITICAL"; type: "critical"; anchors.verticalCenter: parent.verticalCenter }
                    Item { width: 20; height: 1 }
                    Column {
                        spacing: 2
                        Text { text: "Correlation ID"; font.family: Theme.Typography.fontFamily; font.pixelSize: 9; color: Theme.Colors.textLight; horizontalAlignment: Text.AlignRight; width: 140 }
                        Text { text: "BRAKE-482-CAN100"; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; color: Theme.Colors.textPrimary; font.weight: Font.Medium; horizontalAlignment: Text.AlignRight; width: 140 }
                    }
                    Rectangle {
                        width: 96; height: 28; radius: 8; color: "white"; border.color: Theme.Colors.border; border.width: 1
                        Text { anchors.centerIn: parent; text: "View Trace"; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; color: Theme.Colors.textPrimary; font.weight: Font.Medium }
                    }
                }
                Row {
                    spacing: 20
                    Text { text: "Deadline: 10.0 ms"; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; color: Theme.Colors.textSecondary }
                    Text { text: "Actual: 12.8 ms"; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; color: Theme.Colors.textSecondary }
                    Text { text: "Violation: +2.8 ms"; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; color: Theme.Colors.red; font.weight: Font.Medium }
                }
            }

            // Three cards row
            Row {
                width: parent.width
                spacing: 12
                height: 420

                // Causal Flow
                Rectangle {
                    width: (parent.width - 24) * 0.34
                    height: parent.height
                    radius: Theme.Metrics.cardRadius
                    color: Theme.Colors.cardBg
                    border.color: Theme.Colors.border; border.width: 1
                    clip: true
                    Column {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 10
                        Text { text: "CAUSAL FLOW"; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; font.weight: Font.DemiBold; color: Theme.Colors.textPrimary; font.letterSpacing: 0.5 }
                        Column {
                            width: parent.width
                            spacing: 0
                            Repeater {
                                model: rootCauseModel ? rootCauseModel.causalFlowList : []
                                delegate: Item {
                                    width: parent.width; height: 34
                                    Row {
                                        anchors.verticalCenter: parent.verticalCenter
                                        spacing: 10
                                        Rectangle {
                                            width: 22; height: 22; radius: 11
                                            color: modelData.n === "!" ? "#fef2f2" : (modelData.n === "4" ? "#fef2f2" : "white")
                                            border.color: modelData.c; border.width: 1.6
                                            Text { anchors.centerIn: parent; text: modelData.n; font.family: Theme.Typography.fontFamily; font.pixelSize: 10; color: modelData.c; font.weight: Font.Bold }
                                        }
                                        Text {
                                            width: 210
                                            text: modelData.t
                                            font.family: Theme.Typography.fontFamily
                                            font.pixelSize: 11
                                            color: modelData.n === "!" ? Theme.Colors.red : Theme.Colors.textPrimary
                                            font.weight: modelData.n === "!" ? Font.Bold : Font.Normal
                                            elide: Text.ElideRight
                                        }
                                    }
                                    Text {
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: modelData.d
                                        font.family: Theme.Typography.fontFamily
                                        font.pixelSize: 10
                                        color: modelData.n === "!" ? Theme.Colors.red : Theme.Colors.textMuted
                                        font.weight: modelData.n === "!" ? Font.Bold : Font.Normal
                                    }
                                    Rectangle {
                                        anchors.bottom: parent.bottom
                                        anchors.left: parent.left; anchors.leftMargin: 10
                                        width: 1; height: 8
                                        color: Theme.Colors.borderLight
                                        visible: index < 8
                                    }
                                }
                            }
                        }
                    }
                }

                // Root Cause + Delay Attribution
                Rectangle {
                    width: (parent.width - 24) * 0.34
                    height: parent.height
                    radius: Theme.Metrics.cardRadius
                    color: Theme.Colors.cardBg
                    border.color: Theme.Colors.border; border.width: 1
                    clip: true
                    Column {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 12
                        Text { text: "ROOT CAUSE"; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; font.weight: Font.DemiBold; color: Theme.Colors.textPrimary; font.letterSpacing: 0.5 }

                        Item {
                            width: parent.width
                            height: Math.max(leftCol.height, rightCol.height)

                            Column {
                                id: leftCol
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width - rightCol.width - 16
                                spacing: 4
                                Text { text: "Primary Cause"; font.family: Theme.Typography.fontFamily; font.pixelSize: 9; color: Theme.Colors.textLight }
                                Text { text: (rootCauseModel && rootCauseModel.rootCauseSummary) ? rootCauseModel.rootCauseSummary.cause : ""; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; font.weight: Font.Bold; color: Theme.Colors.textPrimary }
                                Item { width:1; height:6 }
                                Text { text: "Description"; font.family: Theme.Typography.fontFamily; font.pixelSize: 9; color: Theme.Colors.textLight }
                                Text { text: "Task delayed due to CPU contention and preemp-\ntion by higher priority task ADAS."; font.family: Theme.Typography.fontFamily; font.pixelSize: 10; color: Theme.Colors.textSecondary; lineHeight: 1.1 }
                            }
                            Column {
                                id: rightCol
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 6
                                Text { text: "Confidence"; font.family: Theme.Typography.fontFamily; font.pixelSize: 9; color: Theme.Colors.textLight; anchors.horizontalCenter: parent.horizontalCenter }
                                Rectangle {
                                    width: 56; height: 56; radius: 28; color: "white"; border.color: "#86efac"; border.width: 4
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    Text { anchors.centerIn: parent; text: (rootCauseModel && rootCauseModel.rootCauseSummary) ? rootCauseModel.rootCauseSummary.confidence : ""; font.family: Theme.Typography.fontFamily; font.pixelSize: 13; font.weight: Font.Bold; color: "#16a34a" }
                                }
                            }
                        }

                        Rectangle { width: parent.width; height: 1; color: Theme.Colors.borderLight }

                        Text { text: "DELAY ATTRIBUTION"; font.family: Theme.Typography.fontFamily; font.pixelSize: 10; font.weight: Font.DemiBold; color: Theme.Colors.textMuted; font.letterSpacing: 0.4 }

                        Row {
                            spacing: 18
                            anchors.horizontalCenter: parent.horizontalCenter
                            // Donut chart - bigger and centered
                            Item {
                                width: 130; height: 130
                                anchors.verticalCenter: parent.verticalCenter
                                Canvas {
                                    id: donutCanvas
                                    anchors.fill: parent
                                    anchors.margins: 4
                                    renderTarget: Canvas.FramebufferObject
                                    onPaint: {
                                        var ctx=getContext("2d"); ctx.clearRect(0,0,width,height)
                                        var cx=width/2, cy=height/2, r= 52, r2=36
                                        var hasData = rootCauseModel && rootCauseModel.rootCauseSummary
                                        var segments=[
                                            {v: hasData ? parseFloat(rootCauseModel.rootCauseSummary.preemption) : 0, c:"#dc2626"},
                                            {v: hasData ? parseFloat(rootCauseModel.rootCauseSummary.readyWait) : 0, c:"#3b82f6"},
                                            {v: hasData ? parseFloat(rootCauseModel.rootCauseSummary.execution) : 0, c:"#22c55e"},
                                            {v: hasData ? parseFloat(rootCauseModel.rootCauseSummary.blocking) : 0, c:"#f59e0b"}
                                        ]
                                        var total= hasData ? parseFloat(rootCauseModel.rootCauseSummary.actual) : 1
                                        var start=-Math.PI/2
                                        for(var i=0;i<segments.length;i++){
                                            var ang= segments[i].v/total * Math.PI*2
                                            ctx.beginPath()
                                            ctx.moveTo(cx,cy)
                                            ctx.arc(cx,cy,r,start,start+ang)
                                            ctx.closePath()
                                            ctx.fillStyle=segments[i].c
                                            ctx.fill()
                                            start+=ang
                                        }
                                        // inner white
                                        ctx.beginPath(); ctx.arc(cx,cy,r2,0,Math.PI*2); ctx.fillStyle="white"; ctx.fill()
                                    }
                                    Component.onCompleted: requestPaint()
                                    onWidthChanged: requestPaint()
                                    onHeightChanged: requestPaint()
                                }
                                Text { anchors.centerIn: parent; text: ((rootCauseModel && rootCauseModel.rootCauseSummary) ? rootCauseModel.rootCauseSummary.actual : "") + "\nTotal"; font.family: Theme.Typography.fontFamily; font.pixelSize: 9; color: Theme.Colors.textSecondary; horizontalAlignment: Text.AlignHCenter }
                            }
                            Column {
                                spacing: 7
                                anchors.verticalCenter: parent.verticalCenter
                                Repeater {
                                    model: (rootCauseModel && rootCauseModel.rootCauseSummary) ? [
                                        {c:"#dc2626", l:"Preemption (ADAS)", v: rootCauseModel.rootCauseSummary.preemption + " (48%)"},
                                        {c:"#3b82f6", l:"Ready Wait", v: rootCauseModel.rootCauseSummary.readyWait + " (16%)"},
                                        {c:"#22c55e", l:"Execution (CPU 0)", v: rootCauseModel.rootCauseSummary.execution + " (33%)"},
                                        {c:"#f59e0b", l:"Blocking (Mutex A)", v: rootCauseModel.rootCauseSummary.blocking + " (6%)"}
                                    ] : []
                                    delegate: Row {
                                        spacing: 6
                                        Rectangle{ width:10; height:10; radius:2; color:modelData.c; anchors.verticalCenter:parent.verticalCenter }
                                        Text{ text: modelData.l; font.family: Theme.Typography.fontFamily; font.pixelSize:10; color:Theme.Colors.textSecondary; width:120; anchors.verticalCenter:parent.verticalCenter }
                                        Text{ text: modelData.v; font.family: Theme.Typography.fontFamily; font.pixelSize:10; color:Theme.Colors.textPrimary; font.weight:Font.Medium; anchors.verticalCenter:parent.verticalCenter }
                                    }
                                }
                                Item{ width:1; height:4 }
                                Row{ spacing: 6; Text{ width:10; text:""; } Text{ width:120; text:"Total"; font.family:Theme.Typography.fontFamily; font.pixelSize:10; font.weight:Font.Bold; color:Theme.Colors.textPrimary } Text{ text: (rootCauseModel && rootCauseModel.rootCauseSummary) ? rootCauseModel.rootCauseSummary.actual : ""; font.family:Theme.Typography.fontFamily; font.pixelSize:10; font.weight:Font.Bold; color:Theme.Colors.textPrimary } }
                            }
                        }
                    }
                }

                // Cause Classification
                Rectangle {
                    width: (parent.width - 24) * 0.32
                    height: parent.height
                    radius: Theme.Metrics.cardRadius
                    color: Theme.Colors.cardBg
                    border.color: Theme.Colors.border; border.width: 1
                    clip: true
                    Column {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 8
                        Text { text: "CAUSE CLASSIFICATION"; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; font.weight: Font.DemiBold; color: Theme.Colors.textPrimary; font.letterSpacing: 0.5 }
                        // Table header - adjusted widths to prevent overflow
                        Row {
                            width: parent.width
                            Text { width: parent.width*0.42; text: "CAUSE"; font.family: Theme.Typography.fontFamily; font.pixelSize: 8; color: Theme.Colors.textLight; font.weight: Font.Medium }
                            Text { width: parent.width*0.28; text: "DURATION"; font.family: Theme.Typography.fontFamily; font.pixelSize: 8; color: Theme.Colors.textLight; font.weight: Font.Medium; horizontalAlignment: Text.AlignRight }
                            Text { width: parent.width*0.30; text: "CONFIDENCE"; font.family: Theme.Typography.fontFamily; font.pixelSize: 8; color: Theme.Colors.textLight; font.weight: Font.Medium; horizontalAlignment: Text.AlignRight; rightPadding: 24 }
                        }
                        Rectangle { width: parent.width; height: 1; color: Theme.Colors.borderLight }
                        Column {
                            width: parent.width; spacing: 0
                            Repeater {
                                model: rootCauseModel ? rootCauseModel.classificationList : []
                                delegate: Rectangle {
                                    width: parent.width; height: 30; color: index%2===0 ? "#f9fafb" : "transparent"
                                    clip: true
                                    Row {
                                        anchors.verticalCenter: parent.verticalCenter; width: parent.width
                                        Text { width: parent.width*0.42; text: modelData.cat; font.family: Theme.Typography.fontFamily; font.pixelSize: 9; color: modelData.cat==="CPU_CONTENTION"?Theme.Colors.textPrimary:Theme.Colors.textSecondary; font.weight: modelData.cat==="CPU_CONTENTION"?Font.Medium:Font.Normal; elide: Text.ElideRight; maximumLineCount: 1 }
                                        Text { width: parent.width*0.28; text: modelData.dur; font.family: Theme.Typography.fontFamily; font.pixelSize: 9; color: Theme.Colors.textSecondary; horizontalAlignment: Text.AlignRight }
                                        Text { width: parent.width*0.30; text: modelData.conf; font.family: Theme.Typography.fontFamily; font.pixelSize: 9; color: Theme.Colors.textSecondary; horizontalAlignment: Text.AlignRight; rightPadding: 24 }
                                    }
                                    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.Colors.borderLight; opacity: 0.5 }
                                }
                            }
                        }
                    }
                }
            }

            // Bottom diagnostic strip
            Rectangle {
                width: parent.width
                height: 68
                radius: Theme.Metrics.cardRadius
                color: Theme.Colors.cardBg
                border.color: Theme.Colors.border; border.width: 1
                Row {
                    anchors.fill: parent
                    anchors.margins: 14
                    Repeater {
                        model: [
                            {k:"Preempting Task:", v:"ADAS_1  (PRI  18)"},
                            {k:"CPU Affinity:", v:"CPU 0 -> CPU 0"},
                            {k:"ISR Activity:", v:"4  ISRs  (Vector  34)"},
                            {k:"Sched Usage:", v:"84%  (Safe)"},
                            {k:"Run Queue Depth:", v:"3  (Peak)"}
                        ]
                        delegate: Item {
                            width: parent.width/5
                            height: parent.height
                            Column {
                                spacing: 3
                                anchors.centerIn: parent
                                Text { text: modelData.k; font.family: Theme.Typography.fontFamily; font.pixelSize: 9; color: Theme.Colors.textLight; font.weight: Font.Medium }
                                Text { text: modelData.v; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; color: Theme.Colors.textPrimary }
                            }
                            Rectangle { anchors.right: parent.right; width:1; height: 28; color: Theme.Colors.borderLight; anchors.verticalCenter: parent.verticalCenter; visible: index<4 }
                        }
                    }
                }
            }
        }
    }
}

