#include <edward/resources/component_upload.hpp>

#include <QJsonObject>
#include <cassert>

int main() {
  const auto component = edward::core::ComponentIr::parse(
      {{"version", "1"}, {"root", QJsonObject{{"id", "root"}, {"type", "container"}}}});
  assert(component);
  const edward::resources::ComponentPackage package{"demo.card", "Demo", *component, {}, {}, {}, {}};
  const edward::resources::AuthSession session{"user-1", "demo@example.com", "token"};
  QString error;
  assert(!edward::resources::ComponentUploadClient::buildRequest("http://localhost/upload", package, session, &error));
  assert(!edward::resources::ComponentUploadClient::buildRequest("https://catalog.example/upload", package,
                                                                  {"", "demo@example.com", "token"}, &error));
  const auto request = edward::resources::ComponentUploadClient::buildRequest(
      "https://catalog.example/upload", package, session, &error);
  assert(request);
  assert(request->body.value("userId").toString() == "user-1");
  assert(request->body.value("username").toString() == "demo@example.com");
  assert(request->body.value("component").toObject().value("version").toString() == "1");
  return 0;
}
