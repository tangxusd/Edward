#pragma once

#include "edward/core/component_edit_command.hpp"

#include <optional>

namespace edward::core {

std::optional<ComponentEditCommand> parseComponentEditCommand(const QJsonObject& object,
                                                              QString* error = nullptr);

}  // namespace edward::core
