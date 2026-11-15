#ifndef FL_BACKEND_H
#define FL_BACKEND_H

#include "fl_core.h"
#include <sys/types.h>

/* Owned child only; no shell, no network listener. Initialize with {0}. */
typedef struct {
    pid_t pid;
    int out_fd, err_fd;
    char *out, *err;
    size_t out_len, out_cap, out_limit, err_len;
    int exit_code;
    bool running, cancelled, overflow;
    double cancelled_at;
} FlProcess;

int fl_process_start(FlProcess *process, const char *const argv[], size_t capture_limit, FlError *error);
/* 0: running, 1: exited, -1: capture/wait failure. Pump at least every 100 ms. */
int fl_process_poll(FlProcess *process, FlError *error);
void fl_process_cancel(FlProcess *process);
void fl_process_destroy(FlProcess *process);

int fl_config_load(const char *sd_root, const char *app_dir, FlConfig *config, FlError *error);
int fl_credentials_check(const FlConfig *config, FlError *error);
/* Invalidate only the short-lived access token. Preserve refresh permission and
 * all other remote settings; never launch an interactive login. */
int fl_credentials_expire_access(const FlConfig *config, FlError *error);
void fl_backend_classify_error(int exit_code, const char *stderr_text, FlError *error);

typedef struct {
    FlProcess process;
    FlConfig config;
    FlListing listing;
    char folder_id[FL_ID], cache_path[FL_PATH];
    time_t started;
    double deadline;
    bool initialized, active, done, disconnect, revoke_failed, timed_out, cache_fallback, auth_retried;
    FlError result;
} FlBackend;

/* The folder ID must be root_id or a directory ID returned by its listings. */
int fl_backend_list_start(FlBackend *backend, const FlConfig *config,
                          const char *folder_id, bool force_refresh, FlError *error);
/* 0: running, 1: done (possibly stale cache), -1: failed. */
int fl_backend_poll(FlBackend *backend, FlError *error);
int fl_backend_take_list(FlBackend *backend, FlListing *listing, FlError *error);
void fl_backend_cancel(FlBackend *backend);
void fl_backend_destroy(FlBackend *backend);
/* Local deletion always proceeds after the optional revocation attempt. */
int fl_backend_disconnect_start(FlBackend *backend, const FlConfig *config,
                                bool try_online_revoke, FlError *error);

#endif
