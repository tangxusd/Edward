# Edward 0.2.0 最终视觉 Route 基线

此表逐一列出视觉确认清单“页面与最终视觉稿”中每个 HTML 文件。实现 05 的 `visual-routes.json` 必须逐行保持相同 `html`、`route`、`objectName`、`screenshotId`；任何遗漏都使 `desktop.visual_manifest` 失败。

| HTML | QML route | 根 objectName | 截图 ID |
| --- | --- | --- | --- |
| `homepage-v5-0.2-fixed.html` | `HomePage.qml` | `homePage` | `ui.home` |
| `new-project-import-v3-0.2-fixed.html` | `NewProjectDialog.qml` | `newProjectDialog` | `ui.new_project_import` |
| `ai-generation-progress-v2-0.2-fixed.html` | `CreationProgress.qml` | `creationProgress` | `ui.creation_progress` |
| `ai-plan-apply-v2-0.2-fixed.html` | `CreationProgress.qml` | `creationComplete` | `ui.creation_complete` |
| `close-project-v2-0.2-fixed.html` | `CloseProjectDialog.qml` | `closeProjectDialog` | `ui.close_project` |
| `edward-workbench-v19-0.2-fixed.html` | `Workbench.qml` | `workbenchRoot` | `ui.workbench` |
| `guides-menu-v4-0.2-fixed.html` | `GuidesMenu.qml` | `guidesMenu` | `ui.guides_menu` |
| `timeline-trim-v3-0.2-fixed.html` | `TimelineView.qml` | `timelinePane` | `ui.timeline_trim` |
| `timeline-track-move-v2-0.2-fixed.html` | `TimelineView.qml` | `timelinePane` | `ui.timeline_move` |
| `timeline-auto-track-v2-0.2-fixed.html` | `TimelineView.qml` | `timelinePane` | `ui.timeline_auto_track` |
| `media-import-success-v2-0.2-fixed.html` | `ResourcePanel.qml` | `resourcePanel` | `ui.media_import_success` |
| `media-import-failed-v2-0.2-fixed.html` | `ResourcePanel.qml` | `resourcePanel` | `ui.media_import_failure` |
| `text-library-v2-0.2-fixed.html` | `ResourcePanel.qml` | `resourcePanel` | `ui.resource_text` |
| `audio-library-v8-0.2-fixed.html` | `ResourcePanel.qml` | `resourcePanel` | `ui.resource_audio` |
| `card-library-v2-0.2-fixed.html` | `ResourcePanel.qml` | `resourcePanel` | `ui.resource_card` |
| `chart-library-v2-0.2-fixed.html` | `ResourcePanel.qml` | `resourcePanel` | `ui.resource_chart` |
| `background-library-v2-0.2-fixed.html` | `ResourcePanel.qml` | `resourcePanel` | `ui.resource_background` |
| `annotation-library-v2-0.2-fixed.html` | `ResourcePanel.qml` | `resourcePanel` | `ui.resource_annotation` |
| `number-library-v2-0.2-fixed.html` | `ResourcePanel.qml` | `resourcePanel` | `ui.resource_number` |
| `chart-properties-ai-v5-appearance-fixed.html` | `PropertyPanel.qml` | `propertyPanel` | `ui.property_chart` |
| `card-properties-ai-v5-0.2-fixed.html` | `PropertyPanel.qml` | `propertyPanel` | `ui.property_card` |
| `media-properties-v2-0.2-fixed.html` | `PropertyPanel.qml` | `propertyPanel` | `ui.property_media` |
| `background-properties-v2-0.2-fixed.html` | `PropertyPanel.qml` | `propertyPanel` | `ui.property_background` |
| `annotation-properties-v2-0.2-fixed.html` | `PropertyPanel.qml` | `propertyPanel` | `ui.property_annotation` |
| `number-properties-v2-0.2-fixed.html` | `PropertyPanel.qml` | `propertyPanel` | `ui.property_number` |
| `audio-properties-v2-0.2-fixed.html` | `PropertyPanel.qml` | `propertyPanel` | `ui.property_audio` |
| `subtitle-unified-style-v2-0.2-fixed.html` | `PropertyPanel.qml` | `propertyPanel` | `ui.property_subtitle` |
| `aspect-ratio-menu-v3-0.2-fixed.html` | `PreviewControls.qml` | `previewControls` | `ui.aspect_menu` |
| `preview-quality-menu-v2-0.2-fixed.html` | `PreviewControls.qml` | `previewControls` | `ui.preview_quality_menu` |
| `autosave-status-v2-0.2-fixed.html` | `NativeTitlebar.qml` | `titlebarStatus` | `ui.autosave_status` |
| `export-panel-v3-0.2-fixed.html` | `ExportDialog.qml` | `exportDialog` | `ui.export_dialog` |
| `settings-project-with-upgrade-v3-0.2-fixed.html` | `SettingsDialog.qml` | `settingsDialog` | `ui.settings_project` |
| `settings-models-v2-0.2-fixed.html` | `SettingsDialog.qml` | `settingsDialog` | `ui.settings_models` |
| `model-add-v5-capabilities.html` | `ModelEditor.qml` | `modelEditor` | `ui.settings_model_add` |
| `resource-upgrade-categories-v8-static-preview.html` | `ResourceUpgrade.qml` | `resourceUpgrade` | `ui.resource_upgrade` |
| `resource-preview-v3-local-status.html` | `ResourcePreview.qml` | `resourcePreview` | `ui.resource_preview` |
| `login-register-v3-captcha-slots.html` | `AccountDialog.qml` | `accountDialog` | `ui.login_register` |
| `register-v3-captcha-fixed.html` | `AccountDialog.qml` | `accountDialog` | `ui.register_captcha` |
| `subscription-v3-signed-catalog.html` | `SubscriptionDialog.qml` | `subscriptionDialog` | `ui.subscription` |
| `device-management-v1.html` | `DeviceManagement.qml` | `deviceManagement` | `ui.devices` |
| `payment-confirm-v2-signed-order.html` | `PaymentConfirm.qml` | `paymentConfirm` | `ui.payment_confirm` |
| `global-ai-create-v3-full-right.html` | `GlobalAiPanel.qml` | `globalAiPanel` | `ui.global_ai` |
| `component-save-v3-silent-local.html` | `GlobalAiPanel.qml` | `componentSaved` | `ui.component_saved` |
| `component-library-v2-my-components.html` | `ComponentLibrary.qml` | `componentLibrary` | `ui.my_components` |

截图基线固定窗口尺寸为 2560×1440 logical pixels、devicePixelRatio 1；macOS 和 Windows 分别保留一套 PNG，截图比较允许抗锯齿区域每通道最大差 8、总差异像素不超过 0.15%。字体缺失时必须使用 Edward 内置字体回退，再截图，不允许用平台默认字体掩盖差异。
