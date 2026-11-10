#pragma once

// Optional private credentials file. Copy WifiConfig.local.example.h to
// WifiConfig.local.h and enter your router credentials there.
// WifiConfig.local.h is excluded from Git to protect your password.
#if __has_include("WifiConfig.local.h")
  #include "WifiConfig.local.h"
#else
  static const char WIFI_SSID[] = "";
  static const char WIFI_PASSWORD[] = "";
#endif
