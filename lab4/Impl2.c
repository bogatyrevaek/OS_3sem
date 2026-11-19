#include "contracts.h"
#include <math.h>
#include <stdlib.h>

float sin_integral(float a, float b, float e)
{
    if (e <= 0.0f || a == b) return 0.0f;
    double sign = 1.0;
    if (a > b) { float t = a; a = b; b = t; sign = -1.0; }

    double sum = 0.0, x = a, step = e;
    while (x < b) {
        double h = (x + step > b) ? (b - x) : step;
        sum += (sin(x) + sin(x + h)) * 0.5 * h;
        x += h;
    }
    return (float)(sign * sum);
}

int prime_count(int a, int b)
{
    if (a < 2) a = 2;
    if (b < a) return 0;

    char *composite = calloc((size_t)b + 1, 1);
    if (!composite) return -1;

    for (long long i = 2; i * i <= b; i++)
        if (!composite[i])
            for (long long j = i * i; j <= b; j += i)
                composite[j] = 1;

    int cnt = 0;
    for (int n = a; n <= b; n++)
        if (!composite[n]) cnt++;

    free(composite);
    return cnt;
}