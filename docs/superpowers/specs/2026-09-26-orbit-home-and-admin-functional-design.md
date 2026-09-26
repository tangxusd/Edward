# Orbit 首页与管理后台功能设计

日期：2026-09-26  
状态：已确认设计方向，待用户审阅规格后进入实现计划

## 1. 范围与边界

本规格同时覆盖两个独立但共享数据合同的子系统：

1. Orbit 桌面端初始页：应用启动后先进入首页，不直接打开编辑器。
2. 管理后台：仅锁定功能信息架构、数据字段、权限和 API 合同，不另做自定义视觉设计；前端采用用户指定的 Ant Design Pro 项目（当前上游 README 标注 v6，基于 React 19、Umi Max 4、Ant Design 6、Pro Components）。

本阶段不实现生产部署、支付规则变更、历史数据迁移执行和视觉稿以外的 UI 重构。所有 Supabase/Cloudflare 持久化设置必须存在对应的后台管理入口和审计记录。

## 2. 已确认的视觉方向

### 2.1 Orbit 首页

- 仅使用深色方案，沿用现有深色底色与橙色强调色。
- 去除系统标题栏，窗口内容承担完整应用边界；保留系统级窗口关闭、退出和快捷键可达性。
- 主内容保持屏幕中央聚焦，整体视觉重量约占屏幕五分之一，外围保持大面积留白。
- 采用左右布局：左侧固定账户/订阅与设置入口，右侧承载创建项目和历史项目。
- 首页默认焦点在右上“创建项目”。

### 2.2 管理后台

- 不复制 Orbit 首页视觉，不增加自定义视觉设计。
- 使用 Ant Design Pro 标准布局、菜单、统计卡片、趋势图、ProTable、表单、抽屉、权限路由和反馈组件。
- 默认进入 Home，采用标准左右分栏：左侧功能导航，右侧业务内容。

## 3. 桌面端首页功能

### 3.1 页面区域

左栏从上到下：

- 账户入口：未登录显示登录；已登录显示用户名/邮箱、订阅状态、到期时间。
- 订阅入口：订阅、续订、权益和支付状态，调用现有 Supabase Auth/entitlement 合同。
- 首页入口：固定选中。
- 设置入口：打开现有设备/模型/偏好设置，不在首页复制设置项。

右栏从上到下：

- 创建项目主按钮：创建空项目、选择模板或导入项目文件；成功后进入编辑器。
- 历史项目网格：卡片展示项目名称、预览图、文件大小、时长和最近打开时间。
- 回收站入口：位于历史项目区域右下角；显示待清理数量。

### 3.2 历史项目操作

- 左键卡片：打开项目；加载失败时保留首页并显示可诊断错误。
- 右键仅对项目卡片启用：复制、删除、重命名。
- 页面其余位置阻止应用自定义右键菜单，并放行系统/浏览器级菜单不可用的桌面行为。
- 删除先进入回收站；回收站支持恢复和彻底删除。彻底删除必须二次确认，并删除项目包、预览缓存和索引记录。
- 项目索引与项目包均使用原子写入和版本字段，避免启动中断造成半写状态。

### 3.3 数据合同

```text
ProjectSummary {
  id: string
  name: string
  previewPath: string | null
  packagePath: string
  sizeBytes: integer
  durationSeconds: number
  updatedAt: string
  deletedAt: string | null
}
```

项目首页只读取摘要；编辑器继续读取原项目合同，不能由首页重写已有项目内容。回收站删除采用单独的 tombstone/删除时间，不复用项目名称判断状态。

## 4. 管理后台功能信息架构

### 4.1 Home 数据纵览

- 统计卡片：新增用户数、意见反馈数、订阅金额、续订金额。
- 趋势图：上述指标按天聚合，默认当天，可切换 7 天/30 天。
- 服务监控：Supabase Auth、Database、Functions、Storage 与 Cloudflare Workers/Pages 的调用量、错误量、延迟和限额状态。
- 缓存监控：边缘命中率、缓存条目数、缓存大小、失效/回源次数、最近刷新时间。
- 所有监控数据标注采集时间、数据源和“实时/延迟”状态；采集失败不能伪造为 0。

### 4.2 用户管理

- ProTable 分页，每页 50 人；支持用户名和邮箱搜索、排序、筛选和详情抽屉。
- 详情合并 auth.users、profiles、entitlements、subscription_records、device_bindings、redemption_events、referral_records、audit_events 等关联数据。
- 管理动作：修改可编辑资料、触发重置密码、解绑设备、下发兑换码、加入/移出黑名单。
- 高风险动作必须二次确认、记录操作者、原因、request_id、目标用户和结果；后台不直接读取或显示密码、refresh token、服务密钥。

### 4.3 资源库管理

- 将“目录”改名为“资源库管理”。
- 支持最多三级分类：根分类 → 二级分类 → 三级分类；禁止循环父子关系和删除仍有资源/子分类的节点。
- 分类字段：名称、slug、描述、排序、状态、封面、父分类、版本、更新时间。
- 资源包字段继续复用现有 resource library 合同：target、manifest、版本哈希、包地址、状态、审计记录。

### 4.4 兑换码

- 将“兑换码批次”改名为“兑换码”。
- 支持创建批次、生成码、设置面额/权益、有效期（默认 7 天）、批次状态。
- 列表显示总量、已使用、未使用库存、过期数；支持查询使用记录、用户、时间和结果。
- 兑换码只存不可逆摘要或加密值；后台回显时不显示完整明文。

### 4.5 默认偏好

- 将“偏好事实”改为“默认偏好”。
- 它是本地偏好属性的云端默认映射，不覆盖用户已确认的本地事实。
- 支持按组件、语义路径、属性类型编辑默认值；保存时校验 manifest、类型、范围和版本。
- 修改必须产生版本和审计事件，客户端同步时显式触发并重新校验。

### 4.6 颜色与字体

- 颜色：常用颜色的添加、修改、停用、排序、预览，字段含名称、色值、透明度、标签和状态。
- 字体：字体元数据、预览文字、安装包、SHA-256、授权信息、状态和版本；上传前校验扩展名、内容类型、大小和哈希。
- 两者均迁移到 Cloudflare 静态资源/边缘缓存，Supabase 保存元数据和权限事实；客户端通过版本哈希缓存，不能静默使用旧包。

### 4.7 大模型模板

- 管理供应商、模型、默认参数、能力标签、启用状态和排序；供应商列表与桌面端设置保持同一数据合同。
- 密钥只存 Cloudflare/Supabase Secret，不进入列表、日志、导出备份或客户端响应。
- 默认参数必须有 schema、范围和版本，保存前进行 provider-specific 校验。

### 4.8 邀请码与激励

- 管理邀请码、有效期（默认 7 天）、邀请激励方案和抵扣方案。
- 支持按用户名、邮箱、邀请码查询邀请记录，按邀请数量排序。
- 奖励发放使用现有事务 RPC/幂等合同，后台只能调用受控管理接口，不直接改余额事实。

### 4.9 订阅方案

- 月/季/年三个周期分别维护原价、折扣价、宣传图、权益、启用时间和状态。
- 订阅/续订引用稳定 plan_key 和版本，不因后台编辑覆盖历史订单快照。
- 发布、停用、价格变更必须审计；宣传图走 Cloudflare 资源地址并带哈希。

### 4.10 数据备份与恢复

- 支持手动导出本地备份和从本地备份恢复。
- 备份包含版本、导出时间、数据范围、哈希和脱敏后的可恢复数据；不包含密钥、密码、refresh token。
- 恢复前执行 schema/version 校验、预览差异、二次确认和自动回滚点；恢复过程写审计日志。

## 5. Cloudflare 与 Supabase 边界

- Cloudflare Workers：管理 API 聚合、缓存控制、监控读取、静态资源/字体/颜色包分发；不持有 Supabase service role 到客户端。
- Supabase：认证、关系数据、RLS、事务 RPC、审计事实和订阅/兑换/邀请业务事实。
- Admin 使用短时管理员会话、严格 origin、CSRF/request_id、最小权限函数；所有写操作以用户身份和管理角色双重校验。
- Cloudflare 缓存键包含资源类型、版本和内容哈希；失效由版本发布或明确 purge 触发。

## 6. 验收标准

- 启动默认进入 Orbit 首页；不直接进入编辑器。
- 首页非卡片区域右键不会显示应用菜单；卡片右键操作可恢复/撤销并有测试覆盖。
- 首页能读取空项目、历史项目、已删除项目和损坏索引四种状态。
- 管理后台默认 Home，所有模块可由导航进入，功能设计不依赖自定义视觉组件。
- 统计时间范围、用户分页/搜索、资源三级分类、兑换码库存、默认偏好、颜色/字体、大模型模板、邀请码、订阅方案、备份恢复均有 API/schema/权限/审计测试。
- Cloudflare 缓存与 Supabase 数据版本一致，密钥和敏感认证数据不会出现在客户端、日志或备份中。

## 7. 依赖与未决项

- Ant Design Pro 上游当前 README 标注 v6.0.3，最终落地前锁定具体 commit/依赖版本并执行官方 lint、typecheck、unit/e2e 检查。
- “去除系统标题栏”与项目旧窗口规范冲突，本规格按本次最新明确需求采用无系统标题栏；需在实现计划中补充 macOS/Windows 窗口拖拽、缩放、关闭和无障碍键盘行为测试。
- 管理后台 API 的 Cloudflare Worker 路由与现有 Supabase Functions 需要在实现计划中逐模块映射，不能先创建无数据源的空页面。

## 8. 业务桥接审查与补充约束

本节是实现前的阻塞审查结果。当前仓库事实表明，不能只把现有管理页面换成 ProLayout：桌面端和后台都需要新增明确的桥接层。

### 8.1 当前事实与缺口

| 业务链路 | 当前已有 | 阻塞缺口 | 实现前必须补齐 |
| --- | --- | --- | --- |
| 桌面首页 → 历史项目 | `Workbench.qml` 直接加载 `Workbench.qml`，运行时有项目加载/恢复能力 | 没有 `ProjectSummary` 索引服务、首页路由、卡片右键命令或回收站持久化 | C++ `ProjectHomeStore` + QML 首页模型 + 原子索引/回收站合同 + 单元/启动测试 |
| 首页账户 → Supabase | `WorkbenchRuntime` 已有登录、entitlement、偏好同步 | 首页没有账户摘要桥接和登录失败/过期状态合同 | 复用 `AuthSessionStore`，新增 `accountSummary` 只读 DTO，禁止首页自行拼接 token |
| 后台 → Supabase | 仅有 `admin-stats/users/catalog/redemption-batch/preferences` 五个通用只读入口 | 通用 endpoint 没有写操作、分页、趋势、监控或动作 API；当前页面仍使用“目录/兑换码批次/偏好事实”旧命名 | 按模块建立显式 query/mutation 合同，前端只调用白名单路由，不允许以资源名拼接任意写路径 |
| 后台 → Cloudflare | 仓库只有部署记录，没有 Worker 源码和路由 | 规格要求 Worker 聚合、缓存、监控和资源分发，但没有可部署实现 | 新增 Worker 项目、版本化路由、Supabase service binding/secret 配置、Wrangler 本地测试和部署门禁 |
| 用户管理 | `profiles`、订阅/订单/审计事实存在 | `auth.users` 详情不能通过普通 PostgREST 关系查询；密码重置、解绑设备、黑名单动作与设备表/接口未形成合同 | 使用受控 Edge Function/GoTrue Admin API；新增 `device_bindings`、黑名单事实和动作审计，禁止客户端直连 service role |
| 资源库管理 | `resource_categories` 支持 `parent_id`，资源/版本合同已存在 | 数据库没有三级深度约束；现有 `admin-catalog` 只读颜色目录，不是资源分类 CRUD | 用事务函数校验父链深度≤3、循环和删除约束；分类、资源、版本发布分别定义读写 endpoint |
| 兑换码 | 批次、码摘要、兑换记录和幂等 RPC 存在 | 没有后台创建/生成/撤销/库存聚合 API；默认 7 天不是数据库默认值；明文码只允许一次性返回 | 生成接口一次性返回明文并只持久化 digest；库存/使用记录分页查询；服务端默认 `expires_at = now()+7 days` 并审计 |
| 默认偏好 | `preference_facts` 是用户事实 | 没有云端默认偏好表、版本、manifest 校验和发布状态 | 新增 `preference_defaults`/版本合同；默认值写入必须过组件 schema 校验，客户端同步按版本和用户事实优先级合并 |
| 颜色/字体 | `color_catalog_*`、`font_catalog_*` 迁移和发布 manifest RPC 已存在 | 没有后台编辑/上传 endpoint，也没有 Cloudflare object upload/purge 合同 | 草稿→校验→发布事务；上传使用预签名/受控 Worker；对象 key、SHA-256、MIME、授权和 purge 结果写审计 |
| 大模型模板 | 桌面端有供应商/模型设置读取路径 | 没有模板表、provider 参数 schema、密钥 secret 引用和后台 CRUD | 新增模板/供应商版本表和校验函数；客户端只拿脱敏配置，密钥永不进入响应/备份/日志 |
| 邀请码 | `profiles.referral_code`、`referral_clicks`、奖励 RPC 存在 | 没有邀请方案版本、抵扣方案、有效期管理和后台查询聚合；当前奖励默认有效期为 365 天，不是规格的 7 天 | 明确“邀请码有效期”和“奖励有效期”两个字段；新增方案版本、邀请查询索引和只读统计 endpoint，不能直接改 credit ledger |
| 订阅方案 | `subscription_plans`、orders、subscriptions、periods 存在 | 没有宣传图/折扣价版本合同和后台发布接口；历史订单快照边界未落实 | 增加 plan revision/marketing asset 合同；月/季/年作为独立周期快照，发布/停用走事务和审计 |
| 备份恢复 | 目前只有偏好本地导入/导出 | 没有全局备份格式、加密、差异预览、事务恢复和回滚点 | 定义脱敏备份 schema、版本/哈希、加密封装、导入预检和后台恢复任务；禁止直接覆盖生产表 |

### 8.2 后台 API 分层合同

后台前端统一调用 Cloudflare Worker `/admin/v1/*`，Worker 再调用受控 Supabase Functions/RPC；不允许前端直接访问 service role 或任意 PostgREST 表。

```text
GET  /admin/v1/home/overview?range=today|7d|30d
GET  /admin/v1/home/monitoring
GET  /admin/v1/users?cursor=&limit=50&q=&sort=
GET  /admin/v1/users/:id
POST /admin/v1/users/:id/actions/reset-password|unbind-device|issue-code|blacklist
GET/POST/PATCH /admin/v1/resource-categories
GET/POST/PATCH /admin/v1/resources
GET  /admin/v1/redemption-codes?status=&cursor=&limit=50
POST /admin/v1/redemption-codes/batches
POST /admin/v1/redemption-codes/:batchId/revoke
GET/POST/PATCH /admin/v1/preference-defaults
GET/POST/PATCH /admin/v1/colors
GET/POST/PATCH /admin/v1/fonts
GET/POST/PATCH /admin/v1/llm-templates
GET/POST/PATCH /admin/v1/invitation-rules
GET/POST/PATCH /admin/v1/subscription-plans
POST /admin/v1/backups
POST /admin/v1/backups/validate
POST /admin/v1/backups/restore
GET  /admin/v1/audit-events
```

每个响应必须包含 `requestId`、`schemaVersion`、`data` 或结构化 `error`；列表必须返回 `nextCursor`，不能依赖 offset 在高并发下分页漂移。所有 mutation 必须接收幂等键，返回审计事件 ID。

### 8.3 桌面首页桥接合同

新增 `ProjectHomeStore`，职责只包含摘要索引、回收站和项目动作，不读取或改写编辑器内部时间线状态。

```text
list(includeDeleted: bool) -> ProjectSummary[]
open(projectId) -> ProjectOpenResult
create(request) -> ProjectOpenResult
duplicate(projectId) -> ProjectSummary
rename(projectId, name) -> ProjectSummary
trash(projectId) -> ProjectSummary
restore(projectId) -> ProjectSummary
purge(projectId, confirmation) -> void
```

QML 只消费 `QAbstractListModel` 和上述 invokable 信号；所有路径由 C++ 校验并限制在用户项目根目录。动作失败必须携带稳定错误码，首页不允许通过字符串猜测错误类型。

### 8.4 角色与审计矩阵

- `owner`：角色管理、备份恢复、订阅/奖励规则和所有用户动作。
- `admin`：用户、资源、兑换码、颜色、字体、模板、邀请和订阅方案管理；不能修改管理员角色或执行恢复。
- `analyst`：Home 统计、监控、用户/订单/审计只读；不能下载字体安装包、查看兑换码明文或执行 mutation。
- 每个动作记录 `requestId、actor、role、target、beforeHash、afterHash、result、reason`；敏感值只记录摘要。

### 8.5 桥接验收门

在任何 UI 实现声称完成前，必须先通过以下合同测试：

1. 桌面首页冷启动、空索引、损坏索引、打开/复制/重命名/回收/恢复/彻底删除全链路测试。
2. Worker → Function/RPC 的认证、角色、CSRF、幂等、分页 cursor 和错误映射测试。
3. 用户高风险动作、资源三级分类、兑换码一次性明文、默认偏好版本合并、字体对象上传/缓存失效、模板脱敏、邀请/订阅快照、备份预检/回滚测试。
4. 每个后台导航项至少绑定一个真实 endpoint 和权限断言；禁止只有静态卡片或通用只读表格的“假完成”。

## 9. Cloudflare 静态资源迁移方案（已确认）

用户已确认采用“Supabase 业务源 + Cloudflare 边缘分发”的混合方案。目标是降低字体、颜色、资源库等高频静态内容的读取延迟；不把认证、权限和交易事实迁出 Supabase。

### 9.1 数据边界

- Supabase 继续作为认证、用户个性化偏好、订阅、兑换码、邀请、设备、黑名单、审计和资源权限的唯一事实源。
- Cloudflare R2 保存字体安装包、颜色/默认偏好版本化 JSON、资源预览图、资源包和资源库 manifest；Cloudflare CDN 负责边缘缓存。
- Cloudflare Worker 只做版本化资源读取、权限校验、短期签名 URL、缓存控制和监控聚合，不向客户端暴露 Supabase service role。
- 公共系统默认偏好可以缓存；用户个性化偏好和授权结果不得进入公共缓存。

### 9.2 免费方案边界

按当前 Cloudflare 官方文档，R2 免费额度为每月 10 GB-month 存储、100 万次 Class A 操作和 1000 万次 Class B 操作，互联网出口流量不收费。Workers 免费方案为每日 100,000 次请求，适合轻量读取和缓存，不承载复杂统计或长任务。管理后台静态站点可继续使用 Cloudflare Pages。

额度和价格以发布时官方文档为准：

- [R2 定价](https://developers.cloudflare.com/r2/pricing/)
- [Workers 限制](https://developers.cloudflare.com/workers/platform/limits/)
- [Pages 限制](https://developers.cloudflare.com/pages/platform/limits/)

超出免费额度前必须配置用量监控；不得因额度不足把权限校验改成客户端判断。

### 9.3 版本与缓存合同

- 所有可缓存对象使用包含版本或内容哈希的不可变路径，例如 `/fonts/v2026.09/a.zip`、`/resources/v35/manifest.json`。
- 对象响应返回 `ETag`、`Cache-Control: public,max-age=31536000,immutable`；可变 manifest 只缓存短时间并使用 `ETag` 条件请求。
- 发布顺序为：上传并校验 R2 对象 → 校验 SHA-256/MIME/授权 → 写入 Supabase 版本记录 → 发布新 manifest → 记录缓存刷新结果。
- 缓存未命中时回源 Supabase/受控 Worker；客户端必须校验 `revision`、`fullHash`，不能静默接受旧包。
- 需要订阅权限的资源使用私有 R2 桶和 Worker 短期签名 URL，不使用可猜测的公开对象地址。

### 9.4 迁移范围

第一阶段迁移字体包、颜色库、资源预览图、资源包、资源库版本化清单和公共默认偏好。用户资料、订阅、兑换、邀请、权限和审计不迁移，只通过受控 API 读取。

### 9.5 性能与正确性验收

- 迁移前后分别记录 Supabase 直连、Cloudflare 缓存命中和缓存未命中的 p50/p95 TTFB，使用真实目标地区请求，不以 DNS/Ping 代替 HTTP 测试。
- 记录缓存命中率、回源率、R2 操作量和 Worker 请求量；缓存命中率异常下降必须阻断发布。
- 验证新版本发布、旧版本不可误用、权限资源不泄露、断网重试、回滚和额度接近上限时的降级路径。
- 只有通过资源哈希、权限隔离、缓存一致性和额度监控测试，才能把模块标记为迁移完成。
