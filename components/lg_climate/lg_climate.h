#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace lg_climate {

// LG IR protocol constants
static const uint32_t LG_PREAMBLE = 0x88;

// Special commands (full 28-bit raw values, pre-checksummed)
static const uint32_t LG_CMD_OFF = 0x88C0051;
static const uint32_t LG_CMD_BOOST = 0x88100DE;
static const uint32_t LG_CMD_LIGHT = 0x88C00A6;
static const uint32_t LG_CMD_SWING = 0x8810001;
static const uint32_t LG_POWER_100 = 0x88C07F2;
static const uint32_t LG_POWER_80 = 0x88C07D0;
static const uint32_t LG_POWER_60 = 0x88C07E1;
static const uint32_t LG_POWER_40 = 0x88C0804;

// Mode values (3-bit, bits 14-12)
static const uint8_t LG_MODE_COOL = 0;
static const uint8_t LG_MODE_DRY = 1;
static const uint8_t LG_MODE_FAN = 2;
static const uint8_t LG_MODE_AUTO = 3;

// Fan speed values (4-bit nibble, bits 7-4)
static const uint8_t LG_FAN_LOWEST = 0;
static const uint8_t LG_FAN_LOW = 9;
static const uint8_t LG_FAN_MEDIUM = 2;
static const uint8_t LG_FAN_HIGH = 10;
static const uint8_t LG_FAN_MAX = 4;
static const uint8_t LG_FAN_AUTO = 5;

class LGClimate : public climate_ir::ClimateIR {
 public:
  LGClimate()
      : climate_ir::ClimateIR(16.0f, 30.0f, 1.0f, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW,
                               climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL},
                              {climate::CLIMATE_PRESET_BOOST}) {}

  void setup() override;
  void control(const climate::ClimateCall &call) override;
  climate::ClimateTraits traits() override;

  void set_header_high(uint32_t header_high) { header_high_ = header_high; }
  void set_header_low(uint32_t header_low) { header_low_ = header_low; }
  void set_bit_high(uint32_t bit_high) { bit_high_ = bit_high; }
  void set_bit_one_low(uint32_t bit_one_low) { bit_one_low_ = bit_one_low; }
  void set_bit_zero_low(uint32_t bit_zero_low) { bit_zero_low_ = bit_zero_low; }

 protected:
  void transmit_state() override;
  bool on_receive(remote_base::RemoteReceiveData data) override;

  void transmit_raw_(uint32_t value);
  uint8_t calc_checksum_(uint32_t value);
  uint32_t build_command_();

  // LG2 protocol timing defaults (from real AKB75215403 remote capture)
  uint32_t header_high_{3200};
  uint32_t header_low_{9900};
  uint32_t bit_high_{500};
  uint32_t bit_one_low_{1550};
  uint32_t bit_zero_low_{520};

  bool power_on_{false};
  bool send_swing_{false};
  bool send_boost_{false};
  bool send_light_{false};
  std::string pending_power_preset_{};
};

}  // namespace lg_climate
}  // namespace esphome
