#pragma once

#include "edward/core/timeline.hpp"
#include "edward/media/render_graph.hpp"

#include <QSize>

#include <filesystem>
#include <optional>

namespace edward::media {

enum class ExportQuality { High, Medium, Low };

struct ExportRequest {
  std::filesystem::path outputPath;
  QSize outputSize{1920, 1080};
  int fpsNumerator = 25;
  int fpsDenominator = 1;
  ExportQuality quality = ExportQuality::High;
};

struct ExportResult {
  std::filesystem::path outputPath;
  edward::core::Frame frameCount = 0;
};

class ExportJob {
 public:
  explicit ExportJob(const RenderGraph& graph);
  std::optional<ExportResult> run(const edward::core::TimelineSnapshot& snapshot,
                                  const ExportRequest& request) const;

 private:
  const RenderGraph& graph_;
};

}  // namespace edward::media
