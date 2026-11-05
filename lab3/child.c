#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
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

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Error: file name argument required\n");
        return EXIT_FAILURE;
    }

    shm_fd = shm_open(SHM_NAME, O_RDWR, 0);
    if (shm_fd < 0) die("shm_open (child)");

    shm = mmap(NULL, sizeof(shared_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm == MAP_FAILED) die("mmap (child)");

    out_fp = fopen(argv[1], "w");
    if (!out_fp) die("fopen result file");

    double sum = 0.0;
    int have_numbers = 0;

    for (;;) {
        sem_wait(&shm->full);
        sem_wait(&shm->mutex);

        slot_t s = shm->slots[shm->head];
        shm->head = (shm->head + 1) % SLOT_COUNT;

        if (s.is_eol && shm->finished_flag && (shm->head == shm->tail)) {
            sem_post(&shm->mutex);
            sem_post(&shm->empty);
            break;
        }

        sem_post(&shm->mutex);
        sem_post(&shm->empty);

        if (s.is_eol) {
            if (have_numbers)
                fprintf(out_fp, "%.10g\n", sum);
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
