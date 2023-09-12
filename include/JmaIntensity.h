#pragma once

typedef enum
{
    JMA_INT_0 = 0,
    JMA_INT_1 = 1,
    JMA_INT_2 = 2,
    JMA_INT_3 = 3,
    JMA_INT_4 = 4,
    JMA_INT_5_LOWER = 5,
    JMA_INT_5_UPPER = 6,
    JMA_INT_6_LOWER = 7,
    JMA_INT_6_UPPER = 8,
    JMA_INT_7 = 9,
} JmaIntensity;

const char* jmaIntensityToChar(JmaIntensity intensity) {
    switch (intensity)
    {
        case JMA_INT_0:
            return "0";
        case JMA_INT_1:
            return "1";
        case JMA_INT_2:
            return "2";
        case JMA_INT_3:
            return "3";
        case JMA_INT_4:
            return "4";
        case JMA_INT_5_LOWER:
            return "5-";
        case JMA_INT_5_UPPER:
            return "5+";
        case JMA_INT_6_LOWER:
            return "6-";
        case JMA_INT_6_UPPER:
            return "6+";
        case JMA_INT_7:
            return "7";
        default:
            return "?";
    }
}

JmaIntensity getJmaIntensity(float rawIntensity)
{
    if (rawIntensity >= 6.5)
    {
        return JMA_INT_7;
    }
    else if (rawIntensity >= 6.0)
    {
        return JMA_INT_6_UPPER;
    }
    else if (rawIntensity >= 5.5)
    {
        return JMA_INT_6_LOWER;
    }
    else if (rawIntensity >= 5.0)
    {
        return JMA_INT_5_UPPER;
    }
    else if (rawIntensity >= 4.5)
    {
        return JMA_INT_5_LOWER;
    }
    else if (rawIntensity >= 3.5)
    {
        return JMA_INT_4;
    }
    else if (rawIntensity >= 2.5)
    {
        return JMA_INT_3;
    }
    else if (rawIntensity >= 1.5)
    {
        return JMA_INT_2;
    }
    else if (rawIntensity >= 0.5)
    {
        return JMA_INT_1;
    }
    else
    {
        return JMA_INT_0;
    }
}
