import QtQuick
import QtQuick.Layouts
import "../theme" as Theme

Rectangle {
    id: root
    color: Theme.Colors.sidebarBg
    border.color: Theme.Colors.border
    border.width: 1

    property int currentIndex: 0
    signal navigate(int index)

    // Logo area
    Column {
        id: logoArea
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: 14
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        spacing: 0

        Row {
            spacing: 10
            Rectangle {
                width: 34
                height: 34
                radius: 8
                color: "#1e40af"
                // S logo
                Text {
                    anchors.centerIn: parent
                    text: "S"
                    color: "white"
                    font.family: Theme.Typography.fontFamily
                    font.pixelSize: 20
                    font.weight: Font.Bold
                    font.italic: true
                }
                // small highlight
                Rectangle {
                    width: 10; height: 10; radius: 5
                    color: "#3b82f6"; opacity: 0.9
                    anchors.top: parent.top; anchors.right: parent.right
                    anchors.topMargin: -2; anchors.rightMargin: -2
                    visible: false
                }
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 1
                Text {
                    text: "SCHEDULIX"
                    font.family: Theme.Typography.fontFamily
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    color: Theme.Colors.textPrimary
                    font.letterSpacing: 0.8
                }
                Text {
                    text: "Automotive RTOS"
                    font.family: Theme.Typography.fontFamily
                    font.pixelSize: 9
                    color: Theme.Colors.textMuted
                    lineHeight: 0.9
                }
                Text {
                    text: "Performance Analyzer"
                    font.family: Theme.Typography.fontFamily
                    font.pixelSize: 9
                    color: Theme.Colors.textMuted
                    lineHeight: 0.9
                }
            }
        }
    }

    // Navigation list
    Column {
        id: navColumn
        anchors.top: logoArea.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: 22
        spacing: 2

        Repeater {
            model: [
                { label: "Overview", iconType: "overview", idx: 0 },
                { label: "Workloads", iconType: "workloads", idx: -1 },
                { label: "Timeline", iconType: "timeline", idx: 1 },
                { label: "CAN Events", iconType: "can", idx: -1 },
                { label: "Scheduling", iconType: "scheduling", idx: -1 },
                { label: "Jitter", iconType: "jitter", idx: 4 },
                { label: "Root Cause", iconType: "rootcause", idx: 2 },
                { label: "Experiments", iconType: "experiments", idx: 3 },
                { label: "Hardware", iconType: "hardware", idx: -1 },
                { label: "Trace Health", iconType: "tracehealth", idx: -1 }
            ]
            delegate: Rectangle {
                width: parent.width
                height: 34
                color: {
                    if (modelData.idx === root.currentIndex) return Theme.Colors.sidebarSelectedBg
                    else if (ma.containsMouse) return Theme.Colors.sidebarHover
                    else return "transparent"
                }
                // left accent for selected
                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 3
                    color: Theme.Colors.accentBlue
                    visible: modelData.idx === root.currentIndex
                }
                Row {
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 10
                    SidebarIcon {
                        iconType: modelData.iconType
                        color: modelData.idx === root.currentIndex ? Theme.Colors.accentBlue : Theme.Colors.textMuted
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: modelData.label
                        font.family: Theme.Typography.fontFamily
                        font.pixelSize: 12
                        font.weight: modelData.idx === root.currentIndex ? Font.DemiBold : Font.Normal
                        color: modelData.idx === root.currentIndex ? Theme.Colors.sidebarSelectedText : Theme.Colors.textSecondary
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
                MouseArea {
                    id: ma
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: modelData.idx >= 0 ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: if (modelData.idx >= 0) root.navigate(modelData.idx)
                }
            }
        }
    }

    // Bottom info
    Column {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottomMargin: 14
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        spacing: 14

        // divider
        Rectangle { width: parent.width; height: 1; color: Theme.Colors.borderLight }

        Column {
            spacing: 6
            width: parent.width
            Row {
                spacing: 8
                Rectangle { width: 18; height: 18; radius: 4; color: "#eef2ff"; border.color: "#c7d2fe"; border.width: 1
                    Text { anchors.centerIn: parent; text: "E"; font.pixelSize: 10; color: "#4f46e5" }
                }
                Column {
                    spacing: 1
                    Text { text: "CURRENT EXPERIMENT"; font.family: Theme.Typography.fontFamily; font.pixelSize: 8; color: Theme.Colors.textLight; font.weight: Font.DemiBold; font.letterSpacing: 0.5 }
                    Text { text: "S0 - Baseline"; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; color: Theme.Colors.textPrimary; font.weight: Font.Medium }
                }
            }
            Row {
                spacing: 8
                Rectangle { width: 18; height: 18; radius: 4; color: "#f3f4f6"; border.color: Theme.Colors.border; border.width: 1
                    Text { anchors.centerIn: parent; text: "H"; font.pixelSize: 9; color: Theme.Colors.textMuted }
                }
                Column {
                    spacing: 1
                    Text { text: "SYSTEM"; font.family: Theme.Typography.fontFamily; font.pixelSize: 8; color: Theme.Colors.textLight; font.weight: Font.DemiBold; font.letterSpacing: 0.5 }
                    Text { text: "QNX Neutrino / RPi 4"; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; color: Theme.Colors.textSecondary }
                    Text { text: "4 CPUs"; font.family: Theme.Typography.fontFamily; font.pixelSize: 10; color: Theme.Colors.textMuted }
                }
            }
            Row {
                spacing: 8
                Rectangle {
                    width: 18; height: 18; radius: 9; color: "#dcfce7"; border.color: "#86efac"; border.width: 1
                    Text { anchors.centerIn: parent; text: "v"; font.pixelSize: 10; color: "#16a34a"; font.weight: Font.Bold }
                }
                Column {
                    spacing: 1
                    Text { text: "TRACE STATUS"; font.family: Theme.Typography.fontFamily; font.pixelSize: 8; color: Theme.Colors.textLight; font.weight: Font.DemiBold; font.letterSpacing: 0.5 }
                    Text { text: "VALID"; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; color: "#16a34a"; font.weight: Font.DemiBold }
                }
            }
        }
    }
}

