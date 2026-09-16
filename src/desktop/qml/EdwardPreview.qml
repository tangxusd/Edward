import QtQuick
import QtWebEngine
import "."

Item {
    id: root
    property int playheadFrame: 0
    property int durationFrames: 1
    property url source
    property bool hasFrame: false
    property bool componentOverlayEnabled: false
    property int componentX: 0
    property int componentY: 0
    property int componentWidth: 220
    property int componentHeight: 72
    property var componentNodes: []
    property string selectedComponentNodeId: ""
    property bool nativeRuntimeVisible: false
    property url nativeRuntimeEntry
    property var nativeRuntimeMessage: ({})
    signal componentDragged(int x, int y)
    signal componentNodeSelected(string nodeId)

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

        Repeater {
            model: 7
            delegate: Item {
                readonly property int offset: (index - 3) * 100
                x: canvas.width / 2 + offset
                y: 0
                width: 1
                height: canvas.height
                Rectangle {
                    width: 1
                    height: 7
                    color: DesignTokens.textSecondary
                }
                Text {
                    x: 4
                    y: 8
                    text: (-offset).toString()
                    color: DesignTokens.textSecondary
                    font.pixelSize: 10
                }
            }
        }
        Repeater {
            model: 5
            delegate: Item {
                readonly property int offset: (index - 2) * 100
                x: 0
                y: canvas.height / 2 + offset
                width: canvas.width
                height: 1
                Rectangle {
                    width: 7
                    height: 1
                    color: DesignTokens.textSecondary
                }
                Text {
                    x: 8
                    y: -16
                    text: (-offset).toString()
                    color: DesignTokens.textSecondary
                    font.pixelSize: 10
                }
            }
        }
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 1
            color: "#335e666b"
        }
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: "#335e666b"
        }

        Rectangle {
            id: componentBounds
            z: 2
            visible: root.componentOverlayEnabled
            x: canvas.width / 2 - root.componentX - width / 2
            y: canvas.height / 2 - root.componentY - height / 2
            width: root.componentWidth
            height: root.componentHeight
            color: "transparent"
            border.color: DesignTokens.accent
            border.width: 1
            MouseArea {
                anchors.fill: parent
                property real pressX
                property real pressY
                property int initialX
                property int initialY
                onPressed: {
                    pressX = mouse.x;
                    pressY = mouse.y;
                    initialX = root.componentX;
                    initialY = root.componentY;
                }
                onPositionChanged: if (pressed)
                    root.componentDragged(initialX - Math.round(mouse.x - pressX), initialY - Math.round(mouse.y - pressY))
            }
        }

        Repeater {
            model: root.componentNodes
            delegate: Rectangle {
                required property var modelData
                readonly property bool hasBounds: modelData.width > 0 && modelData.height > 0
                z: 4
                visible: root.componentOverlayEnabled && hasBounds
                x: canvas.width / 2 - modelData.x - width / 2
                y: canvas.height / 2 - modelData.y - height / 2
                width: modelData.width
                height: modelData.height
                color: "transparent"
                border.color: root.selectedComponentNodeId === modelData.id ? DesignTokens.accent : "#6688a0a8"
                border.width: root.selectedComponentNodeId === modelData.id ? 2 : 1
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.componentNodeSelected(modelData.id)
                }
            }
        }

        Image {
            anchors.fill: parent
            source: root.source.toString() !== "" ? root.source :
                    (root.hasFrame ? "image://edward/frame?frame=" + root.playheadFrame : "")
            cache: false
            fillMode: Image.PreserveAspectFit
            visible: status === Image.Ready || source !== ""
        }

        WebEngineView {
            id: nativeRuntimeView
            anchors.fill: parent
            visible: root.nativeRuntimeVisible && root.nativeRuntimeEntry.toString() !== ""
            url: root.nativeRuntimeEntry
            settings.javascriptEnabled: true
            onLoadingChanged: function(loadRequest) {
                if (loadRequest.status === WebEngineView.LoadSucceededStatus)
                    runJavaScript("window.dispatchEvent(new CustomEvent('edward-runtime-message', {detail: " + JSON.stringify(root.nativeRuntimeMessage) + "}));")
            }
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
