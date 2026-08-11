# Edward 0.2.0 受限插件宿主 RPC 合同

## 进程与权限

插件（首版仅 Remotion、Hyperframes）运行于独立受限子进程。插件代码不得链接、加载或写入 `edward_core`、`edward_media`、工程目录、模型配置或订阅状态；不得直接访问网络、系统凭证或任意用户路径。

`edward_plugin_host` 是唯一 IPC 对端，负责校验已批准的 manifest、Ed25519 签名、Release tag、平台、SHA-256、撤销状态和能力。未通过任一校验的插件不得启动。

## 允许的请求

插件仅可发起以下经过 manifest 授权的 RPC：

1. `read_input_asset`：读取宿主复制到本次任务临时目录的只读截图/视频输入；
2. `request_network`：由宿主按 manifest 的固定 HTTPS 域名白名单转发请求；插件不获得 API Key、Cookie、订阅令牌或网络套接字；
3. `write_draft_output`：仅向本次任务临时目录写入组件草稿、日志和渲染产物；
4. `report_progress` 与 `report_error`：只传递结构化状态。

请求必须包含任务 ID、插件 ID、插件版本、输入/输出相对路径和大小限制。宿主拒绝路径穿越、未知域名、重定向到未批准域名、非 HTTPS、超额资源、未声明能力与超时请求，并返回可呈现错误码。

## 结果接入

宿主先以 `component_draft_validator` 对草稿做字段/资源/大小校验，再由用户点击“应用”或“保存”才复制到工程或资源库。不能转换为原生组件的动画由插件在临时目录生成 ProRes 4444 Alpha 素材；宿主用 FFprobe 验证 Alpha、时长、帧率和音轨后，再作为普通独立素材层导入。

临时目录按任务创建、结束后清理；上传队列副本与用户本地“我的”资源分离。所有网络、安装和转换失败必须可见，不能伪造成功。
