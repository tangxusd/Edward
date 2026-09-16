#pragma once

#include "edward/runtime/render_request.hpp"
#include "edward/runtime/runtime_manifest.hpp"

#include <QJsonObject>
#include <QString>

#include <cstdint>

namespace edward::runtime {

struct HostResult final {
  bool ok = false;
  QString error;
  [[nodiscard]] static HostResult success() { return {true, {}}; }
  [[nodiscard]] static HostResult failure(QString message) { return {false, std::move(message)}; }
};

class WebRuntimeHost final {
 public:
  HostResult mount(const RuntimeManifest& manifest, const QString& packageRoot,
                  const QJsonObject& props);
  HostResult setFrame(std::int64_t frame);
  HostResult setProps(const QJsonObject& props);
  HostResult renderFrame(std::int64_t frame, const QString& outputPath);
  void unmount();

  [[nodiscard]] bool mounted() const { return mounted_; }
  [[nodiscard]] std::int64_t frame() const { return frame_; }
  [[nodiscard]] const QJsonObject& props() const { return props_; }
  [[nodiscard]] const QString& packageRoot() const { return packageRoot_; }

 private:
  const RuntimeManifest* manifest_ = nullptr;
  QString packageRoot_;
  QJsonObject props_;
  std::int64_t frame_ = 0;
  bool mounted_ = false;
};

}  // namespace edward::runtime
