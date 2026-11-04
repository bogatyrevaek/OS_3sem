#ifndef COMMON_H
#define COMMON_H

#include <pthread.h>

#define SHM_NAME    "/lab3_var2_shm"
#define SLOT_COUNT  32

typedef struct {
    double value;
    int    is_eol;
} slot_t;

typedef struct {
    pthread_mutex_t mtx;
    pthread_cond_t  not_empty;
    pthread_cond_t  not_full;

    int head, tail;
    int finished_flag;
    slot_t slots[SLOT_COUNT];
} shared_t;

#endif
