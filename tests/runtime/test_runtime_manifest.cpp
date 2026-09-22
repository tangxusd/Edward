#include <edward/runtime/runtime_manifest.hpp>

#include <QJsonArray>
#include <QJsonObject>

#include <cassert>

namespace {

QJsonObject validManifest() {
  return QJsonObject{{"protocol", "edward.web-runtime.v1"},
                     {"runtime", "react"},
                     {"width", 1920},
                     {"height", 1080},
                     {"fps", 30.0},
                     {"durationInFrames", 90},
                     {"previewEntry", "preview/index.html"},
                     {"renderEntry", "render/index.html"},
                     {"propsSchema", QJsonObject{{"type", "object"}}},
                     {"editableProperties", QJsonArray{"title", "opacity"}}};
}

void test_acceptsSupportedRuntimeManifest() {
  QString error;
  const auto manifest = edward::runtime::RuntimeManifest::parse(validManifest(), &error);
  assert(manifest);
  assert(error.isEmpty());
  assert(manifest->runtime == "react");
  assert(manifest->width == 1920);
  assert(manifest->durationInFrames == 90);
  assert(manifest->editableProperties.size() == 2);
}

void test_rejectsMissingEntryAndNonPositiveMetadata() {
  QString error;
  auto missingEntry = validManifest();
  missingEntry.remove("previewEntry");
  assert(!edward::runtime::RuntimeManifest::parse(missingEntry, &error));

  for (const char* key : {"width", "height", "fps", "durationInFrames"}) {
    auto invalid = validManifest();
    invalid.insert(QString::fromLatin1(key), 0);
    assert(!edward::runtime::RuntimeManifest::parse(invalid, &error));
  }
}

void testRejectsUnexpectedFields() {
  QString error;
  const auto withField = [](const char* key, const QJsonValue& value) {
    auto object = validManifest();
    object.insert(QString::fromLatin1(key), value);
    return object;
  };
  for (const QJsonObject& invalid : {withField("unexpectedPayload", QJsonObject{}),
                                     withField("command", "node host.js"),
                                     withField("previewEntry", "/tmp/preview.html"),
                                     withField("renderEntry", "../render/index.html"),
                                     withField("previewEntry", "C:\\preview.html")}) {
    assert(!edward::runtime::RuntimeManifest::parse(invalid, &error));
  }
}

}  // namespace

int main() {
  test_acceptsSupportedRuntimeManifest();
  test_rejectsMissingEntryAndNonPositiveMetadata();
  testRejectsUnexpectedFields();
  return 0;
}
