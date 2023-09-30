#pragma once

#include <math.h>

#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include "MCP3204.h"
#include "LED.h"

// グループ化するサンプル数 (* 10ms = 計測震度の計算間隔)
#define INTENSITY_PROCESS_SAMPLE_GROUP_SIZE 20

#include "IntensityProcessor.h"

// main.cpp のシリアルコマンドで使用する ADC ステップあたりの gal
#define SEISMOMETER_ADC_STEP (1.0f / (4095.0f / 5.0f) * 980.665f)

void printNmea(const char *format, ...);
void serialCommandTask(void *pvParameters);

#define ADJUST_PIN D16

IntensityProcessor *processor;
QueueHandle_t displayIntensityQueue;

void measureTask(void *pvParameters) {
    auto xLastWakeTime = xTaskGetTickCount();
    uint16_t offsetRawData[3];
    uint16_t rawData[3];
    auto mcp3204 = MCP3204(SPISettings(115200, MSBFIRST, SPI_MODE0), D21);
    mcp3204.begin();

    float sample[3];

    // 収束の高速化のために初回の値をオフセット値として保存する
    mcp3204.read(offsetRawData);

    while (1) {
        mcp3204.read(rawData);
        printNmea("XSRAW,%d,%d,%d", rawData[0], rawData[1], rawData[2]);

        // ADCの返り値を gal に変換する
        // (測定値) / ADCのレンジ / フルレンジの加速度 * 1G
        for (auto i = 0; i < 3; i++)
            sample[i] = ((float)offsetRawData[i] - rawData[i]) / (4095.0f / 5.0f) * 980.665f;

        processor->process(sample);

        // 100Hz で動かす
        if (!xTaskDelayUntil(&xLastWakeTime, configTICK_RATE_HZ / 100) && Serial) {
            printNmea("XSERR,MEASURE_DROPPED");
        }
    }
}

void ledDisplayTask(void *pvParameters) {
    auto xLastWakeTime = xTaskGetTickCount();

    JmaIntensity maxIntensity = JMA_INT_0;
    ulong maxIntensityAt = millis();
    float rawInt = NAN;

    ushort frame = 0;

    Led led;
    led.begin();
    led.wakeup();

    while (1) {
        // 100Hz で動かす
        xTaskDelayUntil(&xLastWakeTime, configTICK_RATE_HZ / 100);

        // キューから値を取り出す
        if (xQueueReceive(displayIntensityQueue, &rawInt, 0) == pdFALSE && rawInt == NAN)
            continue;

        if (frame >= 100)
            frame = 0;

        if (!processor->getIsStable()) {
            if (frame++ % 10 == 0)
                led.stabilityAnimate();
            continue;
        }

        // adjust ボタンが押されたら安定状態を解除する
        if (digitalRead(ADJUST_PIN)) {
            processor->setToUnstable();
            maxIntensity = JMA_INT_0;
            continue;
        }

        auto latestIntensity = getJmaIntensity(rawInt);
        if (millis() - maxIntensityAt > 60 * 10 * 1000 || maxIntensity <= latestIntensity) {
            maxIntensity = latestIntensity;
            maxIntensityAt = millis();
        }

        led.blinkScale(latestIntensity, maxIntensity);
        if (frame++ % 100 <= 50 && latestIntensity < maxIntensity)
            led.toggle(maxIntensity);
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(ADJUST_PIN, INPUT_PULLDOWN);

    displayIntensityQueue = xQueueCreate(1, sizeof(float));
    processor = new IntensityProcessor([](float sample[3]) {
        printNmea("XSACC,%.3f,%.3f,%.3f", sample[0], sample[1], sample[2]);
    }, [](float rawInt) {
        printNmea("XSINT,%.3f,%.2f", -1.0, processor->getIsStable() ? rawInt : NAN);
        xQueueOverwrite(displayIntensityQueue, &rawInt);
    });
    xTaskCreateAffinitySet(measureTask, "Measure", 1024, NULL, 10, 0x01, NULL);
    xTaskCreateAffinitySet(ledDisplayTask, "LedDisplay", 512, NULL, 4, 0x01, NULL);
    xTaskCreateAffinitySet(serialCommandTask, "Serial", 1024, NULL, 5, 0x01, NULL);

    vTaskDelete(NULL);  /* delete loopTask. */
}

void loop() {
}
