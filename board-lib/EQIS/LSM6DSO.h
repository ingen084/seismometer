#pragma once

#include <Arduino.h>
#include <LSM6DSOSensor.h>

#define LSM6DSO_CTRL10_C_TIMESTAMP_EN 0x20
#define LSM6DSO_FIFO_CTRL4_DEC_TS_BATCH_MASK 0xC0
#define LSM6DSO_FIFO_STATUS2_DIFF_FIFO_HIGH_MASK 0x03
#define LSM6DSO_FIFO_STATUS2_OVERRUN_MASK 0x48

struct LSM6DSOFifoSample {
    uint32_t timestamp;
    int16_t raw[3];
};

class LSM6DSO {
    LSM6DSOSensor *sensor;
    float sensitivity;
    uint32_t fifoTimestamp = 0;
    bool hasFifoTimestamp = false;

    uint32_t readTimestampOnce() {
        uint8_t bytes[4];
        sensor->IO_Read(bytes, LSM6DSO_TIMESTAMP0, sizeof(bytes));
        return bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
    }

public:
    void begin() {
        SPI.begin();

        this->sensor = new LSM6DSOSensor(&SPI, D3);
        this->sensor->begin();
        this->sensor->Enable_X();
        // 104Hz 設定になる。実際の周波数は内蔵発振器の個体差で数%ずれる
        this->sensor->Set_X_ODR(100);
        this->sensor->Get_X_Sensitivity(&this->sensitivity);

        // 全サンプルを内蔵タイムスタンプと組にして FIFO に貯める
        uint8_t value = 0;
        sensor->Read_Reg(LSM6DSO_CTRL10_C, &value);
        sensor->Write_Reg(LSM6DSO_CTRL10_C, value | LSM6DSO_CTRL10_C_TIMESTAMP_EN);
        sensor->Set_FIFO_X_BDR(104);
        sensor->Read_Reg(LSM6DSO_FIFO_CTRL4, &value);
        sensor->Write_Reg(LSM6DSO_FIFO_CTRL4, (value & ~LSM6DSO_FIFO_CTRL4_DEC_TS_BATCH_MASK) | (LSM6DSO_DEC_1 << 6));
        sensor->Set_FIFO_Mode(LSM6DSO_STREAM_MODE);
    }

    float getSensitivity() {
        return sensitivity * 980.665f * 0.001f;
    }

    uint32_t readTimestamp() {
        // 複数バイトの読み出し中に桁上がりすると値が崩れるため、2回読んで近い値を採る
        auto first = readTimestampOnce();
        while (true) {
            auto second = readTimestampOnce();
            if (second - first < 8)
                return second;
            first = second;
        }
    }

    // FIFO に溜まった加速度を、直前に格納されたタイムスタンプと組にして読み出す
    int readFifo(LSM6DSOFifoSample *samples, int maxSamples, bool *overrun) {
        uint8_t status[2];
        sensor->IO_Read(status, LSM6DSO_FIFO_STATUS1, sizeof(status));
        uint16_t words = status[0] | ((status[1] & LSM6DSO_FIFO_STATUS2_DIFF_FIFO_HIGH_MASK) << 8);
        if (status[1] & LSM6DSO_FIFO_STATUS2_OVERRUN_MASK)
            *overrun = true;

        int count = 0;
        for (uint16_t i = 0; i < words && count < maxSamples; i++) {
            uint8_t word[7];
            sensor->IO_Read(word, LSM6DSO_FIFO_DATA_OUT_TAG, sizeof(word));
            switch (word[0] >> 3) {
                case LSM6DSO_TIMESTAMP_TAG:
                    fifoTimestamp = word[1] | (word[2] << 8) | (word[3] << 16) | ((uint32_t)word[4] << 24);
                    hasFifoTimestamp = true;
                    break;
                case LSM6DSO_XL_NC_TAG:
                    if (!hasFifoTimestamp)
                        break;
                    samples[count].timestamp = fifoTimestamp;
                    for (int axis = 0; axis < 3; axis++)
                        samples[count].raw[axis] = (int16_t)(word[1 + axis * 2] | (word[2 + axis * 2] << 8));
                    count++;
                    break;
            }
        }
        return count;
    }
};
