# 设置页操作记录

## 2026-09-17

- 目的：恢复 Edward 设置页中的大模型、路径和本地偏好入口，移除不再展示的快捷键布局与时间线素材联动选项。
- 涉及文件：`third_party/FableCut/index.html`、`third_party/FableCut/app.js`、`third_party/FableCut/settings-client.js`、`src/desktop/src/workbench_runtime.cpp`。
- 结果：设置通过现有 Qt WebChannel 持久化模型供应商、HTTPS 端点、模型 ID 和导出目录；API Key 不会回传给网页。偏好设置显示本地数据库路径，并可执行本地写入与编译。
- 验证：FableCut 设置与偏好测试通过；`desktop.preference_bridge`、`desktop.workbench_start_page`、`desktop.workbench_titlebar_actions` 通过。

## 2026-09-17 四 Tab 设置

- 目的：实现可迁移路径、大模型、偏好和调试设置，并保留已有项目与素材迁移。
- 涉及文件：`src/desktop/src/workbench_runtime.cpp`、`src/desktop/CMakeLists.txt`、`src/desktop/src/main.cpp`、`third_party/FableCut/{index.html,app.js,settings-client.js,server.js,paths.js}`、`supabase/migrations/202609170002_settings_cloud.sql`、`supabase/functions/{model-provider-presets,desktop-feedback}/index.ts`。
- 结果：九类路径均可迁移，项目仅迁移 `project.json`，其余目录完整复制且按文件大小校验后才删除源目录；拒绝源目标嵌套。清理仅限缓存、可重新下载的组件和素材、代理及预渲染目录。模型 API Key 不会回传至网页或上传云端；模型测试和模型列表使用本地 HTTPS 请求。偏好支持本地 JSON 导入导出，登录后可通过现有 `preference-sync` 合同上传下载。供应商预设与反馈仅传输公共预设和经敏感字段过滤的数据。
- 验证：`cmake --build build-0.7.0 --target edward_app -j 4`、`ctest --test-dir build-0.7.0 --output-on-failure`（33/33）、`cd third_party/FableCut && node --test`（89/89）和 `deno test --allow-read supabase/tests/settings-cloud-contract.test.ts`（2/2）通过。
- 部署状态：未执行 Supabase migration 或 Edge Function 部署，等待明确确认后才会对远程项目生效。

## 2026-09-17 云端部署

- 目的：部署设置页使用的供应商预设和调试反馈云端能力。
- 远程状态：链接项目 `naybqwiqgviuzjtemerc`，执行 `supabase db push --linked`，部署 `model-provider-presets` 与 `desktop-feedback`。
- 迁移历史：远程曾保留本地已删除的 `202609130002`、`202609130004` 记录；为允许新迁移推送，仅将这两条历史记录标记为 `reverted`，未执行业务表回滚。CLI 随后应用 `202609170001_native_runtime_target.sql` 与 `202609170002_settings_cloud.sql`。
- 验证：`supabase migration list --linked` 显示 `202609170002` 本地和远程一致；`supabase functions list` 显示两个新 Function 均为 `ACTIVE`；`deno test --allow-read supabase/tests/settings-cloud-contract.test.ts` 通过（2/2）。

## 2026-09-17 提供方表单

- 目的：将大模型设置表单调整为 Provider ID、显示名称、API 地址、API 协议、API 密钥和模型目录的明确合同。
- 结果：Provider ID 使用小写字母开头的本机唯一标识并持久化；模型目录由真实拉取结果填充，选择目录项会更新模型 ID。协议目前仅暴露已实现且可测试的 `openai-completions`，避免保存无法调用的 `Responses` 或 Anthropic 协议配置。
- 验证：`cmake --build build-0.7.0 --target test_preference_bridge edward_app -j 4`、相关 CTest（2/2）以及 `node --test test/auth-ui.test.js`（3/3）通过。

## 2026-09-17 多协议模型客户端

- 目的：实现 `openai-responses` 与 `anthropic-messages`，不再将提供方表单中的协议选项限制为单一协议。
- 结果：Responses 使用 `instructions`、`input` 和 `output_text/output[].content[].text`；Anthropic Messages 使用 `system`、`messages`、`x-api-key`、`anthropic-version` 和 `content[].text`。模型目录按协议使用 Bearer 或 Anthropic 请求头，并正确从 `/v1/messages` 推导 `/v1/models`。
- 验证：模型客户端单元测试覆盖三种协议的请求体、认证头、响应解析与模型目录端点。

## 2026-09-17 设置 Tab 布局与路径分类

- 目的：修复设置弹窗 Tab 内容溢出，并让路径设置清晰区分可迁移目录和可清理派生目录。
- 结果：弹窗使用固定的受限高度和内部滚动区域，所有 Tab 保持在弹窗边界内。路径页分为“项目与输出”“下载与插件”“缓存与渲染”；缓存、组件下载、素材下载、代理和预渲染均显示清理按钮。WebChannel 暂不可用时仍显示完整分类与禁用状态，不再显示空白页。
- 验证：桌面应用重建后已重启；`desktop.preference_bridge`、`desktop.workbench_start_page`、`desktop.workbench_titlebar_actions` 通过。

## 2026-09-17 设置页桌面端装载修复

- 目的：修复实际 Edward.app 中设置 Tab 溢出弹窗，以及路径设置无法连接桌面端的问题。
- 涉及文件：`third_party/FableCut/index.html`、`src/desktop/qml/Workbench.qml`、`src/desktop/src/main.cpp`、`tests/desktop/test_workbench_start_page.cpp`。
- 结果：移除模型设置页多余的闭合标签，后续 Tab 和弹窗页脚不再脱离设置对话框；在默认 WebEngine Profile 的文档创建阶段加载 `qwebchannel.js`；QML `WebChannel` 用命令式注册发布 `preferenceStore` 与 `workbenchRuntime`，避免 C++ `QWebChannel` 与 QML WebEngineView 的类型不匹配。
- 验证：重建 `edward_app` 后直接启动 `.app`，进程保持运行，启动日志未出现 QML 装载、WebChannel 赋值或对象注册错误；相关三项桌面 CTest 通过。

## 2026-09-17 桌面诊断日志上报

- 目的：让“上报错误”提交可供管理员分析的桌面诊断日志，而不只是用户文本。
- 涉及文件：`src/desktop/src/workbench_runtime.cpp`、`src/desktop/include/edward/desktop/workbench_runtime.hpp`、`third_party/FableCut/{index.html,style.css,app.js,settings-client.js}`。
- 结果：桌面端维护最大 256 KiB 的滚动日志，上报时最多读取最近 24 KiB；调试页可刷新预览，只有用户点击“上报诊断日志”时才发送。日志在写入和读取时均过滤凭证表达式、API Key、访问令牌、密码以及 macOS/Windows 本地路径，并以中性占位符替换，兼容现有云端敏感字段校验；不采集项目文件或素材。普通意见反馈不会附带诊断日志。
- 验证：`desktop.preference_bridge` 覆盖令牌和本地路径脱敏；相关桌面 CTest、FableCut 前端语法与设置页合同测试通过；更新后的 Edward.app 已启动。

## 2026-09-17 日志与意见反馈拆分

- 目的：将设置页中的日志诊断与产品需求收集分开，避免反馈意图混杂。
- 涉及文件：`third_party/FableCut/{index.html,app.js,test/auth-ui.test.js}`。
- 结果：原“调试”Tab 改为“日志”，仅保留日志刷新、预览和诊断日志上报；新增“意见反馈”Tab，独立收集用户需求、使用场景和期望结果，提交时不会附带日志。
- 验证：设置页五 Tab 合同测试、桌面设置桥接和标题栏测试通过。

## 2026-09-17 偏好包存储与导入导出

- 目的：不在设置页暴露本地数据库文件，并避免将偏好导入导出为数据库格式。
- 涉及文件：`src/desktop/src/{preference_store.cpp,workbench_runtime.cpp}`、`third_party/FableCut/{index.html,app.js}`。
- 结果：内部存储使用中性 `.store` 文件名；首次访问时会将旧 `preferences.sqlite` 及其 WAL/SHM 文件迁移到新名称。设置页只显示本地偏好存储状态，不显示路径。导入导出仅接受 `.edwardprefs` 偏好包，拒绝 `.db`、`.sqlite` 等任意非偏好包后缀。
- 验证：桌面偏好存储和桥接测试覆盖专用扩展名、数据库后缀拒绝以及设置快照不含数据库路径。

## 2026-09-18 紧凑设置弹窗与供应商卡片

- 目的：将设置弹窗缩小为原尺寸约三分之一，并让大模型设置先呈现已添加供应商。
- 涉及文件：`third_party/FableCut/{index.html,style.css,app.js,test/auth-ui.test.js}`。
- 结果：设置弹窗固定为 320 x 274 CSS 像素并由现有遮罩居中；全部设置内容保留内部滚动。大模型页默认显示活动供应商卡片，点击卡片进入详情；“添加供应商”打开空表单，保存后回到卡片列表。当前桌面设置合同只支持一个活动供应商，因此不会伪造多供应商持久化。
- 验证：前端设置页合同测试、桌面偏好/桥接/标题栏 CTest 通过。

## 2026-09-18 设置弹窗紧凑尺寸调整

- 目的：将设置弹窗宽度增至原紧凑尺寸的 1.5 倍、高度增至 2 倍，同时降低控件间距。
- 涉及文件：`third_party/FableCut/style.css`。
- 结果：设置弹窗调整为 480 x 548 CSS 像素的居中面板；四周内边距为 12px，Tab、字段、路径行、卡片和操作按钮均使用更小的间距与紧凑尺寸，内容保留内部滚动。
- 验证：前端设置页合同测试、桌面偏好/桥接/标题栏 CTest 通过。

## 2026-09-18 诊断覆盖与设置控件可读性

- 目的：扩展日志定位能力，并修复日志、反馈、模型设置和路径字段的可用性问题。
- 涉及文件：`src/desktop/src/{main.cpp,workbench_runtime.cpp}`、`src/desktop/include/edward/desktop/workbench_runtime.hpp`、`third_party/FableCut/{index.html,style.css,app.js,settings-client.js}`。
- 结果：诊断日志同时采集 Edward 桌面运行时事件、模型连接/目录结果、FableCut 脚本异常和未处理拒绝，以及本地 FableCut Node 服务的合并输出；所有文本仍经脱敏和长度限制。日志与意见反馈编辑区使用可用高度；“上报整段日志”和“提交意见”使用主色按钮。协议与模型目录均为可读的深色下拉框，路径字段为深灰底、灰白小字。
- 验证：桌面日志桥接测试覆盖 FableCut 事件写入与长度限制；桌面 CTest、前端语法和设置页合同测试通过。

## 2026-09-18 设置字段层级与反馈布局

- 目的：消除路径字段的原生凹陷外观，缩小下拉文字，并避免意见反馈输入区与提交按钮重叠。
- 涉及文件：`third_party/FableCut/style.css`、`third_party/FableCut/test/auth-ui.test.js`。
- 结果：路径字段使用扁平深灰背景、细边框和灰白小字；协议及模型目录下拉框固定为紧凑字号；日志和反馈页均由 Flex 布局分配编辑区与操作区，反馈按钮保持在编辑区下方。
- 验证：设置页合同测试覆盖路径字段、下拉字号与反馈编辑区的关键样式规则；完成后重启实际 Edward.app。

## 2026-09-18 云端供应商预设

- 目的：为桌面设置页提供可直接加载的 OpenAI 和 DeepSeek 公共供应商信息。
- 远程状态：在已链接 Supabase 项目 `naybqwiqgviuzjtemerc` 的 `model_provider_presets` 表中幂等写入 `openai` 与 `deepseek`；分别使用 `https://api.openai.com/v1` / `gpt-4.1` 和 `https://api.deepseek.com/v1` / `deepseek-chat`。
- 安全边界：预设仅包含公开端点与默认模型，不含 API Key；用户密钥仍只由 Edward 本地设置保存。
- 验证：远程查询确认两条记录均为活动状态，排序为 OpenAI 10、DeepSeek 20。

## 2026-09-18 DeepSeek 默认供应商

- 目的：让未配置大模型的新 Edward 桌面端默认显示 DeepSeek，并在云端预设中优先返回 DeepSeek。
- 涉及文件：`src/desktop/src/workbench_runtime.cpp`、`third_party/FableCut/app.js`、`tests/desktop/test_preference_bridge.cpp`、`third_party/FableCut/test/auth-ui.test.js`。
- 结果：不存在本地 `ai/*` 设置时，设置页预填 `DeepSeek`、`deepseek`、`https://api.deepseek.com/v1`、`openai-completions` 和 `deepseek-chat`；已有本地配置保持原值。云端预设排序调整为 DeepSeek 10、OpenAI 20。
- 验证：远程查询确认排序；重建 `edward_app`，桌面桥接/启动页/标题栏 CTest 与 FableCut 设置页测试均通过。

## 2026-09-18 供应商预设与登录会话持久化

- 目的：减少预设供应商配置步骤，并修复 Edward 重启后要求重复登录的问题。
- 涉及文件：`third_party/FableCut/{index.html,app.js,style.css,test/auth-ui.test.js}`、`src/desktop/src/main.cpp`、`tests/desktop/test_workbench_start_page.cpp`。
- 结果：打开大模型设置时自动请求供应商预设，并在顶部下拉框展示；选择预设后仅显示 API 密钥和默认折叠的连接详情，添加自定义供应商才展开可编辑详情。WebEngine 现在使用 Edward 应用数据目录中的固定 `web-profile`，并强制持久化 Cookie，因此同一应用与本地服务来源下的会话和 localStorage 可跨应用重启保留。
- 验证：重建 `edward_app`，桌面偏好桥接/启动页/标题栏 CTest 与 FableCut 设置页测试通过。

## 2026-09-18 遗留本地服务隔离

- 目的：修复 Edward.app 加载旧设置和登录页面的问题。
- 根因：端口 `7777` 上存在孤儿 Node 服务，工作目录为项目根目录的 `third_party/FableCut`，Edward 因此连接到了旧页面而非 0.7.0 工作树。
- 结果：确认工作目录后终止该遗留服务，重启 Edward。当前监听服务的工作目录为 `.worktrees/edward-0.7.0/third_party/FableCut`。
- 验证：读取 `http://127.0.0.1:7777/index.html`，确认包含供应商预设下拉框、注册和找回密码入口，不包含旧设置内容。

## 2026-09-18 供应商卡片优先

- 目的：让大模型设置首先显示已添加的供应商，而非直接进入预设表单。
- 涉及文件：`third_party/FableCut/{index.html,app.js,test/auth-ui.test.js}`。
- 结果：首屏以当前供应商卡片呈现；点击卡片可输入 API 密钥并查看折叠详情；点击“添加供应商”后才请求并显示 Supabase 供应商预设下拉框。
- 验证：FableCut 设置页合同测试和 JavaScript 语法检查通过。

## 2026-09-18 预设代理与本地服务启动修复

- 目的：修复供应商预设无法加载、DeepSeek 连接测试返回 404，以及重启后遗留本地服务导致的启动不稳定。
- 根因：本地代理未给 Edge Function 提供匿名 Bearer 令牌，网关返回 401；DeepSeek 与 OpenAI 预设保存的是 API 根路径而非客户端直接请求的聊天端点；强制退出 Edward 后的 Node 子进程会残留并短暂占用端口。
- 结果：代理默认携带匿名 Bearer 令牌，用户登录令牌仍优先透传；两项预设统一为完整 Chat Completions 地址；旧 DeepSeek `/v1` 本地设置在读取时迁移；启动前仅清理当前工作树的遗留监听服务，并等待固定端口释放。
- 验证：重建桌面应用，桌面 CTest 和 FableCut 设置页测试通过；当前实际 `http://127.0.0.1:7777/api/settings/provider-presets` 返回 DeepSeek 与 OpenAI 预设，服务工作目录为 `.worktrees/edward-0.7.0/third_party/FableCut`。

## 2026-09-18 供应商快捷操作

- 目的：让预设供应商无需展开连接详情即可完成常用配置，并在卡片上提供直接操作。
- 涉及文件：`third_party/FableCut/{index.html,app.js,style.css,test/auth-ui.test.js}`、`src/desktop/src/workbench_runtime.cpp`、`tests/desktop/test_preference_bridge.cpp`。
- 结果：预设添加页将模型目录、测试连接、拉取模型列表和保存供应商置于折叠详情外；卡片右侧改为纵向“设为默认”和“测试”。空密钥测试或拉取模型时，桌面端使用本机保存的 API Key，密钥不会回传至页面。
- 验证：重建 `edward_app`，桌面 CTest、前端语法与设置页合同测试通过。

## 2026-09-18 登录会话恢复与 AI 供应商同步

- 目的：恢复 Edward 的登录会话，并把默认供应商实际用于 AI 剪辑助理。
- 涉及文件：`src/desktop/src/workbench_runtime.cpp`、`src/desktop/include/edward/desktop/workbench_runtime.hpp`、`third_party/FableCut/{index.html,app.js,style.css,test/auth-ui.test.js,test/visual-layout.test.js}`。
- 结果：启动时从持久化会话恢复登录按钮状态；访问需授权接口前会检查令牌，接近过期时通过刷新令牌续期，并在后台查询订阅状态确认会话。默认供应商卡片使用橙色渐变边框标识，点击默认按钮不会再进入详情页；AI 助理显示当前供应商而不显示模型名，并以保存的供应商配置发起对话。助理不再因选中片段或会话而缩为顶部区域；请求期间显示“正在思考”及呼吸式橙色分隔线。
- 验证：`cmake --build build-0.7.0 --target edward_app -j4`、FableCut 前端测试及 `desktop.preference_bridge`、`desktop.workbench_start_page`、`desktop.workbench_titlebar_actions`、`desktop.fablecut_server_recovery` CTest 通过。

## 2026-09-18 属性检查器、偏好方案与渲染求值

- 目的：完成属性检查器基础能力，并让关键帧、循环动画及 In/Out 转场使用同一套预览和导出求值。
- 涉及文件：`third_party/FableCut/{property-contract.js,index.html,app.js,component-runtime.js,components/annotation.rect.*/component.js}`、`src/desktop/src/preference_store.cpp`、`tests/desktop/test_preference_store.cpp`、相关 FableCut 测试。
- 结果：属性显示名和下拉选项通过中文属性合同提供；检查器移除名称编辑，属性标签双击可恢复默认值并清除对应关键帧。新增循环动画类型、频率和幅度；关键帧默认三次贝塞尔缓出，可切换线性、回弹、缓出与缓入。偏好事件存储独立的 A/B/C 方案编号，兼容旧库默认归入方案 A。Canvas 与原生组件均接收同一求值后的属性；擦除与光圈转场由原生组件宿主裁切层实现。闭合圆角矩形的修复仅限四个注释组件，移除描边虚线裁断，改为完整连续描边加整体淡入。
- 验证：`npm test` 93 项通过；`node --check app.js`、`node --check property-contract.js`、`node --check component-runtime.js` 通过；`cmake --build build-0.7.0 -j 4` 通过；`test_preference_store` 与 `test_preference_bridge` 直接执行通过。

## 2026-09-18 属性合同收口、循环默认值与闭合矩形绘制

- 目的：消除检查器遗留英文属性名，调整新建组件循环动画默认值，收缩无对话 AI 助理，并恢复闭合矩形的线条绘制动画。
- 涉及文件：`third_party/FableCut/{property-contract.js,app.js,style.css}`、四个 `components/annotation.rect.*/component.js` 与对应前端测试。
- 结果：属性显示名、关键帧图与转场标签统一经 `property-contract.js` 输出；`duration`、入场和出场均使用中文。新建组件默认循环频率为 0.5 次/秒、幅度为 2。已选中片段但尚无对话时，AI 助理收缩为 118px 紧凑栏。四个闭合矩形均使用低透明连续底描边和上层 `pathLength` 绘制描边，线条会绘制且视觉上没有闭合缺口。
- 验证：`node --test third_party/FableCut/test/*.test.js` 92 项通过；`cmake --build build-0.7.0 --target edward_app -j 4` 通过；随后重启实际 Edward.app。

## 2026-09-18 闭合路径动画根因修复与属性合同国际化入口

- 目的：去除通过半透明底描边掩盖闭合缺口的实现，并为属性名称的多语言切换建立统一数据入口。
- 根因：此前单值 `stroke-dasharray="1"` 按 SVG 规则重复为等长实线与空白段；配合偏移量时，闭合路径的末端会落入空白段。
- 涉及文件：`third_party/FableCut/property-contract.js`、四个 `components/annotation.rect.*/component.js` 与对应回归测试。
- 结果：四个原生运行时均只保留一条 `rect` 描边，以 `pathLength="1"` 和 `${progress} 1` 的描边模式从路径起点增长；进度为 1 时第一段长度恰好覆盖完整周长，未使用底描边或透明度补偿。属性合同新增 `zh-CN`、`en` 词典和 `resolveLocale`，UI 维持中文默认，后续语言仅需向同一合同添加词典并传入 locale。
- 验证：FableCut 完整前端测试 92 项通过；`edward_app` 重建通过；随后重启实际 Edward.app。

## 2026-09-18 导出界面与逻辑修复

- 目的：修复导出弹窗英文混用、编码配置显示不清和文件名冲突检查逻辑不匹配的问题。
- 涉及文件：`third_party/FableCut/index.html`、`third_party/FableCut/app.js`、`third_party/FableCut/test/export-ui.test.js`。
- 结果：导出模式、编码配置、码率控制、操作按钮和进度状态统一为中文；内置编码配置提供稳定中文显示名；快速导出按所选配置的 `.mp4`/`.mov` 扩展名检查重名，实时导出保持 `.mp4`；导出过程中显示真实阶段和进度。
- 验证：FableCut 完整前端测试 95 项通过；`cmake --build build-0.7.0 --target edward_app -j 4` 通过。

## 2026-09-18 恢复 FableCut 导出分辨率选择

- 目的：恢复此前 FableCut 导出面板中的分辨率选择，不再只展示编码模式和编码配置。
- 涉及文件：`third_party/FableCut/index.html`、`third_party/FableCut/app.js`、`third_party/FableCut/test/export-ui.test.js`、`.edward/acceptance/current.json`。
- 结果：导出弹窗新增按当前项目横竖画幅适配的项目原始、720p、1080p、1440p、4K 选项；所选宽高形成统一输出规格，传入快速导出、原生组件合成和 WebCodecs 编码路径，普通画面在快速导出时按目标尺寸缩放。
- 验证：`node --test third_party/FableCut/test/*.test.js` 96 项通过；`node --check third_party/FableCut/app.js` 与 `git diff --check` 通过。

## 2026-09-18 恢复 0.6.0 原生导出面板

- 目的：根据 0.6.0 操作记录恢复已完成实机测试的 Edward 原生导出流程，不再把 FableCut 内部编码弹窗作为主导出界面。
- 依据：`codex/edward-0.6.0` 的 `src/desktop/qml/Workbench.qml` 与 2026-09-13 导出记录；该版本面板包含文件名、分辨率、帧率、格式、质量、导出目录、进度和取消。
- 结果：当前 0.7.0 原生标题栏“导出”打开同款 Qt 面板；选项通过 `pendingFablecutExportPath`、输出规格和 `startChosenExport()` 交给当前 FableCut 实际渲染/编码链路，保留组件直出，不引入中间转换层。
- 验证：`cmake --build build-0.7.0 --target edward_app -j 4` 通过；需在重启后的 Edward.app 中以原生标题栏“导出”检查面板和实际文件落盘。

## 2026-09-18 修复原生导出面板启动语法错误

- 根因：新增导出目录布局时在 QML 对象声明中使用了不被 Qt QML 接受的行内分号，导致 `Workbench.qml:130` 加载失败，Edward.app 启动后立即退出。
- 修复：将导出目录 `ColumnLayout` 展开为标准 QML 子对象声明。
- 验证：重新构建 `edward_app` 并启动实际 Edward.app，当前进程已建立。

## 2026-09-18 导出面板风格与引擎初始化修复

- 根因：原生导出面板未设置 Edward 深色圆角背景；Qt 直接调用 FableCut 导出函数时跳过引擎初始化，可能触发浏览器原生 JavaScript Alert。
- 修复：面板改用 Edward 深色圆角背景；新增 Qt 专用导出入口，先执行 FableCut 引擎探测和编码配置初始化，再启动实际导出，避免直接弹出浏览器 Alert。
- 验证：`node --check third_party/FableCut/app.js`、`cmake --build build-0.7.0 --target edward_app -j 4` 通过；实际 Edward.app 已重启。

## 2026-09-18 导出面板回归修复

- 修复：原生导出面板收紧为 Edward 弹窗的紧凑尺寸和圆角边距；导出目录改为写入 `paths/exportRoot`，下次打开从本地设置回填。
- 修复：Qt 导出入口不再触发旧的浏览器重名确认；仍使用 FableCut 的实际引擎初始化与导出链路。
- 修复：原生 SVG/React 组件导出合成允许描边溢出，避免圆角描边被 SVG 边界裁切；React 组件继续通过直接 SVG 层合成到输出帧。
- 验证：FableCut 前端测试通过，`app.js`、`component-runtime.js` 语法检查通过，CMake 构建通过，实际 Edward.app 已重启。

## 2026-09-18 原生导出面板视觉样式修正

- 修复：移除 Qt 默认黑色标题区域，改为 Edward 深色面板内的标题；输入框、下拉框、取消按钮和橙色开始导出按钮统一圆角、深灰底色、浅色文字和紧凑间距。
- 验证：重新构建并启动实际 Edward.app，当前进程已建立。

## 2026-09-18 透明视频导出

- 结果：导出格式新增“透明 MOV（ProRes 4444）”，使用 PNG 帧输入、`yuva444p10le` 和 ProRes 4444；透明模式合成帧不绘制不透明预览底图，保留组件 Alpha。
- 验证：FableCut 脚本语法检查、前端测试和 `edward_app` 构建通过；实际 Edward.app 已重启。

## 2026-09-18 导出面板紧凑字号与底部间距

- 修复：导出标题、字段标签、输入框、下拉列表和选择文件夹按钮统一为紧凑字号；底部取消/开始导出按钮保留面板下边距。
- 验证：`cmake --build build-0.7.0 --target edward_app -j 4` 通过；实际 Edward.app 进程已重启，启动日志无 QML 语法错误。

## 2026-09-18 导出、登录与 React 组件回归修复

- 修复：导出 footer 改为显式容器并增加底部内边距；导出冲突检查改用弹窗输入的文件名，服务端保留 `_1`、`_2` 等安全后缀，桌面下载使用服务端解析后的文件名。
- 修复：登录检测不再因“无订阅”清除有效会话；登录按钮在有效会话下显示青色背景，继续保留后台刷新检测。
- 修复：React 组件导出前等待 React 提交完成，并直接抓取其 SVG 层。
- 验证：FableCut 96 项测试通过；`node --check`、`git diff --check`、`edward_app` 构建通过；实际 Edward.app PID 49099 持续运行。

## 2026-09-18 导出冲突确认与组件统一定位

- 修复：底部导出按钮改为紧凑圆角样式并增加真实底部间距；文件冲突时先由 Edward 弹窗让用户选择“覆盖”或“返回修改”，不再自动替用户改名。
- 修复：React/SVG 组件记录统一的画布坐标、尺寸和旋转数据，预览与输出合成均按项目画布坐标换算，避免 Card 6 等组件因显示缩放改变位置。
- 验证：FableCut 96 项测试通过；四个相关脚本语法检查、`git diff --check` 和 `edward_app` 构建通过；实际 Edward.app PID 51580 持续运行。

## 2026-09-18 统一画布中心坐标系

- 约定：画布中心为 `(0, 0)`；水平右为正、左为负；垂直上为正、下为负。标尺、渲染、预览组件、命中测试、拖拽和导出合成均使用同一换算。
- 修复：媒体/文字/组件渲染与编辑拖拽的 Y 轴方向不一致；标题上下预设和循环/转场上下动作同步改为中心坐标语义；React/SVG 组件输出使用世界坐标，不再依赖显示缩放后的 DOM 位置推断。
- 验证：FableCut 96 项测试通过；脚本语法检查、`git diff --check`、CMake 构建通过；实际 Edward.app PID 53053 持续运行。

## 2026-09-18 坐标合同覆盖属性检查器与插入流程

- 修复：属性检查器为原生和普通组件统一提供 Position X/Y，编辑值直接使用中心坐标；时间线/画布插入初始位置通过 `canvasToWorld` 写入，拖拽与渲染保持同一符号约定。
- 修复：组件导出定位继续使用画布相对坐标换算，避免只依赖显示层 DOM 尺寸；补充坐标合同回归断言。
- 验证：FableCut 96 项测试通过；脚本语法检查、`git diff --check`、CMake 构建通过；实际 Edward.app PID 54244 持续运行。

## 2026-09-18 固化统一画布坐标规范

- 结果：将“画布中心为 0，右/上为正，左/下为负”的坐标合同写入仓库 `AGENTS.md`、属性检查器设计规范、Web Runtime Host 合同、FableCut `AGENTS.md` 和主手册 `CLAUDE.md`。
- 约束：后续位置字段、属性检查器、时间线插入、关键帧、预览、命中测试、组件运行时和导出必须遵循该合同，并为新增位置逻辑补充中心点及正负方向回归测试。

## 2026-09-19 诊断日志细化到位置与属性

- 修复：日志刷新时生成结构化诊断快照，记录画布尺寸与中心坐标合同、播放头和选择状态、预览/导出规格、每个时间线素材的轨道与时间位置、原始属性、当前计算属性、包围盒、关键帧、入场/出场转场和循环动画状态。
- 上报：错误反馈同时提交原始桌面日志和 `snapshot` 结构化数据，便于按素材、属性和坐标定位预览/渲染差异；密钥、令牌、密码和授权字段统一隐藏，媒体二进制与源地址不上传。
- 验证：FableCut 96 项测试、脚本语法检查和 `git diff --check` 通过；桌面包已重建，实际 Edward.app PID 57993 持续运行。

## 2026-09-19 诊断日志补齐时间线、渲染、插件、偏好与 AI 摘要

- 增加：记录时间线素材/组件添加、删除和属性编辑；关键帧增删、插值修改及求值次数/耗时；快速导出、WebCodecs 和实时导出的引擎、帧数、状态与耗时。
- 增加：记录插件挂载、更新和错误；偏好读取/写入计数；AI 请求仅记录提示词长度、快照大小和成功状态，不记录原始对话内容。
- 约束：事件保留最近 300 条，路径、密钥、令牌、密码和授权字段脱敏；详细属性仍通过结构化快照提交。
- 验证：FableCut 96 项测试、三个脚本语法检查、`git diff --check` 和 `edward_app` 构建通过；实际 Edward.app PID 59635 持续运行。

## 2026-09-22 AI 剪辑助手执行链路与本地密钥存储约束

- 修复：AI 助手实际读取设置中的 endpoint、protocol、model 和 API Key；普通问答保持对话输出，剪辑请求必须解析为 `edward.action-plan.v1` 后进入待确认状态，不自动修改项目。
- 修复：确认应用动作计划时由前端执行器完成目标、版本、轨道冲突和可编辑属性校验，并纳入可撤销事务；撤销只恢复该次事务快照。
- 修复：发送给模型的项目快照补齐项目版本、画布规格、时间线片段、轨道、起止时间、时长、位置/属性、关键帧、资源 runtime/entry/Target 与可验证资源 ID，避免模型脱离当前工程事实生成计划。
- 约束：API Key 按用户明确要求仅保存在本机明文 `settings.ini` 文件中，不进入 macOS Keychain、Windows Credential Manager 或云端；桌面端、会话存储和启动初始化均使用同一 `QStandardPaths::AppConfigLocation/settings.ini`，测试可用 `EDWARD_SETTINGS_PATH` 覆盖；日志只记录请求长度、快照大小和结果，不记录密钥及原始对话。
- 验证：`node --test third_party/FableCut/test/visual-layout.test.js`、AI 前端测试、`node --check third_party/FableCut/app.js`、CMake AI/桌面目标构建通过。
- 加固：模型即使返回带 `json` Markdown 代码围栏的计划，也只去除外层围栏后交给同一套严格解析与版本校验；围栏之外的自然语言不会被当作可执行指令。

## 2026-09-22 WebChannel 注入与单模型默认值修复

- 根因：`QWebEngineScript::setSourceUrl` 只设置脚本来源信息，不会向页面注入 `qwebchannel.js` 的内容；因此 Edward.app 中的 FableCut 页面缺少 `QWebChannel`，AI 助手与模型设置桥接都被误判为非桌面环境。
- 修复：启动时从 `:/qtwebchannel/qwebchannel.js` 读取脚本并通过 `setSourceCode` 在文档创建阶段注入，确保页面装载时 WebChannel 可连接。
- 修复：拉取结果只有一个模型时，将该模型和当前供应商配置立即保存到本机 `settings.ini`，同时更新默认模型卡片和 AI 助理供应商标签。
- 验证：AI 桥接、设置 UI 与桌面启动测试通过；重新构建并重启当前开发测试版 Edward.app。
