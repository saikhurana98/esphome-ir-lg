# ESPHome LG AC IR Climate Component

Custom ESPHome external component for controlling LG air conditioners via infrared, compatible with **ESP-IDF** and **Arduino** frameworks.

Based on the LG AKB75215403 remote protocol (28-bit).

## Features

- Full climate control: Cool, Dry, Fan Only, Auto modes
- Fan speeds: Quiet, Low, Medium, High, Max, Auto
- Vertical swing toggle
- Boost / Jet Cool preset
- Power level presets: 100%, 80%, 60%, 40%
- IR receive and decode with checksum validation
- Configurable IR timing for unit-specific adjustments
- Works with ESP-IDF framework (no Arduino dependency)

## Installation

Add to your ESPHome YAML:

```yaml
external_components:
  - source: github://notkanishk/esphome-ir-lg@main
    components: [lg_climate]
```

## Configuration

```yaml
remote_transmitter:
  pin: GPIO4
  carrier_duty_percent: 50%

remote_receiver:
  pin:
    number: GPIO5
    inverted: true
  dump: raw
  tolerance: 25%

climate:
  - platform: lg_climate
    name: "LG AC"
```

### Optional Timing Parameters

If your AC unit doesn't respond reliably, adjust the IR timing:

| Parameter | Default | Description |
|-----------|---------|-------------|
| `header_high` | 8000 | Header mark duration (µs) |
| `header_low` | 4000 | Header space duration (µs) |
| `bit_high` | 600 | Bit mark duration (µs) |
| `bit_one_low` | 1600 | Bit-1 space duration (µs) |
| `bit_zero_low` | 550 | Bit-0 space duration (µs) |

## Protocol

28-bit LG AC IR protocol at 38 kHz:

```
[Preamble 8-bit][Command 12-bit][Temp 4-bit][Fan 4-bit][Checksum 4-bit]
```

Checksum = sum of upper 7 nibbles, mod 16.
