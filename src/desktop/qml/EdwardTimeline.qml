import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

Item {
    id: root
    property int durationFrames: 300
    property int playheadFrame: 0
    property int videoTrackCount: 1
    property var clips: []
    signal playheadChangedByUser(int frame)
    signal splitRequested
    signal deleteRequested
    signal rippleDeleteRequested

    function frameToX(frame) {
        return rulerWidth + frame * pixelsPerFrame;
    }
    readonly property real rulerWidth: 74
    readonly property real pixelsPerFrame: Math.max(0.25, (width - rulerWidth - 24) / durationFrames)

    Rectangle {
        anchors.fill: parent
        color: DesignTokens.panel
    }

    Row {
        id: toolbar
        height: 36
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 8
        padding: 8
        Button {
            text: "分割"
            onClicked: root.splitRequested()
        }
        Button {
            text: "删除"
            onClicked: root.deleteRequested()
        }
        Button {
            text: "波纹删除"
            onClicked: root.rippleDeleteRequested()
        }
        Button {
            text: "撤销"
            onClicked: workbenchRuntime.undoTimeline()
        }
        Button {
            text: "重做"
            onClicked: workbenchRuntime.redoTimeline()
        }
        Item {
            Layout.fillWidth: true
        }
    }

    Rectangle {
        id: ruler
        anchors.top: toolbar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 28
        color: DesignTokens.panelRaised
        clip: true
        MouseArea {
            anchors.fill: parent
            onClicked: root.playheadChangedByUser(Math.max(0, Math.min(root.durationFrames, Math.round((mouse.x - root.rulerWidth) / root.pixelsPerFrame))))
        }
        Repeater {
            model: 11
            delegate: Item {
                x: root.frameToX(index * root.durationFrames / 10)
                width: 1
                height: ruler.height
                Rectangle {
                    width: 1
                    height: 10
                    color: DesignTokens.textSecondary
                }
                Text {
                    x: 5
                    y: 8
                    text: Math.round(index * root.durationFrames / 10)
                    color: DesignTokens.textSecondary
                    font.pixelSize: 10
                }
            }
        }
    }

    Column {
        id: tracks
        anchors.top: ruler.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        spacing: 1

        Repeater {
            model: root.videoTrackCount + 1
            delegate: Item {
                readonly property bool videoTrack: index < root.videoTrackCount
                height: (tracks.height - root.videoTrackCount) / (root.videoTrackCount + 1)
                Rectangle {
                    anchors.fill: parent
                    color: videoTrack ? "#151515" : "#191919"
                }
                Rectangle {
                    width: root.rulerWidth
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    color: DesignTokens.panelRaised
                }
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: videoTrack ? "V" + (root.videoTrackCount - index) : "A1"
                    color: DesignTokens.textPrimary
                    font.pixelSize: 12
                }
            }
        }
    }

    Repeater {
        model: root.clips
        delegate: Rectangle {
            x: root.frameToX(modelData.timelineStart)
            y: ruler.height + toolbar.height +
               (root.videoTrackCount - 1 - modelData.trackIndex) * ((tracks.height - root.videoTrackCount) / (root.videoTrackCount + 1)) + 3
            width: Math.max(3, (modelData.sourceOut - modelData.sourceIn) * root.pixelsPerFrame)
            height: Math.max(18, (tracks.height - root.videoTrackCount) / (root.videoTrackCount + 1) - 6)
            color: DesignTokens.videoClip
            border.color: modelData.selected ? DesignTokens.accent : "#0a0a0a"
            border.width: modelData.selected ? 2 : 1
            MouseArea {
                anchors.fill: parent
                drag.target: parent
                drag.axis: Drag.XAxis
                drag.minimumX: root.rulerWidth
                drag.maximumX: root.frameToX(root.durationFrames) - parent.width
                onPressed: workbenchRuntime.selectClip(modelData.id)
                onClicked: workbenchRuntime.selectClip(modelData.id)
                onReleased: workbenchRuntime.moveSelected(Math.max(0, Math.min(root.durationFrames, Math.round((parent.x - root.rulerWidth) / root.pixelsPerFrame))))
            }
            Text {
                anchors.centerIn: parent
                text: modelData.name || "素材"
                color: "#ffffff"
                font.pixelSize: 11
                elide: Text.ElideRight
                width: parent.width - 8
            }
        }
    }

    Rectangle {
        id: playhead
        x: root.frameToX(root.playheadFrame)
        y: ruler.y
        width: 1
        height: root.height - ruler.y
        color: DesignTokens.playhead
        z: 10
        MouseArea {
            anchors.fill: parent
            anchors.margins: -8
            drag.target: parent
            drag.axis: Drag.XAxis
            onPositionChanged: if (drag.active)
                root.playheadChangedByUser(Math.max(0, Math.min(root.durationFrames, Math.round((playhead.x - root.rulerWidth) / root.pixelsPerFrame))))
        }
    }
}
