#pragma once
#include <Arduino.h>

// Direct cloud communication was deliberately removed from the CYD firmware.
// The ESP32 serves telemetry only on the local LAN. The Windows Gateway reads
// /api/v1/status and performs authenticated Cloudflare synchronization.
namespace SiteLink {
static void begin() {}
static void loop() {}
static String status() { return "LOCAL ONLY - WINDOWS GATEWAY"; }
}
