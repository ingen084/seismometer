#pragma once

#include <Arduino.h>

#include "JmaIntensity.h"

#define INTENSITY_LED_PIN_OFFSET D6

#define PIDAS_PLUS_LED_COUNT 10
#define PIDAS_PLUS_LED_CHECK_COUNT(i)           \
    if ((i) < 0 || (i) >= PIDAS_PLUS_LED_COUNT) \
    return

/**
 * LEDのドライバもどき
 */
class Led
{
private:
    bool ledState[PIDAS_PLUS_LED_COUNT];
    bool stabilityAnimationReverseFlag = false;
    int8_t stabilityAnimationCount = 0;

    void on(int led)
    {
        PIDAS_PLUS_LED_CHECK_COUNT(led);
        digitalWrite(INTENSITY_LED_PIN_OFFSET + led, HIGH);
        ledState[led] = true;
    }

    void off(int led)
    {
        PIDAS_PLUS_LED_CHECK_COUNT(led);
        digitalWrite(INTENSITY_LED_PIN_OFFSET + led, LOW);
        ledState[led] = false;
    }

public:
    void begin()
    {
        for (auto i = 0; i < PIDAS_PLUS_LED_COUNT; i++)
            pinMode(INTENSITY_LED_PIN_OFFSET + i, OUTPUT);
    }

    /**
     * 起動時のアニメーションを行う
     * 1秒かかる
     */
    void wakeup()
    {
        for (auto i = 0; i < PIDAS_PLUS_LED_COUNT; i++)
            on(i);
        vTaskDelay(1000);
        for (auto i = 0; i < PIDAS_PLUS_LED_COUNT; i++)
            off(i);
    }

    void stabilityAnimate()
    {
        for (auto i = 0; i < PIDAS_PLUS_LED_COUNT; i++) {
            if (i == stabilityAnimationCount)
                on(i);
            else
                off(i);
        }
        if (stabilityAnimationReverseFlag) {
            stabilityAnimationCount--;
            if (stabilityAnimationCount <= 0) {
                stabilityAnimationReverseFlag = false;
                stabilityAnimationCount = 0;
            }
        } else {
            stabilityAnimationCount++;
            if (stabilityAnimationCount >= PIDAS_PLUS_LED_COUNT) {
                stabilityAnimationReverseFlag = true;
                stabilityAnimationCount = PIDAS_PLUS_LED_COUNT - 2;
            }
        }
    }

    void blinkScale(JmaIntensity scale, JmaIntensity max)
    {
        PIDAS_PLUS_LED_CHECK_COUNT(scale);
        PIDAS_PLUS_LED_CHECK_COUNT(max);
        for (auto i = 0; i < PIDAS_PLUS_LED_COUNT; i++)
        {
            if (i <= scale || i == max)
                on(i);
            else
                off(i);
        }
    }

    void toggle(int led)
    {
        PIDAS_PLUS_LED_CHECK_COUNT(led);
        if (ledState[led])
            off(led);
        else
            on(led);
    }

    void clear()
    {
        for (auto i = 0; i < PIDAS_PLUS_LED_COUNT; i++)
            off(i);
    }
};
