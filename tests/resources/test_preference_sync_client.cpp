#include <edward/resources/preference_sync_client.hpp>

#include <cassert>

namespace {
QVariantMap fact(const QString& id, const QString& path, const QVariant& value) {
  return {{"eventId", id}, {"installationId", "install"}, {"componentId", "card6"},
          {"componentFamily", "card"}, {"componentVersion", "1"}, {"manifestHash", "hash"},
          {"semanticPath", "root"}, {"propertyPath", path}, {"valueType", "color"},
          {"value", value}, {"creationSessionId", "session"}, {"source", "user-confirmed"}};
}
}

int main() {
  const auto blue = fact("a", "text.color", "#0000ff");
  const auto red = fact("b", "border.color", "#ff0000");
  const auto merged = edward::resources::PreferenceSyncClient::mergeFacts({blue}, {blue, red});
  assert(merged.size() == 2);
  const auto payload = edward::resources::PreferenceSyncClient::buildUploadPayload(merged);
  QString error;
  const auto roundTrip = edward::resources::PreferenceSyncClient::parseDownloadPayload(payload, &error);
  assert(error.isEmpty());
  assert(roundTrip.size() == 2);
  auto invalid = blue;
  invalid.insert("projectId", "must-not-sync");
  assert(!edward::resources::PreferenceSyncClient::validateFact(invalid, &error));
  assert(!error.isEmpty());
  return 0;
}
