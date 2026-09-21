import { assert, assertStringIncludes } from "https://deno.land/std@0.224.0/assert/mod.ts";

const readSource = (path: string) => Deno.readTextFile(new URL(path, import.meta.url));
const migration = await readSource("../migrations/202609210001_component_library_lifecycle.sql");
const catalog = await readSource("../functions/resource-catalog/index.ts");
const detail = await readSource("../functions/resource-detail/index.ts");
const favorite = await readSource("../functions/resource-favorite/index.ts");

Deno.test("组件资源以唯一用户收藏原子维护热门计数", () => {
  assertStringIncludes(migration, "favorite_count = favorite_count + 1");
  assertStringIncludes(migration, "favorite_count = greatest(favorite_count - 1, 0)");
  assertStringIncludes(migration, "resource_favorites_favorite_count");
  assertStringIncludes(migration, "recount_resource_favorite_counts");
});

Deno.test("资源生命周期只扩展既有 status 并保留已发布版本", () => {
  assertStringIncludes(migration, "status in ('draft', 'published', 'archived', 'withdrawn')");
  assertStringIncludes(migration, "resources_popular_category_idx");
  assertStringIncludes(migration, "revoke all on function public.recount_resource_favorite_counts()");
});

Deno.test("目录的固定筛选与业务分类可以组合", () => {
  assertStringIncludes(catalog, 'filter === "favorites"');
  assertStringIncludes(catalog, 'filter === "popular"');
  assertStringIncludes(catalog, "categoryId");
  assertStringIncludes(catalog, "is_favorite");
  assertStringIncludes(catalog, "preview_video_path");
});

Deno.test("详情返回锁定版本的包和预览签名地址", () => {
  assertStringIncludes(detail, "packageUrl");
  assertStringIncludes(detail, "previewUrl");
  assertStringIncludes(detail, "createSignedUrl");
});

Deno.test("收藏接口只修改用户收藏并读取触发器后的聚合值", () => {
  assertStringIncludes(favorite, "resource_favorites");
  assertStringIncludes(favorite, "favorite_count");
  assert(!favorite.includes("update({ favorite_count"));
});
