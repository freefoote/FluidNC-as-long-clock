// Copyright (c) 2024 Mitch Bradley All rights reserved.
// Use of this source code is governed by a GPLv3 license that can be found in the LICENSE file.

#include "LongClock.h"

#include "Logging.h"
#include "Configuration/HandlerBase.h"
#include "State.h"
#include "GCode.h"
#include "Machine/Homing.h"
#include "Types.h"

#include <Arduino.h>
#include <esp_sntp.h>
#include <sys/time.h>
#include <cstdio>

void LongClock::group(Configuration::HandlerBase& handler) {
    handler.item("enabled", _enabled);
    handler.item("hour24", _hour24);
    handler.item("pos_start_mm", _pos_start_mm);
    handler.item("pos_end_mm", _pos_end_mm);
    handler.item("feed_mm_min", _feed_mm_min);
    handler.item("rapid", _rapid);
}

void LongClock::init() {
    log_info("LongClock: Initializing Long Clock module");
    _last_minute = -1;
    _last_check_ms = 0;
}

void LongClock::poll() {
    // Rate-limit to checking time at most once per 5 seconds
    uint32_t now_ms = millis();
    uint32_t elapsed_ms = now_ms - _last_check_ms;
    if (elapsed_ms < 5000) {
        return;
    }
    _last_check_ms = now_ms;

    // Skip if not enabled
    if (!_enabled) {
    log_info("LongClock: Not enabled");
        return;
    }

    // Skip if SNTP hasn't synced yet
    if (!clockIsReady()) {
        log_info("LongClock: Clock not ready yet, skipping poll");
        return;
    }

    // Get current time
    time_t now = time(nullptr);
    struct tm timeinfo = *localtime(&now);

    int current_minute = timeinfo.tm_min;
    int current_hour = timeinfo.tm_hour;

    // Check if we've entered a new minute
    if (current_minute != _last_minute) {
        _last_minute = current_minute;

        // Only move if machine is idle
        if (state_is(State::Idle)) {
            moveToTime(current_hour, current_minute);
        } else if (state_is(State::Alarm)) {
            // If in alarm state, attempt to home on X axis to clear alarm
            log_info("LongClock: Machine in alarm state, attempting to home X axis");
            Machine::Homing::run_cycles(1 << X_AXIS);
        }
    }
}

float LongClock::timeToPosition(int hours, int minutes) {
    float fraction;

    if (_hour24) {
        // 24-hour mode: total_minutes = 24*60-1 = 1439
        int total_minutes = 24 * 60 - 1;
        int current_minutes = hours * 60 + minutes;
        fraction = (float)current_minutes / (float)total_minutes;
    } else {
        // 12-hour mode: total_minutes = 12*60-1 = 719
        int h12 = hours % 12;
        int total_minutes = 12 * 60 - 1;
        int current_minutes = h12 * 60 + minutes;
        fraction = (float)current_minutes / (float)total_minutes;
    }

    // Calculate position
    float x_pos = _pos_start_mm + fraction * (_pos_end_mm - _pos_start_mm);
    return x_pos;
}

void LongClock::moveToTime(int hours, int minutes) {
    float target_pos = timeToPosition(hours, minutes);

    char buf[256];
    if (_rapid) {
        // Rapid move: G90G0X<position>
        snprintf(buf, sizeof(buf), "G90G0X%.3f", target_pos);
    } else {
        // Fed move: G90G1X<position>F<feed>
        snprintf(buf, sizeof(buf), "G90G1X%.3fF%.0f", target_pos, _feed_mm_min);
    }

    log_info("LongClock: Moving to position " << target_pos << " mm for " << hours << ":"
             << (minutes < 10 ? "0" : "") << minutes << " (" << buf << ")");

    // Execute the G-code line
    gc_execute_line(buf);
}

bool LongClock::clockIsReady() {
    // Check if SNTP is synced
    // Try to get the time and verify it's reasonable
    time_t now = time(nullptr);
    struct tm timeinfo = *localtime(&now);

    // If year is greater than 2020, time has been synced
    if (timeinfo.tm_year + 1900 > 2020) {
        return true;
    }

    return false;
}

// Factory registration with init_priority 115
ConfigurableModuleFactory::InstanceBuilder<LongClock> long_clock_module __attribute__((init_priority(115))) ("long_clock");
