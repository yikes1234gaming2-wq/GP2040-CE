#include "addons/mpr121.h"
#include "storagemanager.h"
#include "gamepad.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "pico/time.h"

#define MPR121_TOUCH_STATUS_LSB 0x00
#define MPR121_ECR              0x5E

#define SETUP_TIMEOUT_US   10000 
#define PROCESS_TIMEOUT_US  1000

static bool mpr121_present = false;

void MPR121Input::setup() {
    mpr121_present = false;

    const uint sda_pin = 0;
    const uint scl_pin = 1;

    // 1. Temporarily set pins as general GPIO inputs with pull-ups to check line health
    gpio_init(sda_pin);
    gpio_init(scl_pin);
    gpio_set_dir(sda_pin, GPIO_IN);
    gpio_set_dir(scl_pin, GPIO_IN);
    gpio_pull_up(sda_pin);
    gpio_pull_up(scl_pin);

    sleep_us(100); // Small settling delay

    // If either line is stuck LOW, the bus is shorted or miswired. 
    // ABORT immediately to prevent i2c_init from freezing the Pico!
    if (!gpio_get(sda_pin) || !gpio_get(scl_pin)) {
        return; 
    }

    // 2. Lines are clear! Now assign to I2C peripheral hardware
    gpio_set_function(sda_pin, GPIO_FUNC_I2C);
    gpio_set_function(scl_pin, GPIO_FUNC_I2C);

    // Initialize I2C hardware at standard speed
    i2c_init(i2c0, 100 * 1000);

    sleep_ms(50); // Power stabilization delay

    auto safe_write = [](uint8_t reg, uint8_t val) -> bool {
        uint8_t buf[2] = { reg, val };
        int res = i2c_write_timeout_us(i2c0, MPR121_I2C_ADDR, buf, 2, false, SETUP_TIMEOUT_US);
        return res == 2;
    };

    // Probe: Try setting ECR to 0x00 (Stop Mode)
    if (!safe_write(MPR121_ECR, 0x00)) {
        return; // MPR121 not answering at 0x5A, safely abort
    }

    // Soft reset sequence
    safe_write(0x80, 0x63);
    sleep_ms(10);
    safe_write(MPR121_ECR, 0x00);

    // Set touch/release thresholds (ELE0 to ELE11)
    for (int i = 0; i < 12; i++) {
        safe_write((uint8_t)(0x41 + (i * 2)), 12);     // Touch
        safe_write((uint8_t)(0x41 + (i * 2) + 1), 6);  // Release
    }

    // Default baseline filtering
    safe_write(0x5D, 0x04);

    // Enable electrodes (Run Mode)
    if (safe_write(MPR121_ECR, 0x8F)) {
        mpr121_present = true;
    }
}

void MPR121Input::process() {
    if (!mpr121_present) return;

    uint8_t reg = MPR121_TOUCH_STATUS_LSB;
    uint8_t buf[2] = {0};

    if (i2c_write_timeout_us(i2c0, MPR121_I2C_ADDR, &reg, 1, false, PROCESS_TIMEOUT_US) < 0) {
        return; 
    }

    if (i2c_read_timeout_us(i2c0, MPR121_I2C_ADDR, buf, 2, false, PROCESS_TIMEOUT_US) < 0) {
        return; 
    }

    uint16_t touched = ((uint16_t)buf[1] << 8) | buf[0];
    if (touched == 0) return;

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
