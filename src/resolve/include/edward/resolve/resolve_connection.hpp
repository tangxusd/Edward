#pragma once

#include "edward/resolve/resolve_types.hpp"

#include <QJsonObject>
#include <QUrl>

#include <optional>

namespace edward::resolve {

class ResolveConnection final {
 public:
  explicit ResolveConnection(int timeoutMs = 1000);
  ~ResolveConnection();

  ResolveConnection(const ResolveConnection&) = delete;
  ResolveConnection& operator=(const ResolveConnection&) = delete;

  bool connectToBridge(const QUrl& url, QString* error = nullptr);
  void disconnect();
  [[nodiscard]] ConnectionState state() const;
  [[nodiscard]] std::optional<QJsonObject> call(const QString& method,
                                                const QJsonObject& params,
                                                ResolveError* error = nullptr);

 private:
  class Impl;
  Impl* impl_;
};

}  // namespace edward::resolve
