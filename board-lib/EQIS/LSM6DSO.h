#pragma once

#include <Arduino.h>
#include <LSM6DSOSensor.h>

class LSM6DSO {
    LSM6DSOSensor *sensor;
    float sensitivity;

public:
    void begin() {
        SPI.begin();

        this->sensor = new LSM6DSOSensor(&SPI, D3);
        this->sensor->begin();
        this->sensor->Enable_X();
        this->sensor->Set_X_ODR(100);
        this->sensor->Get_X_Sensitivity(&this->sensitivity);
    }

    float getSensitivity() {
        return sensitivity * 980.665f * 0.001f;
    }

    void read(int16_t (&data)[3])
    {
        this->sensor->Get_X_AxesRaw(data);
    }
};
