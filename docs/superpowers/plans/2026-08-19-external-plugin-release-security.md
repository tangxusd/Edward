# Edward 外部插件发布安全门计划

**目标：** 将用户自行安装 Remotion/HyperFrames 所依赖的外部动画适配器，从当前的“受控子进程”提升为可验证来源、固定运行时和 OS 级最小权限运行；不把这项工作误称为基础剪辑 MVP 的组成部分。

## 当前事实

- `PluginManifest` 已限制入口必须位于选定插件根目录，且运行时仅允许 `native`、`node`、`bun`。
- `QProcess` 已限制 RPC 方法、工作目录、超时及返回 PNG Alpha/Component IR，但子进程仍继承当前用户权限。
- Remotion 与 HyperFrames 适配器均通过 PATH 查找 `node`，其 Node 与 Chromium 依赖由用户在插件目录安装。
- 本机 macOS 15.7.7 存在 `/usr/bin/sandbox-exec`，但这不能代替 Windows 隔离，也不能为依赖 PATH 的任意 Node/Chromium 构成可发布的完整安全边界。

## 不变约束

- Edward 继续保存并编辑标准 Component IR；插件不得成为工程编辑真相源。
- 用户仍可独立安装 Remotion/HyperFrames；Edward 不把它们的完整运行时代码并入主程序。
- 插件只获得插件目录、为本次任务创建的只读输入目录和仅本次任务的输出目录；不得访问工程以外的用户文件、Edward 引擎写目录或任意网络。
- 不采用“普通 QProcess + 文档提示”冒充 OS 级隔离。
- 未能提供两端等价隔离时，正式发行版拒绝启动外部适配器；开发构建可保留显式诊断，不静默降级。

## 决策前提

已确认的发行决策：Edward 分发并校验一份固定 Node 运行时；Remotion/HyperFrames 适配器及其 npm 依赖仍由用户在插件目录独立安装；Chromium 作为两个适配器共用的首次启用缓存运行时下载。Node 运行时体积、许可证和更新策略属于发行包决策，不能在未完成打包合同前悄悄加入。

## 里程碑

### M1：安全合同与失败测试

**涉及文件：**

- Create: `docs/contracts/external-plugin-sandbox.md`
- Modify: `docs/contracts/animation-plugin-rpc.json`
- Modify: `src/plugins/include/edward/plugins/plugin_manifest.hpp`
- Modify: `src/plugins/src/plugin_manifest.cpp`
- Modify: `tests/plugins/test_plugin_manifest.cpp`
- Create: `tests/plugins/test_plugin_sandbox_policy.cpp`

**成功标准：**

1. 清单显式声明适配器签名、目标运行时版本和只读/只写任务目录需求。
2. 缺少签名、未知密钥、PATH 运行时、网络权限或根目录外访问请求均先有失败测试，再被拒绝。
3. 生成的策略只包含插件根、固定运行时、任务输入与任务输出四类路径。

### M2：固定运行时与签名信任根

**当前进度：** 固定 Node 目录和 SHA-256 校验、清单 Ed25519 规范化验签原语、Release 对缺失/不可信公钥的加载拒绝均已完成并有定向构建验证；真实发行公钥资产、已签名测试夹具与运行时版本探测仍待完成。

**涉及文件：**

- Create: `src/plugins/include/edward/plugins/plugin_trust_store.hpp`
- Create: `src/plugins/src/plugin_trust_store.cpp`
- Create: `src/plugins/include/edward/plugins/plugin_runtime_resolver.hpp`
- Create: `src/plugins/src/plugin_runtime_resolver.cpp`
- Modify: `src/plugins/src/plugin_host.cpp`
- Modify: `src/plugins/CMakeLists.txt`
- Modify: `tests/plugins/test_plugin_host.cpp`

**成功标准：**

1. 适配器清单和被签名内容严格绑定；替换入口、版本或权限会被拒绝。
2. 发布构建只从已验证的 Edward 运行时目录解析 Node；PATH 仅可在开发构建下以显式诊断开关使用。
3. 没有可信密钥、运行时散列不匹配或运行时版本不匹配时，插件不启动。

### M3：macOS 最小权限执行器

**涉及文件：**

- Create: `src/plugins/include/edward/plugins/plugin_sandbox_runner.hpp`
- Create: `src/plugins/src/plugin_sandbox_runner_macos.mm`
- Create: `tests/plugins/test_plugin_sandbox_macos.cpp`
- Modify: `src/plugins/src/plugin_host.cpp`

**成功标准：**

1. 只使用 M2 已验证运行时；运行配置显式列出插件根、输入根、输出根与 Chromium 必需的临时目录。
2. 夹具可读取允许输入、写入允许输出；读取工程外哨兵文件、写入工程外哨兵文件、建立网络连接均被 OS 拒绝。
3. Node/Chromium 子进程树在超时或取消时被完整回收。

### M4：Windows AppContainer 执行器

**涉及文件：**

- Create: `src/plugins/src/plugin_sandbox_runner_windows.cpp`
- Create: `tests/plugins/test_plugin_sandbox_windows.cpp`
- Modify: `src/plugins/CMakeLists.txt`
- Modify: `.github/workflows/windows.yml`

**成功标准：**

1. 为每个插件版本创建或复用受限 AppContainer 身份，仅授予任务目录 ACL。
2. Windows 测试验证与 macOS 同等的允许读写、拒绝越界与拒绝网络行为。
3. 不支持 AppContainer 的环境必须拒绝外部适配器启动并给出可呈现错误，不回退至普通用户进程。

### M5：发行包与验收

**涉及文件：**

- Modify: `packaging/macos/Package.cmake`
- Modify: `packaging/windows/Package.cmake`
- Modify: `docs/contracts/dependency-lock.md`
- Modify: `docs/operation-log/2026-08-10-001-requirement-audit.md`

**成功标准：**

1. macOS `.app` 和 Windows MSI 都包含固定运行时、信任根、许可证与适配器验证元数据；资源库内容不混入程序包。
2. macOS 本机和 GitHub Windows 环境分别验证隔离、签名校验、基础 Remotion/HyperFrames 透明帧与完整 CTest。
3. 任何一个安全门失败均阻止发布包生成。

## 执行顺序

先完成 M1、M2，再分别完成 M3 和 M4。M3/M4 不能在无固定运行时和签名信任根的情况下单独宣称完成；M5 只在两端真实隔离测试通过后开始。
