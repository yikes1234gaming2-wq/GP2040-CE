#include "gpaddon.h"
#include "storagemanager.h"
#include "gamepad.h"
#include "hardware/i2c.h"
//Disclaimer: written fully by AI.
#ifndef MPR121_I2C_ADDR
#define MPR121_I2C_ADDR 0x5A
#endif

#define MPR121_TOUCH_STATUS_LSB 0x00
#define MPR121_ECR              0x5E
#define MPR121_SOFT_RESET       0x80

class MPR121Addon : public GPAddon {
public:
    virtual bool available() override { return true; }

    virtual void setup() override {
        // Initialize I2C0 on GP0 (SDA) and GP1 (SCL) at 400kHz
        i2c_init(i2c0, 400 * 1000);
        gpio_set_function(0, GPIO_FUNC_I2C); // GP0 = SDA
        gpio_set_function(1, GPIO_FUNC_I2C); // GP1 = SCL
        gpio_pull_up(0);
        gpio_pull_up(1);

        // Soft Reset & Configure baseline
        uint8_t reset_buf[2] = { MPR121_SOFT_RESET, 0x63 };
        i2c_write_blocking(i2c0, MPR121_I2C_ADDR, reset_buf, 2, false);

        uint8_t ecr_stop[2] = { MPR121_ECR, 0x00 };
        i2c_write_blocking(i2c0, MPR121_I2C_ADDR, ecr_stop, 2, false);

        // Set thresholds (12 = Touch, 6 = Release)
        for (int i = 0; i < 12; i++) {
            uint8_t tth[2] = { (uint8_t)(0x41 + (i * 2)), 12 };
            uint8_t rth[2] = { (uint8_t)(0x41 + (i * 2) + 1), 6 };
            i2c_write_blocking(i2c0, MPR121_I2C_ADDR, tth, 2, false);
            i2c_write_blocking(i2c0, MPR121_I2C_ADDR, rth, 2, false);
        }

        uint8_t ecr_run[2] = { MPR121_ECR, 0x8F };
        i2c_write_blocking(i2c0, MPR121_I2C_ADDR, ecr_run, 2, false);
    }

    virtual void process() override {
        uint8_t buf[2] = {0};
        uint8_t reg = MPR121_TOUCH_STATUS_LSB;
        i2c_write_blocking(i2c0, MPR121_I2C_ADDR, &reg, 1, true);
        i2c_read_blocking(i2c0, MPR121_I2C_ADDR, buf, 2, false);
        uint16_t touched = ((uint16_t)buf[1] << 8) | buf[0];

        Gamepad *gamepad = Storage::getInstance().getGamepad();

        // Directional mapping (ELE0 - ELE3)
        if (touched & (1 << 1))  gamepad->state.dpad |= GAMEPAD_MASK_UP;    // ELE0
        if (touched & (1 << 4))  gamepad->state.dpad |= GAMEPAD_MASK_DOWN;  // ELE1
        if (touched & (1 << 0))  gamepad->state.dpad |= GAMEPAD_MASK_LEFT;  // ELE2
        if (touched & (1 << 2))  gamepad->state.dpad |= GAMEPAD_MASK_RIGHT; // ELE3

        // Action buttons mapping (ELE4 - ELE11)
        if (touched & (1 << 5))  gamepad->state.buttons |= GAMEPAD_MASK_B1; // ELE4 (Square / X)
        if (touched & (1 << 6))  gamepad->state.buttons |= GAMEPAD_MASK_B2; // ELE5 (Triangle / Y)
        if (touched & (1 << 9))  gamepad->state.buttons |= GAMEPAD_MASK_R1; // ELE6 (R1 / RB)
        // if (touched & (1 << 7))  gamepad->state.buttons |= GAMEPAD_MASK_L1; // ELE7 (L1 / LB)
        if (touched & (1 << 3))  gamepad->state.buttons |= GAMEPAD_MASK_A1; // ELE8 (Cross / A)
        if (touched & (1 << 7))  gamepad->state.buttons |= GAMEPAD_MASK_A2; // ELE9 (Circle / B)
        if (touched & (1 << 8)) gamepad->state.buttons |= GAMEPAD_MASK_R2; // ELE10 (R2 / RT)
        // if (touched & (1 << 11)) gamepad->state.buttons |= GAMEPAD_MASK_L2; // ELE11 (L2 / LT)
    }

    virtual std::string name() override { return "MPR121 Touch Sensor"; }
};
