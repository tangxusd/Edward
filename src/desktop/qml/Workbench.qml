import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtWebEngine
import "."

ApplicationWindow {
    id: window
    property var importedMediaClips: workbenchRuntime.clips.filter(function(clip) { return clip.kind === "media"; })
    property string recoveryProjectPath: ""
    property string exportOutputDirectory: ""
    property string exportFileName: "未命名项目.mp4"
    property bool fablecutEmbedded: true
    property real uiScale: width / 388.0
    function uiFontSize(baseSize) {
        return baseSize + (uiScale < 1.0 ? 1 : 0)
    }
    property int sidebarControlWidth: 340
    property int activeRailIndex: 0
    property int activeInspectorTab: 0
    property int resourceTabIndex: 0
    property bool transformExpanded: true
    property bool effectsExpanded: true
    property bool infoExpanded: true
    property string activeUtility: ""
    property var aiAttachments: []
    property int aiProcessingDots: 1
    property var inspectorTabs: ["视频", "音频", "效果", "文本"]
    property var railItems: [
        { label: "首页", icon: "qrc:/qml/icons/home.svg", activeIcon: "qrc:/qml/home-active.svg" },
        { label: "媒体", icon: "qrc:/qml/icons/media.svg", activeIcon: "qrc:/qml/media-active.svg" },
        { label: "文本", icon: "qrc:/qml/icons/text.svg", activeIcon: "qrc:/qml/text-active.svg" },
        { label: "音频", icon: "qrc:/qml/icons/audio.svg", activeIcon: "qrc:/qml/audio-active.svg" },
        { label: "卡片", icon: "qrc:/qml/icons/card.svg", activeIcon: "qrc:/qml/card-active.svg" },
        { label: "图表", icon: "qrc:/qml/icons/chart.svg", activeIcon: "qrc:/qml/chart-active.svg" },
        { label: "背景", icon: "qrc:/qml/icons/background.svg", activeIcon: "qrc:/qml/background-active.svg" },
        { label: "标注", icon: "qrc:/qml/icons/annotation.svg", activeIcon: "qrc:/qml/annotation-active.svg" },
        { label: "数字", icon: "qrc:/qml/icons/number.svg", activeIcon: "qrc:/qml/number-active.svg" }
    ]
    property string activeRailLabel: activeRailIndex >= 0 && activeRailIndex < railItems.length
                                      ? railItems[activeRailIndex].label : "AI"
    function selectedNodeSupports(property) {
        var type = workbenchRuntime.selectedComponentNodeType
        if (activeInspectorTab === 1) return false
        if (activeInspectorTab === 2 && ["rotation", "scale", "opacity"].indexOf(property) < 0) return false
        if (activeInspectorTab === 3 && ["color", "fontFamily", "opacity"].indexOf(property) < 0) return false
        var properties = {
            "text": ["position", "size", "rotation", "scale", "opacity", "color", "fontFamily"],
            "shape": ["position", "size", "opacity", "color", "borderColor", "borderWidth"],
            "image": ["position", "size", "opacity"]
        }
        return (properties[type] || []).indexOf(property) >= 0
    }
    function selectedNodeSupportsKeyframe(property) {
        var type = workbenchRuntime.selectedComponentNodeType
        if (property === "position") return type === "text" || type === "shape" || type === "image"
        return property === "opacity" && type === "text"
    }
    function componentSelectionActive() {
        return workbenchRuntime.resolveSelectionKind === "component" && workbenchRuntime.demoOverlayEnabled
    }
    function selectionPropertyHint() {
        if (window.componentSelectionActive()) return "组件属性：Edward 控制"
        var hints = {"video": "视频片段属性：请在 Resolve 原生 Inspector 调整",
                     "audio": "音频片段属性：请在 Resolve 原生 Inspector 调整",
                     "subtitle": "字幕属性：请在 Resolve 原生字幕面板调整",
                     "none": "未选中对象：请先在 Resolve 时间线选择对象"}
        return hints[workbenchRuntime.resolveSelectionKind] || "当前对象属性：由 Resolve 原生面板管理"
    }
    function activeResourceCount() {
        if (activeRailIndex === 1) return importedMediaClips.length
        if (activeRailIndex === 2) return workbenchRuntime.clips.filter(function(clip) { return clip.kind === "subtitle"; }).length
        if (activeRailIndex === 3) return workbenchRuntime.clips.filter(function(clip) { return clip.kind === "audio"; }).length
        return workbenchRuntime.localComponents.length
    }
    function activeTimelineClips() {
        var kind = activeRailIndex === 1 ? "media" : activeRailIndex === 2 ? "subtitle" : activeRailIndex === 3 ? "audio" : ""
        if (kind.length === 0) return []
        return workbenchRuntime.clips.filter(function(clip) { return clip.kind === kind })
    }
    function capabilityStatus() {
        if (workbenchRuntime.resolveActionPlan.length === 0)
            return ""
        try {
            var plan = JSON.parse(workbenchRuntime.resolveActionPlan)
            var status = plan.capabilityStatus || ""
            if (status === "verified") return "能力状态：已验证"
            if (status === "candidate_verified") return "能力状态：候选已验证"
            if (status === "unverified") return "能力状态：未验证"
            if (status === "disabled") return "能力状态：已禁用"
            if (status === "unsupported") return "能力状态：不支持"
        } catch (error) {
            return ""
        }
        return ""
    }
    function executionStatus() {
        if (workbenchRuntime.resolveActionLastResult.length === 0)
            return ""
        try {
            var result = JSON.parse(workbenchRuntime.resolveActionLastResult)
            if (result.status === "applied") {
                var item = result.verifiedTimelineItemId || ""
                return item.length > 0 ? "最近执行：已发送并回读（片段 " + item + "），请在 Resolve 预览确认" : "最近执行：已发送并回读，请在 Resolve 预览确认"
            }
            if (result.status === "failed") return "最近执行：失败"
        } catch (error) {
            return ""
        }
        return ""
    }
    function rebuildTransactionStatus() {
        var id = workbenchRuntime.resolveRebuildTransactionId || ""
        var state = workbenchRuntime.resolveRebuildTransactionState || ""
        if (id.length === 0) return "重建事务：未开始"
        var labels = {"open": "待校验", "validated": "已校验", "committed": "已提交", "rolled_back": "已回滚"}
        return "重建事务：" + (labels[state] || state) + "（" + id.slice(0, 8) + "）"
    }
    function capabilityMatrixSummary() {
        if (workbenchRuntime.resolveActionLastResult.length === 0)
            return ""
        try {
            var result = JSON.parse(workbenchRuntime.resolveActionLastResult)
            var entries = (result.lastWriteResult || {}).capabilities || []
            if (entries.length === 0) return ""
            var labels = {"verified": "已验证", "candidate_verified": "候选", "unverified": "未验证", "disabled": "已禁用", "unsupported": "不支持"}
            var names = {"text": "文字", "rectangle": "矩形", "image": "图片", "ograf": "OGraf（已禁用）", "particle": "粒子", "3d": "3D"}
            var parts = []
            for (var i = 0; i < entries.length; ++i) {
                var entry = entries[i]
                parts.push((names[entry.id] || entry.id) + "：" + (labels[entry.status] || entry.status))
            }
            return "组件能力：" + parts.join("、")
        } catch (error) {
            return ""
        }
    }
    function sendAiDockMessage() {
        var prompt = aiDockInput.text.trim()
        if (aiAttachments.length > 0) {
            var refs = []
            for (var i = 0; i < aiAttachments.length; ++i) refs.push(aiAttachments[i].name + ":" + aiAttachments[i].url)
            prompt = (prompt.length > 0 ? prompt + "\n" : "") + "[附件]\n" + refs.join("\n")
        }
        if (prompt.length === 0) {
            workbenchRuntime.appendAiConversationError("AI 请求未发送：输入内容为空")
            return
        }
        aiDockInput.clear()
        aiAttachments = []
        var accepted = workbenchRuntime.requestAiComponentDraft("", "", "", prompt)
        if (!accepted) {
            aiDockInput.placeholderText = "请先在设置中配置模型"
            aiDockInput.forceActiveFocus()
        } else {
            aiDockInput.placeholderText = "给 Edward 发消息…"
        }
    }
    function addAiAttachment(url) {
        if (!url || url.length === 0) return
        var clean = url.replace(/^file:\/\//, "")
        var parts = clean.split("/")
        var name = parts.length > 0 ? parts[parts.length - 1] : url
        aiAttachments = aiAttachments.concat([{url: url, name: name}])
    }
    function aiModelEntries() {
        var provider = workbenchRuntime.aiProviderName || "当前供应商"
        var entries = [{kind: "provider", label: provider}]
        var models = workbenchRuntime.aiAvailableModels.slice()
        var saved = (workbenchRuntime.aiModelList || "").split(/[\r\n,]+/)
        for (var s = 0; s < saved.length; ++s) if (saved[s].trim() !== "" && models.indexOf(saved[s].trim()) < 0) models.push(saved[s].trim())
        if (models.length === 0) models = ["GPT-4o", "Claude", "Gemini", "通义千问", "豆包"]
        for (var i = 0; i < models.length; ++i) entries.push({kind: "model", label: models[i]})
        return entries
    }
    function conversationMessages() {
        var raw = workbenchRuntime.aiConversation
        if (!raw || raw.length === 0) return [{role: "ai", text: "你好，我是 Edward。已就绪，可帮你分析片段、调整参数或生成效果。"}]
        var lines = raw.split("\n")
        var result = []
        var current = null
        for (var i = 0; i < lines.length; ++i) {
            var line = lines[i]
            if (line.indexOf("用户：") === 0) {
                if (current) result.push(current)
                current = {role: "user", text: line.slice(3)}
            } else if (line.indexOf("AI：") === 0) {
                if (current) result.push(current)
                current = {role: "ai", text: line.slice(3)}
            } else if (current) {
                current.text += "\n" + line
            }
        }
        if (current) result.push(current)
        return result
    }
    function pasteAiAttachmentToBar() {
        var pasted = workbenchRuntime.pasteAiAttachments()
        var added = false
        for (var i = 0; i < pasted.length; ++i) {
            if (/^(https?:|file:)/.test(pasted[i]) || /\.(png|jpe?g|webp|gif|mp4|mov|mkv|mp3|wav|m4a|txt|md)$/i.test(pasted[i])) {
                window.addAiAttachment(pasted[i]); added = true
            }
        }
        return added
    }
    visible: true
    title: workbenchRuntime.projectWindowTitle
    color: DesignTokens.background
    width: 1
    minimumHeight: 680

    Connections {
        target: workbenchRuntime
        function onResolveStateChanged() {
            // 窗口尺寸由 C++ 按当前屏幕可用宽度比例统一约束。
        }
        function onTimelineChanged() {
            if (workbenchRuntime.exportDialogRequested) {
                timelineExportPanel.open()
                workbenchRuntime.clearExportDialogRequest()
            }
        }
    }

    Component.onCompleted: {
        Qt.callLater(function() {
            if (workbenchRuntime.qualityImprovementNoticeRequired)
                qualityImprovementNoticeDialog.open()
        })
    }

    Shortcut {
        sequence: "Return"
        enabled: aiDockInput.activeFocus
        context: Qt.WindowShortcut
        onActivated: window.sendAiDockMessage()
    }
    Timer {
        id: aiProcessingTimer
        interval: 350
        repeat: true
        running: workbenchRuntime.aiRequestBusy
        onTriggered: window.aiProcessingDots = window.aiProcessingDots % 3 + 1
    }
    Shortcut {
        sequence: "Enter"
        enabled: aiDockInput.activeFocus
        context: Qt.WindowShortcut
        onActivated: window.sendAiDockMessage()
    }
    Shortcut {
        sequence: "Ctrl+V"
        enabled: aiDockInput.activeFocus
        context: Qt.WindowShortcut
        onActivated: window.pasteAiAttachmentToBar()
    }
    Shortcut {
        sequence: "Meta+V"
        enabled: aiDockInput.activeFocus
        context: Qt.WindowShortcut
        onActivated: window.pasteAiAttachmentToBar()
    }

    ColumnLayout {
        width: 388
        height: window.height / Math.max(window.uiScale, 0.001)
        transform: Scale {
            xScale: window.uiScale
            yScale: window.uiScale
            origin.x: 0
            origin.y: 0
        }
        spacing: 0

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 76
                Layout.minimumHeight: 76
                Layout.maximumHeight: 76
                color: DesignTokens.background
                Rectangle { anchors.top: parent.top; anchors.right: parent.right; anchors.bottom: parent.bottom; width: 1; color: DesignTokens.divider }
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    spacing: 1
                    Image {
                        width: 26; height: 26
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        Layout.alignment: Qt.AlignVCenter
                        Layout.rightMargin: 8
                        // 使用 1024px 正式 PNG 源以避免 Qt SVG 小尺寸栅格化产生四角杂色；源文件含透明 Alpha。
                        source: "qrc:/qml/video-editor-logo-indigo-pink.png"; fillMode: Image.PreserveAspectFit
                        sourceSize.width: 1024
                        sourceSize.height: 1024
                        smooth: true
                        mipmap: false
                    }
                    Repeater {
                        model: window.railItems
                        delegate: Button {
                            width: 44; height: 48
                            Layout.alignment: Qt.AlignVCenter
                            Layout.preferredHeight: 48
                            Layout.preferredWidth: 44
                            onClicked: {
                                window.activeRailIndex = index
                                window.activeUtility = ""
                                window.activeInspectorTab = 0
                            }
                            background: Rectangle {
                                color: index === window.activeRailIndex ? DesignTokens.accentSoft : "transparent"
                                radius: 2
                                border.width: 0
                                Rectangle {
                                    visible: index === window.activeRailIndex
                                    width: 2
                                    height: parent.height - 18
                                    anchors.left: parent.left
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: DesignTokens.accent
                                    topLeftRadius: 0
                                    topRightRadius: 2
                                    bottomRightRadius: 2
                                    bottomLeftRadius: 0
                                    // CSS 合同：top: 9px; bottom: 9px；旧版合同记录 anchors.topMargin: parent.height * 0.05 / anchors.bottomMargin: parent.height * 0.05。
                                }
                            }
                            contentItem: Item {
                                implicitWidth: 44
                                implicitHeight: 44
                                Column {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 44
                                    spacing: 2
                                    Image { anchors.horizontalCenter: parent.horizontalCenter; width: 17; height: 17; source: index === window.activeRailIndex ? modelData.activeIcon : modelData.icon; sourceSize.width: 17; sourceSize.height: 17; fillMode: Image.PreserveAspectFit; smooth: true; asynchronous: false }
                                    Label { width: parent.width; height: 11; text: modelData.label; horizontalAlignment: Text.AlignHCenter; color: index === window.activeRailIndex ? DesignTokens.accent : DesignTokens.textSecondary; font.pixelSize: window.uiFontSize(9); font.weight: Font.Normal }
                                }
                            }
                        }
                    }
                    Item { width: 1; height: 1; Layout.fillWidth: true; Layout.fillHeight: true }
                    Rectangle {
                        width: 48
                        height: 1
                        Layout.fillWidth: true
                        color: DesignTokens.divider
                    }
                    Button {
                        width: 34; height: 30
                        onClicked: { window.activeRailIndex = -1; window.activeUtility = "ai" }
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredHeight: 30
                        Layout.preferredWidth: 34
                        background: Item {
                            Rectangle {
                                width: 29; height: 26
                                anchors.centerIn: parent
                                radius: 7
                                border.width: 1
                                border.color: "#27a2df"
                                gradient: Gradient {
                                    GradientStop { position: 0.0; color: "#182b35" }
                                    GradientStop { position: 1.0; color: "#28264a" }
                                }
                            }
                        }
                        contentItem: Image { width: 28; height: 28; anchors.centerIn: parent; source: "qrc:/qml/icons/ai.svg"; fillMode: Image.PreserveAspectFit; sourceSize.width: 28; sourceSize.height: 28; asynchronous: false }
                    }
                    Button {
                        width: 44; height: 40
                        onClicked: { window.activeRailIndex = -1; window.activeUtility = "settings" }
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredHeight: 40
                        Layout.preferredWidth: 44
                        background: Rectangle { color: "transparent"; radius: 2 }
                        contentItem: Column {
                            width: 44
                            height: 31
                            anchors.centerIn: parent
                            spacing: 2
                            Image { x: (parent.width - width) / 2; width: 18; height: 18; source: "qrc:/qml/icons/settings.svg"; fillMode: Image.PreserveAspectFit; sourceSize.width: 18; sourceSize.height: 18; asynchronous: false }
                            Label { x: 0; width: parent.width; height: 11; text: "设置"; color: DesignTokens.textSecondary; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: window.uiFontSize(9) }
                        }
                    }
                }
            }

            Rectangle {
                id: inspectorPane
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: DesignTokens.panel
                Rectangle {
                    id: headerDock
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 66
                    visible: window.activeRailIndex >= 0
                    z: 40
                    color: DesignTokens.background
                    Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: DesignTokens.divider }
                    Rectangle {
                        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                        height: 36; color: DesignTokens.background
                        Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: DesignTokens.divider }
                        RowLayout {
                            anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 6; spacing: 6
                            Text { text: "Edward"; color: DesignTokens.textPrimary; font.pixelSize: window.uiFontSize(11.5); font.bold: true; Layout.alignment: Qt.AlignVCenter }
                            Item { Layout.fillWidth: true }
                            Text { text: "Davinci Resolve连接"; color: DesignTokens.textSecondary; font.pixelSize: window.uiFontSize(9); Layout.alignment: Qt.AlignVCenter }
                            Rectangle { width: 7; height: 7; radius: 4; color: workbenchRuntime.resolveConnected ? DesignTokens.success : DesignTokens.error; Layout.alignment: Qt.AlignVCenter }
                            Button {
                                Layout.preferredWidth: 38; Layout.preferredHeight: 18
                                enabled: true
                                background: Rectangle { color: "transparent"; border.width: 1; border.color: DesignTokens.accent; radius: 2 }
                                contentItem: Text {
                                    text: workbenchRuntime.resolveConnected ? "已连" : "连接"
                                    color: workbenchRuntime.resolveConnected ? DesignTokens.textSecondary : DesignTokens.accent
                                    font.pixelSize: window.uiFontSize(9)
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                onClicked: if (!workbenchRuntime.resolveConnected) workbenchRuntime.connectResolve()
                            }
                        }
                    }
                    Row {
                        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                        height: 30; x: 2; spacing: 0
                        Repeater {
                            model: window.inspectorTabs
                            delegate: Button {
                                width: (headerDock.width - 4) / 4; height: 30
                                onClicked: window.activeInspectorTab = index
                                background: Rectangle {
                                    color: "transparent"
                                    Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 2; color: index === window.activeInspectorTab ? DesignTokens.accent : "transparent" }
                                }
                                contentItem: Label { text: modelData; color: index === window.activeInspectorTab ? DesignTokens.accent : DesignTokens.textTertiary; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: window.uiFontSize(9.5); font.bold: true }
                            }
                        }
                    }
                }
                Flickable {
                    id: sidecarScroller
                    visible: window.activeRailIndex >= 0
                    anchors.top: headerDock.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: aiConversationDock.top
                    contentWidth: width
                    contentHeight: sidecarContent.height + 32
                    clip: true
                    ScrollBar.vertical: ScrollBar {}

                    Column {
                    id: sidecarContent
                    width: sidecarScroller.width
                    anchors.top: parent.top
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.topMargin: 0
                    spacing: 0
                    Rectangle {
                        visible: false
                        height: 0
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: window.sidebarControlWidth
                        color: DesignTokens.background
                        border.width: 0
                        Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: DesignTokens.divider }
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 6
                            spacing: 6
                            Column {
                                Layout.alignment: Qt.AlignVCenter
                                spacing: 2
                            Text {
                                text: "Edward"
                                color: DesignTokens.textPrimary
                                font.pixelSize: window.uiFontSize(11.5)
                                font.bold: true
                            }
                            }
                            Item { Layout.fillWidth: true; width: 1; height: 1 }
                            Text {
                                Layout.alignment: Qt.AlignVCenter
                                text: "Davinci Resolve连接"
                                color: DesignTokens.textSecondary
                                font.pixelSize: window.uiFontSize(9)
                            }
                            Rectangle {
                                width: 7; height: 7; radius: 4
                                Layout.alignment: Qt.AlignVCenter
                                color: workbenchRuntime.resolveConnected ? DesignTokens.success : DesignTokens.error
                            }
                            Button {
                                width: 38; height: 18
                                Layout.alignment: Qt.AlignVCenter
                                text: workbenchRuntime.resolveConnected ? "已连" : "连接"
                                enabled: !workbenchRuntime.resolveConnected
                                font.pixelSize: window.uiFontSize(9)
                                background: Rectangle {
                                    color: "transparent"
                                    border.width: 1
                                    border.color: workbenchRuntime.resolveConnected ? DesignTokens.border : DesignTokens.accent
                                    radius: 2
                                }
                                onClicked: workbenchRuntime.connectResolve()
                            }
                        }
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: window.activeRailLabel
                        visible: window.activeRailIndex > 0
                        color: DesignTokens.accent
                        font.pixelSize: window.uiFontSize(13)
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: window.sidebarControlWidth
                        visible: window.activeRailIndex > 0
                        text: window.activeRailLabel + "资源：" + window.activeResourceCount()
                        color: DesignTokens.textSecondary
                        font.pixelSize: window.uiFontSize(10)
                        horizontalAlignment: Text.AlignHCenter
                    }
                    Row {
                        visible: false
                        height: 0
                        x: 2
                        width: window.sidebarControlWidth - 4
                        spacing: 0
                        Repeater {
                            model: window.inspectorTabs
                            delegate: Button {
                                width: 84
                                height: 30
                                text: modelData
                                font.pixelSize: window.uiFontSize(10)
                                onClicked: window.activeInspectorTab = index
                                background: Rectangle {
                                    color: "transparent"
                                    border.width: 0
                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.bottom: parent.bottom
                                        height: 2
                                        color: index === window.activeInspectorTab ? DesignTokens.accent : "transparent"
                                    }
                                }
                                contentItem: Label {
                                    text: modelData
                                    color: index === window.activeInspectorTab ? DesignTokens.accent : DesignTokens.textTertiary
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    font.pixelSize: window.uiFontSize(9.5)
                                }
                            }
                        }
                    }
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: window.sidebarControlWidth
                        height: 27
                        visible: window.activeRailIndex === 0
                        color: DesignTokens.panel
                        border.color: DesignTokens.divider
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            Text { text: "当前对象：" + (workbenchRuntime.resolveSelectionName || "跟随 Resolve 选择"); color: DesignTokens.textSecondary; font.pixelSize: window.uiFontSize(10); elide: Text.ElideRight; Layout.fillWidth: true }
                            Text { text: workbenchRuntime.resolveConnected ? "● 已同步" : "● 未连接"; color: workbenchRuntime.resolveConnected ? DesignTokens.success : DesignTokens.error; font.pixelSize: window.uiFontSize(9) }
                        }
                    }
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: window.sidebarControlWidth
                        height: 27
                        visible: window.activeRailIndex === 0
                        color: DesignTokens.panel
                        border.color: DesignTokens.divider
                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: 7
                            anchors.rightMargin: 7
                            spacing: 3
                            Repeater {
                                model: [
                                    {icon: "qrc:/qml/icons/action-split.svg", action: "split", hint: "分割当前片段"},
                                    {icon: "qrc:/qml/icons/action-delete.svg", action: "delete", hint: "删除当前片段"},
                                    {icon: "qrc:/qml/icons/action-ripple.svg", action: "ripple-delete", hint: "波纹删除"},
                                    {icon: "qrc:/qml/icons/action-subtitle.svg", action: "subtitle", hint: "添加字幕"},
                                    {icon: "qrc:/qml/icons/action-fade-in.svg", action: "fade-in", hint: "淡入"},
                                    {icon: "qrc:/qml/icons/action-fade-out.svg", action: "fade-out", hint: "淡出"},
                                    {icon: "qrc:/qml/icons/action-keep-range.svg", action: "keep-range", hint: "保留当前 I/O 范围"},
                                    {icon: "qrc:/qml/icons/action-marker.svg", action: "add-marker", hint: "在播放头添加标记"},
                                    {icon: "qrc:/qml/icons/action-color.svg", action: "clip-color", hint: "设置片段颜色"},
                                    {icon: "qrc:/qml/icons/action-io.svg", action: "mark-io", hint: "设置/清除 I/O"}
                                ]
                                delegate: Button {
                                    width: 26
                                    height: 24
                                    ToolTip.visible: hovered
                                    ToolTip.text: modelData.hint
                                    background: Rectangle { color: DesignTokens.input; border.color: DesignTokens.border; radius: 2 }
                                    contentItem: Image { anchors.centerIn: parent; width: 14; height: 14; source: modelData.icon; fillMode: Image.PreserveAspectFit; sourceSize.width: 14; sourceSize.height: 14 }
                                    onClicked: workbenchRuntime.executeResolveQuickAction(modelData.action)
                                }
                            }
                        }
                    }
                    ListView {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: window.sidebarControlWidth
                        height: (window.activeRailIndex >= 1 && window.activeRailIndex <= 3)
                                ? Math.min(108, Math.max(0, window.activeTimelineClips().length * 27)) : 0
                        visible: window.activeRailIndex >= 1 && window.activeRailIndex <= 3
                        clip: true
                        model: window.activeTimelineClips()
                        delegate: RowLayout {
                            required property var modelData
                            width: window.sidebarControlWidth
                            height: 25
                            spacing: 4
                            Label {
                                Layout.fillWidth: true
                                text: modelData.name || "未命名片段"
                                color: modelData.selected ? DesignTokens.accent : DesignTokens.textPrimary
                                elide: Text.ElideRight
                                font.pixelSize: window.uiFontSize(10)
                            }
                            Button {
                                Layout.preferredWidth: 42
                                Layout.preferredHeight: 22
                                text: "选中"
                                onClicked: workbenchRuntime.selectClip(modelData.id)
                            }
                        }
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: window.sidebarControlWidth
                        text: workbenchRuntime.resolveStatus
                        color: workbenchRuntime.resolveConnected ? DesignTokens.accent : DesignTokens.textSecondary
                        font.pixelSize: window.uiFontSize(11)
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: window.sidebarControlWidth
                        text: workbenchRuntime.resolveContextSummary
                        color: DesignTokens.textSecondary
                        font.pixelSize: window.uiFontSize(11)
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: window.sidebarControlWidth
                        text: {
                            var labels = {"video": "视频片段", "audio": "音频片段", "subtitle": "字幕片段", "component": "Fusion 组件", "none": "无选中对象"}
                            var kind = labels[workbenchRuntime.resolveSelectionKind] || "未识别对象"
                            return "当前对象：" + kind + (workbenchRuntime.resolveSelectionName.length > 0 ? " · " + workbenchRuntime.resolveSelectionName : "")
                        }
                        color: DesignTokens.textPrimary
                        font.pixelSize: window.uiFontSize(11)
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideMiddle
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: window.sidebarControlWidth
                        text: window.selectionPropertyHint()
                        color: window.componentSelectionActive() ? DesignTokens.accent : DesignTokens.textSecondary
                        font.pixelSize: window.uiFontSize(10)
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                    }
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: window.sidebarControlWidth
                        visible: workbenchRuntime.resolveConnected
                        text: "刷新 Resolve 上下文"
                        onClicked: workbenchRuntime.resolveConnected
                                  ? workbenchRuntime.refreshResolveTimeline()
                                  : workbenchRuntime.connectResolve()
                    }
                    Button {
                        objectName: "sidecarAccountButton"
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: window.sidebarControlWidth
                        text: workbenchRuntime.authenticated
                              ? "退出登录：" + workbenchRuntime.authenticatedUsername
                              : "登录"
                        onClicked: workbenchRuntime.authenticated
                                   ? workbenchRuntime.signOut()
                                   : signInDialog.open()
                    }
                    Button {
                        objectName: "sidecarSettingsButton"
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: window.sidebarControlWidth
                        text: "设置"
                        onClicked: {
                            proxyRootField.text = workbenchRuntime.proxyStorageRoot
                            cacheRootField.text = workbenchRuntime.cacheStorageRoot
                            renderRootField.text = workbenchRuntime.renderStorageRoot
                            previewStorageDialog.open()
                        }
                    }
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: window.sidebarControlWidth
                        text: "确保 Edward 字幕轨道"
                        enabled: workbenchRuntime.resolveConnected
                        onClicked: workbenchRuntime.ensureResolveSubtitleTrack()
                    }
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: window.sidebarControlWidth
                        text: "导入 Resolve 布局预设"
                        enabled: workbenchRuntime.resolveConnected
                        onClicked: resolveLayoutFileDialog.open()
                    }
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: window.sidebarControlWidth
                        text: "保存 Edward 侧栏布局"
                        enabled: workbenchRuntime.resolveConnected
                        onClicked: workbenchRuntime.saveResolveLayoutPreset("Edward Sidecar")
                    }
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: window.sidebarControlWidth
                        text: "加载 Edward 侧栏布局"
                        enabled: workbenchRuntime.resolveConnected
                        onClicked: workbenchRuntime.loadResolveLayoutPreset("Edward Sidecar")
                    }
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: window.sidebarControlWidth
                        text: "导出 Edward 侧栏布局"
                        enabled: workbenchRuntime.resolveConnected
                        onClicked: resolveLayoutSaveDialog.open()
                    }
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: window.sidebarControlWidth
                        text: workbenchRuntime.demoOverlayEnabled ? "重新生成组件草稿" : "生成组件草稿"
                        onClicked: workbenchRuntime.generateComponentDraft()
                    }
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: window.sidebarControlWidth
                        text: workbenchRuntime.componentBoundToClip ? "已绑定到片段" : "绑定到选中片段"
                        enabled: workbenchRuntime.demoOverlayEnabled && !workbenchRuntime.componentBoundToClip
                        onClicked: workbenchRuntime.bindComponentToSelectedClip()
                    }
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: window.sidebarControlWidth
                        text: workbenchRuntime.resolveComponentImportBusy ? "正在导入组件…" : "添加组件到时间线"
                        enabled: workbenchRuntime.demoOverlayEnabled && !workbenchRuntime.resolveComponentImportBusy
                        onClicked: workbenchRuntime.addCurrentComponentToTimeline()
                    }
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: window.sidebarControlWidth
                        text: "保存到我的组件库"
                        enabled: workbenchRuntime.demoOverlayEnabled
                        onClicked: componentLibrarySaveDialog.open()
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "我的组件"
                        color: DesignTokens.textSecondary
                    }
                    ListView {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 220
                        height: Math.min(150, Math.max(34, workbenchRuntime.localComponents.length * 38))
                        clip: true
                        model: workbenchRuntime.localComponents
                        delegate: Row {
                            required property var modelData
                            width: 220
                            height: 34
                            spacing: 4
                            Label {
                                width: 92
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData.displayName
                                color: DesignTokens.textPrimary
                                elide: Text.ElideRight
                            }
                            Button {
                                width: 58
                                height: 28
                                text: "载入"
                                onClicked: workbenchRuntime.loadLibraryComponent(modelData.resourceId)
                            }
                            Button {
                                width: 58
                                height: 28
                                text: "添加"
                                enabled: !workbenchRuntime.resolveComponentImportBusy
                                onClicked: workbenchRuntime.insertLibraryComponentAtPlayhead(modelData.resourceId)
                            }
                        }
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "AI 组件编辑"
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
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: window.componentSelectionActive() && !workbenchRuntime.componentPlayheadEditable
                        text: "播放头不在组件时间范围内"
                        color: DesignTokens.warning
                        font.pixelSize: window.uiFontSize(11)
                    }
                    ListView {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 220
                        height: 64
                        clip: true
                        model: workbenchRuntime.selectedComponentNodeKeyframes
                        delegate: Row {
                            required property var modelData
                            width: 220
                            height: 22
                            spacing: 4
                            Button {
                                width: 160
                                height: 22
                                text: modelData.field + " @ " + modelData.frame + "  " + modelData.easing
                                font.pixelSize: window.uiFontSize(11)
                                onClicked: workbenchRuntime.toggleSelectedComponentNodeKeyframeEasing(modelData.field, modelData.frame)
                                ToolTip.visible: hovered
                                ToolTip.text: "点击切换线性/贝塞尔"
                            }
                            Button {
                                width: 56
                                height: 22
                                text: "删除"
                                font.pixelSize: window.uiFontSize(11)
                                onClicked: workbenchRuntime.removeSelectedComponentNodeKeyframe(modelData.field, modelData.frame)
                            }
                        }
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: false
                        text: "组件 AI 会话"
                        color: DesignTokens.textSecondary
                    }
                    TextArea {
                        id: aiConversationView
                        visible: false
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
                        visible: false
                        text: workbenchRuntime.aiRequestBusy ? "AI 请求中" : "使用模型生成草案"
                        enabled: !workbenchRuntime.aiRequestBusy
                        onClicked: aiModelDialog.open()
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Resolve 操作"
                        color: DesignTokens.textSecondary
                    }
                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 3
                        enabled: workbenchRuntime.resolveConnected
                        Button {
                            width: 25; height: 26; text: "删"
                            ToolTip.visible: hovered; ToolTip.text: "删除当前片段"
                            onClicked: workbenchRuntime.executeResolveQuickAction("delete")
                        }
                        Button {
                            width: 25; height: 26; text: "波"
                            ToolTip.visible: hovered; ToolTip.text: "波纹删除当前片段"
                            onClicked: workbenchRuntime.executeResolveQuickAction("ripple-delete")
                        }
                        Button {
                            width: 25; height: 26; text: "字"
                            ToolTip.visible: hovered; ToolTip.text: "自动生成字幕"
                            onClicked: workbenchRuntime.executeResolveQuickAction("subtitle")
                        }
                        Button {
                            width: 25; height: 26; text: "入"
                            ToolTip.visible: hovered; ToolTip.text: "设置入点"
                            onClicked: workbenchRuntime.executeResolveQuickAction("set-in")
                        }
                        Button {
                            width: 25; height: 26; text: "出"
                            ToolTip.visible: hovered; ToolTip.text: "设置出点"
                            onClicked: workbenchRuntime.executeResolveQuickAction("set-out")
                        }
                        Button {
                            width: 25; height: 26; text: "标"
                            ToolTip.visible: hovered; ToolTip.text: "添加重点标记"
                            onClicked: workbenchRuntime.executeResolveQuickAction("add-marker")
                        }
                        Button {
                            width: 25; height: 26; text: "留"
                            ToolTip.visible: hovered; ToolTip.text: "保留当前 I/O 范围"
                            onClicked: workbenchRuntime.executeResolveQuickAction("keep-range")
                        }
                        Button {
                            width: 25; height: 26; text: "色"
                            ToolTip.visible: hovered; ToolTip.text: "将当前片段标为蓝色"
                            onClicked: workbenchRuntime.executeResolveQuickAction("clip-color")
                        }
                        Button {
                            width: 25; height: 26; text: "清"
                            ToolTip.visible: hovered; ToolTip.text: "清除 I/O"
                            onClicked: workbenchRuntime.executeResolveQuickAction("clear-io")
                        }
                        Button {
                            width: 25; height: 26; text: "淡入"
                            ToolTip.visible: hovered; ToolTip.text: "当前片段淡入"
                            onClicked: workbenchRuntime.executeResolveQuickAction("fade-in")
                        }
                        Button {
                            width: 25; height: 26; text: "淡出"
                            ToolTip.visible: hovered; ToolTip.text: "当前片段淡出"
                            onClicked: workbenchRuntime.executeResolveQuickAction("fade-out")
                        }
                    }
                    TextField {
                        id: resolveApiRequestField
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 220
                        placeholderText: "用自然语言描述 Resolve 操作"
                    }
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "查手册核对"
                        enabled: resolveApiRequestField.text.length > 0
                        onClicked: workbenchRuntime.assessResolveRequest(resolveApiRequestField.text)
                    }
                    TextArea {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 220
                        height: 78
                        readOnly: true
                        wrapMode: TextEdit.Wrap
                        text: workbenchRuntime.resolveApiAssessment.length > 0
                              ? workbenchRuntime.resolveApiAssessment
                              : "尚未核对"
                        color: DesignTokens.textPrimary
                    }
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "执行操作计划"
                        enabled: workbenchRuntime.resolveConnected
                                 && workbenchRuntime.resolveActionPlan.indexOf('"supported":true') >= 0
                        onClicked: workbenchRuntime.executeResolveActionPlan()
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Edward 操作计划"
                        color: DesignTokens.textSecondary
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: window.capabilityStatus()
                        visible: text.length > 0
                        color: text.indexOf("已验证") >= 0 ? DesignTokens.accent : DesignTokens.warning
                    }
                    TextArea {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 220
                        height: 96
                        readOnly: true
                        wrapMode: TextEdit.Wrap
                        text: workbenchRuntime.resolveActionPlan.length > 0
                              ? workbenchRuntime.resolveActionPlan
                              : "输入请求后生成计划，确认前不会执行"
                        color: DesignTokens.textPrimary
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: window.executionStatus()
                        visible: text.length > 0
                        color: text.indexOf("已发送并回读") >= 0 ? DesignTokens.accent : DesignTokens.warning
                        elide: Text.ElideMiddle
                        width: 220
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: window.rebuildTransactionStatus()
                        color: text.indexOf("已提交") >= 0 || text.indexOf("已校验") >= 0
                               ? DesignTokens.accent : DesignTokens.textSecondary
                        width: 220
                        elide: Text.ElideMiddle
                    }
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 220
                        height: 24
                        text: "导入 OTIO/EDL（隔离重建）"
                        enabled: workbenchRuntime.resolveConnected
                        onClicked: resolveRebuildFileDialog.open()
                    }
                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 8
                        visible: workbenchRuntime.resolveRebuildTransactionId.length > 0
                                 && (workbenchRuntime.resolveRebuildTransactionState === "open"
                                     || workbenchRuntime.resolveRebuildTransactionState === "validated")
                        Button {
                            width: 68
                            height: 24
                            text: "校验当前重建"
                            enabled: workbenchRuntime.resolveRebuildTransactionState === "open"
                            onClicked: workbenchRuntime.validateResolveRebuild()
                        }
                        Button {
                            width: 68
                            height: 24
                            text: "确认提交"
                            enabled: workbenchRuntime.resolveRebuildTransactionState === "validated"
                            onClicked: {
                                rebuildConfirmDialog.action = "commit"
                                rebuildConfirmDialog.title = "确认提交重建"
                                rebuildConfirmDialog.open()
                            }
                        }
                        Button {
                            width: 68
                            height: 24
                            text: "回滚"
                            enabled: workbenchRuntime.resolveRebuildTransactionState === "open"
                                     || workbenchRuntime.resolveRebuildTransactionState === "validated"
                            onClicked: {
                                rebuildConfirmDialog.action = "rollback"
                                rebuildConfirmDialog.title = "确认回滚重建"
                                rebuildConfirmDialog.open()
                            }
                        }
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: window.capabilityMatrixSummary()
                        visible: text.length > 0
                        color: DesignTokens.textSecondary
                        width: 220
                        wrapMode: Text.Wrap
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "组件 X: " + workbenchRuntime.demoOverlayX
                        color: DesignTokens.textSecondary
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: window.componentSelectionActive() && window.selectedNodeSupports("position")
                        text: "◆ 选中节点 X: " + workbenchRuntime.selectedComponentNodeX
                        color: DesignTokens.textSecondary
                    }
                    Slider {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: window.componentSelectionActive() && window.selectedNodeSupports("position")
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
                        visible: window.componentSelectionActive() && window.selectedNodeSupports("position")
                        text: "◆ 选中节点 Y: " + workbenchRuntime.selectedComponentNodeY
                        color: DesignTokens.textSecondary
                    }
                    Slider {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: window.componentSelectionActive() && window.selectedNodeSupports("position")
                        width: 220
                        from: -360
                        to: 360
                        value: workbenchRuntime.selectedComponentNodeY
                        enabled: workbenchRuntime.selectedComponentNodeId !== ""
                        onMoved: workbenchRuntime.selectedComponentNodeY = value
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: window.componentSelectionActive() && window.selectedNodeSupports("size")
                        text: "选中节点宽度: " + workbenchRuntime.selectedComponentNodeWidth
                        color: DesignTokens.textSecondary
                    }
                    Slider {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: window.componentSelectionActive() && window.selectedNodeSupports("size")
                        width: 220
                        from: 1
                        to: 640
                        value: workbenchRuntime.selectedComponentNodeWidth
                        enabled: workbenchRuntime.selectedComponentNodeId !== ""
                        onMoved: workbenchRuntime.selectedComponentNodeWidth = value
                        ToolTip.visible: hovered
                        ToolTip.text: "尺寸已验证可调；当前不支持关键帧"
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: window.componentSelectionActive() && window.selectedNodeSupports("size")
                        text: "选中节点高度: " + workbenchRuntime.selectedComponentNodeHeight
                        color: DesignTokens.textSecondary
                    }
                    Slider {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: window.componentSelectionActive() && window.selectedNodeSupports("size")
                        width: 220
                        from: 1
                        to: 360
                        value: workbenchRuntime.selectedComponentNodeHeight
                        enabled: workbenchRuntime.selectedComponentNodeId !== ""
                        onMoved: workbenchRuntime.selectedComponentNodeHeight = value
                        ToolTip.visible: hovered
                        ToolTip.text: "尺寸已验证可调；当前不支持关键帧"
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: window.componentSelectionActive() && window.selectedNodeSupports("rotation")
                        text: "选中节点旋转: " + workbenchRuntime.selectedComponentNodeRotation.toFixed(0) + "°"
                        color: DesignTokens.textSecondary
                    }
                    Slider {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: window.componentSelectionActive() && window.selectedNodeSupports("rotation")
                        width: 220
                        from: -180
                        to: 180
                        value: workbenchRuntime.selectedComponentNodeRotation
                        enabled: workbenchRuntime.selectedComponentNodeId !== ""
                        onMoved: workbenchRuntime.selectedComponentNodeRotation = value
                        ToolTip.visible: hovered
                        ToolTip.text: "旋转已验证可调；当前不支持关键帧"
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: window.componentSelectionActive() && window.selectedNodeSupports("scale")
                        text: "选中节点缩放: " + workbenchRuntime.selectedComponentNodeScale.toFixed(2)
                        color: DesignTokens.textSecondary
                    }
                    Slider {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: window.componentSelectionActive() && window.selectedNodeSupports("scale")
                        width: 220
                        from: 0.1
                        to: 3
                        value: workbenchRuntime.selectedComponentNodeScale
                        enabled: workbenchRuntime.selectedComponentNodeId !== ""
                        onMoved: workbenchRuntime.selectedComponentNodeScale = value
                        ToolTip.visible: hovered
                        ToolTip.text: "缩放已验证可调；当前不支持关键帧"
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: window.componentSelectionActive() && window.selectedNodeSupports("opacity")
                        text: "◆ 选中节点透明度: " + workbenchRuntime.selectedComponentNodeOpacity.toFixed(2)
                        color: DesignTokens.textSecondary
                    }
                    Slider {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: window.componentSelectionActive() && window.selectedNodeSupports("opacity")
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
                        visible: window.componentSelectionActive() && window.selectedNodeSupports("color")
                        enabled: visible
                        onEditingFinished: workbenchRuntime.selectedComponentNodeColor = text
                    }
                    TextField {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 220
                        text: workbenchRuntime.selectedComponentNodeBorderColor
                        placeholderText: "边框颜色，例如 #ffffff"
                        visible: window.componentSelectionActive() && window.selectedNodeSupports("borderColor")
                        enabled: visible
                        onEditingFinished: workbenchRuntime.selectedComponentNodeBorderColor = text
                    }
                    TextField {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 220
                        text: workbenchRuntime.selectedComponentNodeFontFamily
                        placeholderText: "字体族，例如 PingFang SC"
                        visible: window.componentSelectionActive() && window.selectedNodeSupports("fontFamily")
                        enabled: visible
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
                Rectangle {
                    id: homePropertyPanel
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: aiConversationDock.top
                    anchors.topMargin: 67
                    visible: window.activeRailIndex === 0
                    z: 10
                    color: DesignTokens.panelRaised
                    clip: true
                    Flickable {
                        anchors.fill: parent
                        contentWidth: width
                        contentHeight: homePropertyColumn.height
                        clip: true
                        Column {
                            id: homePropertyColumn
                            width: homePropertyPanel.width
                            spacing: 0
                            Rectangle {
                                width: parent.width
                                height: 27
                                color: DesignTokens.panel
                                border.width: 0
                                Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: DesignTokens.divider }
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    Text { text: "当前对象：" + (workbenchRuntime.resolveSelectionName || "跟随 Resolve 选择"); color: DesignTokens.textSecondary; font.pixelSize: window.uiFontSize(10); Layout.fillWidth: true; elide: Text.ElideRight }
                                    Text { text: workbenchRuntime.resolveConnected ? "● 已同步" : "● 未同步"; color: workbenchRuntime.resolveConnected ? DesignTokens.success : DesignTokens.error; font.pixelSize: window.uiFontSize(9) }
                                }
                            }
                            Row {
                                width: parent.width
                                height: 34
                                leftPadding: 7
                                rightPadding: 7
                                spacing: 3
                                Repeater {
                                    model: [
                                        {icon: "qrc:/qml/icons/action-split.svg", action: "split", hint: "分割当前片段"},
                                        {icon: "qrc:/qml/icons/action-delete.svg", action: "delete", hint: "删除当前片段"},
                                        {icon: "qrc:/qml/icons/action-ripple.svg", action: "ripple-delete", hint: "波纹删除"},
                                        {icon: "qrc:/qml/icons/action-subtitle.svg", action: "subtitle", hint: "添加字幕"},
                                        {icon: "qrc:/qml/icons/action-fade-in.svg", action: "fade-in", hint: "淡入"},
                                        {icon: "qrc:/qml/icons/action-fade-out.svg", action: "fade-out", hint: "淡出"},
                                        {icon: "qrc:/qml/icons/action-marker.svg", action: "add-marker", hint: "添加标记"},
                                        {icon: "qrc:/qml/icons/action-color.svg", action: "clip-color", hint: "片段颜色"},
                                        {icon: "qrc:/qml/icons/action-io.svg", action: "mark-io", hint: "设置/清除 I/O"}
                                    ]
                                    delegate: Button {
                                        width: 26; height: 24
                                        ToolTip.visible: hovered; ToolTip.text: modelData.hint
                                        background: Rectangle { color: DesignTokens.input; border.color: DesignTokens.border; radius: 2 }
                                        contentItem: Image { anchors.centerIn: parent; width: 14; height: 14; source: modelData.icon; sourceSize.width: 14; sourceSize.height: 14 }
                                        onClicked: workbenchRuntime.executeResolveQuickAction(modelData.action)
                                    }
                                }
                            }
                            Rectangle {
                                width: parent.width
                                height: 27
                                color: DesignTokens.panel
                                border.width: 0
                                Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: DesignTokens.divider }
                                Text { anchors.left: parent.left; anchors.leftMargin: 8; anchors.verticalCenter: parent.verticalCenter; text: window.transformExpanded ? "⌄" : "›"; color: DesignTokens.textTertiary; font.pixelSize: 10 }
                                Text { anchors.left: parent.left; anchors.leftMargin: 22; anchors.verticalCenter: parent.verticalCenter; text: "变换"; color: DesignTokens.textTertiary; font.pixelSize: window.uiFontSize(9.5); font.bold: true }
                                Rectangle {
                                    anchors.right: parent.right
                                    anchors.rightMargin: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 27; height: 13; radius: 7
                                    color: DesignTokens.accentSoft
                                    border.color: DesignTokens.accent
                                    Rectangle { x: 16; y: 1.5; width: 7; height: 7; radius: 4; color: DesignTokens.accent }
                                }
                            }
                            Repeater {
                                model: [
                                    {label: "位置 X", value: "1920.0", keyframe: true},
                                    {label: "位置 Y", value: "1080.0", keyframe: true},
                                    {label: "锚点 X", value: "0.00", keyframe: false},
                                    {label: "锚点 Y", value: "0.00", keyframe: false},
                                    {label: "缩放", section: true},
                                    {label: "缩放 X", value: "100.00", keyframe: false},
                                    {label: "缩放 Y", value: "100.00", keyframe: false},
                                    {label: "等比缩放", check: true},
                                    {label: "旋转", section: true},
                                    {label: "旋转角度", value: "0.00°", keyframe: true},
                                    {label: "俯仰", value: "0.00°", keyframe: true},
                                    {label: "偏航", value: "0.00°", keyframe: true},
                                    {label: "翻转", section: true},
                                    {label: "方向：水平 / 垂直", flip: true},
                                    {label: "透明度", value: "100.00", keyframe: true},
                                    {label: "裁剪", section: true},
                                    {label: "左侧", value: "0.00", keyframe: false},
                                    {label: "右侧", value: "0.00", keyframe: false},
                                    {label: "顶部", value: "0.00", keyframe: false},
                                    {label: "底部", value: "0.00", keyframe: false},
                                    {label: "柔化", value: "0.00", keyframe: false},
                                    {label: "保留图像位置", check: true}
                                ]
                                delegate: Item {
                                    x: 8
                                    width: homePropertyColumn.width - 16
                                    height: window.transformExpanded ? (!!modelData.section ? 27 : 21) : 0
                                    visible: window.transformExpanded
                                    Rectangle {
                                        anchors.fill: parent
                                        visible: !!modelData.section
                                        color: DesignTokens.panel
                                        Text { anchors.left: parent.left; anchors.leftMargin: 34; anchors.verticalCenter: parent.verticalCenter; text: modelData.label || ""; color: DesignTokens.textTertiary; font.pixelSize: window.uiFontSize(9.5); font.bold: true }
                                        MouseArea { anchors.fill: parent; onClicked: window.transformExpanded = !window.transformExpanded }
                                    }
                                    RowLayout {
                                        anchors.fill: parent
                                        visible: !modelData.section && !modelData.check && !modelData.flip
                                        spacing: 0
                                        Text { text: !!modelData.keyframe ? "◆" : "◇"; color: !!modelData.keyframe ? DesignTokens.accent : DesignTokens.textTertiary; font.pixelSize: 9; Layout.preferredWidth: 16; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                        Text { text: modelData.label; color: DesignTokens.textSecondary; font.pixelSize: window.uiFontSize(11); Layout.preferredWidth: 76; verticalAlignment: Text.AlignVCenter; leftPadding: 1 }
                                        Item {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 21
                                            Slider {
                                                anchors.fill: parent
                                                from: 0; to: 100; value: 50
                                                background: Rectangle { x: 0; y: (parent.height - 2) / 2; width: parent.width; height: 2; color: "#0d0d0d"; radius: 1 }
                                                handle: Rectangle { x: parent.leftPadding + parent.visualPosition * (parent.availableWidth - width); y: (parent.height - 9) / 2; width: 9; height: 9; radius: 5; color: "#c0c0c0"; border.color: "#e0e0e0"; border.width: 1 }
                                            }
                                        }
                                        Rectangle { Layout.preferredWidth: 44; Layout.preferredHeight: 17; color: DesignTokens.input; border.color: DesignTokens.border; radius: 2; Text { anchors.fill: parent; anchors.rightMargin: 4; text: modelData.value || "0.00"; color: DesignTokens.textPrimary; font.pixelSize: window.uiFontSize(10); font.family: "Menlo"; horizontalAlignment: Text.AlignRight; verticalAlignment: Text.AlignVCenter } }
                                        Text { text: "↻"; color: DesignTokens.accent; font.pixelSize: 12; Layout.preferredWidth: 14; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    }
                                    RowLayout {
                                        anchors.fill: parent
                                        visible: !!modelData.check
                                        Text { text: modelData.label; color: DesignTokens.textSecondary; font.pixelSize: window.uiFontSize(11); Layout.preferredWidth: 92; leftPadding: 17 }
                                        Rectangle {
                                            id: ratioCheck
                                            Layout.preferredWidth: 13; Layout.preferredHeight: 13
                                            color: checked ? DesignTokens.accent : DesignTokens.input
                                            border.color: checked ? DesignTokens.accent : DesignTokens.border
                                            radius: 2
                                            property bool checked: true
                                            Text { anchors.centerIn: parent; text: "✓"; visible: parent.checked; color: "#ffffff"; font.pixelSize: 11; font.bold: true }
                                            MouseArea { anchors.fill: parent; onClicked: ratioCheck.checked = !ratioCheck.checked }
                                        }
                                    }
                                    RowLayout {
                                        anchors.fill: parent
                                        visible: !!modelData.flip
                                        Text { text: "方向"; color: DesignTokens.textSecondary; font.pixelSize: window.uiFontSize(11); Layout.preferredWidth: 92; leftPadding: 17 }
                                        Button { text: "水平"; Layout.preferredWidth: 52; Layout.preferredHeight: 22; background: Rectangle { color: DesignTokens.input; border.color: DesignTokens.border; radius: 2 } }
                                        Button { text: "垂直"; Layout.preferredWidth: 52; Layout.preferredHeight: 22; background: Rectangle { color: DesignTokens.input; border.color: DesignTokens.border; radius: 2 } }
                                    }
                                }
                            }
                            Rectangle {
                                width: parent.width
                                height: 27
                                color: DesignTokens.panel
                                Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: DesignTokens.divider }
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    Text { text: effectsExpanded ? "⌄" : "›"; color: DesignTokens.textTertiary; font.pixelSize: 10 }
                                    Text { text: "效果"; color: DesignTokens.textTertiary; font.pixelSize: window.uiFontSize(9.5); font.bold: true; Layout.fillWidth: true }
                                    Text { text: "2"; color: DesignTokens.textTertiary; font.pixelSize: window.uiFontSize(9) }
                                }
                                MouseArea { anchors.fill: parent; onClicked: window.effectsExpanded = !window.effectsExpanded }
                            }
                            Repeater {
                                model: ["模糊效果", "胶片颗粒"]
                                delegate: Rectangle {
                                    x: 8
                                    width: parent.width - 16
                                    height: window.effectsExpanded ? 22 : 0
                                    visible: window.effectsExpanded
                                    color: "transparent"
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 5
                                        anchors.rightMargin: 5
                                        spacing: 5
                                        Rectangle { Layout.preferredWidth: 5; Layout.preferredHeight: 5; radius: 3; color: DesignTokens.accent }
                                        Text { text: modelData; color: DesignTokens.textPrimary; font.pixelSize: window.uiFontSize(11); Layout.fillWidth: true }
                                        Text { text: "OFX"; color: DesignTokens.textTertiary; font.pixelSize: window.uiFontSize(8.5); font.bold: true }
                                        Text { text: "⋮"; color: DesignTokens.textTertiary; font.pixelSize: 14 }
                                    }
                                }
                            }
                            Rectangle {
                                width: parent.width
                                height: 27
                                color: DesignTokens.panel
                                Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: DesignTokens.divider }
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    Text { text: infoExpanded ? "⌄" : "›"; color: DesignTokens.textTertiary; font.pixelSize: 10 }
                                    Text { text: "片段信息"; color: DesignTokens.textTertiary; font.pixelSize: window.uiFontSize(9.5); font.bold: true; Layout.fillWidth: true }
                                }
                                MouseArea { anchors.fill: parent; onClicked: window.infoExpanded = !window.infoExpanded }
                            }
                            Repeater {
                                model: ["文件名    Clip_001.mov", "分辨率    3840×2160", "帧率      25 fps", "时长      00:00:05:12", "编解码器  ProRes 422", "状态      ● 已链接"]
                                delegate: Text {
                                    x: 8
                                    width: parent.width - 16
                                    height: window.infoExpanded ? 21 : 0
                                    visible: window.infoExpanded
                                    leftPadding: 0
                                    text: modelData
                                    color: index === 5 ? DesignTokens.success : DesignTokens.textSecondary
                                    font.pixelSize: window.uiFontSize(10)
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                        }
                    }
                }
                Rectangle {
                    id: aiConversationDock
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: sidecarFooter.top
                    // HTML inspector body：总高减 Header 36、Tabs 30、Footer 40 后，上下两区等分。
                    height: (parent.height - 106) / 2
                    visible: window.activeRailIndex === 0
                    z: 40
                    color: DesignTokens.panelRaised
                    border.width: 0
                    Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; height: 2; color: DesignTokens.accent; z: 5 }
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        anchors.bottomMargin: 2
                        spacing: 6
                        Label {
                            text: "AI 对话"
                            color: DesignTokens.accent
                            font.pixelSize: window.uiFontSize(9)
                            font.bold: true
                        }
                        Flickable {
                            id: chatMessagesFlickable
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            contentWidth: width
                            contentHeight: chatMessagesColumn.height
                            clip: true
                            onContentHeightChanged: Qt.callLater(function() {
                                contentY = Math.max(0, contentHeight - height)
                            })
                            onHeightChanged: Qt.callLater(function() {
                                contentY = Math.max(0, contentHeight - height)
                            })
                            Column {
                                id: chatMessagesColumn
                                width: parent.width
                                spacing: 7
                                Repeater {
                                    model: window.conversationMessages()
                                    delegate: Rectangle {
                                        width: parent.width * 0.88
                                        x: modelData.role === "user" ? parent.width - width : 0
                                        height: chatMessageText.implicitHeight + 14
                                        color: modelData.role === "user" ? DesignTokens.accentSoft : DesignTokens.input
                                        radius: 8
                                        Text {
                                            id: chatMessageText
                                            anchors.fill: parent
                                            anchors.margins: 7
                                            text: modelData.role === "user" ? modelData.text : "Edward\n" + modelData.text
                                            color: DesignTokens.textPrimary
                                            font.pixelSize: window.uiFontSize(11)
                                            lineHeight: 1.45
                                            wrapMode: Text.Wrap
                                        }
                                    }
                                }
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: workbenchRuntime.aiRequestBusy
                            text: "正在处理" + ".".repeat(window.aiProcessingDots)
                            color: DesignTokens.accent
                            font.pixelSize: window.uiFontSize(9)
                            horizontalAlignment: Text.AlignLeft
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            visible: workbenchRuntime.aiComponentDraftAvailable
                            spacing: 6
                            Button {
                                Layout.fillWidth: true
                                visible: workbenchRuntime.aiAnalysisDraftAvailable
                                text: "应用并添加到 Resolve"
                                enabled: workbenchRuntime.resolveConnected && !workbenchRuntime.resolveComponentImportBusy
                                onClicked: workbenchRuntime.applyAiAnalysisDraftToResolve()
                            }
                            Button {
                                Layout.fillWidth: true
                                visible: !workbenchRuntime.aiAnalysisDraftAvailable
                                text: "应用参数草案"
                                onClicked: workbenchRuntime.applyPendingAiComponentCommand()
                            }
                            Button {
                                Layout.preferredWidth: 56
                                text: "放弃"
                                onClicked: workbenchRuntime.discardPendingAiComponentCommand()
                            }
                        }
                        Rectangle {
                            objectName: "aiSendButton"
                            id: aiInputSurface
                            Layout.fillWidth: true
                            Layout.preferredHeight: 63 + (aiAttachments.length > 0 ? 30 : 0)
                            color: DesignTokens.panelRaised
                            radius: 6
                            border.width: 0
                            Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; height: 1; color: DesignTokens.divider }
                            Flow {
                                id: aiAttachmentBar
                                anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                                anchors.leftMargin: 6; anchors.rightMargin: 6; anchors.topMargin: 5
                                spacing: 4
                                visible: aiAttachments.length > 0
                                Repeater {
                                    model: aiAttachments
                                    delegate: Rectangle {
                                        width: 24; height: 24; radius: 3
                                        color: DesignTokens.input
                                        border.color: DesignTokens.border
                                        Image { anchors.centerIn: parent; width: 15; height: 15; source: "qrc:/qml/icons/attachment.svg"; sourceSize.width: 15; sourceSize.height: 15; fillMode: Image.PreserveAspectFit }
                                        ToolTip.visible: attachmentHover.containsMouse
                                        ToolTip.text: modelData.name
                                        MouseArea { id: attachmentHover; anchors.fill: parent; hoverEnabled: true; onClicked: { var next = aiAttachments.slice(); next.splice(index, 1); aiAttachments = next } }
                                    }
                                }
                            }
                            TextArea {
                                id: aiDockInput
                                anchors.fill: parent
                                anchors.leftMargin: 6; anchors.rightMargin: 6; anchors.bottomMargin: 6
                                anchors.topMargin: aiAttachments.length > 0 ? 35 : 6
                                placeholderText: "给 Edward 发消息…"
                                color: DesignTokens.textPrimary
                                font.pixelSize: window.uiFontSize(11)
                                wrapMode: TextEdit.Wrap
                                activeFocusOnPress: true
                                z: 2
                                background: Rectangle { color: DesignTokens.input; border.color: DesignTokens.border; radius: 6 }
                                Keys.onPressed: function(event) {
                                    if ((event.modifiers & (Qt.ControlModifier | Qt.MetaModifier)) !== 0 && event.key === Qt.Key_V) {
                                        var pasted = workbenchRuntime.pasteAiAttachments()
                                        var handledAttachment = false
                                        for (var p = 0; p < pasted.length; ++p) {
                                            if (/^(https?:|file:)/.test(pasted[p]) || /\.(png|jpe?g|webp|gif|mp4|mov|mkv|mp3|wav|m4a|txt|md)$/i.test(pasted[p])) { window.addAiAttachment(pasted[p]); handledAttachment = true }
                                        }
                                        // 普通文本/代码粘贴必须交还给 TextArea 默认处理，不能被附件快捷键吞掉。
                                        event.accepted = handledAttachment
                                        return
                                    }
                                    if (event.key !== Qt.Key_Return && event.key !== Qt.Key_Enter) return
                                    if ((event.modifiers & (Qt.MetaModifier | Qt.ControlModifier)) !== 0) {
                                        event.accepted = true
                                        aiDockInput.insert(aiDockInput.cursorPosition, "\n")
                                        return
                                    }
                                    event.accepted = true
                                    window.sendAiDockMessage()
                                }
                                Keys.onReturnPressed: function(event) {
                                    if ((event.modifiers & (Qt.MetaModifier | Qt.ControlModifier)) !== 0) {
                                        event.accepted = true
                                        aiDockInput.insert(aiDockInput.cursorPosition, "\n")
                                        return
                                    }
                                    event.accepted = true
                                    window.sendAiDockMessage()
                                }
                                Keys.onEnterPressed: function(event) {
                                    if ((event.modifiers & (Qt.MetaModifier | Qt.ControlModifier)) !== 0) {
                                        event.accepted = true
                                        aiDockInput.insert(aiDockInput.cursorPosition, "\n")
                                        return
                                    }
                                    event.accepted = true
                                    window.sendAiDockMessage()
                                }
                            }
                        }
                    }
                    Rectangle {
                        id: mediaLibrarySurface
                        parent: sidecarScroller.parent
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: headerDock.bottom
                        anchors.bottom: parent.bottom
                        visible: window.activeRailIndex > 0
                        z: 30
                        color: DesignTokens.panelRaised
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 0
                            Row {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 30
                                spacing: 0
                                Repeater {
                                    model: ["我的", "AI生成", "收藏", "公共库"]
                                    delegate: Button {
                                        width: 84; height: 30
                                        text: modelData
                                        onClicked: window.resourceTabIndex = index
                                        background: Rectangle { color: "transparent"; Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 2; color: index === window.resourceTabIndex ? DesignTokens.accent : "transparent" } }
                                        contentItem: Text { text: modelData; color: index === window.resourceTabIndex ? DesignTokens.accent : DesignTokens.textTertiary; font.pixelSize: window.uiFontSize(9.5); font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    }
                                }
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 36
                                color: DesignTokens.background
                                border.width: 0
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 10
                                    anchors.rightMargin: 10
                                    TextField {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 24
                                        placeholderText: "搜索" + window.activeRailLabel + "名称、类型、用途"
                                        font.pixelSize: window.uiFontSize(10)
                                        color: DesignTokens.textPrimary
                                        placeholderTextColor: DesignTokens.textTertiary
                                        leftPadding: 26
                                        background: Rectangle { color: DesignTokens.input; border.color: DesignTokens.border; radius: 2 }
                                        Text { anchors.left: parent.left; anchors.leftMargin: 8; anchors.verticalCenter: parent.verticalCenter; text: "⌕"; color: DesignTokens.textTertiary; font.pixelSize: 14 }
                                    }
                                    Button {
                                        Layout.preferredWidth: 24
                                        Layout.preferredHeight: 24
                                        background: Rectangle { color: DesignTokens.input; border.color: DesignTokens.border; radius: 2 }
                                        contentItem: Text { text: "≡"; color: DesignTokens.textSecondary; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    }
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 22
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                Text { text: "全部"; color: DesignTokens.accent; font.pixelSize: window.uiFontSize(9.5); font.bold: true }
                                Item { Layout.fillWidth: true }
                                Text { text: window.activeResourceCount() + " 项"; color: DesignTokens.textTertiary; font.pixelSize: window.uiFontSize(9) }
                            }
                            Flickable {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                contentWidth: width
                                contentHeight: mediaCardGrid.height + 16
                                clip: true
                                GridLayout {
                                    id: mediaCardGrid
                                    x: 8; y: 8
                                    width: parent.width - 16
                                    columns: 2
                                    columnSpacing: 6
                                    rowSpacing: 7
                                    Repeater {
                                        model: ["音乐结尾", "Breaking-News", "字幕条", "进度条", "箭头标注", "纯色背景", "渐变背景", "大数字"]
                                        delegate: Rectangle {
                                            property bool hovered: false
                                            property bool selected: false
                                            Layout.fillWidth: true
                                            Layout.preferredWidth: (mediaCardGrid.width - 6) / 2
                                            Layout.preferredHeight: 108
                                            color: DesignTokens.background
                                            border.color: selected ? DesignTokens.accent : (hovered ? DesignTokens.focusBorder : DesignTokens.divider)
                                            radius: 2
                                            Column {
                                                anchors.fill: parent
                                                anchors.margins: 6
                                                spacing: 4
                                                Rectangle { width: parent.width; height: width * 9 / 16; color: DesignTokens.deepest; border.color: DesignTokens.divider; border.width: 1; Text { anchors.centerIn: parent; text: modelData; color: DesignTokens.textSecondary; font.pixelSize: window.uiFontSize(9) } }
                                                Text { width: parent.width; height: 17; text: modelData; color: DesignTokens.textSecondary; font.pixelSize: window.uiFontSize(10); elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                                            }
                                            MouseArea {
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                onEntered: parent.hovered = true
                                                onExited: parent.hovered = false
                                                onClicked: {
                                                    parent.selected = true
                                                    if (workbenchRuntime.resolveConnected)
                                                        workbenchRuntime.insertLibraryComponentAtPlayhead(modelData)
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                    Rectangle {
                        id: sidecarFooter
                    // 原始 SVG 资源合同：qrc:/qml/icons/plus.svg、qrc:/qml/icons/microphone.svg、qrc:/qml/icons/send.svg。
                    // 可见变体仅将 currentColor 固定为设计稿颜色，几何路径保持一致。
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 40
                    visible: window.activeRailIndex === 0
                    z: 25
                    color: DesignTokens.background
                    border.width: 0
                    Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; height: 1; color: DesignTokens.divider }
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 4
                        Rectangle {
                            objectName: "aiSendButtonHitTarget"
                            Layout.preferredWidth: 26; Layout.preferredHeight: 26
                            color: DesignTokens.input; border.color: DesignTokens.border; radius: 4
                            Image { width: 14; height: 14; anchors.centerIn: parent; source: "qrc:/qml/icons/plus-visible.svg"; sourceSize.width: 14; sourceSize.height: 14; fillMode: Image.PreserveAspectFit; asynchronous: false }
                            MouseArea { anchors.fill: parent; onClicked: { var urls = workbenchRuntime.chooseAiAttachments(); for (var i = 0; i < urls.length; ++i) window.addAiAttachment(urls[i]) } }
                        }
                        Item { Layout.fillWidth: true }
                        ComboBox {
                            id: aiFooterModelSelector
                            Layout.preferredWidth: 135; Layout.preferredHeight: 26
                            model: workbenchRuntime.aiAvailableModels.length > 0
                                   ? workbenchRuntime.aiAvailableModels
                                   : ["GPT-4o", "Claude", "Gemini", "通义千问", "豆包"]
                            currentIndex: Math.max(0, model.indexOf(workbenchRuntime.aiModelId))
                            font.pixelSize: window.uiFontSize(9.5)
                            background: Rectangle { color: DesignTokens.input; border.color: DesignTokens.border; radius: 4 }
                            contentItem: Text { leftPadding: 6; rightPadding: 16; text: parent.currentText; color: DesignTokens.textSecondary; font.pixelSize: window.uiFontSize(9.5); verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                            indicator: Rectangle {
                                x: parent.width - 16; y: 0; width: 16; height: parent.height
                                color: "transparent"
                                Text { anchors.centerIn: parent; text: "⌄"; color: DesignTokens.textSecondary; font.pixelSize: 12 }
                            }
                            popup: Popup {
                                y: aiFooterModelSelector.height
                                width: aiFooterModelSelector.width
                                height: Math.min(window.aiModelEntries().length * 25 + 2, 164)
                                padding: 1
                                closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                                background: Rectangle {
                                    color: DesignTokens.input
                                    border.color: DesignTokens.border
                                    radius: 4
                                }
                                contentItem: ListView {
                                    clip: true
                                    implicitHeight: contentHeight
                                    model: window.aiModelEntries()
                                    currentIndex: aiFooterModelSelector.highlightedIndex
                                    delegate: ItemDelegate {
                                        width: aiFooterModelSelector.popup.width - 2
                                        height: modelData.kind === "provider" ? 23 : 25
                                        highlighted: modelData.kind === "model" && aiFooterModelSelector.currentText === modelData.label
                                        background: Rectangle { color: parent.highlighted ? DesignTokens.hover : "transparent" }
                                        contentItem: Text {
                                            text: modelData.label
                                            color: modelData.kind === "provider" ? DesignTokens.accent : DesignTokens.textSecondary
                                            font.pixelSize: window.uiFontSize(modelData.kind === "provider" ? 8.5 : 9.5)
                                            font.bold: modelData.kind === "provider"
                                            verticalAlignment: Text.AlignVCenter
                                            leftPadding: modelData.kind === "provider" ? 6 : 12
                                        }
                                        onClicked: {
                                            if (modelData.kind !== "model") return
                                            aiFooterModelSelector.currentIndex = aiFooterModelSelector.model.indexOf(modelData.label)
                                            aiFooterModelSelector.popup.close()
                                            workbenchRuntime.selectAiModel(modelData.label)
                                        }
                                    }
                                }
                            }
                            onActivated: workbenchRuntime.selectAiModel(currentText)
                        }
                        Rectangle { Layout.preferredWidth: 7; Layout.preferredHeight: 7; radius: 4; color: DesignTokens.success }
                        Button {
                            Layout.preferredWidth: 26
                            Layout.preferredHeight: 26
                            text: ""
                            background: Rectangle { color: "transparent" }
                            contentItem: Image { anchors.centerIn: parent; width: 24; height: 24; source: "qrc:/qml/icons/microphone-visible.svg"; sourceSize.width: 24; sourceSize.height: 24; fillMode: Image.PreserveAspectFit; asynchronous: false; smooth: true }
                        }
                        Rectangle {
                            Layout.preferredWidth: 26; Layout.preferredHeight: 26
                            color: DesignTokens.accent; radius: 4
                            objectName: "aiSendButton"
                            z: 10
                            Image { width: 13; height: 13; anchors.centerIn: parent; rotation: -45; source: "qrc:/qml/icons/send-visible.svg"; sourceSize.width: 13; sourceSize.height: 13; fillMode: Image.PreserveAspectFit; asynchronous: false }
                            MouseArea { anchors.fill: parent; acceptedButtons: Qt.LeftButton; onClicked: window.sendAiDockMessage() }
                            TapHandler { onTapped: window.sendAiDockMessage() }
                        }
                    }
                    MouseArea {
                        objectName: "aiSendButtonOverlay"
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.rightMargin: 10
                        anchors.bottomMargin: 7
                        width: 26
                        height: 26
                        z: 100
                        enabled: window.activeRailIndex === 0
                        acceptedButtons: Qt.LeftButton
                        onClicked: window.sendAiDockMessage()
                    }
                }
                Rectangle {
                    id: railPageOverlay
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: headerDock.bottom
                    anchors.bottom: parent.bottom
                    visible: window.activeRailIndex > 0
                    z: 20
                    color: DesignTokens.panel
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 8
                        Label {
                            Layout.fillWidth: true
                            text: window.activeRailLabel + "资源库"
                            color: DesignTokens.accent
                            font.pixelSize: window.uiFontSize(14)
                            horizontalAlignment: Text.AlignHCenter
                        }
                        Label {
                            Layout.fillWidth: true
                            text: (window.activeRailIndex >= 1 && window.activeRailIndex <= 3)
                                  ? "来自当前 Resolve 时间线的片段"
                                  : "从 Edward 资源库选择后，再通过 Resolve 执行"
                            color: DesignTokens.textSecondary
                            font.pixelSize: window.uiFontSize(10)
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.Wrap
                        }
                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            visible: window.activeRailIndex >= 1 && window.activeRailIndex <= 3
                            clip: true
                            model: window.activeTimelineClips()
                            delegate: Rectangle {
                                required property var modelData
                                width: ListView.view.width
                                height: 42
                                color: modelData.selected ? DesignTokens.accentSoft : "transparent"
                                border.color: DesignTokens.border
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 6
                                    Label {
                                        Layout.fillWidth: true
                                        text: modelData.name || "未命名片段"
                                        color: DesignTokens.textPrimary
                                        elide: Text.ElideRight
                                    }
                                    Button {
                                        Layout.preferredWidth: 52
                                        text: "选中"
                                        onClicked: workbenchRuntime.selectClip(modelData.id)
                                    }
                                }
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            visible: window.activeRailIndex >= 4 && workbenchRuntime.localComponents.length === 0
                            text: workbenchRuntime.localComponents.length === 0
                                  ? "该分类暂无本地组件，请先在首页保存组件。" : "本地组件库"
                            color: DesignTokens.textSecondary
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            wrapMode: Text.Wrap
                        }
                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            visible: window.activeRailIndex >= 4
                            clip: true
                            model: workbenchRuntime.localComponents
                            delegate: Rectangle {
                                required property var modelData
                                width: ListView.view.width
                                height: 44
                                color: DesignTokens.background
                                border.color: DesignTokens.border
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 6
                                    Label {
                                        Layout.fillWidth: true
                                        text: modelData.displayName || modelData.resourceId
                                        color: DesignTokens.textPrimary
                                        elide: Text.ElideRight
                                    }
                                    Button {
                                        Layout.preferredWidth: 48
                                        text: "载入"
                                        onClicked: workbenchRuntime.loadLibraryComponent(modelData.resourceId)
                                    }
                                    Button {
                                        Layout.preferredWidth: 48
                                        text: "添加"
                                        enabled: workbenchRuntime.resolveConnected && !workbenchRuntime.resolveComponentImportBusy
                                        onClicked: workbenchRuntime.insertLibraryComponentAtPlayhead(modelData.resourceId)
                                    }
                                }
                            }
                        }
                    }
                }
                Rectangle {
                    id: utilityPageOverlay
                    anchors.fill: parent
                    visible: window.activeRailIndex < 0
                    z: 30
                    color: DesignTokens.panel
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 10
                        Label {
                            Layout.fillWidth: true
                            text: window.activeUtility === "settings" ? "设置" : "AI 一键分析"
                            color: DesignTokens.accent
                            font.pixelSize: window.uiFontSize(14)
                            horizontalAlignment: Text.AlignHCenter
                        }
                        Label {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            text: window.activeUtility === "settings"
                                  ? "Resolve 连接、账户、存储和插件设置请从首页对应入口打开。"
                                  : "AI 一键分析入口已保留在首页上方；分析结果将在首页下方 AI 对话区显示。"
                            color: DesignTokens.textSecondary
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            wrapMode: Text.Wrap
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            visible: window.activeUtility === "settings"
                            spacing: 6
                            Button {
                                Layout.fillWidth: true
                                text: "账户"
                                onClicked: workbenchRuntime.authenticated ? workbenchRuntime.signOut() : signInDialog.open()
                            }
                            Button {
                                Layout.fillWidth: true
                                text: "存储"
                                onClicked: previewStorageDialog.open()
                            }
                            Button {
                                Layout.fillWidth: true
                                text: "布局"
                                enabled: workbenchRuntime.resolveConnected
                                onClicked: resolveLayoutFileDialog.open()
                            }
                            Button {
                                Layout.fillWidth: true
                                text: workbenchRuntime.aiModelConfigured ? "大模型已配置" : "大模型"
                                onClicked: aiSettingsDialog.open()
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            visible: window.activeUtility === "ai"
                            spacing: 6
                            Button {
                                Layout.fillWidth: true
                                text: "生成组件草稿"
                                onClicked: workbenchRuntime.generateComponentDraft()
                            }
                            Button {
                                Layout.fillWidth: true
                                text: workbenchRuntime.aiRequestBusy ? "分析中" : "分析当前片段"
                                enabled: !workbenchRuntime.aiRequestBusy
                                onClicked: workbenchRuntime.analyzeCurrentClipWithAi()
                            }
                            Button {
                                Layout.fillWidth: true
                                text: "返回首页"
                                onClicked: { window.activeRailIndex = 0; window.activeUtility = "" }
                            }
                        }
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
        onAccepted: {
            const path = selectedFile.toLocalFile()
            if (workbenchRuntime.hasProjectRecovery(path)) {
                window.recoveryProjectPath = path
                projectRecoveryDialog.open()
            } else {
                workbenchRuntime.loadProject(path)
            }
        }
    }

    Dialog {
        id: projectRecoveryDialog
        anchors.centerIn: Overlay.overlay
        width: 420
        modal: true
        title: "发现自动保存副本"
        standardButtons: Dialog.NoButton
        contentItem: Label {
            text: "此工程存在比正式文件更新的自动保存副本。恢复不会覆盖正式工程，直到你再次保存。"
            color: DesignTokens.textSecondary
            wrapMode: Text.Wrap
            padding: 16
        }
        footer: RowLayout {
            spacing: 8
            Button {
                Layout.fillWidth: true
                text: "丢弃副本并打开正式工程"
                onClicked: {
                    workbenchRuntime.discardProjectRecovery(window.recoveryProjectPath)
                    workbenchRuntime.loadProject(window.recoveryProjectPath)
                    projectRecoveryDialog.close()
                }
            }
            Button {
                Layout.fillWidth: true
                text: "恢复自动保存"
                onClicked: {
                    if (workbenchRuntime.recoverProject(window.recoveryProjectPath))
                        projectRecoveryDialog.close()
                }
            }
        }
    }

    Dialog {
        id: timelineExportPanel
        anchors.centerIn: Overlay.overlay
        width: 620
        modal: true
        title: "导出"
        closePolicy: Popup.CloseOnEscape

        contentItem: ColumnLayout {
            spacing: 14
            Label {
                Layout.fillWidth: true
                text: "将当前时间线合成为 MP4 视频"
                color: DesignTokens.textSecondary
                font.pixelSize: window.uiFontSize(12)
            }
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 14
                rowSpacing: 12

                Label { text: "文件名"; color: DesignTokens.textPrimary; font.pixelSize: window.uiFontSize(13) }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 5
                    TextField {
                        id: exportFileNameField
                        Layout.fillWidth: true
                        text: window.exportFileName
                        placeholderText: "未命名项目.mp4"
                    }
                    Label { text: "默认使用当前项目名称"; color: DesignTokens.textSecondary; font.pixelSize: window.uiFontSize(11) }
                }

                Label { text: "分辨率"; color: DesignTokens.textPrimary; font.pixelSize: window.uiFontSize(13) }
                RowLayout {
                    ComboBox {
                        id: exportResolution
                        model: workbenchRuntime.resolveRenderResolutions.length > 0
                               ? workbenchRuntime.resolveRenderResolutions
                               : [{label: "1080p · 1920 × 1080", width: 1920, height: 1080},
                                  {label: "720p · 1280 × 720", width: 1280, height: 720},
                                  {label: "540p · 960 × 540", width: 960, height: 540}]
                        textRole: "label"
                        currentIndex: 0
                    }
                    Label { text: "横屏项目"; color: DesignTokens.textSecondary; font.pixelSize: window.uiFontSize(11) }
                }

                Label { text: "帧率"; color: DesignTokens.textPrimary; font.pixelSize: window.uiFontSize(13) }
                ComboBox { id: exportFps; model: ["25 fps", "30 fps"]; currentIndex: 0 }

                Label { text: "格式"; color: DesignTokens.textPrimary; font.pixelSize: window.uiFontSize(13) }
                ComboBox {
                    id: exportFormat
                    model: workbenchRuntime.resolveRenderFormats.length > 0
                           ? workbenchRuntime.resolveRenderFormats
                           : [{label: "MP4（H.264）", extension: "mp4"}]
                    textRole: "label"
                    currentIndex: 0
                }

                Label { text: "质量"; color: DesignTokens.textPrimary; font.pixelSize: window.uiFontSize(13) }
                ComboBox { id: exportQuality; model: ["高质量", "标准", "较小文件"]; currentIndex: 0 }

                Label { text: "导出位置"; color: DesignTokens.textPrimary; font.pixelSize: window.uiFontSize(13) }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 5
                    Button { text: "选择文件夹"; onClicked: exportDirectoryDialog.open() }
                    Label {
                        Layout.fillWidth: true
                        text: window.exportOutputDirectory === "" ? "尚未选择导出位置" : window.exportOutputDirectory
                        color: DesignTokens.textSecondary
                        elide: Text.ElideMiddle
                        font.pixelSize: window.uiFontSize(11)
                    }
                }
            }
            Rectangle { Layout.fillWidth: true; height: 1; color: DesignTokens.divider }
            Rectangle {
                Layout.fillWidth: true
                height: 5
                radius: 3
                color: DesignTokens.panelRaised
                Rectangle {
                    width: parent.width * workbenchRuntime.timelineExportProgress / 100
                    height: parent.height
                    radius: parent.radius
                    color: DesignTokens.accent
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Label {
                    Layout.fillWidth: true
                    text: workbenchRuntime.timelineExportBusy
                          ? "正在导出：" + workbenchRuntime.timelineExportProgress + "%"
                          : workbenchRuntime.timelineExportProgress === 100 ? "导出完成" : "等待导出"
                    color: DesignTokens.textSecondary
                    font.pixelSize: window.uiFontSize(11)
                }
                Label {
                    visible: workbenchRuntime.timelineExportBusy
                    text: "导出中"
                    color: DesignTokens.accent
                    font.pixelSize: window.uiFontSize(11)
                }
            }
        }
        footer: RowLayout {
            spacing: 8
            Button {
                Layout.fillWidth: true
                text: "取消"
                onClicked: {
                    if (workbenchRuntime.timelineExportBusy)
                        workbenchRuntime.cancelTimelineExport()
                    else
                        timelineExportPanel.close()
                }
            }
            Button {
                Layout.fillWidth: true
                text: workbenchRuntime.timelineExportBusy ? "导出中" : "导出"
                enabled: !workbenchRuntime.timelineExportBusy
                         && window.exportOutputDirectory !== "" && exportFileNameField.text.trim() !== ""
                onClicked: {
                    var fileName = exportFileNameField.text.trim()
                    var formatItem = exportFormat.currentValue
                    var extension = formatItem && formatItem.extension ? formatItem.extension : "mp4"
                    var suffix = "." + extension
                    if (!fileName.toLowerCase().endsWith(suffix.toLowerCase()))
                        fileName += suffix
                    var resolutionItem = exportResolution.currentValue
                    var width = resolutionItem && resolutionItem.width ? resolutionItem.width : 1920
                    var height = resolutionItem && resolutionItem.height ? resolutionItem.height : 1080
                    var frameRates = [25, 30]
                    if (workbenchRuntime.exportTimelineWithOptions(window.exportOutputDirectory + "/" + fileName,
                                                                   width,
                                                                   height,
                                                                   frameRates[exportFps.currentIndex],
                                                                   exportQuality.currentIndex)) {
                        window.exportFileName = fileName
                    }
                }
            }
        }
    }

    FolderDialog {
        id: exportDirectoryDialog
        title: "选择视频导出位置"
        onAccepted: window.exportOutputDirectory = selectedFolder.toLocalFile()
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

    Dialog {
        id: aiSettingsDialog
        anchors.centerIn: Overlay.overlay
        width: 340
        height: 520
        modal: true
        title: ""
        padding: 0
        background: Rectangle {
            color: DesignTokens.panel
            border.color: DesignTokens.border
            border.width: 1
        }
        header: Rectangle {
            width: parent.width
            height: 42
            color: DesignTokens.background
            Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: DesignTokens.divider }
            Text {
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                text: "大模型接入设置"
                color: DesignTokens.textPrimary
                font.pixelSize: window.uiFontSize(13)
                font.bold: true
            }
        }
        footer: RowLayout {
            width: parent.width
            height: 22
            spacing: 4
            anchors.margins: 14
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: DesignTokens.input
                border.color: DesignTokens.border
                radius: 3
                Text { anchors.centerIn: parent; text: "取消"; color: DesignTokens.textPrimary; font.pixelSize: window.uiFontSize(9.5) }
                MouseArea { anchors.fill: parent; onClicked: aiSettingsDialog.reject() }
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: DesignTokens.accent
                radius: 3
                Text { anchors.centerIn: parent; text: "保存"; color: "#ffffff"; font.pixelSize: window.uiFontSize(9.5); font.bold: true }
                MouseArea { anchors.fill: parent; onClicked: aiSettingsDialog.accept() }
            }
        }
        contentItem: Column {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 14
            spacing: 8
            Text { text: "供应商配置 · 从预设选择供应商"; color: DesignTokens.accent; font.bold: true; font.pixelSize: window.uiFontSize(10) }
            ComboBox {
                id: aiProviderPreset
                objectName: "aiProviderPreset"
                width: parent.width
                model: ["自定义供应商", "OpenAI", "DeepSeek", "通义千问（兼容 OpenAI）", "智谱 GLM（兼容 OpenAI）"]
                onActivated: {
                    var names = ["", "OpenAI", "DeepSeek", "通义千问", "智谱 GLM"]
                    var endpoints = ["", "https://api.openai.com/v1/chat/completions", "https://api.deepseek.com/chat/completions", "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions", "https://open.bigmodel.cn/api/paas/v4/chat/completions"]
                    var models = ["", "gpt-4o", "deepseek-chat", "qwen-plus", "glm-4-flash"]
                    if (currentIndex > 0) { aiProviderName.text = names[currentIndex]; aiSettingsEndpoint.text = endpoints[currentIndex]; aiSettingsModel.text = models[currentIndex]; aiSettingsTestModel.text = models[currentIndex] }
                }
                background: Rectangle { color: DesignTokens.input; border.color: DesignTokens.border; radius: 3 }
                contentItem: Text { leftPadding: 8; text: parent.currentText; color: DesignTokens.textPrimary; font.pixelSize: window.uiFontSize(10); verticalAlignment: Text.AlignVCenter }
                indicator: Text { x: parent.width - 18; anchors.verticalCenter: parent.verticalCenter; text: "⌄"; color: DesignTokens.textSecondary; font.pixelSize: 14 }
            }
            TextField { id: aiProviderName; objectName: "aiProviderName"; text: workbenchRuntime.aiProviderName; placeholderText: "供应商名称"; color: DesignTokens.textPrimary; placeholderTextColor: DesignTokens.textTertiary; background: Rectangle { color: DesignTokens.input; border.color: DesignTokens.border; radius: 3 } }
            Text { text: "AI 对话和组件草案共用此模型配置"; color: DesignTokens.textSecondary; font.pixelSize: window.uiFontSize(9) }
            Text { text: "API 模式：Chat Completions"; color: DesignTokens.textSecondary; font.pixelSize: window.uiFontSize(9) }
            TextField { id: aiSettingsEndpoint; text: workbenchRuntime.aiModelEndpoint; placeholderText: "模型 HTTPS 端点"; color: DesignTokens.textPrimary; placeholderTextColor: DesignTokens.textTertiary; background: Rectangle { color: DesignTokens.input; border.color: DesignTokens.border; radius: 3 } }
            TextField { id: aiSettingsApiKey; placeholderText: "API Key（留空保持现有 Key）"; echoMode: TextInput.Password; color: DesignTokens.textPrimary; placeholderTextColor: DesignTokens.textTertiary; background: Rectangle { color: DesignTokens.input; border.color: DesignTokens.border; radius: 3 } }
            TextField { id: aiSettingsModel; text: workbenchRuntime.aiModelId; placeholderText: "模型 ID，例如 deepseek-chat"; color: DesignTokens.textPrimary; placeholderTextColor: DesignTokens.textTertiary; background: Rectangle { color: DesignTokens.input; border.color: DesignTokens.border; radius: 3 } }
            RowLayout {
                width: parent.width
                ComboBox {
                    id: aiModelSelector
                    Layout.fillWidth: true
                    model: workbenchRuntime.aiAvailableModels
                    enabled: model.length > 0
                    currentIndex: Math.max(0, model.indexOf(workbenchRuntime.aiModelId))
                    displayText: model.length > 0 ? currentText : "先保存后获取模型列表"
                    onActivated: aiSettingsModel.text = currentText
                    background: Rectangle { color: DesignTokens.input; border.color: DesignTokens.border; radius: 3 }
                    contentItem: Text { leftPadding: 8; text: parent.displayText; color: DesignTokens.textSecondary; font.pixelSize: window.uiFontSize(9); verticalAlignment: Text.AlignVCenter }
                    indicator: Text { x: parent.width - 18; anchors.verticalCenter: parent.verticalCenter; text: "⌄"; color: DesignTokens.textSecondary; font.pixelSize: 14 }
                }
                Rectangle {
                    Layout.preferredWidth: 96
                    Layout.preferredHeight: 26
                    color: aiModelListMouse.containsMouse ? DesignTokens.hover : DesignTokens.input
                    border.color: aiModelListMouse.containsMouse ? DesignTokens.focusBorder : DesignTokens.border
                    radius: 4
                    opacity: workbenchRuntime.aiModelCredentialsConfigured ? 1 : 0.7
                    Text { anchors.centerIn: parent; text: workbenchRuntime.aiModelListBusy ? "获取中…" : "获取模型列表"; color: DesignTokens.textSecondary; font.pixelSize: window.uiFontSize(9) }
                    MouseArea {
                        id: aiModelListMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        enabled: !workbenchRuntime.aiModelListBusy
                        onClicked: workbenchRuntime.refreshAiModelList()
                    }
                }
            }
            TextField { id: aiSettingsTestModel; text: workbenchRuntime.aiTestModel; placeholderText: "测试模型（可选）"; color: DesignTokens.textPrimary; placeholderTextColor: DesignTokens.textTertiary; background: Rectangle { color: DesignTokens.input; border.color: DesignTokens.border; radius: 3 } }
            TextField { id: aiSettingsModelList; text: workbenchRuntime.aiModelList; placeholderText: "模型列表（每行一个，可选）"; color: DesignTokens.textPrimary; placeholderTextColor: DesignTokens.textTertiary; background: Rectangle { color: DesignTokens.input; border.color: DesignTokens.border; radius: 3 } }
            TextField { id: aiSettingsContextWindow; text: workbenchRuntime.aiContextWindow; placeholderText: "上下文窗口 Token 数（可选）"; color: DesignTokens.textPrimary; placeholderTextColor: DesignTokens.textTertiary; background: Rectangle { color: DesignTokens.input; border.color: DesignTokens.border; radius: 3 } }
        }
        onOpened: { aiProviderName.text = workbenchRuntime.aiProviderName; aiSettingsEndpoint.text = workbenchRuntime.aiModelEndpoint; aiSettingsModel.text = workbenchRuntime.aiModelId; aiSettingsTestModel.text = workbenchRuntime.aiTestModel; aiSettingsModelList.text = workbenchRuntime.aiModelList; aiSettingsContextWindow.text = workbenchRuntime.aiContextWindow }
        onAccepted: workbenchRuntime.configureAiModel(aiProviderName.text, aiSettingsEndpoint.text, aiSettingsApiKey.text, aiSettingsModel.text, aiSettingsTestModel.text, aiSettingsModelList.text, aiSettingsContextWindow.text)
    }

    FileDialog {
        id: aiAttachmentDialog
        title: "添加图片、视频或链接"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["媒体文件 (*.png *.jpg *.jpeg *.webp *.gif *.mp4 *.mov *.mkv *.mp3 *.wav *.m4a)", "文本文件 (*.txt *.md *.json *.csv)", "所有文件 (*)"]
        onAccepted: {
            for (var i = 0; i < selectedFiles.length; ++i) window.addAiAttachment(selectedFiles[i].toString())
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

    Dialog {
        id: previewStorageDialog
        anchors.centerIn: Overlay.overlay
        width: 560
        modal: true
        title: "预览与存储设置"
        standardButtons: Dialog.Cancel | Dialog.Save
        contentItem: ColumnLayout {
            spacing: 12
            Label {
                Layout.fillWidth: true
                text: "设置只影响后续代理、缓存和预渲染任务；已有派生文件不会自动迁移。"
                wrapMode: Text.WordWrap
                color: DesignTokens.textSecondary
                font.pixelSize: window.uiFontSize(12)
            }
            Label { text: "代理位置"; color: DesignTokens.textPrimary }
            TextField { id: proxyRootField; Layout.fillWidth: true }
            Label { text: "缓存位置"; color: DesignTokens.textPrimary }
            TextField { id: cacheRootField; Layout.fillWidth: true }
            Label { text: "预渲染位置"; color: DesignTokens.textPrimary }
            TextField { id: renderRootField; Layout.fillWidth: true }
            CheckBox {
                Layout.fillWidth: true
                text: "允许发送匿名质量改进数据"
                checked: workbenchRuntime.qualityImprovementEnabled
                onToggled: workbenchRuntime.setQualityImprovementEnabled(checked)
            }
            Label {
                Layout.fillWidth: true
                text: "仅记录能力 ID、版本、平台和脱敏错误码；不包含项目、素材、路径、提示词或密钥。关闭后仍保留本地诊断日志。"
                wrapMode: Text.WordWrap
                color: DesignTokens.textSecondary
                font.pixelSize: window.uiFontSize(12)
            }
            RowLayout {
                Layout.fillWidth: true
                Label { text: "清理仅删除当前工程的派生文件"; color: DesignTokens.textSecondary; font.pixelSize: window.uiFontSize(12) }
                Item { Layout.fillWidth: true }
                Button { text: "清理代理"; onClicked: workbenchRuntime.clearDerivedStorage(0) }
                Button { text: "清理缓存"; onClicked: workbenchRuntime.clearDerivedStorage(1) }
                Button { text: "清理预渲染"; onClicked: workbenchRuntime.clearDerivedStorage(2) }
            }
        }
        onAccepted: {
            if (!workbenchRuntime.configurePreviewStorageRoots(proxyRootField.text, cacheRootField.text,
                                                                renderRootField.text)) {
                failureToast.text = "存储位置无效或当前代理任务尚未完成";
                failureToast.open();
                failureTimer.restart();
            }
        }
    }

    Dialog {
        id: qualityImprovementNoticeDialog
        anchors.centerIn: Overlay.overlay
        width: 330
        modal: true
        closePolicy: Popup.NoAutoClose
        title: "质量改进数据"
        contentItem: Label {
            width: parent.width
            text: "Edward 可发送匿名能力成功/失败记录，用于确认 Resolve 功能兼容性。不会包含项目、素材、路径、提示词或密钥；关闭后仍保留本地诊断日志。"
            wrapMode: Text.WordWrap
            color: DesignTokens.textPrimary
            font.pixelSize: window.uiFontSize(12)
        }
        footer: DialogButtonBox {
            Button {
                text: "不发送"
                onClicked: {
                    workbenchRuntime.setQualityImprovementEnabled(false)
                    workbenchRuntime.acknowledgeQualityImprovementNotice()
                    qualityImprovementNoticeDialog.close()
                }
            }
            Button {
                text: "允许"
                onClicked: {
                    workbenchRuntime.setQualityImprovementEnabled(true)
                    workbenchRuntime.acknowledgeQualityImprovementNotice()
                    qualityImprovementNoticeDialog.close()
                }
            }
        }
    }

    FolderDialog {
        id: componentPackageDirectoryDialog
        title: "选择组件包保存目录"
        onAccepted: workbenchRuntime.saveComponentPackage(
                         selectedFolder.toLocalFile(), packageResourceId.text, packageDisplayName.text)
    }

    FileDialog {
        id: resolveLayoutFileDialog
        title: "选择 Resolve 布局预设"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Resolve Layout (*.drp *.xml *.preset)", "所有文件 (*)"]
        onAccepted: workbenchRuntime.importResolveLayoutPreset(selectedFile.toString().replace("file://", ""), "Edward Sidecar")
    }

    FileDialog {
        id: resolveLayoutSaveDialog
        title: "导出 Resolve 布局预设"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Resolve Layout (*.preset)", "所有文件 (*)"]
        onAccepted: workbenchRuntime.exportResolveLayoutPreset("Edward Sidecar", selectedFile.toString().replace("file://", ""))
    }

    FileDialog {
        id: resolveRebuildFileDialog
        title: "选择 OTIO/EDL 重建文件"
        fileMode: FileDialog.OpenFile
        nameFilters: ["时间线交换文件 (*.otio *.edl)", "所有文件 (*)"]
        onAccepted: workbenchRuntime.importResolveRebuildFile(selectedFile.toLocalFile())
    }

    FileDialog {
        id: componentFileDialog
        title: "打开组件 JSON"
        fileMode: FileDialog.OpenFile
        nameFilters: ["组件 JSON (*.json)", "所有文件 (*)"]
        onAccepted: workbenchRuntime.loadComponentFile(selectedFile.toLocalFile())
    }

    Dialog {
        id: rebuildConfirmDialog
        anchors.centerIn: Overlay.overlay
        modal: true
        standardButtons: Dialog.Cancel | Dialog.Ok
        property string action: ""
        contentItem: Label {
            text: rebuildConfirmDialog.action === "commit"
                  ? "请确认已在 Resolve 原生预览中检查重建结果。提交后将结束当前事务。"
                  : "确认回滚到重建前的原时间线？当前隔离重建结果将不再作为本次事务结果。"
            color: DesignTokens.textPrimary
            wrapMode: Text.Wrap
            width: 360
            padding: 16
        }
        onAccepted: {
            if (action === "commit") workbenchRuntime.commitResolveRebuild()
            else if (action === "rollback") workbenchRuntime.rollbackResolveRebuild()
            action = ""
        }
        onRejected: action = ""
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
            workbenchRuntime.appendAiConversationError(message)
        }
        function onOperationSucceeded(message) {
            workbenchRuntime.appendAiConversationError(message)
        }
    }

    Popup {
        id: failureToast
        modal: false
        closePolicy: Popup.NoAutoClose
        anchors.centerIn: Overlay.overlay
        property alias text: failureLabel.text
        width: Math.min(parent ? parent.width - 32 : 320, Math.max(220, failureLabel.implicitWidth + 32))
        height: failureLabel.implicitHeight + 28
        background: Rectangle {
            color: DesignTokens.input
            border.color: DesignTokens.error
            border.width: 1
            radius: 3
        }
        contentItem: Text {
            id: failureLabel
            anchors.fill: parent
            anchors.margins: 14
            color: DesignTokens.textPrimary
            font.pixelSize: window.uiFontSize(10)
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        Timer { id: failureTimer; interval: 5000; repeat: false; onTriggered: failureToast.close() }
    }

    Popup {
        id: successToast
        modal: false
        closePolicy: Popup.NoAutoClose
        anchors.centerIn: Overlay.overlay
        property alias text: successLabel.text
        width: Math.min(parent ? parent.width - 32 : 320, Math.max(220, successLabel.implicitWidth + 32))
        height: successLabel.implicitHeight + 28
        background: Rectangle {
            color: DesignTokens.input
            border.color: DesignTokens.success
            border.width: 1
            radius: 3
        }
        contentItem: Text {
            id: successLabel
            anchors.fill: parent
            anchors.margins: 14
            color: DesignTokens.textPrimary
            font.pixelSize: window.uiFontSize(10)
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        Timer { id: successTimer; interval: 5000; repeat: false; onTriggered: successToast.close() }
    }

    WebEngineView {
        id: fablecutView
        anchors.fill: parent
        visible: window.fablecutEmbedded
        z: 1000
        url: "http://127.0.0.1:7777/"
        settings.javascriptEnabled: true
        settings.localStorageEnabled: true
    }
}
