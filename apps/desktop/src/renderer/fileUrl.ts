export function toLocalFileUrl(path: string): string {
  const normalized = path.replace(/\\/g, '/');
  const absolute = normalized.startsWith('/') ? normalized : `/${normalized}`;
  const encoded = absolute.split('/').map((segment, index) => {
    if (index === 0) return '';
    return encodeURIComponent(segment).replace(/%3A/gi, ':');
  }).join('/');
  return `file://${encoded}`;
}
