#include "signal-handler.h"
#include "definitions.h"
#include "event/event.h"
#include "logger.h"

#include <errno.h>
#include <linux/prctl.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <sys/prctl.h>

static void* handle_signals(void* param);

static pthread_t thread;

void signal_system_init(void) {
    struct sigaction action;
    sigemptyset(&action.sa_mask);
    sigaddset(&action.sa_mask, SIGINT);
    sigaddset(&action.sa_mask, SIGTERM);
    action.sa_flags   = SA_RESTART;
    action.sa_handler = SIG_IGN;

    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);

    pthread_create(&thread, NULL, &handle_signals, NULL);
    log_debug("Signal handling subsystem initialized.");
}

static void* handle_signals(void* param) {
    (void) param;

    prctl(PR_SET_NAME, "signalh");

    sigset_t sigmask;
    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGINT);
    sigaddset(&sigmask, SIGTERM);

    i32 signal  = 0;
    EventInfo e = {
        .sender = NULL,
    };

test_label:
    while ((signal = sigwaitinfo(&sigmask, NULL)) != -1) {
        log_debugf("Received signal '%s'.", strsignal(signal));
        switch (signal) {
        case SIGINT:
        case SIGTERM:
            event_trigger(BEVENT_STOP, e);
            return NULL;
        }
    }
    goto test_label;

    if (signal == -1) {
        if (errno == EINTR)
            log_fatal("Unexpected signal received, shutting down.");
        else
            log_fatalf("Signal handler failure: %s", strerror(errno));

        event_trigger(BEVENT_STOP, e);
    }

    return NULL;
}

void signal_system_cleanup(void) {
    pthread_join(thread, NULL);
    log_debug("Signal handler thread exited.");
}
