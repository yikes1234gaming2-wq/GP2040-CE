#include "addons/mpr121.h"
#include "storagemanager.h"
#include "gamepad.h"
#include "hardware/i2c.h"
#include "pico/time.h" // Needed for sleep_ms()

#define MPR121_TOUCH_STATUS_LSB 0x00
#define MPR121_ECR              0x5E
#define MPR121_SOFT_RESET       0x80

bool MPR121Input::available() {
    return true;
}

void MPR121Input::setup() {
    // Setup GP0 (SDA) and GP1 (SCL) at 400kHz
    i2c_init(i2c0, 400 * 1000);
    gpio_set_function(0, GPIO_FUNC_I2C);
    gpio_set_function(1, GPIO_FUNC_I2C);
    gpio_pull_up(0);
    gpio_pull_up(1);

    // Give hardware time to stabilize power on boot
    sleep_ms(10);

    // Soft Reset
    uint8_t reset_buf[2] = { MPR121_SOFT_RESET, 0x63 };
    i2c_write_blocking(i2c0, MPR121_I2C_ADDR, reset_buf, 2, false);

    // MANDATORY: Wait 10ms for MPR121 to finish rebooting before writing registers
    sleep_ms(10);

    // Disable ECR to enter Stop Mode for threshold configuration
    uint8_t ecr_stop[2] = { MPR121_ECR, 0x00 };
    i2c_write_blocking(i2c0, MPR121_I2C_ADDR, ecr_stop, 2, false);

    // Set touch/release thresholds for all 12 channels
    // (Addresses 0x41/0x42 through 0x57/0x58)
    for (int i = 0; i < 12; i++) {
        uint8_t tth[2] = { (uint8_t)(0x41 + (i * 2)), 12 };
        uint8_t rth[2] = { (uint8_t)(0x41 + (i * 2) + 1), 6 };
        i2c_write_blocking(i2c0, MPR121_I2C_ADDR, tth, 2, false);
        i2c_write_blocking(i2c0, MPR121_I2C_ADDR, rth, 2, false);
    }

    // Enable touch channels (ELE0 - ELE11) with baseline tracking enabled
    uint8_t ecr_run[2] = { MPR121_ECR, 0x8F };
    i2c_write_blocking(i2c0, MPR121_I2C_ADDR, ecr_run, 2, false);
}

void MPR121Input::process() {
    uint8_t buf[2] = {0};
    uint8_t reg = MPR121_TOUCH_STATUS_LSB;

    // Request touch status bytes from 0x00
    if (i2c_write_blocking(i2c0, MPR121_I2C_ADDR, &reg, 1, true) < 0) {
        return; // I2C read failed or device not responding
    }

    if (i2c_read_blocking(i2c0, MPR121_I2C_ADDR, buf, 2, false) < 0) {
        return;
    }

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
