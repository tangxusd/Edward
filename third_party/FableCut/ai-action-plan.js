/* Orbit AI ActionPlan executor. It never mutates its input project. */
(function attachEdwardAiActionPlan(root, factory) {
  const api = factory();
  if (typeof module !== "undefined" && module.exports) module.exports = api;
  root.edwardAiActionPlan = api;
})(typeof globalThis !== "undefined" ? globalThis : this, function createEdwardAiActionPlan() {
  const ALLOWED_TYPES = new Set([
    "insert_native_component", "set_component_props", "move_clip", "resize_clip", "remove_clip",
  ]);

  function fail(message) { throw new Error(message); }
  function clipEnd(clip) { return Number(clip.start) + Number(clip.duration); }
  function clone(value) { return JSON.parse(JSON.stringify(value)); }
  function defaults(manifest) {
    const properties = manifest?.props || manifest?.propsSchema?.properties || {};
    return Object.fromEntries(Object.entries(properties).map(([key, spec]) => [key, spec.default]));
  }
  function chooseTrack(clips, tracks, preferred, start, duration, ignoreId, maxTracks) {
    const videoTracks = tracks.filter((track) => track.kind === "video");
    if (!videoTracks.length) fail("项目没有可用的视频轨道");
    const from = Math.max(0, videoTracks.findIndex((track) => track.id === (preferred || "V2")));
    for (let index = from; index >= 0; index--) {
      const id = videoTracks[index].id;
      const overlaps = clips.some((clip) => clip.id !== ignoreId && clip.track === id &&
        start < clipEnd(clip) - 1e-4 && start + duration > Number(clip.start) + 1e-4);
      if (!overlaps) return id;
    }
    if (videoTracks.length >= maxTracks) fail("没有可用的上层视频轨道");
    const numbers = videoTracks.map((track) => Number.parseInt(String(track.id).slice(1), 10) || 0);
    const id = `V${Math.max(0, ...numbers) + 1}`;
    tracks.push({ id, kind: "video" });
    return id;
  }
  function validObject(value) { return value && typeof value === "object" && !Array.isArray(value); }
  function allowedFields(operation, fields) { return Object.keys(operation).every((key) => fields.includes(key)); }
  function validateOperation(operation) {
    if (!validObject(operation) || !ALLOWED_TYPES.has(operation.type)) fail("AI 操作不在允许范围内");
    const text = (key) => typeof operation[key] === "string" && operation[key].length > 0;
    if (operation.type === "insert_native_component") {
      if (!allowedFields(operation, ["type", "resourceId"]) || !text("resourceId")) fail("AI 插入组件参数无效");
    } else if (operation.type === "set_component_props") {
      if (!allowedFields(operation, ["type", "targetId", "props"]) || !text("targetId") || !validObject(operation.props)) fail("AI 属性修改参数无效");
    } else if (operation.type === "move_clip") {
      if (!allowedFields(operation, ["type", "targetId", "timelineStart"]) || !text("targetId") || !Number.isInteger(operation.timelineStart) || operation.timelineStart < 0) fail("AI 移动参数无效");
    } else if (operation.type === "resize_clip") {
      if (!allowedFields(operation, ["type", "targetId", "durationFrames"]) || !text("targetId") || !Number.isInteger(operation.durationFrames) || operation.durationFrames < 1) fail("AI 时长参数无效");
    } else if (operation.type === "remove_clip" && (!allowedFields(operation, ["type", "targetId"]) || !text("targetId"))) {
      fail("AI 删除参数无效");
    }
  }
  function validateComponentProps(target, resources, props) {
    const manifest = resources.get(target.componentId);
    const declared = manifest?.props || manifest?.propsSchema?.properties || {};
    for (const [key, value] of Object.entries(props)) {
      const spec = declared[key];
      if (Object.keys(declared).length && !spec) fail("AI 属性未在组件协议中声明");
      const expected = spec?.type || typeof target.props?.[key];
      if (expected === "number" && (typeof value !== "number" || !Number.isFinite(value))) fail("AI 数值属性无效");
      if ((expected === "string" || expected === "color") && typeof value !== "string") fail("AI 文本属性无效");
      if (expected === "boolean" && typeof value !== "boolean") fail("AI 布尔属性无效");
      if (!spec && !(key in (target.props || {}))) fail("AI 属性未在当前组件中声明");
    }
  }
  function apply(plan, context) {
    if (!validObject(plan) || plan.schemaVersion !== "edward.action-plan.v1") fail("AI 操作计划格式无效");
    if (Number(plan.baseProjectRevision) !== Number(context.project.revision)) fail("AI 操作计划已过期，请重新请求");
    if (!Array.isArray(plan.operations) || !plan.operations.length) fail("AI 操作计划为空");
    const clips = clone(context.project.clips || []);
    const tracks = clone(context.tracks || []);
    const resources = new Map((context.resources || []).map((item) => [item.id, item]));
    const fps = Number(context.project.fps || 30);
    if (!(fps > 0)) fail("项目帧率无效");
    for (const operation of plan.operations) {
      validateOperation(operation);
      const target = clips.find((clip) => clip.id === operation.targetId);
      if (operation.type === "insert_native_component") {
        const manifest = resources.get(operation.resourceId);
        if (!manifest) fail("AI 选择的组件未验证");
        const props = defaults(manifest);
        const duration = Number(props.duration || 3);
        const start = Number(context.playhead || 0);
        if (!(duration > 0) || start < 0) fail("组件默认时长无效");
        clips.push({
          id: context.nextClipId(), mediaId: null, kind: "component", componentId: manifest.id,
          runtime: manifest.runtime, source: manifest.entry, track: chooseTrack(clips, tracks, "V2", start, duration, null, context.maxTracks || 16),
          start, in: 0, duration, name: manifest.name || manifest.id, props,
        });
      } else {
        if (!target) fail("AI 操作目标已不存在");
        if (operation.type === "set_component_props") {
          if (target.kind !== "component") fail("属性修改仅适用于组件");
          validateComponentProps(target, resources, operation.props);
          target.props = { ...target.props, ...operation.props };
        } else if (operation.type === "move_clip") {
          target.start = operation.timelineStart / fps;
          target.track = chooseTrack(clips, tracks, target.track, target.start, target.duration, target.id, context.maxTracks || 16);
        } else if (operation.type === "resize_clip") {
          target.duration = operation.durationFrames / fps;
          target.track = chooseTrack(clips, tracks, target.track, target.start, target.duration, target.id, context.maxTracks || 16);
        } else if (operation.type === "remove_clip") {
          clips.splice(clips.indexOf(target), 1);
        }
      }
    }
    return { clips, tracks };
  }
  return { apply };
});
