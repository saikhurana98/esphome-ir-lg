#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace lg_climate {

// LG IR protocol constants
static const uint32_t LG_PREAMBLE = 0x88;
static const uint32_t LG_COMMAND_OFF = 0xC0000;
static const uint32_t LG_COMMAND_SWING = 0x10000;
static const uint32_t LG_COMMAND_BOOST = 0x100DE;

// Power level commands (raw 28-bit values)
static const uint32_t LG_POWER_100 = 0x88C07F2;
static const uint32_t LG_POWER_80 = 0x88C07D0;
static const uint32_t LG_POWER_60 = 0x88C07E1;
static const uint32_t LG_POWER_40 = 0x88C0804;

// Mode commands when turning ON (from OFF state)
static const uint32_t LG_MODE_COOL_ON = 0x00000;
static const uint32_t LG_MODE_DRY_ON = 0x01000;
static const uint32_t LG_MODE_FAN_ON = 0x02000;
static const uint32_t LG_MODE_AUTO_ON = 0x03000;

// Mode commands when switching (already ON)
static const uint32_t LG_MODE_COOL_SW = 0x08000;
static const uint32_t LG_MODE_DRY_SW = 0x09000;
static const uint32_t LG_MODE_FAN_SW = 0x0A000;
static const uint32_t LG_MODE_AUTO_SW = 0x0B000;

// Fan speed nibble values (bits 7-4)
static const uint8_t LG_FAN_QUIET = 0x00;
static const uint8_t LG_FAN_LOW = 0x90;
static const uint8_t LG_FAN_MEDIUM = 0x20;
static const uint8_t LG_FAN_HIGH = 0xA0;
static const uint8_t LG_FAN_AUTO = 0x50;
static const uint8_t LG_FAN_MAX = 0x40;

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

  uint32_t header_high_{8000};
  uint32_t header_low_{4000};
  uint32_t bit_high_{600};
  uint32_t bit_one_low_{1600};
  uint32_t bit_zero_low_{550};

  bool power_on_{false};
  bool send_swing_{false};
  bool send_boost_{false};
  std::string pending_power_preset_{};
};

}  // namespace lg_climate
}  // namespace esphome
