import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

Item {
    id: root
    focus: true
    Keys.onSpacePressed: {
        workbenchRuntime.togglePlayback();
        event.accepted = true;
    }
    Keys.onPressed: {
        if (event.key === Qt.Key_S && !event.modifiers) {
            root.splitRequested();
            event.accepted = true;
        } else if (event.key === Qt.Key_Delete) {
            if (event.modifiers & Qt.ShiftModifier) root.rippleDeleteRequested();
            else root.deleteRequested();
            event.accepted = true;
        }
    }
    property int durationFrames: 300
    property int playheadFrame: 0
    property int videoTrackCount: 1
    property int selectedVideoTrackIndex: 0
    property var clips: []
    signal playheadChangedByUser(int frame)
    signal splitRequested
    signal deleteRequested
    signal rippleDeleteRequested
    signal videoTrackSelected(int trackIndex)

    function frameToX(frame) {
        return rulerWidth + frame * pixelsPerFrame;
    }
    function formatTimecode(frame) {
        var totalSeconds = Math.floor(frame / 25);
        var frames = frame % 25;
        var seconds = totalSeconds % 60;
        var minutes = Math.floor(totalSeconds / 60) % 60;
        var hours = Math.floor(totalSeconds / 3600);
        function pad(value) { return value < 10 ? "0" + value : value; }
        return pad(hours) + ":" + pad(minutes) + ":" + pad(seconds) + ":" + pad(frames);
    }
    function snapFrame(frame, clipId) {
        var candidates = [0, durationFrames, playheadFrame];
        for (var i = 0; i < clips.length; ++i) {
            if (clips[i].id === clipId) continue;
            candidates.push(clips[i].timelineStart);
            candidates.push(clips[i].timelineStart + clips[i].sourceOut - clips[i].sourceIn);
        }
        var threshold = Math.max(2, Math.round(8 / pixelsPerFrame));
        var best = frame;
        var distance = threshold + 1;
        for (var j = 0; j < candidates.length; ++j) {
            var candidateDistance = Math.abs(candidates[j] - frame);
            if (candidateDistance <= threshold && candidateDistance < distance) {
                best = candidates[j];
                distance = candidateDistance;
            }
        }
        return Math.max(0, Math.min(durationFrames, best));
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
            text: workbenchRuntime.playing ? "暂停" : "播放"
            onClicked: workbenchRuntime.togglePlayback()
        }
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
            text: "左裁切"
            onClicked: workbenchRuntime.trimSelectedLeft()
        }
        Button {
            text: "右裁切"
            onClicked: workbenchRuntime.trimSelectedRight()
        }
        Button {
            text: "撤销"
            onClicked: workbenchRuntime.undoTimeline()
        }
        Button {
            text: "重做"
            onClicked: workbenchRuntime.redoTimeline()
        }
        Button {
            text: "新增视频轨"
            onClicked: workbenchRuntime.addVideoTrack()
        }
        Button {
            text: "删除空视频轨"
            enabled: root.videoTrackCount > 1
            onClicked: workbenchRuntime.removeEmptyVideoTrack()
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
            onPressed: root.forceActiveFocus()
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
                    text: root.formatTimecode(Math.round(index * root.durationFrames / 10))
                    color: DesignTokens.textSecondary
                    font.pixelSize: 10
                }
                MouseArea {
                    anchors.left: parent.left
                    anchors.leftMargin: root.rulerWidth
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    onClicked: root.playheadChangedByUser(Math.max(0, Math.min(root.durationFrames,
                        Math.round((mouse.x - root.rulerWidth) / root.pixelsPerFrame))))
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
                    color: videoTrack && root.selectedVideoTrackIndex === root.videoTrackCount - 1 - index
                           ? DesignTokens.selection : DesignTokens.panelRaised
                }
                Rectangle {
                    x: root.rulerWidth - 1
                    width: 1
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    color: DesignTokens.divider
                }
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: videoTrack ? "V" + (root.videoTrackCount - index) : "A1"
                    color: DesignTokens.textPrimary
                    font.pixelSize: 12
                }
                MouseArea {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: root.rulerWidth
                    enabled: videoTrack
                    onClicked: root.videoTrackSelected(root.videoTrackCount - 1 - index)
                }
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 5
                    text: videoTrack ? "视频" : "音频"
                    color: DesignTokens.textSecondary
                    font.pixelSize: 10
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
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: 3
                color: modelData.kind === "component" ? DesignTokens.accent : "#4c9ac1"
            }
            Image {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.topMargin: 4
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 3
                visible: modelData.kind === "media" && modelData.thumbnail
                source: modelData.thumbnail ? modelData.thumbnail : ""
                fillMode: Image.PreserveAspectCrop
                clip: true
                opacity: 0.86
            }
            Canvas {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.topMargin: 5
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 4
                visible: modelData.kind === "media" && modelData.waveform && modelData.waveform.length > 0
                z: 1
                onPaint: {
                    var values = modelData.waveform;
                    if (!values || values.length === 0) return;
                    var context = getContext("2d");
                    context.reset();
                    context.strokeStyle = "#143641";
                    context.lineWidth = 1;
                    var center = height / 2;
                    for (var i = 0; i < values.length; ++i) {
                        var x = values.length === 1 ? width / 2 : i * width / (values.length - 1);
                        var amplitude = Math.max(1, values[i] * Math.max(1, center - 1));
                        context.beginPath();
                        context.moveTo(x, center - amplitude);
                        context.lineTo(x, center + amplitude);
                        context.stroke();
                    }
                }
            }
            Text {
                anchors.left: parent.left
                anchors.leftMargin: 5
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 3
                visible: modelData.selected && parent.width > 92
                text: root.formatTimecode(modelData.timelineStart) + " - " +
                      root.formatTimecode(modelData.timelineStart + modelData.sourceOut - modelData.sourceIn)
                color: "#d9f7fa"
                font.pixelSize: 9
                elide: Text.ElideRight
                width: parent.width - 10
            }
            MouseArea {
                anchors.fill: parent
                drag.target: parent
                drag.axis: Drag.XAxis
                drag.minimumX: root.rulerWidth
                drag.maximumX: root.frameToX(root.durationFrames) - parent.width
                onPressed: workbenchRuntime.selectClip(modelData.id)
                onClicked: workbenchRuntime.selectClip(modelData.id)
                onDoubleClicked: root.playheadChangedByUser(modelData.timelineStart)
                onReleased: workbenchRuntime.moveSelected(root.snapFrame(
                    Math.round((parent.x - root.rulerWidth) / root.pixelsPerFrame), modelData.id))
            }
            MouseArea {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: Math.min(9, parent.width / 3)
                cursorShape: Qt.SizeHorCursor
                onPressed: workbenchRuntime.selectClip(modelData.id)
                onPositionChanged: if (pressed) {
                    var frame = Math.max(0, Math.min(root.durationFrames,
                        Math.round((parent.x + mouse.x - root.rulerWidth) / root.pixelsPerFrame)))
                    root.playheadChangedByUser(frame)
                }
                onReleased: workbenchRuntime.trimSelectedLeft()
            }
            MouseArea {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: Math.min(9, parent.width / 3)
                cursorShape: Qt.SizeHorCursor
                onPressed: workbenchRuntime.selectClip(modelData.id)
                onPositionChanged: if (pressed) {
                    var frame = Math.max(0, Math.min(root.durationFrames,
                        Math.round((parent.x + mouse.x - root.rulerWidth) / root.pixelsPerFrame)))
                    root.playheadChangedByUser(frame)
                }
                onReleased: workbenchRuntime.trimSelectedRight()
            }
            Text {
                anchors.centerIn: parent
                z: 2
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
        y: ruler.y + 7
        width: 1
        height: root.height - ruler.y - 7
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

    Canvas {
        id: playheadMarker
        x: root.frameToX(root.playheadFrame) - 6
        y: ruler.y
        width: 12
        height: 9
        z: 11
        onPaint: {
            var context = getContext("2d");
            context.reset();
            context.fillStyle = DesignTokens.playhead;
            context.beginPath();
            context.moveTo(0, 0);
            context.lineTo(width, 0);
            context.lineTo(width / 2, height);
            context.closePath();
            context.fill();
        }
    }
}
