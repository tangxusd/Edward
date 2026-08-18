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

        Text { anchors.horizontalCenter: parent.horizontalCenter; y: 6; text: "0"; color: DesignTokens.textSecondary; font.pixelSize: 10 }
        Text { x: 8; anchors.verticalCenter: parent.verticalCenter; text: "0"; color: DesignTokens.textSecondary; font.pixelSize: 10 }
        Rectangle { anchors.horizontalCenter: parent.horizontalCenter; anchors.top: parent.top; anchors.bottom: parent.bottom; width: 1; color: "#335e666b" }
        Rectangle { anchors.verticalCenter: parent.verticalCenter; anchors.left: parent.left; anchors.right: parent.right; height: 1; color: "#335e666b" }

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
