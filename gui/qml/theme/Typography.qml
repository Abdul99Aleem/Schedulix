pragma Singleton
import QtQuick

QtObject {
    property string fontFamily: "Segoe UI"

    // Sizes tuned to reference
    property int sizeXXS: 9
    property int sizeXS: 10
    property int sizeS: 11
    property int sizeSM: 12
    property int sizeM: 13
    property int sizeL: 14
    property int sizeXL: 15
    property int sizeXXL: 16
    property int sizeTitle: 18
    property int sizeHeader: 13
    property int sizeMetricBig: 20
    property int sizeMetricMedium: 18

    property int weightRegular: Font.Normal
    property int weightMedium: Font.Medium
    property int weightSemiBold: Font.DemiBold
    property int weightBold: Font.Bold

    // Helpers to create font objects is done in components via Text.font
}
