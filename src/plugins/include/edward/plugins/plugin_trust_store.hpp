#pragma once

#include "edward/plugins/plugin_manifest.hpp"

#include <QByteArray>
#include <QString>

#include <array>
#include <map>
#include <optional>

namespace edward::plugins {

using PluginPublicKey = std::array<unsigned char, 32>;
using PluginSignature = std::array<unsigned char, 64>;

QByteArray canonicalPluginManifestPayload(const PluginManifest& manifest);

class PluginTrustStore final {
 public:
  explicit PluginTrustStore(std::map<QString, PluginPublicKey> publicKeys);

  [[nodiscard]] bool verify(const QByteArray& bytes, const PluginSignature& signature,
                            const QString& keyId) const;
  [[nodiscard]] bool verifyManifest(const PluginManifest& manifest) const;
  [[nodiscard]] static std::optional<PluginSignature> decodeBase64Signature(const QString& value);

 private:
  std::map<QString, PluginPublicKey> publicKeys_;
};

}  // namespace edward::plugins
