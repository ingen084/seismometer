#pragma once

#include <Arduino.h>

#include <ArduinoSort.h>

#include "IntensityFilter.h"


#ifndef INTENSITY_PROCESS_SAMPLE_GROUP_SIZE
// グループ化するサンプル数 (= 計測震度の計算間隔)
#define INTENSITY_PROCESS_SAMPLE_GROUP_SIZE 100
#endif

// サンプルのグループ数(1分)
#define PROCESS_SAMPLE_GROUP_COUNT (6000 / INTENSITY_PROCESS_SAMPLE_GROUP_SIZE)

class IntensityProcessor {
    void (*highpassFilteredCallback)(float[3]);
    void (*intensityProcessedCallback)(float);

    // float hpFilteredSample[INTENSITY_PROCESS_SAMPLE_GROUP_SIZE];
    float compositeSample[INTENSITY_PROCESS_SAMPLE_GROUP_SIZE];
    float sortedGroups[PROCESS_SAMPLE_GROUP_COUNT][INTENSITY_PROCESS_SAMPLE_GROUP_SIZE];
    char sortedGroupIndice[PROCESS_SAMPLE_GROUP_COUNT];
    
    ushort sampleIndex = 0;
    ushort sortedGroupIndex = 0;

    IntensityFilter filter;

    float rawIntHistory[PROCESS_SAMPLE_GROUP_COUNT];
    unsigned int rawIntIndex = 0;
    unsigned int stabilityCheckCount = 0;
    bool isStable = false;
public:
    IntensityProcessor(void (*highpassFilteredCallback)(float[3]), void (*intensityProcessedCallback)(float)) {
        this->highpassFilteredCallback = highpassFilteredCallback;
        this->intensityProcessedCallback = intensityProcessedCallback;
    }

    bool getIsStable() {
        return isStable;
    }
    void setToUnstable() {
        isStable = false;
        stabilityCheckCount = 0;
    }

    float calcStdDev() {
        // 平均を求める
        auto sum = 0.0f;
        for (auto i = 0; i < PROCESS_SAMPLE_GROUP_COUNT; i++)
            sum += rawIntHistory[i];
        auto average = sum / PROCESS_SAMPLE_GROUP_COUNT;

        // 分散を求める
        sum = 0;
        for (auto i = 0; i < PROCESS_SAMPLE_GROUP_COUNT; i++)
            sum += (rawIntHistory[i] - average) * (rawIntHistory[i] - average);
        auto variance = sum / PROCESS_SAMPLE_GROUP_COUNT;

        // 標準偏差を求める
        return sqrt(variance);
    }

    void process(float (&sample)[3]) {            
        filter.filterHP(sample);
        if (highpassFilteredCallback != NULL)
            highpassFilteredCallback(sample);
        filter.filterForShindo(sample);

        // 3軸合成
        compositeSample[sampleIndex] = sqrt(
            sample[0] * sample[0] +
            sample[1] * sample[1] +
            sample[2] * sample[2]);

        sampleIndex++;
        // 1グループ分貯まったら配列にコピーして計測震度を出す
        if (sampleIndex < INTENSITY_PROCESS_SAMPLE_GROUP_SIZE)
            return;

        auto us = micros();

        // 計測震度を計算させる
        memcpy(sortedGroups[sortedGroupIndex], compositeSample, sizeof(compositeSample));
        sortArray(sortedGroups[sortedGroupIndex], INTENSITY_PROCESS_SAMPLE_GROUP_SIZE);

        // 比較用のインデックスを初期化
        memset(sortedGroupIndice, 0, sizeof(sortedGroupIndice));
        // グループ数の配列を比較しながら 下位30番目(0.3秒)の値を取り出す
        float lastMaxValue = 0;
        for (auto i = 0; i < 30; i++) {
            float max = 0;
            int maxIndex = 0;
            for (auto g = 0; g < PROCESS_SAMPLE_GROUP_COUNT; g++) {
                auto v = sortedGroups[g][INTENSITY_PROCESS_SAMPLE_GROUP_SIZE - (sortedGroupIndice[g] + 1)];
                if (v != NAN && max < v) {
                    max = v;
                    maxIndex = g;
                }
            }
            lastMaxValue = max;
            // 使用したインデックスを加算
            sortedGroupIndice[maxIndex]++;
        }
        // Serial.printf("us: %d v: %f\n", micros() - us, lastMaxValue);

        if (lastMaxValue > 0)
        {
            // 小数第3位を四捨五入して小数第2位を切り捨てる
            auto rawInt = floor(round((2.0f * log10(lastMaxValue) + 0.94f) * 100.0f) / 10.0f) / 10.0f;

            rawIntHistory[rawIntIndex++] = rawInt;
            if (rawIntIndex >= PROCESS_SAMPLE_GROUP_COUNT)
                rawIntIndex = 0;

            // 安定していない場合は安定しているかチェックする
            // 1分以上経過しており標準偏差 0.05 未満なら安定とみなす
            if (!isStable && stabilityCheckCount++ >= PROCESS_SAMPLE_GROUP_COUNT && this->calcStdDev() <= 0.05f) {
                isStable = true;
            }

            // 震度算出時のコールバックを呼び出し
            if (intensityProcessedCallback != NULL)
                intensityProcessedCallback(rawInt);
        }

        // 元になるカウントをリセット
        sampleIndex = 0;
        if (++sortedGroupIndex >= PROCESS_SAMPLE_GROUP_COUNT)
            sortedGroupIndex = 0;
    }
};
