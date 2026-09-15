# Edward 偏好系统实现记录

时间：2026-09-15

目的：落实 Edward 0.6.0 的设备级偏好方案：本地 SQLite、属性隔离、只作用于新建组件、
300 秒闲置批处理，以及可选的事实同步载荷。

涉及文件：

- `src/desktop/include/edward/desktop/preference_store.hpp`
- `src/desktop/src/preference_store.cpp`
- `src/desktop/src/workbench_runtime.cpp`
- `src/desktop/src/main.cpp`
- `src/desktop/qml/Workbench.qml`
- `third_party/FableCut/preference-client.js`
- `third_party/FableCut/app.js`
- `src/resources/include/edward/resources/preference_sync_client.hpp`
- `src/resources/src/preference_sync_client.cpp`

结果：

- SQLite 使用 WAL；事件以 `eventId` 幂等写入，设备安装标识由宿主补充。
- 完整 manifest 身份、语义路径和属性路径参与隔离，文字颜色与边框颜色不会混用。
- 仅新建组件读取 A/B/C；已有项目片段没有创建会话元数据，不会产生偏好事实。
- 正常项目切换、导出、退出和 300 秒无操作会批量写入并收敛方案。
- 偏好事实导出/导入载荷排除项目 ID、路径、时间线和素材内容；远端事实逐条校验后按
  `eventId` 合并，不覆盖本地同 ID 事实。

验证：

- `cmake --build build --target edward_app test_preference_store test_preference_bridge test_preference_sync_client -j2`
- `ctest --test-dir build -R 'desktop\\.(preference_store|preference_bridge)|resources\\.preference_sync_client' --output-on-failure`
- `node --test third_party/FableCut/test/preferences.test.js`
- `node --test third_party/FableCut/test/user-components.test.js`

全量 CTest 构建成功；89 个测试中偏好相关 3 个全部通过，FableCut Node 测试 76/76 通过。
全量 CTest 另有 6 个既有 Resolve/认证环境测试失败（`desktop.visual_routes`、
`desktop.workbench_plugins`、`desktop.resolve_workbench`、`resources.auth_session_store`、
`resources.supabase_auth_client`、`resolve.sidecar_rectangle`），未修改这些无关路径。

Supabase 端已补充 `preference_facts` 表、RLS 和 `preference-sync` Edge Function；云端部署和
真实账号往返尚未在本机执行，设置入口保持手动触发，不会在登录、打开项目或导出时联网。
