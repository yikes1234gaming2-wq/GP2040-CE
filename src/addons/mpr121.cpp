#include "addons/mpr121.h"
#include "storagemanager.h"
#include "gamepad.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "pico/time.h"

#define MPR121_TOUCH_STATUS_LSB 0x00
#define MPR121_ECR              0x5E

// Use generous setup timeout (20ms), quick process timeout (1ms)
#define SETUP_TIMEOUT_US   20000 
#define PROCESS_TIMEOUT_US  1000

static bool mpr121_present = false;

void MPR121Input::setup() {
    mpr121_present = false;

    // Set up GP0 (SDA) and GP1 (SCL)
    gpio_set_function(0, GPIO_FUNC_I2C);
    gpio_set_function(1, GPIO_FUNC_I2C);
    gpio_pull_up(0);
    gpio_pull_up(1);

    // Standard 100kHz I2C speed
    i2c_init(i2c0, 100 * 1000);

    sleep_ms(50); // Power stabilization delay

    // Helper for setup writes (nostop = false to prevent I2C bus locks)
    auto safe_write = [](uint8_t reg, uint8_t val) -> bool {
        uint8_t buf[2] = { reg, val };
        int res = i2c_write_timeout_us(i2c0, MPR121_I2C_ADDR, buf, 2, false, SETUP_TIMEOUT_US);
        return res == 2;
    };

    // Probe check: Try to put MPR121 into Stop Mode (0x00 to ECR register)
    if (!safe_write(MPR121_ECR, 0x00)) {
        // Device missing or SDA/SCL lines stuck low! 
        // Bail out gracefully so the Pico and other buttons still work.
        return; 
    }

    // Soft reset
    safe_write(0x80, 0x63);
    sleep_ms(10);

    // Stop mode again before register updates
    safe_write(MPR121_ECR, 0x00);

    // Set touch/release thresholds for ELE0 through ELE11
    for (int i = 0; i < 12; i++) {
        safe_write((uint8_t)(0x41 + (i * 2)), 12);     // Touch threshold
        safe_write((uint8_t)(0x41 + (i * 2) + 1), 6);  // Release threshold
    }

    // Baseline filtering default
    safe_write(0x5D, 0x04);

    // Enter Run Mode (Enable all 12 electrodes)
    if (safe_write(MPR121_ECR, 0x8F)) {
        mpr121_present = true; // Mark device active only if final write succeeded
    }
}

void MPR121Input::process() {
    if (!mpr121_present) return; // Skip completely if setup failed to prevent locking GP2040

    uint8_t reg = MPR121_TOUCH_STATUS_LSB;
    uint8_t buf[2] = {0};

    // Send register address (nostop MUST be false to prevent I2C hardware hang on dropped ACK)
    if (i2c_write_timeout_us(i2c0, MPR121_I2C_ADDR, &reg, 1, false, PROCESS_TIMEOUT_US) < 0) {
        return; 
    }

    // Read 2 bytes of touch status
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
