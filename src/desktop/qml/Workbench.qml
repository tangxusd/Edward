import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "."

ApplicationWindow {
    id: window
    visible: true
    title: "Edward"
    color: DesignTokens.background
    minimumWidth: 1100
    minimumHeight: 680

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 1

            Rectangle {
                Layout.preferredWidth: 240
                Layout.fillHeight: true
                color: DesignTokens.panel
                Column {
                    anchors.centerIn: parent
                    spacing: 12
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: "素材库"; color: DesignTokens.textPrimary; font.pixelSize: 14 }
                    Button { anchors.horizontalCenter: parent.horizontalCenter; text: "+ 导入素材"; onClicked: mediaDialog.open() }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 1
                EdwardPreview { Layout.fillWidth: true; Layout.fillHeight: true; playheadFrame: workbenchRuntime.playheadFrame }
                EdwardTimeline {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 220
                    playheadFrame: workbenchRuntime.playheadFrame
                    clips: workbenchRuntime.clips
                    onPlayheadChangedByUser: workbenchRuntime.setPlayhead(frame)
                    onSplitRequested: workbenchRuntime.splitSelected()
                    onDeleteRequested: workbenchRuntime.deleteSelected()
                    onRippleDeleteRequested: workbenchRuntime.rippleDeleteSelected()
                }
            }

            Rectangle {
                Layout.preferredWidth: 280
                Layout.fillHeight: true
                color: DesignTokens.panel
                Column {
                    anchors.centerIn: parent
                    spacing: 12
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: "AI 创作"; color: DesignTokens.accent; font.pixelSize: 13 }
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: workbenchRuntime.demoOverlayEnabled ? "移除测试组件" : "添加测试组件"
                        onClicked: workbenchRuntime.toggleDemoOverlay()
                    }
                }
            }
        }
    }

    FileDialog {
        id: mediaDialog
        title: "导入视频素材"
        fileMode: FileDialog.OpenFile
        nameFilters: ["视频文件 (*.mp4 *.mov *.mkv *.webm)", "所有文件 (*)"]
        onAccepted: workbenchRuntime.importMedia(selectedFile.toLocalFile())
    }

    Connections {
        target: workbenchRuntime
        function onOperationFailed(message) { failureToast.text = message; failureToast.open() }
    }

    Dialog {
        id: failureToast
        modal: false
        closePolicy: Popup.NoAutoClose
        anchors.centerIn: Overlay.overlay
        standardButtons: Dialog.Ok
        property alias text: failureLabel.text
        contentItem: Label { id: failureLabel; color: DesignTokens.textPrimary; padding: 16 }
    }
}
