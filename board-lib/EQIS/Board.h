#pragma once

#include <Arduino.h>
#ifndef ESP32
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#endif

#include "LSM6DSO.h"
#include "Resampler.h"
#include "SSD1306Display.h"

// グループ化するサンプル数 (* 10ms = 計測震度の計算間隔)
#define INTENSITY_PROCESS_SAMPLE_GROUP_SIZE 20

#include "IntensityProcessor.h"

// main.cpp のシリアルコマンドで使用する ADC ステップあたりの gal
#define SEISMOMETER_ADC_STEP (lsm6dso.getSensitivity())

void printNmea(const char *format, ...);
void printErrorNmea(const char *id);
void serialCommandTask(void *pvParameters);

IntensityProcessor *processor;
QueueHandle_t displayIntensityQueue;

auto lsm6dso = LSM6DSO();
static void processRawSample(const float (&raw)[3]) {
    static float offset[3];
    static bool hasOffset = false;
    float sample[3];

    printNmea("XSRAW,%ld,%ld,%ld", lroundf(raw[0]), lroundf(raw[1]), lroundf(raw[2]));
    if (!hasOffset) {
        memcpy(offset, raw, sizeof(offset));
        hasOffset = true;
    }
    for (auto i = 0; i < 3; i++)
        sample[i] = (offset[i] - raw[i]) * lsm6dso.getSensitivity();

    processor->process(sample);
}

void measureTask(void *pvParameters) {
    auto xLastWakeTime = xTaskGetTickCount();
    LSM6DSOFifoSample fifo[16];
    float resampled[4][3];
    SensorClock clock;
    // センサーは内蔵発振器で動くため、マイコンの時計で正確な 100Hz に補間し直す
    Resampler resampler(10000);
    lsm6dso.begin();

    while (1) {
        // FIFO に貯まっている分を 10ms ごとに読み出す
        xTaskDelayUntil(&xLastWakeTime, configTICK_RATE_HZ / 100);
        if (clock.isUpdateDue(micros()))
            clock.update(lsm6dso.readTimestamp(), micros());

        bool dropped = false;
        int count;
        do {
            count = lsm6dso.readFifo(fifo, sizeof(fifo) / sizeof(fifo[0]), &dropped);
            for (int i = 0; i < count; i++) {
                int outputs = resampler.push(fifo[i].timestamp, fifo[i].raw, clock, resampled, sizeof(resampled) / sizeof(resampled[0]), &dropped);
                for (int j = 0; j < outputs; j++)
                    processRawSample(resampled[j]);
            }
        } while (count == sizeof(fifo) / sizeof(fifo[0]));
        if (dropped)
            printErrorNmea("MEASURE_DROPPED");
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
        if (millis() - maxIntensityAt > (ulong)displayConfig.resetMinutes * 60UL * 1000UL || maxIntensity <= latestIntensity) {
            maxIntensity = latestIntensity;
            maxIntensityAt = millis();
        }

        bool showCurrent = latestIntensity >= (JmaIntensity)displayConfig.currentThreshold;
        bool showMax = maxIntensity > JMA_INT_0 && maxIntensity >= (JmaIntensity)displayConfig.maxThreshold;
        // 常時表示をすると OLED の寿命が溶けるので動きがない場合は消灯させる
        bool hideDisplay = processor->calcStdDev() <= 0.05;
        bool effectiveHide = hideDisplay || !showCurrent;

        display.displayIntensity(
            latestIntensity, rawInt,
            effectiveHide,
            maxIntensity,
            showMax && (effectiveHide || maxIntensity != latestIntensity)
        );
    }
}


void setup() {
    Serial.begin(115200);
    loadDisplayConfig();

    displayIntensityQueue = xQueueCreate(1, sizeof(float));
    processor = new IntensityProcessor([](float sample[3]) {
        printNmea("XSACC,%.3f,%.3f,%.3f", sample[0], sample[1], sample[2]);
    }, [](float rawInt) {
        printNmea("XSINT,%.3f,%.2f", -1.0, processor->getIsStable() ? rawInt : NAN);
        xQueueOverwrite(displayIntensityQueue, &rawInt);
    });

#ifdef ESP32
    xTaskCreatePinnedToCore(measureTask, "Measure", 6144, NULL, 10, NULL, 0x01);
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
