#include <edward/plugins/plugin_trust_store.hpp>

#include <QJsonArray>
#include <QJsonObject>
#include <sodium.h>

#include <cassert>

int main() {
  assert(sodium_init() >= 0);
  const auto manifest = edward::plugins::PluginManifest::parse(
      QJsonObject{{"pluginId", "edward.remotion"}, {"version", "0.3.0"}, {"entry", "host.mjs"},
                  {"runtime", "node"}, {"capabilities", QJsonArray{"describe", "renderFrame"}},
                  {"permissions", QJsonArray{"read_input_asset", "write_draft_output"}},
                  {"editableProps", QJsonArray{"x", "opacity"}}});
  assert(manifest);

  edward::plugins::PluginPublicKey publicKey{};
  std::array<unsigned char, crypto_sign_SECRETKEYBYTES> secretKey{};
  assert(crypto_sign_keypair(publicKey.data(), secretKey.data()) == 0);
  const auto payload = edward::plugins::canonicalPluginManifestPayload(*manifest);
  edward::plugins::PluginSignature signature{};
  assert(crypto_sign_detached(signature.data(), nullptr,
                              reinterpret_cast<const unsigned char*>(payload.constData()), payload.size(),
                              secretKey.data()) == 0);
  const edward::plugins::PluginTrustStore store{{{"edward-plugin-2026", publicKey}}};
  assert(store.verify(payload, signature, "edward-plugin-2026"));
  auto signedManifest = *manifest;
  signedManifest.signingKeyId = QStringLiteral("edward-plugin-2026");
  signedManifest.signatureBase64 = QString::fromLatin1(
      QByteArray(reinterpret_cast<const char*>(signature.data()), signature.size()).toBase64());
  assert(store.verifyManifest(signedManifest));
  signedManifest.version = QStringLiteral("0.3.1");
  assert(!store.verifyManifest(signedManifest));
  assert(!store.verify(payload + "tampered", signature, "edward-plugin-2026"));
  assert(!store.verify(payload, signature, "unknown"));
  assert(!edward::plugins::PluginTrustStore::decodeBase64Signature("invalid").has_value());
  return 0;
}
