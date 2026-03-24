// Copyright (c) 2024 Mitch Bradley All rights reserved.
// Use of this source code is governed by a GPLv3 license that can be found in the LICENSE file.

#include "LongClockSNTP.h"

#include "Logging.h"
#include "Configuration/HandlerBase.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <sys/time.h>

void LongClockSNTP::group(Configuration::HandlerBase& handler) {
    handler.item("ntp_server", _ntp_server);
    handler.item("sync_interval_s", _sync_interval_s, 300, 86400);
    handler.item("utc_offset_s", _utc_offset_s, -43200, 43200);
    handler.item("dst_offset_s", _dst_offset_s, -3600, 3600);
}

void LongClockSNTP::init() {
    log_info("LongClockSNTP: Initializing SNTP");

    // Check WiFi connection status
    if (WiFi.status() == WL_CONNECTED) {
        log_info("LongClockSNTP: WiFi is connected");
    } else {
        log_info("LongClockSNTP: WiFi is not connected (will retry on poll)");
    }

    // Initialize SNTP with NTP server and timezone offsets
    // configTime parameters: (gmtOffsetSeconds, daylightOffsetSeconds, server1, server2, server3)
    configTime(_utc_offset_s, _dst_offset_s, _ntp_server.c_str());

    log_info("LongClockSNTP: Configured with NTP server: " << _ntp_server);
    log_info("LongClockSNTP: UTC offset: " << _utc_offset_s << "s, DST offset: " << _dst_offset_s << "s");

    _last_sync_ms = millis();
    _synced       = false;
}

void LongClockSNTP::poll() {
    // Rate-limit re-sync attempts to once per _sync_interval_s
    uint32_t now_ms = millis();
    uint32_t elapsed_ms = now_ms - _last_sync_ms;
    uint32_t interval_ms = (uint32_t)_sync_interval_s * 1000;

    // Check if it's time to sync
    if (elapsed_ms >= interval_ms) {
        // Attempt to re-sync by calling configTime() again
        configTime(_utc_offset_s, _dst_offset_s, _ntp_server.c_str());
        _last_sync_ms = now_ms;
        log_info("LongClockSNTP: Re-sync attempt (" << _last_sync_ms << "ms)");
    }

    // Check if time has been synced by attempting to get local time
    // A valid time has year > 2020
    if (!_synced) {
        time_t now = time(nullptr);
        struct tm timeinfo = *localtime(&now);

        // If year is greater than 2020, time has been synced
        if (timeinfo.tm_year + 1900 > 2020) {
            _synced = true;
            log_info("LongClockSNTP: Time synchronized. Current time: " << asctime(&timeinfo));
        }
    }
}

// Factory registration with init_priority 110
ConfigurableModuleFactory::InstanceBuilder<LongClockSNTP> long_clock_sntp_module __attribute__((init_priority(110))) ("long_clock_sntp");
