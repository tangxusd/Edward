# FableCut 左栏导航标签

## 2026-09-12

- 目的：将九个导航 Tab 改为横向排列，并支持超出宽度时左右移动。
- 涉及文件：`third_party/FableCut/index.html`、`third_party/FableCut/style.css`、`third_party/FableCut/app.js`。
- 结果：九项 Tab 单行横向显示；最右侧提供左右移动按钮，按内容溢出状态自动禁用/启用；保留激活态和原有数据切换逻辑。
- 验证：`node --check third_party/FableCut/app.js` 通过；服务可访问；已启动 Qt 应用。

## 2026-09-12（导入 Tab）

- 目的：移除原资源栏标题，将导入操作集中到“导入”Tab。
- 结果：原资源栏顶部已移除；首页改名为“导入”；标题、组件、调整层、导入、URL 操作移入导入 Tab；九个 Tab 图标与文字上下居中，间距收拢。
- 验证：`node --check third_party/FableCut/app.js` 通过；服务与 Qt 应用已重启。

## 2026-09-12（卡片与监视器修正）

- 结果：资源卡片固定每行 3 个；“+”按钮固定在卡片右下信息行；导入 Tab 空状态提示水平/垂直居中；移除监视器灯泡按钮并统一右侧文字垂直居中。
- 验证：服务与 Qt 应用已重启。

## 2026-09-12（导入空状态与播放栏）

- 结果：导入空状态提示改为容器水平/垂直双向居中；监视器底部播放控制行压缩内边距与按钮高度，减少占用空间。
- 验证：服务与 Qt 应用已重启。

## 2026-09-12（预览与 4K）

- 结果：导入空状态提示居中；卡片列数改为响应式 1–4 列，默认 3 列；隐藏预览画面上的导出 Frame 框；增加 16:9 与 9:16 4K 画幅；底部时间码改为 10px 非粗体。
- 验证：`node --check third_party/FableCut/app.js` 通过；服务与 Qt 应用已重启。

## 2026-09-12（卡片操作与监视器控件）

- 结果：资源卡片的“+”按钮移至右下角，与大小信息同一行；移除“完整画布”和“画面框”控件；监视器右侧控件保持紧凑并垂直居中。
- 验证：`node --check third_party/FableCut/app.js` 通过；服务与 Qt 应用已重启。

## 2026-09-12（图标边框与箭头对齐）

- 结果：移除所有 Tab 图标容器外框；导入 SVG 蓝色路径改为灰色，底部橙色路径保留；左右按钮底部对齐并下移。
- 验证：服务与 Qt 应用已重启。

## 2026-09-12（Tab 与监视器紧凑化）

- 结果：Tab 宽度与间距收拢；素材内容改为卡片网格；节目监视器右侧按钮、下拉框和文字统一为 11px/24px 紧凑尺寸。
- 验证：`node --check third_party/FableCut/app.js` 通过；服务与 Qt 应用已重启。

## 2026-09-12（导航 SVG 规范）

- 结果：九个导航 Tab SVG 图标线宽统一为 1px，尺寸缩小 20%。
- 验证：服务与 Qt 应用已重启。

## 2026-09-12（导入 Tab 内嵌与箭头定位）

- 结果：导入按钮改为直接嵌入“导入”Tab；其他图标容器去除外框；左右移动按钮固定在 Tab 文字区域左右角。
- 验证：`node --check third_party/FableCut/app.js` 通过；服务与 Qt 应用已重启。

## 2026-09-12（导入图标与移动按钮）

- 结果：导入按钮仅在导入 Tab 的操作区显示；导入图标替换为附件提供的双色 SVG 且无外框；左右移动按钮移至 Tab 行下方并保持可见。
- 验证：`node --check third_party/FableCut/app.js` 通过；服务与 Qt 应用已重启。

## 2026-09-12（原始 SVG 与按钮位置）

- 结果：导入按钮仅在“导入”Tab 显示；导航图标按附件 HTML 的 SVG 风格重建；左右移动按钮缩小并移至 Tab 行下方。
- 验证：`node --check third_party/FableCut/app.js` 通过；服务与 Qt 应用已重启。

## 2026-09-12（导入显示与图标尺寸）

- 结果：导入按钮仅在“导入”Tab 显示；九个图标放大一倍；左右移动按钮缩小并固定在右侧；Qt 初始窗口尺寸改为正常值，消除启动时 1px 放大闪烁。
- 验证：`node --check third_party/FableCut/app.js` 通过；服务与 Qt 应用已重启。

## 2026-09-12（滚动与导入收敛）

- 结果：仅保留“导入”操作按钮并固定在 Tab 顶部；图标/文字恢复上下布局；左右按钮改为固定控制位，通过平移 Tab 内容工作，不再随内容移动或消失。
- 验证：`node --check third_party/FableCut/app.js` 通过；服务与 Qt 应用已重启。

## 2026-09-12（预览铺满与音频电平紧凑化）

- 结果：有素材时资源卡片保持左上网格排列，空状态继续水平/垂直居中；监视器适配模式让预览画布尽可能铺满并保留滚轮缩放；电平格缩小为 3×3px 并增加 6dB 响应增益；监视器不显示导出帧灯泡按钮。
- 验证：`node --check third_party/FableCut/app.js` 通过；HTTP 服务和 Qt 应用已重启。

## 2026-09-12（资源卡片对齐与电平尺寸回调）

- 结果：修正隐藏空状态仍触发居中布局的问题；有内容时卡片从左上开始排列。音频电平格恢复为上一版两倍尺寸（5×5px）。
- 验证：`node --check third_party/FableCut/app.js` 通过；Qt 应用已重启。

## 2026-09-12（活动音轨与监视器底色）

- 结果：电平表仅创建实际含音频片段的轨道，不再显示空置 A1/A2/A3/A4；画幅外区域使用深灰色；时间线缩放滑块统一为紧凑橙色滑块样式。
- 验证：`node --check third_party/FableCut/app.js` 通过；服务与 Qt 应用已重启。

## 2026-09-12（播放头、标记与剪映快捷键）

- 结果：移除时间尺上的重复下五边形，仅保留 CSS 播放头顶标记；时间尺标记点改为蓝色。设置中加入 Edward/剪映快捷键布局选项，并接入剪映 Cmd/Ctrl+B 分割、Q/W 修剪逻辑。
- 验证：`node --check third_party/FableCut/app.js` 通过；服务与 Qt 应用已重启。

## 2026-09-12（Qt 页面缓存刷新）

- 结果：为 Qt 内嵌 FableCut 地址增加版本参数，强制刷新 WebView 缓存，确保标题栏连接状态使用最新逻辑。
- 验证：CMake 构建成功；Qt 应用已重启。

## 2026-09-12（原生标题栏连接状态与启动最大化）

- 结果：原生 macOS 标题栏改为根据本地服务启动状态显示绿色/橙色圆点与状态文字；窗口在 QML 首帧即进入最大化，避免启动时普通窗口闪现。
- 验证：CMake 构建成功；Qt 应用已重启。

## 2026-09-12（本地服务可达性判定）

- 结果：原生标题栏不再只依据 Qt 子进程状态判断连接；当已有本地服务占用 7777 端口时，通过 TCP 可达性确认并显示绿色状态。
- 验证：CMake 构建成功。

## 2026-09-12（Supabase 资源库基础设施）

- 结果：初始化并关联 Supabase 项目 `naybqwiqgviuzjtemerc`；创建资源分类、资源、版本、收藏、浏览和权限表，配置索引与 RLS；部署 `resource-catalog`、`resource-detail`、`resource-favorite` Edge Functions。
- 验证：`supabase db push --linked --yes` 成功；三个 Edge Functions 部署成功。部署过程未写入 service role key。
## 2026-09-12 Supabase 资源浏览器接入

- 目的：将“导入”之外的素材 Tab 切换到 Supabase 分类与资源目录，同时保留导入 Tab 的本地项目素材流程。
- 涉及文件：`third_party/FableCut/index.html`、`third_party/FableCut/style.css`、`third_party/FableCut/app.js`、`third_party/FableCut/server.js`。
- 结果：新增两级分类导航、收藏/热门/最新排序、分页加载和仅缓存资源元数据的本地缓存；本地代理转发浏览器 Authorization，不保存服务端密钥。
- 验证：`node --check third_party/FableCut/app.js`、`node --check third_party/FableCut/server.js` 通过；未配置 Supabase 环境变量时接口返回 `503 supabase_not_configured`，未向匿名请求开放资源数据。

## 2026-09-12 Supabase 资源录入管道

- 目的：提供仅供本地/Codex 调用的资源发布 CLI。
- 涉及文件：`tools/resource-publisher/publish.mjs`、`tools/resource-publisher/manifest-schema.json`、`tools/resource-publisher/test/publish.test.mjs`、三个 Edge Function。
- 结果：发布前校验组件 ID、一级 Tab、三级分类 UUID、版本号；上传 manifest 与包文件并按 SHA-256 记录版本；服务角色密钥只从环境变量读取，不写入项目。
- 验证：`node --test tools/resource-publisher/test/publish.test.mjs` 通过；三个函数重新部署成功；CORS 已限制到本地 FableCut 来源。
- 补充：已创建私有 `resource-packages` Storage bucket，并推送 `202609120002_resource_storage.sql`；仅 authenticated 可读、service role 可写。
- 预览：目录函数为已发布资源生成 60 秒私有视频签名 URL；卡片仅在鼠标悬停时播放，移出即暂停并回到起点；已移除装饰性上浮效果。演示资源模式改为显式开启（`fablecut-demo-auth=1`），默认不替代 Supabase 数据。
- 账号：本地服务增加 `/api/auth/login`、`/api/auth/signup`、`/api/auth/refresh` 代理，密码仅转发至 Supabase Auth，不写入本地或日志；订阅权限继续由 `entitlements` RLS 表控制，前端登录界面接入仍需下一步完成。
- 认证订阅实施：已推送 `202609120003_auth_subscription.sql`，部署 `auth-register`、`auth-login`、`auth-entitlement`、`subscription-catalog`、`subscription-create-order`、`subscription-cancel`、`referral-credit`。注册/登录按账号名或邮箱解析，服务端限流；订单金额由套餐与规则快照计算，推荐奖励通过 service-role 内部函数幂等发放。
- Qt 认证：Qt 壳认证客户端已改为调用 `auth-login`/`auth-register` Edge Functions，账号名解析和注册限流不再可被客户端直连绕过；登录请求测试和 Qt 构建通过。
- 会话：Qt `AuthSessionStore` 现在持久化用户 ID、账号名及轮换 access/refresh token，退出登录会清除认证命名空间；认证客户端和会话存储测试通过。
- 资源授权：目录、详情和收藏函数统一检查服务端有效试用/订阅及周期结束时间；无有效权益返回 `403 entitlement_required`，已重新部署三个函数。
- Qt 权益：登录后由 Qt 壳调用 `auth-entitlement`，显示试用/订阅状态、到期日期和推荐抵扣余额；构建及认证相关测试通过。
- Qt 会话续期：登录会话保留 refresh token，并在 Qt 壳运行期间每 10 分钟调用 Supabase Auth token 接口轮换 access/refresh token，避免长时间运行时因 access token 过期而失效；构建及认证相关测试通过。
