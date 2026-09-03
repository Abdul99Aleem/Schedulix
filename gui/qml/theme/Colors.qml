pragma Singleton
import QtQuick

QtObject {
    // Backgrounds
    property color windowBg: "#f1f3f6"
    property color sidebarBg: "#ffffff"
    property color cardBg: "#ffffff"
    property color headerBg: "#ffffff"
    property color contentBg: "#f4f5f8"
    property color lightGrayBg: "#f8f9fb"

    // Borders
    property color border: "#e5e7eb"
    property color borderLight: "#eef0f3"
    property color borderMedium: "#dde1e8"

    // Text
    property color textPrimary: "#111827"
    property color textSecondary: "#4b5563"
    property color textMuted: "#6b7280"
    property color textLight: "#9aa3b2"
    property color textHeader: "#1a2744"

    // Brand / accents
    property color brandBlue: "#1e40af"
    property color accentBlue: "#2563eb"
    property color accentLightBlue: "#dbeafe"
    property color green: "#0e9f6e"
    property color greenLight: "#ecfdf5"
    property color greenBorder: "#a7f3d0"
    property color red: "#dc2626"
    property color redLight: "#fef2f2"
    property color redBorder: "#fecaca"
    property color orange: "#ea580c"
    property color orangeLight: "#fff7ed"
    property color purple: "#7c3aed"
    property color purpleLight: "#f5f3ff"
    property color teal: "#0d9488"

    // Status
    property color validGreen: "#16a34a"
    property color warnOrange: "#f59e0b"
    property color failRed: "#ef4444"

    // Charts / tasks
    property color brake: "#22c55e"
    property color brakeDark: "#16a34a"
    property color adas: "#3b82f6"
    property color adasDark: "#2563eb"
    property color diagnostic: "#a855f7"
    property color stress: "#9ca3af"
    property color idle: "#e5e7eb"
    property color canEvent: "#3b82f6"
    property color irq: "#f59e0b"

    // Sidebar selection
    property color sidebarSelectedBg: "#eef2ff"
    property color sidebarSelectedBorder: "#2563eb"
    property color sidebarSelectedText: "#1e40af"
    property color sidebarHover: "#f3f4f6"

    // Badge
    property color badgeMissBg: "#fef2f2"
    property color badgeMissText: "#dc2626"
    property color badgePassBg: "#ecfdf5"
    property color badgePassText: "#059669"
    property color badgeWarnBg: "#fffbeb"
    property color badgeWarnText: "#d97706"
    property color badgeCriticalBg: "#fef2f2"
    property color badgeCriticalText: "#dc2626"
    property color badgeCriticalBorder: "#fecaca"

    // Timeline
    property color timelineGrid: "#eef0f3"
    property color timelineRed: "#ef4444"
    property color timelineBlue: "#3b82f6"
    property color timelineGreen: "#22c55e"

    // Misc
    property color shadow: "#0f172a0d"
    property color overlay: "#00000008"
}
