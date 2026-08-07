export type EntitlementStatus = 'active' | 'expired' | 'revoked';

export type EntitlementCache = {
  accountId: string;
  deviceIdHash: string;
  sessionId: string;
  status: EntitlementStatus;
  checkedAt: number;
  validUntil: number;
};

export type EntitlementClient = { validate(cache: EntitlementCache): Promise<EntitlementCache> };

export function canEditOffline(cache: EntitlementCache | undefined, now = Date.now()): boolean {
  return Boolean(cache && cache.status === 'active' && now >= cache.checkedAt && now < cache.validUntil);
}

export async function canUseAi(cache: EntitlementCache | undefined, now: number, client: EntitlementClient): Promise<boolean> {
  if (canEditOffline(cache, now)) return true;
  if (!cache) return false;
  try {
    const refreshed = await client.validate(cache);
    if (refreshed.accountId !== cache.accountId || refreshed.deviceIdHash !== cache.deviceIdHash) return false;
    return canEditOffline(refreshed, now);
  } catch {
    return false;
  }
}

export function hashDeviceSerial(serial: string): string {
  let hash = 2166136261;
  for (const char of serial) hash = Math.imul(hash ^ char.charCodeAt(0), 16777619);
  return (hash >>> 0).toString(16).padStart(8, '0');
}
