#pragma once

#include <QString>

namespace edward::resolve {

enum class ConnectionState { Disconnected, Connecting, Connected, Faulted };

struct ResolveError final {
  QString code;
  QString message;
};

}  // namespace edward::resolve
