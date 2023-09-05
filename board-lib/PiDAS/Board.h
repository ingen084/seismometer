#pragma once

#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include <ArduinoSort.h>

#include "IntensityFilter.h"
#include "MCP3204.h"
#include "LED.h"

#define ADJUST_PIN D16

QueueHandle_t processQueue;
QueueHandle_t displayIntensityQueue;

void printNmea(const char *format, ...);

// グループ化するサンプル数 (= 計測震度の計算間隔)
const int sampleGroupSize = 20;
// サンプルのグループ数(1分)
const int sortedGroupCount = 6000 / sampleGroupSize;

// float hpFilteredSample[sampleGroupSize];
float compositeSample[sampleGroupSize];
float sortedGroups[sortedGroupCount][sampleGroupSize];
int sortedGroupIndice[sortedGroupCount];

ushort sampleIndex = 0;
ushort sortedGroupIndex = 0;

void MeasureTask(void *pvParameters) {
    auto xLastWakeTime = xTaskGetTickCount();
    bool isOffsetted = false;
    uint16_t offsetRawData[3];
    uint16_t rawData[3];
    auto mcp3204 = MCP3204(SPISettings(115200, MSBFIRST, SPI_MODE0), D21);
    mcp3204.begin();

    float sample[3];
    IntensityFilter filter;

    while (1) {
        mcp3204.read(rawData);
        xQueueSend(processQueue, rawData, 0);

        // MEMO: 震度計算しない場合はここで終了

        // 収束の高速化のために初回の値をオフセット値として保存する
        if (!isOffsetted) {
            memcpy(offsetRawData, rawData, sizeof(uint16_t) * 3);
            isOffsetted = true;
        }

        // ADCの返り値を gal に変換する
        // (測定値) / ADCのレンジ / フルレンジの加速度 * 1G
        for (auto i = 0; i < 3; i++)
            sample[i] = ((float)offsetRawData[i] - rawData[i]) / (4095.0f / 5.0f) * 980.665f;

        filter.filterHP(sample);
        //hpFilteredSample[sampleIndex] = sample[0];

        printNmea("XSACC,%.3f,%.3f,%.3f", sample[0], sample[1], sample[2]);

        filter.filterForShindo(sample);

        // 3軸合成
        compositeSample[sampleIndex] = sqrt(
            sample[0] * sample[0] +
            sample[1] * sample[1] +
            sample[2] * sample[2]);

        // 1グループ分貯まったら配列にコピーして計測震度を出す
        if (++sampleIndex >= sampleGroupSize) {
            auto us = micros();

            // 計測震度を計算させる
            memcpy(sortedGroups[sortedGroupIndex], compositeSample, sizeof(float) * sampleGroupSize);
            sortArray(sortedGroups[sortedGroupIndex], sampleGroupSize);

            // 60個の配列を比較しながら 下位30番目(0.3秒)の値を取り出す
            memset(sortedGroupIndice, 0, sizeof(int) * sortedGroupCount);
            float lastMaxValue = 0;
            for (auto i = 0; i < 30; i++) {
                float max = 0;
                int maxIndex = 0;
                for (auto g = 0; g < sortedGroupCount; g++) {
                    if (max < sortedGroups[g][sampleGroupSize - (sortedGroupIndice[g] + 1)]) {
                        max = sortedGroups[g][sampleGroupSize - (sortedGroupIndice[g] + 1)];
                        maxIndex = g;
                    }
                }
                lastMaxValue = max;
                // 参照済みのインデックスを加算
                sortedGroupIndice[maxIndex]++;
            }
            // Serial.printf("us: %d v: %f\n", micros() - us, lastMaxValue);

            if (lastMaxValue > 0)
            {
                // 小数第3位を四捨五入して小数第2位を切り捨てる
                auto rawInt = floor(round((2.0f * log10(lastMaxValue) + 0.94f) * 100.0f) / 10.0f) / 10.0f;
                printNmea("XSINT,%.3f,%.2f", lastMaxValue, rawInt);
                xQueueSend(displayIntensityQueue, &rawInt, 0);
            }

            // 元になるカウントをリセット
            sampleIndex = 0;
            if (++sortedGroupIndex >= 60)
                sortedGroupIndex = 0;
        }


        // 100Hz で動かす
        if (!xTaskDelayUntil(&xLastWakeTime, configTICK_RATE_HZ / 100)) {
            Serial.println("Measure task dropped");
        }
    }
}

float rawIntHistory[sortedGroupCount];

void LedDisplayTask(void *pvParameters) {
    JmaIntensity maxIntensity = JMA_INT_0;
    ulong maxIntensityAt = millis();
    float rawInt;

    Led led;
    led.begin();
    led.wakeup();
    
    ushort frame = 0;

    bool isStable = false;
    int stabilityCheckCount = 0;

    while (1) {
        xQueueReceive(displayIntensityQueue, &rawInt, portMAX_DELAY);

        if (!isStable) {
            led.stabilityAnimate();
            rawIntHistory[stabilityCheckCount++ % sortedGroupCount] = rawInt;

            // 平均を求める
            auto sum = 0.0f;
            for (auto i = 0; i < sortedGroupCount; i++)
                sum += rawIntHistory[i];
            auto average = sum / sortedGroupCount;

            // 分散を求める
            sum = 0;
            for (auto i = 0; i < sortedGroupCount; i++)
                sum += (rawIntHistory[i] - average) * (rawIntHistory[i] - average);
            auto variance = sum / sortedGroupCount;

            // 標準偏差を求める
            auto standardDeviation = sqrt(variance);

            // Serial.printf("cnt: %d, standardDeviation: %f\n", stabilityCheckCount, standardDeviation);
            // 1分以上経過しており標準偏差 0.05 未満なら安定とみなす
            if (stabilityCheckCount >= sortedGroupCount && standardDeviation < 0.05f) {
                isStable = true;
            }
            continue;
        }

        // adjust ボタンが押されたら安定状態を解除する
        if (digitalRead(ADJUST_PIN)) {
            isStable = false;
            stabilityCheckCount = 0;
            maxIntensity = JMA_INT_0;
            continue;
        }

        auto latestIntensity = getJmaIntensity(rawInt);
        if (millis() - maxIntensityAt > 60 * 10 * 1000 || maxIntensity <= latestIntensity) {
            maxIntensity = latestIntensity;
            maxIntensityAt = millis();
        }

        led.blinkScale(latestIntensity, maxIntensity);
        if (frame++ % 2 == 0 && latestIntensity < maxIntensity)
            led.toggle(maxIntensity);
    }
}

// void SerialTask(void *pvParameters) {
//     uint16_t rawData[3];
//     auto value = MsgPack::arr_t<uint16_t>(3);
//     while (1) {
//         xQueueReceive(processQueue, &rawData, portMAX_DELAY);
//         value[0] = rawData[0];
//         value[1] = rawData[1];
//         value[2] = rawData[2];
//     }
// }

void boardSetup() {
    pinMode(ADJUST_PIN, INPUT_PULLDOWN);

    processQueue = xQueueCreate(10, sizeof(uint16_t) * 3);
    displayIntensityQueue = xQueueCreate(1, sizeof(float));
    xTaskCreateAffinitySet(MeasureTask, "Measure", 512, NULL, 10, 0x01, NULL);
    xTaskCreateAffinitySet(LedDisplayTask, "LedDisplay", 512, NULL, 4, 0x02, NULL);
    // xTaskCreateAffinitySet(SerialTask, "Serial", 512, NULL, 2, 0x03, NULL);
}

// メモ: $はつけない
void printNmea(const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    char temp[64];
    char *buffer = temp;
    size_t len = vsnprintf(temp, sizeof(temp), format, arg);
    va_end(arg);
    if (len > sizeof(temp) - 1)
    {
        buffer = new char[len + 1];
        if (!buffer)
            return;
        va_start(arg, format);
        vsnprintf(buffer, len + 1, format, arg);
        va_end(arg);
    }
    byte checkSum = 0;
    for (int i = 0; buffer[i]; i++)
        checkSum ^= (byte)buffer[i];
    Serial.printf("$%s*%02X\r\n", buffer, checkSum);
    if (buffer != temp)
    {
        delete[] buffer;
    }
    Serial.flush();
}
