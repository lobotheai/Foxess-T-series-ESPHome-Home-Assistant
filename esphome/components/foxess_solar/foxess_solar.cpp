#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "foxess_solar.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace foxess_solar {

namespace {
	
static const char *const TAG = "foxess_solar";

static inline void publish_sensor_state(sensor::Sensor *sensor, int32_t raw, float scale) {
  if (!sensor)
    return;
  if (std::isnan(scale)) {
    sensor->publish_state(NAN);
  } else {
    sensor->publish_state(static_cast<float>(raw) * scale);
  }
}

static inline void publish_unsigned_sensor_state(sensor::Sensor *sensor, uint32_t raw, float scale) {
  if (sensor != nullptr)
    sensor->publish_state(static_cast<float>(raw) * scale);
}

static inline int16_t decode_int16(uint8_t msb, uint8_t lsb) {
  uint16_t u = static_cast<uint16_t>((static_cast<uint16_t>(msb) << 8) | lsb);
  return static_cast<int16_t>(u);
}

static inline uint16_t decode_uint16(uint8_t msb, uint8_t lsb) {
  return static_cast<uint16_t>((static_cast<uint16_t>(msb) << 8) | lsb);
}

static inline uint32_t decode_uint32(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
  return (static_cast<uint32_t>(b0) << 24) |
         (static_cast<uint32_t>(b1) << 16) |
         (static_cast<uint32_t>(b2) << 8)  |
         (static_cast<uint32_t>(b3));
}

static std::string decode_ascii(const uint8_t *data, std::size_t length) {
  std::string value;
  value.reserve(length);
  for (std::size_t i = 0; i < length; i++) {
    if (data[i] == 0)
      break;
    if (data[i] >= 0x20 && data[i] <= 0x7E)
      value.push_back(static_cast<char>(data[i]));
  }
  return value;
}

static std::string decode_device_factory(uint16_t factory) {
  switch (factory) {
    case 0:
      return "Fox Wenzhou Factory";
    case 1:
      return "Fox Wuxi Factory";
    case 16:
      return "Fox Wuxi Factory2";
    default:
      return "Unknown factory (" + std::to_string(factory) + ")";
  }
}

}

// -----------------------------------------------------------------------------

void FoxessSolar::setup() {
  ESP_LOGVV(TAG, "setup start");

  if (this->flow_control_pin_ != nullptr) {
    this->flow_control_pin_->setup();
  }

  this->millis_last_frame_ = millis();
  this->millis_last_realtime_ = millis();
}

void FoxessSolar::publish_zero_phases() {
  for (auto &ph : this->phases_) {
    publish_sensor_state(ph.voltage_sensor_, 0, 1.0f);
    publish_sensor_state(ph.current_sensor_, 0, 1.0f);
    publish_sensor_state(ph.frequency_sensor_, 0, NAN);
    publish_sensor_state(ph.active_power_sensor_, 0, 1.0f);
  }
}

void FoxessSolar::publish_zero_pvs() {
  for (auto &pv : this->pvs_) {
    publish_sensor_state(pv.voltage_sensor_, 0, 1.0f);
    publish_sensor_state(pv.current_sensor_, 0, 1.0f);
    publish_sensor_state(pv.active_power_sensor_, 0, 1.0f);
  }
}

void FoxessSolar::update() {
  ESP_LOGVV(TAG, "update start");

  // handle timeout
  if (millis() - this->millis_last_realtime_ >= INVERTER_TIMEOUT) {
    if (this->inverter_mode_ != 0) {
      if (millis() - this->millis_last_frame_ >= INVERTER_TIMEOUT) {
        ESP_LOGW(TAG, "Realtime data timed out; no valid FoxESS frames received recently");
      } else {
        ESP_LOGW(TAG, "Realtime data timed out; other FoxESS frames are still arriving");
      }
      this->set_inverter_mode(0);  // OFFLINE

      publish_sensor_state(this->generation_power_, 0, 1.0f);
      publish_sensor_state(this->grid_power_, 0, NAN);
      publish_sensor_state(this->loads_power_, 0, NAN);

      publish_sensor_state(this->boost_temp_, 0, NAN);
      publish_sensor_state(this->ambient_temp_, 0, NAN);
      publish_sensor_state(this->inverter_temp_, 0, NAN);
      publish_sensor_state(this->eps_voltage_, 0, NAN);
      publish_sensor_state(this->eps_current_, 0, NAN);
      publish_sensor_state(this->eps_power_, 0, NAN);

      this->publish_zero_phases();
      this->publish_zero_pvs();
    }
  }

  // read bytes
  while (this->available() > 0) {
    this->read_byte(&this->input_buffer[this->buffer_end]);
    optional<bool> state = this->check_msg();

    if (!state.has_value()) {
      // still reading
      this->buffer_end++;
    } else if (*state) {
      // good complete message
      this->status_clear_warning();
      this->parse_message();
      this->buffer_end = 0;
      this->millis_last_frame_ = millis();
    } else {
      // invalid message
      this->buffer_end = 0;
    }
  }
}

// Return std::nullopt -> more bytes needed
// Return true         -> complete & valid
// Return false        -> invalid, clear
optional<bool> FoxessSolar::check_msg() {
  const std::size_t idx = this->buffer_end;

  // 1) header check
  if (idx < MSG_HEADER.size()) {
    if (this->input_buffer[idx] == MSG_HEADER[idx]) {
      return {};
    } else {
      ESP_LOGVV(TAG, "header mismatch at %d: 0x%x", (int) idx, this->input_buffer[idx]);
      return false;
    }
  }

  // 2) need length bytes
  if (idx < 9) {
    return {};
  }

  const uint16_t payload_len = decode_uint16(this->input_buffer[7], this->input_buffer[8]);
  const uint16_t msg_len     = payload_len + 13;

  if (msg_len > BUFFER_SIZE) {
    ESP_LOGE(TAG, "message too long: %u", msg_len);
    this->status_set_warning();
    return false;
  }

  // not fully read yet?
  if (idx + 1 < msg_len) {
    ESP_LOGVV(TAG, "message incomplete: have %u, need %u", (unsigned)(idx + 1), (unsigned)msg_len);
    return {};
  }

  // footer
  if (this->input_buffer[msg_len - 2] != MSG_FOOTER[0] ||
      this->input_buffer[msg_len - 1] != MSG_FOOTER[1]) {
    ESP_LOGE(TAG, "bad footer: ... 0x%x 0x%x",
             this->input_buffer[msg_len - 2], this->input_buffer[msg_len - 1]);
    this->status_set_warning();
    return false;
  }

 // CRC
  const uint16_t calc_crc = crc16(&this->input_buffer[2], msg_len - 6);
  const uint16_t msg_crc  = decode_uint16(this->input_buffer[msg_len - 3],
                                          this->input_buffer[msg_len - 4]);
  if (calc_crc != msg_crc) {
    ESP_LOGE(TAG, "checksum mismatch, calc: 0x%x, msg: 0x%x", calc_crc, msg_crc);
    this->status_set_warning();
    return false;
  }

  return true;
}

void FoxessSolar::parse_message() {
  ESP_LOGVV(TAG, "parse_message start");

  const std::size_t total_len = this->buffer_end + 1;
  auto &msg = this->input_buffer;
  const uint8_t function_code = msg[2];
  const uint16_t payload_len = decode_uint16(msg[7], msg[8]);
  const uint32_t frame_timestamp = decode_uint32(msg[3], msg[4], msg[5], msg[6]);

  ESP_LOGD(TAG, "FoxESS frame: function=0x%02X timestamp=%lu payload=%u total=%u",
           static_cast<unsigned>(function_code), static_cast<unsigned long>(frame_timestamp),
           static_cast<unsigned>(payload_len), static_cast<unsigned>(total_len));

  switch (function_code) {
    case 0x01:
      this->parse_device_attributes();
      return;
    case 0x02:
      this->parse_realtime_data();
      return;
    case 0x06:
      this->parse_heartbeat();
      return;
    case 0x03: {
      if (payload_len == MsgOffset::FUNCTION_03_PAYLOAD_LEN) {
        publish_sensor_state(this->function_03_counter_,
                             decode_uint16(msg[MsgOffset::FUNCTION_03_COUNTER_MSB],
                                           msg[MsgOffset::FUNCTION_03_COUNTER_MSB + 1]),
                             1.0f);
        if (!this->has_function_03_payload_) {
          constexpr std::size_t preview_limit = MsgOffset::FUNCTION_03_PAYLOAD_LEN;
          char payload_hex[preview_limit * 3 + 1]{};
          std::size_t output_len = 0;
          for (std::size_t i = 0; i < preview_limit; i++) {
            this->last_function_03_payload_[i] = msg[9 + i];
            const int written = snprintf(payload_hex + output_len, sizeof(payload_hex) - output_len,
                                         "%02X%s", static_cast<unsigned>(msg[9 + i]),
                                         i + 1 < preview_limit ? " " : "");
            if (written < 0 || static_cast<std::size_t>(written) >= sizeof(payload_hex) - output_len)
              break;
            output_len += static_cast<std::size_t>(written);
          }
          this->has_function_03_payload_ = true;
          ESP_LOGD(TAG, "FoxESS function 0x03 initial payload: %s", payload_hex);
        } else {
          const uint16_t previous_counter = decode_uint16(
              this->last_function_03_payload_[36], this->last_function_03_payload_[37]);
          const uint16_t current_counter = decode_uint16(msg[45], msg[46]);
          bool changed = false;
          for (std::size_t i = 0; i < MsgOffset::FUNCTION_03_PAYLOAD_LEN; i++) {
            const uint8_t value = msg[9 + i];
            const uint8_t previous = this->last_function_03_payload_[i];
            if (value != previous) {
              ESP_LOGD(TAG, "FoxESS function 0x03 payload p[%u]: 0x%02X -> 0x%02X",
                       static_cast<unsigned>(i), static_cast<unsigned>(previous),
                       static_cast<unsigned>(value));
              this->last_function_03_payload_[i] = value;
              changed = true;
            }
          }
          if (changed) {
            const int32_t delta =
                (static_cast<uint32_t>(current_counter) + 65536U - previous_counter) % 65536U;
            ESP_LOGD(TAG, "FoxESS function 0x03 p[36..37] raw counter=%u modulo_delta=%ld",
                     static_cast<unsigned>(current_counter), static_cast<long>(delta));
          }
        }
      } else {
        ESP_LOGD(TAG, "Unsupported FoxESS function 0x03 payload length: %u", payload_len);
      }
      return;
    }
    default:
      ESP_LOGD(TAG, "Unsupported FoxESS function code: 0x%02X", function_code);
      return;
  }
}

void FoxessSolar::parse_realtime_data() {
  auto &msg = this->input_buffer;
  const uint16_t payload_len = decode_uint16(msg[7], msg[8]);
  if (payload_len != MsgOffset::REALTIME_PAYLOAD_LEN) {
    ESP_LOGW(TAG, "Unsupported FoxESS realtime payload length: %u; frame ignored", payload_len);
    return;
  }
  this->millis_last_realtime_ = millis();

  // powers
  publish_sensor_state(this->grid_power_,
                       decode_int16(msg[MsgOffset::GRID_POWER_MSB],
                                    msg[MsgOffset::GRID_POWER_LSB]),
                       1.0f);
  publish_sensor_state(this->generation_power_,
                       decode_int16(msg[MsgOffset::GEN_POWER_MSB],
                                    msg[MsgOffset::GEN_POWER_LSB]),
                       1.0f);
  publish_sensor_state(this->loads_power_,
                       decode_int16(msg[MsgOffset::LOAD_POWER_MSB],
                                    msg[MsgOffset::LOAD_POWER_LSB]),
                       1.0f);

  // phases
  for (std::size_t i = 0; i < 3; i++) {
    const std::size_t base = MsgOffset::PHASE1_BASE + i * MsgOffset::PHASE_STRIDE;
    auto &ph = this->phases_[i];

    publish_sensor_state(ph.voltage_sensor_,   decode_uint16(msg[base + 0], msg[base + 1]), 0.1f);
    publish_sensor_state(ph.current_sensor_,   decode_int16(msg[base + 2], msg[base + 3]), 0.1f);
    publish_sensor_state(ph.frequency_sensor_, decode_uint16(msg[base + 4], msg[base + 5]), 0.01f);
    publish_sensor_state(ph.active_power_sensor_, decode_int16(msg[base + 6], msg[base + 7]), 1.0f);
  }

  // pvs
  for (std::size_t i = 0; i < 4; i++) {
    const std::size_t base = MsgOffset::PV1_BASE + i * MsgOffset::PV_STRIDE;
    const uint16_t volt = decode_uint16(msg[base + 0], msg[base + 1]);
    const uint16_t amps = decode_uint16(msg[base + 2], msg[base + 3]);
    const uint16_t raw_power = decode_uint16(msg[base + 4], msg[base + 5]);
    auto &pv = this->pvs_[i];
    const float calculated_power = static_cast<float>(volt) * static_cast<float>(amps) * 0.01f;

    publish_sensor_state(pv.voltage_sensor_, volt, 0.1f);
    publish_sensor_state(pv.current_sensor_, amps, 0.1f);
    if (pv.active_power_sensor_ != nullptr)
      pv.active_power_sensor_->publish_state(calculated_power);
    ESP_LOGD(TAG, "PV%u power comparison: raw=%u W calculated=%.1f W delta=%.1f W",
             static_cast<unsigned>(i + 1), static_cast<unsigned>(raw_power), calculated_power,
             static_cast<float>(raw_power) - calculated_power);
  }

  // Rev.1..Rev.10 are undocumented; log their raw values only on first sample or change.
  for (std::size_t i = 0; i < MsgOffset::REV_FIELDS_COUNT; i++) {
    const std::size_t offset = MsgOffset::REV_FIELDS_BEGIN + i * 2;
    const uint16_t raw_value = decode_uint16(msg[offset], msg[offset + 1]);
    if (!this->has_rev_sample_ || raw_value != this->last_rev_values_[i]) {
      const int32_t value = i < 4 ? decode_int16(msg[offset], msg[offset + 1]) : raw_value;
      ESP_LOGD(TAG, "FoxESS realtime Rev.%u raw=0x%04X value=%ld",
               static_cast<unsigned>(i + 1), static_cast<unsigned>(raw_value),
               static_cast<long>(value));
      this->last_rev_values_[i] = raw_value;
    }
  }
  this->has_rev_sample_ = true;

  // EPS output
  publish_sensor_state(this->eps_voltage_,
                       decode_int16(msg[MsgOffset::EPS_VOLTAGE_MSB],
                                    msg[MsgOffset::EPS_VOLTAGE_MSB + 1]),
                       0.1f);
  publish_sensor_state(this->eps_current_,
                       decode_int16(msg[MsgOffset::EPS_CURRENT_MSB],
                                    msg[MsgOffset::EPS_CURRENT_MSB + 1]),
                       0.1f);
  publish_sensor_state(this->eps_power_,
                       decode_int16(msg[MsgOffset::EPS_POWER_MSB],
                                    msg[MsgOffset::EPS_POWER_MSB + 1]),
                       1.0f);

  // temps
  publish_sensor_state(this->boost_temp_,
                       decode_int16(msg[MsgOffset::BOOST_TEMP_MSB],
                                    msg[MsgOffset::BOOST_TEMP_LSB]),
                       1.0f);
  publish_sensor_state(this->inverter_temp_,
                       decode_int16(msg[MsgOffset::INVERTER_TEMP_MSB],
                                    msg[MsgOffset::INVERTER_TEMP_LSB]),
                       1.0f);
  publish_sensor_state(this->ambient_temp_,
                       decode_int16(msg[MsgOffset::AMBIENT_TEMP_MSB],
                                    msg[MsgOffset::AMBIENT_TEMP_LSB]),
                       1.0f);

  // energy
  publish_sensor_state(this->energy_production_day_,
                       decode_uint16(msg[MsgOffset::ENERGY_DAY_MSB],
                                     msg[MsgOffset::ENERGY_DAY_LSB]),
                       0.1f);
  publish_unsigned_sensor_state(this->total_energy_production_,
                                decode_uint32(msg[MsgOffset::TOTAL_ENERGY_MSB0],
                                              msg[MsgOffset::TOTAL_ENERGY_MSB0 + 1],
                                              msg[MsgOffset::TOTAL_ENERGY_MSB0 + 2],
                                              msg[MsgOffset::TOTAL_ENERGY_MSB0 + 3]),
                                0.1f);

  // Additional cumulative energy counters documented in the 152-byte realtime payload.
  publish_unsigned_sensor_state(this->from_grid_yield_generation_,
                                decode_uint32(msg[MsgOffset::FROM_GRID_YIELD_GENERATION_MSB],
                                              msg[MsgOffset::FROM_GRID_YIELD_GENERATION_MSB + 1],
                                              msg[MsgOffset::FROM_GRID_YIELD_GENERATION_MSB + 2],
                                              msg[MsgOffset::FROM_GRID_YIELD_GENERATION_MSB + 3]),
                                0.1f);
  publish_unsigned_sensor_state(this->feedin_generation_1_,
                                decode_uint32(msg[MsgOffset::FEEDIN_GENERATION_1_MSB],
                                              msg[MsgOffset::FEEDIN_GENERATION_1_MSB + 1],
                                              msg[MsgOffset::FEEDIN_GENERATION_1_MSB + 2],
                                              msg[MsgOffset::FEEDIN_GENERATION_1_MSB + 3]),
                                0.1f);
  publish_unsigned_sensor_state(this->feedin_generation_2_,
                                decode_uint32(msg[MsgOffset::FEEDIN_GENERATION_2_MSB],
                                              msg[MsgOffset::FEEDIN_GENERATION_2_MSB + 1],
                                              msg[MsgOffset::FEEDIN_GENERATION_2_MSB + 2],
                                              msg[MsgOffset::FEEDIN_GENERATION_2_MSB + 3]),
                                0.1f);
  publish_unsigned_sensor_state(this->consumption_generation_1_,
                                decode_uint32(msg[MsgOffset::CONSUMPTION_GENERATION_1_MSB],
                                              msg[MsgOffset::CONSUMPTION_GENERATION_1_MSB + 1],
                                              msg[MsgOffset::CONSUMPTION_GENERATION_1_MSB + 2],
                                              msg[MsgOffset::CONSUMPTION_GENERATION_1_MSB + 3]),
                                0.1f);
  publish_unsigned_sensor_state(this->consumption_generation_2_,
                                decode_uint32(msg[MsgOffset::CONSUMPTION_GENERATION_2_MSB],
                                              msg[MsgOffset::CONSUMPTION_GENERATION_2_MSB + 1],
                                              msg[MsgOffset::CONSUMPTION_GENERATION_2_MSB + 2],
                                              msg[MsgOffset::CONSUMPTION_GENERATION_2_MSB + 3]),
                                0.1f);
  publish_unsigned_sensor_state(this->loads_generation_,
                                decode_uint32(msg[MsgOffset::LOADS_GENERATION_MSB],
                                              msg[MsgOffset::LOADS_GENERATION_MSB + 1],
                                              msg[MsgOffset::LOADS_GENERATION_MSB + 2],
                                              msg[MsgOffset::LOADS_GENERATION_MSB + 3]),
                                0.1f);

  // These are documented as UINT16 values without a published interpretation/enum.
  publish_sensor_state(this->master_state_, decode_uint16(msg[MsgOffset::MASTER_STATE_MSB],
                                                          msg[MsgOffset::MASTER_STATE_MSB + 1]), 1.0f);
  publish_sensor_state(this->pv_input_number_, decode_uint16(msg[MsgOffset::PV_INPUT_NUMBER_MSB],
                                                             msg[MsgOffset::PV_INPUT_NUMBER_MSB + 1]), 1.0f);

  // Keep fault registers as hexadecimal text so all 32 raw bits remain exact.
  if (this->fault_registers_ != nullptr) {
    char summary[128]{};
    std::size_t summary_len = 0;
    for (std::size_t i = 0; i < 8; i++) {
      const std::size_t base = MsgOffset::ERROR_BLOCK_BEGIN + i * 4;
      const uint32_t value = decode_uint32(msg[base], msg[base + 1], msg[base + 2], msg[base + 3]);
      const int written = snprintf(summary + summary_len, sizeof(summary) - summary_len,
                                   "%sF%u=0x%08lX", i == 0 ? "" : " ", static_cast<unsigned>(i + 1),
                                   static_cast<unsigned long>(value));
      if (written < 0 || static_cast<std::size_t>(written) >= sizeof(summary) - summary_len)
        break;
      summary_len += static_cast<std::size_t>(written);
    }
    if (!this->has_fault_registers_value_ || summary != this->last_fault_registers_value_) {
      this->fault_registers_->publish_state(summary);
      this->last_fault_registers_value_ = summary;
      this->has_fault_registers_value_ = true;
    }
  }

  // error block check
  if (!std::all_of(msg.begin() + MsgOffset::ERROR_BLOCK_BEGIN,
                   msg.begin() + MsgOffset::ERROR_BLOCK_END,
                   [](uint8_t b) { return b == 0; })) {
    this->set_inverter_mode(2);  // ERROR
    return;
  }

  this->set_inverter_mode(1);  // ONLINE
}

void FoxessSolar::parse_device_attributes() {
  auto &msg = this->input_buffer;
  const uint16_t payload_len = decode_uint16(msg[7], msg[8]);
  if (payload_len != MsgOffset::DEVICE_ATTRIBUTE_PAYLOAD_LEN) {
    ESP_LOGD(TAG, "Unsupported FoxESS device attribute payload length: %u", payload_len);
    return;
  }

  if (this->master_version_ != nullptr)
    this->master_version_->publish_state(decode_ascii(&msg[MsgOffset::DEVICE_ATTRIBUTE_MASTER_VERSION], 6));
  if (this->slave_version_ != nullptr)
    this->slave_version_->publish_state(decode_ascii(&msg[MsgOffset::DEVICE_ATTRIBUTE_SLAVE_VERSION], 6));
  if (this->manager_version_ != nullptr)
    this->manager_version_->publish_state(decode_ascii(&msg[MsgOffset::DEVICE_ATTRIBUTE_MANAGER_VERSION], 6));
  if (this->device_factory_ != nullptr)
    this->device_factory_->publish_state(decode_device_factory(
        decode_uint16(msg[MsgOffset::DEVICE_ATTRIBUTE_FACTORY],
                      msg[MsgOffset::DEVICE_ATTRIBUTE_FACTORY + 1])));
  if (this->device_type_ != nullptr)
    this->device_type_->publish_state(decode_ascii(&msg[MsgOffset::DEVICE_ATTRIBUTE_TYPE], 2));
  if (this->device_model_ != nullptr)
    this->device_model_->publish_state(decode_ascii(&msg[MsgOffset::DEVICE_ATTRIBUTE_MODEL], 16));
  publish_sensor_state(this->device_capacity_,
                       decode_uint16(msg[MsgOffset::DEVICE_ATTRIBUTE_CAPACITY],
                                     msg[MsgOffset::DEVICE_ATTRIBUTE_CAPACITY + 1]),
                       1.0f);
  if (this->afg_version_ != nullptr)
    this->afg_version_->publish_state(decode_ascii(&msg[MsgOffset::DEVICE_ATTRIBUTE_AFG_VERSION], 6));
}

void FoxessSolar::parse_heartbeat() {
  auto &msg = this->input_buffer;
  const uint16_t payload_len = decode_uint16(msg[7], msg[8]);
  if (payload_len != MsgOffset::HEARTBEAT_PAYLOAD_LEN) {
    ESP_LOGD(TAG, "Unsupported FoxESS heartbeat payload length: %u", payload_len);
    return;
  }

  const std::string protocol_version =
      decode_ascii(&msg[MsgOffset::HEARTBEAT_PROTOCOL_VERSION], 6);
  if (!protocol_version.empty() && protocol_version != this->protocol_version_value_) {
    this->protocol_version_value_ = protocol_version;
    ESP_LOGD(TAG, "FoxESS protocol version: %s", this->protocol_version_value_.c_str());
  }
  if (this->protocol_version_ != nullptr)
    this->protocol_version_->publish_state(protocol_version);
  // Serial number is exposed only when explicitly configured. ESPHome VERBOSE logs may include its state.
  if (this->serial_number_ != nullptr)
    this->serial_number_->publish_state(decode_ascii(&msg[MsgOffset::HEARTBEAT_SERIAL_NUMBER], 15));
}

void FoxessSolar::set_inverter_mode(uint32_t mode) {
  this->inverter_mode_ = mode;
  if (this->inverter_status_ != nullptr)
    this->inverter_status_->publish_state(mode);
}

}  // namespace foxess_solar
}  // namespace esphome
