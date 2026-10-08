#include <unistd.h>
#include <fcntl.h>
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

static int parse_float(const char **p, float *out) {
    const char *s = *p;
    while (*s == ' ' || *s == '\t') s++;

    int sign = 1;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') { s++; }

    if (*s < '0' || *s > '9') return 0;
    float val = 0.0f;
    while (*s >= '0' && *s <= '9') {
        val = val * 10.0f + (float)(*s - '0');
        s++;
    }

    if (*s == '.') {
        s++;
        float frac = 0.1f;
        while (*s >= '0' && *s <= '9') {
            val += (float)(*s - '0') * frac;
            frac *= 0.1f;
            s++;
        }
    }

    if (*s == 'e' || *s == 'E') {
        const char *save = s;
        s++;
        int esign = 1;
        if (*s == '-') { esign = -1; s++; }
        else if (*s == '+') { s++; }
        if (*s >= '0' && *s <= '9') {
            int exp = 0;
            while (*s >= '0' && *s <= '9') {
                exp = exp * 10 + (*s - '0');
                s++;
            }
            float mult = 1.0f;
            for (int i = 0; i < exp; i++) mult *= 10.0f;
            if (esign < 0) val /= mult;
            else val *= mult;
        } else {
            s = save;
        }
    }

    *out = sign * val;
    *p = s;
    return 1;
}

static int float_to_str(float v, char *out, size_t outsz) {
    size_t pos = 0;
    if (outsz < 16) return -1;

    if (v < 0) {
        out[pos++] = '-';
        v = -v;
    }

    long long scaled = (long long)((double)v * 1000000.0 + 0.5);

    long long intPart = scaled / 1000000LL;
    long long fracPart = scaled % 1000000LL;

    char tmp[32];
    int t = 0;
    if (intPart == 0) {
        tmp[t++] = '0';
    } else {
        while (intPart > 0) {
            tmp[t++] = (char)('0' + (intPart % 10));
            intPart /= 10;
        }
    }
    while (t > 0) out[pos++] = tmp[--t];

    out[pos++] = '.';

    char frac[6];
    for (int i = 5; i >= 0; i--) {
        frac[i] = (char)('0' + (fracPart % 10));
        fracPart /= 10;
    }
    for (int i = 0; i < 6; i++) out[pos++] = frac[i];

    out[pos++] = '\n';
    out[pos] = '\0';
    return (int)pos;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        put_str(STDERR_FILENO, "Error: file name argument required\n");
        return 1;
    }
    const char *fileName = argv[1];

    int fd = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        put_str(STDERR_FILENO, "Error: cannot open file for writing\n");
        return 1;
    }

    char line[4096];
    while (1) {
        ssize_t n = read_line(STDIN_FILENO, line, sizeof(line));
        if (n <= 0) break;

        const char *p = line;
        float sum = 0.0f;
        int count = 0;

        while (*p) {
            while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
            if (*p == '\0') break;

            float val;
            if (!parse_float(&p, &val)) {
                p++;
                continue;
            }
            sum += val;
            count++;
        }

        if (count == 0) continue;

        char outbuf[64];
        int m = float_to_str(sum, outbuf, sizeof(outbuf));
        if (m > 0) {
            ssize_t off = 0;
            while (off < m) {
                ssize_t w = write(fd, outbuf + off, (size_t)(m - off));
                if (w <= 0) break;
                off += w;
            }
        }
    }

    close(fd);

    put_str(STDOUT_FILENO, "Child: done, results written to file\n");
    return 0;
}