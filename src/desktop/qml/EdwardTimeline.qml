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
    property var transitions: []
    property real zoomFactor: 1.0
    property int viewStartFrame: 0
    signal playheadChangedByUser(int frame)
    signal splitRequested
    signal deleteRequested
    signal rippleDeleteRequested
    signal videoTrackSelected(int trackIndex)

    function frameToX(frame) {
        return rulerWidth + (frame - viewStartFrame) * pixelsPerFrame;
    }
    function frameAtX(x) {
        return Math.max(0, Math.min(durationFrames,
            Math.round((x - rulerWidth) / pixelsPerFrame) + viewStartFrame));
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
    readonly property real pixelsPerFrame: Math.max(0.25, (width - rulerWidth - 24) / durationFrames) * zoomFactor

    WheelHandler {
        onWheel: function(event) {
            if (event.modifiers & Qt.ControlModifier) {
                root.zoomFactor = Math.max(0.5, Math.min(8, root.zoomFactor * (event.angleDelta.y > 0 ? 1.25 : 0.8)));
            } else {
                root.viewStartFrame = Math.max(0, Math.min(root.durationFrames,
                    root.viewStartFrame - Math.round(event.angleDelta.y / root.pixelsPerFrame)));
            }
            event.accepted = true;
        }
    }

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
            text: "添加叠化"
            onClicked: workbenchRuntime.addDissolveToSelected()
        }
        Button {
            text: "闪黑"
            onClicked: workbenchRuntime.addTransitionToSelected("flash_black")
        }
        Button {
            text: "闪白"
            onClicked: workbenchRuntime.addTransitionToSelected("flash_white")
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
            onClicked: root.playheadChangedByUser(root.frameAtX(mouse.x))
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
                    onClicked: root.playheadChangedByUser(root.frameAtX(mouse.x))
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
                visible: false
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
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                drag.target: parent
                drag.axis: Drag.XAxis
                drag.minimumX: root.rulerWidth
                drag.maximumX: root.frameToX(root.durationFrames) - parent.width
                onPressed: workbenchRuntime.selectClip(modelData.id)
                onClicked: function(mouse) {
                    workbenchRuntime.selectClip(modelData.id)
                    if (mouse.button === Qt.RightButton) transitionMenu.popup()
                }
                onDoubleClicked: root.playheadChangedByUser(modelData.timelineStart)
                onReleased: function(mouse) {
                    if (mouse.button === Qt.LeftButton)
                        workbenchRuntime.moveSelected(root.snapFrame(root.frameAtX(parent.x), modelData.id))
                }
            }
            Menu {
                id: transitionMenu
                MenuItem {
                    text: "添加叠化"
                    onTriggered: workbenchRuntime.addDissolveToSelected()
                }
                MenuItem {
                    text: "添加闪黑"
                    onTriggered: workbenchRuntime.addTransitionToSelected("flash_black")
                }
                MenuItem {
                    text: "添加闪白"
                    onTriggered: workbenchRuntime.addTransitionToSelected("flash_white")
                }
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
                        root.frameAtX(parent.x + mouse.x)))
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
                        root.frameAtX(parent.x + mouse.x)))
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

    Item {
        id: transitionOverlay
        anchors.fill: parent
        z: 3
        Repeater {
            model: root.transitions
            delegate: Rectangle {
                property int displayDuration: resizeHandle.pressed ? resizeHandle.pendingDuration
                                                                    : modelData.durationFrames
                x: root.frameToX(modelData.startFrame + modelData.durationFrames - displayDuration)
                y: ruler.height + toolbar.height +
                   (root.videoTrackCount - 1 - modelData.trackIndex) *
                   ((tracks.height - root.videoTrackCount) / (root.videoTrackCount + 1)) + 4
                width: Math.max(12, displayDuration * root.pixelsPerFrame)
                height: 16
                radius: 2
                color: modelData.type === "dissolve" ? "#6d5ca8"
                                                   : modelData.type === "flash_white" ? "#e1e5ea" : "#2f3740"
                border.color: DesignTokens.accent
                border.width: 1
                Text {
                    anchors.centerIn: parent
                    text: modelData.type === "dissolve" ? "叠化"
                         : modelData.type === "flash_white" ? "闪白" : "闪黑"
                    color: modelData.type === "flash_white" ? "#172027" : "#ffffff"
                    font.pixelSize: 9
                    visible: parent.width >= 28
                }
                MouseArea {
                    id: resizeHandle
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: Math.min(8, parent.width / 2)
                    cursorShape: Qt.SizeHorCursor
                    property int pendingDuration: modelData.durationFrames
                    onPressed: pendingDuration = modelData.durationFrames
                    onPositionChanged: if (pressed) {
                        var boundary = modelData.startFrame + modelData.durationFrames;
                        pendingDuration = Math.max(1, boundary - root.frameAtX(parent.x + mouse.x));
                    }
                    onReleased: workbenchRuntime.setTransitionDuration(modelData.leftClipId,
                                                                         modelData.rightClipId,
                                                                         pendingDuration)
                }
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.RightButton
                    onClicked: transitionRemoveMenu.popup()
                }
                Menu {
                    id: transitionRemoveMenu
                    MenuItem {
                        text: "删除转场"
                        onTriggered: workbenchRuntime.removeTransition(modelData.leftClipId,
                                                                        modelData.rightClipId)
                    }
                }
            }
        }
    }

    Repeater {
        model: root.clips
        delegate: Rectangle {
            visible: modelData.hasAudio
            x: root.frameToX(modelData.timelineStart)
            y: ruler.height + toolbar.height + root.videoTrackCount * ((tracks.height - root.videoTrackCount) / (root.videoTrackCount + 1)) + 3
            width: Math.max(3, (modelData.sourceOut - modelData.sourceIn) * root.pixelsPerFrame)
            height: Math.max(18, (tracks.height - root.videoTrackCount) / (root.videoTrackCount + 1) - 6)
            color: DesignTokens.audioClip
            border.color: modelData.selected ? DesignTokens.accent : "#0a0a0a"
            border.width: modelData.selected ? 2 : 1
            Canvas {
                anchors.fill: parent
                anchors.margins: 3
                onPaint: {
                    var values = modelData.waveform;
                    if (!values || values.length === 0) return;
                    var context = getContext("2d"); context.reset();
                    context.strokeStyle = "#28583d"; context.lineWidth = 1;
                    var center = height / 2;
                    for (var i = 0; i < values.length; ++i) {
                        var x = values.length === 1 ? width / 2 : i * width / (values.length - 1);
                        var amplitude = Math.max(1, values[i] * Math.max(1, center - 1));
                        context.beginPath(); context.moveTo(x, center - amplitude); context.lineTo(x, center + amplitude); context.stroke();
                    }
                }
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
                root.playheadChangedByUser(root.frameAtX(playhead.x))
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
