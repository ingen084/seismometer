#define APP_VERSION "0.1.0"
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
