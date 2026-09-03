import QtQuick
import QtQuick.Layouts
import "../theme" as Theme

Rectangle {
    id: root
    height: 56
    color: Theme.Colors.headerBg
    border.color: Theme.Colors.border
    border.width: 1

    property int currentIndex: 0
    property string scenarioText: "S0 - BASELINE"
    property bool showTabs: false
    property int activeTab: 0

    signal tabSelected(int idx)

    Text {
        id: rtosText
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        text: "RTOS PERFORMANCE"
        font.family: Theme.Typography.fontFamily
        font.pixelSize: 13
        font.weight: Font.DemiBold
        color: Theme.Colors.textPrimary
        font.letterSpacing: 0.6
    }
    Text {
        id: baselineText
        anchors.left: rtosText.right
        anchors.leftMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        text: "|  " + scenarioText
        font.family: Theme.Typography.fontFamily
        font.pixelSize: 12
        color: Theme.Colors.textMuted
        visible: !showTabs
    }

    // Tabs for screens 2-4
    Row {
        id: tabsRow
        anchors.centerIn: parent
        visible: showTabs
        spacing: 18
        Repeater {
            model: ["Dashboard", "Metrics", "Logs"]
            delegate: Item {
                width: tabText.width
                height: 20
                Text {
                    id: tabText
                    text: modelData
                    font.family: Theme.Typography.fontFamily
                    font.pixelSize: 12
                    font.weight: index === root.activeTab ? Font.DemiBold : Font.Normal
                    color: index === root.activeTab ? Theme.Colors.textPrimary : Theme.Colors.textMuted
                }
                Rectangle {
                    anchors.top: tabText.bottom
                    anchors.topMargin: 4
                    width: parent.width
                    height: 2
                    radius: 1
                    color: Theme.Colors.accentBlue
                    visible: index === root.activeTab
                }
                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.tabSelected(index) }
            }
        }
    }

    Row {
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        spacing: 12

        // QNX online + trace valid
        Column {
            spacing: 2
            Row {
                spacing: 6
                Rectangle { width: 7; height: 7; radius: 3.5; color: "#22c55e" }
                Text { text: "QNX ONLINE"; font.family: Theme.Typography.fontFamily; font.pixelSize: 9; color: Theme.Colors.textMuted; font.weight: Font.DemiBold; font.letterSpacing: 0.3 }
            }
            Row {
                spacing: 6
                Rectangle { width: 7; height: 7; radius: 3.5; color: "#22c55e" }
                Text { text: "TRACE VALID"; font.family: Theme.Typography.fontFamily; font.pixelSize: 9; color: Theme.Colors.textMuted; font.weight: Font.DemiBold; font.letterSpacing: 0.3 }
            }
        }

        // Last 60 sec dropdown
        Rectangle {
            width: 96; height: 28; radius: 8
            color: "white"; border.color: Theme.Colors.border; border.width: 1
            Row {
                anchors.centerIn: parent
                spacing: 6
                Text { text: root.currentIndex === 1 ? "Last 10 sec" : "Last 60 sec"; font.family: Theme.Typography.fontFamily; font.pixelSize: 11; color: Theme.Colors.textSecondary }
                Text { text: "v"; font.pixelSize: 10; color: Theme.Colors.textMuted }
            }
        }

        // Buttons
        Row {
            spacing: 6
            Repeater {
                model: root.currentIndex === 3 ? ["R","S","L","Export"] : ["R","S","U"]
                delegate: Rectangle {
                    width: modelData === "Export" ? 62 : 28
                    height: 28
                    radius: 8
                    color: modelData === "Export" ? Theme.Colors.textPrimary : "white"
                    border.color: Theme.Colors.border
                    border.width: 1
                    Text {
                        anchors.centerIn: parent
                        text: modelData
                        font.family: Theme.Typography.fontFamily
                        font.pixelSize: modelData === "Export" ? 11 : 12
                        font.weight: modelData === "Export" ? Font.Medium : Font.Normal
                        color: modelData === "Export" ? "white" : Theme.Colors.textSecondary
                    }
                }
            }
        }
    }
}

