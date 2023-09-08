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

IntensityProcessor *processor;
QueueHandle_t displayIntensityQueue;

void printNmea(const char *format, ...);

void measureTask(void *pvParameters) {
    auto xLastWakeTime = xTaskGetTickCount();
    int16_t offsetRawData[3];
    int16_t rawData[3];
    float sample[3];
    auto lsm6dso = LSM6DSO();
    lsm6dso.begin();

    lsm6dso.read(offsetRawData);
    while (1) {
        lsm6dso.read(rawData);

        for (auto i = 0; i < 3; i++)
            sample[i] = ((float)offsetRawData[i] - rawData[i]) * lsm6dso.getSensitivity();

        processor->process(sample);

        // 100Hz で動かす
        if (!xTaskDelayUntil(&xLastWakeTime, configTICK_RATE_HZ / 100)) {
            Serial.println("Measure task dropped");
        }
    }
}

auto display = SSD1306Display();
void oledDisplayTask(void *pvParameters) {
    auto xLastWakeTime = xTaskGetTickCount();
    float rawInt = NAN;

    display.begin();
    display.wakeup();
    vTaskDelay(1000 / portTICK_RATE_MS);

    while (1) {
        // 100Hz で動かす
        xTaskDelayUntil(&xLastWakeTime, configTICK_RATE_HZ / 100);

        // キューから値を取り出す
        if (xQueueReceive(displayIntensityQueue, &rawInt, 0) == pdFALSE && rawInt == NAN)
            continue;

        if (!processor->getIsStable()) {
            display.stabilityAnimate();
            continue;
        }
        
        auto latestIntensity = getJmaIntensity(rawInt);
        display.updateIntensity(latestIntensity, rawInt);
    }
}

void setup() {
    Serial.begin(115200);

    displayIntensityQueue = xQueueCreate(1, sizeof(float));
    processor = new IntensityProcessor([](float sample[3]) {
        printNmea("XSACC,%.3f,%.3f,%.3f", sample[0], sample[1], sample[2]);
    }, [](float rawInt) {
        printNmea("XSINT,%.3f,%.2f", -1.0, rawInt);
        xQueueOverwrite(displayIntensityQueue, &rawInt);
    });

#ifdef ESP32
    xTaskCreatePinnedToCore(measureTask, "Measure", 2048, NULL, 10, NULL, 0x01);
    xTaskCreatePinnedToCore(oledDisplayTask, "OLEDDisplay", 1024, NULL, 5, NULL, 0x01);
#else
    xTaskCreateAffinitySet(measureTask, "Measure", 1024, NULL, 10, 0x01, NULL);
    xTaskCreateAffinitySet(oledDisplayTask, "OLEDDisplay", 1024, NULL, 5, 0x01, NULL);
#endif

    vTaskDelete(NULL);  /* delete loopTask. */
}

void loop() {
}
