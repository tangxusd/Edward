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
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 12
                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "素材库"
                        color: DesignTokens.textPrimary
                        font.pixelSize: 14
                    }
                    Button {
                        Layout.alignment: Qt.AlignHCenter
                        text: workbenchRuntime.authenticated
                              ? "已登录: " + workbenchRuntime.authenticatedUsername
                              : "登录"
                        onClicked: workbenchRuntime.authenticated ? workbenchRuntime.signOut() : signInDialog.open()
                    }
                    Button {
                        Layout.alignment: Qt.AlignHCenter
                        text: "+ 导入素材"
                        onClicked: mediaDialog.open()
                    }
                    Button {
                        Layout.alignment: Qt.AlignHCenter
                        text: "保存工程"
                        onClicked: projectSaveDialog.open()
                    }
                    Button {
                        Layout.alignment: Qt.AlignHCenter
                        text: "打开工程"
                        onClicked: projectOpenDialog.open()
                    }
                    Button {
                        Layout.alignment: Qt.AlignHCenter
                        text: workbenchRuntime.timelineExportBusy ? "视频导出中" : "导出视频"
                        enabled: workbenchRuntime.clips.length > 0 && !workbenchRuntime.timelineExportBusy
                        onClicked: timelineExportDialog.open()
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.topMargin: 8
                        color: DesignTokens.background
                        radius: 4
                        border.color: DesignTokens.divider
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 6
                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: "我的组件"; color: DesignTokens.textPrimary; font.pixelSize: 13 }
                                Item { Layout.fillWidth: true }
                                Button { text: "选择目录"; onClicked: componentLibraryDirectoryDialog.open() }
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: workbenchRuntime.localComponents.length === 0
                                text: "尚未保存组件"
                                color: DesignTokens.textSecondary
                                font.pixelSize: 12
                            }
                            ListView {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                model: workbenchRuntime.localComponents
                                delegate: Button {
                                    required property var modelData
                                    width: ListView.view.width
                                    text: modelData.displayName
                                    onClicked: workbenchRuntime.loadLibraryComponent(modelData.resourceId)
                                }
                            }
                        }
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
            videoTrackCount: workbenchRuntime.videoTrackCount
                    hasFrame: workbenchRuntime.clips.length > 0
                    componentOverlayEnabled: workbenchRuntime.demoOverlayEnabled
                    componentX: workbenchRuntime.demoOverlayX
                    componentY: workbenchRuntime.demoOverlayY
                    componentWidth: workbenchRuntime.demoOverlayWidth
                    componentHeight: workbenchRuntime.demoOverlayHeight
                    componentNodes: workbenchRuntime.componentNodes
                    selectedComponentNodeId: workbenchRuntime.selectedComponentNodeId
                    onComponentNodeSelected: workbenchRuntime.selectComponentNode(nodeId)
                    onComponentDragged: {
                        workbenchRuntime.demoOverlayX = x;
                        workbenchRuntime.demoOverlayY = y;
                    }
                }
                EdwardTimeline {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 220
                    playheadFrame: workbenchRuntime.playheadFrame
                    videoTrackCount: workbenchRuntime.videoTrackCount
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
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: workbenchRuntime.componentBoundToClip ? "已绑定到片段" : "绑定到选中片段"
                        enabled: workbenchRuntime.demoOverlayEnabled && !workbenchRuntime.componentBoundToClip
                        onClicked: workbenchRuntime.bindComponentToSelectedClip()
                    }
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "添加组件到时间线"
                        enabled: workbenchRuntime.demoOverlayEnabled
                        onClicked: workbenchRuntime.addCurrentComponentToTimeline()
                    }
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "保存到我的组件库"
                        enabled: workbenchRuntime.demoOverlayEnabled
                        onClicked: componentLibrarySaveDialog.open()
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "AI 编辑命令（JSON）"
                        color: DesignTokens.textSecondary
                    }
                    TextArea {
                        id: aiCommandInput
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 220
                        height: 68
                        textFormat: TextEdit.PlainText
                        wrapMode: TextEdit.Wrap
                        placeholderText: '{"operation":"setProperty",...}'
                    }
                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 6
                        Button {
                            text: "生成草案"
                            onClicked: workbenchRuntime.proposeAiComponentCommand(aiCommandInput.text)
                        }
                        Button {
                            text: "应用草案"
                            enabled: workbenchRuntime.aiComponentDraftAvailable
                            onClicked: workbenchRuntime.applyPendingAiComponentCommand()
                        }
                        Button {
                            text: "放弃"
                            enabled: workbenchRuntime.aiComponentDraftAvailable
                            onClicked: workbenchRuntime.discardPendingAiComponentCommand()
                        }
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: workbenchRuntime.aiComponentDraftAvailable ? "有待确认草案" : "无待确认草案"
                        color: workbenchRuntime.aiComponentDraftAvailable ? DesignTokens.accent : DesignTokens.textSecondary
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "组件子节点"
                        color: DesignTokens.textSecondary
                    }
                    ComboBox {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 220
                        model: workbenchRuntime.componentNodes
                        textRole: "displayName"
                        valueRole: "id"
                        enabled: workbenchRuntime.componentNodes.length > 0
                        currentIndex: {
                            for (var i = 0; i < model.length; ++i)
                                if (model[i].id === workbenchRuntime.selectedComponentNodeId) return i
                            return -1
                        }
                        onActivated: workbenchRuntime.selectComponentNode(currentValue)
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "关键帧: " + workbenchRuntime.selectedComponentNodeKeyframes.length
                        color: DesignTokens.textSecondary
                    }
                    ListView {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 220
                        height: 64
                        clip: true
                        model: workbenchRuntime.selectedComponentNodeKeyframes
                        delegate: Label {
                            required property var modelData
                            text: modelData.field + " @ " + modelData.frame
                            color: DesignTokens.textSecondary
                            font.pixelSize: 11
                            MouseArea {
                                anchors.fill: parent
                                onClicked: workbenchRuntime.removeSelectedComponentNodeKeyframe(modelData.field, modelData.frame)
                            }
                        }
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "组件 AI 会话"
                        color: DesignTokens.textSecondary
                    }
                    TextArea {
                        id: aiConversationView
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 220
                        height: 82
                        readOnly: true
                        textFormat: TextEdit.PlainText
                        wrapMode: TextEdit.Wrap
                        text: workbenchRuntime.aiConversation.length > 0
                              ? workbenchRuntime.aiConversation
                              : "暂无会话"
                        color: DesignTokens.textPrimary
                        placeholderText: "暂无会话"
                    }
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: workbenchRuntime.aiRequestBusy ? "AI 请求中" : "使用模型生成草案"
                        enabled: !workbenchRuntime.aiRequestBusy
                        onClicked: aiModelDialog.open()
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "组件 X: " + workbenchRuntime.demoOverlayX
                        color: DesignTokens.textSecondary
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "选中节点 X: " + workbenchRuntime.selectedComponentNodeX
                        color: DesignTokens.textSecondary
                    }
                    Slider {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 220
                        from: -640
                        to: 640
                        value: workbenchRuntime.selectedComponentNodeX
                        enabled: workbenchRuntime.selectedComponentNodeId !== ""
                        onMoved: workbenchRuntime.selectedComponentNodeX = value
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "组件 Y: " + workbenchRuntime.demoOverlayY
                        color: DesignTokens.textSecondary
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "选中节点 Y: " + workbenchRuntime.selectedComponentNodeY
                        color: DesignTokens.textSecondary
                    }
                    Slider {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 220
                        from: -360
                        to: 360
                        value: workbenchRuntime.selectedComponentNodeY
                        enabled: workbenchRuntime.selectedComponentNodeId !== ""
                        onMoved: workbenchRuntime.selectedComponentNodeY = value
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "选中节点宽度: " + workbenchRuntime.selectedComponentNodeWidth
                        color: DesignTokens.textSecondary
                    }
                    Slider {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 220
                        from: 1
                        to: 640
                        value: workbenchRuntime.selectedComponentNodeWidth
                        enabled: workbenchRuntime.selectedComponentNodeId !== ""
                        onMoved: workbenchRuntime.selectedComponentNodeWidth = value
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "选中节点高度: " + workbenchRuntime.selectedComponentNodeHeight
                        color: DesignTokens.textSecondary
                    }
                    Slider {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 220
                        from: 1
                        to: 360
                        value: workbenchRuntime.selectedComponentNodeHeight
                        enabled: workbenchRuntime.selectedComponentNodeId !== ""
                        onMoved: workbenchRuntime.selectedComponentNodeHeight = value
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "选中节点旋转: " + workbenchRuntime.selectedComponentNodeRotation.toFixed(0) + "°"
                        color: DesignTokens.textSecondary
                    }
                    Slider {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 220
                        from: -180
                        to: 180
                        value: workbenchRuntime.selectedComponentNodeRotation
                        enabled: workbenchRuntime.selectedComponentNodeId !== ""
                        onMoved: workbenchRuntime.selectedComponentNodeRotation = value
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "选中节点透明度: " + workbenchRuntime.selectedComponentNodeOpacity.toFixed(2)
                        color: DesignTokens.textSecondary
                    }
                    Slider {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 220
                        from: 0
                        to: 1
                        value: workbenchRuntime.selectedComponentNodeOpacity
                        enabled: workbenchRuntime.selectedComponentNodeId !== ""
                        onMoved: workbenchRuntime.selectedComponentNodeOpacity = value
                    }
                    TextField {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 220
                        text: workbenchRuntime.selectedComponentNodeColor
                        placeholderText: "节点颜色，例如 #00b8c8"
                        enabled: workbenchRuntime.selectedComponentNodeType === "text" ||
                                 workbenchRuntime.selectedComponentNodeType === "shape"
                        onEditingFinished: workbenchRuntime.selectedComponentNodeColor = text
                    }
                    TextField {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 220
                        text: workbenchRuntime.selectedComponentNodeBorderColor
                        placeholderText: "边框颜色，例如 #ffffff"
                        enabled: workbenchRuntime.selectedComponentNodeType === "shape"
                        onEditingFinished: workbenchRuntime.selectedComponentNodeBorderColor = text
                    }
                    TextField {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 220
                        text: workbenchRuntime.selectedComponentNodeFontFamily
                        placeholderText: "字体族，例如 PingFang SC"
                        enabled: workbenchRuntime.selectedComponentNodeFontFamily !== ""
                        onEditingFinished: workbenchRuntime.selectedComponentNodeFontFamily = text
                    }
                    Slider {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 220
                        from: -640
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
                        text: "缩放: " + workbenchRuntime.demoOverlayScale.toFixed(2)
                        color: DesignTokens.textSecondary
                    }
                    Slider {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 220
                        from: 0.1
                        to: 3
                        value: workbenchRuntime.demoOverlayScale
                        onMoved: workbenchRuntime.demoOverlayScale = value
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "旋转: " + workbenchRuntime.demoOverlayRotation.toFixed(0) + "°"
                        color: DesignTokens.textSecondary
                    }
                    Slider {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 220
                        from: -180
                        to: 180
                        value: workbenchRuntime.demoOverlayRotation
                        onMoved: workbenchRuntime.demoOverlayRotation = value
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
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "文字字号: " + workbenchRuntime.demoOverlayFontSize
                        color: DesignTokens.textSecondary
                    }
                    Slider {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 220
                        from: 8
                        to: 96
                        value: workbenchRuntime.demoOverlayFontSize
                        onMoved: workbenchRuntime.demoOverlayFontSize = value
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "边框宽度: " + workbenchRuntime.demoOverlayBorderWidth
                        color: DesignTokens.textSecondary
                    }
                    Slider {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 220
                        from: 0
                        to: 32
                        value: workbenchRuntime.demoOverlayBorderWidth
                        onMoved: workbenchRuntime.demoOverlayBorderWidth = value
                    }
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "导入组件草稿"
                        onClicked: componentDialog.open()
                    }
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "从文件导入组件"
                        onClicked: componentFileDialog.open()
                    }
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "保存组件 JSON"
                        enabled: workbenchRuntime.demoOverlayEnabled
                        onClicked: componentSaveDialog.open()
                    }
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "保存组件包"
                        enabled: workbenchRuntime.demoOverlayEnabled
                        onClicked: componentPackageDialog.open()
                    }
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: workbenchRuntime.componentUploadBusy ? "组件上传中" : "上传组件包"
                        enabled: workbenchRuntime.authenticated && workbenchRuntime.demoOverlayEnabled
                                 && !workbenchRuntime.componentUploadBusy
                        onClicked: componentUploadDialog.open()
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
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "组件插件: " + workbenchRuntime.componentPluginDependencyStatus
                        color: DesignTokens.textSecondary
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
                        text: "读取插件组件"
                        enabled: workbenchRuntime.installedPluginAvailable
                        onClicked: workbenchRuntime.describeInstalledPlugin(compositionIdField.text)
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
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: workbenchRuntime.pluginExportBusy ? "插件应用中" : "应用到时间线"
                        enabled: workbenchRuntime.installedPluginAvailable && !workbenchRuntime.pluginExportBusy
                        onClicked: pluginApplyDialog.open()
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

    FileDialog {
        id: projectSaveDialog
        title: "保存 Edward 工程"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Edward 工程 (*.edward.json)", "JSON 文件 (*.json)"]
        onAccepted: workbenchRuntime.saveProject(selectedFile.toLocalFile())
    }

    FileDialog {
        id: projectOpenDialog
        title: "打开 Edward 工程"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Edward 工程 (*.edward.json *.json)"]
        onAccepted: workbenchRuntime.loadProject(selectedFile.toLocalFile())
    }

    FileDialog {
        id: timelineExportDialog
        title: "导出视频"
        fileMode: FileDialog.SaveFile
        nameFilters: ["MP4 视频 (*.mp4)", "所有文件 (*)"]
        onAccepted: workbenchRuntime.exportTimeline(selectedFile.toLocalFile())
    }

    FolderDialog {
        id: pluginDirectoryDialog
        title: "选择动画插件目录"
        onAccepted: workbenchRuntime.selectInstalledPlugin(selectedFolder.toLocalFile())
    }

    Dialog {
        id: componentLibrarySaveDialog
        anchors.centerIn: Overlay.overlay
        width: 420
        modal: true
        title: "保存到我的组件库"
        standardButtons: Dialog.Cancel | Dialog.Ok
        contentItem: Column {
            spacing: 10
            TextField { id: libraryResourceId; placeholderText: "资源 ID，例如 my.card"; text: "my.component" }
            TextField { id: libraryDisplayName; placeholderText: "显示名称"; text: "我的组件" }
        }
        onAccepted: componentLibraryDirectoryDialog.open()
    }

    FolderDialog {
        id: componentLibraryDirectoryDialog
        title: "选择我的组件库目录"
        onAccepted: {
            if (!workbenchRuntime.configureComponentLibrary(selectedFolder.toLocalFile()))
                return
            if (componentLibrarySaveDialog.visible
                    && workbenchRuntime.saveCurrentComponentToLibrary(libraryResourceId.text,
                                                                       libraryDisplayName.text))
                componentLibrarySaveDialog.close()
        }
    }

    Dialog {
        id: componentUploadDialog
        anchors.centerIn: Overlay.overlay
        width: 420
        modal: true
        title: "上传组件包"
        standardButtons: Dialog.Cancel | Dialog.Ok
        contentItem: Column {
            spacing: 10
            TextField { id: componentUploadEndpoint; placeholderText: "Supabase Edge Function HTTPS 地址" }
            TextField { id: componentUploadResourceId; placeholderText: "资源 ID，例如 demo.card"; text: "demo.card" }
            TextField { id: componentUploadDisplayName; placeholderText: "显示名称"; text: "Edward 组件" }
        }
        onAccepted: {
            if (workbenchRuntime.uploadCurrentComponent(componentUploadEndpoint.text,
                                                        componentUploadResourceId.text,
                                                        componentUploadDisplayName.text))
                close()
        }
    }

    Dialog {
        id: aiModelDialog
        anchors.centerIn: Overlay.overlay
        width: 420
        modal: true
        title: "使用 AI 生成组件草案"
        standardButtons: Dialog.Cancel | Dialog.Ok
        contentItem: Column {
            spacing: 10
            TextField { id: aiEndpoint; placeholderText: "模型 HTTPS 端点" }
            TextField { id: aiApiKey; placeholderText: "API Key"; echoMode: TextInput.Password }
            TextField { id: aiModel; placeholderText: "模型 ID，例如 deepseek-chat" }
            TextArea { id: aiPrompt; width: 360; height: 90; placeholderText: "描述要修改的组件效果" }
        }
        onAccepted: {
            if (workbenchRuntime.requestAiComponentDraft(aiEndpoint.text, aiApiKey.text,
                                                         aiModel.text, aiPrompt.text))
                close()
        }
    }

    Dialog {
        id: signInDialog
        anchors.centerIn: Overlay.overlay
        width: 420
        modal: true
        title: "登录"
        standardButtons: Dialog.Cancel | Dialog.Ok
        contentItem: Column {
            spacing: 10
            TextField { id: supabaseProjectUrl; placeholderText: "Supabase 项目 URL" }
            TextField { id: supabaseAnonKey; placeholderText: "Supabase anon key"; echoMode: TextInput.Password }
            TextField { id: signInEmail; placeholderText: "邮箱" }
            TextField { id: signInPassword; placeholderText: "密码"; echoMode: TextInput.Password }
        }
        onAccepted: {
            if (workbenchRuntime.signInWithSupabase(supabaseProjectUrl.text, supabaseAnonKey.text,
                                                     signInEmail.text, signInPassword.text))
                close()
        }
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

    FileDialog {
        id: pluginApplyDialog
        title: "导出并应用插件动画"
        fileMode: FileDialog.SaveFile
        nameFilters: ["透明视频 (*.mov)", "所有文件 (*)"]
        onAccepted: workbenchRuntime.applyInstalledPluginToTimeline(
                         "apply-" + workbenchRuntime.playheadFrame,
                         compositionIdField.text,
                         selectedFile.toLocalFile())
    }

    FileDialog {
        id: componentSaveDialog
        title: "保存组件 JSON"
        fileMode: FileDialog.SaveFile
        nameFilters: ["组件 JSON (*.json)", "所有文件 (*)"]
        onAccepted: workbenchRuntime.saveComponentJson(selectedFile.toLocalFile())
    }

    Dialog {
        id: componentPackageDialog
        anchors.centerIn: Overlay.overlay
        width: 420
        modal: true
        title: "保存组件包"
        standardButtons: Dialog.Cancel | Dialog.Ok
        contentItem: Column {
            spacing: 10
            TextField { id: packageResourceId; placeholderText: "资源 ID，例如 demo.card"; text: "demo.card" }
            TextField { id: packageDisplayName; placeholderText: "显示名称"; text: "Edward 组件" }
        }
        onAccepted: componentPackageDirectoryDialog.open()
    }

    FolderDialog {
        id: componentPackageDirectoryDialog
        title: "选择组件包保存目录"
        onAccepted: workbenchRuntime.saveComponentPackage(
                         selectedFolder.toLocalFile(), packageResourceId.text, packageDisplayName.text)
    }

    FileDialog {
        id: componentFileDialog
        title: "打开组件 JSON"
        fileMode: FileDialog.OpenFile
        nameFilters: ["组件 JSON (*.json)", "所有文件 (*)"]
        onAccepted: workbenchRuntime.loadComponentFile(selectedFile.toLocalFile())
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
            failureTimer.restart();
        }
        function onOperationSucceeded(message) {
            successToast.text = message;
            successToast.open();
            successTimer.restart();
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
        Timer { id: failureTimer; interval: 5000; repeat: false; onTriggered: failureToast.close() }
    }

    Dialog {
        id: successToast
        modal: false
        anchors.centerIn: Overlay.overlay
        property alias text: successLabel.text
        contentItem: Label {
            id: successLabel
            color: DesignTokens.textPrimary
            padding: 16
        }
        Timer { id: successTimer; interval: 5000; repeat: false; onTriggered: successToast.close() }
    }
}
