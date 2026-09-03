import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "../theme" as Theme
import "../components" as Comp

Rectangle {
    id: root
    color: Theme.Colors.contentBg

    property string currentTask: jitterModel.currentTask

    Connections {
        target: jitterModel
        function onDerivedDataChanged() {
            responseCanvas.requestPaint()
            latencyCanvas.requestPaint()
        }
    }

    Column {
        id: mainCol
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        // Header Row
        Row {
            width: parent.width
            height: 36

            Column {
                spacing: 2
                Text { text: "JITTER ANALYSIS"; font.family: Theme.Typography.fontFamily; font.pixelSize: 16; font.weight: Font.Bold; color: Theme.Colors.textPrimary }
                Text { text: "Task timing variability and release-to-response behavior"; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; color: Theme.Colors.textMuted }
            }

            Item { width: parent.width - 500 - 320; height: 1 }

            // Task Selector Row
            Row {
                spacing: 8
                anchors.verticalCenter: parent.verticalCenter
                Repeater {
                    model: ["BRAKE_CTL", "ADAS_FUSION", "DIAG_POLL"]
                    delegate: Rectangle {
                        width: 96; height: 28; radius: 8
                        color: modelData === root.currentTask ? Theme.Colors.accentBlue : "white"
                        border.color: modelData === root.currentTask ? "transparent" : Theme.Colors.border
                        border.width: 1
                        Text {
                            anchors.centerIn: parent
                            text: modelData
                            font.family: Theme.Typography.fontFamily; font.pixelSize: 10
                            font.weight: modelData === root.currentTask ? Font.DemiBold : Font.Normal
                            color: modelData === root.currentTask ? "white" : Theme.Colors.textSecondary
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: jitterModel.selectTask(modelData)
                        }
                    }
                }
            }
        }

        // Cards Row
        Row {
            width: parent.width
            height: parent.height - 36 - 12
            spacing: 12

            // Left Card: Task Response-Time Jitter
            Rectangle {
                id: leftCard
                width: (parent.width - 12) / 2
                height: parent.height
                radius: Theme.Metrics.cardRadius
                color: Theme.Colors.cardBg
                border.color: Theme.Colors.border; border.width: 1
                clip: true

                Column {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 12

                    Text { text: "TASK RESPONSE-TIME JITTER"; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; font.weight: Font.DemiBold; color: Theme.Colors.textMuted; font.letterSpacing: 0.5 }

                    Item {
                        width: parent.width
                        height: parent.parent.height - 28 - 24 - 48 - 14 // total height minus margins, title, metrics and borders

                        // Y-axis labels
                        Item {
                            id: responseYAxis
                            anchors.left: parent.left
                            width: 24
                            height: parent.height - 20
                            property var yLabels: root.currentTask === "BRAKE_CTL" ? ["15", "10", "5", "0"] : (root.currentTask === "ADAS_FUSION" ? ["30", "20", "10", "0"] : ["120", "80", "40", "0"])
                            Repeater {
                                model: 4
                                delegate: Text {
                                    width: 24
                                    y: index * (parent.height / 3) - height/2
                                    text: responseYAxis.yLabels[index] + (index === 0 ? " ms" : "")
                                    font.family: Theme.Typography.fontFamily; font.pixelSize: 8; color: Theme.Colors.textLight
                                    horizontalAlignment: Text.AlignRight
                                    visible: text !== ""
                                }
                            }
                        }

                        Item {
                            x: 30
                            width: parent.width - 30
                            height: parent.height

                            // Grid lines
                            Item {
                                anchors.fill: parent
                                anchors.bottomMargin: 20
                                Repeater {
                                    model: 4
                                    delegate: Rectangle {
                                        width: parent.width; height: 1
                                        y: index * (parent.height / 3)
                                        color: Theme.Colors.borderLight; opacity: 0.7
                                    }
                                }
                            }

                            // Response Jitter Canvas
                            Canvas {
                                id: responseCanvas
                                anchors.fill: parent
                                anchors.bottomMargin: 20
                                renderTarget: Canvas.FramebufferObject
                                onPaint: {
                                    var ctx = getContext("2d"); ctx.clearRect(0,0,width,height)
                                    var maxY = root.currentTask === "BRAKE_CTL" ? 15.0 : (root.currentTask === "ADAS_FUSION" ? 30.0 : 120.0)
                                    var deadline = root.currentTask === "BRAKE_CTL" ? 10.0 : (root.currentTask === "ADAS_FUSION" ? 25.0 : 100.0)
                                    var vals = jitterModel.derivedResponseTimes
                                    var numPts = vals.length

                                    // Draw grid background line for deadline
                                    ctx.beginPath()
                                    ctx.strokeStyle = "#dc2626"
                                    ctx.lineWidth = 1.0
                                    ctx.setLineDash([4, 4])
                                    var deadlineY = (1 - deadline/maxY) * height
                                    ctx.moveTo(0, deadlineY)
                                    ctx.lineTo(width, deadlineY)
                                    ctx.stroke()
                                    ctx.setLineDash([]) // reset dash

                                    // Deadline Label
                                    ctx.fillStyle = "#dc2626"
                                    ctx.font = "8px 'Segoe UI'"
                                    ctx.textAlign = "right"
                                    ctx.fillText("DEADLINE (" + deadline + " ms)", width - 10, deadlineY - 4)

                                    // Draw line trace
                                    ctx.beginPath()
                                    ctx.strokeStyle = Theme.Colors.accentBlue
                                    ctx.lineWidth = 1.6
                                    ctx.lineJoin = "round"
                                    for (var i = 0; i < numPts; i++) {
                                        var x = (i / (numPts - 1)) * width
                                        var y = (1 - vals[i]/maxY) * height
                                        if (i === 0) ctx.moveTo(x, y)
                                        else ctx.lineTo(x, y)
                                    }
                                    ctx.stroke()

                                    // Draw dots
                                    for (var i = 0; i < numPts; i++) {
                                        var x = (i / (numPts - 1)) * width
                                        var y = (1 - vals[i]/maxY) * height
                                        var isMiss = vals[i] > deadline

                                        ctx.beginPath()
                                        ctx.arc(x, y, isMiss ? 5 : 3.5, 0, Math.PI*2)
                                        ctx.fillStyle = isMiss ? "#dc2626" : Theme.Colors.accentBlue
                                        ctx.fill()
                                        ctx.strokeStyle = "white"
                                        ctx.lineWidth = 1.0
                                        ctx.stroke()

                                        if (isMiss) {
                                            ctx.fillStyle = "#dc2626"
                                            ctx.font = "bold 8px 'Segoe UI'"
                                            ctx.textAlign = "center"
                                            ctx.fillText(vals[i].toFixed(1) + " ms", x, y - 8)
                                        }
                                    }
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
                                    model: ["A1", "A2", "A3", "A4", "A5", "A6", "A7", "A8", "A9", "A10"]
                                    delegate: Text {
                                        width: parent.width / 10
                                        text: modelData
                                        font.family: Theme.Typography.fontFamily; font.pixelSize: 8; color: Theme.Colors.textLight
                                        horizontalAlignment: Text.AlignHCenter
                                    }
                                }
                            }
                        }
                    }

                    Rectangle { width: parent.width; height: 1; color: Theme.Colors.borderLight }

                    // Metrics Row
                    Row {
                        width: parent.width
                        height: 48
                        Repeater {
                            model: [
                                { k: "P50", v: jitterModel.activeData.p50 },
                                { k: "P95", v: jitterModel.activeData.p95 },
                                { k: "P99", v: jitterModel.activeData.p99 },
                                { k: "MIN", v: jitterModel.activeData.min },
                                { k: "MAX", v: jitterModel.activeData.max },
                                { k: "RESPONSE JITTER", v: jitterModel.activeData.jitter, highlight: true }
                            ]
                            delegate: Item {
                                width: parent.width / 6
                                height: parent.height
                                Column {
                                    anchors.centerIn: parent
                                    spacing: 2
                                    Text { text: modelData.k; font.family: Theme.Typography.fontFamily; font.pixelSize: 8; color: Theme.Colors.textLight; font.weight: Font.DemiBold; anchors.horizontalCenter: parent.horizontalCenter }
                                    Text { text: modelData.v; font.family: Theme.Typography.fontFamily; font.pixelSize: 12; font.weight: Font.Bold; color: modelData.highlight ? Theme.Colors.red : Theme.Colors.textPrimary; anchors.horizontalCenter: parent.horizontalCenter }
                                }
                                Rectangle { anchors.right: parent.right; width: 1; height: 24; color: Theme.Colors.borderLight; anchors.verticalCenter: parent.verticalCenter; visible: index < 5 }
                            }
                        }
                    }
                }
            }

            // Right Card: Release -> Response Latency
            Rectangle {
                id: rightCard
                width: (parent.width - 12) / 2
                height: parent.height
                radius: Theme.Metrics.cardRadius
                color: Theme.Colors.cardBg
                border.color: Theme.Colors.border; border.width: 1
                clip: true

                Column {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 12

                    Text { text: "RELEASE → RESPONSE LATENCY"; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; font.weight: Font.DemiBold; color: Theme.Colors.textMuted; font.letterSpacing: 0.5 }

                    Item {
                        width: parent.width
                        height: parent.parent.height - 28 - 24 - 48 - 14

                        // Y-axis labels
                        Item {
                            id: latencyYAxis
                            anchors.left: parent.left
                            width: 24
                            height: parent.height - 20
                            property var yLabels: root.currentTask === "BRAKE_CTL" ? ["3.0", "2.0", "1.0", "0"] : (root.currentTask === "ADAS_FUSION" ? ["6.0", "4.0", "2.0", "0"] : ["12.0", "8.0", "4.0", "0"])
                            Repeater {
                                model: 4
                                delegate: Text {
                                    width: 24
                                    y: index * (parent.height / 3) - height/2
                                    text: latencyYAxis.yLabels[index] + (index === 0 ? " ms" : "")
                                    font.family: Theme.Typography.fontFamily; font.pixelSize: 8; color: Theme.Colors.textLight
                                    horizontalAlignment: Text.AlignRight
                                }
                            }
                        }

                        Item {
                            x: 30
                            width: parent.width - 30
                            height: parent.height

                            // Grid lines
                            Item {
                                anchors.fill: parent
                                anchors.bottomMargin: 20
                                Repeater {
                                    model: 4
                                    delegate: Rectangle {
                                        width: parent.width; height: 1
                                        y: index * (parent.height / 3)
                                        color: Theme.Colors.borderLight; opacity: 0.7
                                    }
                                }
                            }

                            // Latency Canvas
                            Canvas {
                                id: latencyCanvas
                                anchors.fill: parent
                                anchors.bottomMargin: 20
                                renderTarget: Canvas.FramebufferObject
                                onPaint: {
                                    var ctx = getContext("2d"); ctx.clearRect(0,0,width,height)
                                    var maxY = root.currentTask === "BRAKE_CTL" ? 3.0 : (root.currentTask === "ADAS_FUSION" ? 6.0 : 12.0)
                                    var vals = jitterModel.derivedLatencyOffsets
                                    var numPts = vals.length

                                    // Calculate Mean
                                    var sum = 0
                                    for(var i=0; i<numPts; i++) sum += vals[i]
                                    var mean = sum / numPts

                                    // Draw Mean line
                                    ctx.beginPath()
                                    ctx.strokeStyle = "#16a34a"
                                    ctx.lineWidth = 1.0
                                    ctx.setLineDash([3, 3])
                                    var meanY = (1 - mean/maxY) * height
                                    ctx.moveTo(0, meanY)
                                    ctx.lineTo(width, meanY)
                                    ctx.stroke()
                                    ctx.setLineDash([]) // reset

                                    // Mean label
                                    ctx.fillStyle = "#16a34a"
                                    ctx.font = "8px 'Segoe UI'"
                                    ctx.textAlign = "right"
                                    ctx.fillText("MEAN (" + mean.toFixed(2) + " ms)", width - 10, meanY - 4)

                                    // Draw line trace
                                    ctx.beginPath()
                                    ctx.strokeStyle = Theme.Colors.accentBlue
                                    ctx.lineWidth = 1.6
                                    ctx.lineJoin = "round"
                                    for (var i = 0; i < numPts; i++) {
                                        var x = (i / (numPts - 1)) * width
                                        var y = (1 - vals[i]/maxY) * height
                                        if (i === 0) ctx.moveTo(x, y)
                                        else ctx.lineTo(x, y)
                                    }
                                    ctx.stroke()

                                    // Draw dots
                                    for (var i = 0; i < numPts; i++) {
                                        var x = (i / (numPts - 1)) * width
                                        var y = (1 - vals[i]/maxY) * height

                                        ctx.beginPath()
                                        ctx.arc(x, y, 3.5, 0, Math.PI*2)
                                        ctx.fillStyle = Theme.Colors.accentBlue
                                        ctx.fill()
                                        ctx.strokeStyle = "white"
                                        ctx.lineWidth = 1.0
                                        ctx.stroke()
                                    }
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
                                    model: ["A1", "A2", "A3", "A4", "A5", "A6", "A7", "A8", "A9", "A10"]
                                    delegate: Text {
                                        width: parent.width / 10
                                        text: modelData
                                        font.family: Theme.Typography.fontFamily; font.pixelSize: 8; color: Theme.Colors.textLight
                                        horizontalAlignment: Text.AlignHCenter
                                    }
                                }
                            }
                        }
                    }

                    Rectangle { width: parent.width; height: 1; color: Theme.Colors.borderLight }

                    // Metrics Row
                    Row {
                        width: parent.width
                        height: 48
                        Repeater {
                            model: [
                                { k: "MEAN RELEASE OFFSET", v: jitterModel.activeData.meanOffset },
                                { k: "MIN", v: jitterModel.activeData.releaseMin },
                                { k: "MAX", v: jitterModel.activeData.releaseMax },
                                { k: "LATENCY JITTER", v: jitterModel.activeData.releaseJitter, highlight: true }
                            ]
                            delegate: Item {
                                width: parent.width / 4
                                height: parent.height
                                Column {
                                    anchors.centerIn: parent
                                    spacing: 2
                                    Text { text: modelData.k; font.family: Theme.Typography.fontFamily; font.pixelSize: 8; color: Theme.Colors.textLight; font.weight: Font.DemiBold; anchors.horizontalCenter: parent.horizontalCenter }
                                    Text { text: modelData.v; font.family: Theme.Typography.fontFamily; font.pixelSize: 12; font.weight: Font.Bold; color: modelData.highlight ? Theme.Colors.accentBlue : Theme.Colors.textPrimary; anchors.horizontalCenter: parent.horizontalCenter }
                                }
                                Rectangle { anchors.right: parent.right; width: 1; height: 24; color: Theme.Colors.borderLight; anchors.verticalCenter: parent.verticalCenter; visible: index < 3 }
                            }
                        }
                    }
                }
            }
        }
    }
}
