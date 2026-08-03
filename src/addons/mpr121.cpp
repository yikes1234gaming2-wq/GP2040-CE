#include "addons/mpr121.h"
#include "storagemanager.h"
#include "gamepad.h"
#include "peripheralmanager.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "pico/time.h"

#define MPR121_TOUCH_STATUS_LSB 0x00
#define MPR121_ECR              0x5E
#define MPR121_SOFT_RESET       0x80

// Timeouts in microseconds (10ms for setup, 2ms for per-frame read)
#define SETUP_TIMEOUT_US  10000 
#define PROCESS_TIMEOUT_US 2000

void MPR121Input::setup() {
    // Rely on GP2040-CE PeripheralManager for I2C bus setup
    // Ensure I2C is initialized via PeripheralManager before accessing
    PeripheralI2C* i2c = PeripheralManager::getInstance().getI2C(0);
    if (i2c == nullptr || !i2c->configured()) {
        // Fallback manually only if PeripheralManager isn't managing it
        gpio_set_function(0, GPIO_FUNC_I2C);
        gpio_set_function(1, GPIO_FUNC_I2C);
        gpio_pull_up(0);
        gpio_pull_up(1);
        i2c_init(i2c0, 100 * 1000); // 100kHz
    }

    sleep_ms(100); // Power stabilization delay

    // Safe write helper with standard setup timeout
    auto safe_write = [](uint8_t reg, uint8_t val) -> bool {
        uint8_t buf[2] = { reg, val };
        int res = i2c_write_timeout_us(i2c0, MPR121_I2C_ADDR, buf, 2, false, SETUP_TIMEOUT_US);
        return res == 2;
    };

    // Soft reset the MPR121
    safe_write(0x80, 0x63);
    sleep_ms(10);

    // 1. Put MPR121 in Stop Mode to allow configuration
    if (!safe_write(MPR121_ECR, 0x00)) {
        return; // Device not acknowledging at 0x5A
    }

    // 2. Set touch/release thresholds for ELE0 through ELE11
    // (Touch: 12, Release: 6)
    for (int i = 0; i < 12; i++) {
        safe_write((uint8_t)(0x41 + (i * 2)), 12);     // Touch threshold
        safe_write((uint8_t)(0x41 + (i * 2) + 1), 6);  // Release threshold
    }

    // Set baseline filtering defaults
    safe_write(0x5D, 0x04); // FFI / CDC configuration

    // 3. Enter Run Mode (Enable all 12 electrodes with baseline tracking)
    safe_write(MPR121_ECR, 0x8F);
}

void MPR121Input::process() {
    uint8_t buf[2] = {0};
    uint8_t reg = MPR121_TOUCH_STATUS_LSB;

    // Send register address to read from
    if (i2c_write_timeout_us(i2c0, MPR121_I2C_ADDR, &reg, 1, true, PROCESS_TIMEOUT_US) < 0) {
        return; // Skip frame on timeout/bus error
    }

    // Read 2 bytes of touch status (ELE0-ELE7 in byte 0, ELE8-ELE11 in byte 1)
    if (i2c_read_timeout_us(i2c0, MPR121_I2C_ADDR, buf, 2, false, PROCESS_TIMEOUT_US) < 0) {
        return; // Skip frame on timeout/bus error
    }

    uint16_t touched = ((uint16_t)buf[1] << 8) | buf[0];
    if (touched == 0) return; // Save cycles if nothing is touched

    Gamepad *gamepad = Storage::getInstance().GetGamepad();

    // Directional mapping
    if (touched & (1 << 1))  gamepad->state.dpad |= GAMEPAD_MASK_UP;    // ELE1
    if (touched & (1 << 4))  gamepad->state.dpad |= GAMEPAD_MASK_DOWN;  // ELE4
    if (touched & (1 << 0))  gamepad->state.dpad |= GAMEPAD_MASK_LEFT;  // ELE0
    if (touched & (1 << 2))  gamepad->state.dpad |= GAMEPAD_MASK_RIGHT; // ELE2

    // Action buttons mapping
    if (touched & (1 << 5))  gamepad->state.buttons |= GAMEPAD_MASK_B1; // ELE5
    if (touched & (1 << 6))  gamepad->state.buttons |= GAMEPAD_MASK_B2; // ELE6
    if (touched & (1 << 9))  gamepad->state.buttons |= GAMEPAD_MASK_R1; // ELE9
    if (touched & (1 << 3))  gamepad->state.buttons |= GAMEPAD_MASK_S1; // ELE3 (Start)
    if (touched & (1 << 7))  gamepad->state.buttons |= GAMEPAD_MASK_S2; // ELE7 (Select)
    if (touched & (1 << 8))  gamepad->state.buttons |= GAMEPAD_MASK_R2; // ELE8
}
