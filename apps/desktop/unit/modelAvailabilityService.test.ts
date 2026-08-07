import { describe, expect, it, vi } from 'vitest';
import { checkModelAvailability } from '../src/main/modelAvailabilityService.js';

describe('checkModelAvailability', () => {
  it('reports online after the compatible models endpoint accepts the credential', async () => {
    const request = vi.fn().mockResolvedValue(new Response('{}', { status: 200 }));

    await expect(checkModelAvailability({ baseUrl: 'https://model.example.test/v1', apiKey: 'secret-test-value' }, request)).resolves.toEqual({ online: true });
    expect(request).toHaveBeenCalledWith('https://model.example.test/v1/models', {
      headers: { authorization: 'Bearer secret-test-value' },
    });
  });

  it('reports offline when the compatible models endpoint cannot be reached', async () => {
    const request = vi.fn().mockRejectedValue(new Error('network unavailable'));

    await expect(checkModelAvailability({ baseUrl: 'https://model.example.test/v1', apiKey: 'secret-test-value' }, request)).resolves.toEqual({ online: false });
  });
});
