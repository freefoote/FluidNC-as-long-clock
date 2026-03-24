// Copyright (c) 2024 Mitch Bradley All rights reserved.
// Use of this source code is governed by a GPLv3 license that can be found in the LICENSE file.

#pragma once

#include "Configuration/Configurable.h"
#include "Module.h"

#include <cstdint>

class LongClock : public ConfigurableModule {
private:
    bool     _enabled        = false;
    bool     _hour24         = false;
    float    _pos_start_mm   = 0.0f;
    float    _pos_end_mm     = 2300.0f;
    float    _feed_mm_min    = 3000.0f;
    bool     _rapid          = false;
    int      _last_minute    = -1;
    uint32_t _last_check_ms  = 0;

public:
    LongClock(const char* name) : ConfigurableModule(name) {}
    virtual ~LongClock() = default;

    // ConfigurableModule methods
    void group(Configuration::HandlerBase& handler) override;
    void init() override;
    void poll() override;

    // Helper methods
    float timeToPosition(int hours, int minutes);
    void  moveToTime(int hours, int minutes);
    bool  clockIsReady();
};
