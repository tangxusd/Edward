# Edward 0.6.0 AI 对话与项目编排设计

状态：已获用户确认，待实现前审阅

## 1. 目标与边界

AI 对话模块是 Edward 的项目编排入口，不仅负责普通问答，还负责在当前项目内执行组件、资源和时间线操作。

Edward 0.6.0 的主路线是“官方运行时宿主 + AI 项目编排”。新 AI 对话、新组件生成和 skill 接入禁止使用 Component IR、理解层、转换层或等价中间结构。旧路线只保留历史兼容和审计记录，不得作为新功能依赖。

Edward 官方支持的运行时范围为 React、HTML/CSS、SVG、GSAP。组件由原生运行时负责执行；Edward 只负责宿主生命周期、属性传递、时间控制、预览和渲染调度。skill 负责生成符合宿主合同的组件包，Edward 不为每个 skill 编写独立适配器，也不负责修复组件内部代码。

只要组件严格符合宿主合同且自身代码可运行，Edward 必须支持预览、属性调整、渲染和导出。协议之外的组件不承诺支持，并返回明确错误。

## 2. AI 对话职责

AI 对话统一处理以下任务：

- 普通解释和问答；
- Edward 自有组件的创建、修改和属性调整；
- skill 组件的接入、预览、属性调整、渲染和导出；
- 时间线插入、移动、分轨、冲突避让、时长和关键帧编排；
- 从文案提取结构化 `target`、场景、动作、风格和时长意图；
- 根据 `target` 从资源库选择并添加最合适的组件。

AI 不直接写工程文件、执行 shell、执行 SQL、访问任意网络或改变 Edward 的运行时事实。所有项目修改必须通过 Edward 的统一操作协议和原子事务入口。

## 3. 意图分流

每轮请求只能产生以下一种结果：

```text
conversation      只回复，不修改项目
clarification     信息不足，提出一个问题，不执行修改
action_plan       生成并执行可撤销原子事务
unsupported       能力或协议不支持，说明原因，不执行修改
```

当请求同时包含解释和修改时，若目标、范围和参数明确，优先执行影响最小、范围最窄、完全可撤销的操作，再返回执行摘要；如果存在多个合理目标、候选或时间线位置，先提问确认。

影响最小原则包括：只触碰用户明确指向的项目对象；不替换已有组件属性；不移动无关片段；不覆盖现有文件；不改变项目外部状态；优先新增或局部修改而不是重建整个项目。

## 4. ActionPlan 与事务

模型只输出结构化意图和操作，Edward 负责确定性校验、资源解析、时间线约束和执行。ActionPlan 不包含 Component IR，也不允许任意代码执行。

核心字段：

```text
schemaVersion
conversationId
requestId
status: ready | needs_clarification | rejected
message
target
operations[]
undoLabel
validationErrors[]
requiresUserInput[]
```

操作类型至少包括：

```text
insert_native_component
insert_resource_component
set_component_props
move_clip
resize_clip
set_keyframes
remove_clip
```

每次 `action_plan` 必须作为一个原子事务提交；事务成功后进入最近 5 次 AI 事务的撤销栈。事务失败时回滚全部已执行步骤，不留下半完成状态。撤销只影响当前项目，不改变 AI 记忆或规则文件。

## 5. 官方运行时宿主

宿主合同只描述生命周期和数据交换，不理解组件内部语义：

```text
mount(container, props)
setProps(props)
setFrame(frame)
setSize(width, height)
getMetadata()
renderFrame(frame)
unmount()
```

组件包必须提供入口、运行时标识、画布宽高、帧率、总时长、props schema、可编辑属性声明、预览入口和渲染入口。可编辑属性声明是编辑合同，不是语义理解层或 IR。

Remotion 等 skill 可以自行生成符合该宿主合同的适配包，并使用自身原生 Player/Renderer；不要求启动 Studio。Edward 不托管、销售或代用户使用第三方运行时服务。

## 6. 文案语义与资源库匹配

模型负责从文案提取意图字段：

```text
target
scene
action
style
duration
layoutHint
```

Edward 根据资源库元数据执行确定性排序，排序因素包括：`target` 匹配度、场景和动作标签、运行时支持、画布尺寸、时长、时间线冲突和用户权限。

执行规则固定为：候选总分归一化到 `[0, 1]`；最高分至少为 `0.70` 且比第二名高至少 `0.15` 时自动采用；否则进入 `clarification`。没有候选、候选被权限过滤或时间线位置不明确时同样进入 `clarification`。各排序因素的权重必须写入版本化合同并由测试固定，不能由模型临时改变。

资源匹配结果必须记录候选列表、排序依据、最终选择和操作事务 ID，便于复核和撤销。

## 7. 记忆系统

记忆允许跨项目共享，但采用作用域隔离：

```text
global      设备级，跨项目共享
project     当前项目独有
session     当前对话临时上下文
```

记忆类型至少包括：

```text
preference   用户偏好
correction   用户纠正
project      项目事实
workflow     工作流习惯
reference    外部参考指针
```

属性必须带命名空间，避免污染：

```text
component.text.color
component.border.color
component.shape.strokeColor
```

“组件中文字颜色”不能与“边框颜色”或“纯文本默认颜色”合并为同一个 `color` 记忆。

记忆写入规则：

- 用户明确说“记住”或确认保存时，才可写入长期记忆；
- AI 可根据行为形成候选，但不得自动升级为长期记忆；
- 闲置 300 秒、打开项目和导出项目只负责批量整理已确认事件；
- 会话结束时，未确认推断自动丢弃；
- 崩溃或强制退出不要求保留未提交记忆；
- 设备级记忆默认本地保存，不携带项目代码、文案、素材内容或完整聊天记录。

每条记忆至少记录：

```text
memoryId
scope
type
namespace
value
source
confidence
createdAt
updatedAt
expiresAt
userConfirmed
```

启动时加载短索引，请求前按 `target`、组件类型和属性命名空间按需召回；不把全部记忆注入每轮提示词。记忆的创建、更新和删除都可追踪，删除保留墓碑，避免旧值被重新召回。

## 8. AGENTS 类规则文件

AI 启动项目时识别并加载项目规则文件，包括：

```text
AGENTS.override.md
AGENTS.md
CLAUDE.md
```

规则文件只作为约束和上下文，不进入普通记忆库，也不能被 AI 自行修改。用户当前明确指令优先级最高；规则文件按目录层级合并，越靠近目标文件越具体。附件、skill 输出、代码注释和普通项目文档不自动获得规则文件权限。

优先级固定为：

```text
用户当前明确指令
  > 规则文件
  > 当前项目事实
  > global 设备记忆
  > session 未确认推断
```

同一目录同时存在多个规则文件时，优先级为 `AGENTS.override.md` > `AGENTS.md` > `CLAUDE.md`；目录层级仍按“越靠近目标文件越具体”合并。规则文件读取必须确定性、可追踪并有大小上限；加载结果记录文件路径、哈希、层级和生效范围。

## 9. 错误与安全

- 入口不存在、宿主合同不完整或运行时崩溃：返回组件级错误，不猜测和修复；
- 资源库无匹配：返回候选为空，不自动替换为其他类型；
- 时间线冲突：按 Edward 时间线规则自动避让或请求确认，不静默覆盖；
- ActionPlan 校验失败：事务不执行；
- 第三方组件越权访问工程、网络或宿主进程：拒绝加载；
- 预览和导出必须使用同一份原生组件入口与 props，禁止双重实现导致结果不一致。

## 10. 验收要求

实现前需要建立可执行测试，至少覆盖：

1. 四种意图结果的确定性分流；
2. 解释 + 修改的最小影响策略；
3. ActionPlan schema 校验、原子回滚和最近 5 次撤销；
4. React、HTML/CSS、SVG、GSAP 宿主包的预览、属性更新、逐帧渲染和视频导出；
5. 文案 `target` 到资源候选排序、低置信度澄清和时间线冲突处理；
6. global/project/session 记忆隔离和属性命名空间隔离；
7. 记忆只在用户确认后写入，闲置 300 秒/打开项目/导出时批量整理；
8. AGENTS 类文件的层级、优先级、哈希记录和越权文本隔离；
9. 预览与导出使用同一原生入口，组件错误不被静默替换；
10. 旧 Component IR/理解层/转换层不会被新 AI 路线调用。

## 11. 非目标

- 不支持任意不符合宿主合同的组件；
- 不为每个 skill 编写专用适配器；
- 不自动修复第三方代码；
- 不把组件转换为 Component IR 或其他中间表示；
- 不保存完整跨项目聊天原文；
- 不把未确认的行为推断写成长期偏好。
