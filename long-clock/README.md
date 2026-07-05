# Long Clock

This is a fork of [FluidNC](https://github.com/bdring/FluidNC) that repurposes a
single-axis CNC machine as an analog-style clock. Instead of running G-code jobs,
the machine continuously moves its X axis carriage to a position that represents
the current time of day, turning the length of travel into a clock face.

The firmware syncs time over WiFi via NTP and, once synced, moves the X axis to a
position proportional to the current time within a 12-hour (or 24-hour) cycle.

## How it works

Two new modules are added to FluidNC as `ConfigurableModule`s (see
`FluidNC/src/Module.h`), so they are configured from YAML and polled
periodically by the main polling loop like other FluidNC modules:

### `LongClockSNTP` (`FluidNC/src/LongClockSNTP.h` / `.cpp`)

- Wraps the ESP32 SNTP client (`configTime()`) to sync the system clock from an
  NTP server.
- Configurable NTP server, UTC offset, DST offset, and re-sync interval.
- Periodically re-syncs (`sync_interval_s`) and tracks whether a valid time has
  ever been obtained (`isSynced()`), determined heuristically by checking
  whether the local year is greater than 2020.

### `LongClock` (`FluidNC/src/LongClock.h` / `.cpp`)

- Polls the current local time (via `time()`/`localtime()`) at most once every
  5 seconds.
- Waits until the clock looks synced (same "year > 2020" heuristic) before
  doing anything.
- When the current minute changes:
  - If the machine is `Idle`, computes a target X position from the current
    time and issues a G-code move (`moveToTime()`), using either a rapid move
    (`G90G0`) or a feed move (`G90G1F<feed>`) depending on configuration.
  - If the machine is in `Alarm` state (e.g. because it hit a soft/hard limit
    or lost position), it automatically re-homes the X axis to try to clear
    the alarm and resume operation unattended.
- `timeToPosition()` maps the current time to a position between
  `pos_start_mm` and `pos_end_mm`:
  - In 12-hour mode, the range represents one 12-hour revolution (`12:00`/`00:00`
    at `pos_start_mm` through `11:59`/`23:59` at `pos_end_mm`).
  - In 24-hour mode, the same range represents a full 24-hour day.

Both modules are registered with `ConfigurableModuleFactory` using
`init_priority` (SNTP at 110, LongClock at 115) so that networking/time sync is
initialized before the clock logic that depends on it.

## Hardware configuration

`long-clock/mks-dlc32-long-clock.yaml` is the FluidNC machine configuration for
the physical build, based on an MKS-DLC32 controller:

- Single X axis, `43.529` steps/mm, travel limited to `2300 mm`, with soft
  limits enabled and homing against a negative-direction limit switch on
  `gpio.36`.
- `must_home: true` on startup, so the machine homes the axis before the clock
  starts moving it.
- `long_clock_sntp` section: NTP server `pool.ntp.org`, syncing hourly, with a
  `utc_offset_s` of `28800` (UTC+8, for Perth).
- `long_clock` section: enabled, 12-hour mode, mapping `pos_start_mm: 3` to
  `pos_end_mm: 2235.0` across the 12-hour cycle, using a fed move at
  `3000 mm/min` rather than a rapid move (so the carriage visibly "ticks"
  across the face rather than snapping instantly).

## Building

```
source .venv/bin/activate
uv run pio run
```

Then fetch the firmware from `.pio/build/wifi/firmware.bin`.

## Flashing and configuring

1. Flash `firmware.bin` to the MKS-DLC32 controller as you would for stock
   FluidNC.
2. Upload `mks-dlc32-long-clock.yaml` as the machine's `config.yaml` (e.g. via
   the FluidNC web UI or WebSocket/SD upload) and select it as the active
   configuration.
3. Connect the controller to WiFi. Once it obtains an NTP time sync, the
   machine will home (if required) and then begin tracking the current time
   automatically whenever it is `Idle`.

## Notes / caveats

- The "clock is synced" check is a heuristic (system year > 2020) rather than
  a true SNTP sync-status callback, so there can be a short window after boot
  where the reported time is not yet valid.
- Because moves are only issued while the machine is `Idle`, sending manual
  G-code, jogging, or running a job will pause the clock until the machine
  returns to `Idle`.
- If the machine enters `Alarm` (e.g. a limit fault), `LongClock` will
  automatically attempt to re-home the X axis on the next poll to resume
  unattended clock operation.
