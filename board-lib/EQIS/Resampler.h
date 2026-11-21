#pragma once

#include <Arduino.h>

// センサー内蔵タイムスタンプの1カウントが、マイコンの時計で何μsかを推定する
class SensorClock {
    static const int HISTORY = 16;
    uint32_t ticks[HISTORY];
    uint32_t micros_[HISTORY];
    int count = 0;
    int head = 0;
    double usPerTick = 0;

public:
    // 前回の記録から1秒以上経っていれば、次の記録を受け付ける
    bool isUpdateDue(uint32_t us) const {
        return count == 0 || us - micros_[(head + HISTORY - 1) % HISTORY] >= 1000000;
    }

    // 1秒ごとに記録し、最大16秒前との差から求める
    void update(uint32_t tick, uint32_t us) {
        if (!isUpdateDue(us))
            return;
        ticks[head] = tick;
        micros_[head] = us;
        head = (head + 1) % HISTORY;
        if (count < HISTORY)
            count++;
        if (count < 2)
            return;
        auto oldest = (head + HISTORY - count) % HISTORY;
        auto newest = (head + HISTORY - 1) % HISTORY;
        usPerTick = (double)(uint32_t)(micros_[newest] - micros_[oldest]) / (uint32_t)(ticks[newest] - ticks[oldest]);
    }

    bool isReady() const {
        return count >= 2;
    }

    double getUsPerTick() const {
        return usPerTick;
    }
};

// タイムスタンプ付きのサンプルを、マイコンの時計で一定間隔の時刻に線形補間する
class Resampler {
    // これより間隔が空いたらデータが欠けたとみなし、補間せずに区切り直す
    static constexpr double GAP_US = 40000;

    double intervalUs;
    double nextTick = 0;
    double previousTick = 0;
    float previous[3];
    bool hasPrevious = false;
    uint32_t lastTimestamp = 0;
    uint64_t timestampHigh = 0;

    // 32bit のタイムスタンプは約28時間で一周するため、桁を伸ばす
    double extend(uint32_t timestamp) {
        if (hasPrevious && timestamp < lastTimestamp && lastTimestamp - timestamp > 0x80000000u)
            timestampHigh += 1ULL << 32;
        lastTimestamp = timestamp;
        return (double)(timestampHigh + timestamp);
    }

public:
    explicit Resampler(double intervalUs) : intervalUs(intervalUs) {}

    // 補間した値を書き出して個数を返す。データの欠けを検出したら gap を立てる
    int push(uint32_t timestamp, const int16_t raw[3], const SensorClock &clock, float (*out)[3], int maxOut, bool *gap) {
        if (!clock.isReady())
            return 0;
        auto tick = extend(timestamp);
        auto stepTicks = intervalUs / clock.getUsPerTick();
        int count = 0;
        if (hasPrevious) {
            auto span = tick - previousTick;
            if (span <= 0)
                return 0;
            if (span * clock.getUsPerTick() > GAP_US) {
                *gap = true;
                nextTick = tick;
            }
            while (nextTick <= tick && count < maxOut) {
                auto ratio = (nextTick - previousTick) / span;
                if (ratio < 0)
                    ratio = 0;
                for (int i = 0; i < 3; i++)
                    out[count][i] = previous[i] + (raw[i] - previous[i]) * (float)ratio;
                count++;
                nextTick += stepTicks;
            }
        } else {
            nextTick = tick;
        }
        for (int i = 0; i < 3; i++)
            previous[i] = raw[i];
        previousTick = tick;
        hasPrevious = true;
        return count;
    }
};
