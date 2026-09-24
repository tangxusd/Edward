/* Orbit 属性合同：内部键保持稳定，检查器与偏好只消费这里的元数据。 */
(function () {
  "use strict";
  const zhLabels = {
    x: "位置 X", y: "位置 Y", scale: "缩放", rotation: "旋转", opacity: "不透明度",
    volume: "音量", pan: "声像", speed: "速度", brightness: "亮度", contrast: "对比度",
    saturation: "饱和度", hue: "色相", blur: "模糊", grayscale: "灰度", sepia: "褐色",
    invert: "反相", temperature: "色温", tint: "色调", vignette: "暗角", fit: "适配方式",
    blend: "混合模式", filterPreset: "滤镜预设", cropL: "左侧裁剪", cropR: "右侧裁剪",
    cropT: "上侧裁剪", cropB: "下侧裁剪", cornerRadius: "圆角", flipH: "水平翻转",
    flipV: "垂直翻转", chromaKey: "抠像颜色", chromaTolerance: "抠像容差",
    chromaSoftness: "抠像柔化", bgRemove: "AI 移除背景", shake: "镜头抖动",
    shakeSpeed: "抖动速度", rgbSplit: "色散", grain: "颗粒", text: "文本内容",
    fontSize: "字号", color: "颜色", color2: "渐变颜色", font: "字体", bold: "粗体",
    italic: "斜体", weight: "字重", align: "水平对齐", letterSpacing: "字间距",
    lineHeight: "行高", uppercase: "大写", textShadow: "文字阴影", glow: "发光",
    glowColor: "发光颜色", textAnim: "文字动画", wordRate: "逐字速度", direction: "文字方向",
    strokeWidth: "描边宽度", strokeColor: "描边颜色", bgColor: "背景颜色", bgOpacity: "背景不透明度",
    boxW: "文本框宽度", boxH: "文本框高度", boxFit: "缩放以适配", vAlign: "垂直对齐",
    width: "宽度", height: "高度", borderWidth: "边框宽度", radius: "圆角半径", progress: "进度",
    duration: "展示时长（秒）", start: "开始（秒）", transitionIn: "入场", transitionOut: "出场",
    loopAnimation: "循环动画", loopFrequency: "循环频率", loopAmplitude: "循环幅度",
  };
  const defaults = { loopAnimation: "none", loopFrequency: 0.5, loopAmplitude: 2 };
  const animatable = new Set(["x", "y", "scale", "rotation", "opacity", "volume", "pan", "speed",
    "brightness", "contrast", "saturation", "hue", "blur", "grayscale", "sepia", "invert",
    "temperature", "tint", "vignette", "cornerRadius", "shake", "rgbSplit", "grain",
    "fontSize", "letterSpacing", "glow", "loopFrequency", "loopAmplitude", "width", "height", "borderWidth", "radius", "progress"]);
  const options = {
    none: "无", "gentle-shake": "轻微晃动", "gentle-bounce": "轻微跳跃", flicker: "闪烁", "gentle-scale": "轻微缩放",
    linear: "线性", "cubic-out": "三次贝塞尔缓出", "back-out": "回弹", "ease-out": "缓出", "ease-in": "缓入",
    contain: "完整显示", cover: "裁切填满", stretch: "拉伸填满", normal: "正常", multiply: "正片叠底", screen: "滤色", overlay: "叠加",
    lighter: "变亮", "soft-light": "柔光", "hard-light": "强光", "color-dodge": "颜色减淡", darken: "变暗", lighten: "变亮", difference: "差值",
    left: "左", center: "居中", right: "右", top: "顶部", middle: "中部", bottom: "底部",
    auto: "自动", ltr: "从左到右", rtl: "从右到左", fade: "淡入淡出", "slide-left": "向左滑动", "slide-right": "向右滑动",
    "slide-up": "向上滑动", "slide-down": "向下滑动", zoom: "缩放", wipe: "向左擦除", "wipe-right": "向右擦除",
    "wipe-up": "向上擦除", "wipe-down": "向下擦除", iris: "光圈", spin: "旋转", blur: "模糊", whip: "甩动", glitch: "故障", pop: "弹出",
    cinematic: "电影感", "teal-orange": "青橙", noir: "黑白", vintage: "复古", faded: "褪色", warm: "暖色", cold: "冷色", dreamy: "梦幻", retro: "怀旧", "bw-soft": "柔和黑白", cyberpunk: "赛博朋克", sunset: "日落", midnight: "午夜",
    typewriter: "打字机", "word-pop": "逐词弹出", "word-slide": "逐词滑入", karaoke: "卡拉 OK", "letter-pop": "逐字弹出", wave: "波浪", bounce: "弹跳", shake: "抖动", "clip-reveal": "裁切显现", "zoom-in": "缩放进入", "font-cut": "字体切换", "rise-mask": "上升遮罩",
  };
  const aliases = {
    Preference: "偏好方案", Source: "素材来源", "Start (s)": "开始（秒）", "Length (s)": "时长（秒）",
    Transform: "变换", Layout: "布局", "Filter / Color": "滤镜与颜色", "Motion FX": "运动效果",
    "Keying / Cut-out": "抠像与移除背景", "Audio / Time": "音频与时间", Transition: "转场", In: "入场", Out: "出场",
    Text: "文字", Font: "字体", "Text style": "文字样式", "Title & caption": "标题与字幕", Component: "组件",
    "Position X": "位置 X", "Position Y": "位置 Y", "Crop L/R %": "左右裁剪（%）", "Crop T/B %": "上下裁剪（%）",
    "Key color": "抠像颜色", Channel: "声道", Content: "文本内容", "Font size": "字号", "Max size": "最大字号",
    "Box W/H": "文本框宽高", Color: "颜色", Align: "水平对齐", "V-align": "垂直对齐", Direction: "文字方向",
    Family: "字体家族", "Google font": "Google 字体", Weight: "字重", Bold: "粗体", Italic: "斜体", Uppercase: "大写",
    "Stroke col.": "描边颜色", "Bg color": "背景颜色", "Glow color": "发光颜色", "Title style": "标题样式", Animation: "文字动画",
    "Adjustment layer": "调整图层", "Clip —": "片段 —", Title: "标题", "Back color": "后层颜色", "Mid color": "中层颜色", "Front color": "前层颜色",
    "Back opacity": "后层不透明度", "Mid opacity": "中层不透明度", "Front opacity": "前层不透明度", "Scale to fit": "缩放以适配",
  };
  const enLabels = {
    x: "Position X", y: "Position Y", scale: "Scale", rotation: "Rotation", opacity: "Opacity",
    volume: "Volume", pan: "Pan", speed: "Speed", brightness: "Brightness", contrast: "Contrast",
    saturation: "Saturation", hue: "Hue", blur: "Blur", grayscale: "Grayscale", sepia: "Sepia",
    invert: "Invert", temperature: "Temperature", tint: "Tint", vignette: "Vignette", fit: "Fit mode",
    blend: "Blend mode", filterPreset: "Filter preset", cropL: "Left crop", cropR: "Right crop",
    cropT: "Top crop", cropB: "Bottom crop", cornerRadius: "Corner radius", flipH: "Flip horizontal",
    flipV: "Flip vertical", chromaKey: "Chroma key color", chromaTolerance: "Chroma tolerance",
    chromaSoftness: "Chroma softness", bgRemove: "AI background removal", shake: "Camera shake",
    shakeSpeed: "Shake speed", rgbSplit: "RGB split", grain: "Grain", text: "Text",
    fontSize: "Font size", color: "Color", color2: "Gradient color", font: "Font", bold: "Bold",
    italic: "Italic", weight: "Weight", align: "Horizontal alignment", letterSpacing: "Letter spacing",
    lineHeight: "Line height", uppercase: "Uppercase", textShadow: "Text shadow", glow: "Glow",
    glowColor: "Glow color", textAnim: "Text animation", wordRate: "Word rate", direction: "Text direction",
    strokeWidth: "Stroke width", strokeColor: "Stroke color", bgColor: "Background color", bgOpacity: "Background opacity",
    boxW: "Text box width", boxH: "Text box height", boxFit: "Scale to fit", vAlign: "Vertical alignment",
    width: "Width", height: "Height", borderWidth: "Border width", radius: "Corner radius", progress: "Progress",
    duration: "Display duration (s)", start: "Start (s)", transitionIn: "In", transitionOut: "Out",
    loopAnimation: "Loop animation", loopFrequency: "Loop frequency", loopAmplitude: "Loop amplitude",
  };
  const enOptions = {
    none: "None", "gentle-shake": "Gentle shake", "gentle-bounce": "Gentle bounce", flicker: "Flicker", "gentle-scale": "Gentle scale",
    linear: "Linear", "cubic-out": "Cubic bezier ease out", "back-out": "Back out", "ease-out": "Ease out", "ease-in": "Ease in",
    contain: "Contain", cover: "Cover", stretch: "Stretch", normal: "Normal", multiply: "Multiply", screen: "Screen", overlay: "Overlay",
    lighter: "Lighter", "soft-light": "Soft light", "hard-light": "Hard light", "color-dodge": "Color dodge", darken: "Darken", lighten: "Lighten", difference: "Difference",
    left: "Left", center: "Center", right: "Right", top: "Top", middle: "Middle", bottom: "Bottom",
    auto: "Auto", ltr: "Left to right", rtl: "Right to left", fade: "Fade", "slide-left": "Slide left", "slide-right": "Slide right",
    "slide-up": "Slide up", "slide-down": "Slide down", zoom: "Zoom", wipe: "Wipe left", "wipe-right": "Wipe right",
    "wipe-up": "Wipe up", "wipe-down": "Wipe down", iris: "Iris", spin: "Spin", blur: "Blur", whip: "Whip", glitch: "Glitch", pop: "Pop",
  };
  const locales = {
    "zh-CN": { labels: zhLabels, aliases, options },
    en: { labels: enLabels, aliases: {}, options: enOptions },
  };
  function resolveLocale(locale) {
    const value = String(locale || window.edwardLocale || "zh-CN").toLowerCase();
    return value.startsWith("zh") ? "zh-CN" : "en";
  }
  function label(key, locale) {
    const dictionary = locales[resolveLocale(locale)];
    return dictionary.labels[key] || dictionary.aliases[key] || String(key).replace(/([a-z])([A-Z])/g, "$1 $2");
  }
  function option(value, locale) {
    const dictionary = locales[resolveLocale(locale)];
    return dictionary.options[String(value)] || String(value);
  }
  function defaultValue(key, fallback) { return Object.hasOwn(defaults, key) ? defaults[key] : fallback; }
  window.fablecutPropertyContract = { locales, labels: zhLabels, defaults, animatable, options, resolveLocale, label, option, defaultValue };
})();
