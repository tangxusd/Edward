# Edward 0.2.0 受限插件宿主 RPC 合同（历史路线）

> 本合同仅供历史审计。Edward 0.6.0 AI 对话和官方运行时宿主不得使用 Component IR、转换层或 ProRes 替代输出；新实现以 `docs/superpowers/specs/2026-09-16-ai-conversation-module-design.md` 为准。

## 进程与权限

插件（首版仅 Remotion、Hyperframes）在发行版目标状态下运行于独立受限子进程。插件代码不得链接、加载或写入 `edward_core`、`edward_media`、工程目录、模型配置或订阅状态；不得直接访问网络、系统凭证或任意用户路径。当前 Debug 宿主仅用于本地适配器开发，不等同于这一发行级隔离。

`edward_plugin_host` 是唯一 IPC 对端，负责校验已批准的 manifest、Ed25519 签名、Release tag、平台、SHA-256、撤销状态和能力。未通过任一校验的插件不得启动。

## 允许的请求

插件仅可发起以下经过 manifest 授权的 RPC：

1. `read_input_asset`：读取宿主复制到本次任务临时目录的只读截图/视频输入；
2. `write_draft_output`：仅向本次任务临时目录写入组件草稿、日志和渲染产物；
3. `report_progress` 与 `report_error`：只传递结构化状态。

请求必须包含任务 ID、插件 ID、插件版本、输入/输出相对路径和大小限制。宿主拒绝路径穿越、任何网络请求、超额资源、未声明能力与超时请求，并返回可呈现错误码。插件网络访问不能通过 manifest、环境变量或用户选择插件目录重新启用。

## 结果接入

宿主先以 `component_draft_validator` 对草稿做字段/资源/大小校验，再由用户点击“应用”或“保存”才复制到工程或资源库。不能转换为原生组件的动画由插件在临时目录生成 ProRes 4444 Alpha 素材；宿主用 FFprobe 验证 Alpha、时长、帧率和音轨后，再作为普通独立素材层导入。

临时目录按任务创建、结束后清理；上传队列副本与用户本地“我的”资源分离。所有网络、安装和转换失败必须可见，不能伪造成功。
