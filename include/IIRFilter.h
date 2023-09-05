#pragma once

#define IIR_FILTER_LENGTH 3

class IIRFilter
{
private:
    const float *coefsA;
    const float *coefsB;
    float dlyX[IIR_FILTER_LENGTH];
    float dlyY[IIR_FILTER_LENGTH];

public:
    IIRFilter() {}
    IIRFilter(const float (&coefsA)[IIR_FILTER_LENGTH], const float (&coefsB)[IIR_FILTER_LENGTH])
    {
        this->coefsA = coefsA;
        this->coefsB = coefsB;
        reset();
    }

    void reset()
    {
        for (int i = 0; i < IIR_FILTER_LENGTH; i++)
            dlyX[i] = 0.0;
        for (int j = 0; j < IIR_FILTER_LENGTH; j++)
            dlyY[j] = 0.0;
    }

    float filter(float input)
    {
        float acc1 = 0.0;
        float acc2 = 0.0;
        /* b coeficients*/
        dlyX[0] = input;

        for (int i = 0; i < IIR_FILTER_LENGTH; i++)
            acc1 += coefsB[i] * dlyX[i];
        for (int i = (IIR_FILTER_LENGTH)-1; i > 0; i--)
            dlyX[i] = dlyX[i - 1];
        /* a coeficients*/
        for (int i = 1; i < IIR_FILTER_LENGTH; i++)
            acc1 -= coefsA[i] * dlyY[i];
        dlyY[0] = (acc1 + acc2) / coefsA[0];

        for (int i = (IIR_FILTER_LENGTH)-1; i > 0; i--)
            dlyY[i] = dlyY[i - 1];
        return dlyY[0];
    }
};
