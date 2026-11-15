#ifndef FL_PHONE_H
#define FL_PHONE_H
#include "fl_backend.h"
#include <stdint.h>

/* Public Desktop application definition; never a Google account credential. */
typedef struct { char client_id[256],client_secret[256],root_id[FL_ID]; } FlPhoneProfile;
typedef struct {
    FlProcess worker;
    FlConfig config,staged;
    FlPhoneProfile profile;
    char workspace[FL_PATH],url[192];
    char *old_credentials,*old_settings;
    bool had_credentials,had_settings,active,cancelled,done;
    unsigned phase;
    double deadline;
    FlError result;
} FlPhone;

int fl_phone_profile(const FlConfig *config,FlPhoneProfile *profile,FlError *error);
int fl_phone_url(const char *output,char url[192]);
int fl_phone_start(FlPhone *phone,const FlConfig *config,FlError *error);
/* Shared account transaction used by HTTPS pairing too. No network listener. */
int fl_account_stage(FlPhone *account,const FlConfig *config,const FlPhoneProfile *profile,FlError *error);
int fl_account_commit(FlPhone *account,const char *token_url,FlError *error);
/* 0 pending; 1 installed; -1 failure. Only sanitized error messages leave worker. */
int fl_phone_poll(FlPhone *phone,FlError *error);
void fl_phone_cancel(FlPhone *phone);
void fl_phone_destroy(FlPhone *phone);
/* After taking the app lock: remove only known files in owned staging dirs. */
void fl_phone_recover(const FlConfig *config);

#define FL_QR_BYTES 512
int fl_phone_qr(const char *text,uint8_t qr[FL_QR_BYTES]);
#endif
