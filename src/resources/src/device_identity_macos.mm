#include "edward/resources/device_identity.hpp"

#import <IOKit/IOKitLib.h>
#import <CoreFoundation/CoreFoundation.h>

#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_dl.h>

#include <QStringList>

namespace edward::resources {

DeviceIdentity collectDeviceIdentity() {
  io_registry_entry_t platform = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("IOPlatformExpertDevice"));
  QString serial;
  if (platform) {
    CFTypeRef value = IORegistryEntryCreateCFProperty(platform, CFSTR("IOPlatformSerialNumber"), kCFAllocatorDefault, 0);
    if (value && CFGetTypeID(value) == CFStringGetTypeID()) serial = QString::fromCFString(static_cast<CFStringRef>(value));
    if (value) CFRelease(value);
    IOObjectRelease(platform);
  }

  QString mac;
  ifaddrs* interfaces = nullptr;
  if (getifaddrs(&interfaces) == 0) {
    for (ifaddrs* current = interfaces; current; current = current->ifa_next) {
      if (!current->ifa_addr || current->ifa_addr->sa_family != AF_LINK) continue;
      if ((current->ifa_flags & IFF_LOOPBACK) || !(current->ifa_flags & IFF_UP)) continue;
      const auto* link = reinterpret_cast<sockaddr_dl*>(current->ifa_addr);
      const auto* bytes = reinterpret_cast<const unsigned char*>(LLADDR(link));
      if (!bytes || link->sdl_alen != 6) continue;
      QStringList octets;
      bool nonZero = false;
      for (int index = 0; index < 6; ++index) {
        octets << QStringLiteral("%1").arg(bytes[index], 2, 16, QLatin1Char('0'));
        nonZero = nonZero || bytes[index] != 0;
      }
      if (nonZero) { mac = octets.join(QLatin1Char(':')); break; }
    }
    freeifaddrs(interfaces);
  }
  return normalizeDeviceIdentity(serial, mac);
}

}  // namespace edward::resources
