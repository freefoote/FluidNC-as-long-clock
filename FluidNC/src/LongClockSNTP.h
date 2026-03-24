// Copyright (c) 2024 Mitch Bradley All rights reserved.
// Use of this source code is governed by a GPLv3 license that can be found in the LICENSE file.

#pragma once

#include "Configuration/Configurable.h"
#include "Module.h"

#include <string>
#include <cstdint>

class LongClockSNTP : public ConfigurableModule {
private:
    std::string _ntp_server   = "pool.ntp.org";
    int32_t     _sync_interval_s = 3600;
    int32_t     _utc_offset_s  = 0;
    int32_t     _dst_offset_s  = 0;
    bool        _synced       = false;
    uint32_t    _last_sync_ms = 0;

public:
    LongClockSNTP(const char* name) : ConfigurableModule(name) {}
    virtual ~LongClockSNTP() = default;

    // ConfigurableModule methods
    void group(Configuration::HandlerBase& handler) override;
    void init() override;
    void poll() override;

    // Status query
    bool isSynced() const { return _synced; }
};
