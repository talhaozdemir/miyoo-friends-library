#ifndef FL_DOWNLOAD_H
#define FL_DOWNLOAD_H
#include "fl_backend.h"

typedef struct {
    pid_t pid;
    int status_fd;
    bool active, done, cancelling;
    int64_t downloaded, total;
    double bytes_per_second;
    char target[FL_PATH], temporary[FL_PATH], warning[192];
    FlError result;
    struct timespec last_sample;
    struct timespec cancel_started;
    int64_t last_bytes;
} FlDownload;

int fl_download_start(FlDownload *download, const FlConfig *config, const FlSystem *system,
                      const FlItem *item, bool allow_overwrite, FlError *error);
/* 0 running; 1 completed; -1 failed/cancelled. Verification runs in the child. */
int fl_download_poll(FlDownload *download, FlError *error);
void fl_download_cancel(FlDownload *download);
void fl_download_destroy(FlDownload *download);
/* Caller must hold the application instance lock and have no active download. */
int fl_download_recover(const FlConfig *config, const FlSystems *systems, FlError *error);

#endif
