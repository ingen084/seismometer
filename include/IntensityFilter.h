#pragma once

#include <Arduino.h>
#include "IirFilter.h"

/**
 * @author François LN
 */
class IntensityFilter
{
private:
    const int fs = 100;
    const float f0 = 0.45, f1 = 7.0, f2 = 0.5, f3 = 12.0, f4 = 20.0, f5 = 30.0, h2a = 1.0, h2b = 0.75, h3 = 0.9, h4 = 0.6, h5 = 0.6, g = 1.262, pi = PI;

    float coefA[6][3];
    float coefB[6][3];
    IIRFilter filters[3][6];
    IIRFilter hpFilters[3];

    // 2次バターワースハイパスフィルター（カットオフ0.05Hz）100Hz のサンプル周波数に対してのみ有効
    const float hpb[3] = {0.997781024102941, -1.995562048205882, 0.997781024102941};
    const float hpa[3] = {1, -1.995557124345789, 0.995566972065975};

    void initFilterCoefTypeA14(float *coefA, float *coefB, float hc, float fc)
    {
        // A14
        float omega_c = 2 * pi * fc;
        coefA[0] = 12 * (fs * fs) + (12 * hc * omega_c) * fs + (omega_c * omega_c);
        coefA[1] = 10 * (omega_c * omega_c) - 24 * (fs * fs);
        coefA[2] = 12 * (fs * fs) - (12 * hc * omega_c) * fs + (omega_c * omega_c);
        coefB[0] = omega_c * omega_c;
        coefB[1] = 10 * (omega_c * omega_c);
        coefB[2] = omega_c * omega_c;
    }
    void initFilter01Coef(float *coefA, float *coefB)
    {
        // A11
        float fa1 = f0;
        float fa2 = f1;

        float omega_a1 = 2 * pi * fa1;
        float omega_a2 = 2 * pi * fa2;

        coefA[0] = 8 * (fs * fs) + (4 * omega_a1 + 2 * omega_a2) * fs + omega_a1 * omega_a2;
        coefA[1] = 2 * omega_a1 * omega_a2 - 16 * (fs * fs);
        coefA[2] = 8 * (fs * fs) - (4 * omega_a1 + 2 * omega_a2) * fs + omega_a1 * omega_a2;
        coefB[0] = 4 * (fs * fs) + 2 * omega_a2 * fs;
        coefB[1] = -8 * (fs * fs);
        coefB[2] = 4 * (fs * fs) - 2 * omega_a2 * fs;
    }
    void initFilter02Coef(float *coefA, float *coefB)
    {
        float fa3 = f1;
        // A12
        float omega_a3 = 2 * pi * fa3;
        coefA[0] = 16 * (fs * fs) + 17 * omega_a3 * fs + (omega_a3 * omega_a3);
        coefA[1] = 2 * omega_a3 * omega_a3 - 32 * (fs * fs);
        coefA[2] = 16 * (fs * fs) - 17 * omega_a3 * fs + (omega_a3 * omega_a3);
        coefB[0] = 4 * (fs * fs) + 8.5 * omega_a3 * fs + (omega_a3 * omega_a3);
        coefB[1] = 2 * omega_a3 * omega_a3 - 8 * (fs * fs);
        coefB[2] = 4 * (fs * fs) - 8.5 * omega_a3 * fs + (omega_a3 * omega_a3);
    }
    void initFilter03Coef(float *coefA, float *coefB)
    {
        float hb1 = h2a;
        float hb2 = h2b;
        float fb = f2;

        // A13
        float omega_b = 2 * pi * fb;
        coefA[0] = 12 * (fs * fs) + (12 * hb2 * omega_b) * fs + (omega_b * omega_b);
        coefA[1] = 10 * (omega_b * omega_b) - 24 * (fs * fs);
        coefA[2] = 12 * (fs * fs) - (12 * hb2 * omega_b) * fs + (omega_b * omega_b);
        coefB[0] = 12 * (fs * fs) + (12 * hb1 * omega_b) * fs + (omega_b * omega_b);
        coefB[1] = 10 * (omega_b * omega_b) - 24 * (fs * fs);
        coefB[2] = 12 * (fs * fs) - (12 * hb1 * omega_b) * fs + (omega_b * omega_b);
    }
    void initFilter04Coef(float *coefA, float *coefB)
    {

        float hc = h3;
        float fc = f3;

        initFilterCoefTypeA14(coefA, coefB, hc, fc);
    }
    void initFilter05Coef(float *coefA, float *coefB)
    {
        float hc = h4;
        float fc = f4;

        initFilterCoefTypeA14(coefA, coefB, hc, fc);
    }
    void initFilter06Coef(float *coefA, float *coefB)
    {
        float hc = h5;
        float fc = f5;

        initFilterCoefTypeA14(coefA, coefB, hc, fc);
    }

public:
    IntensityFilter()
    {
        initFilter01Coef(coefA[0], coefB[0]);
        initFilter02Coef(coefA[1], coefB[1]);
        initFilter03Coef(coefA[2], coefB[2]);
        initFilter04Coef(coefA[3], coefB[3]);
        initFilter05Coef(coefA[4], coefB[4]);
        initFilter06Coef(coefA[5], coefB[5]);

        for (int i = 0; i < 3; i++)
        {
            hpFilters[i] = IIRFilter(hpa, hpb);
            for (int j = 0; j < 6; j++)
                filters[i][j] = IIRFilter(coefA[j], coefB[j]);
        }
    }

    void reset()
    {
        for (int i = 0; i < 3; i++)
        {
            hpFilters[i].reset();
            for (int j = 0; j < 6; j++)
                filters[i][j].reset();
        }
    }

    void filterForShindo(float (&newSample)[3])
    {
        for (int i = 0; i < 3; i++)
            newSample[i] = filters[i][5].filter(
                filters[i][4].filter(
                    filters[i][3].filter(
                        filters[i][2].filter(
                            filters[i][1].filter(
                                filters[i][0].filter(newSample[i])
                            )
                        )
                    )
                )
            ) * g;
    }
    void filterHP(float (&newSample)[3])
    {
        for (int i = 0; i < 3; i++)
            newSample[i] = hpFilters[i].filter(newSample[i]);
    }
};
