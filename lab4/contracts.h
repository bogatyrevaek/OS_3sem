#ifndef CONTRACTS_H
#define CONTRACTS_H

#if defined(_WIN32)
  #define LIB_API __declspec(dllexport)
#else
  #define LIB_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

LIB_API float sin_integral(float a, float b, float e);

LIB_API int prime_count(int a, int b);

#ifdef __cplusplus
}
#endif

#endif