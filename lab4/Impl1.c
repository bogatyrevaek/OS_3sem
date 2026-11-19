#include "contracts.h"
#include <math.h>

float sin_integral(float a, float b, float e)
{
    if (e <= 0.0f || a == b) return 0.0f;
    double sign = 1.0;
    if (a > b) { float t = a; a = b; b = t; sign = -1.0; }

    double sum = 0.0, x = a, step = e;

    while (x < b) {
        double h = (x + step > b) ? (b - x) : step;
        sum += sin(x) * h;
        x += h;
    }
    return (float)(sign * sum);
}

static int is_prime_naive(int n)
{
    if (n < 2) return 0;

    for (int d = 2; d < n; d++)
        if (n % d == 0) return 0;
    return 1;
}

int prime_count(int a, int b)
{
    if (a < 2) a = 2;
    int cnt = 0;
    for (int n = a; n <= b; n++)
        cnt += is_prime_naive(n);
    return cnt;
}