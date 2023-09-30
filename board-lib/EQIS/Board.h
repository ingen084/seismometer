#pragma once

#include <Arduino.h>
#ifndef ESP32
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#endif

#include "LSM6DSO.h"
#include "SSD1306Display.h"

// グループ化するサンプル数 (* 10ms = 計測震度の計算間隔)
#define INTENSITY_PROCESS_SAMPLE_GROUP_SIZE 20

#include "IntensityProcessor.h"

// main.cpp のシリアルコマンドで使用する ADC ステップあたりの gal
#define SEISMOMETER_ADC_STEP (lsm6dso.getSensitivity())

void printNmea(const char *format, ...);
void serialCommandTask(void *pvParameters);

IntensityProcessor *processor;
QueueHandle_t displayIntensityQueue;

auto lsm6dso = LSM6DSO();
void measureTask(void *pvParameters) {
    auto xLastWakeTime = xTaskGetTickCount();
    int16_t offsetRawData[3];
    int16_t rawData[3];
    float sample[3];
    lsm6dso.begin();

    lsm6dso.read(offsetRawData);
    while (1) {
        lsm6dso.read(rawData);
        printNmea("XSRAW,%d,%d,%d", rawData[0], rawData[1], rawData[2]);

        for (auto i = 0; i < 3; i++)
            sample[i] = ((float)offsetRawData[i] - rawData[i]) * lsm6dso.getSensitivity();

        processor->process(sample);

        // 100Hz で動かす
        if (!xTaskDelayUntil(&xLastWakeTime, configTICK_RATE_HZ / 100)) {
            printNmea("XSERR,MEASURE_DROPPED");
        }
    }
}

auto display = SSD1306Display();
void oledDisplayTask(void *pvParameters) {
    auto xLastWakeTime = xTaskGetTickCount();
    float rawInt = NAN;

    JmaIntensity maxIntensity = JMA_INT_0;
    ulong maxIntensityAt = millis();

    display.begin();
    display.wakeup();
    vTaskDelay(configTICK_RATE_HZ * 3);

    while (1) {
        // 100Hz で動かす
        xTaskDelayUntil(&xLastWakeTime, configTICK_RATE_HZ / 100);

        // キューから値を取り出す
        if (xQueueReceive(displayIntensityQueue, &rawInt, 0) == pdFALSE && rawInt == NAN)
            continue;

        if (!processor->getIsStable()) {
            display.stabilityAnimate(rawInt, processor->calcStdDev());
            continue;
        }
        
        auto latestIntensity = getJmaIntensity(rawInt);
        if (millis() - maxIntensityAt > 60 * 10 * 1000 || maxIntensity <= latestIntensity) {
            maxIntensity = latestIntensity;
            maxIntensityAt = millis();
        }

        // 常時表示をすると OLED の寿命が溶けるので動きがない場合は消灯させる
        display.displayIntensity(latestIntensity, rawInt, processor->calcStdDev() <= 0.05, maxIntensity);
    }
}


void setup() {
    Serial.begin(115200);

    displayIntensityQueue = xQueueCreate(1, sizeof(float));
    processor = new IntensityProcessor([](float sample[3]) {
        printNmea("XSACC,%.3f,%.3f,%.3f", sample[0], sample[1], sample[2]);
    }, [](float rawInt) {
        printNmea("XSINT,%.3f,%.2f", -1.0, processor->getIsStable() ? rawInt : NAN);
        xQueueOverwrite(displayIntensityQueue, &rawInt);
    });

#ifdef ESP32
    xTaskCreatePinnedToCore(measureTask, "Measure", 4096, NULL, 10, NULL, 0x01);
    xTaskCreatePinnedToCore(oledDisplayTask, "OLEDDisplay", 4096, NULL, 5, NULL, 0x01);
    xTaskCreatePinnedToCore(serialCommandTask, "Serial", 4096, NULL, 5, NULL, 0x01);
#else
    xTaskCreateAffinitySet(measureTask, "Measure", 4096, NULL, 10, 0x01, NULL);
    xTaskCreateAffinitySet(oledDisplayTask, "OLEDDisplay", 2048, NULL, 5, 0x01, NULL);
    xTaskCreateAffinitySet(serialCommandTask, "Serial", 4096, NULL, 5, 0x01, NULL);
#endif

    vTaskDelete(NULL);  /* delete loopTask. */
}

void loop() {
}
