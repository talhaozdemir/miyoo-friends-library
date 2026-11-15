#ifndef FL_ACCOUNT_H
#define FL_ACCOUNT_H
#include "fl_backend.h"

/* Staged account transaction. No OAuth listener or SSH setup is provided. */
typedef struct { char client_id[256],client_secret[256],root_id[FL_ID]; } FlAccountProfile;
typedef struct {
    FlConfig config,staged;
    FlAccountProfile profile;
    char workspace[FL_PATH];
    char *old_credentials,*old_settings;
    bool had_credentials,had_settings,active;
} FlAccount;
int fl_account_stage(FlAccount *account,const FlConfig *config,const FlAccountProfile *profile,FlError *error);
int fl_account_commit(FlAccount *account,const char *token_url,FlError *error);
void fl_account_destroy(FlAccount *account);
/* After the app lock: clean only known staging files, including older versions. */
void fl_account_recover(const FlConfig *config);
#endif
