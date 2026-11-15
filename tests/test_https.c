#include "fl_https.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"https %d: %s (%s)\n",__LINE__,#x,e.message);exit(1);}}while(0)
static const char *profile="{\"schema_version\":1,\"service_url\":\"https://test.workers.dev\",\"client_id\":\"mock.apps.googleusercontent.com\",\"root_folder_id\":\"mock_root\"}";
static const char *old="[friends]\ntype=drive\nscope=drive.readonly\nclient_id=mock.apps.googleusercontent.com\ntoken={\"refresh_token\":\"OLD_TEST_REFRESH\"}\n";
static int tick(FlHttps *h,FlError *e){struct timespec t={0,10000000};nanosleep(&t,NULL);return fl_https_poll(h,e);}
static int until_ready(FlHttps *h,FlError *e){for(unsigned i=0;i<300;i++){h->next_poll=0;int rc=tick(h,e);if(rc||h->ready)return rc;}return -9;}
static int finish(FlHttps *h,FlError *e){for(unsigned i=0;i<300;i++){int rc=tick(h,e);if(rc)return rc;}return -9;}
static bool same(const char *path,const char *expected){char *s=NULL;if(fl_read_file(path,131072,&s,NULL))return false;bool b=!strcmp(s,expected);free(s);return b;}
int main(int argc,char **argv){
    FlError e={0};CHECK(argc>1);char temp[]="/tmp/fl-https-XXXXXX",path[FL_PATH],source[FL_PATH],profile_path[FL_PATH];FlConfig c={0};FlHttps h={0};FlHttpsProfile pub;
    CHECK(mkdtemp(temp));strcpy(c.sd_root,temp);strcpy(c.app_dir,temp);strcpy(c.remote,"friends");c.timeout_seconds=30;
    snprintf(c.data_dir,FL_PATH,"%s/.config/friends-library",temp);CHECK(!fl_mkdirs(c.data_dir,&e));
    snprintf(c.credential_path,FL_PATH,"%s/.config/friends-library/rclone.conf",temp);snprintf(c.settings_path,FL_PATH,"%s/.config/friends-library/settings.json",temp);
    snprintf(path,FL_PATH,"%s/config",temp);CHECK(!fl_mkdirs(path,&e));snprintf(profile_path,FL_PATH,"%s/config/https-setup.json",temp);
    CHECK(fl_https_profile(&c,&pub,&e)<0);
    CHECK(!fl_write_atomic(profile_path,profile,strlen(profile),&e));CHECK(!fl_https_profile(&c,&pub,&e));
    const char *invalid[]={"http://test.workers.dev","https://evil.test","https://test.workers.dev.evil.test","https://u:p@test.workers.dev","https://test.workers.dev/path","https://test.workers.dev:443"};
    for(unsigned i=0;i<6;i++){char text[2048];snprintf(text,sizeof(text),"{\"schema_version\":1,\"service_url\":\"%s\",\"client_id\":\"mock.apps.googleusercontent.com\",\"root_folder_id\":\"mock_root\"}",invalid[i]);CHECK(!fl_write_atomic(profile_path,text,strlen(text),&e));CHECK(fl_https_profile(&c,&pub,&e)<0);}
    CHECK(!fl_write_atomic(profile_path,profile,strlen(profile),&e));strcpy(c.root_id,"other_root");CHECK(fl_https_profile(&c,&pub,&e)<0);c.root_id[0]=0;
    CHECK(fl_https_start(&h,&c,&e)<0); /* missing Onion runtime */
    snprintf(path,FL_PATH,"%s/.tmp_update/bin",temp);CHECK(!fl_mkdirs(path,&e));strcat(path,"/curl");snprintf(source,FL_PATH,"%s/tests/fixtures/https-curl.sh",argv[1]);CHECK(!symlink(source,path));
    /* First setup: nothing is stored before explicit A confirmation. */
    CHECK(!fl_https_start(&h,&c,&e));CHECK(!until_ready(&h,&e)&&h.ready);CHECK(strlen(h.secret)==43);CHECK(!strcmp(h.code,"123456"));CHECK(access(c.credential_path,F_OK)<0);CHECK(access(c.settings_path,F_OK)<0);
    CHECK(!fl_https_confirm(&h,&e));CHECK(finish(&h,&e)>0);CHECK(fl_credentials_check(&h.account.config,&e)<0); /* workspace was erased */
    char *saved=NULL;CHECK(!fl_read_file(c.credential_path,131072,&saved,&e));CHECK(strstr(saved,"token_url = https://test.workers.dev/v1/token\n"));CHECK(strstr(saved,"client_secret = \n"));CHECK(strstr(saved,"MOCK_REFRESH"));free(saved);CHECK(!*h.secret);fl_https_destroy(&h);
    strcpy(c.root_id,"mock_root");CHECK(!fl_write_atomic(c.credential_path,old,strlen(old),&e));
    const char *failures[]={"offline","unsafe","expired","denied","wrongclient","wrongurl"};
    for(unsigned i=0;i<6;i++){
        CHECK(!setenv("FL_HTTPS_TEST_MODE",failures[i],1));CHECK(!fl_https_start(&h,&c,&e));int rc=until_ready(&h,&e);
        if(h.ready){CHECK(!fl_https_confirm(&h,&e));rc=finish(&h,&e);}CHECK(rc<0&&rc!=-9);CHECK(same(c.credential_path,old));CHECK(!*h.secret);fl_https_destroy(&h);
    }
    CHECK(!setenv("FL_HTTPS_TEST_MODE","pending",1));CHECK(!fl_https_start(&h,&c,&e));for(unsigned i=0;i<20;i++){h.next_poll=0;CHECK(!tick(&h,&e));}CHECK(!h.ready);fl_https_cancel(&h);CHECK(finish(&h,&e)<0&&e.code==FL_ERR_CANCELLED);CHECK(same(c.credential_path,old));fl_https_destroy(&h);
    CHECK(!setenv("FL_HTTPS_TEST_MODE","rate",1));CHECK(!fl_https_start(&h,&c,&e));for(unsigned i=0;i<20;i++){h.next_poll=0;CHECK(!tick(&h,&e));}CHECK(!h.ready);h.deadline=0;CHECK(fl_https_poll(&h,&e)<0&&e.code==FL_ERR_TIMEOUT);CHECK(same(c.credential_path,old));fl_https_destroy(&h);
    CHECK(!setenv("FL_HTTPS_TEST_MODE","ok",1));CHECK(!fl_https_start(&h,&c,&e));CHECK(!until_ready(&h,&e));
    const char *changed="USER_CHANGED_FILE";CHECK(!fl_write_atomic(c.credential_path,changed,strlen(changed),&e));CHECK(!fl_https_confirm(&h,&e));CHECK(finish(&h,&e)<0&&e.code==FL_ERR_BUSY);CHECK(same(c.credential_path,changed));fl_https_destroy(&h);
    CHECK(!setenv("FL_HTTPS_TEST_MODE","ackfail",1));CHECK(!fl_https_start(&h,&c,&e));CHECK(!until_ready(&h,&e));CHECK(!fl_https_confirm(&h,&e));CHECK(finish(&h,&e)>0);fl_https_destroy(&h);
    unsetenv("FL_HTTPS_TEST_MODE");printf("https: %u checks passed (mock transport, TLS constraints, explicit confirmation, first setup, cancellation, failure preservation, concurrency, timeout and ACK loss)\n",checks);return 0;
}
