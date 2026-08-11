# Edward 0.2.0 实施文件所有权与公开接口合同

本合同消除 00–08 计划中的路径简写和跨模块直接访问歧义。所有路径相对项目根目录；每个 `.hpp` 是该功能唯一公开入口，`.cpp` 只能由所属目标编译。未在下表列出的代码不得绕过这些接口读写 `project.json`、MLT、FFmpeg、网络或插件目录。

## 共同类型与目标边界

| 目标 | 目录 | 可依赖 | 禁止依赖 |
| --- | --- | --- | --- |
| `edward_core` | `src/core/` | C++20、nlohmann/json | Qt、FFmpeg、MLT、网络、文件选择器 |
| `edward_media` | `src/media/` | `edward_core`、FFmpeg、MLT | QML、账户、插件网络 |
| `edward_desktop` | `src/desktop/` | core/media、Qt 6 | 直接读写工程 JSON、直接调用 FFmpeg 命令行 |
| `edward_resources` | `src/resources/` | core、Qt Network、libsodium | 直接修改时间线 |
| `edward_ai` | `src/ai/` | core/resources、Qt Network、whisper.cpp | 未应用即修改实例 |
| `edward_plugins` | `src/plugins/` | core/resources、libsodium、受限进程 IPC | 进入主进程、引擎写入、直接网络 |
| `edward_account` | `src/account/` | Qt Network、libsodium | 伪造服务端成功 |

所有模块共享 `src/core/include/edward/core/ids.hpp`：`ProjectId`、`AssetId`、`TrackId`、`ClipId`、`ComponentId`、`ResourceId` 均为不与路径混用的强类型字符串；`Frame` 为有符号 64 位整数工程帧。

## 00–02：工程、媒体与时间线

| 公开头文件 | 唯一实现 | 测试 | 最小公开接口 |
| --- | --- | --- | --- |
| `src/core/include/edward/core/project_document.hpp` | `src/core/src/project_document.cpp` | `tests/core/test_project_document.cpp` | `ProjectDocument::create(ProjectId, ProjectSettings)`、`snapshot()`、`apply(const EditCommand&)` |
| `src/core/include/edward/core/project_store.hpp` | `src/core/src/project_store.cpp` | `tests/core/test_project_store.cpp` | `saveFull(const ProjectDocument&, const ProjectRoot&) -> Expected<void, StoreError>`、`load(const ProjectRoot&) -> Expected<ProjectDocument, StoreError>` |
| `src/media/include/edward/media/media_probe.hpp` | `src/media/src/media_probe.cpp` | `tests/media/test_media_probe.cpp` | `probe(const std::filesystem::path&) -> Expected<MediaInfo, ProbeError>` |
| `src/media/include/edward/media/media_importer.hpp` | `src/media/src/media_importer.cpp` | `tests/media/test_media_importer.cpp` | `importCopy(const ProjectRoot&, const SourcePath&) -> Expected<ManagedAsset, ImportError>`、`markMissing(AssetId)` |
| `src/core/include/edward/core/track.hpp` | `src/core/src/track.cpp` | `tests/core/test_timeline.cpp` | `TrackKind`、`TrackState`、`Track` |
| `src/core/include/edward/core/clip.hpp` | `src/core/src/clip.cpp` | `tests/core/test_timeline.cpp` | `Clip`、`FrameRange`、`SourceRange`、`TimeRemap` |
| `src/core/include/edward/core/timeline.hpp` | `src/core/src/timeline.cpp` | `tests/core/test_timeline.cpp` | `drop(ClipDraft, DropTarget)`、`trim(ClipId, FrameRange)`、`split(ClipId, Frame)`、`setRange(RangeKind, Frame)` |
| `src/core/include/edward/core/transitions.hpp` | `src/core/src/transitions.cpp` | `tests/core/test_timeline_transitions.cpp` | `applyTransition(ClipId, ClipId, TransitionKind, FrameRange)`；仅闪黑、闪白、叠化 |
| `src/core/include/edward/core/edit_command.hpp` | `src/core/src/edit_command.cpp` | `tests/core/test_edit_commands.cpp` | `EditCommand::apply(ProjectDocument&)`、`revert(ProjectDocument&)`、`kind()` |
| `src/core/include/edward/core/command_history.hpp` | `src/core/src/command_history.cpp` | `tests/core/test_edit_commands.cpp` | `execute`、`undo`、`redo`；所有栈最大 5 |
| `src/core/include/edward/core/playback_state.hpp` | `src/core/src/playback_state.cpp` | `tests/core/test_playback_state.cpp` | `seek(Frame)`、`setLoop(FrameRange)`、`setInOut(FrameRange)` |
| `src/core/include/edward/core/markers.hpp` | `src/core/src/markers.cpp` | `tests/core/test_markers.cpp` | `add(Frame, MarkerColor)`、`move`、`remove`；无名称、十种颜色 |
| `src/core/include/edward/core/shortcut_map.hpp` | `src/core/src/shortcut_map.cpp` | `tests/core/test_shortcut_map.cpp` | `dispatch(KeyEvent, FocusKind) -> std::optional<TimelineAction>` |

## 03–04：组件、动画与媒体执行

| 公开头文件 | 唯一实现 | 测试 | 最小公开接口 |
| --- | --- | --- | --- |
| `src/core/include/edward/core/component_template.hpp` | `src/core/src/component_template.cpp` | `tests/core/test_component_instance.cpp` | 不可变 `ComponentTemplate`、`instantiate(ComponentId)` |
| `src/core/include/edward/core/component_instance.hpp` | `src/core/src/component_instance.cpp` | `tests/core/test_component_instance.cpp` | `clone()`、`removeNode(NodeId)`、`replaceTemplate(const ComponentTemplate&)` |
| `src/core/include/edward/core/keyframes.hpp` | `src/core/src/keyframes.cpp` | `tests/core/test_keyframes.cpp` | `KeyframeTrack<T>::set(Frame, T, Interpolation)`、`remove(Frame)`、`evaluate(Frame)` |
| `src/core/include/edward/core/animation_presets.hpp` | `src/core/src/animation_presets.cpp` | `tests/core/test_animation_presets.cpp` | `applyPreset(AnimationPreset, ComponentInstance&, FrameRange)` |
| `src/core/include/edward/core/guides.hpp` | `src/core/src/guides.cpp` | `tests/core/test_guides.cpp` | `addHorizontal`、`addVertical`、`addDiagonal`、`move`、`lockAll`、`setVisible` |
| `src/media/include/edward/media/render_graph.hpp` | `src/media/src/render_graph.cpp` | `tests/media/test_render_graph.cpp` | `RenderGraph::build(const ProjectSnapshot&, const RenderRequest&) -> RenderScene` |
| `src/media/include/edward/media/mlt_adapter.hpp` | `src/media/src/mlt_adapter.cpp` | `tests/media/test_mlt_adapter.cpp` | `buildProducer(const RenderScene&)`、`renderFrame(Frame)` |
| `src/media/include/edward/media/preview_session.hpp` | `src/media/src/preview_session.cpp` | `tests/media/test_preview_session.cpp` | `open(ProjectSnapshot)`、`seek(Frame)`、`setQuality(PreviewQuality)` |
| `src/media/include/edward/media/proxy_manager.hpp` | `src/media/src/proxy_manager.cpp` | `tests/media/test_proxy_manager.cpp` | `ensureProxy(AssetId, PreviewQuality)`、`resolveForPreview` |
| `src/media/include/edward/media/waveform_cache.hpp` | `src/media/src/waveform_cache.cpp` | `tests/media/test_waveform_cache.cpp` | `ensureWaveform(AssetId)`；只写 cache |

## 05：桌面控制器与 QML

| 控制器头/源 | QML | 测试 | 职责 |
| --- | --- | --- | --- |
| `src/desktop/include/edward/desktop/home_controller.hpp` / `src/desktop/src/home_controller.cpp` | `src/desktop/qml/HomePage.qml` | `tests/desktop/test_home_controller.cpp` | 历史工程、右键删除、进入/关闭工程 |
| `src/desktop/include/edward/desktop/project_create_controller.hpp` / `src/desktop/src/project_create_controller.cpp` | `src/desktop/qml/NewProjectDialog.qml` | `tests/desktop/test_project_create_controller.cpp` | 主媒体校验、文稿可选、创建状态 |
| `src/desktop/include/edward/desktop/workbench_controller.hpp` / `src/desktop/src/workbench_controller.cpp` | `src/desktop/qml/Workbench.qml` | `tests/desktop/test_workbench_layout.cpp` | 四栏比例、分隔条和触底布局 |
| `src/desktop/include/edward/desktop/timeline_controller.hpp` / `src/desktop/src/timeline_controller.cpp` | `src/desktop/qml/TimelineView.qml` | `tests/desktop/test_timeline_controller.cpp` | 指针/快捷键转为 core 命令 |
| `src/desktop/include/edward/desktop/timeline_selection_controller.hpp` / `src/desktop/src/timeline_selection_controller.cpp` | `src/desktop/qml/TimelineView.qml` | `tests/desktop/test_timeline_selection_controller.cpp` | 多选、分组、链接、轨道状态与命令边界 |
| `src/desktop/include/edward/desktop/preview_controller.hpp` / `src/desktop/src/preview_controller.cpp` | `src/desktop/qml/PreviewCanvas.qml`、`GuidesMenu.qml`、`PreviewControls.qml` | `tests/desktop/test_preview_controller.cpp` | 坐标逆变换、命中、拖拽、缩放、吸附 |
| `src/desktop/include/edward/desktop/property_controller.hpp` / `src/desktop/src/property_controller.cpp` | `src/desktop/qml/PropertyPanel.qml` | `tests/desktop/test_property_controller.cpp` | 文字/图表/组件属性、字幕统一样式、关键帧 |
| `src/desktop/include/edward/desktop/settings_controller.hpp` / `src/desktop/src/settings_controller.cpp` | `SettingsDialog.qml`、`ModelEditor.qml` | `tests/desktop/test_settings_controller.cpp` | 项目、模型、插件设置 |
| `src/desktop/include/edward/desktop/account_controller.hpp` / `src/desktop/src/account_controller.cpp` | `AccountDialog.qml`、`SubscriptionDialog.qml`、`DeviceManagement.qml`、`PaymentConfirm.qml` | `tests/desktop/test_account_controller.cpp` | 真实失败态和签名目录状态 |
| `src/desktop/include/edward/desktop/export_controller.hpp` / `src/desktop/src/export_controller.cpp` | `ExportDialog.qml` | `tests/desktop/test_export_controller.cpp` | 标题栏唯一导出入口与任务状态 |

`src/desktop/qml/Main.qml`、`NativeTitlebar.qml`、`DesignTokens.qml`、`ResourcePanel.qml`、`ResourceUpgrade.qml`、`ResourcePreview.qml`、`GlobalAiPanel.qml`、`ComponentLibrary.qml`、`CreationProgress.qml`、`CloseProjectDialog.qml` 分别只组合上述控制器的状态，不保存工程真相。

## 06–08：资源、AI、插件、账户和导出

| 公开头文件 | 唯一实现 | 测试 | 最小公开接口 |
| --- | --- | --- | --- |
| `src/resources/include/edward/resources/resource_index.hpp` | `src/resources/src/resource_index.cpp` | `tests/resources/test_resource_index.cpp` | `category(ResourceId)`、`isInstalled(ResourceId)`、`installLocal` |
| `src/resources/include/edward/resources/resource_catalog.hpp` | `src/resources/src/resource_catalog.cpp` | `tests/resources/test_resource_catalog_view.cpp` | `localDefaults()`、`replaceVerifiedCatalog(...)`；05 先本地、06 再注入验签目录 |
| `src/resources/include/edward/resources/resource_catalog_manifest.hpp` | `src/resources/src/resource_catalog_manifest.cpp` | `tests/resources/test_resource_catalog.cpp` | `verifyResourceCatalog(bytes, signature, keyId)` |
| `src/resources/include/edward/resources/resource_installer.hpp` | `src/resources/src/resource_installer.cpp` | `tests/resources/test_resource_installer.cpp` | `installApproved(ResourceId, Version)` |
| `src/resources/include/edward/resources/upload_queue.hpp` | `src/resources/src/upload_queue.cpp` | `tests/resources/test_upload_queue.cpp` | `enqueue(ComponentDraft)`、`runDueJobs(now)` |
| `src/ai/include/edward/ai/model_config.hpp` | `src/ai/src/model_config.cpp` | `tests/ai/test_model_client.cpp` | 本地明文配置，不序列化进工程 |
| `src/ai/include/edward/ai/model_template_catalog.hpp` | `src/ai/src/model_template_catalog.cpp` | `tests/ai/test_model_client.cpp` | 已签名模板目录与模型能力显示 |
| `src/ai/include/edward/ai/model_client.hpp` | `src/ai/src/model_client.cpp` | `tests/ai/test_model_client.cpp` | `complete(ModelRequest) -> ModelResult` |
| `src/ai/include/edward/ai/transcription.hpp` | `src/ai/src/transcription.cpp` | `tests/ai/test_transcription.cpp` | `transcribe(ManagedAsset) -> MarkdownTranscript` |
| `src/ai/include/edward/ai/semantic_plan.hpp` | `src/ai/src/semantic_plan.cpp` | `tests/ai/test_semantic_plan.cpp` | `analyze(CreateInput) -> SemanticPlan` |
| `src/ai/include/edward/ai/component_merge.hpp` | `src/ai/src/component_merge.cpp` | `tests/ai/test_component_merge.cpp` | `compareAndMerge(instance, draft) -> MergePreview`、`apply` |
| `src/ai/include/edward/ai/component_creator.hpp` | `src/ai/src/component_creator.cpp` | `tests/ai/test_component_creator.cpp` | `createDraft(ReferenceAsset)` |
| `src/plugins/include/edward/plugins/plugin_manifest.hpp` | `src/plugins/src/plugin_manifest.cpp` | `tests/plugins/test_plugin_manifest.cpp` | `verify`、`isApprovedForPlatform` |
| `src/plugins/include/edward/plugins/plugin_installer.hpp` | `src/plugins/src/plugin_installer.cpp` | `tests/plugins/test_plugin_manifest.cpp` | `installApproved(PluginId, Version)` |
| `src/plugins/include/edward/plugins/plugin_host.hpp` | `src/plugins/src/plugin_host.cpp` | `tests/plugins/test_plugin_host.cpp` | `launch(VerifiedPlugin)`、受限 RPC |
| `src/plugins/include/edward/plugins/component_draft_validator.hpp` | `src/plugins/src/component_draft_validator.cpp` | `tests/plugins/test_plugin_host.cpp` | `validate(ComponentDraft)`；拒绝越权数据与引擎写入 |
| `src/account/include/edward/account/signed_catalog.hpp` | `src/account/src/signed_catalog.cpp` | `tests/account/test_signed_catalog.cpp` | `verify(bytes, signature, keyId)` |
| `src/account/include/edward/account/subscription_status_client.hpp` | `src/account/src/subscription_status_client.cpp` | `tests/account/test_subscription_status_client.cpp` | `checkBeforeCloudAi()` |
| `src/media/include/edward/media/export_profile.hpp` | `src/media/src/export_profile.cpp` | `tests/media/test_export_profile.cpp` | `resolveExportProfile(ExportOptions)` |
| `src/media/include/edward/media/hardware_capabilities.hpp` | `src/media/src/hardware_capabilities.cpp` | `tests/media/test_encoder_selector.cpp` | `probeHardwareEncoders()` |
| `src/media/include/edward/media/encoder_selector.hpp` | `src/media/src/encoder_selector.cpp` | `tests/media/test_encoder_selector.cpp` | `selectEncoder(profile, capabilities)` |
| `src/media/include/edward/media/export_job.hpp` | `src/media/src/export_job.cpp` | `tests/media/test_export_job.cpp` | `start`、`cancel`、单调 `progress` 信号 |

## 外部工件所有权

- 字体：`assets/fonts/`；只允许许可证、版本、SHA-256 都写入 `docs/contracts/font-manifest.json` 的可再分发文件。
- 转写模型：`artifacts/models/` 仅供本地开发/安装包工件；文件名、许可证、SHA-256 写入 `docs/contracts/model-artifact-manifest.json`，模型二进制不进入 Git。
- 性能夹具：`tests/perf/fixtures/`；每个实际媒体文件仅由 manifest 的 SHA-256 引用，二进制不进入 Git。
