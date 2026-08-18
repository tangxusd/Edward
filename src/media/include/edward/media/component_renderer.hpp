#pragma once

#include "edward/core/component_ir.hpp"

#include <QImage>
#include <QSize>

namespace edward::media {

class ComponentRenderer final {
 public:
  [[nodiscard]] QImage render(const edward::core::ComponentIr& component,
                              int frame,
                              const QSize& canvasSize) const;
};

}  // namespace edward::media
