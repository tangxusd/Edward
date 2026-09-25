/* Orbit AI ActionPlan executor. It never mutates its input project. */
(function attachEdwardAiActionPlan(root, factory) {
  const api = factory();
  if (typeof module !== "undefined" && module.exports) module.exports = api;
  root.edwardAiActionPlan = api;
})(typeof globalThis !== "undefined" ? globalThis : this, function createEdwardAiActionPlan() {
  function fail(message) { throw new Error(message); }
  function clipEnd(clip) { return Number(clip.start) + Number(clip.duration); }
  function clone(value) { return JSON.parse(JSON.stringify(value)); }
  function fnv1a64(value) {
    let hash = 0xcbf29ce484222325n;
    for (const byte of new TextEncoder().encode(value)) {
      hash ^= BigInt(byte);
      hash = BigInt.asUintN(64, hash * 0x100000001b3n);
    }
    return hash.toString(16).padStart(16, "0");
  }
  function modelContracts(snapshot) {
    if (!snapshot || snapshot.schemaVersion !== "orbit.capability-snapshot.v1" || snapshot.version !== 1 ||
        !Array.isArray(snapshot.contractIds) || !Array.isArray(snapshot.capabilities) || typeof snapshot.hash !== "string") {
      fail("缺少宿主能力注册表快照");
    }
    const ids = snapshot.contractIds.map(String);
    const sortedIds = [...ids].sort();
    if (ids.length === 0 || ids.some((id, index) => id !== sortedIds[index]) || new Set(ids).size !== ids.length) {
      fail("能力注册表 contractIds 未规范化排序");
    }
    const canonical = `orbit.capability-snapshot.v1|${snapshot.version}|${ids.join(",")}`;
    if (snapshot.hash !== `fnv1a64:${fnv1a64(canonical)}`) fail("能力注册表快照 hash 不一致");
    if (snapshot.capabilities.length !== ids.length || snapshot.capabilities.some((item, index) => `${item.id}@${item.version}` !== ids[index])) {
      fail("能力注册表合同不一致");
    }
    const required = ["inputSchema", "targetTypes", "permissionCategory", "mutatesProject", "externalSideEffects",
      "selectionPolicy", "unitPolicy", "coalescingPolicy", "lockPolicy", "playbackPolicy", "limits", "taskPolicy",
      "undoScope", "collisionPolicy", "trackPlacementPolicy", "linkedMediaPolicy", "executionMode", "reversibility",
      "allowedPolicies", "markerPolicy", "validate", "execute", "postconditions", "preview", "render", "verificationAdapter"];
    for (const item of snapshot.capabilities) for (const key of required) if (!(key in item)) fail(`能力注册表缺少字段: ${key}`);
    for (const item of snapshot.capabilities) {
      const task = item.taskPolicy;
      if (!task || typeof task !== "object" || typeof task.mode !== "string" || typeof task.resourceClass !== "string" ||
          !Number.isInteger(task.priority) || task.priority < 0 || task.priority > 100 || typeof task.cancellableUntil !== "string" ||
          typeof task.resumable !== "boolean" || !Number.isInteger(task.maxConcurrency) || task.maxConcurrency < 1 ||
          !Number.isFinite(task.diskReservation) || task.diskReservation < 0 || typeof task.progressAdapter !== "string") {
        fail("能力注册表 taskPolicy 无效");
      }
    }
    return snapshot.capabilities;
  }
  function modelView(snapshot) {
    return modelContracts(snapshot).map((item) => ({
      id: item.id, version: item.version, inputSchema: clone(item.inputSchema), targetTypes: [...item.targetTypes],
      permissionCategory: item.permissionCategory, mutatesProject: item.mutatesProject, externalSideEffects: [...item.externalSideEffects],
      selectionPolicy: item.selectionPolicy, unitPolicy: item.unitPolicy, coalescingPolicy: item.coalescingPolicy,
      lockPolicy: item.lockPolicy, playbackPolicy: item.playbackPolicy, limits: clone(item.limits), undoScope: item.undoScope, collisionPolicy: item.collisionPolicy,
      trackPlacementPolicy: item.trackPlacementPolicy, linkedMediaPolicy: item.linkedMediaPolicy,
      taskPolicy: clone(item.taskPolicy), executionMode: item.executionMode, reversibility: item.reversibility,
      allowedPolicies: [...item.allowedPolicies], markerPolicy: item.markerPolicy, validate: item.validate,
      execute: item.execute, postconditions: [...item.postconditions], preview: item.preview, render: item.render,
      verificationAdapter: item.verificationAdapter,
    }));
  }
  function defaults(manifest) {
    const properties = manifest?.props || manifest?.propsSchema?.properties || {};
    return Object.fromEntries(Object.entries(properties).map(([key, spec]) => [key, spec.default]));
  }
  function canPlace(clips, track, start, duration, ignoreId) {
    return !clips.some((clip) => clip.id !== ignoreId && clip.track === track && start < clipEnd(clip) - 1e-4 && start + duration > Number(clip.start) + 1e-4);
  }
  function chooseTrack(clips, tracks, preferred, start, duration, ignoreId, maxTracks, requestedKind = "video") {
    const videoTracks = tracks.filter((track) => track.kind === requestedKind);
    if (!videoTracks.length) fail(`项目没有可用的${requestedKind === "audio" ? "音频" : "视频"}轨道`);
    const from = Math.max(0, videoTracks.findIndex((track) => track.id === (preferred || "V2")));
    for (let index = from; index >= 0; index--) {
      const id = videoTracks[index].id;
      if (canPlace(clips, id, start, duration, ignoreId)) return id;
    }
    if (videoTracks.length >= maxTracks) fail("没有可用的上层视频轨道");
    const numbers = videoTracks.map((track) => Number.parseInt(String(track.id).slice(1), 10) || 0);
    const id = `V${Math.max(0, ...numbers) + 1}`;
    tracks.push({ id, kind: "video" });
    return id;
  }
  function validObject(value) { return value && typeof value === "object" && !Array.isArray(value); }
  function allowedFields(operation, fields) { return Object.keys(operation).every((key) => fields.includes(key)); }
  function validateOperation(operation, allowedTypes) {
    if (!validObject(operation) || !allowedTypes.has(operation.type)) fail("AI 操作不在允许范围内");
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
    } else if (operation.type === "insert_media") {
      if (!allowedFields(operation, ["type", "mediaId", "kind", "timelineStart", "durationFrames", "track"]) || !text("mediaId") ||
          (operation.timelineStart != null && (!Number.isInteger(operation.timelineStart) || operation.timelineStart < 0)) ||
          (operation.durationFrames != null && (!Number.isInteger(operation.durationFrames) || operation.durationFrames < 1))) fail("AI 素材插入参数无效");
    } else if (operation.type === "insert_text" || operation.type === "insert_subtitle") {
      if (!allowedFields(operation, ["type", "text", "props", "timelineStart", "durationFrames", "track"]) || typeof operation.text !== "string" ||
          (operation.timelineStart != null && (!Number.isInteger(operation.timelineStart) || operation.timelineStart < 0)) ||
          (operation.durationFrames != null && (!Number.isInteger(operation.durationFrames) || operation.durationFrames < 1))) fail("AI 文本插入参数无效");
    } else if (operation.type === "set_clip_props" || operation.type === "set_audio_props") {
      if (!allowedFields(operation, ["type", "targetId", "props"]) || !text("targetId") || !validObject(operation.props)) fail("AI 片段属性参数无效");
    } else if (operation.type === "replace_source") {
      if (!allowedFields(operation, ["type", "targetId", "mediaId"]) || !text("targetId") || !text("mediaId")) fail("AI 替换素材参数无效");
    } else if (operation.type === "duplicate_clip") {
      if (!allowedFields(operation, ["type", "targetId", "timelineStart", "track"]) || !text("targetId") ||
          (operation.timelineStart != null && (!Number.isInteger(operation.timelineStart) || operation.timelineStart < 0))) fail("AI 复制参数无效");
    } else if (operation.type === "split_clip") {
      if (!allowedFields(operation, ["type", "targetId", "atFrame"]) || !text("targetId") || !Number.isInteger(operation.atFrame) || operation.atFrame < 1) fail("AI 分割参数无效");
    } else if (operation.type === "trim_head" || operation.type === "trim_tail") {
      if (!allowedFields(operation, ["type", "targetId", "atFrame"]) || !text("targetId") || !Number.isInteger(operation.atFrame) || operation.atFrame < 0) fail("AI 裁剪参数无效");
    } else if (operation.type === "set_speed") {
      if (!allowedFields(operation, ["type", "targetId", "speed"]) || !text("targetId") || typeof operation.speed !== "number" || !Number.isFinite(operation.speed) || operation.speed <= 0) fail("AI 速度参数无效");
    } else if (operation.type === "create_marker") {
      if (!allowedFields(operation, ["type", "timelineFrame", "label", "color", "clipId", "localFrame"]) || !Number.isInteger(operation.timelineFrame) || operation.timelineFrame < 0) fail("AI 标记参数无效");
    } else if (operation.type === "delete_marker" || operation.type === "set_marker_color") {
      if (!allowedFields(operation, ["type", "markerId", "color"]) || !text("markerId") || (operation.type === "set_marker_color" && !text("color"))) fail("AI 标记参数无效");
    } else if (operation.type === "close_gap") {
      if (!allowedFields(operation, ["type", "track", "startFrame", "endFrame"]) || !text("track") || !Number.isInteger(operation.startFrame) || !Number.isInteger(operation.endFrame) || operation.startFrame < 0 || operation.endFrame <= operation.startFrame) fail("AI 闭合间隙参数无效");
    }
  }
  function validateTarget(contract, operation, target, context, tracks, markers) {
    const targets = new Set(contract.targetTypes || []);
    if ((targets.has("selected_clip") || targets.has("selected_component") || targets.has("selected_audio")) && !target) fail("AI 操作目标已不存在");
    if (targets.has("selected_clip") && (!target || !["video", "audio", "component", "text", "subtitle"].includes(target.kind))) fail("AI 目标不是可编辑片段");
    if (targets.has("selected_component") && (!target || target.kind !== "component")) fail("AI 目标不是组件");
    if (targets.has("selected_audio") && (!target || target.kind !== "audio")) fail("AI 目标不是音频");
    if (targets.has("marker") && !markers.some((marker) => String(marker.markerId || marker.id || marker.label) === String(operation.markerId))) fail("AI 标记不存在");
    if (targets.has("track") && !tracks.some((track) => String(track.id) === String(operation.track))) fail("AI 轨道不存在");
    if (targets.has("playhead") && (!Number.isFinite(Number(context.playhead)) || Number(context.playhead) < 0)) fail("AI 播放头位置无效");
    if (operation.type === "create_marker" && operation.clipId && !context.project.clips.some((clip) => String(clip.id) === String(operation.clipId))) fail("AI 素材级标记目标不存在");
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
    if (!validObject(plan) || plan.schemaVersion !== "orbit.bound-action-plan.v2") fail("AI 操作计划格式无效");
    if (Number(plan.baseProjectRevision) !== Number(context.project.revision)) fail("AI 操作计划已过期，请重新请求");
    if (!Array.isArray(plan.operations) || !plan.operations.length) fail("AI 操作计划为空");
    const contracts = modelContracts(context.capabilitySnapshot);
    const contractMap = new Map(contracts.map((item) => [item.id, item]));
    const allowedTypes = new Set(contractMap.keys());
    const clips = clone(context.project.clips || []);
    const tracks = clone(context.tracks || []);
    const resources = new Map((context.resources || []).map((item) => [item.id, item]));
    const media = new Map((context.media || []).map((item) => [item.id, item]));
    const markers = clone(context.project.markers || []);
    const fps = Number(context.project.fps || 30);
    if (!(fps > 0)) fail("项目帧率无效");
    for (const operation of plan.operations) {
      validateOperation(operation, allowedTypes);
      const target = clips.find((clip) => clip.id === operation.targetId);
      validateTarget(contractMap.get(operation.type), operation, target, context, tracks, markers);
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
      } else if (operation.type === "insert_media") {
        const source = media.get(operation.mediaId);
        if (!source) fail("AI 选择的素材未验证");
        const kind = operation.kind || source.kind || "video";
        const start = (operation.timelineStart ?? Math.round(Number(context.playhead || 0) * fps)) / fps;
        const duration = operation.durationFrames ? operation.durationFrames / fps : Number(source.duration || 3);
        if (!(duration > 0) || start < 0) fail("AI 素材时长无效");
        const trackKind = kind === "audio" ? "audio" : "video";
        let track = operation.track || tracks.find((item) => item.kind === trackKind)?.id;
        if (!track || !canPlace(clips, track, start, duration, null)) {
          track = chooseTrack(clips, tracks, operation.track, start, duration, null, context.maxTracks || 16, trackKind);
        }
        clips.push({ id: context.nextClipId(), mediaId: source.id, kind, track, start, in: 0, duration, name: source.name || source.id, props: {} });
      } else if (operation.type === "insert_text" || operation.type === "insert_subtitle") {
        const start = (operation.timelineStart ?? Math.round(Number(context.playhead || 0) * fps)) / fps;
        const duration = (operation.durationFrames || Math.round(fps * 3)) / fps;
        const track = operation.track || chooseTrack(clips, tracks, operation.track, start, duration, null, context.maxTracks || 16, "video");
        clips.push({ id: context.nextClipId(), mediaId: null, kind: "text", track, start, in: 0, duration, name: operation.type === "insert_subtitle" ? "字幕" : "文字", text: operation.text, props: clone(operation.props || {}) });
      } else if (operation.type === "create_marker") {
        const marker = { markerId: `m_${context.nextMarkerId ? context.nextMarkerId() : Date.now()}`, t: operation.timelineFrame / fps, label: operation.label || "", color: operation.color || "blue" };
        if (operation.clipId) { marker.clipId = operation.clipId; marker.localFrame = operation.localFrame ?? operation.timelineFrame; }
        markers.push(marker);
      } else if (operation.type === "delete_marker") {
        const index = markers.findIndex((marker) => String(marker.markerId || marker.id || marker.label) === operation.markerId);
        if (index < 0) fail("AI 标记不存在");
        markers.splice(index, 1);
      } else if (operation.type === "set_marker_color") {
        const marker = markers.find((item) => String(item.markerId || item.id || item.label) === operation.markerId);
        if (!marker) fail("AI 标记不存在");
        if (!["blue", "green", "yellow", "red", "purple"].includes(operation.color)) fail("AI 标记颜色无效");
        marker.color = operation.color;
      } else if (operation.type === "close_gap") {
        const delta = (operation.endFrame - operation.startFrame) / fps;
        for (const clip of clips) if (clip.track === operation.track && Number(clip.start) >= operation.endFrame / fps - 1e-4) clip.start = Math.max(0, Number(clip.start) - delta);
        for (const marker of markers) if (Number(marker.t) >= operation.endFrame / fps - 1e-4) marker.t = Math.max(0, Number(marker.t) - delta);
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
        } else if (operation.type === "set_clip_props" || operation.type === "set_audio_props") {
          target.props = { ...(target.props || {}), ...clone(operation.props) };
        } else if (operation.type === "replace_source") {
          if (!media.has(operation.mediaId)) fail("AI 替换素材未验证");
          target.mediaId = operation.mediaId;
        } else if (operation.type === "duplicate_clip") {
          const copy = clone(target);
          copy.id = context.nextClipId();
          copy.start = operation.timelineStart != null ? operation.timelineStart / fps : Number(target.start) + Number(target.duration);
          if (operation.track) copy.track = operation.track;
          clips.push(copy);
        } else if (operation.type === "split_clip") {
          const at = operation.atFrame / fps;
          if (!(at > Number(target.start) && at < Number(target.start) + Number(target.duration))) fail("AI 分割点不在片段内");
          const right = clone(target); right.id = context.nextClipId(); right.start = at; right.in = Number(target.in || 0) + (at - Number(target.start)) * Number(target.speed || 1); right.duration = Number(target.start) + Number(target.duration) - at; target.duration = at - Number(target.start); clips.push(right);
        } else if (operation.type === "trim_head") {
          const at = operation.atFrame / fps; const end = Number(target.start) + Number(target.duration);
          if (!(at > Number(target.start) && at < end)) fail("AI 头部裁剪点无效");
          target.in = Number(target.in || 0) + (at - Number(target.start)) * Number(target.speed || 1); target.start = at; target.duration = end - at;
        } else if (operation.type === "trim_tail") {
          const at = operation.atFrame / fps;
          if (!(at > Number(target.start) && at < Number(target.start) + Number(target.duration))) fail("AI 尾部裁剪点无效");
          target.duration = at - Number(target.start);
        } else if (operation.type === "set_speed") {
          target.speed = operation.speed;
        }
      }
    }
    return {
      clips, tracks, markers,
      receipt: {
        requestId: String(plan.requestId), schemaVersion: plan.schemaVersion,
        operationCount: plan.operations.length, affectedClipIds: clips.map((clip) => clip.id),
        markerCount: markers.length, reversible: true,
      },
    };
  }
  return { apply, modelView };
});
