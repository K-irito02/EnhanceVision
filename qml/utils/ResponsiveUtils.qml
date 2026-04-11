pragma Singleton
import QtQuick

QtObject {
    readonly property int breakpointFull: 700
    readonly property int breakpointStandard: 500

    function getMode(width) {
        if (width >= breakpointFull) return "full"
        if (width >= breakpointStandard) return "standard"
        return "compact"
    }

    function shouldCollapseSpeedButtons(width) {
        return width < breakpointFull
    }

    function shouldCollapseSettings(width) {
        return width < breakpointFull
    }

    function shouldCollapseVolumeSlider(width) {
        return width < breakpointStandard
    }
}
