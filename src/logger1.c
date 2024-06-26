#include "logger.h"

#ifdef MC_CPLX_LOGGER

#include "containers/bytebuffer.h"
#include "containers/ring_queue.h"
#include "containers/vector.h"
#include "memory/arena.h"
#include "platform/mc_cond_var.h"
#include "platform/mc_mutex.h"
#include "platform/mc_threads.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define LOGGER_ARENA_SIZE 1048576
#define LOGGER_MAX_ENTRY_COUNT 32
#define LOGGER_MAX_THREADS 32
#define LOGGER_MSG_MAX_SIZE 8192
#define LOGGER_PREFIX_LENGTH 7

static char* names[_LOG_LEVEL_COUNT] = {
    [LOG_LEVEL_FATAL] = "[FATAL]",
    [LOG_LEVEL_ERROR] = "[ERROR]",
    [LOG_LEVEL_WARN] = "[ WARN]",
    [LOG_LEVEL_INFO] = "[ INFO]",
#ifdef DEBUG
    [LOG_LEVEL_DEBUG] = "[DEBUG]",
#endif
#ifdef TRACE
    [LOG_LEVEL_TRACE] = "[TRACE]",
#endif
};

static char* colors[_LOG_LEVEL_COUNT] = {
    [LOG_LEVEL_FATAL] = "\x1b[91;1m",
    [LOG_LEVEL_ERROR] = "\x1b[31m",
    [LOG_LEVEL_WARN] = "\x1b[33m",
    [LOG_LEVEL_INFO] = "\x1b[0m",
#ifdef DEBUG
    [LOG_LEVEL_DEBUG] = "\x1b[32;3m",
#endif
#ifdef TRACE
    [LOG_LEVEL_TRACE] = "\x1b[30;3m",
#endif
};

typedef struct logger_ctx {
    Arena arena;

    Vector thread_buffers;

    MCThread thread;
    MCThreadKey buffer_key;
    MCCondVar cond_var;
    bool had_messages;
    bool running;
} LoggerCtx;

typedef struct log_entry {
    ByteBuffer data;
    enum LogLevel lvl;
    time_t timestamp;
} LogEntry;

static LoggerCtx ctx;

static void* logger_handle(void* param);

void logger_system_init(void) {
    ctx.arena = arena_create_silent(LOGGER_ARENA_SIZE);

    // TODO: Create a dictionnary of ring buffers

    vector_init_fixed(&ctx.thread_buffers, &ctx.arena, LOGGER_MAX_THREADS, sizeof(RingQueue));

    mcthread_create_attachment(&ctx.buffer_key);
    mcthread_create(&ctx.thread, &logger_handle, NULL);
    mcvar_create(&ctx.cond_var);

    ctx.had_messages = FALSE;
    ctx.running = TRUE;
}

static RingQueue* get_entry_queue(void) {
    time_t min_time = time(NULL);
    RingQueue* queue = NULL;
    u32 size = ctx.thread_buffers.size;
    for (u32 i = 0; i < size; i++) {
        RingQueue* q = vector_ref(&ctx.thread_buffers, i);
        const LogEntry* entry = rqueue_peek(q);
        if (!entry)
            continue;
        f64 diff = difftime(entry->timestamp, min_time);
        if (diff < 0) {
            min_time = entry->timestamp;
            queue = q;
        }
    }

    return queue;
}

static void flush_entry(RingQueue* entry_queue) {
    LogEntry entry;
    if (!rqueue_dequeue(entry_queue, &entry))
        return;

    int fd = entry.lvl <= LOG_LEVEL_ERROR ? STDERR_FILENO : STDOUT_FILENO;

    ByteBuffer scratch_buffer = bytebuf_create(LOGGER_MSG_MAX_SIZE);
    bytebuf_write(&scratch_buffer, colors[entry.lvl], strlen(colors[entry.lvl]));
    bytebuf_write(&scratch_buffer, names[entry.lvl], LOGGER_PREFIX_LENGTH);
    bytebuf_write(&scratch_buffer, " ", 1);
    bytebuf_copy(&scratch_buffer, &entry.data);
    bytebuf_write(&scratch_buffer, "\x1b[0m\n", 5);

    write(fd, scratch_buffer.buf, scratch_buffer.size);
    bytebuf_destroy(&entry.data);
    bytebuf_destroy(&scratch_buffer);
}

static void* logger_handle(void* param) {
    (void) param;
    mcthread_set_name("logger");

    MCMutex mutex;
    mcmutex_create(&mutex);
    mcmutex_lock(&mutex);

    while (ctx.running) {
        if (!ctx.had_messages)
            mcvar_wait(&ctx.cond_var, &mutex);
        while (TRUE) {
            RingQueue* queue = get_entry_queue();
            if (!queue)
                break;
            flush_entry(queue);
        }
        ctx.had_messages = FALSE;
    }
    mcmutex_unlock(&mutex);
    mcmutex_destroy(&mutex);
    return NULL;
}

void logger_system_cleanup(void) {
    ctx.running = FALSE;
    mcvar_broadcast(&ctx.cond_var);
    mcthread_join(&ctx.thread, NULL);
    arena_destroy(&ctx.arena);
    mcvar_destroy(&ctx.cond_var);
}

static void add_log_entry(enum LogLevel lvl, ByteBuffer* buffer) {
    RingQueue* q = mcthread_get_data(ctx.buffer_key);
    if (!q) {
        q = vector_reserve(&ctx.thread_buffers);
        *q = rqueue_create(LOGGER_MAX_ENTRY_COUNT, sizeof(LogEntry), &ctx.arena);
        mcthread_attach_data(ctx.buffer_key, q);
    }
    LogEntry e = {
        .data = *buffer,
        .lvl = lvl,
        .timestamp = time(NULL),
    };

    rqueue_enqueue(q, &e);
    ctx.had_messages = TRUE;
    mcvar_signal(&ctx.cond_var);
}

void _log_msg(enum LogLevel lvl, char* msg) {
    if (!ctx.running || mcthread_equals(&ctx.thread))
        return;
    if (lvl < LOG_LEVEL_FATAL || lvl >= _LOG_LEVEL_COUNT)
        return;

    ByteBuffer buffer = bytebuf_create(LOGGER_MSG_MAX_SIZE);

    bytebuf_write(&buffer, msg, strlen(msg));
    add_log_entry(lvl, &buffer);
}

void _log_msgf(enum LogLevel lvl, char* msg, ...) {
    if (!ctx.running || mcthread_equals(&ctx.thread))
        return;
    if (lvl < LOG_LEVEL_FATAL || lvl >= _LOG_LEVEL_COUNT)
        return;

    va_list args;
    va_start(args, msg);

    ByteBuffer buffer = bytebuf_create(LOGGER_MSG_MAX_SIZE);

    arena_save(&ctx.arena);

    i32 msg_len = vsnprintf(NULL, 0, msg, args);
    char* buf = arena_allocate(&ctx.arena, msg_len + 1);
    if (buf) {
        vsnprintf(buf, msg_len + 1, msg, args);
        bytebuf_write(&buffer, buf, msg_len);
    }

    arena_restore(&ctx.arena);
    va_end(args);
    if (buf)
        add_log_entry(lvl, &buffer);
}

#endif
