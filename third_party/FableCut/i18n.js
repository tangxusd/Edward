/* Lightweight UI localization. User data is never translated. */
(function () {
  const zh = {"FableCut — Video Editor":"FableCut — 视频编辑器","Untitled Project":"未命名项目","Assets":"资源","Program Monitor":"节目监视器","Project":"项目","Elements":"元素","Sound FX":"音效","Export":"导出","Import":"导入","Cancel":"取消","Close":"关闭","Settings":"设置","Help":"帮助","Timeline":"时间线","+ Title":"+ 标题","+ Adjust":"+ 调整层","+ Component":"+ 组件","+ Import":"+ 导入","+ URL":"+ URL","No assets yet.":"暂无资源","Transform":"变换","Layout":"布局","Filter / Color":"滤镜 / 画面调节","Motion FX":"运动效果","Audio / Time":"音频 / 时间","Transition":"转场","Text":"文本","Font":"字体","Name":"名称","Source":"来源","Start (s)":"开始（秒）","Length (s)":"时长（秒）","Position X":"位置 X","Position Y":"位置 Y","Scale":"缩放","Rotation":"旋转","Opacity":"不透明度","Blend":"混合","Brightness":"亮度","Contrast":"对比度","Saturation":"饱和度","Hue":"色相","Blur":"模糊","Temperature":"色温","Tint":"色调","Grayscale":"灰度","Sepia":"棕褐色","Invert":"反相","Vignette":"暗角","Volume":"音量","Pan":"声像","Speed":"速度","Color":"颜色","Title":"标题","Content":"内容","Family":"字体","Weight":"字重","Bold":"粗体","Italic":"斜体","Align":"对齐","Fit":"适配","Preset":"预设","Flip H":"水平翻转","Flip V":"垂直翻转","Exporting…":"正在导出…","Rendering…":"正在渲染…","Encoding…":"正在编码…","video":"视频","audio":"音频","image":"图片","component":"组件","none":"无","contain":"适应","cover":"覆盖","stretch":"拉伸","normal":"正常","fade":"淡入淡出","slide-left":"向左滑动","slide-right":"向右滑动","slide-up":"向上滑动","slide-down":"向下滑动","zoom":"缩放","blur":"模糊","pop":"弹出"};
  Object.assign(zh, {"Add audio track":"添加音频轨道","Add video track":"添加视频轨道","Remove track":"删除轨道","Enable track":"启用轨道","Disable track":"禁用轨道","Solo track":"独奏轨道","Close gap":"闭合间隙","Find gap":"查找间隙","Split":"切分","Trim":"修剪","Keyboard shortcuts":"键盘快捷键","Import from URL":"从 URL 导入","Media URL":"媒体 URL","Encoding profile":"编码配置","Fast (ffmpeg)":"快速（ffmpeg）","Realtime (in-browser)":"实时（浏览器）","WebCodecs (HW encode)":"WebCodecs（硬件编码）","Draft · H.264 fast":"草稿 · H.264 快速","Delivery · H.264 balanced":"交付 · H.264 均衡","High quality · H.264 slow":"高质量 · H.264 慢速","Full canvas":"完整画布","Custom":"自定义","Choose…":"选择…","Delete selected":"删除所选","Delete":"删除","Snap":"吸附","Audio Hold":"音频保持","Audio hold":"音频保持","Shake":"抖动","SHAKE":"抖动","Inspector":"属性检查器","INSPECTOR":"属性检查器","Adjust":"调整层","ADJUST":"调整层","Adjustment layer":"调整层","ADJUSTMENT LAYER":"调整层","Frame":"画面框","FRAME":"画面框","Safe":"安全区","SAFE":"安全区","or click":"或点击","wipe":"擦除","wipe-right":"向右擦除","wipe-up":"向上擦除","wipe-down":"向下擦除","iris":"光圈","spin":"旋转","whip":"甩动","glitch":"故障","pop":"弹出","Slide":"滑动","center":"居中","left":"左对齐","right":"右对齐","justify":"两端对齐","auto":"自动","ltr":"从左到右","rtl":"从右到左","Title":"标题","Box W/H":"框宽/高","flat":"纯色","Select a clip first":"请先选择片段","Timeline is empty — add some clips first.":"时间线为空——请先添加片段。","Drop video, audio or images here":"将视频、音频或图片拖到这里","Then drag media onto the timeline below.":"然后将素材拖到下面的时间线上。","Preferences are stored in this browser only.":"偏好设置仅保存在当前浏览器中。","New folder":"新建文件夹","New subfolder":"新建子文件夹","Add at playhead":"在播放头处添加","Random style":"随机样式","Scale to fit":"缩放适配","Keying / Cut-out":"抠像 / 抠图","Text style":"文本样式","Title & caption":"标题和字幕","Title &amp; caption":"标题和字幕","Title style":"标题样式","No gaps found":"未找到间隙","No keyframes":"没有关键帧","No previous keyframe":"没有上一个关键帧","No next keyframe":"没有下一个关键帧","Unknown track":"未知轨道","Track has clips":"轨道包含片段","Remove empty track":"删除空轨道","Close graph":"关闭曲线图","Show / hide keyframe graph":"显示 / 隐藏关键帧曲线图","Select a clip to edit its":"选择片段以编辑其","No assets yet.":"暂无资源","Interface language":"界面语言","User component layers":"用户组件图层"});
  Object.assign(zh, {
    "Edward settings are stored on this device.":"Edward 设置保存在此设备上。",
    "AI model":"大模型设置",
    "Provider":"供应商",
    "API endpoint":"API 端点",
    "API Key":"API 密钥",
    "Model ID":"模型 ID",
    "OpenAI-compatible provider":"兼容 OpenAI 的供应商",
    "Leave blank to keep the saved key":"留空则保留已保存的密钥",
    "Paths":"路径设置",
    "Export directory":"导出目录",
    "Choose":"选择",
    "Exports use this directory. Choose another directory whenever needed.":"导出默认使用此目录，仍可在导出时另行选择。",
    "Preferences":"偏好设置",
    "Preference data stays on this device and is never included in project files.":"偏好数据仅保存在此设备，不会写入项目文件。",
    "Local database":"本地数据库",
    "Save and compile preferences":"保存并编译偏好",
    "Save settings":"保存设置",
    "Edward desktop settings are unavailable in this browser.":"当前浏览器无法使用 Edward 桌面设置。",
    "Settings saved.":"设置已保存。",
    "Settings could not be saved. Check the HTTPS endpoint and path.":"设置无法保存，请检查 HTTPS 端点和目录。",
    "Preferences saved and compiled.":"偏好已保存并编译。",
    "Preferences could not be compiled.":"偏好无法编译。",
    "Login":"登录",
    "Register":"注册",
    "Username":"用户名",
    "Forgot password":"找回密码"
  });
  const en = Object.fromEntries(Object.entries(zh).map(([k,v]) => [v,k]));
  const dicts = {"zh-CN":zh,"en-US":en}; let language = localStorage.getItem("fablecut-language") || "zh-CN";
  const t = (value) => { let out = String(value ?? ""); for (const [from, to] of Object.entries(dicts[language]).sort((a,b) => b[0].length - a[0].length)) out = out.split(from).join(to); return out; };
  function translate(root=document.body) { const w=document.createTreeWalker(root,NodeFilter.SHOW_TEXT), a=[]; let n; while((n=w.nextNode())) a.push(n); for(const x of a){const raw=x.nodeValue, s=raw.trim(); if(!s||x.parentElement?.closest("script,style,textarea,[data-i18n-ignore]")) continue; const v=t(s); if(v!==s)x.nodeValue=raw.replace(s,v);} root.querySelectorAll("[title],[aria-label]").forEach(e=>["title","aria-label"].forEach(k=>{const v=e.getAttribute(k), n=t(v); if(n!==v)e.setAttribute(k,n);})); document.documentElement.lang=language; }
  function setLanguage(next){ language=dicts[next]?next:"zh-CN"; localStorage.setItem("fablecut-language",language); translate(); window.dispatchEvent(new CustomEvent("fablecut-language-change",{detail:language})); }
  window.fablecutI18n={t,translate,setLanguage,getLanguage:()=>language};
  document.addEventListener("DOMContentLoaded",()=>{translate();new MutationObserver(()=>translate()).observe(document.body,{childList:true,subtree:true});});
})();
