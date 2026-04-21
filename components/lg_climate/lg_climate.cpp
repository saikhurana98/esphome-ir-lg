#include "lg_climate.h"
#include "esphome/core/log.h"

namespace esphome {
namespace lg_climate {

static const char *const TAG = "lg_climate";

void LGClimate::setup() {
  climate_ir::ClimateIR::setup();

  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(this);
  } else {
    this->mode = climate::CLIMATE_MODE_OFF;
    this->target_temperature = 25.0f;
    this->fan_mode = climate::CLIMATE_FAN_AUTO;
    this->swing_mode = climate::CLIMATE_SWING_OFF;
  }

  if (std::isnan(this->target_temperature)) {
    this->target_temperature = 25.0f;
  }
}

climate::ClimateTraits LGClimate::traits() {
  auto traits = climate_ir::ClimateIR::traits();
  traits.set_supported_modes({
      climate::CLIMATE_MODE_OFF,
      climate::CLIMATE_MODE_AUTO,
      climate::CLIMATE_MODE_COOL,
      climate::CLIMATE_MODE_DRY,
      climate::CLIMATE_MODE_FAN_ONLY,
  });
  traits.set_supported_fan_modes({
      climate::CLIMATE_FAN_AUTO,
      climate::CLIMATE_FAN_LOW,
      climate::CLIMATE_FAN_MEDIUM,
      climate::CLIMATE_FAN_HIGH,
  });
  traits.set_supported_custom_fan_modes({"Quiet", "Max"});
  traits.set_supported_swing_modes({
      climate::CLIMATE_SWING_OFF,
      climate::CLIMATE_SWING_VERTICAL,
  });
  traits.set_supported_presets({climate::CLIMATE_PRESET_BOOST});
  traits.set_supported_custom_presets({"Power - 100%", "Power - 80%", "Power - 60%", "Power - 40%"});
  traits.set_visual_min_temperature(16.0f);
  traits.set_visual_max_temperature(30.0f);
  traits.set_visual_temperature_step(1.0f);
  traits.set_supports_current_temperature(false);
  return traits;
}

void LGClimate::control(const climate::ClimateCall &call) {
  if (call.get_preset().has_value()) {
    auto preset = *call.get_preset();
    if (preset == climate::CLIMATE_PRESET_BOOST) {
      this->send_boost_ = true;
      this->preset = preset;
      this->custom_preset.reset();
    }
  }

  if (call.get_custom_preset().has_value()) {
    auto custom_preset = *call.get_custom_preset();
    this->pending_power_preset_ = custom_preset;
    this->custom_preset = custom_preset;
    this->preset.reset();
  }

  if (call.get_swing_mode().has_value()) {
    auto swing = *call.get_swing_mode();
    if (swing != this->swing_mode) {
      this->send_swing_ = true;
    }
    this->swing_mode = swing;
  }

  if (call.get_mode().has_value()) {
    this->mode = *call.get_mode();
  }

  if (call.get_target_temperature().has_value()) {
    this->target_temperature = *call.get_target_temperature();
  }

  if (call.get_fan_mode().has_value()) {
    this->fan_mode = *call.get_fan_mode();
    this->custom_fan_mode.reset();
  }

  if (call.get_custom_fan_mode().has_value()) {
    this->custom_fan_mode = *call.get_custom_fan_mode();
    this->fan_mode.reset();
  }

  this->transmit_state();
  this->publish_state();
}

void LGClimate::transmit_state() {
  // Handle boost preset (Jet Cool)
  if (this->send_boost_) {
    this->send_boost_ = false;
    this->transmit_raw_(LG_CMD_BOOST);
    ESP_LOGD(TAG, "Sent boost/jet cool: 0x%07X", LG_CMD_BOOST);
    this->power_on_ = true;
    return;
  }

  // Handle power level presets
  if (!this->pending_power_preset_.empty()) {
    uint32_t power_cmd = 0;
    if (this->pending_power_preset_ == "Power - 100%") {
      power_cmd = LG_POWER_100;
    } else if (this->pending_power_preset_ == "Power - 80%") {
      power_cmd = LG_POWER_80;
    } else if (this->pending_power_preset_ == "Power - 60%") {
      power_cmd = LG_POWER_60;
    } else if (this->pending_power_preset_ == "Power - 40%") {
      power_cmd = LG_POWER_40;
    }
    this->pending_power_preset_.clear();
    if (power_cmd != 0) {
      this->transmit_raw_(power_cmd);
      ESP_LOGD(TAG, "Sent power preset: 0x%07X", power_cmd);
      return;
    }
  }

  // Handle light toggle
  if (this->send_light_) {
    this->send_light_ = false;
    this->transmit_raw_(LG_CMD_LIGHT);
    ESP_LOGD(TAG, "Sent light toggle: 0x%07X", LG_CMD_LIGHT);
    return;
  }

  // Handle swing toggle
  if (this->send_swing_) {
    this->send_swing_ = false;
    this->transmit_raw_(LG_CMD_SWING);
    ESP_LOGD(TAG, "Sent swing toggle: 0x%07X", LG_CMD_SWING);
    return;
  }

  // Handle OFF
  if (this->mode == climate::CLIMATE_MODE_OFF) {
    this->transmit_raw_(LG_CMD_OFF);
    this->power_on_ = false;
    ESP_LOGD(TAG, "Sent OFF: 0x%07X", LG_CMD_OFF);
    return;
  }

  // Build and send normal state command
  uint32_t cmd = this->build_command_();
  this->transmit_raw_(cmd);
  this->power_on_ = true;
  ESP_LOGD(TAG, "Sent: 0x%07X (mode=%d, temp=%.0f)", cmd,
           static_cast<int>(this->mode), this->target_temperature);
}

uint32_t LGClimate::build_command_() {
  uint32_t cmd = LG_PREAMBLE << 20;

  // Bits 19-18: Power (0b00 = on)
  // Bit 15: "already on" flag (set when switching mode while already on)
  if (this->power_on_) {
    cmd |= (1 << 15);
  }

  // Bits 14-12: Mode
  uint8_t mode_val = LG_MODE_COOL;
  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      mode_val = LG_MODE_COOL;
      break;
    case climate::CLIMATE_MODE_DRY:
      mode_val = LG_MODE_DRY;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      mode_val = LG_MODE_FAN;
      break;
    case climate::CLIMATE_MODE_AUTO:
      mode_val = LG_MODE_AUTO;
      break;
    default:
      mode_val = LG_MODE_COOL;
      break;
  }
  cmd |= (mode_val & 0x7) << 12;

  // Bits 11-8: Temperature (temp - 15)
  if (this->mode == climate::CLIMATE_MODE_COOL || this->mode == climate::CLIMATE_MODE_AUTO) {
    uint8_t temp = static_cast<uint8_t>(std::round(this->target_temperature));
    if (temp < 16) temp = 16;
    if (temp > 30) temp = 30;
    cmd |= ((temp - 15) & 0xF) << 8;
  }

  // Bits 7-4: Fan speed
  uint8_t fan = LG_FAN_AUTO;
  if (this->custom_fan_mode.has_value()) {
    auto cfm = this->custom_fan_mode.value();
    if (cfm == "Quiet") {
      fan = LG_FAN_LOWEST;
    } else if (cfm == "Max") {
      fan = LG_FAN_MAX;
    }
  } else if (this->fan_mode.has_value()) {
    switch (this->fan_mode.value()) {
      case climate::CLIMATE_FAN_LOW:
        fan = LG_FAN_LOW;
        break;
      case climate::CLIMATE_FAN_MEDIUM:
        fan = LG_FAN_MEDIUM;
        break;
      case climate::CLIMATE_FAN_HIGH:
        fan = LG_FAN_HIGH;
        break;
      case climate::CLIMATE_FAN_AUTO:
      default:
        fan = LG_FAN_AUTO;
        break;
    }
  }
  cmd |= (fan & 0xF) << 4;

  // Bits 3-0: Checksum
  cmd |= this->calc_checksum_(cmd) & 0xF;

  return cmd;
}

uint8_t LGClimate::calc_checksum_(uint32_t value) {
  uint8_t sum = 0;
  for (int i = 4; i < 28; i += 4) {
    sum += (value >> i) & 0xF;
  }
  return sum & 0xF;
}

void LGClimate::transmit_raw_(uint32_t value) {
  ESP_LOGV(TAG, "Transmitting raw IR: 0x%07X", value);

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  data->set_carrier_frequency(38000);

  // Header
  data->mark(this->header_high_);
  data->space(this->header_low_);

  // 28 data bits, MSB first
  for (int i = 27; i >= 0; i--) {
    data->mark(this->bit_high_);
    if ((value >> i) & 1) {
      data->space(this->bit_one_low_);
    } else {
      data->space(this->bit_zero_low_);
    }
  }

  // Final mark
  data->mark(this->bit_high_);

  transmit.perform();
}

bool LGClimate::on_receive(remote_base::RemoteReceiveData data) {
  // Validate header with generous tolerance
  // LG2 header: ~3200us mark, ~9900us space
  if (!data.expect_item(this->header_high_, this->header_low_)) {
    return false;
  }

  // Decode 28 bits
  // Use a threshold approach: space > 1000us = 1, space < 1000us = 0
  // This is more reliable than expect_item for distinguishing 1-bit (~1550us) from 0-bit (~520us)
  uint32_t value = 0;
  for (int i = 27; i >= 0; i--) {
    // Try 1-bit first (long space)
    if (data.expect_item(this->bit_high_, this->bit_one_low_)) {
      value |= (1 << i);
    } else if (data.expect_item(this->bit_high_, this->bit_zero_low_)) {
      // 0-bit, value already 0
    } else {
      // Could not decode bit
      return false;
    }
  }

  // Validate preamble
  uint8_t preamble = (value >> 20) & 0xFF;
  if (preamble != LG_PREAMBLE) {
    return false;
  }

  // Validate checksum
  uint8_t received_checksum = value & 0xF;
  uint8_t expected_checksum = this->calc_checksum_(value);
  if (received_checksum != expected_checksum) {
    ESP_LOGW(TAG, "Checksum mismatch: 0x%07X (got=%X, want=%X)", value, received_checksum, expected_checksum);
    return false;
  }

  ESP_LOGD(TAG, "Received LG IR: 0x%07X", value);

  // Match special commands (use exact pre-checksummed values)
  if (value == LG_CMD_BOOST) {
    this->preset = climate::CLIMATE_PRESET_BOOST;
    this->custom_preset.reset();
    this->mode = climate::CLIMATE_MODE_COOL;
    this->target_temperature = 16.0f;
    this->fan_mode = climate::CLIMATE_FAN_AUTO;
    this->custom_fan_mode.reset();
    this->power_on_ = true;
    this->publish_state();
    return true;
  }

  if (value == LG_POWER_100) { this->custom_preset = std::string("Power - 100%"); this->preset.reset(); this->publish_state(); return true; }
  if (value == LG_POWER_80)  { this->custom_preset = std::string("Power - 80%");  this->preset.reset(); this->publish_state(); return true; }
  if (value == LG_POWER_60)  { this->custom_preset = std::string("Power - 60%");  this->preset.reset(); this->publish_state(); return true; }
  if (value == LG_POWER_40)  { this->custom_preset = std::string("Power - 40%");  this->preset.reset(); this->publish_state(); return true; }

  if (value == LG_CMD_LIGHT) {
    ESP_LOGD(TAG, "Received light toggle");
    this->publish_state();
    return true;
  }

  if (value == LG_CMD_SWING) {
    if (this->swing_mode == climate::CLIMATE_SWING_OFF) {
      this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
    } else {
      this->swing_mode = climate::CLIMATE_SWING_OFF;
    }
    this->publish_state();
    return true;
  }

  if (value == LG_CMD_OFF) {
    this->mode = climate::CLIMATE_MODE_OFF;
    this->power_on_ = false;
    this->swing_mode = climate::CLIMATE_SWING_OFF;
    this->preset.reset();
    this->custom_preset.reset();
    this->publish_state();
    return true;
  }

  // Decode normal command
  this->preset.reset();
  this->custom_preset.reset();

  // Bits 14-12: Mode
  uint8_t mode_val = (value >> 12) & 0x7;
  switch (mode_val) {
    case LG_MODE_COOL:
      this->mode = climate::CLIMATE_MODE_COOL;
      break;
    case LG_MODE_DRY:
      this->mode = climate::CLIMATE_MODE_DRY;
      break;
    case LG_MODE_FAN:
      this->mode = climate::CLIMATE_MODE_FAN_ONLY;
      break;
    case LG_MODE_AUTO:
      this->mode = climate::CLIMATE_MODE_AUTO;
      break;
    default:
      this->mode = climate::CLIMATE_MODE_COOL;
      break;
  }
  this->power_on_ = true;

  // Bits 11-8: Temperature
  uint8_t temp_raw = (value >> 8) & 0xF;
  if (temp_raw > 0) {
    this->target_temperature = static_cast<float>(temp_raw + 15);
  }

  // Bits 7-4: Fan speed
  uint8_t fan_raw = (value >> 4) & 0xF;
  this->custom_fan_mode.reset();
  switch (fan_raw) {
    case LG_FAN_LOWEST:
      this->fan_mode.reset();
      this->custom_fan_mode = std::string("Quiet");
      break;
    case LG_FAN_LOW:
      this->fan_mode = climate::CLIMATE_FAN_LOW;
      break;
    case LG_FAN_MEDIUM:
      this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
      break;
    case LG_FAN_HIGH:
      this->fan_mode = climate::CLIMATE_FAN_HIGH;
      break;
    case LG_FAN_MAX:
      this->fan_mode.reset();
      this->custom_fan_mode = std::string("Max");
      break;
    case LG_FAN_AUTO:
    default:
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
      break;
  }

  this->publish_state();
  return true;
}

}  // namespace lg_climate
}  // namespace esphome
