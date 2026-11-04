#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "common.h"

static shared_t *shm = NULL;
static int shm_fd = -1;
static FILE *out_fp = NULL;

static void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

static void cleanup(void) {
    if (out_fp) { fclose(out_fp); out_fp = NULL; }
    if (shm) { munmap(shm, sizeof(shared_t)); shm = NULL; }
    if (shm_fd >= 0) { close(shm_fd); shm_fd = -1; }
}

int main(void) {
    shm_fd = shm_open(SHM_NAME, O_RDWR, 0);
    if (shm_fd < 0) die("shm_open (child)");

    shm = mmap(NULL, sizeof(shared_t),
               PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm == MAP_FAILED) die("mmap (child)");

    out_fp = fopen("result.txt", "w");
    if (!out_fp) die("fopen result.txt");

    double sum = 0.0;
    int have_numbers = 0;

    for (;;) {
        pthread_mutex_lock(&shm->mtx);

        while (shm->head == shm->tail && !shm->finished_flag) {
            pthread_cond_wait(&shm->not_empty, &shm->mtx);
        }
        if (shm->head == shm->tail && shm->finished_flag) {
            pthread_mutex_unlock(&shm->mtx);
            break;
        }

        slot_t s = shm->slots[shm->head];
        shm->head = (shm->head + 1) % SLOT_COUNT;
        pthread_cond_signal(&shm->not_full);
        pthread_mutex_unlock(&shm->mtx);

        if (s.is_eol) {
            if (have_numbers)
                fprintf(out_fp, "%.10g\n", sum);
            else
                fprintf(out_fp, "(empty line)\n");
            fflush(out_fp);
            sum = 0.0;
            have_numbers = 0;
        } else {
            sum += s.value;
            have_numbers = 1;
        }
    }

    cleanup();
    return EXIT_SUCCESS;
}