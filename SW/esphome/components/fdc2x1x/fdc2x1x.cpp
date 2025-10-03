#include "fdc2x1x.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace fdc2x1x {

static const char *const TAG = "fdc2x1x";

void FDC2x1xSensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up FDC2x1x sensor...");

  // Give FDC time to be ready after power-up
  delay(100);

  // Try to read manufacturer ID register (16-bit)
  uint16_t manufacturer_id;
  if (this->read_register16(REG_MANUFACTURER_ID, manufacturer_id) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Failed to read manufacturer ID register");
    this->mark_failed();
    return;
  }

  ESP_LOGI(TAG, "Manufacturer ID: 0x%04X", manufacturer_id);

  if (manufacturer_id != EXPECTED_MANUFACTURER_ID) {
    ESP_LOGE(TAG, "Unexpected manufacturer ID: 0x%04X (expected: 0x%04X)",
             manufacturer_id, EXPECTED_MANUFACTURER_ID);
    this->mark_failed();
    return;
  }

  // Try to read device ID register (16-bit: 0x3054 or 0x3055)
  uint16_t device_id;
  if (this->read_register16(REG_DEVICE_ID, device_id) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Failed to read device ID register");
    this->mark_failed();
    return;
  }

  ESP_LOGI(TAG, "Device ID: 0x%04X", device_id);
  this->device_detected_ = true;

  // Reset the device first
  ESP_LOGI(TAG, "Resetting device...");
  if (this->write_register16(REG_RESET_DEV, 0x8000) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Failed to reset device");
    this->mark_failed();
    return;
  }

  // Wait for reset to complete
  delay(10);
  ESP_LOGI(TAG, "Device reset complete");

  // Configure sensor following TI recommended sequence
  ESP_LOGD(TAG, "Configuring sensor using TI recommended sequence...");

  // Step 1: Set the dividers for channel 0
  if (this->write_register16(REG_CLOCK_DIVIDERS_CH0, CLOCK_DIVIDER_CH0) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Failed to set clock dividers");
    this->mark_failed();
    return;
  }

  // Step 2: Set sensor drive current
  if (this->write_register16(REG_DRIVE_CH0, DRIVE_CURRENT) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Failed to set drive current");
    this->mark_failed();
    return;
  }

  // Step 3: Set settling time for Channel 0
  if (this->write_register16(REG_SETTLECOUNT_CH0, SETTLECOUNT_CH0) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Failed to set settle count");
    this->mark_failed();
    return;
  }

  // Step 5: Set conversion time / reference count
  if (this->write_register16(REG_RCOUNT_CH0, RCOUNT_CH0) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Failed to set reference count");
    this->mark_failed();
    return;
  }

  // Step 7: Program the MUX_CONFIG register
  if (this->write_register16(REG_MUX_CONFIG, MUX_CONFIG) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Failed to set MUX config");
    this->mark_failed();
    return;
  }

  // Program the ERROR_CONFIG register
  if (this->write_register16(REG_ERROR_CONFIG, ERROR_CONFIG) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Failed to set MUX config");
    this->mark_failed();
    return;
  }

  // Step 8: Finally, program the CONFIG register
  if (this->write_register16(REG_CONFIG, CONFIG) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Failed to set configuration");
    this->mark_failed();
    return;
  }

  ESP_LOGI(TAG, "FDC2x1x sensor initialized successfully");
}

void FDC2x1xSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "FDC2x1x Sensor:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with FDC2x1x failed!");
  } else {
    ESP_LOGCONFIG(TAG, "  Device detected: %s", this->device_detected_ ? "YES" : "NO");
  }
  LOG_UPDATE_INTERVAL(this);
}

void FDC2x1xSensor::update() {
  if (this->is_failed()) {
    return;
  }

  // Check status register first
  uint16_t status;
  if (this->read_register16(REG_STATUS, status) == i2c::ERROR_OK) {
    ESP_LOGD(TAG, "Status register: 0x%04X", status);
  
    // Bit 6: Data Ready
    ESP_LOGD(TAG, "  Data ready: %s", (status & 0x0040) ? "YES" : "NO");
    
    // Bit 11: Watchdog Timeout Error
    ESP_LOGD(TAG, "  Watchdog error: %s", (status & 0x0800) ? "YES" : "NO");
    
    // Bit 10: Amplitude High Warning
    ESP_LOGD(TAG, "  Amplitude HIGH warning: %s", (status & 0x0400) ? "YES" : "NO");
    
    // Bit 9: Amplitude Low Warning
    ESP_LOGD(TAG, "  Amplitude LOW warning: %s", (status & 0x0200) ? "YES" : "NO");
    
    // Bits 15:14: Error Channel (which channel caused the error)
    uint8_t err_chan = (status >> 14) & 0x03;
    if (status & 0xC000) {  // If any error channel bits are set
      ESP_LOGD(TAG, "  Error channel: CH%d", err_chan);
    }

    // Unread conversion flags
    ESP_LOGD(TAG, "  CH0 unread: %s", (status & 0x0008) ? "YES" : "NO");
    ESP_LOGD(TAG, "  CH1 unread: %s", (status & 0x0004) ? "YES" : "NO");
    ESP_LOGD(TAG, "  CH2 unread: %s", (status & 0x0002) ? "YES" : "NO");
    ESP_LOGD(TAG, "  CH3 unread: %s", (status & 0x0001) ? "YES" : "NO");
  }

  // Read back configuration registers for debugging
  // uint16_t readback;
  // if (this->read_register16(REG_CONFIG, readback) == i2c::ERROR_OK) {
  //   ESP_LOGD(TAG, "CONFIG register: 0x%04X (expected: 0x%04X)", readback, CONFIG);
  // }
  // if (this->read_register16(REG_MUX_CONFIG, readback) == i2c::ERROR_OK) {
  //   ESP_LOGD(TAG, "MUX_CONFIG register: 0x%04X (expected: 0x%04X)", readback, MUX_CONFIG);
  // }
  // if (this->read_register16(REG_DRIVE_CH0, readback) == i2c::ERROR_OK) {
  //   uint8_t idrive = (readback >> 11) & 0x1F;
  //   ESP_LOGD(TAG, "DRIVE_CH0 register: 0x%04X, IDRIVE: %d", readback, idrive);
  // }
  // if (this->read_register16(REG_CLOCK_DIVIDERS_CH0, readback) == i2c::ERROR_OK) {
  //   uint8_t ref_div = (readback >> 10) & 0x3F;
  //   uint8_t fin_sel = (readback >> 8) & 0x03;
  //   ESP_LOGD(TAG, "CLOCK_DIV_CH0: 0x%04X, REF_DIV: %d, FIN_SEL: %d", readback, ref_div, fin_sel);
  // }

  // Read channel 0 data
  uint16_t ch0_data_msb;
  if (this->read_register16(REG_DATA_CH0_MSB, ch0_data_msb) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Failed to read channel 0 data MSB");
    return;
  }

  bool ch0_watchdog_timeout = ch0_data_msb & (1 << 13);
  bool ch0_amplitude_warn = ch0_data_msb & (1 << 12);
  uint16_t ch0_data_res_msb = ch0_data_msb & 0xFFF;

  ESP_LOGD(TAG, "Channel 0 MSB: 0x%04X (%u)", ch0_data_msb, ch0_data_msb);

  uint16_t ch0_data_lsb;
  if (this->read_register16(REG_DATA_CH0_LSB, ch0_data_lsb) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Failed to read channel 0 data LSB");
    return;
  }

  ESP_LOGD(TAG, "Channel 0 LSB: 0x%04X (%u)", ch0_data_lsb, ch0_data_lsb);

  uint32_t ch0_res = ch0_data_res_msb << 16 | ch0_data_lsb; // TODO add masked MSB
  ESP_LOGD(TAG, "Channel 0 data: 0x%04X (%u) amp:%u, timeout:%u", ch0_res, ch0_res, ch0_amplitude_warn, ch0_watchdog_timeout);

  // Publish to sensor if configured (use 32-bit value)
  if (this->channel0_sensor_ != nullptr) {
    this->channel0_sensor_->publish_state(ch0_res);
  }
}

i2c::ErrorCode FDC2x1xSensor::write_register16(uint8_t reg, uint16_t value) {
  uint8_t data[2];
  data[0] = (value >> 8) & 0xFF;  // MSB first
  data[1] = value & 0xFF;         // LSB second

  return this->write_register(reg, data, 2);
}

i2c::ErrorCode FDC2x1xSensor::read_register16(uint8_t reg, uint16_t &value) {
  uint8_t data[2];
  i2c::ErrorCode err = this->read_register(reg, data, 2);
  value = (data[0] << 8) | data[1];  // MSB first
  return err;
}

}  // namespace fdc2x1x
}  // namespace esphome