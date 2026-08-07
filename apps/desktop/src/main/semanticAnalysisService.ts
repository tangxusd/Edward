import { AiEditPlanSchema, type AiEditPlan } from '@ai-video/domain';

export type SemanticRequest = { baseUrl: string; modelId: string; apiKey: string; script: string; transcript: Array<{ start: number; end: number; text: string }> };

export async function analyzeSemantics(request: SemanticRequest): Promise<AiEditPlan> {
  const response = await fetch(`${request.baseUrl.replace(/\/$/, '')}/chat/completions`, {
    method: 'POST', headers: { 'content-type': 'application/json', authorization: `Bearer ${request.apiKey}` },
    body: JSON.stringify({ model: request.modelId, messages: [{ role: 'user', content: JSON.stringify({ script: request.script, transcript: request.transcript }) }], response_format: { type: 'json_object' } }),
  });
  if (!response.ok) throw new Error(`semantic analysis failed: ${response.status}`);
  const data = await response.json() as { choices?: Array<{ message?: { content?: string } }> };
  const content = data.choices?.[0]?.message?.content;
  if (!content) throw new Error('semantic analysis returned no content');
  return AiEditPlanSchema.parse(JSON.parse(content));
}
