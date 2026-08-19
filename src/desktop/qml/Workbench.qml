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
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "素材库"
                        color: DesignTokens.textPrimary
                        font.pixelSize: 14
                    }
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "+ 导入素材"
                        onClicked: mediaDialog.open()
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 1
                EdwardPreview {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    playheadFrame: workbenchRuntime.playheadFrame
                    componentOverlayEnabled: workbenchRuntime.demoOverlayEnabled
                    componentX: workbenchRuntime.demoOverlayX
                    componentY: workbenchRuntime.demoOverlayY
                    componentWidth: workbenchRuntime.demoOverlayWidth
                    componentHeight: workbenchRuntime.demoOverlayHeight
                    onComponentDragged: {
                        workbenchRuntime.demoOverlayX = x;
                        workbenchRuntime.demoOverlayY = y;
                    }
                }
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
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "AI 创作"
                        color: DesignTokens.accent
                        font.pixelSize: 13
                    }
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: workbenchRuntime.demoOverlayEnabled ? "重新生成组件草稿" : "生成组件草稿"
                        onClicked: workbenchRuntime.generateComponentDraft()
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "组件 X: " + workbenchRuntime.demoOverlayX
                        color: DesignTokens.textSecondary
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "组件 Y: " + workbenchRuntime.demoOverlayY
                        color: DesignTokens.textSecondary
                    }
                    Slider {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 220
                        from: 0
                        to: 640
                        value: workbenchRuntime.demoOverlayX
                        onMoved: workbenchRuntime.demoOverlayX = value
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "宽度: " + workbenchRuntime.demoOverlayWidth
                        color: DesignTokens.textSecondary
                    }
                    Slider {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 220
                        from: 40
                        to: 640
                        value: workbenchRuntime.demoOverlayWidth
                        onMoved: workbenchRuntime.demoOverlayWidth = value
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "高度: " + workbenchRuntime.demoOverlayHeight
                        color: DesignTokens.textSecondary
                    }
                    Slider {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 220
                        from: 24
                        to: 360
                        value: workbenchRuntime.demoOverlayHeight
                        onMoved: workbenchRuntime.demoOverlayHeight = value
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "透明度: " + workbenchRuntime.demoOverlayOpacity.toFixed(2)
                        color: DesignTokens.textSecondary
                    }
                    Slider {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 220
                        from: 0
                        to: 1
                        value: workbenchRuntime.demoOverlayOpacity
                        onMoved: workbenchRuntime.demoOverlayOpacity = value
                    }
                    TextField {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 220
                        text: workbenchRuntime.demoOverlayText
                        placeholderText: "组件文字"
                        onEditingFinished: workbenchRuntime.demoOverlayText = text
                    }
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "导入组件草稿"
                        onClicked: componentDialog.open()
                    }
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "清除组件叠加"
                        enabled: workbenchRuntime.demoOverlayEnabled
                        onClicked: workbenchRuntime.clearComponentOverlay()
                    }
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "选择动画插件目录"
                        onClicked: pluginDirectoryDialog.open()
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: workbenchRuntime.installedPluginAvailable
                              ? "插件: " + workbenchRuntime.installedPluginId
                              : "未选择动画插件"
                        color: workbenchRuntime.installedPluginAvailable
                               ? DesignTokens.textSecondary : DesignTokens.textSecondary
                    }
                    TextField {
                        id: compositionIdField
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 220
                        placeholderText: "组件 ID"
                        text: "main"
                    }
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: workbenchRuntime.pluginRenderBusy ? "插件渲染中" : "插件预览"
                        enabled: workbenchRuntime.installedPluginAvailable && !workbenchRuntime.pluginRenderBusy
                        onClicked: workbenchRuntime.renderInstalledPluginFrame(
                                        "frame-" + workbenchRuntime.playheadFrame,
                                        compositionIdField.text)
                    }
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: workbenchRuntime.pluginExportBusy ? "插件导出中" : "插件导出"
                        enabled: workbenchRuntime.installedPluginAvailable && !workbenchRuntime.pluginExportBusy
                        onClicked: pluginExportDialog.open()
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

    FolderDialog {
        id: pluginDirectoryDialog
        title: "选择动画插件目录"
        onAccepted: workbenchRuntime.selectInstalledPlugin(selectedFolder.toLocalFile())
    }

    FileDialog {
        id: pluginExportDialog
        title: "保存插件导出"
        fileMode: FileDialog.SaveFile
        nameFilters: ["视频文件 (*.mov *.mp4)", "所有文件 (*)"]
        onAccepted: workbenchRuntime.exportInstalledPlugin(
                         "export-" + workbenchRuntime.playheadFrame,
                         compositionIdField.text,
                         selectedFile.toLocalFile())
    }

    Dialog {
        id: componentDialog
        anchors.centerIn: Overlay.overlay
        width: 520
        height: 420
        modal: true
        title: "导入组件草稿"
        standardButtons: Dialog.Cancel | Dialog.Ok
        contentItem: Column {
            spacing: 10
            Label {
                text: "粘贴版本化 Component IR JSON"
                color: DesignTokens.textSecondary
            }
            TextArea {
                id: componentJson
                Layout.fillWidth: true
                Layout.fillHeight: true
                textFormat: TextEdit.PlainText
                placeholderText: '{"version":"1","root":{...}}'
                wrapMode: TextEdit.Wrap
            }
        }
        onAccepted: {
            if (workbenchRuntime.loadComponentJson(componentJson.text))
                componentJson.clear();
        }
    }

    Connections {
        target: workbenchRuntime
        function onOperationFailed(message) {
            failureToast.text = message;
            failureToast.open();
        }
    }

    Dialog {
        id: failureToast
        modal: false
        closePolicy: Popup.NoAutoClose
        anchors.centerIn: Overlay.overlay
        standardButtons: Dialog.Ok
        property alias text: failureLabel.text
        contentItem: Label {
            id: failureLabel
            color: DesignTokens.textPrimary
            padding: 16
        }
    }
}
