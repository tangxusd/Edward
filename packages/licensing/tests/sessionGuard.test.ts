import { describe, expect, it, vi } from 'vitest';
import { canEditOffline, canUseAi, type EntitlementCache } from '../src/index.js';

const checkedAt = Date.parse('2026-08-07T00:00:00.000Z');
const activeCache: EntitlementCache = { accountId: 'account-1', deviceIdHash: 'device-1', sessionId: 'session-1', status: 'active', checkedAt, validUntil: checkedAt + 2 * 60 * 60 * 1000 };

describe('local entitlement guard', () => {
  it('allows editing within the two-hour offline window only', () => {
    expect(canEditOffline(activeCache, checkedAt + 119 * 60 * 1000)).toBe(true);
    expect(canEditOffline(activeCache, checkedAt + 121 * 60 * 1000)).toBe(false);
  });

  it('revalidates AI use after the cache expires', async () => {
    const validate = vi.fn().mockResolvedValue({ ...activeCache, checkedAt: checkedAt + 3 * 60 * 60 * 1000, validUntil: checkedAt + 5 * 60 * 60 * 1000 });
    await expect(canUseAi(activeCache, checkedAt + 3 * 60 * 60 * 1000, { validate })).resolves.toBe(true);
    expect(validate).toHaveBeenCalledTimes(1);
  });

  it('rejects revoked sessions even when the time window has not elapsed', async () => {
    const validate = vi.fn().mockResolvedValue({ ...activeCache, status: 'revoked' });
    await expect(canUseAi(activeCache, checkedAt + 3 * 60 * 60 * 1000, { validate })).resolves.toBe(false);
  });

  it('rejects an entitlement issued for a different device', async () => {
    const now = checkedAt + 3 * 60 * 60 * 1000;
    const validate = vi.fn().mockResolvedValue({ ...activeCache, deviceIdHash: 'device-2', checkedAt: now, validUntil: now + 2 * 60 * 60 * 1000 });

    await expect(canUseAi(activeCache, checkedAt + 3 * 60 * 60 * 1000, { validate })).resolves.toBe(false);
  });
});
