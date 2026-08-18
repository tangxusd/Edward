import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
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
                Text { anchors.centerIn: parent; text: "素材库\n+ 导入素材"; color: DesignTokens.textSecondary; horizontalAlignment: Text.AlignHCenter; font.pixelSize: 13 }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 1
                EdwardPreview { Layout.fillWidth: true; Layout.fillHeight: true }
                EdwardTimeline { Layout.fillWidth: true; Layout.preferredHeight: 220 }
            }

            Rectangle {
                Layout.preferredWidth: 280
                Layout.fillHeight: true
                color: DesignTokens.panel
                Text { anchors.centerIn: parent; text: "AI 创作"; color: DesignTokens.accent; font.pixelSize: 13 }
            }
        }
    }
}
