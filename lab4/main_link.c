#include "contracts.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    char line[256];
    puts("Программа №1 (линковка при компиляции)");
    puts("  1 a b e  - интеграл sin(x) на [a, b] с шагом e");
    puts("  2 a b    - количество простых на [a, b]");
    puts("  q        - выход");

    while (printf("> "), fgets(line, sizeof line, stdin)) {
        int cmd;
        if (line[0] == 'q') break;
        if (sscanf(line, "%d", &cmd) != 1) { puts("Неверная команда"); continue; }

        if (cmd == 0) {
            puts("Переключение реализаций недоступно в программе №1");
        } else if (cmd == 1) {
            float a, b, e;
            if (sscanf(line, "%*d %f %f %f", &a, &b, &e) != 3) { puts("Нужно: 1 a b e"); continue; }
            printf("sin_integral = %f\n", sin_integral(a, b, e));
        } else if (cmd == 2) {
            int a, b;
            if (sscanf(line, "%*d %d %d", &a, &b) != 2) { puts("Нужно: 2 a b"); continue; }
            printf("prime_count = %d\n", prime_count(a, b));
        } else {
            puts("Неизвестная команда");
        }
    }
    return 0;
}
