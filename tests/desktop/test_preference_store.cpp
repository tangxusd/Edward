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
                        const QString& session, int profileRank = 0) {
  auto result = key;
  result.insert("eventId", eventId);
  result.insert("value", value);
  result.insert("creationSessionId", session);
  result.insert("source", "user-confirmed");
  result.insert("profileRank", profileRank);
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
  const auto facts = store.exportFacts();
  assert(facts.size() == 5);
  assert(!facts.first().toMap().contains("projectId"));
  assert(store.compilePreferences());

  const auto textPreference = store.creationPreferences(text);
  const auto borderPreference = store.creationPreferences(border);
  assert(textPreference.value("value").toString() == "#0000ff");
  assert(borderPreference.value("value").toString() == "#00ff00");

  assert(store.recordConfirmedPropertyChange(observation("a1", text, "#111111", "profile-a", 0)));
  assert(store.recordConfirmedPropertyChange(observation("b1", text, "#222222", "profile-b", 1)));
  assert(store.recordConfirmedPropertyChange(observation("c1", text, "#333333", "profile-c", 2)));
  assert(store.flushPendingPreferences());
  assert(store.compilePreferences());
  assert(store.creationPreferences(text, 0).value("value").toString() == "#0000ff");
  assert(store.creationPreferences(text, 1).value("value").toString() == "#222222");
  assert(store.creationPreferences(text, 2).value("value").toString() == "#333333");

  auto duplicate = observation("e1", text, "#ffffff", "s4");
  assert(store.recordConfirmedPropertyChange(duplicate));
  assert(store.flushPendingPreferences());
  assert(store.compilePreferences());
  assert(store.creationPreferences(text).value("value").toString() == "#0000ff");

  auto forbidden = observation("e6", text, "#ffffff", "s5");
  forbidden.insert("projectId", "must-not-persist");
  assert(!store.recordConfirmedPropertyChange(forbidden));
  assert(store.status().value("pending").toInt() == 0);
  auto invalidRank = observation("e7", text, "#ffffff", "s7", 3);
  assert(!store.recordConfirmedPropertyChange(invalidRank));

  store.setAccountScope("account-a");
  assert(store.recordConfirmedPropertyChange(observation("same-event", text, "#aaaaaa", "account-a")));
  assert(store.flushPendingPreferences());
  store.setAccountScope("account-b");
  assert(store.recordConfirmedPropertyChange(observation("same-event", text, "#bbbbbb", "account-b")));
  assert(store.flushPendingPreferences());
  assert(store.exportFacts().size() == 1);
  assert(store.creationPreferences(text).value("value").toString() == "#bbbbbb");
  store.setAccountScope("account-a");
  assert(store.exportFacts().size() == 1);
  assert(store.creationPreferences(text).value("value").toString() == "#aaaaaa");
  return 0;
}
