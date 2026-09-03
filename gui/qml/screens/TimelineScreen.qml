import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "../theme" as Theme
import "../components" as Comp

Rectangle {
    id: root
    color: Theme.Colors.contentBg

    Row {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 0

        // Main timeline area
        Item {
            width: parent.width - 272
            height: parent.height

            Column {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.topMargin: 14
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 10

                // Toolbar
                Row {
                    width: parent.width
                    spacing: 10
                    height: 28

                    Row {
                        spacing: 6
                        Rectangle { width: 28; height: 28; radius: 8; color: "white"; border.color: Theme.Colors.border; border.width: 1; Text { anchors.centerIn: parent; text: "+"; font.pixelSize: 12; color: Theme.Colors.textSecondary } }
                        Rectangle { width: 28; height: 28; radius: 8; color: "white"; border.color: Theme.Colors.border; border.width: 1; Text { anchors.centerIn: parent; text: "-"; font.pixelSize: 12; color: Theme.Colors.textSecondary } }
                        Rectangle { width: 52; height: 28; radius: 8; color: "white"; border.color: Theme.Colors.border; border.width: 1; Text { anchors.centerIn: parent; text: "100%"; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; color: Theme.Colors.textSecondary } }
                        Rectangle { width: 52; height: 28; radius: 8; color: "white"; border.color: Theme.Colors.border; border.width: 1; Row { anchors.centerIn: parent; spacing: 4; Text { text: "[]"; font.pixelSize: 11; color: Theme.Colors.textMuted } Text { text: "Fit"; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; color: Theme.Colors.textSecondary } } }
                    }
                    Rectangle { width: 1; height: 18; color: Theme.Colors.border }

                    Text { text: "Display:"; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; color: Theme.Colors.textMuted }
                    Rectangle {
                        width: 138; height: 28; radius: 8; color: "white"; border.color: Theme.Colors.border; border.width: 1
                        Row { anchors.centerIn: parent; spacing: 6; Text { text: "All Resources"; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; color: Theme.Colors.textSecondary } Text { text: "v"; font.pixelSize: 9; color: Theme.Colors.textMuted } }
                    }
                    Row {
                        spacing: 6
                        Rectangle { width: 14; height: 14; radius: 3; color: "white"; border.color: Theme.Colors.border; border.width: 1; }
                        Text { text: "Show Events"; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; color: Theme.Colors.textSecondary }
                    }
                    Row {
                        spacing: 6
                        Rectangle { width: 14; height: 14; radius: 3; color: Theme.Colors.accentBlue; border.color: Theme.Colors.accentBlue; border.width: 1; Text { anchors.centerIn: parent; text: "v"; font.pixelSize: 9; color: "white"; font.weight: Font.Bold } }
                        Text { text: "Show Critical Path"; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; color: Theme.Colors.textSecondary }
                    }
                    Item { width: 12; height: 1 }
                    Text { text: "Cursor: 1.463009 s"; font.family: Theme.Typography.fontFamily; font.pixelSize: 10; color: Theme.Colors.textLight }
                    Item { width: 20; height: 1 } // spacer

                    Rectangle {
                        width: 72; height: 26; radius: 14; color: "#fef2f2"; border.color: Theme.Colors.redBorder; border.width: 1
                        Text { anchors.centerIn: parent; text: "Next Miss"; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; color: Theme.Colors.red; font.weight: Font.Medium }
                    }
                }

                // Timeline card
                Rectangle {
                    width: parent.width
                    height: 520
                    radius: Theme.Metrics.cardRadius
                    color: Theme.Colors.cardBg
                    border.color: Theme.Colors.border; border.width: 1
                    clip: true

                    Column {
                        anchors.fill: parent
                        anchors.margins: 0
                        spacing: 0

                        // Time axis header
                        Item {
                            width: parent.width; height: 30
                            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.Colors.borderLight }
                            Row {
                                anchors.fill: parent
                                Rectangle { width: 72; height: parent.height
                                    Text { anchors.centerIn: parent; text: "CPU"; font.family: Theme.Typography.fontFamily; font.pixelSize: 9; color: Theme.Colors.textLight; font.weight: Font.Medium }
                                }
                                Item {
                                    width: parent.width - 72; height: parent.height
                                    Row {
                                        width: parent.width; height: parent.height
                                        Repeater {
                                            model: ["1.4520 s","1.4530 s","1.4536 s","1.4540 s","1.4540 s","1.4544 s","1.4546 s","1.4548 s"]
                                            delegate: Item { width: parent.width/8; height: parent.height
                                                Text { anchors.centerIn: parent; text: modelData; font.family: Theme.Typography.fontFamily; font.pixelSize: 8; color: Theme.Colors.textLight }
                                                Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: Theme.Colors.borderLight; opacity: 0.5 }
                                            }
                                        }
                                    }
                                }
                            }
                            // vertical cursor line
                            Rectangle { x: 72 + (parent.width-72)*0.48; width: 1; height: parent.height+1000; color: Theme.Colors.red; opacity: 0.35 }
                        }

                        // CPU rows
                        Column {
                            width: parent.width
                            Repeater {
                                model: timelineModel ? timelineModel.cpuLanes : []
                                delegate: Item {
                                    width: parent.width; height: 38
                                    Rectangle { width: parent.width; height: 1; color: Theme.Colors.borderLight; opacity: 0.6; anchors.top: parent.top }
                                    Row {
                                        anchors.fill: parent
                                        Rectangle { width: 72; height: parent.height; color: "#f9fafb"; border.width: 0
                                            Text { anchors.centerIn: parent; text: modelData.label; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; color: Theme.Colors.textPrimary; font.weight: Font.Medium }
                                        }
                                        Item {
                                            width: parent.width - 72; height: parent.height
                                            // grid lines
                                            Row {
                                                anchors.fill: parent
                                                Repeater { model: 8; delegate: Rectangle { width: parent.width/8; height: parent.height; color: "transparent"; border.color: Theme.Colors.borderLight; border.width: 0
                                                        Rectangle { anchors.right: parent.right; width:1; height: parent.height; color: Theme.Colors.borderLight; opacity: 0.4 } } }
                                            }
                                            Repeater {
                                                model: modelData.bars
                                                delegate: Rectangle {
                                                    x: parent.width * modelData.x + 2
                                                    y: 9
                                                    width: parent.width * modelData.w
                                                    height: 20
                                                    radius: 4
                                                    color: modelData.c
                                                    Text { anchors.centerIn: parent; text: modelData.l; font.family: Theme.Typography.fontFamily; font.pixelSize: 9; color: modelData.tc ? modelData.tc : "white"; font.weight: Font.Medium }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // Critical tasks divider
                        Rectangle { width: parent.width; height: 20; color: "#f9fafb"; border.color: Theme.Colors.borderLight; border.width: 0
                            Rectangle { width: parent.width; height: 1; color: Theme.Colors.border; anchors.top: parent.top }
                            Text { anchors.left: parent.left; anchors.leftMargin: 12; anchors.verticalCenter: parent.verticalCenter; text: "CRITICAL TASKS"; font.family: Theme.Typography.fontFamily; font.pixelSize: 9; color: Theme.Colors.textLight; font.weight: Font.DemiBold; font.letterSpacing: 0.4 }
                            Rectangle { width: parent.width; height: 1; color: Theme.Colors.borderLight; anchors.bottom: parent.bottom }
                        }

                        // BRAKE_CTL track
                        Rectangle {
                            width: parent.width; height: 54
                            Row {
                                anchors.fill: parent
                                Rectangle { width: 72; height: parent.height; color: "#f9fafb"
                                    Text { anchors.centerIn: parent; text: "BRAKE_CTL"; font.family: Theme.Typography.fontFamily; font.pixelSize: 9; color: "#dc2626"; font.weight: Font.Medium }
                                }
                                Item {
                                    width: parent.width - 72; height: parent.height
                                    // grid
                                    Row { anchors.fill: parent; Repeater { model:8; delegate: Rectangle { width: parent.width/8; height: parent.height; color: "transparent"; Rectangle { anchors.right: parent.right; width:1; height:parent.height; color:Theme.Colors.borderLight; opacity:0.4 } } } }
                                    // markers
                                    // RELEASE
                                    Column { x: parent.width*0.18; y: 6; spacing: 2; width: 30
                                        Rectangle { width: 10; height: 10; rotation: 45; color: "#22c55e"; anchors.horizontalCenter: parent.horizontalCenter; border.color: "white"; border.width: 1 }
                                        Text { text: "RELEASE"; font.family: Theme.Typography.fontFamily; font.pixelSize: 7; color: Theme.Colors.textLight; anchors.horizontalCenter: parent.horizontalCenter }
                                    }
                                    Column { x: parent.width*0.28; y:6; spacing:2; width:30
                                        Rectangle { width: 10; height:10; rotation:45; color: "#3b82f6"; anchors.horizontalCenter: parent.horizontalCenter; border.color:"white"; border.width:1 }
                                        Text { text:"READY"; font.family: Theme.Typography.fontFamily; font.pixelSize:7; color: Theme.Colors.textLight; anchors.horizontalCenter: parent.horizontalCenter }
                                    }
                                    // PREEMPTED bar
                                    Rectangle {
                                        x: parent.width*0.36; y: 18; width: parent.width*0.18; height: 14; radius: 3; color: "#fecaca"; border.color: "#f87171"; border.width: 1
                                        Text { anchors.centerIn: parent; text: "PREEMPTED (ADAS_1)"; font.family: Theme.Typography.fontFamily; font.pixelSize: 7; color: "#dc2626"; font.weight: Font.Medium }
                                    }
                                    // RUNNING bar
                                    Rectangle {
                                        x: parent.width*0.58; y:18; width: parent.width*0.14; height:14; radius:3; color:"#bbf7d0"; border.color:"#22c55e"; border.width:1
                                        Text { anchors.centerIn: parent; text:"RUNNING"; font.family: Theme.Typography.fontFamily; font.pixelSize:7; color:"#166534"; font.weight:Font.Medium }
                                    }
                                    // FINISH
                                    Column { x: parent.width*0.74; y:6; spacing:2; width:30
                                        Rectangle { width:10; height:10; rotation:45; color:"#3b82f6"; anchors.horizontalCenter: parent.horizontalCenter; border.color:"white"; border.width:1 }
                                        Text { text:"FINISH"; font.family: Theme.Typography.fontFamily; font.pixelSize:7; color:Theme.Colors.textLight; anchors.horizontalCenter: parent.horizontalCenter }
                                    }
                                    Column { x: parent.width*0.82; y:6; spacing:2; width:40
                                        Rectangle { width:10; height:10; radius:5; color:"#ef4444"; anchors.horizontalCenter: parent.horizontalCenter; border.color:"white"; border.width:1; Text { anchors.centerIn: parent; text:"!"; font.pixelSize:7; color:"white"; font.weight:Font.Bold } }
                                        Text { text:"DEADLINE MISS"; font.family: Theme.Typography.fontFamily; font.pixelSize:7; color:Theme.Colors.red; font.weight:Font.Bold; anchors.horizontalCenter: parent.horizontalCenter }
                                    }
                                    // duration labels
                                    Text { x: parent.width*0.31; y: 38; text:"2.1 ms"; font.family: Theme.Typography.fontFamily; font.pixelSize:8; color: Theme.Colors.textLight }
                                    Text { x: parent.width*0.44; y: 38; text:"8.8 ms"; font.family: Theme.Typography.fontFamily; font.pixelSize:8; color: Theme.Colors.textLight }
                                    Text { x: parent.width*0.64; y: 38; text:"4.2 ms"; font.family: Theme.Typography.fontFamily; font.pixelSize:8; color: Theme.Colors.textLight }
                                    // dashed lines between markers?
                                    Canvas {
                                        anchors.fill: parent
                                        onPaint: {
                                            var ctx=getContext("2d")
                                            ctx.clearRect(0,0,width,height)
                                            ctx.strokeStyle="#cbd5e1"
                                            ctx.setLineDash([3,3])
                                            ctx.lineWidth=1
                                            ctx.beginPath()
                                            ctx.moveTo(width*0.19, 22); ctx.lineTo(width*0.29,22)
                                            ctx.moveTo(width*0.30, 25); ctx.lineTo(width*0.36,25)
                                            ctx.moveTo(width*0.54,25); ctx.lineTo(width*0.58,25)
                                            ctx.stroke()
                                        }
                                    }
                                }
                            }
                        }

                        // ADAS_FUSION row
                        Rectangle {
                            width: parent.width; height: 28
                            Row {
                                anchors.fill: parent
                                Rectangle { width: 72; height: parent.height; color:"#f9fafb"; Text { anchors.centerIn: parent; text:"ADAS_FUSION"; font.family: Theme.Typography.fontFamily; font.pixelSize:8; color:Theme.Colors.textSecondary } }
                                Item {
                                    width: parent.width-72; height:parent.height
                                    Row { anchors.fill: parent; Repeater { model:8; delegate: Rectangle{width:parent.width/8; height:parent.height; color:"transparent"; Rectangle{anchors.right:parent.right; width:1; height:parent.height; color:Theme.Colors.borderLight; opacity:0.4}}}}
                                    Rectangle { x: parent.width*0.15; y:7; width: parent.width*0.24; height:14; radius:3; color:"#1f2937" }
                                    Rectangle { x: parent.width*0.52; y:7; width: parent.width*0.34; height:14; radius:3; color:"#111827" }
                                }
                            }
                        }
                        // DIAG_POLL row
                        Rectangle {
                            width: parent.width; height: 28
                            Row {
                                anchors.fill: parent
                                Rectangle { width: 72; height: parent.height; color:"#f9fafb"; Text{anchors.centerIn: parent; text:"DIAG_POLL"; font.family: Theme.Typography.fontFamily; font.pixelSize:8; color:Theme.Colors.textSecondary}}
                                Item {
                                    width: parent.width-72; height:parent.height
                                    Row{anchors.fill:parent; Repeater{model:8; delegate:Rectangle{width:parent.width/8; height:parent.height; color:"transparent"; Rectangle{anchors.right:parent.right; width:1; height:parent.height; color:Theme.Colors.borderLight; opacity:0.4}}}}
                                    Rectangle{ x: parent.width*0.20; y:7; width: parent.width*0.18; height:14; radius:3; color:"#7c3aed"}
                                }
                            }
                        }

                        // EVENTS divider
                        Rectangle { width: parent.width; height: 20; color:"#f9fafb"; Text{anchors.left:parent.left; anchors.leftMargin:12; anchors.verticalCenter:parent.verticalCenter; text:"EVENTS"; font.family:Theme.Typography.fontFamily; font.pixelSize:9; color:Theme.Colors.textLight; font.weight:Font.DemiBold } Rectangle{width:parent.width; height:1; color:Theme.Colors.borderLight; anchors.top:parent.top} Rectangle{width:parent.width; height:1; color:Theme.Colors.borderLight; anchors.bottom:parent.bottom} }

                        // CAN row
                        Rectangle {
                            width: parent.width; height: 24
                            Row {
                                anchors.fill: parent
                                Rectangle { width: 72; height: parent.height; color:"#f9fafb"; Text{anchors.centerIn:parent; text:"CAN"; font.family:Theme.Typography.fontFamily; font.pixelSize:9; color:Theme.Colors.textSecondary}}
                                Item{width:parent.width-72; height:parent.height; Row{anchors.fill:parent; Repeater{model:8; delegate:Rectangle{width:parent.width/8; height:parent.height; color:"transparent"; Rectangle{anchors.right:parent.right; width:1; height:parent.height; color:Theme.Colors.borderLight; opacity:0.4}}}}
                                    Repeater {
                                        model: timelineModel ? timelineModel.canEvents : []
                                        delegate: Item {
                                            x: parent.width * modelData - 5
                                            y: 7
                                            Rectangle{ width:10; height:10; radius:1; color:Theme.Colors.accentBlue; rotation:45; border.color:"white"; border.width:1 }
                                            Text{ x: 13; y: 0; text: index === 0 ? "CAN 0x100" : "CAN 0x101"; font.family:Theme.Typography.fontFamily; font.pixelSize:8; color:Theme.Colors.textSecondary}
                                        }
                                    }
                                }
                            }
                        }
                        // IRQ row
                        Rectangle{
                            width:parent.width; height:24
                            Row{
                                anchors.fill:parent
                                Rectangle { width: 72; height: parent.height; color:"#f9fafb"; Text{anchors.centerIn:parent; text:"IRQ"; font.family:Theme.Typography.fontFamily; font.pixelSize:9; color:Theme.Colors.textSecondary}}
                                Item{width:parent.width-72; height:parent.height; Row{anchors.fill:parent; Repeater{model:8; delegate:Rectangle{width:parent.width/8; height:parent.height; color:"transparent"; Rectangle{anchors.right:parent.right; width:1; height:parent.height; color:Theme.Colors.borderLight; opacity:0.4}}}}
                                    Repeater{ model: timelineModel ? timelineModel.irqs : []; delegate: Rectangle{ x: parent.width*modelData; y:6; width:12; height:12; radius:6; color:"#fdba74"; border.color:"#fb923c"; border.width:1; Text{anchors.centerIn:parent; text:"!"; font.pixelSize:7} } }
                                }
                            }
                        }
                        // GPIO
                        Rectangle{
                            width:parent.width; height:24
                            Row{
                                anchors.fill:parent
                                Rectangle { width: 72; height: parent.height; color:"#f9fafb"; Text{anchors.centerIn:parent; text:"GPIO MARKER"; font.family:Theme.Typography.fontFamily; font.pixelSize:7; color:Theme.Colors.textSecondary; wrapMode:Text.WordWrap; horizontalAlignment:Text.AlignHCenter; width:60}}
                                Item{width:parent.width-72; height:parent.height; Row{anchors.fill:parent; Repeater{model:8; delegate:Rectangle{width:parent.width/8; height:parent.height; color:"transparent"; Rectangle{anchors.right:parent.right; width:1; height:parent.height; color:Theme.Colors.borderLight; opacity:0.4}}}}
                                    Repeater{ model: timelineModel ? timelineModel.gpioMarkers : []; delegate: Rectangle{ x: parent.width*modelData; y:9; width: parent.width*0.06; height:4; radius:2; color:"#cbd5e1"} }
                                }
                            }
                        }
                    }
                }
            }
        }

        // Inspector panel
        Rectangle {
            width: 272
            height: parent.height
            color: Theme.Colors.cardBg
            border.color: Theme.Colors.border
            border.width: 1

            Flickable {
                anchors.fill: parent
                anchors.margins: 14
                contentHeight: inspectorCol.height
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 6 }

                Column {
                    id: inspectorCol
                    width: parent.width
                    spacing: 12

                    Text { text: "INSPECTOR"; font.family: Theme.Typography.fontFamily; font.pixelSize: 10; font.weight: Font.DemiBold; color: Theme.Colors.textMuted; font.letterSpacing: 0.5 }

                    Row {
                        width: parent.width; spacing: 6
                        Text { text: "SELECTED: BRAKE #482"; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; font.weight: Font.Bold; color: Theme.Colors.textPrimary }
                        Rectangle { width:12; height:12; radius:2; color:"#fef2f2"; border.color:Theme.Colors.redBorder; border.width:1; Text{anchors.centerIn:parent; text:"!"; font.pixelSize:8; color:Theme.Colors.red} }
                    }
                    Column {
                        spacing: 2
                        Text { text: "Task: BRAKE_CTL"; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; color: Theme.Colors.textSecondary }
                        Text { text: "PID: 1042    PRI: 20"; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; color: Theme.Colors.textMuted }
                    }

                    Rectangle { width: parent.width; height: 1; color: Theme.Colors.borderLight }

                    Text { text: "TIMING DETAILS"; font.family: Theme.Typography.fontFamily; font.pixelSize: 9; font.weight: Font.DemiBold; color: Theme.Colors.textMuted; font.letterSpacing: 0.4 }

                    Column {
                        width: parent.width; spacing: 6
                        Repeater {
                            model: [
                                {k:"Release:", v:"1.452301294 s"},
                                {k:"Ready:", v:"1.452303284 s"},
                                {k:"Start (Run):", v:"1.452318184 s"},
                                {k:"Preempted:", v:"1.452350000 s"},
                                {k:"Resume:", v:"1.452357065 s"},
                                {k:"Finish:", v:"1.452364889 s"},
                                {k:"Response Time:", v:"12.8 ms", c: Theme.Colors.red},
                                {k:"Deadline:", v:"10.0 ms"},
                                {k:"Violation:", v:"+2.8 ms", c: Theme.Colors.red},
                                {k:"Exec Time:", v:"4.2 ms"},
                                {k:"CPU:", v:"0 -> 0"}
                            ]
                            delegate: Row {
                                width: parent.width
                                Text { width: 110; text: modelData.k; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; color: Theme.Colors.textMuted }
                                Text { text: modelData.v; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; color: modelData.c ? modelData.c : Theme.Colors.textPrimary; font.weight: modelData.c ? Font.Bold : Font.Normal }
                            }
                        }
                    }

                    Rectangle { width: parent.width; height: 1; color: Theme.Colors.borderLight }

                    Text { text: "CORRELATION"; font.family: Theme.Typography.fontFamily; font.pixelSize: 9; font.weight: Font.DemiBold; color: Theme.Colors.textMuted; font.letterSpacing: 0.4 }

                    Column {
                        width: parent.width; spacing: 6
                        Repeater {
                            model: [
                                {k:"CAN:", v:"0x100"},
                                {k:"Trace ID:", v:"trace_8812a"},
                                {k:"IRQ Vector:", v:"34  (12us)"},
                                {k:"Correlation ID:", v:"BRAKE-482-CAN100", small:true}
                            ]
                            delegate: Column {
                                width: parent.width; spacing: 2
                                Text { text: modelData.k; font.family: Theme.Typography.fontFamily; font.pixelSize: 10; color: Theme.Colors.textLight }
                                Text { text: modelData.v; font.family: Theme.Typography.fontFamily; font.pixelSize: modelData.small ? 10 : 11; color: Theme.Colors.textPrimary; font.weight: Font.Medium }
                            }
                        }
                    }

                    Rectangle {
                        width: parent.width; height: 36; radius: 8; color: "#1e3a5f"
                        Text { anchors.centerIn: parent; text: "View Full Trace"; font.family: Theme.Typography.fontFamily; font.pixelSize: 12; color: "white"; font.weight: Font.Medium }
                    }

                    Item { width:1; height: 20 }
                }
            }
        }
    }
}


