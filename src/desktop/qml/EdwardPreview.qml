import QtQuick
import "."

Item {
    id: root
    property int playheadFrame: 0
    property int durationFrames: 1
    property url source

    Rectangle {
        anchors.fill: parent
        color: DesignTokens.background
    }

    Rectangle {
        id: canvas
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, parent.height * 16 / 9)
        height: width * 9 / 16
        color: "#050505"
        border.color: DesignTokens.divider
        border.width: 1

        Image {
            anchors.fill: parent
            source: root.source.toString() !== ""
                ? root.source
                : ("image://edward/frame?frame=" + root.playheadFrame)
            cache: false
            fillMode: Image.PreserveAspectFit
            visible: status === Image.Ready || source !== ""
        }

        Text {
            anchors.centerIn: parent
            text: "预览窗"
            color: DesignTokens.textSecondary
            visible: !root.source || root.source.toString() === ""
            font.pixelSize: 13
        }
    }
}
