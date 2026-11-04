#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "common.h"

static shared_t *shm = NULL;
static int shm_fd = -1;

static void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

static void cleanup(void) {
    if (shm) { munmap(shm, sizeof(shared_t)); shm = NULL; }
    if (shm_fd >= 0) {
        close(shm_fd);
        shm_unlink(SHM_NAME);
        shm_fd = -1;
    }
}

static void push(double v, int is_eol) {
    pthread_mutex_lock(&shm->mtx);
    while ((shm->tail + 1) % SLOT_COUNT == shm->head) {
        pthread_cond_wait(&shm->not_full, &shm->mtx);
    }
    shm->slots[shm->tail].value  = v;
    shm->slots[shm->tail].is_eol = is_eol;
    shm->tail = (shm->tail + 1) % SLOT_COUNT;
    pthread_cond_signal(&shm->not_empty);
    pthread_mutex_unlock(&shm->mtx);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <child_binary>\n", argv[0]);
        return EXIT_FAILURE;
    }

    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR | O_EXCL, 0600);
    if (shm_fd < 0) die("shm_open");
    if (ftruncate(shm_fd, sizeof(shared_t)) < 0) die("ftruncate");

    shm = mmap(NULL, sizeof(shared_t),
               PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm == MAP_FAILED) die("mmap");

    pthread_mutexattr_t ma;
    pthread_condattr_t  ca;
    pthread_mutexattr_init(&ma);
    pthread_condattr_init(&ca);
    pthread_mutexattr_setpshared(&ma, PTHREAD_PROCESS_SHARED);
    pthread_condattr_setpshared(&ca, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&shm->mtx, &ma);
    pthread_cond_init(&shm->not_empty, &ca);
    pthread_cond_init(&shm->not_full,  &ca);
    pthread_mutexattr_destroy(&ma);
    pthread_condattr_destroy(&ca);

    shm->head = shm->tail = 0;
    shm->finished_flag = 0;

    pid_t pid = fork();
    if (pid < 0) die("fork");
    if (pid == 0) {
        close(shm_fd);
        execl(argv[1], argv[1], (char *)NULL);
        perror("execl");
        _exit(127);
    }

    char *line = NULL;
    size_t cap = 0;
    ssize_t len;

    while ((len = getline(&line, &cap, stdin)) > 0) {
        char *p = line;
        while (*p) {
            while (*p == ' ' || *p == '\t' || *p == '\r') p++;
            if (*p == '\0' || *p == '\n') break;

            char *end = NULL;
            errno = 0;
            double v = strtod(p, &end);
            if (end == p) {
                fprintf(stderr, "[parent] bad token, skipping\n");
                while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
                continue;
            }
            push(v, 0);
            p = end;
        }
        push(0.0, 1);
    }
    free(line);

    pthread_mutex_lock(&shm->mtx);
    shm->finished_flag = 1;
    pthread_cond_broadcast(&shm->not_empty);
    pthread_cond_broadcast(&shm->not_full);
    pthread_mutex_unlock(&shm->mtx);

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) die("waitpid");

    if (WIFEXITED(status))
        printf("[parent] child exited with code %d\n", WEXITSTATUS(status));
    else if (WIFSIGNALED(status))
        printf("[parent] child killed by signal %d\n", WTERMSIG(status));

    pthread_mutex_destroy(&shm->mtx);
    pthread_cond_destroy(&shm->not_empty);
    pthread_cond_destroy(&shm->not_full);

    cleanup();
    return EXIT_SUCCESS;
}