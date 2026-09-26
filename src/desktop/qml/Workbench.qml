import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtWebEngine
import QtWebChannel

ApplicationWindow {
    id: window
    visible: true
    width: 1280
    height: 800
    color: "#161719"
    title: "Orbit"
    property string exportOutputDirectory: ""
    property string exportFileName: "未命名项目.mp4"
    property var pendingExportRequest: null
    property var exportResolutionOptions: [
        {label: "4K · 3840 × 2160", width: 3840, height: 2160},
        {label: "1080p · 1920 × 1080", width: 1920, height: 1080},
        {label: "720p · 1280 × 720", width: 1280, height: 720},
        {label: "540p · 960 × 540", width: 960, height: 540}
    ]
    property var exportFpsOptions: ["24 fps", "25 fps", "30 fps", "50 fps", "60 fps"]
    property var exportFpsValues: [24, 25, 30, 50, 60]

    function localPathFromDialogUrl(value) {
        if (value === undefined || value === null) return ""
        var raw = value.toString()
        if (raw.indexOf("file://") === 0) raw = raw.slice(7)
        try { return decodeURIComponent(raw) } catch (error) { return raw }
    }

    function applyTimelineExportDefaults(result) {
        try {
            var spec = JSON.parse(result || "{}")
            var width = Number(spec.width), height = Number(spec.height), fps = Number(spec.fps)
            if (!(width > 0 && height > 0)) return
            var options = exportResolutionOptions.slice(0), index = -1
            for (var i = 0; i < options.length; ++i) {
                if (options[i].width === width && options[i].height === height) { index = i; break }
            }
            if (index < 0) {
                options.unshift({label: "时间线 · " + width + " × " + height, width: width, height: height})
                exportResolutionOptions = options
            }
            var fpsIndex = exportFpsValues.indexOf(fps)
            if (fpsIndex < 0 && fps > 0) {
                exportFpsValues = [fps].concat(exportFpsValues)
                exportFpsOptions = [fps + " fps"].concat(exportFpsOptions)
            }
            if (spec.name) exportFileName = String(spec.name).replace(/\.[^/.]+$/, "") + ".mp4"
        } catch (error) { }
    }

    FileDialog {
        id: packageDialog
        fileMode: FileDialog.OpenFile
        nameFilters: ["Orbit Runtime (edward-runtime.json)"]
        onAccepted: workbenchRuntime.addNativeRuntimePackage(selectedFile.toString().replace("file://", "").replace(/\/edward-runtime\.json$/, ""))
    }

    WebChannel {
        id: preferenceWebChannel
        Component.onCompleted: registerObjects({
            preferenceStore: preferenceStore,
            workbenchRuntime: workbenchRuntime
        })
    }

    function handleNativeTitlebarAction(action) {
        if (workbenchRuntime.nativeRuntimeSelected)
            return
        if (action === "login") {
            fablecutView.runJavaScript("document.getElementById('btnAuth')?.click()")
            return
        }
        if (action === "layout") {
            fablecutView.runJavaScript("document.getElementById('btnLayoutReset')?.click()")
            return
        }
        if (action === "settings") {
            fablecutView.runJavaScript("document.getElementById('btnSettings')?.click()")
            return
        }
        if (action === "export") {
            timelineExportPanel.open()
            return
        }
        if (action === "S" || action === "M" || action === "L") {
            fablecutView.runJavaScript("document.querySelector('[data-track-size=\"" + action.toLowerCase() + "\"]')?.click()")
            return
        }
        if (action === "language-en" || action === "language-zh") {
            const language = action === "language-en" ? "en-US" : "zh-CN"
            fablecutView.runJavaScript("(function(){const select=document.getElementById('languageSel');if(!select)return;select.value='" + language + "';select.dispatchEvent(new Event('change',{bubbles:true}));})()")
        }
    }

    Dialog {
        id: timelineExportPanel
        anchors.centerIn: Overlay.overlay
        width: 560
        modal: true
        title: ""
        closePolicy: Popup.NoAutoClose
        padding: 16
        bottomPadding: 30
        background: Rectangle {
            color: "#1b1b1d"
            radius: 18
            border.color: "#3a3a3e"
            border.width: 1
        }
        onOpened: {
            fablecutView.runJavaScript(
                "typeof project !== 'undefined' ? JSON.stringify({name: project.name, width: project.width, height: project.height, fps: project.fps}) : ''",
                function(result) {
                    window.applyTimelineExportDefaults(result)
                    if (window.exportOutputDirectory === "") window.exportOutputDirectory = workbenchRuntime.fablecutSettings().exportDirectory || ""
                })
        }
        contentItem: ColumnLayout {
            Rectangle { Layout.fillWidth: true; height: 30; color: "transparent"; Label { anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; text: "导出"; color: "#f5f5f7"; font.pixelSize: 18; font.bold: true } }
            spacing: 12
            Label { Layout.fillWidth: true; text: "将当前时间线合成为视频文件"; color: DesignTokens.textSecondary; font.pixelSize: 12 }
            GridLayout {
                Layout.fillWidth: true; columns: 2; columnSpacing: 14; rowSpacing: 10
                Label { text: "文件名"; color: DesignTokens.textPrimary; font.pixelSize: 13 }
                TextField { id: exportFileNameField; Layout.fillWidth: true; implicitHeight: 34; text: window.exportFileName; placeholderText: "未命名项目.mp4"; color: "#f5f5f7"; font.pixelSize: 13; background: Rectangle { color: "#242426"; radius: 8; border.color: "#3b3b40" } }
                Label { text: "分辨率"; color: DesignTokens.textPrimary; font.pixelSize: 13 }
                ComboBox {
                    id: exportResolution; Layout.fillWidth: true; implicitHeight: 34; model: window.exportResolutionOptions; textRole: "label"; currentIndex: 1
                    contentItem: Text { leftPadding: 12; text: exportResolution.displayText; color: "#f5f5f7"; font.pixelSize: 13; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: "#242426"; radius: 8; border.color: "#3b3b40" }
                }
                Label { text: "帧率"; color: DesignTokens.textPrimary; font.pixelSize: 13 }
                ComboBox {
                    id: exportFps; Layout.fillWidth: true; implicitHeight: 34; model: window.exportFpsOptions; currentIndex: 2
                    contentItem: Text { leftPadding: 12; text: exportFps.displayText; color: "#f5f5f7"; font.pixelSize: 13; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: "#242426"; radius: 8; border.color: "#3b3b40" }
                }
                Label { text: "格式"; color: DesignTokens.textPrimary; font.pixelSize: 13 }
                ComboBox {
                    id: exportFormat; Layout.fillWidth: true; implicitHeight: 34; model: ["MP4（H.264）", "透明 MOV（ProRes 4444）"]; currentIndex: 0
                    contentItem: Text { leftPadding: 12; text: exportFormat.displayText; color: "#f5f5f7"; font.pixelSize: 13; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: "#242426"; radius: 8; border.color: "#3b3b40" }
                }
                Label { text: "质量"; color: DesignTokens.textPrimary; font.pixelSize: 13 }
                ComboBox {
                    id: exportQuality; Layout.fillWidth: true; implicitHeight: 34; model: ["高质量", "标准", "较小文件"]; currentIndex: 1
                    contentItem: Text { leftPadding: 12; text: exportQuality.displayText; color: "#f5f5f7"; font.pixelSize: 13; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: "#242426"; radius: 8; border.color: "#3b3b40" }
                }
                Label { text: "导出位置"; color: DesignTokens.textPrimary; font.pixelSize: 13 }
                ColumnLayout {
                    Layout.fillWidth: true
                    Button {
                        text: "选择文件夹"; implicitHeight: 30; implicitWidth: 108
                        onClicked: exportDirectoryDialog.open()
                        contentItem: Text { text: "选择文件夹"; color: "#f5f5f7"; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: "#2b2b2e"; radius: 8; border.color: "#44444a" }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: window.exportOutputDirectory === "" ? "尚未选择导出位置" : window.exportOutputDirectory
                        color: DesignTokens.textSecondary
                        elide: Text.ElideMiddle
                    }
                }
            }
            Rectangle { Layout.fillWidth: true; height: 1; color: DesignTokens.divider }
        }
        footer: Item {
            implicitHeight: 44
            RowLayout {
                anchors.fill: parent
                anchors.bottomMargin: 10
                spacing: 8
                Button {
                    Layout.fillWidth: true; implicitHeight: 32; text: "取消"; onClicked: timelineExportPanel.close()
                    contentItem: Text { text: parent.text; color: "#f5f5f7"; font.pixelSize: 13; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: "#2b2b2e"; radius: 9; border.color: "#44444a" }
                }
                Button {
                Layout.fillWidth: true; implicitHeight: 32; text: "开始导出"
                contentItem: Text { text: parent.text; color: "#151515"; font.pixelSize: 13; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: "#ff8a00"; radius: 9 }
                enabled: window.exportOutputDirectory !== "" && exportFileNameField.text.trim() !== ""
                onClicked: {
                    var fileName = exportFileNameField.text.trim()
                    if (!/\.[^/.]+$/.test(fileName)) fileName += ".mp4"
                    var item = exportResolution.currentValue
                    var fps = window.exportFpsValues[exportFps.currentIndex] || 30
                    var qualityProfile = exportFormat.currentIndex === 1 ? "prores4444" : (["hq", "delivery", "draft"][exportQuality.currentIndex] || "delivery")
                    var request = {fileName: fileName, item: item, fps: fps, qualityProfile: qualityProfile}
                    var target = window.exportOutputDirectory + "/" + fileName
                    if (workbenchRuntime.exportFileExists(target)) {
                        window.pendingExportRequest = request
                        exportConflictDialog.open()
                        return
                    }
                    window.startTimelineExport(request)
                }
                }
            }
        }
    }

    function startTimelineExport(request) {
        workbenchRuntime.setPendingFablecutExportPath(window.exportOutputDirectory + "/" + request.fileName)
        fablecutView.runJavaScript("window.startFablecutExportFromQt(" + JSON.stringify({width: request.item.width, height: request.item.height, fps: request.fps, crop: "none", format: "mp4"}) + "," + JSON.stringify(request.qualityProfile) + "," + JSON.stringify(request.fileName) + ")")
        window.exportFileName = request.fileName
        window.pendingExportRequest = null
        timelineExportPanel.close()
    }

    Dialog {
        id: exportConflictDialog
        anchors.centerIn: Overlay.overlay
        modal: true
        title: "文件已存在"
        standardButtons: Dialog.NoButton
        padding: 18
        background: Rectangle { color: "#1b1b1d"; radius: 14; border.color: "#3a3a3e"; border.width: 1 }
        contentItem: ColumnLayout {
            spacing: 12
            Label { Layout.fillWidth: true; text: "文件名已存在，是否覆盖？"; color: "#f5f5f7"; font.pixelSize: 15 }
            Label { Layout.fillWidth: true; text: window.pendingExportRequest ? window.pendingExportRequest.fileName : ""; color: DesignTokens.textSecondary; elide: Text.ElideMiddle }
            RowLayout {
                Layout.fillWidth: true; spacing: 8
                Button { Layout.fillWidth: true; text: "返回修改"; onClicked: exportConflictDialog.close() }
                Button {
                    Layout.fillWidth: true; text: "覆盖"; highlighted: true
                    onClicked: {
                        if (window.pendingExportRequest && workbenchRuntime.removeExportFile(window.exportOutputDirectory + "/" + window.pendingExportRequest.fileName)) {
                            var request = window.pendingExportRequest
                            exportConflictDialog.close()
                            window.startTimelineExport(request)
                        }
                    }
                }
            }
        }
    }

    FolderDialog {
        id: exportDirectoryDialog
        title: "选择视频导出位置"
        onAccepted: {
            var folderPath = window.localPathFromDialogUrl(selectedFolder)
            if (folderPath !== "") {
                window.exportOutputDirectory = folderPath
                var settings = workbenchRuntime.fablecutSettings()
                workbenchRuntime.saveFablecutSettings(settings.providerId, settings.provider, settings.endpoint, "", settings.model, settings.protocol, folderPath)
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        WebEngineView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            url: workbenchRuntime.nativeRuntimePreviewEntry
            visible: workbenchRuntime.nativeRuntimeSelected
            onLoadingChanged: if (loadRequest.status === WebEngineView.LoadSucceededStatus)
                runJavaScript("window.dispatchEvent(new CustomEvent('edward-runtime-message',{detail:" + JSON.stringify(workbenchRuntime.nativeRuntimeHostMessage) + "}))")
        }
        WebEngineView {
            id: fablecutView
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !workbenchRuntime.nativeRuntimeSelected
            url: fablecutServerUrl
            settings.javascriptEnabled: true
            settings.localStorageEnabled: true
            webChannel: preferenceWebChannel
            onLoadingChanged: if (loadRequest.status === WebEngineView.LoadSucceededStatus)
                runJavaScript("window.edwardAiCapabilitySnapshot=" + JSON.stringify(workbenchRuntime.fablecutCapabilitySnapshot()) + ";")
        }
    }
}
