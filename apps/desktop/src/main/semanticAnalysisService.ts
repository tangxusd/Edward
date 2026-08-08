import { AiEditPlanSchema, type AiEditPlan } from '@ai-video/domain';

export type ComponentChatRequest = { baseUrl: string; modelId: string; apiKey: string; script: string; componentType: string; content: unknown; message: string; history: Array<{ role: 'user' | 'assistant'; content: string }> };
export type ComponentChatResult = { reply: string; draftContent: unknown };

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

export async function requestComponentContentEdit(request: ComponentChatRequest): Promise<ComponentChatResult> {
  const response = await fetch(`${request.baseUrl.replace(/\/$/, '')}/chat/completions`, {
    method: 'POST', headers: { 'content-type': 'application/json', authorization: `Bearer ${request.apiKey}` },
    body: JSON.stringify({ model: request.modelId, messages: [{ role: 'system', content: 'Return JSON only with reply and draftContent. Modify only component content.' }, ...request.history, { role: 'user', content: JSON.stringify({ script: request.script, componentType: request.componentType, currentContent: request.content, request: request.message }) }], response_format: { type: 'json_object' } }),
  });
  if (!response.ok) throw new Error(`component edit failed: ${response.status}`);
  const data = await response.json() as { choices?: Array<{ message?: { content?: string } }> };
  const content = data.choices?.[0]?.message?.content;
  if (!content) throw new Error('component edit returned no content');
  const result = JSON.parse(content) as ComponentChatResult;
  if (typeof result.reply !== 'string' || !result.reply.trim() || !('draftContent' in result)) throw new Error('component edit returned invalid content');
  return result;
}
