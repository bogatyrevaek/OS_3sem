#include "contracts.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
  #include <windows.h>
  typedef HMODULE lib_handle;
  #define LIB_OPEN(p)    LoadLibraryA(p)
  #define LIB_SYM(h, n)  ((void *)GetProcAddress((h), (n)))
  #define LIB_CLOSE(h)   FreeLibrary(h)
  #define DEFAULT_LIB1 "impl1.dll"
  #define DEFAULT_LIB2 "impl2.dll"
#else
  #include <dlfcn.h>
  typedef void *lib_handle;
  #define LIB_OPEN(p)    dlopen((p), RTLD_NOW)
  #define LIB_SYM(h, n)  dlsym((h), (n))
  #define LIB_CLOSE(h)   dlclose(h)
  #define DEFAULT_LIB1 "./libimpl1.so"
  #define DEFAULT_LIB2 "./libimpl2.so"
#endif

typedef float (*sin_integral_fn)(float, float, float);
typedef int   (*prime_count_fn)(int, int);

static const char *paths[2] = { DEFAULT_LIB1, DEFAULT_LIB2 };
static lib_handle lib = NULL;
static sin_integral_fn f_integral = NULL;
static prime_count_fn  f_primes = NULL;

static int load(int idx)
{
    lib_handle h = LIB_OPEN(paths[idx]);
    if (!h) { fprintf(stderr, "Не удалось загрузить %s\n", paths[idx]); return 0; }

    sin_integral_fn fi = (sin_integral_fn)LIB_SYM(h, "sin_integral");
    prime_count_fn  fp = (prime_count_fn)LIB_SYM(h, "prime_count");
    if (!fi || !fp) {
        fprintf(stderr, "В %s нет нужных функций\n", paths[idx]);
        LIB_CLOSE(h);
        return 0;
    }
    if (lib) LIB_CLOSE(lib);
    lib = h; f_integral = fi; f_primes = fp;
    return 1;
}

int main(int argc, char **argv)
{
    if (argc >= 3) { paths[0] = argv[1]; paths[1] = argv[2]; }

    int cur = 0;
    if (!load(cur)) return 1;

    char line[256];
    puts("Программа №2 (загрузка библиотек во время исполнения)");
    puts("  0        - переключить реализацию");
    puts("  1 a b e  - интеграл sin(x) на [a, b] с шагом e");
    puts("  2 a b    - количество простых на [a, b]");
    puts("  q        - выход");
    printf("Текущая реализация: №%d (%s)\n", cur + 1, paths[cur]);

    while (printf("> "), fgets(line, sizeof line, stdin)) {
        int cmd;
        if (line[0] == 'q') break;
        if (sscanf(line, "%d", &cmd) != 1) { puts("Неверная команда"); continue; }

        if (cmd == 0) {
            int next = 1 - cur;
            if (load(next)) cur = next;
            printf("Текущая реализация: №%d (%s)\n", cur + 1, paths[cur]);
        } else if (cmd == 1) {
            float a, b, e;
            if (sscanf(line, "%*d %f %f %f", &a, &b, &e) != 3) { puts("Нужно: 1 a b e"); continue; }
            printf("sin_integral = %f\n", f_integral(a, b, e));
        } else if (cmd == 2) {
            int a, b;
            if (sscanf(line, "%*d %d %d", &a, &b) != 2) { puts("Нужно: 2 a b"); continue; }
            printf("prime_count = %d\n", f_primes(a, b));
        } else {
            puts("Неизвестная команда");
        }
    }
    if (lib) LIB_CLOSE(lib);
    return 0;
}