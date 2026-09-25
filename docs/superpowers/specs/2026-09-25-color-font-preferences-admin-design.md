# Orbit 颜色、字体、偏好与管理后台设计

## 状态

- 状态：设计已确认，等待实现计划
- 范围：颜色模块、字体模块、默认偏好、兑换码、邀请码、时间线交互、循环动画、Supabase 管理能力和 `admin.edward.uno` 管理后台
- 产品：Orbit（原 Edward）
- 原则：Supabase 是云端事实源；Orbit 本地缓存是编辑会话事实源；项目文件保存已应用的具体值，不依赖在线目录

## 1. 目标与拆分

五个子项目共同推进，但共享一套数据合同和版本规则：

1. 颜色与字体模块：独立目录、缓存、安装、预览和偏好桥接。
2. 新用户默认偏好、兑换码和邀请码奖励。
3. 播放头交互、逐帧定位和循环动画方向。
4. Supabase 管理数据、管理员权限、审计和统计。
5. `admin.edward.uno` 管理后台。

每个子项目都必须可以单独测试和回滚；不得把后台写权限、在线目录访问或字体安装逻辑塞进项目文件或组件运行时。

## 2. 总体架构

```text
Supabase
├─ color_catalog / font_catalog / default_preference_profiles
├─ preference_facts
├─ redeem_codes / redeem_code_redemptions
├─ referrals / credit_ledger / referral_rules
├─ admin_roles / admin_audit_events
└─ Edge Functions：所有写操作、权益、兑换、奖励、后台管理

Orbit 桌面端
├─ ColorPresetModule
├─ FontCatalogModule
├─ PreferenceStore
├─ TimelineInteractionModule
├─ AnimationEvaluator
└─ Preview / Render / Export 共用规范化属性数据

admin.edward.uno
└─ Refine 管理 UI → 管理 Edge Functions → Supabase
```

Refine 只负责管理界面、资源列表、筛选、表单、权限路由和统计图表。它不能持有 `service_role`，不能直接绕过 Edge Functions 修改业务表。管理员身份同时由 Supabase 登录会话和 `admin_roles` 服务端记录决定。

## 3. 颜色模块

### 3.1 统一颜色来源

属性检查器中的文字、线条、边框、背景、阴影、SVG `fill`、SVG `stroke` 和组件声明的颜色属性全部通过 `ColorPresetModule` 提供。模块输出：

```text
colorPresetId
colorValue
colorSpace
alpha
catalogRevision
source
preferenceSubject
```

颜色目录只负责提供可选值和元数据，项目实例保存用户实际应用的色值快照。历史项目不引用未来目录 ID，因此目录更新不会改变历史项目。

### 3.2 读取与缓存

进入项目时执行一次目录版本检查和必要的增量同步，建立项目会话快照；项目会话内属性检查器、预览、渲染和导出只读取本地缓存和会话快照。网络不可用时使用上一次有效缓存，完全没有缓存时使用内置安全默认值。

后台发布目录新版本后，不打断当前项目。用户下次进入项目时比较 `catalogRevision`，通过内容哈希验证后更新缓存。缓存更新不回写已有片段。

## 4. 字体模块

### 4.1 字体目录和文件

字体目录包含：

```text
fontId
familyName
displayName
version
fileFormat
fileSize
contentHash
licenseStatus
entitlement
fallbackFamilies
catalogRevision
```

字体文件存储在 Supabase 私有 Storage，只能由服务端返回短时签名地址。客户端下载后先校验大小、格式、内容哈希和授权状态，再写入用户本地字体缓存和用户字体目录。安装失败必须明确提示，不得静默替换字体。

### 4.2 用户选择后的安装流程

字体只在用户明确选择或点击下载后下载和安装，不在项目进入时下载全部字体：

```text
字体列表预览
→ 用户选择/点击下载
→ 获取短时签名地址
→ 下载到临时缓存
→ 校验格式、大小、哈希和授权
→ 安装到用户本地字体目录
→ 刷新字体运行时
→ 写入项目字体快照和偏好事实
```

### 4.3 字体列表 UI

列表按字体自身渲染字体名称，提供：

- 已安装/下载按钮和状态；
- 收藏星标；
- 资源权限标记；
- 下载进度和失败重试；
- 当前选择状态。

项目记录：

```text
fontId
fontFamily
fontVersion
fontFileHash
installedPath
fallbackFamily
```

历史项目继续使用已安装并记录哈希的字体；目录下架或新版本发布不会自动替换历史项目字体。

## 5. 偏好系统和默认偏好

### 5.1 会话缓存

缓存分三层：

```text
内置安全默认值 → 用户本地目录缓存 → 当前项目会话快照
```

项目进入时只同步一次颜色/字体目录。后续属性选择、预览、渲染和导出全部访问缓存，不在每次操作时访问 Supabase。

### 5.2 首次安装默认偏好

Supabase 维护版本化默认偏好。默认偏好只在以下条件同时满足时使用：

1. 当前安装实例尚未建立本地用户偏好；
2. 用户首次创建或打开项目；
3. 当前属性没有用户 A/B/C 或确认过的单属性值。

默认值应用不产生学习事实，也不覆盖已有用户方案。用户明确确认颜色、字体或其他属性后，才写入 `property` 事实并开始收敛 A/B/C。

### 5.3 统一读写边界

`PreferenceStore` 是唯一偏好写入方。颜色模块和字体模块只提交经过合同校验的确认事实；项目文件只保存实际应用值和资源快照，不保存偏好方案本身。

## 6. 兑换码

### 6.1 数据和安全

新增：

- `redeem_code_batches`：批次、套餐、时长、数量、创建者、生成时间；
- `redeem_codes`：代码哈希、批次、有效期、状态、使用者、使用时间；
- `redeem_code_redemptions`：用户、代码、订单/权益、请求 ID、结果、时间。

明文兑换码只在生成时展示一次，不写入数据库。兑换码默认有效 24 小时，数据库使用唯一约束和幂等请求 ID 防止重复兑换。

### 6.2 客户端流程

订阅/续订面板右上角增加“兑换码”小字按钮：

```text
输入兑换码
→ Edge Function 验证 JWT、哈希、有效期、状态和风控
→ 原子创建月度/季度/年度权益
→ 写入兑换记录和审计记录
→ 返回新的权益状态
```

客户端不能提交或修改套餐、时长、金额和权益状态。

## 7. 邀请码和邀请奖励

订阅/续订面板右上角增加橙色“邀请码”按钮，显示用户专属邀请码和邀请链接。

后台规则控制：

```text
allow_unsubscribed_inviter
reward_amount
max_credit_per_order
credit_valid_days
eligibility_requirements
```

被邀请用户完成注册、邮箱验证并成功订阅后，服务端只发放一次不可提现奖励。奖励进入不可变 `credit_ledger`，可在下次续费抵扣，不可兑现、转赠或由客户端修改。退款、拒付和风控命中时奖励冻结或撤销。

## 8. 播放头和时间线交互

- 播放头颜色固定为 `#6CFF37`；
- 播放头分割线宽度为 `1px`；
- 鼠标进入轨道区域播放头线的命中范围时显示拖拽光标；
- 拖拽按时间轴像素换算为项目帧，并以帧边界吸附；
- 空白轨道单击直接把播放头定位到鼠标位置；
- 左右键按项目帧率精准移动一帧；
- 预览、时间线和导出统一使用 `state.time` 的帧值，避免秒/帧漂移。

## 9. 循环动画

循环动画新增方向枚举：

```text
right
left
up
down
scale-up
scale-down
```

动画数据保存为规范化参数和关键帧，不保存渲染结果。求值器必须同时被预览、离线渲染和导出调用：

```text
direction + distance/scale + durationFrames + easing + loopMode
```

超出组件支持范围、属性未声明或关键帧非法时阻断执行并报告原因。

## 10. 管理后台

### 10.1 管理范围

统计：

- 用户总数和增长；
- 订阅、续订、到期、退款；
- 意见反馈和问题日志；
- 兑换码生成与使用；
- 邀请关系和奖励记录。

管理：

- 颜色目录和版本发布；
- 字体目录、字体文件和授权状态；
- 默认偏好；
- 大模型模板；
- 兑换码批次生成；
- 邀请规则和奖励金额；
- 用户状态和人工处理记录；
- 管理员角色与审计日志。

### 10.2 权限

新增：

- `admin_roles`：用户 UUID、角色、状态、生效时间；
- `admin_audit_events`：管理员、动作、目标、前后摘要、请求 ID、时间和结果。

初始管理员为 `tangxu8@icloud.com` 对应的 Supabase 用户，但生产授权以用户 UUID 和 `admin_roles` 为准。邮箱本身不能作为唯一授权条件。

### 10.3 部署

后台部署到 Cloudflare，域名为 `admin.edward.uno`。前端不嵌入 service role；敏感配置只放 Cloudflare/Functions 环境变量。后台每个写操作都经过 Edge Function 的管理员校验、输入合同、幂等处理和审计记录。

## 11. 错误、缓存和兼容

- 颜色或字体目录同步失败不影响已有项目编辑；
- 字体下载或安装失败不得修改当前项目属性；
- 缓存损坏时隔离损坏条目并回退内置默认值；
- 目录更新不改写历史项目；
- 兑换和奖励失败必须保持事务原子性；
- 管理后台只能看到经过聚合或授权的用户数据；
- 预览、渲染、导出使用同一快照和求值器；
- 不引入 Component IR 或新的组件转换层。

## 12. 验收标准

1. 项目进入时只进行一次颜色/字体目录同步检查，项目会话内不重复访问 Supabase。
2. 颜色块覆盖文字、线条、边框、背景、阴影、SVG 和组件颜色属性。
3. 字体列表以字体自身渲染名称，用户明确选择后才下载、安装并可用于预览/渲染/导出。
4. 历史项目保存具体色值和字体文件哈希，目录更新不改变历史结果。
5. 默认偏好只在首次安装且没有用户方案时生效，不计入学习事实。
6. 兑换码 24 小时有效、单次兑换、可追溯且不能由客户端伪造权益。
7. 邀请奖励遵循后台规则，进入不可变抵扣账本，支持冻结、撤销和审计。
8. 播放头颜色、拖拽、空白定位和左右键逐帧操作可验证。
9. 六类循环动画方向在预览、渲染和导出中结果一致。
10. `admin.edward.uno` 使用 Refine，所有管理写操作经过管理员 Edge Function 和审计。
11. `tangxu8@icloud.com` 登录后只有在 `admin_roles` 授权时才具备管理员权限。
