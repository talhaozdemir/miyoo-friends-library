#ifndef FL_UI_H
#define FL_UI_H

#include "fl_core.h"

#define FL_UI_VISIBLE_ROWS 8

typedef enum {
    FL_KEY_NONE = 0, FL_KEY_UP, FL_KEY_DOWN, FL_KEY_LEFT, FL_KEY_RIGHT,
    FL_KEY_OPEN, FL_KEY_BACK, FL_KEY_REFRESH, FL_KEY_DISCONNECT,
    FL_KEY_EXIT, FL_KEY_PAGE_UP, FL_KEY_PAGE_DOWN
} FlKey;

/* All strings are borrowed for the duration of fl_ui_draw. UI owns no model data. */
typedef struct {
    const char *title;
    const char *subtitle;
    const char *status;
    const char *footer;
    const char *const *rows;
    const char *const *markers; /* optional per-row suffix, e.g. "INSTALLED" */
    size_t row_count, selected, scroll;
    const char *const *details;
    size_t detail_count;
    bool progress, busy, error;
    double progress_fraction; /* [0,1], negative means indeterminate */
} FlView;

typedef struct FlUi FlUi;
FlUi *fl_ui_open(FlError *error);
void fl_ui_close(FlUi *ui);
/* Display-only diagnostics, no account or library data. */
void fl_ui_describe(FlUi *ui, char *out, size_t capacity);
int fl_ui_draw(FlUi *ui, const FlView *view, FlError *error);
/* timeout_ms < 0 blocks until an event. Worker screens should use 50-100 ms. */
FlKey fl_ui_wait_event(FlUi *ui, int timeout_ms);
int fl_ui_screenshot(FlUi *ui, const char *path, FlError *error);

#endif
