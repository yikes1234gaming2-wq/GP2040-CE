#ifndef _MPR121_H_
#define _MPR121_H_

#include "gpaddon.h"

#ifndef MPR121_I2C_ADDR
#define MPR121_I2C_ADDR 0x5A
#endif

class MPR121Input : public GPAddon {
public:
    virtual bool available() override { return true; }
    virtual void setup() override;
    virtual void process() override;
    virtual std::string name() override { return "MPR121"; }

    virtual void preprocess() override {}
    virtual void postprocess(bool processed) override {}
    virtual void reinit() override {}
};

#endif // _MPR121_H_
