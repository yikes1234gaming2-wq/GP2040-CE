#include "addons/mpr121.h"
#include "storagemanager.h"
#include "gamepad.h"
#include "hardware/gpio.h"
#include "pico/time.h"

#define SDA_PIN 0
#define SCL_PIN 1

#define MPR121_TOUCH_STATUS_LSB 0x00
#define MPR121_ECR              0x5E

static bool mpr121_active = false;

// --- Safe Software I2C Driver ---

static inline void i2c_delay() {
    sleep_us(5); // ~100kHz equivalent speed
}

static void bb_scl_hi() {
    gpio_set_dir(SCL_PIN, GPIO_IN); // High impedance (let pull-up pull high)
}

static void bb_scl_lo() {
    gpio_set_dir(SCL_PIN, GPIO_OUT);
    gpio_put(SCL_PIN, 0);
}

static void bb_sda_hi() {
    gpio_set_dir(SDA_PIN, GPIO_IN); // High impedance
}

static void bb_sda_lo() {
    gpio_set_dir(SDA_PIN, GPIO_OUT);
    gpio_put(SDA_PIN, 0);
}

static void bb_start() {
    bb_sda_hi();
    bb_scl_hi();
    i2c_delay();
    bb_sda_lo();
    i2c_delay();
    bb_scl_lo();
    i2c_delay();
}

static void bb_stop() {
    bb_sda_lo();
    i2c_delay();
    bb_scl_hi();
    i2c_delay();
    bb_sda_hi();
    i2c_delay();
}

static bool bb_write_byte(uint8_t byte) {
    for (int i = 7; i >= 0; i--) {
        if ((byte >> i) & 1) {
            bb_sda_hi();
        } else {
            bb_sda_lo();
        }
        i2c_delay();
        bb_scl_hi();
        i2c_delay();
        bb_scl_lo();
    }

    // Read ACK with timeout guard
    bb_sda_hi();
    i2c_delay();
    bb_scl_hi();
    i2c_delay();

    bool ack = (gpio_get(SDA_PIN) == 0);

    bb_scl_lo();
    i2c_delay();
    return ack;
}

static uint8_t bb_read_byte(bool ack) {
    uint8_t byte = 0;
    bb_sda_hi();

    for (int i = 7; i >= 0; i--) {
        bb_scl_hi();
        i2c_delay();
        byte |= (gpio_get(SDA_PIN) << i);
        bb_scl_lo();
        i2c_delay();
    }

    // Send ACK/NACK
    if (ack) {
        bb_sda_lo();
    } else {
        bb_sda_hi();
    }
    i2c_delay();
    bb_scl_hi();
    i2c_delay();
    bb_scl_lo();
    bb_sda_hi();
    i2c_delay();

    return byte;
}

static bool bb_write_reg(uint8_t reg, uint8_t val) {
    bb_start();
    if (!bb_write_byte((MPR121_I2C_ADDR << 1) | 0)) { bb_stop(); return false; }
    if (!bb_write_byte(reg))                       { bb_stop(); return false; }
    if (!bb_write_byte(val))                       { bb_stop(); return false; }
    bb_stop();
    return true;
}

static bool bb_read_bytes(uint8_t reg, uint8_t *dest, uint8_t count) {
    bb_start();
    if (!bb_write_byte((MPR121_I2C_ADDR << 1) | 0)) { bb_stop(); return false; }
    if (!bb_write_byte(reg))                       { bb_stop(); return false; }

    bb_start(); // Repeated START
    if (!bb_write_byte((MPR121_I2C_ADDR << 1) | 1)) { bb_stop(); return false; }

    for (uint8_t i = 0; i < count; i++) {
        dest[i] = bb_read_byte(i < (count - 1)); // ACK all except last byte
    }
    bb_stop();
    return true;
}

// --- Addon Core ---

void MPR121Input::setup() {
    mpr121_active = false;

    // Initialize pins as standard GPIOs with internal pull-ups
    gpio_init(SDA_PIN);
    gpio_init(SCL_PIN);
    gpio_set_dir(SDA_PIN, GPIO_IN);
    gpio_set_dir(SCL_PIN, GPIO_IN);
    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);

    sleep_ms(50); // Power stabilization

    // Check if lines are dead-shorted to GND
    if (!gpio_get(SDA_PIN) || !gpio_get(SCL_PIN)) {
        return; // Line is held ground permanently, abort safely!
    }

    // Probe: Soft Reset MPR121
    if (!bb_write_reg(0x80, 0x63)) {
        return; // Device didn't respond at 0x5A! Bails cleanly so Pico boots.
    }

    sleep_ms(10);

    // Stop Mode before register setup
    bb_write_reg(MPR121_ECR, 0x00);

    // Set touch (12) / release (6) thresholds for all 12 electrodes
    for (int i = 0; i < 12; i++) {
        bb_write_reg((uint8_t)(0x41 + (i * 2)), 12);
        bb_write_reg((uint8_t)(0x41 + (i * 2) + 1), 6);
    }

    // Default baseline filtering
    bb_write_reg(0x5D, 0x04);

    // Run Mode (Enable all 12 electrodes)
    if (bb_write_reg(MPR121_ECR, 0x8F)) {
        mpr121_active = true;
    }
}

void MPR121Input::process() {
    if (!mpr121_active) return; // Completely skips if setup failed!

    uint8_t buf[2] = {0};

    if (!bb_read_bytes(MPR121_TOUCH_STATUS_LSB, buf, 2)) {
        return; // Skip cycle if read failed
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
    if (touched & (1 << 3))  gamepad->state.buttons |= GAMEPAD_MASK_S1; // ELE3
    if (touched & (1 << 7))  gamepad->state.buttons |= GAMEPAD_MASK_S2; // ELE7
    if (touched & (1 << 8))  gamepad->state.buttons |= GAMEPAD_MASK_R2; // ELE8
}
