# Supabase 资源库与组件属性契约设计

## 目标

将左栏除“导入”外的 Tab 接入 Supabase，提供一级 Tab、二级分类、三级分类和资源浏览；使用本地缓存降低重复请求；通过组件属性契约让属性检查器生成标准化控件；同时保证订阅授权、资源访问和资源录入的安全边界。

## 范围与固定约束

- 一级分类固定对应现有 Tab：媒体、文本、音频、卡片、图表、背景、标注、数字。
- “导入”继续使用本地项目素材流程，不接入远程资源库。
- 资源分类和资源元数据存放在 Supabase；二级、三级分类由管理管道维护。
- 前端不持有 Supabase service role key，不直接写收藏计数或订阅状态。
- 暂不绑定支付平台，先定义通用 `entitlements` 权限接口。

## 页面结构

每个非导入 Tab 使用左右两栏：

- 左栏较窄，显示二级分类列表；顶部固定“收藏、热门、最新”三个入口。
- 右栏显示三级分类下的资源卡片，支持分页、懒加载和本地缓存。
- 一级 Tab 始终由现有左栏 Tab 直接决定 `tab_key`。
- 资源预览、名称、简述和收藏状态在卡片展示；详细内容在用户打开资源时加载。

## Supabase 数据模型

### `resource_categories`

`id`、`tab_key`、`parent_id`、`name`、`slug`、`sort_order`、`status`、`created_at`、`updated_at`。

`parent_id = null` 表示二级分类；有父级的记录表示三级分类。通过 `tab_key` 强制分类只能归属于对应一级 Tab。

### `resources`

`id`、`component_id`、`tab_key`、`category_id`、`name`、`summary`、`detail_markdown`、`status`、`visibility`、`required_plan`、`favorite_count`、`view_count`、`created_at`、`updated_at`、`published_at`。

`component_id` 是稳定 ID，资源更新不改变它。

### `resource_versions`

`id`、`resource_id`、`version`、`content_hash`、`manifest_path`、`package_path`、`preview_image_path`、`preview_video_path`、`file_size`、`mime_type`、`min_app_version`、`compatibility`、`created_at`、`published_at`。

### 用户关系与权限表

- `resource_favorites(user_id, resource_id, created_at)`，主键为 `(user_id, resource_id)`，收藏写入幂等。
- `resource_views(user_id, resource_id, viewed_at)`，通过批量写入去重统计。
- `entitlements(user_id, plan_key, status, starts_at, expires_at, source, updated_at)`，支付平台无关。

数据库默认拒绝直接写入统计和资源文件字段。前端通过 Edge Function 获取经过权限裁剪的结果。

## Storage 与访问安全

- 原始组件包和预览文件放在私有 Bucket，禁止公开 URL。
- Edge Function 验证 JWT、订阅权限、资源状态和版本后，按资源生成短时效签名 URL。
- 列表接口只返回必要元数据；详细介绍和源包地址按需获取。
- 对用户、IP、设备和资源设置频率限制；异常访问写入审计日志。
- 本地缓存只保存元数据、预览和已授权版本，缓存键为 `component_id + version + content_hash`。
- 订阅到期后禁止新授权和新签名 URL；本地已下载内容无法被绝对收回，因此必须设置离线宽限期和再次联网校验。
- 不依赖代码混淆作为安全边界；通过私有存储、短期签名、水印、授权审计降低反编译和盗用风险。

## 请求与缓存策略

- 首次进入 Tab：分类请求一次，资源首屏分页请求一次。
- 分类树、资源元数据采用 stale-while-revalidate 本地缓存。
- 资源卡片预览按可视区域批量申请签名 URL。
- 收藏和浏览记录先写本地队列，再批量提交。
- 不启用普通用户端实时订阅，不按卡片轮询权限。
- 数据库为 `tab_key`、`category_id`、`favorite_count`、`published_at` 建索引。

## 组件属性契约

每个组件包必须包含 `manifest.json`，其中 `inspector.properties` 声明属性 ID、类型、标签、分组、默认值、单位、范围、步进、枚举、是否可动画以及 `mapsTo` 目标路径。

检查器只根据契约生成控件。用户修改写入组件实例 `overrides` 和统一 `keyframes`，不修改原始组件包。输入必须经过类型解析、单位转换、范围限制、步进对齐和未声明字段拒绝；同时保留原始值、解析值、计算值和最终写入值，供浏览器与 Fusion 转换追踪。

## Codex 专用资源录入管道

独立 CLI/管道使用仅保存在环境变量或 CI Secret 中的 service role key，普通应用不可调用。流程为：校验 manifest → 上传原始包与预览 → 计算内容哈希 → 写入版本和分类 → 发布或撤下 → 写入审计记录。支持资源版本替换、预览更新、撤下和回滚。

## 验证标准

- RLS/Edge Function 测试覆盖匿名访问、普通订阅、到期订阅和越权资源 ID。
- 组件列表分页、缓存命中、版本更新和撤下均有可复现测试。
- 属性契约测试覆盖类型、单位、范围、枚举、关键帧和未声明字段拒绝。
- 录入管道测试覆盖哈希重复、错误 manifest、私有 Storage 和版本回滚。
- Qt 内嵌页面验证 Tab 切换、分类筛选、收藏、缓存和权限失败状态。

