pragma Singleton
import QtQuick

QtObject {
    readonly property color window: "#eef2f7"
    readonly property color rail: "#e9edf3"
    readonly property color surface: "#f6f7fa"
    readonly property color raised: "#e7ebf1"
    readonly property color raisedHover: "#eae7df"
    readonly property color ink: "#1b2028"
    readonly property color secondary: "#303744"
    readonly property color faint: "#343c48"
    readonly property color line: "#deddd7"
    readonly property color accent: "#ad4739"
    readonly property color accentHover: "#93392f"
    readonly property color accentWash: "#f8e9e3"
    readonly property color accentInk: "#ffffff"
    readonly property color warning: "#866023"
    readonly property color danger: "#b43f3b"
    readonly property color dangerWash: "#fbeceb"
    readonly property color success: "#32755d"
    readonly property color sky: "#dcecf4"
    readonly property color mint: "#e0eee4"
    readonly property int radius: 12
    readonly property int radiusSmall: 8
    readonly property int radiusLarge: 18
    readonly property string fontFamily: "Segoe UI"
    readonly property string displayFamily: "Georgia"
    property bool reducedMotion: false
    readonly property int fastMotion: reducedMotion ? 0 : 140
    readonly property int coverMotion: reducedMotion ? 0 : 420
}
