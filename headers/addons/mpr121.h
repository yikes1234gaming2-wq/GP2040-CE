#ifndef _MPR121_H_
#define _MPR121_H_

#include "gpaddon.h"
#include "GamepadEnums.h"

#ifndef MPR121_I2C_ADDR
#define MPR121_I2C_ADDR 0x5A
#endif

class MPR121Input : public GPAddon {
public:
    virtual bool available() override;
    virtual void setup() override;
    virtual void process() override;
    virtual std::string name() override { return "MPR121"; }
};

#endif // _MPR121_H_
