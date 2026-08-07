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

export async function hashDeviceSerial(serial: string): Promise<string> {
  const digest = await globalThis.crypto.subtle.digest('SHA-256', new TextEncoder().encode(serial.trim()));
  return Array.from(new Uint8Array(digest), (byte) => byte.toString(16).padStart(2, '0')).join('');
}
