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
#include <semaphore.h>
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
    sem_wait(&shm->empty);
    sem_wait(&shm->mutex);

    shm->slots[shm->tail].value  = v;
    shm->slots[shm->tail].is_eol = is_eol;
    shm->tail = (shm->tail + 1) % SLOT_COUNT;

    sem_post(&shm->mutex);
    sem_post(&shm->full);
}

static void push_final(void) {
    sem_wait(&shm->empty);
    sem_wait(&shm->mutex);

    shm->slots[shm->tail].value  = 0.0;
    shm->slots[shm->tail].is_eol = 1;
    shm->tail = (shm->tail + 1) % SLOT_COUNT;
    shm->finished_flag = 1;

    sem_post(&shm->mutex);
    sem_post(&shm->full);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <child_binary>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char fileName[1024];
    printf("Enter output file name: ");
    if (fgets(fileName, sizeof(fileName), stdin) == NULL) {
        fprintf(stderr, "Error reading file name\n");
        return EXIT_FAILURE;
    }
    size_t len = strlen(fileName);
    if (len > 0 && fileName[len - 1] == '\n') fileName[len - 1] = '\0';
    if (strlen(fileName) == 0) {
        fprintf(stderr, "Empty file name\n");
        return EXIT_FAILURE;
    }

    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR | O_EXCL, 0600);
    if (shm_fd < 0) die("shm_open");
    if (ftruncate(shm_fd, sizeof(shared_t)) < 0) die("ftruncate");

    shm = mmap(NULL, sizeof(shared_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm == MAP_FAILED) die("mmap");

    if (sem_init(&shm->mutex, 1, 1) < 0) die("sem_init mutex");
    if (sem_init(&shm->empty, 1, SLOT_COUNT) < 0) die("sem_init empty");
    if (sem_init(&shm->full,  1, 0) < 0) die("sem_init full");

    shm->head = shm->tail = 0;
    shm->finished_flag = 0;

    pid_t pid = fork();
    if (pid < 0) die("fork");
    if (pid == 0) {
        close(shm_fd);
        execl(argv[1], argv[1], fileName, (char *)NULL);
        perror("execl");
        _exit(127);
    }

    char *line = NULL;
    size_t cap = 0;
    ssize_t bytes_read;

    printf("Enter numbers (Ctrl+D to exit):\n");
    while ((bytes_read = getline(&line, &cap, stdin)) > 0) {
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

    push_final();

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) die("waitpid");

    if (WIFEXITED(status))
        printf("[parent] child exited with code %d\n", WEXITSTATUS(status));
    else if (WIFSIGNALED(status))
        printf("[parent] child killed by signal %d\n", WTERMSIG(status));

    sem_destroy(&shm->mutex);
    sem_destroy(&shm->empty);
    sem_destroy(&shm->full);

    cleanup();
    return EXIT_SUCCESS;
}
