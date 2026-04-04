#pragma once

#include <Arduino.h>
#include <EEPROM.h>

#include "JmaIntensity.h"

#define DISPLAY_CONFIG_MAGIC 0x53454901
#define DISPLAY_CONFIG_EEPROM_ADDR 0
#define DISPLAY_CONFIG_EEPROM_SIZE 16

struct DisplayConfig {
    uint32_t magic;
    uint8_t currentThreshold;  // JmaIntensity enum値 (0-9): 現在震度の表示閾値
    uint8_t maxThreshold;      // JmaIntensity enum値 (0-9): 最大震度の表示閾値
    uint16_t resetMinutes;     // 最大震度のリセット時間（分）
};

DisplayConfig displayConfig;

void saveDisplayConfig() {
    EEPROM.put(DISPLAY_CONFIG_EEPROM_ADDR, displayConfig);
    EEPROM.commit();
}

void loadDisplayConfig() {
    EEPROM.begin(DISPLAY_CONFIG_EEPROM_SIZE);
    EEPROM.get(DISPLAY_CONFIG_EEPROM_ADDR, displayConfig);
    if (displayConfig.magic != DISPLAY_CONFIG_MAGIC ||
        displayConfig.currentThreshold > 9 ||
        displayConfig.maxThreshold > 9 ||
        displayConfig.resetMinutes == 0 ||
        displayConfig.resetMinutes > 1440) {
        displayConfig.magic = DISPLAY_CONFIG_MAGIC;
        displayConfig.currentThreshold = JMA_INT_0;
        displayConfig.maxThreshold = JMA_INT_0;
        displayConfig.resetMinutes = 10;
        saveDisplayConfig();
    }
}
