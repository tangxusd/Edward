#include "edward/plugins/plugin_trust_store.hpp"

#include <sodium.h>

#include <algorithm>

namespace edward::plugins {
namespace {

void appendField(QByteArray& target, const QString& value) {
  const auto utf8 = value.toUtf8();
  target.append(QByteArray::number(utf8.size()));
  target.append(':');
  target.append(utf8);
}

void appendList(QByteArray& target, QStringList values) {
  std::sort(values.begin(), values.end());
  target.append(QByteArray::number(values.size()));
  target.append(':');
  for (const auto& value : values) appendField(target, value);
}

}  // namespace

QByteArray canonicalPluginManifestPayload(const PluginManifest& manifest) {
  QByteArray payload{"edward-plugin-manifest-v1"};
  appendField(payload, manifest.pluginId);
  appendField(payload, manifest.version);
  appendField(payload, manifest.entry);
  appendField(payload, manifest.runtime);
  appendList(payload, manifest.capabilities);
  appendList(payload, manifest.permissions);
  appendList(payload, manifest.editableProps);
  return payload;
}

PluginTrustStore::PluginTrustStore(std::map<QString, PluginPublicKey> publicKeys)
    : publicKeys_(std::move(publicKeys)) {}

bool PluginTrustStore::verify(const QByteArray& bytes, const PluginSignature& signature,
                              const QString& keyId) const {
  if (sodium_init() < 0) return false;
  const auto key = publicKeys_.find(keyId);
  if (key == publicKeys_.end()) return false;
  return crypto_sign_verify_detached(signature.data(),
                                     reinterpret_cast<const unsigned char*>(bytes.constData()), bytes.size(),
                                     key->second.data()) == 0;
}

bool PluginTrustStore::verifyManifest(const PluginManifest& manifest) const {
  const auto signature = decodeBase64Signature(manifest.signatureBase64);
  return signature && verify(canonicalPluginManifestPayload(manifest), *signature, manifest.signingKeyId);
}

std::optional<PluginSignature> PluginTrustStore::decodeBase64Signature(const QString& value) {
  PluginSignature signature{};
  std::size_t decodedLength = 0;
  const auto encoded = value.toUtf8();
  if (sodium_base642bin(signature.data(), signature.size(), encoded.constData(), encoded.size(), nullptr,
                        &decodedLength, nullptr, sodium_base64_VARIANT_ORIGINAL) != 0 ||
      decodedLength != signature.size()) {
    return std::nullopt;
  }
  return signature;
}

}  // namespace edward::plugins
