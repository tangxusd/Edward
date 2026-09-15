#include <edward/desktop/preference_store.hpp>

#include <QTemporaryDir>
#include <QCoreApplication>

#include <cassert>

namespace {
QVariantMap identity(const QString& semanticPath, const QString& propertyPath) {
  return QVariantMap{{"componentId", "demo"},
                     {"componentFamily", "annotation"},
                     {"componentVersion", "1"},
                     {"manifestHash", "manifest-a"},
                     {"semanticPath", semanticPath},
                     {"propertyPath", propertyPath},
                     {"valueType", "color"}};
}

QVariantMap observation(const QString& eventId, const QVariantMap& key, const QString& value,
                        const QString& session) {
  auto result = key;
  result.insert("eventId", eventId);
  result.insert("value", value);
  result.insert("creationSessionId", session);
  result.insert("source", "user-confirmed");
  return result;
}
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication application(argc, argv);
  QTemporaryDir directory;
  assert(directory.isValid());
  edward::desktop::PreferenceStore store;
  store.setDatabasePathForTests(directory.filePath("preferences.sqlite"));

  const auto text = identity("root.title", "color");
  const auto border = identity("root.border", "color");
  assert(store.recordConfirmedPropertyChange(observation("e1", text, "#0000ff", "s1")));
  assert(store.recordConfirmedPropertyChange(observation("e2", text, "#0000ff", "s2")));
  assert(store.recordConfirmedPropertyChange(observation("e3", text, "#ff0000", "s3")));
  assert(store.recordConfirmedPropertyChange(observation("e4", border, "#00ff00", "s1")));
  assert(store.recordConfirmedPropertyChange(observation("e5", border, "#00ff00", "s2")));
  assert(store.flushPendingPreferences());
  assert(store.compilePreferences());

  const auto textPreference = store.creationPreferences(text);
  const auto borderPreference = store.creationPreferences(border);
  assert(textPreference.value("value").toString() == "#0000ff");
  assert(borderPreference.value("value").toString() == "#00ff00");

  auto duplicate = observation("e1", text, "#ffffff", "s4");
  assert(store.recordConfirmedPropertyChange(duplicate));
  assert(store.flushPendingPreferences());
  assert(store.compilePreferences());
  assert(store.creationPreferences(text).value("value").toString() == "#0000ff");

  auto forbidden = observation("e6", text, "#ffffff", "s5");
  forbidden.insert("projectId", "must-not-persist");
  assert(!store.recordConfirmedPropertyChange(forbidden));
  assert(store.status().value("pending").toInt() == 0);
  return 0;
}
