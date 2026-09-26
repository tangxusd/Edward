# 属性检查器与渲染接入实施计划

> **执行约束：** 仅修改当前 worktree；不触碰既有暂存的大规模清理。最终只能启动 `Edward.app` 验证，不提供 HTML 预览链接。

## 目标

完成属性检查器的全部基础能力并接入同一套预览/导出属性求值：中文属性合同、A/B/C 偏好方案、双击复位、循环动画、关键帧及插值、In/Out 转场。闭合圆角矩形仅作为对应注释组件的补充能力处理。

## 影响范围

- `third_party/FableCut/index.html`
- `third_party/FableCut/app.js`
- `third_party/FableCut/component-runtime.js`
- `third_party/FableCut/components/annotations/*.gsap`
- `third_party/FableCut/components/annotations/*.html`
- `third_party/FableCut/components/annotations/*.svg`
- `src/desktop/include/edward/desktop/preference_store.hpp`
- `src/desktop/src/preference_store.cpp`
- `tests/desktop/test_preference_bridge.cpp`
- `third_party/FableCut/tests/*`（新增或扩展针对属性合同和求值的测试）
- `.edward/acceptance/current.json`
- `docs/operation-log/settings.md`

## 实施步骤

### 1. 建立验收门和回归测试骨架

**文件：** `.edward/acceptance/current.json`、相关测试文件。

1. 创建当前任务的验收条目，列出属性合同、偏好 A/B/C、关键帧、循环动画、转场、闭合路径、构建和实际 App 启动检查。
2. 先补失败测试：三套偏好按 rank 独立读写；关键帧默认 `cubic-out`；未知旧数据回退；各循环动画影响求值；转场只作用于片段端点。
3. 在实现完成后将每项标记为通过，并记录实际命令与结果。

**验证：** 初始测试能表达新行为，完成后所有相关测试通过。

### 2. 引入统一属性合同和中文展示层

**文件：** `third_party/FableCut/property-contract.js`、`index.html`、`app.js`。

1. 以浏览器全局脚本形式引入 `property-contract.js`，避免把当前非模块化 `app.js` 改为 ES module。
2. 为检查器现有属性、循环属性、转场属性定义内部键、中文标签、默认值、可关键帧、可偏好、适用范围、选项中文标签。
3. 让检查器标签、下拉项、原生组件属性和偏好入口从合同读取；工程内部字段和已保存项目键不重命名。
4. 移除检查器“名称”编辑项，不删除项目数据中的 `clip.name`。
5. 将双击标签统一接到复位函数：还原合同或原生清单默认值、清除该属性关键帧、写入当前偏好方案。

**验证：** 检查器不出现“名称”或裸露英文属性键；属性合同测试能断言中文、默认值、关键帧和偏好资格。

### 3. 实现独立 A/B/C 偏好方案

**文件：** `app.js`、`preference_store.hpp`、`preference_store.cpp`、`test_preference_bridge.cpp`。

1. 将偏好方案选择从禁用显示改为 A/B/C 可切换；显示为“方案 A/B/C”，不暴露内部 rank。
2. 为 `preference_events` 增加可迁移的 `profile_rank` 字段；旧记录默认归入 A。
3. 记录确认修改时传入当前 rank；编译/读取时按 rank 独立返回，创建组件按所选方案取值。
4. 让合同中标记为可偏好的新增循环属性也使用同一偏好记录入口；保留既有原生组件 manifest 身份作为偏好隔离边界。

**验证：** 同一属性在 A/B/C 写入不同值后，新建组件按对应方案得到不同值；旧数据库可打开。

### 4. 统一关键帧、循环动画和转场求值

**文件：** `app.js`、`component-runtime.js`、测试文件。

1. 新增 `loopAnimation`、`loopFrequency`、`loopAmplitude` 默认值和检查器“循环动画”分区，提供无、轻微晃动、轻微跳跃、闪烁、轻微缩放。
2. 扩展 `EASE`：`cubic-out` 为默认三次贝塞尔缓出，另有线性、回弹、缓出、缓入；关键帧记录存储 `easing`，同一时间点更新而非重复。
3. 在关键帧控件旁提供当前段插值下拉；采用前一关键帧的插值预设。
4. 将 `evalProps` 固定为：关键帧基础值 -> 循环动画 -> In/Out 转场。转场时长裁剪到片段长度，片段中段不受影响。
5. 把已求值的属性传入 Canvas 绘制与原生组件运行时，使预览、直接组件和导出路径共享同一实际值。

**验证：** 测试覆盖五种循环效果、五种插值、关键帧编辑/删除和 In/Out 端点；直接组件也接收求值后的 opacity、位置、缩放等属性。

### 5. 补充闭合矩形组件并完成本地化检查器布局

**文件：** `components/annotations/*`、`app.js`、必要样式文件。

1. 仅定位闭合圆角矩形注释组件中的 `stroke-dasharray`/`stroke-dashoffset` 用法。
2. 在该组件内改用单个完整闭合 `<rect>` 或等价闭合路径描边，不再以虚线截断实现进入效果；若保留出现动画，仅改变该组件整体透明度或缩放，不能裁断描边。
3. 不向通用渲染底座、其他组件或属性合同新增闭合路径规则。原生组件的字段标签通过属性合同翻译；下拉选项中文化，数值与布局遵循当前紧凑检查器样式。

**验证：** 源码测试或静态断言确认闭合路径不包含虚线裁断属性；组件预览在任意有效时间保持连续闭合描边。

### 6. 运行检查、更新记录并启动实际 App

**文件：** `.edward/acceptance/current.json`、`docs/operation-log/settings.md`。

1. 运行新增/受影响的 JavaScript 测试、桌面偏好测试和项目构建；逐项处理失败。
2. 检查现有 Edward 进程：存在则结束对应 `Edward.app` 进程后重启；不存在则直接启动 `build-0.7.0/bin/Edward.app`。
3. 通过应用进程及本地服务健康状态确认实际 App 已启动，不以 HTML 文件或浏览器预览代替验证。
4. 在验收文件和操作记录追加变更、验证命令和结果。

**验证：** 所有验收项为通过，实际 `Edward.app` 已运行。

## 自检

- 未使用 Component IR、Resolve 或任何被禁路线。
- 不重命名已存工程字段，不破坏旧项目读取。
- 关键帧和转场由共享求值函数驱动，避免 Canvas 与原生组件行为分叉。
- 闭合矩形的连续描边实现仅限对应注释组件，不成为通用底座规则。
- 所有新增代码、测试、记录均位于当前项目 worktree。
