import { assert, assertStringIncludes } from "https://deno.land/std@0.224.0/assert/mod.ts";

const readSource = (path: string) => Deno.readTextFile(new URL(path, import.meta.url));
const migration = await readSource("../migrations/202609210001_component_library_lifecycle.sql");
const publishMigration = await readSource("../migrations/202609210002_atomic_resource_publish.sql");
const semverMigration = await readSource("../migrations/202609200005_fix_resource_version_semver.sql");
const versionVisibilityMigration = await readSource("../migrations/202609210003_resource_version_visibility.sql");
const catalog = await readSource("../functions/resource-catalog/index.ts");
const detail = await readSource("../functions/resource-detail/index.ts");
const favorite = await readSource("../functions/resource-favorite/index.ts");

Deno.test("组件资源以唯一用户收藏原子维护热门计数", () => {
  assertStringIncludes(migration, "favorite_count = favorite_count + 1");
  assertStringIncludes(migration, "favorite_count = greatest(favorite_count - 1, 0)");
  assertStringIncludes(migration, "resource_favorites_favorite_count");
  assertStringIncludes(migration, "recount_resource_favorite_counts");
});

Deno.test("资源版本号使用稳定的三段语义版本校验", () => {
  assertStringIncludes(semverMigration, "resource_versions_version_check");
  assertStringIncludes(semverMigration, "^[0-9]+[.][0-9]+[.][0-9]+$");
});

Deno.test("资源版本元数据的读取权限跟随资源可见性", () => {
  assertStringIncludes(versionVisibilityMigration, "drop policy if exists resource_versions_read_published");
  assertStringIncludes(versionVisibilityMigration, "resources.visibility in ('public', 'unlisted')");
  assertStringIncludes(versionVisibilityMigration, "resources.status = 'published'");
});

Deno.test("资源生命周期只扩展既有 status 并保留已发布版本", () => {
  assertStringIncludes(migration, "status in ('draft', 'published', 'archived', 'withdrawn')");
  assertStringIncludes(migration, "resources_popular_category_idx");
  assertStringIncludes(migration, "revoke all on function public.recount_resource_favorite_counts()");
});

Deno.test("目录的固定筛选与业务分类可以组合", () => {
  assertStringIncludes(catalog, 'filter === "favorites"');
  assertStringIncludes(catalog, 'filter === "popular"');
  assert(!catalog.includes("During a rolling deployment"));
  assertStringIncludes(catalog, "categoryId");
  assertStringIncludes(catalog, "is_favorite");
  assertStringIncludes(catalog, "preview_video_path");
  assertStringIncludes(catalog, "invalid_category");
  assertStringIncludes(catalog, "boundedInteger");
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
  assertStringIncludes(favorite, '.eq("status", "published")');
  assertStringIncludes(favorite, '.in("visibility", ["public", "unlisted"])');
  assertStringIncludes(favorite, 'error: "resource_unavailable"');
});

Deno.test("发布版本在单个数据库事务中绑定资源身份和不可变版本", () => {
  assertStringIncludes(publishMigration, "function public.publish_resource_version");
  assertStringIncludes(publishMigration, "on conflict (resource_id, version) do nothing");
  assertStringIncludes(publishMigration, "resource_version_conflict");
  assertStringIncludes(publishMigration, "grant execute on function public.publish_resource_version");
});
