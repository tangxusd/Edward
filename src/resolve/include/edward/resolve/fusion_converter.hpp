#pragma once

#include "edward/core/component_conversion_report.hpp"
#include "edward/core/component_ir.hpp"
#include "edward/resolve/resolve_adapter.hpp"
#include "edward/resolve/resolve_types.hpp"

#include <QJsonObject>

namespace edward::resolve {

struct FusionConversionResult final {
  QJsonObject fusionPayload;
  edward::core::ComponentConversionReport report;
  bool transparentVideoFallbackAllowed = false;
};

FusionConversionResult convertComponentToFusion(const edward::core::ComponentIr& component,
                                                const ResolveCapabilities& capabilities);

}  // namespace edward::resolve
