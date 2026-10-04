#ifndef PLATFORM_H
#define PLATFORM_H

// Portability shim: the only place that knows about OS differences.
// Provides threads, sleep, a monotonic clock, getpid and isatty-on-stdin for
// both POSIX (Linux/macOS, pthreads) and Windows (Win32 API). Everything else
// in the engine is plain C11 plus <stdatomic.h>.

#include <stdlib.h>
#include <string.h>

typedef void* (*plat_thread_fn)(void*);

#ifdef _WIN32
#include <io.h>
#include <process.h>
#include <windows.h>

#define strtok_r strtok_s
#define GETPID() _getpid()
#define IS_TTY(fd) _isatty(fd)

typedef HANDLE plat_thread;

typedef struct { plat_thread_fn fn; void* arg; } plat_start;

static inline DWORD WINAPI plat_trampoline(LPVOID p) {
    plat_start s = *(plat_start*)p;
    free(p);
    s.fn(s.arg);
    return 0;
}

// Returns 0 on success.
static inline int plat_thread_create(plat_thread* t, plat_thread_fn fn, void* arg) {
    plat_start* s = malloc(sizeof(*s));
    if (!s) return -1;
    s->fn = fn;
    s->arg = arg;
    *t = CreateThread(NULL, 0, plat_trampoline, s, 0, NULL);
    if (!*t) { free(s); return -1; }
    return 0;
}

static inline void plat_thread_join(plat_thread t) {
    WaitForSingleObject(t, INFINITE);
    CloseHandle(t);
}

static inline void plat_sleep_ms(int ms) { Sleep(ms); }

static inline double plat_now_ms(void) {
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return c.QuadPart * 1000.0 / f.QuadPart;
}

#else // POSIX
#include <pthread.h>
#include <time.h>
#include <unistd.h>

#define GETPID() getpid()
#define IS_TTY(fd) isatty(fd)

typedef pthread_t plat_thread;

// Returns 0 on success.
static inline int plat_thread_create(plat_thread* t, plat_thread_fn fn, void* arg) {
    return pthread_create(t, NULL, fn, arg);
}

static inline void plat_thread_join(plat_thread t) { pthread_join(t, NULL); }

static inline void plat_sleep_ms(int ms) { usleep(ms * 1000); }

static inline double plat_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}
#endif

#endif // PLATFORM_H
