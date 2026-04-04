#define APP_VERSION "0.2.0"
#define APP_NAME "ingen-seismometer"

#ifndef SEISMOMETER_DEVICE_NAME
#error "platformio.ini で、現在のプロファイルに対する SEISMOMETER_DEVICE_NAME を定義してください"
#endif
#ifndef  SEISMOMETER_SENSOR_NAME
#error "platformio.ini で、現在のプロファイルに対する SEISMOMETER_SENSOR_NAME を定義してください"
#endif
#ifndef SEISMOMETER_ADC_NAME
#error "platformio.ini で、現在のプロファイルに対する SEISMOMETER_ADC_NAME を定義してください"
#endif

#ifdef SEISMOMETER_HAS_DISPLAY
#include "DisplayConfig.h"
#endif

#include <Board.h>

#ifndef SEISMOMETER_ADC_STEP
#error "Board.h で SEISMOMETER_ADC_STEP を定義してください"
#endif

// メモ: $はつけない
void printNmea(const char *format, ...)
{
    // シリアルが接続されていない場合は何もしない
    if(!Serial)
        return;
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
    // len + 7 ($ + * + 2桁のチェックサム + \r\n + \0)
    char temp2[len + 7];
    sprintf(temp2, "$%s*%02X\r\n", buffer, checkSum);
    Serial.write(temp2);
    if (buffer != temp)
        delete[] buffer;
}
void printErrorNmea(const char *id)
{
    printNmea("XSEER,%s", id);
}

#ifdef SEISMOMETER_HAS_DISPLAY
void printDisplayConfig() {
    printNmea("XSCFG,DSP,%d,%d,%d", displayConfig.currentThreshold, displayConfig.maxThreshold, displayConfig.resetMinutes);
}
#endif

void serialCommandTask(void *pvParameters) {
    char buffer[32];
    char bufferIndex = 0;

    while (1) {
        // 100Hz で動かす
        vTaskDelay(configTICK_RATE_HZ / 100);

        while (Serial.available()) {
            auto c = Serial.read();
            if (c == '\r' || c == '\n') {
                buffer[bufferIndex] = '\0';
                if (strcmp(buffer, "HWINFO") == 0 || strcmp(buffer, "hwinfo") == 0)
                    printNmea("XSHWI,1,%s;%s,%s,%s,%s,%f", APP_NAME, APP_VERSION, SEISMOMETER_DEVICE_NAME, SEISMOMETER_SENSOR_NAME, SEISMOMETER_ADC_NAME, SEISMOMETER_ADC_STEP);
#ifdef SEISMOMETER_HAS_DISPLAY
                else if (strcmp(buffer, "DSPCFG") == 0 || strcmp(buffer, "dspcfg") == 0)
                    printDisplayConfig();
                else if (strncmp(buffer, "DSPCFG ", 7) == 0 || strncmp(buffer, "dspcfg ", 7) == 0) {
                    // "DSPCFG X N" で最低10文字必要 (コマンド7 + サブコマンド1 + スペース1 + 値1以上)
                    if (bufferIndex < 10 || buffer[8] != ' ') {
                        printErrorNmea("DSPCFG_INVALID");
                        bufferIndex = 0;
                        continue;
                    }
                    auto cmd = buffer[7];
                    char *endPtr;
                    auto value = strtol(buffer + 9, &endPtr, 10);
                    if (*endPtr != '\0') {
                        printErrorNmea("DSPCFG_INVALID");
                        bufferIndex = 0;
                        continue;
                    }
                    bool valid = false;
                    switch (cmd) {
                        case 'C': case 'c':
                            if (value >= 0 && value <= 9) {
                                displayConfig.currentThreshold = value;
                                valid = true;
                            }
                            break;
                        case 'M': case 'm':
                            if (value >= 0 && value <= 9) {
                                displayConfig.maxThreshold = value;
                                valid = true;
                            }
                            break;
                        case 'R': case 'r':
                            if (value >= 1 && value <= 1440) {
                                displayConfig.resetMinutes = value;
                                valid = true;
                            }
                            break;
                    }
                    if (valid) {
                        saveDisplayConfig();
                        printDisplayConfig();
                    } else {
                        printErrorNmea("DSPCFG_INVALID");
                    }
                }
#endif
                bufferIndex = 0;
                continue;
            }
            buffer[bufferIndex++] = c;
            // 32文字を超えた場合はスルー
            if (bufferIndex >= sizeof(buffer))
                bufferIndex = 0;
        }
    }
}
