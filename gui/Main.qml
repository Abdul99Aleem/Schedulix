import QtQuick
import QtQuick.Controls
import "qml/theme" as Theme
import "qml/components" as Comp
import "qml/screens" as Screens

ApplicationWindow {
    id: window
    width: 1536
    height: 864
    minimumWidth: 1280
    minimumHeight: 720
    visible: true
    title: "Schedulix - Automotive RTOS Performance Analyzer"
    color: Theme.Colors.windowBg

    property int currentScreen: 0 // 0 dashboard, 1 timeline, 2 rootcause, 3 experiments

    Row {
        anchors.fill: parent
        spacing: 0

        Comp.Sidebar {
            id: sidebar
            width: Theme.Metrics.sidebarWidth
            height: parent.height
            currentIndex: window.currentScreen
            onNavigate: function(idx) { window.currentScreen = idx }
        }

        Column {
            width: parent.width - sidebar.width
            height: parent.height
            spacing: 0

            Comp.TopBar {
                id: topBar
                width: parent.width
                currentIndex: window.currentScreen
                showTabs: window.currentScreen !== 0
                activeTab: window.currentScreen === 1 ? 0 : 1 // timeline shows Dashboard tab, others Metrics
                onTabSelected: function(idx) { /* tabs not switching screens in mock */ }
            }

            Item {
                width: parent.width
                height: parent.height - topBar.height
                clip: true

                Loader {
                    anchors.fill: parent
                    sourceComponent: {
                        if (window.currentScreen === 0) return dashboardComp
                        if (window.currentScreen === 1) return timelineComp
                        if (window.currentScreen === 2) return rootCauseComp
                        if (window.currentScreen === 3) return experimentsComp
                        if (window.currentScreen === 4) return jitterComp
                        return dashboardComp
                    }
                }
            }
        }
    }

    Component { id: dashboardComp; Screens.Dashboard {} }
    Component { id: timelineComp; Screens.TimelineScreen {} }
    Component { id: rootCauseComp; Screens.RootCauseScreen {} }
    Component { id: experimentsComp; Screens.ExperimentsScreen {} }
    Component { id: jitterComp; Screens.JitterScreen {} }

    // Keyboard navigation for validation
    Shortcut { sequence: "Ctrl+1"; onActivated: window.currentScreen = 0 }
    Shortcut { sequence: "Ctrl+2"; onActivated: window.currentScreen = 1 }
    Shortcut { sequence: "Ctrl+3"; onActivated: window.currentScreen = 2 }
    Shortcut { sequence: "Ctrl+4"; onActivated: window.currentScreen = 3 }
    Shortcut { sequence: "Ctrl+5"; onActivated: window.currentScreen = 4 }
}
