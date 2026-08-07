export type ModelAvailability = { online: boolean };

type AvailabilityRequest = { baseUrl: string; apiKey: string };
type Fetcher = (input: string, init: RequestInit) => Promise<Response>;

export async function checkModelAvailability(request: AvailabilityRequest, fetcher: Fetcher = fetch): Promise<ModelAvailability> {
  try {
    const response = await fetcher(`${request.baseUrl.replace(/\/$/, '')}/models`, {
      headers: { authorization: `Bearer ${request.apiKey}` },
    });
    return { online: response.ok };
  } catch {
    return { online: false };
  }
}
