#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

static void put_str(int fd, const char *s) {
    size_t len = strlen(s);
    size_t off = 0;
    while (off < len) {
        ssize_t n = write(fd, s + off, len - off);
        if (n <= 0) return;
        off += (size_t)n;
    }
}

static ssize_t read_line(int fd, char *buf, size_t maxlen) {
    size_t i = 0;
    while (i < maxlen - 1) {
        char c;
        ssize_t n = read(fd, &c, 1);
        if (n < 0) {
            if (i == 0) return -1;
            break;
        }
        if (n == 0) {
            if (i == 0) return 0;
            break;
        }
        buf[i++] = c;
        if (c == '\n') break;
    }
    buf[i] = '\0';
    return (ssize_t)i;
}

static void put_int(int fd, int v) {
    char buf[16];
    int pos = 0;
    if (v == 0) {
        buf[pos++] = '0';
    } else {
        if (v < 0) {
            buf[pos++] = '-';
            v = -v;
        }
        char tmp[16];
        int t = 0;
        while (v > 0) {
            tmp[t++] = (char)('0' + (v % 10));
            v /= 10;
        }
        while (t > 0) {
            buf[pos++] = tmp[--t];
        }
    }
    write(fd, buf, (size_t)pos);
}

int main(void) {
    char fileName[1024];
    put_str(STDOUT_FILENO, "Enter file name: ");
    ssize_t rl = read_line(STDIN_FILENO, fileName, sizeof(fileName));
    if (rl <= 0) {
        put_str(STDERR_FILENO, "Error: cannot read file name\n");
        return 1;
    }
    size_t len = strlen(fileName);
    if (len > 0 && fileName[len - 1] == '\n') fileName[len - 1] = '\0';
    if (strlen(fileName) == 0) {
        put_str(STDERR_FILENO, "Error: empty file name\n");
        return 1;
    }

    int pipe1[2];
    int pipe2[2];

    if (pipe(pipe1) == -1) {
        put_str(STDERR_FILENO, "Error: pipe1 creation failed\n");
        return 1;
    }
    if (pipe(pipe2) == -1) {
        put_str(STDERR_FILENO, "Error: pipe2 creation failed\n");
        close(pipe1[0]); close(pipe1[1]);
        return 1;
    }

    pid_t pid = fork();
    if (pid == -1) {
        put_str(STDERR_FILENO, "Error: fork failed\n");
        close(pipe1[0]); close(pipe1[1]);
        close(pipe2[0]); close(pipe2[1]);
        return 1;
    }

    if (pid == 0) {
        close(pipe1[1]);
        close(pipe2[0]);
        if (dup2(pipe1[0], STDIN_FILENO) == -1) {
            put_str(STDERR_FILENO, "Error: dup2(stdin) failed\n");
            _exit(1);
        }
        close(pipe1[0]);

        if (dup2(pipe2[1], STDOUT_FILENO) == -1) {
            put_str(STDERR_FILENO, "Error: dup2(stdout) failed\n");
            _exit(1);
        }
        close(pipe2[1]);

        char *argv[] = { (char*)"./child", fileName, NULL };
        char *envp[] = { NULL };
        execve("./child", argv, envp);

        put_str(STDERR_FILENO, "Error: execve failed\n");
        _exit(1);
    }

    close(pipe1[0]);
    close(pipe2[1]);

    put_str(STDOUT_FILENO,
        "Enter numbers (e.g. '1.5 2.3 3.7'), empty line to exit:\n");

    char line[4096];
    while (1) {
        put_str(STDOUT_FILENO, "> ");

        ssize_t n = read_line(STDIN_FILENO, line, sizeof(line));
        if (n <= 0) break;
        int isEmpty = 1;
        for (ssize_t i = 0; i < n; i++) {
            if (line[i] != ' ' && line[i] != '\t' && line[i] != '\n') {
                isEmpty = 0;
                break;
            }
        }
        if (isEmpty) break;
        ssize_t written = 0;
        while (written < n) {
            ssize_t w = write(pipe1[1], line + written, (size_t)(n - written));
            if (w <= 0) {
                put_str(STDERR_FILENO, "Error: write to pipe1 failed\n");
                close(pipe1[1]);
                close(pipe2[0]);
                waitpid(pid, NULL, 0);
                return 1;
            }
            written += w;
        }
    }
    close(pipe1[1]);

    char buf[4096];
    ssize_t n;
    while ((n = read(pipe2[0], buf, sizeof(buf))) > 0) {
        write(STDOUT_FILENO, buf, (size_t)n);
    }
    close(pipe2[0]);

    int status;
    if (waitpid(pid, &status, 0) == -1) {
        put_str(STDERR_FILENO, "Error: waitpid failed\n");
        return 1;
    }

    if (WIFEXITED(status)) {
        put_str(STDOUT_FILENO, "Child exited with code ");
        put_int(STDOUT_FILENO, WEXITSTATUS(status));
        put_str(STDOUT_FILENO, "\n");
    } else if (WIFSIGNALED(status)) {
        put_str(STDOUT_FILENO, "Child killed by signal ");
        put_int(STDOUT_FILENO, WTERMSIG(status));
        put_str(STDOUT_FILENO, "\n");
    }

    return 0;
}
