import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "../theme" as Theme
import "../components" as Comp

Rectangle {
    id: root
    color: Theme.Colors.contentBg

    // content scroll
    Flickable {
        anchors.fill: parent
        anchors.margins: 16
        contentHeight: col.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: col
            width: parent.width
            spacing: 12

            // Metric cards row
            Row {
                width: parent.width
                spacing: 12
                Comp.MetricCard {
                    width: (parent.width - 60)/6; height: 102
                    title: "DEADLINE MISS RATE"
                    value: systemMetrics ? systemMetrics.deadlineMissRate : ""
                    subtitle: "7 / 1000 activations"
                    valueColor: Theme.Colors.red
                    sparkPoints: [0.5,0.55,0.45,0.6,0.4,0.7,0.3,0.8,0.5,0.6]
                }
                Comp.MetricCard {
                    width: (parent.width - 60)/6; height: 102
                    title: "P99 RESPONSE"
                    value: systemMetrics ? systemMetrics.p99Response : ""
                    valueColor: Theme.Colors.accentBlue
                    sparkPoints: [0.4,0.45,0.35,0.5,0.3,0.55,0.4,0.5,0.45,0.4]
                }
                Comp.MetricCard {
                    width: (parent.width - 60)/6; height: 102
                    title: "WORST-CASE RESPONSE"
                    value: systemMetrics ? systemMetrics.worstCaseResponse : ""
                    valueColor: Theme.Colors.red
                    sparkPoints: [0.3,0.32,0.31,0.33,0.3,0.35,0.3,0.7,0.32,0.31]
                }
                Comp.MetricCard {
                    width: (parent.width - 60)/6; height: 102
                    title: "P99 READY WAIT"
                    value: systemMetrics ? systemMetrics.p99ReadyWait : ""
                    valueColor: Theme.Colors.accentBlue
                    sparkPoints: [0.4,0.5,0.3,0.6,0.45,0.5,0.35,0.55,0.4,0.45]
                }
                Comp.MetricCard {
                    width: (parent.width - 60)/6; height: 102
                    title: "CAN -> RESPONSE"
                    value: systemMetrics ? systemMetrics.canToResponse : ""
                    valueColor: Theme.Colors.accentBlue
                    sparkPoints: [0.35,0.4,0.38,0.42,0.36,0.44,0.4,0.43,0.39,0.41]
                }
                Comp.MetricCard {
                    width: (parent.width - 60)/6; height: 102
                    title: "TRACE INTEGRITY"
                    value: systemMetrics ? systemMetrics.traceIntegrity : ""
                    subtitle: "99.8% correlated"
                    valueColor: Theme.Colors.validGreen
                    sparkPoints: [0.7,0.72,0.71,0.73,0.72,0.74,0.73,0.75,0.74,0.76]
                }
            }

            // Second row: Workload Health + Root Cause Summary
            Row {
                width: parent.width
                spacing: 12
                height: 210

                // Workload Health
                Rectangle {
                    width: parent.width * 0.68
                    height: parent.height
                    radius: Theme.Metrics.cardRadius
                    color: Theme.Colors.cardBg
                    border.color: Theme.Colors.border; border.width: 1
                    clip: true

                    Column {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 10

                        Text {
                            text: "WORKLOAD HEALTH"
                            font.family: Theme.Typography.fontFamily
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            color: Theme.Colors.textPrimary
                            font.letterSpacing: 0.5
                        }

                        // Table header
                        Rectangle { width: parent.width; height: 1; color: Theme.Colors.borderLight }

                        Row {
                            width: parent.width
                            spacing: 0
                            // header labels
                            Repeater {
                                model: [
                                    {t:"TASK", w: 0.18, a: Text.AlignLeft},
                                    {t:"PRIO", w: 0.07, a: Text.AlignHCenter},
                                    {t:"PERIOD", w: 0.08, a: Text.AlignHCenter},
                                    {t:"DEADLINE", w: 0.09, a: Text.AlignHCenter},
                                    {t:"P50 RESP", w: 0.09, a: Text.AlignHCenter},
                                    {t:"P95 RESP", w: 0.09, a: Text.AlignHCenter},
                                    {t:"P99 RESP", w: 0.09, a: Text.AlignHCenter},
                                    {t:"MAX RESP", w: 0.10, a: Text.AlignHCenter},
                                    {t:"MISS %", w: 0.07, a: Text.AlignHCenter},
                                    {t:"STATUS", w: 0.08, a: Text.AlignHCenter}
                                ]
                                delegate: Text {
                                    width: parent.width * modelData.w
                                    text: modelData.t
                                    font.family: Theme.Typography.fontFamily
                                    font.pixelSize: 8
                                    color: Theme.Colors.textMuted
                                    font.weight: Font.Medium
                                    horizontalAlignment: modelData.a
                                }
                            }
                        }
                        Rectangle { width: parent.width; height: 1; color: Theme.Colors.borderLight }

                        // Rows
                        Column {
                            width: parent.width
                            spacing: 0
                            Repeater {
                                model: taskMetrics ? taskMetrics.workloadHealthList : []
                                delegate: Rectangle {
                                    width: parent.width; height: 34; color: "transparent"
                                    Row {
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: parent.width
                                        // TASK
                                        Item {
                                            width: parent.width * 0.18; height: 20
                                            Row { spacing: 6; anchors.verticalCenter: parent.verticalCenter
                                                Rectangle { width: 8; height: 8; radius: 4; color: modelData.name === "BRAKE" ? "#22c55e" : (modelData.name === "ADAS" ? "#3b82f6" : "#a855f7"); anchors.verticalCenter: parent.verticalCenter }
                                                Text { text: modelData.name; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; font.weight: Font.Medium; color: Theme.Colors.textPrimary; anchors.verticalCenter: parent.verticalCenter }
                                            }
                                        }
                                        Text { width: parent.width*0.07; text: modelData.prio; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; color: Theme.Colors.textSecondary; horizontalAlignment: Text.AlignHCenter }
                                        Text { width: parent.width*0.08; text: modelData.period; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; color: Theme.Colors.textSecondary; horizontalAlignment: Text.AlignHCenter }
                                        Text { width: parent.width*0.09; text: modelData.deadline; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; color: Theme.Colors.textSecondary; horizontalAlignment: Text.AlignHCenter }
                                        Text { width: parent.width*0.09; text: modelData.name === "BRAKE" ? "1.8ms" : (modelData.name === "ADAS" ? "4.1ms" : "9.0ms"); font.family: Theme.Typography.fontFamily; font.pixelSize: 11; color: Theme.Colors.textSecondary; horizontalAlignment: Text.AlignHCenter }
                                        Text { width: parent.width*0.09; text: modelData.name === "BRAKE" ? "4.2ms" : (modelData.name === "ADAS" ? "11.3ms" : "28.0ms"); font.family: Theme.Typography.fontFamily; font.pixelSize: 11; color: Theme.Colors.textSecondary; horizontalAlignment: Text.AlignHCenter }
                                        Text { width: parent.width*0.09; text: modelData.p99; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; color: modelData.name === "BRAKE" ? Theme.Colors.textPrimary : Theme.Colors.textSecondary; font.weight: modelData.name === "BRAKE" ? Font.Medium : Font.Normal; horizontalAlignment: Text.AlignHCenter }
                                        Text { width: parent.width*0.10; text: modelData.name === "BRAKE" ? "12.8ms" : (modelData.name === "ADAS" ? "22.1ms" : "62.3ms"); font.family: Theme.Typography.fontFamily; font.pixelSize: 11; color: modelData.name === "BRAKE" ? Theme.Colors.red : Theme.Colors.textSecondary; horizontalAlignment: Text.AlignHCenter }
                                        Text { width: parent.width*0.07; text: modelData.miss; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; color: modelData.name === "BRAKE" ? Theme.Colors.red : Theme.Colors.textSecondary; horizontalAlignment: Text.AlignHCenter }
                                        Item {
                                            width: parent.width*0.085; height: 18
                                            Comp.StatusBadge { anchors.centerIn: parent; text: modelData.status === "NOMINAL" ? "PASS" : "MISS"; type: modelData.status === "NOMINAL" ? "pass" : "miss" }
                                        }
                                    }
                                    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.Colors.borderLight; opacity: 0.6; visible: index < 2 }
                                }
                            }
                        }
                    }
                }

                // Root Cause Summary
                Rectangle {
                    width: parent.width * 0.32 - 12
                    height: parent.height
                    radius: Theme.Metrics.cardRadius
                    color: Theme.Colors.cardBg
                    border.color: Theme.Colors.border; border.width: 1
                    clip: true

                    Flickable {
                        anchors.fill: parent
                        anchors.margins: 14
                        contentHeight: rootCauseCol.height
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 6 }
                        Column {
                            id: rootCauseCol
                            width: parent.width
                            spacing: 10

                            Row {
                                spacing: 6
                                Rectangle { width: 6; height: 6; radius: 3; color: Theme.Colors.red; anchors.verticalCenter: parent.verticalCenter }
                                Text { text: "ROOT CAUSE SUMMARY"; font.family: Theme.Typography.fontFamily; font.pixelSize: 10; font.weight: Font.DemiBold; color: Theme.Colors.textMuted; font.letterSpacing: 0.5; anchors.verticalCenter: parent.verticalCenter }
                            }

                            Row {
                                width: parent.width
                                spacing: 8
                                Text { text: (rootCauseModel && rootCauseModel.rootCauseSummary) ? rootCauseModel.rootCauseSummary.task : ""; font.family: Theme.Typography.fontFamily; font.pixelSize: 13; font.weight: Font.Bold; color: Theme.Colors.red }
                                Rectangle {
                                    height: 18; width: 78; radius: 4; color: Theme.Colors.badgeMissBg; border.color: Theme.Colors.redBorder; border.width: 1
                                    Text { anchors.centerIn: parent; text: (rootCauseModel && rootCauseModel.rootCauseSummary) ? rootCauseModel.rootCauseSummary.type.toUpperCase() : ""; font.family: Theme.Typography.fontFamily; font.pixelSize: 8; font.weight: Font.DemiBold; color: Theme.Colors.red }
                                }
                            }
                            Text { text: (rootCauseModel && rootCauseModel.rootCauseSummary) ? (rootCauseModel.rootCauseSummary.actual + " / " + rootCauseModel.rootCauseSummary.limit + " (" + rootCauseModel.rootCauseSummary.violation + ")") : ""; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; color: Theme.Colors.textSecondary; font.weight: Font.Medium }

                            Rectangle { width: parent.width; height: 1; color: Theme.Colors.borderLight }

                            Column {
                                spacing: 8
                                width: parent.width
                                Text { text: (rootCauseModel && rootCauseModel.rootCauseSummary) ? ("Primary Cause: " + rootCauseModel.rootCauseSummary.cause) : ""; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; font.weight: Font.Medium; color: Theme.Colors.textPrimary }
                                Row {
                                    spacing: 6
                                    Text { text: (rootCauseModel && rootCauseModel.rootCauseSummary) ? ("Confidence: " + rootCauseModel.rootCauseSummary.confidence) : ""; font.family: Theme.Typography.fontFamily; font.pixelSize: 10; color: Theme.Colors.textMuted }
                                    Rectangle { width: 48; height: 4; radius: 2; color: Theme.Colors.borderLight; anchors.verticalCenter: parent.verticalCenter
                                        Rectangle { width: 46; height: 4; radius: 2; color: Theme.Colors.accentBlue }
                                    }
                                }
                            }

                            Column {
                                width: parent.width; spacing: 5
                                Repeater {
                                    model: (rootCauseModel && rootCauseModel.rootCauseSummary) ? [
                                        {label:"Ready Wait", color:"#3b82f6", val: rootCauseModel.rootCauseSummary.readyWait + "  (16%)"},
                                        {label:"Preemption (ADAS)", color:"#2563eb", val: rootCauseModel.rootCauseSummary.preemption + "  (45%)"},
                                        {label:"Blocking (Mutex A)", color:"#f59e0b", val: rootCauseModel.rootCauseSummary.blocking + "  (8%)"},
                                        {label:"Execution (CPU 0)", color:"#22c55e", val: rootCauseModel.rootCauseSummary.execution + "  (33%)"}
                                    ] : []
                                    delegate: Row {
                                        spacing: 8; width: parent.width
                                        Rectangle { width: 8; height: 8; radius: 2; color: modelData.color; anchors.verticalCenter: parent.verticalCenter }
                                        Text { width: 140; text: modelData.label; font.family: Theme.Typography.fontFamily; font.pixelSize: 10; color: Theme.Colors.textSecondary; anchors.verticalCenter: parent.verticalCenter }
                                        Text { text: modelData.val; font.family: Theme.Typography.fontFamily; font.pixelSize: 10; color: Theme.Colors.textPrimary; font.weight: Font.Medium; anchors.verticalCenter: parent.verticalCenter }
                                    }
                                }
                                Row {
                                    spacing: 8; width: parent.width
                                    Rectangle { width: 8; height: 2; color: "transparent"; }
                                    Text { width: 140; text: "Total"; font.family: Theme.Typography.fontFamily; font.pixelSize: 10; font.weight: Font.Bold; color: Theme.Colors.textPrimary }
                                    Text { text: (rootCauseModel && rootCauseModel.rootCauseSummary) ? rootCauseModel.rootCauseSummary.actual : ""; font.family: Theme.Typography.fontFamily; font.pixelSize: 10; font.weight: Font.Bold; color: Theme.Colors.textPrimary }
                                }
                            }

                            Text {
                                text: "View Incident Trace ->"
                                font.family: Theme.Typography.fontFamily
                                font.pixelSize: 11
                                color: Theme.Colors.accentBlue
                                font.weight: Font.Medium
                            }
                        }
                    }
                }
            }

            // Third row: Timeline Preview + Response Distribution
            Row {
                width: parent.width
                spacing: 12
                height: 260

                // CPU Utilization Card
                Rectangle {
                    id: cpuCard
                    width: parent.width * 0.60
                    height: parent.height
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
                            spacing: 6
                            Text { text: "CPU UTILIZATION"; font.family: Theme.Typography.fontFamily; font.pixelSize: 10; font.weight: Font.DemiBold; color: Theme.Colors.textMuted; font.letterSpacing: 0.5 }
                            Text { text: "(Last 60 sec)"; font.family: Theme.Typography.fontFamily; font.pixelSize: 10; color: Theme.Colors.textLight }
                        }

                        Item {
                            width: parent.width
                            height: cpuCard.height - 56

                            // Y-axis labels
                            Item {
                                anchors.left: parent.left
                                width: 24
                                height: parent.height - 20
                                Repeater {
                                    model: ["100%", "75%", "50%", "25%", "0%"]
                                    delegate: Text {
                                        width: 24
                                        y: index * (parent.height / 4) - height/2
                                        text: modelData
                                        font.family: Theme.Typography.fontFamily; font.pixelSize: 8; color: Theme.Colors.textLight
                                        horizontalAlignment: Text.AlignRight
                                    }
                                }
                            }

                            Item {
                                x: 30
                                width: parent.width - 30 - 110
                                height: parent.height

                                // Grid lines
                                Item {
                                    anchors.fill: parent
                                    anchors.bottomMargin: 20
                                    Repeater {
                                        model: 5
                                        delegate: Rectangle {
                                            width: parent.width; height: 1
                                            y: index * (parent.height / 4)
                                            color: Theme.Colors.borderLight; opacity: 0.7
                                        }
                                    }
                                }

                                // Traces Canvas
                                Canvas {
                                    id: cpuCanvas
                                    anchors.fill: parent
                                    anchors.bottomMargin: 20
                                    renderTarget: Canvas.FramebufferObject
                                    onPaint: {
                                        var ctx = getContext("2d"); ctx.clearRect(0,0,width,height)
                                        var cpus = [
                                            { pts: systemMetrics ? systemMetrics.cpu0History : [], col: "#3b82f6" }, // CPU 0
                                            { pts: systemMetrics ? systemMetrics.cpu1History : [], col: "#ef4444" }, // CPU 1
                                            { pts: systemMetrics ? systemMetrics.cpu2History : [], col: "#22c55e" }, // CPU 2
                                            { pts: systemMetrics ? systemMetrics.cpu3History : [], col: "#f59e0b" }  // CPU 3
                                        ]
                                        var numPts = 10
                                        for (var c=0; c<cpus.length; c++) {
                                            if (cpus[c].pts.length < numPts) continue;
                                            ctx.beginPath()
                                            ctx.strokeStyle = cpus[c].col
                                            ctx.lineWidth = 1.6
                                            ctx.lineJoin = "round"
                                            for (var i=0; i<numPts; i++) {
                                                var x = (i / (numPts - 1)) * width
                                                var y = (1 - cpus[c].pts[i]) * height
                                                if (i === 0) ctx.moveTo(x, y)
                                                else ctx.lineTo(x, y)
                                            }
                                            ctx.stroke()
                                        }
                                    }
                                    
                                    Connections {
                                        target: systemMetrics
                                        function onCpuHistoriesChanged() { cpuCanvas.requestPaint() }
                                    }
                                    Component.onCompleted: requestPaint()
                                    onWidthChanged: requestPaint()
                                    onHeightChanged: requestPaint()
                                }

                                // X-axis labels
                                Row {
                                    anchors.bottom: parent.bottom
                                    width: parent.width
                                    height: 14
                                    Repeater {
                                        model: ["-60s", "-45s", "-30s", "-15s", "0s"]
                                        delegate: Text {
                                            width: parent.width / 5
                                            text: modelData
                                            font.family: Theme.Typography.fontFamily; font.pixelSize: 8; color: Theme.Colors.textLight
                                            horizontalAlignment: Text.AlignHCenter
                                        }
                                    }
                                }
                            }

                            // Legend and current values on right
                            Column {
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.topMargin: 4
                                width: 90
                                spacing: 8

                                Text {
                                    text: "CURRENT UTIL"
                                    font.family: Theme.Typography.fontFamily; font.pixelSize: 8; font.weight: Font.DemiBold; color: Theme.Colors.textMuted
                                    font.letterSpacing: 0.3
                                }

                                Repeater {
                                    model: [
                                        { name: "CPU 0", color: "#3b82f6", val: "78%" },
                                        { name: "CPU 1", color: "#ef4444", val: "91%" },
                                        { name: "CPU 2", color: "#22c55e", val: "38%" },
                                        { name: "CPU 3", color: "#f59e0b", val: "66%" }
                                    ]
                                    delegate: Row {
                                        spacing: 6
                                        width: parent.width
                                        Rectangle {
                                            width: 8; height: 8; radius: 2; color: modelData.color
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                        Text {
                                            text: modelData.name
                                            font.family: Theme.Typography.fontFamily; font.pixelSize: 10; color: Theme.Colors.textSecondary
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: 40
                                        }
                                        Text {
                                            text: modelData.val
                                            font.family: Theme.Typography.fontFamily; font.pixelSize: 10; font.weight: Font.Bold; color: Theme.Colors.textPrimary
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // Response Distribution
                Rectangle {
                    width: parent.width * 0.40 - 12
                    height: parent.height
                    radius: Theme.Metrics.cardRadius
                    color: Theme.Colors.cardBg
                    border.color: Theme.Colors.border; border.width: 1
                    clip: true

                    Column {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 8
                        Text { text: "RESPONSE DISTRIBUTION (BRAKE)"; font.family: Theme.Typography.fontFamily; font.pixelSize: 10; font.weight: Font.DemiBold; color: Theme.Colors.textMuted; font.letterSpacing: 0.4 }

                        Item {
                            width: parent.width; height: 200
                            // Y grid lines
                            Column {
                                anchors.fill: parent
                                anchors.bottomMargin: 24
                                anchors.topMargin: 8
                                spacing: (height - 4) / 3
                                Repeater { model: 4; delegate: Rectangle { width: parent.width; height: 1; color: Theme.Colors.borderLight; opacity: 0.7 } }
                            }
                            // Bars - increased height to fill card
                            Row {
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 22
                                anchors.left: parent.left
                                anchors.leftMargin: 6
                                anchors.right: parent.right
                                anchors.rightMargin: 6
                                spacing: 6
                                height: 165
                                property var heights: [0.18,0.35,0.58,0.72,0.92,0.62,0.32,0.12]
                                Repeater {
                                    model: 8
                                    delegate: Column {
                                        width: (parent.width - 42)/8
                                        spacing: 0
                                        anchors.bottom: parent.bottom
                                        Rectangle {
                                            width: parent.width
                                            height: parent.parent.heights[index] * 145
                                            color: index===4 ? "#1e40af" : Theme.Colors.accentBlue
                                            radius: 2
                                            opacity: index===4 ? 1.0 : 0.85
                                        }
                                        Item { width:1; height:6 }
                                    }
                                }
                            }
                            // DL marker
                            Item {
                                x: parent.width * 0.70
                                y: 8
                                width: 40; height: 145
                                Rectangle { width:1; height: 170; color: Theme.Colors.red; opacity: 0.85; anchors.horizontalCenter: parent.horizontalCenter }
                                Rectangle { width: 34; height: 14; radius: 3; color: "#fef2f2"; border.color: Theme.Colors.redBorder; border.width: 1; anchors.horizontalCenter: parent.horizontalCenter
                                    Text { anchors.centerIn: parent; text: "DL (10ms)"; font.family: Theme.Typography.fontFamily; font.pixelSize: 7; color: Theme.Colors.red; font.weight: Font.DemiBold }
                                }
                            }
                            // X labels
                            Row {
                                anchors.bottom: parent.bottom
                                anchors.left: parent.left; anchors.leftMargin: 6
                                anchors.right: parent.right; anchors.rightMargin: 6
                                Repeater {
                                    model: ["0ms","2ms","4ms","6ms","8ms","10ms","12ms","14ms"]
                                    delegate: Text {
                                        width: (parent.width - 0)/8
                                        text: modelData
                                        font.family: Theme.Typography.fontFamily
                                        font.pixelSize: 8
                                        color: Theme.Colors.textLight
                                        horizontalAlignment: Text.AlignHCenter
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Bottom KPI strip
            Rectangle {
                width: parent.width
                height: 62
                radius: Theme.Metrics.cardRadius
                color: Theme.Colors.cardBg
                border.color: Theme.Colors.border; border.width: 1

                Row {
                    anchors.fill: parent
                    anchors.margins: 12
                    Repeater {
                        model: [
                            {label:"AVG CPU UTIL", value: systemMetrics ? systemMetrics.avgCpuUtil : ""},
                            {label:"CTX SWITCHES /s", value: systemMetrics ? systemMetrics.ctxSwitches : ""},
                            {label:"PREEMPTIONS /s", value: systemMetrics ? systemMetrics.preemptions : ""},
                            {label:"MIGRATIONS /s", value: systemMetrics ? systemMetrics.migrations : ""},
                            {label:"CAN RATE", value: systemMetrics ? systemMetrics.canRate : ""},
                            {label:"TRACE RECORDS", value: systemMetrics ? systemMetrics.traceRecords : ""},
                            {label:"DROPPED RECORDS", value: systemMetrics ? systemMetrics.droppedRecords : ""}
                        ]
                        delegate: Item {
                            width: parent.width / 7
                            height: parent.height
                            Column {
                                anchors.centerIn: parent
                                spacing: 3
                                Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.label; font.family: Theme.Typography.fontFamily; font.pixelSize: 8; color: Theme.Colors.textLight; font.weight: Font.DemiBold; font.letterSpacing: 0.3 }
                                Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.value; font.family: Theme.Typography.fontFamily; font.pixelSize: 12; color: Theme.Colors.textPrimary; font.weight: Font.Medium }
                            }
                            Rectangle { anchors.right: parent.right; width:1; height: 32; color: Theme.Colors.borderLight; anchors.verticalCenter: parent.verticalCenter; visible: index < 6 }
                        }
                    }
                }
            }
        }
    }
}

