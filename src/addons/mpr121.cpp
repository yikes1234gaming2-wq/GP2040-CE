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

// Standard Pico onboard LED pin for visual diagnostic
#ifndef PICO_DEFAULT_LED_PIN
#define PICO_DEFAULT_LED_PIN 25
#endif

bool MPR121Input::available() {
    return true;
}

void MPR121Input::setup() {
    // 1. Enable Onboard LED as an I2C handshake indicator
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, 0); // Off by default

    // 2. Force-claim GP0 (SDA) and GP1 (SCL) to prevent button-mapping overrides
    gpio_init(0);
    gpio_init(1);
    gpio_set_function(0, GPIO_FUNC_I2C);
    gpio_set_function(1, GPIO_FUNC_I2C);
    gpio_pull_up(0);
    gpio_pull_up(1);

    // 3. Re-initialize i2c0 at 100kHz standard mode for maximum bus reliability
    i2c_deinit(i2c0);
    i2c_init(i2c0, 100 * 1000);

    // Give hardware and Adafruit regulator 50ms to fully stabilize
    sleep_ms(50);

    // 4. Hardware Ping Test: Check if MPR121 responds at address 0x5A
    uint8_t test_reg = MPR121_ECR;
    uint8_t test_val = 0;
    int ack = i2c_write_blocking(i2c0, MPR121_I2C_ADDR, &test_reg, 1, true);
    if (ack >= 0) {
        ack = i2c_read_blocking(i2c0, MPR121_I2C_ADDR, &test_val, 1, false);
    }

    // IF ACK IS RECEIVED: Light up the onboard LED!
    if (ack >= 0) {
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
    } else {
        // Stop setup if communication failed
        return;
    }

    // 5. Soft Reset MPR121
    uint8_t reset_buf[2] = { MPR121_SOFT_RESET, 0x63 };
    i2c_write_blocking(i2c0, MPR121_I2C_ADDR, reset_buf, 2, false);
    sleep_ms(20); // Delay for reset completion

    // 6. Enter Stop Mode to allow register updates
    uint8_t ecr_stop[2] = { MPR121_ECR, 0x00 };
    i2c_write_blocking(i2c0, MPR121_I2C_ADDR, ecr_stop, 2, false);

    // 7. Configure Touch and Release thresholds for all 12 electrodes
    for (int i = 0; i < 12; i++) {
        uint8_t tth[2] = { (uint8_t)(0x41 + (i * 2)), 12 };
        uint8_t rth[2] = { (uint8_t)(0x41 + (i * 2) + 1), 6 };
        i2c_write_blocking(i2c0, MPR121_I2C_ADDR, tth, 2, false);
        i2c_write_blocking(i2c0, MPR121_I2C_ADDR, rth, 2, false);
    }

    // 8. Enable touch channels (ELE0 - ELE11) with baseline tracking
    uint8_t ecr_run[2] = { MPR121_ECR, 0x8F };
    i2c_write_blocking(i2c0, MPR121_I2C_ADDR, ecr_run, 2, false);
}

void MPR121Input::process() {
    uint8_t buf[2] = {0};
    uint8_t reg = MPR121_TOUCH_STATUS_LSB;

    // Read 2 status bytes starting from register 0x00
    if (i2c_write_blocking(i2c0, MPR121_I2C_ADDR, &reg, 1, true) < 0) return;
    if (i2c_read_blocking(i2c0, MPR121_I2C_ADDR, buf, 2, false) < 0) return;

    // Convert LSB/MSB to a 16-bit touched state
    uint16_t touched = ((uint16_t)buf[1] << 8) | buf[0];

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
    if (touched & (1 << 3))  gamepad->state.buttons |= GAMEPAD_MASK_A1; // ELE3
    if (touched & (1 << 7))  gamepad->state.buttons |= GAMEPAD_MASK_A2; // ELE7
    if (touched & (1 << 8))  gamepad->state.buttons |= GAMEPAD_MASK_R2; // ELE8
}
