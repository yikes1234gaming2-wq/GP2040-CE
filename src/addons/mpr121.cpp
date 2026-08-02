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

#ifndef PICO_DEFAULT_LED_PIN
#define PICO_DEFAULT_LED_PIN 25
#endif

// Timeout for non-blocking I2C operations (1000 microseconds = 1ms)
#define I2C_TIMEOUT_US 1000 

void MPR121Input::setup() {
    // 1. Enable Onboard LED Heartbeat
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, 1); 

    // 2. EXPLICITLY initialize i2c0 and pins GP0 (SDA) / GP1 (SCL)
    // This prevents the RP2040 from hard-faulting on unclocked I2C hardware!
    gpio_set_function(0, GPIO_FUNC_I2C);
    gpio_set_function(1, GPIO_FUNC_I2C);
    gpio_pull_up(0);
    gpio_pull_up(1);
    
    i2c_init(i2c0, 100 * 1000); // 100kHz standard speed

    sleep_ms(150); // Power stabilization delay

    // 3. Soft Reset MPR121
    uint8_t reset_buf[2] = { MPR121_SOFT_RESET, 0x63 };
    i2c_write_timeout_us(i2c0, MPR121_I2C_ADDR, reset_buf, 2, false, I2C_TIMEOUT_US);
    sleep_ms(20);

    // 4. Enter Stop Mode
    uint8_t ecr_stop[2] = { MPR121_ECR, 0x00 };
    i2c_write_timeout_us(i2c0, MPR121_I2C_ADDR, ecr_stop, 2, false, I2C_TIMEOUT_US);

    // 5. Threshold Configuration
    for (int i = 0; i < 12; i++) {
        uint8_t tth[2] = { (uint8_t)(0x41 + (i * 2)), 12 };
        uint8_t rth[2] = { (uint8_t)(0x41 + (i * 2) + 1), 6 };
        i2c_write_timeout_us(i2c0, MPR121_I2C_ADDR, tth, 2, false, I2C_TIMEOUT_US);
        i2c_write_timeout_us(i2c0, MPR121_I2C_ADDR, rth, 2, false, I2C_TIMEOUT_US);
    }

    // 6. Run Mode
    uint8_t ecr_run[2] = { MPR121_ECR, 0x8F };
    i2c_write_timeout_us(i2c0, MPR121_I2C_ADDR, ecr_run, 2, false, I2C_TIMEOUT_US);
}

void MPR121Input::process() {
//    uint8_t buf[2] = {0};
//    uint8_t reg = MPR121_TOUCH_STATUS_LSB;

    // Non-blocking register write request
//    if (i2c_write_timeout_us(i2c0, MPR121_I2C_ADDR, &reg, 1, true, I2C_TIMEOUT_US) < 0) {
//        return; // Bus failed/timed out, skip frame without blocking main loop
//    }

    // Non-blocking register data read
//    if (i2c_read_timeout_us(i2c0, MPR121_I2C_ADDR, buf, 2, false, I2C_TIMEOUT_US) < 0) {
//        return; // Bus failed/timed out, skip frame without blocking main loop
//    }

//    uint16_t touched = ((uint16_t)buf[1] << 8) | buf[0];

//    Gamepad *gamepad = Storage::getInstance().GetGamepad();

    // Directional mapping
//    if (touched & (1 << 1))  gamepad->state.dpad |= GAMEPAD_MASK_UP;    // ELE1
//    if (touched & (1 << 4))  gamepad->state.dpad |= GAMEPAD_MASK_DOWN;  // ELE4
//    if (touched & (1 << 0))  gamepad->state.dpad |= GAMEPAD_MASK_LEFT;  // ELE0
//    if (touched & (1 << 2))  gamepad->state.dpad |= GAMEPAD_MASK_RIGHT; // ELE2

    // Action buttons mapping
//    if (touched & (1 << 5))  gamepad->state.buttons |= GAMEPAD_MASK_B1; // ELE5
//    if (touched & (1 << 6))  gamepad->state.buttons |= GAMEPAD_MASK_B2; // ELE6
//    if (touched & (1 << 9))  gamepad->state.buttons |= GAMEPAD_MASK_R1; // ELE9
//    if (touched & (1 << 3))  gamepad->state.buttons |= GAMEPAD_MASK_A1; // ELE3
//    if (touched & (1 << 7))  gamepad->state.buttons |= GAMEPAD_MASK_A2; // ELE7
//    if (touched & (1 << 8))  gamepad->state.buttons |= GAMEPAD_MASK_R2; // ELE8
}
