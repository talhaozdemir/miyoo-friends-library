#ifndef FL_HTTPS_H
#define FL_HTTPS_H
#include "fl_account.h"
typedef struct { char origin[256],client_id[256],root_id[FL_ID]; } FlHttpsProfile;
typedef struct {
    FlAccount account;
    FlProcess worker;
    FlHttpsProfile profile;
    char curl[FL_PATH],request_path[FL_PATH],body_path[FL_PATH];
    char id[44],secret[44],url[192],code[7];
    unsigned phase;
    double deadline,next_poll;
    bool active,ready,cancelled,installed,done;
    FlError result;
} FlHttps;
int fl_https_profile(const FlConfig *config,FlHttpsProfile *profile,FlError *error);
int fl_https_start(FlHttps *https,const FlConfig *config,FlError *error);
int fl_https_poll(FlHttps *https,FlError *error);
/* Only after matching the code on phone and Miyoo. */
int fl_https_confirm(FlHttps *https,FlError *error);
void fl_https_cancel(FlHttps *https);
void fl_https_destroy(FlHttps *https);
#define FL_QR_BYTES 512
int fl_https_qr(const char *text,uint8_t qr[FL_QR_BYTES]);
#endif
