#include <edward/resources/device_identity.hpp>

#include <cassert>

int main() {
  const auto normalized = edward::resources::normalizeDeviceIdentity(" SERIAL-1 ", "aa-bb-cc-dd-ee-ff");
  assert(normalized.valid());
  assert(normalized.serial == "SERIAL-1");
  assert(normalized.mac == "AA:BB:CC:DD:EE:FF");
  assert(!edward::resources::normalizeDeviceIdentity("", "AA:BB:CC:DD:EE:FF").valid());
  return 0;
}
