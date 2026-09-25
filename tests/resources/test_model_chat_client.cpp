#include <edward/resources/model_chat_client.hpp>

#include <QJsonArray>
#include <cassert>

int main() {
  QString error;
  const auto request = edward::resources::ModelChatClient::buildRequest(
      {"https://api.example.com/v1/chat/completions", "runtime-key", "deepseek-chat"},
      "Return one JSON edit command.", "Move the component left.", &error);
  assert(request);
  assert(request->body.value("model").toString() == "deepseek-chat");
  assert(request->body.value("messages").toArray().size() == 2);
  assert(request->headers.value("Authorization") == "Bearer runtime-key");
  const auto responsesRequest = edward::resources::ModelChatClient::buildRequest(
      {"https://api.openai.com/v1/responses", "runtime-key", "gpt-5", "openai-responses"}, "System", "User", &error);
  assert(responsesRequest);
  assert(responsesRequest->body.value("instructions") == "System");
  assert(responsesRequest->body.value("input") == "User");
  assert(responsesRequest->headers.value("Authorization") == "Bearer runtime-key");
  const auto anthropicRequest = edward::resources::ModelChatClient::buildRequest(
      {"https://api.anthropic.com/v1/messages", "runtime-key", "claude-sonnet-4-5", "anthropic-messages"}, "System", "User", &error);
  assert(anthropicRequest);
  assert(anthropicRequest->body.value("system") == "System");
  assert(anthropicRequest->body.value("max_tokens") == 256);
  assert(anthropicRequest->headers.value("x-api-key") == "runtime-key");
  assert(!edward::resources::ModelChatClient::buildRequest(
      {"http://api.example.com", "key", "model"}, "system", "user", &error));
  assert(!edward::resources::ModelChatClient::buildRequest(
      {"https://api.example.com", "", "model"}, "system", "user", &error));
  const auto text = edward::resources::ModelChatClient::extractAssistantText(
      QJsonObject{{"choices", QJsonArray{QJsonObject{{"message", QJsonObject{{"content", QStringLiteral("{\\\"operation\\\":\\\"setProperty\\\"}")}}}}}}}, "openai-completions", &error);
  assert(text && text->contains("setProperty"));
  assert(!edward::resources::ModelChatClient::extractAssistantText(QJsonObject{}, "openai-completions", &error));
  const auto responseText = edward::resources::ModelChatClient::extractAssistantText(
      QJsonObject{{"output_text", "response output"}}, "openai-responses", &error);
  assert(responseText && *responseText == "response output");
  const auto anthropicText = edward::resources::ModelChatClient::extractAssistantText(
      QJsonObject{{"content", QJsonArray{QJsonObject{{"type", "text"}, {"text", "anthropic output"}}}}}, "anthropic-messages", &error);
  assert(anthropicText && *anthropicText == "anthropic output");
  const auto models = edward::resources::ModelChatClient::extractModelIds(
      QJsonObject{{"data", QJsonArray{QJsonObject{{"id", "deepseek-chat"}}, QJsonObject{{"id", "deepseek-reasoner"}}}}}, &error);
  assert(models.size() == 2);
  assert(models.at(0) == "deepseek-chat");
  assert(edward::resources::ModelChatClient::modelsEndpoint("https://api.example.com/v1/chat/completions") == "https://api.example.com/v1/models");
  assert(edward::resources::ModelChatClient::modelsEndpoint("https://api.anthropic.com/v1/messages") == "https://api.anthropic.com/v1/models");
  assert(edward::resources::ModelChatClient::extractModelIds(QJsonObject{}, &error).isEmpty());
  const auto openaiEvent = edward::resources::ModelChatClient::parseStreamingLine(
      "data: {\"choices\":[{\"delta\":{\"content\":\"片\"}}],\"sequence\":2}", "openai-completions", &error);
  assert(openaiEvent && openaiEvent->text == "片" && openaiEvent->upstreamSequence == 2);
  const auto responseEvent = edward::resources::ModelChatClient::parseStreamingLine(
      "data: {\"type\":\"response.output_text.delta\",\"delta\":\"段\",\"index\":3}", "openai-responses", &error);
  assert(responseEvent && responseEvent->text == "段" && responseEvent->upstreamSequence == 3);
  const auto anthropicEvent = edward::resources::ModelChatClient::parseStreamingLine(
      "data: {\"type\":\"content_block_delta\",\"delta\":{\"text\":\"落\"}}", "anthropic-messages", &error);
  assert(anthropicEvent && anthropicEvent->text == "落");
  const auto doneEvent = edward::resources::ModelChatClient::parseStreamingLine("data: [DONE]", "openai-completions", &error);
  assert(doneEvent && doneEvent->done);
  const auto errorEvent = edward::resources::ModelChatClient::parseStreamingLine(
      "data: {\"type\":\"error\",\"error\":{\"message\":\"bad\"}}", "openai-completions", &error);
  assert(errorEvent && errorEvent->error);
  assert(!edward::resources::ModelChatClient::parseStreamingLine(": heartbeat", "openai-completions", &error));
  return 0;
}
