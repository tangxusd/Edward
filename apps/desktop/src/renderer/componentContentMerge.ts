function typeOf(value: unknown): string {
  if (Array.isArray(value)) return 'array';
  if (value === null) return 'null';
  return typeof value;
}

export function mergeComponentContent(current: unknown, draft: unknown, field = ''): unknown {
  const currentType = typeOf(current);
  const draftType = typeOf(draft);
  if (currentType !== draftType) throw new Error(`字段“${field || '内容'}”类型不兼容`);
  if (Array.isArray(current) && Array.isArray(draft)) {
    if (!current.length) return draft;
    return draft.map((item, index) => mergeComponentContent(current[Math.min(index, current.length - 1)], item, `${field}[${index}]`));
  }
  if (currentType !== 'object' || current === null || draft === null) return draft;
  const source = current as Record<string, unknown>;
  const patch = draft as Record<string, unknown>;
  for (const key of Object.keys(patch)) if (!(key in source)) throw new Error(`字段“${key}”不在组件数据格式中`);
  return Object.fromEntries(Object.entries(source).map(([key, value]) => [key, key in patch ? mergeComponentContent(value, patch[key], key) : value]));
}

export function describeComponentChanges(current: unknown, next: unknown, field = ''): string[] {
  if (Object.is(current, next)) return [];
  if (Array.isArray(current) || Array.isArray(next)) return [field || '内容'];
  if (typeOf(current) === 'object' && current !== null && typeOf(next) === 'object' && next !== null) {
    const before = current as Record<string, unknown>;
    const after = next as Record<string, unknown>;
    return Object.keys(before).flatMap((key) => describeComponentChanges(before[key], after[key], field ? `${field}.${key}` : key));
  }
  return [field || '内容'];
}
