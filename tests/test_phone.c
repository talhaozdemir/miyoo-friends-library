#include "fl_phone.h"
#include "qrcodegen.h"
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <signal.h>
#include <unistd.h>

static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"phone %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void write_test(const char *path,const char *text){CHECK(fl_write_atomic(path,text,strlen(text),NULL)==0);}
static void path_test(char out[FL_PATH],const char *base,const char *name){CHECK(snprintf(out,FL_PATH,"%s/%s",base,name)<FL_PATH);}
static int wait_phone(FlPhone *p,FlError *e){
    int rc=0;for(unsigned i=0;i<400&&!rc;i++){rc=fl_phone_poll(p,e);struct timespec t={0,10000000};nanosleep(&t,NULL);}CHECK(rc!=0);return rc;
}
static bool same_file(const char *path,const char *text){char *actual=NULL;if(fl_read_file(path,131072,&actual,NULL))return false;bool same=!strcmp(actual,text);free(actual);return same;}
static unsigned staging(const char *dir){DIR *d=opendir(dir);CHECK(d!=NULL);unsigned n=0;struct dirent *e;while((e=readdir(d)))if(!strncmp(e->d_name,".phone-auth-",12))n++;closedir(d);return n;}
static const char *profile="{\"schema_version\":1,\"client_type\":\"desktop_public\",\"root_folder_id\":\"mock_root\",\"client_id\":\"mock.apps.googleusercontent.com\",\"client_secret\":\"MOCK_PUBLIC_SECRET\"}";
static const char *old_credentials="[friends]\ntype=drive\nscope=drive.readonly\nclient_id=mock.apps.googleusercontent.com\nclient_secret=MOCK_PUBLIC_SECRET\ntoken={\"access_token\":\"OLD_TEST_ACCESS\",\"refresh_token\":\"OLD_TEST_REFRESH\"}\n";
static const char *settings="{\"root_folder_id\":\"mock_root\",\"remote\":\"friends\",\"timeout_seconds\":30}";

int main(int argc,char **argv){
    CHECK(argc>1);char base[]="/tmp/fl-phone-XXXXXX",dir[FL_PATH],profile_path[FL_PATH],url[192],child[FL_PATH];FlConfig c={0};FlPhone p={0};FlPhoneProfile pub;FlError e={0};struct stat st;
    CHECK(mkdtemp(base)!=NULL);strcpy(c.sd_root,base);strcpy(c.app_dir,base);strcpy(c.remote,"friends");c.timeout_seconds=30;
    path_test(c.data_dir,base,".config/friends-library");CHECK(fl_mkdirs(c.data_dir,&e)==0);
    path_test(c.settings_path,c.data_dir,"settings.json");path_test(c.credential_path,c.data_dir,"rclone.conf");
    path_test(c.rclone_path,argv[1],"tests/fixtures/phone-rclone.sh");path_test(dir,base,"config");CHECK(fl_mkdirs(dir,&e)==0);path_test(profile_path,dir,"phone-setup.json");
    CHECK(fl_phone_profile(&c,&pub,&e)<0&&e.code==FL_ERR_CONFIG);
    write_test(profile_path,profile);CHECK(fl_phone_profile(&c,&pub,&e)==0);CHECK(!strcmp(pub.root_id,"mock_root"));
    write_test(profile_path,"{\"schema_version\":1,\"client_type\":\"desktop_public\",\"refresh_token\":\"REJECT\"}");CHECK(fl_phone_profile(&c,&pub,&e)<0);
    write_test(profile_path,"{\"schema_version\":1,\"schema_version\":1}");CHECK(fl_phone_profile(&c,&pub,&e)<0);
    write_test(profile_path,profile);
    CHECK(fl_phone_url("https://evil.test/login?state=foo\n",url)==0);
    CHECK(fl_phone_url("http://127.0.0.1:53682/auth?state=abcdefghijklmnop",url)==0); /* incomplete pipe chunk */
    CHECK(fl_phone_url("http://127.0.0.1:53682/auth?state=short\n",url)<0);
    CHECK(fl_phone_url("http://127.0.0.1:53682/auth?state=abcdefghijklmnop&evil=1\n",url)<0);
    CHECK(fl_phone_url("Please go http://127.0.0.1:53682/auth?state=abcdefghijklmnopqrstuv\n",url)==1);
    uint8_t qr[FL_QR_BYTES];CHECK(fl_phone_qr(url,qr)==0);CHECK(qrcodegen_getSize(qr)>=21&&qrcodegen_getSize(qr)<=57);CHECK(qrcodegen_getModule(qr,0,0));
    char long_url[3000];memset(long_url,'a',sizeof(long_url)-1);long_url[sizeof(long_url)-1]=0;CHECK(fl_phone_qr(long_url,qr)<0);
    CHECK(setenv("FL_PHONE_TEST_MODE","wait",1)==0);CHECK(fl_phone_start(&p,&c,&e)==0);fl_phone_cancel(&p);
    CHECK(wait_phone(&p,&e)<0&&e.code==FL_ERR_CANCELLED);CHECK(lstat(c.settings_path,&st)<0&&errno==ENOENT);CHECK(lstat(c.credential_path,&st)<0&&errno==ENOENT);fl_phone_destroy(&p);unsetenv("FL_PHONE_TEST_MODE");
    /* First installation: no account/settings and no old refresh permission. */
    CHECK(fl_phone_start(&p,&c,&e)==0);CHECK(lstat(c.credential_path,&st)<0&&errno==ENOENT);
    CHECK(staging(c.data_dir)==1);CHECK(wait_phone(&p,&e)>0);CHECK(e.code==FL_OK);CHECK(!strcmp(p.config.root_id,"mock_root"));
    CHECK(fl_credentials_check(&p.config,&e)==0);CHECK(lstat(c.settings_path,&st)==0);CHECK(staging(c.data_dir)==0);fl_phone_destroy(&p);
    strcpy(c.root_id,"mock_root");write_test(c.settings_path,settings);write_test(c.credential_path,old_credentials);
    /* Existing accounts use their own public client even without bundle profile. */
    CHECK(unlink(profile_path)==0);CHECK(fl_phone_profile(&c,&pub,&e)==0);
    const char *modes[]={"denied","missing","scope","permission","offline","wrongroot","unsafe"};
    for(unsigned i=0;i<sizeof(modes)/sizeof(modes[0]);i++){
        CHECK(setenv("FL_PHONE_TEST_MODE",modes[i],1)==0);CHECK(fl_phone_start(&p,&c,&e)==0);CHECK(wait_phone(&p,&e)<0);
        CHECK(same_file(c.credential_path,old_credentials));CHECK(same_file(c.settings_path,settings));
        CHECK(!strstr(e.message,"DO_NOT_SHOW_TEST_TOKEN"));CHECK(staging(c.data_dir)==0);fl_phone_destroy(&p);
    }
    CHECK(setenv("FL_PHONE_TEST_MODE","wait",1)==0);CHECK(fl_phone_start(&p,&c,&e)==0);
    for(unsigned i=0;i<100&&!*p.url;i++){CHECK(fl_phone_poll(&p,&e)==0);struct timespec t={0,10000000};nanosleep(&t,NULL);}
    CHECK(*p.url);fl_phone_cancel(&p);CHECK(wait_phone(&p,&e)<0&&e.code==FL_ERR_CANCELLED);CHECK(same_file(c.credential_path,old_credentials));CHECK(staging(c.data_dir)==0);fl_phone_destroy(&p);
    CHECK(fl_phone_start(&p,&c,&e)==0);p.deadline=0;CHECK(fl_phone_poll(&p,&e)<0&&e.code==FL_ERR_TIMEOUT);CHECK(same_file(c.credential_path,old_credentials));fl_phone_destroy(&p);
    CHECK(setenv("FL_PHONE_TEST_MODE","ok",1)==0);CHECK(fl_phone_start(&p,&c,&e)==0);
    write_test(c.credential_path,"CONCURRENT_ACCOUNT_CHANGE");CHECK(wait_phone(&p,&e)<0&&e.code==FL_ERR_BUSY);CHECK(same_file(c.credential_path,"CONCURRENT_ACCOUNT_CHANGE"));fl_phone_destroy(&p);
    write_test(c.credential_path,old_credentials);CHECK(fl_phone_start(&p,&c,&e)==0);write_test(c.settings_path,"CONCURRENT_SETTINGS_CHANGE");CHECK(wait_phone(&p,&e)<0&&e.code==FL_ERR_BUSY);CHECK(same_file(c.credential_path,old_credentials));fl_phone_destroy(&p);write_test(c.settings_path,settings);
    /* A real atomic-write failure after Google approval preserves old bytes. */
    CHECK(fl_phone_start(&p,&c,&e)==0);
    for(unsigned i=0;i<200&&p.phase!=2;i++){CHECK(fl_phone_poll(&p,&e)==0);struct timespec t={0,10000000};nanosleep(&t,NULL);}CHECK(p.phase==2);
    struct rlimit previous,small;CHECK(getrlimit(RLIMIT_FSIZE,&previous)==0);small=previous;small.rlim_cur=1;
    void (*old_handler)(int)=signal(SIGXFSZ,SIG_IGN);CHECK(old_handler!=SIG_ERR);CHECK(setrlimit(RLIMIT_FSIZE,&small)==0);
    int failed=wait_phone(&p,&e);CHECK(setrlimit(RLIMIT_FSIZE,&previous)==0);CHECK(signal(SIGXFSZ,old_handler)!=SIG_ERR);
    CHECK(failed<0&&e.code==FL_ERR_IO);CHECK(same_file(c.credential_path,old_credentials));CHECK(same_file(c.settings_path,settings));CHECK(staging(c.data_dir)==0);fl_phone_destroy(&p);
    CHECK(fl_phone_start(&p,&c,&e)==0);CHECK(wait_phone(&p,&e)>0);CHECK(fl_credentials_check(&c,&e)==0);CHECK(same_file(c.settings_path,settings));fl_phone_destroy(&p);write_test(c.credential_path,old_credentials);
    /* Abrupt exit cleanup touches only known staging names, not unknown data. */
    path_test(dir,c.data_dir,".phone-auth-Ab1234");CHECK(mkdir(dir,0700)==0);path_test(child,dir,"rclone.conf");write_test(child,"TEST_ONLY");
    fl_phone_recover(&c);CHECK(lstat(dir,&st)<0&&errno==ENOENT);CHECK(same_file(c.credential_path,old_credentials));
    CHECK(mkdir(dir,0700)==0);path_test(child,dir,"user-data.txt");write_test(child,"KEEP");fl_phone_recover(&c);CHECK(same_file(child,"KEEP"));
    CHECK(unlink(c.credential_path)==0);CHECK(symlink(child,c.credential_path)==0);CHECK(fl_phone_start(&p,&c,&e)<0&&e.code==FL_ERR_PATH);CHECK(same_file(child,"KEEP"));CHECK(unlink(c.credential_path)==0);
    unsetenv("FL_PHONE_TEST_MODE");
    printf("phone: %u checks passed (first setup/reconnect, scoped profile, QR, mock OAuth, cancellation/timeout, account preservation, permissions/offline and interrupted cleanup)\n",checks);return 0;
}
