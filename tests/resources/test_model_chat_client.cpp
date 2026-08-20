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
  assert(!edward::resources::ModelChatClient::buildRequest(
      {"http://api.example.com", "key", "model"}, "system", "user", &error));
  assert(!edward::resources::ModelChatClient::buildRequest(
      {"https://api.example.com", "", "model"}, "system", "user", &error));
  const auto text = edward::resources::ModelChatClient::extractAssistantText(
      QJsonObject{{"choices", QJsonArray{QJsonObject{{"message", QJsonObject{{"content", QStringLiteral("{\\\"operation\\\":\\\"setProperty\\\"}")}}}}}}}, &error);
  assert(text && text->contains("setProperty"));
  assert(!edward::resources::ModelChatClient::extractAssistantText(QJsonObject{}, &error));
  return 0;
}
