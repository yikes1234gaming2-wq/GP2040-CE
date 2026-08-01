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
    // 1. Enable Onboard LED as a diagnostic indicator
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, 0); // Off by default

    // 2. Force-claim GP0 (SDA) and GP1 (SCL)
    gpio_init(0);
    gpio_init(1);
    gpio_set_function(0, GPIO_FUNC_I2C);
    gpio_set_function(1, GPIO_FUNC_I2C);
    gpio_pull_up(0);
    gpio_pull_up(1);

    // 3. Re-initialize i2c0 at 100kHz standard mode
    i2c_deinit(i2c0);
    i2c_init(i2c0, 100 * 1000);

    sleep_ms(100); // Allow lines and regulator to settle

    // 4. I2C BUS SCANNER: Probe every possible address (0x08 to 0x77)
    bool device_found = false;
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        uint8_t rxdata;
        // Probe address with a dummy read
        int ret = i2c_read_blocking(i2c0, addr, &rxdata, 1, false);
        if (ret >= 0) {
            device_found = true;
            break; // Acknowledge received!
        }
    }

    // IF ANY DEVICE RESPONDS ON THE BUS: Turn ON onboard LED!
    if (device_found) {
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
    } else {
        // No device acknowledged on GP0/GP1
        return;
    }

    // 5. Soft Reset MPR121
    uint8_t reset_buf[2] = { MPR121_SOFT_RESET, 0x63 };
    i2c_write_blocking(i2c0, MPR121_I2C_ADDR, reset_buf, 2, false);
    sleep_ms(20);

    // 6. Enter Stop Mode for register setup
    uint8_t ecr_stop[2] = { MPR121_ECR, 0x00 };
    i2c_write_blocking(i2c0, MPR121_I2C_ADDR, ecr_stop, 2, false);

    // 7. Configure Touch/Release thresholds for all 12 channels
    for (int i = 0; i < 12; i++) {
        uint8_t tth[2] = { (uint8_t)(0x41 + (i * 2)), 12 };
        uint8_t rth[2] = { (uint8_t)(0x41 + (i * 2) + 1), 6 };
        i2c_write_blocking(i2c0, MPR121_I2C_ADDR, tth, 2, false);
        i2c_write_blocking(i2c0, MPR121_I2C_ADDR, rth, 2, false);
    }

    // 8. Run Mode (Enable ELE0 - ELE11)
    uint8_t ecr_run[2] = { MPR121_ECR, 0x8F };
    i2c_write_blocking(i2c0, MPR121_I2C_ADDR, ecr_run, 2, false);
}

void MPR121Input::process() {
    uint8_t buf[2] = {0};
    uint8_t reg = MPR121_TOUCH_STATUS_LSB;

    if (i2c_write_blocking(i2c0, MPR121_I2C_ADDR, &reg, 1, true) < 0) return;
    if (i2c_read_blocking(i2c0, MPR121_I2C_ADDR, buf, 2, false) < 0) return;

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
