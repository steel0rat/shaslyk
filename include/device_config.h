#pragma once

// Firmware version used for OTA comparison with GitHub release tag.
#define FW_VERSION "0.2.0"

// GitHub repository in owner/repo format.
// Example: "steel0rat/shaslyk-firmware"
#define GITHUB_REPO "/steel0rat/shaslyk"

// OTA binary asset name expected in GitHub release assets.
// Leave empty to use the first *.bin asset.
#define OTA_ASSET_NAME "firmware.bin"

// OTA check interval in milliseconds.
#define OTA_CHECK_INTERVAL_MS (6UL * 60UL * 60UL * 1000UL)

// Optional built-in Wi-Fi credentials used before setup portal.
// Leave empty to disable fallback connection.
#define DEFAULT_WIFI_SSID ""
#define DEFAULT_WIFI_PASSWORD ""

// Setup AP credentials shown on first boot/config reset.
#define SETUP_AP_SSID "Shaslyk-Setup"
#define SETUP_AP_PASSWORD "12345678"

// Auto-restart if setup portal is left idle too long.
#define SETUP_PORTAL_TIMEOUT_MS (10UL * 60UL * 1000UL)
