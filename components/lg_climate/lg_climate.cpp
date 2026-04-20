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
    if (this->mode != climate::CLIMATE_MODE_OFF) {
      this->power_on_ = true;
    }
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
    uint32_t boost_cmd = (LG_PREAMBLE << 20) | LG_COMMAND_BOOST;
    boost_cmd |= (this->calc_checksum_(boost_cmd) & 0xF);
    this->transmit_raw_(boost_cmd);
    ESP_LOGD(TAG, "Sent boost/jet cool command: 0x%07X", boost_cmd);
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
      ESP_LOGD(TAG, "Sent power preset command: 0x%07X", power_cmd);
      return;
    }
  }

  // Handle swing toggle
  if (this->send_swing_) {
    this->send_swing_ = false;
    uint32_t swing_cmd = (LG_PREAMBLE << 20) | LG_COMMAND_SWING;
    swing_cmd |= (this->calc_checksum_(swing_cmd) & 0xF);
    this->transmit_raw_(swing_cmd);
    ESP_LOGD(TAG, "Sent swing toggle command: 0x%07X", swing_cmd);
    return;
  }

  // Handle OFF
  if (this->mode == climate::CLIMATE_MODE_OFF) {
    uint32_t off_cmd = (LG_PREAMBLE << 20) | LG_COMMAND_OFF;
    off_cmd |= (this->calc_checksum_(off_cmd) & 0xF);
    this->transmit_raw_(off_cmd);
    this->power_on_ = false;
    ESP_LOGD(TAG, "Sent OFF command: 0x%07X", off_cmd);
    return;
  }

  // Build and send normal state command
  uint32_t cmd = this->build_command_();
  this->transmit_raw_(cmd);
  this->power_on_ = true;
  ESP_LOGD(TAG, "Sent command: 0x%07X (mode=%d, temp=%.0f)", cmd,
           static_cast<int>(this->mode), this->target_temperature);
}

uint32_t LGClimate::build_command_() {
  uint32_t cmd = LG_PREAMBLE << 20;

  // Mode nibble (bits 19-12)
  uint32_t mode_cmd = 0;
  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      mode_cmd = this->power_on_ ? LG_MODE_COOL_SW : LG_MODE_COOL_ON;
      break;
    case climate::CLIMATE_MODE_DRY:
      mode_cmd = this->power_on_ ? LG_MODE_DRY_SW : LG_MODE_DRY_ON;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      mode_cmd = this->power_on_ ? LG_MODE_FAN_SW : LG_MODE_FAN_ON;
      break;
    case climate::CLIMATE_MODE_AUTO:
      mode_cmd = this->power_on_ ? LG_MODE_AUTO_SW : LG_MODE_AUTO_ON;
      break;
    default:
      mode_cmd = this->power_on_ ? LG_MODE_COOL_SW : LG_MODE_COOL_ON;
      break;
  }
  cmd |= mode_cmd;

  // Temperature nibble (bits 11-8) - only for cool and auto modes
  if (this->mode == climate::CLIMATE_MODE_COOL || this->mode == climate::CLIMATE_MODE_AUTO) {
    uint8_t temp = static_cast<uint8_t>(std::round(this->target_temperature));
    if (temp < 16) temp = 16;
    if (temp > 30) temp = 30;
    cmd |= ((temp - 15) & 0xF) << 8;
  }

  // Fan speed nibble (bits 7-4)
  uint8_t fan = LG_FAN_AUTO;
  if (this->custom_fan_mode.has_value()) {
    auto cfm = this->custom_fan_mode.value();
    if (cfm == "Quiet") {
      fan = LG_FAN_QUIET;
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
  cmd |= fan;

  // Checksum (bits 3-0)
  cmd |= this->calc_checksum_(cmd) & 0xF;

  return cmd;
}

uint8_t LGClimate::calc_checksum_(uint32_t value) {
  uint8_t sum = 0;
  // Sum all 7 nibbles (bits 27-4)
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
  // Validate header
  if (!data.expect_item(this->header_high_, this->header_low_)) {
    return false;
  }

  // Decode 28 bits
  uint32_t value = 0;
  for (int i = 27; i >= 0; i--) {
    if (data.expect_item(this->bit_high_, this->bit_one_low_)) {
      value |= (1 << i);
    } else if (data.expect_item(this->bit_high_, this->bit_zero_low_)) {
      // bit is 0, already set
    } else {
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
  uint8_t calc_checksum = this->calc_checksum_(value);
  if (received_checksum != calc_checksum) {
    ESP_LOGW(TAG, "Checksum mismatch: received=0x%X, calculated=0x%X", received_checksum, calc_checksum);
    return false;
  }

  ESP_LOGD(TAG, "Received LG IR command: 0x%07X", value);

  // Check for special commands
  uint32_t command = (value >> 8) & 0xFFF;

  // Check for boost/jet cool
  if (value == ((LG_PREAMBLE << 20) | LG_COMMAND_BOOST | (this->calc_checksum_((LG_PREAMBLE << 20) | LG_COMMAND_BOOST) & 0xF))) {
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

  // Check for power level presets
  if (value == LG_POWER_100 || value == LG_POWER_80 ||
      value == LG_POWER_60 || value == LG_POWER_40) {
    if (value == LG_POWER_100) this->custom_preset = std::string("Power - 100%");
    else if (value == LG_POWER_80) this->custom_preset = std::string("Power - 80%");
    else if (value == LG_POWER_60) this->custom_preset = std::string("Power - 60%");
    else if (value == LG_POWER_40) this->custom_preset = std::string("Power - 40%");
    this->preset.reset();
    this->publish_state();
    return true;
  }

  // Check for swing toggle
  uint32_t swing_check = (LG_PREAMBLE << 20) | LG_COMMAND_SWING;
  swing_check |= (this->calc_checksum_(swing_check) & 0xF);
  if (value == swing_check) {
    // Toggle swing state
    if (this->swing_mode == climate::CLIMATE_SWING_OFF) {
      this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
    } else {
      this->swing_mode = climate::CLIMATE_SWING_OFF;
    }
    this->publish_state();
    return true;
  }

  // Check for OFF command
  uint32_t off_check = (LG_PREAMBLE << 20) | LG_COMMAND_OFF;
  off_check |= (this->calc_checksum_(off_check) & 0xF);
  if (value == off_check) {
    this->mode = climate::CLIMATE_MODE_OFF;
    this->power_on_ = false;
    this->swing_mode = climate::CLIMATE_SWING_OFF;
    this->publish_state();
    return true;
  }

  // Decode normal command
  // Clear any presets on normal commands
  this->preset.reset();
  this->custom_preset.reset();

  // Decode mode from command bits
  uint32_t mode_bits = (value >> 12) & 0xFF;
  // Strip the "already on" flag (bit 3 of mode byte)
  uint8_t mode_val = mode_bits & 0x07;
  switch (mode_val) {
    case 0:  // Cool
      this->mode = climate::CLIMATE_MODE_COOL;
      break;
    case 1:  // Dry
      this->mode = climate::CLIMATE_MODE_DRY;
      break;
    case 2:  // Fan only
      this->mode = climate::CLIMATE_MODE_FAN_ONLY;
      break;
    case 3:  // Auto/AI
      this->mode = climate::CLIMATE_MODE_AUTO;
      break;
    default:
      this->mode = climate::CLIMATE_MODE_COOL;
      break;
  }
  this->power_on_ = true;

  // Decode temperature (bits 11-8)
  uint8_t temp_raw = (value >> 8) & 0xF;
  if (temp_raw > 0) {
    this->target_temperature = static_cast<float>(temp_raw + 15);
  }

  // Decode fan speed (bits 7-4)
  uint8_t fan_raw = value & 0xF0;
  this->custom_fan_mode.reset();
  switch (fan_raw) {
    case LG_FAN_QUIET:
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
