#ifndef COMMON_H
#define COMMON_H

#include <semaphore.h>

#define SHM_NAME    "/lab3_var2_shm"
#define SLOT_COUNT  32

typedef struct {
    double value;
    int    is_eol;
} slot_t;

typedef struct {
    sem_t   mutex;
    sem_t   empty;
    sem_t   full;

    int     head, tail;
    int     finished_flag;
    slot_t  slots[SLOT_COUNT];
} shared_t;

#endif
