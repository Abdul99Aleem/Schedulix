import QtQuick
import "../theme" as Theme

Rectangle {
    id: root
    property string text: ""
    property string type: "miss" // miss, pass, warn, fail, critical

    implicitWidth: badgeText.width + 12
    implicitHeight: 18
    radius: 4
    border.width: 1

    color: {
        if (type === "miss" || type === "fail" || type === "critical") return Theme.Colors.badgeMissBg
        if (type === "pass") return Theme.Colors.badgePassBg
        if (type === "warn") return Theme.Colors.badgeWarnBg
        return "#f3f4f6"
    }
    border.color: {
        if (type === "miss" || type === "fail" || type === "critical") return Theme.Colors.badgeMissText + "30"
        if (type === "pass") return Theme.Colors.badgePassText + "30"
        if (type === "warn") return Theme.Colors.badgeWarnText + "30"
        return Theme.Colors.border
    }

    Text {
        id: badgeText
        anchors.centerIn: parent
        text: root.text
        font.family: Theme.Typography.fontFamily
        font.pixelSize: 9
        font.weight: Font.DemiBold
        font.letterSpacing: 0.3
        color: {
            if (root.type === "miss" || root.type === "fail" || root.type === "critical") return Theme.Colors.badgeMissText
            if (root.type === "pass") return Theme.Colors.badgePassText
            if (root.type === "warn") return Theme.Colors.badgeWarnText
            return Theme.Colors.textMuted
        }
    }
}

