#include "fl_account.h"
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"account %d: %s (%s)\n",__LINE__,#x,e.message);exit(1);}}while(0)
static const char *old="[friends]\ntype=drive\nscope=drive.readonly\nclient_id=mock.apps.googleusercontent.com\ntoken={\"refresh_token\":\"OLD_TEST_REFRESH\"}\n";
static const char *settings="{\"root_folder_id\":\"mock_root\",\"remote\":\"friends\"}\n";
static const char *fresh="[friends]\ntype=drive\nscope=drive.readonly\nclient_id=mock.apps.googleusercontent.com\ntoken={\"access_token\":\"MOCK_ACCESS\",\"refresh_token\":\"MOCK_REFRESH\",\"expiry\":\"2099-01-01T00:00:00Z\"}\n";
static bool same(const char *path,const char *expected){char *s=NULL;if(fl_read_file(path,131072,&s,NULL))return false;bool b=!strcmp(s,expected);free(s);return b;}
static void path_join(char out[FL_PATH],const char *base,const char *name){int n=snprintf(out,FL_PATH,"%s/%s",base,name);if(n<0||n>=FL_PATH)exit(2);}
static unsigned staging(const char *path){DIR *d=opendir(path);unsigned n=0;struct dirent *v;if(d){while((v=readdir(d)))if(!strncmp(v->d_name,".phone-auth-",12))n++;closedir(d);}return n;}
int main(void){
    FlError e={0};FlConfig c={0};FlAccount a={0};FlAccountProfile profile={0};
    char temp[]="/tmp/fl-account-XXXXXX",path[FL_PATH],child[FL_PATH];struct stat st;
    CHECK(mkdtemp(temp));strcpy(c.sd_root,temp);strcpy(c.app_dir,temp);strcpy(c.remote,"friends");
    snprintf(c.data_dir,FL_PATH,"%s/.config/friends-library",temp);CHECK(!fl_mkdirs(c.data_dir,&e));
    path_join(c.credential_path,c.data_dir,"rclone.conf");path_join(c.settings_path,c.data_dir,"settings.json");
    strcpy(profile.client_id,"mock.apps.googleusercontent.com");strcpy(profile.root_id,"mock_root");
    /* Staging/cancel never installs a partial first account. */
    CHECK(!fl_account_stage(&a,&c,&profile,&e));CHECK(staging(c.data_dir)==1);
    CHECK(fl_account_stage(&a,&c,&profile,&e)<0&&e.code==FL_ERR_BUSY);
    CHECK(access(c.credential_path,F_OK)<0);CHECK(access(c.settings_path,F_OK)<0);
    fl_account_destroy(&a);CHECK(staging(c.data_dir)==0);
    CHECK(!fl_account_stage(&a,&c,&profile,&e));
    CHECK(!fl_write_atomic(a.staged.credential_path,fresh,strlen(fresh),&e));
    CHECK(!fl_account_commit(&a,"https://test.workers.dev/v1/token",&e));
    strcpy(c.root_id,"mock_root");CHECK(!fl_credentials_check(&c,&e));
    char *text=NULL;CHECK(!fl_read_file(c.credential_path,131072,&text,&e));
    CHECK(strstr(text,"client_secret = \n"));CHECK(strstr(text,"token_url = https://test.workers.dev/v1/token\n"));free(text);
    fl_account_destroy(&a);CHECK(staging(c.data_dir)==0);
    CHECK(!fl_write_atomic(c.credential_path,old,strlen(old),&e));CHECK(!fl_write_atomic(c.settings_path,settings,strlen(settings),&e));
    /* Incomplete credentials and unsafe broker URLs cannot replace old bytes. */
    CHECK(!fl_account_stage(&a,&c,&profile,&e));
    const char *invalid="[friends]\ntype=drive\nscope=drive\nclient_id=mock.apps.googleusercontent.com\ntoken={\"refresh_token\":\"MOCK\"}\n";
    CHECK(!fl_write_atomic(a.staged.credential_path,invalid,strlen(invalid),&e));CHECK(fl_account_commit(&a,NULL,&e)<0);CHECK(same(c.credential_path,old));
    CHECK(!fl_write_atomic(a.staged.credential_path,fresh,strlen(fresh),&e));CHECK(fl_account_commit(&a,"http://unsafe.test/token",&e)<0);CHECK(same(c.credential_path,old));fl_account_destroy(&a);
    /* Concurrent changes are preserved, not rolled back to the old snapshot. */
    for(unsigned i=0;i<2;i++){
        CHECK(!fl_account_stage(&a,&c,&profile,&e));CHECK(!fl_write_atomic(a.staged.credential_path,fresh,strlen(fresh),&e));
        const char *target=i?c.settings_path:c.credential_path;CHECK(!fl_write_atomic(target,"USER_CHANGE",11,&e));
        CHECK(fl_account_commit(&a,NULL,&e)<0&&e.code==FL_ERR_BUSY);CHECK(same(target,"USER_CHANGE"));fl_account_destroy(&a);
        CHECK(!fl_write_atomic(c.credential_path,old,strlen(old),&e));CHECK(!fl_write_atomic(c.settings_path,settings,strlen(settings),&e));
    }
    /* Real atomic-write failure preserves both account files. */
    CHECK(!fl_account_stage(&a,&c,&profile,&e));CHECK(!fl_write_atomic(a.staged.credential_path,fresh,strlen(fresh),&e));
    struct rlimit previous,small;CHECK(!getrlimit(RLIMIT_FSIZE,&previous));small=previous;small.rlim_cur=1;
    void (*handler)(int)=signal(SIGXFSZ,SIG_IGN);CHECK(handler!=SIG_ERR);CHECK(!setrlimit(RLIMIT_FSIZE,&small));
    int rc=fl_account_commit(&a,NULL,&e);CHECK(!setrlimit(RLIMIT_FSIZE,&previous));CHECK(signal(SIGXFSZ,handler)!=SIG_ERR);
    CHECK(rc<0&&e.code==FL_ERR_IO);CHECK(same(c.credential_path,old));CHECK(same(c.settings_path,settings));fl_account_destroy(&a);
    /* Abrupt-exit recovery remains compatible with old staging directories. */
    path_join(path,c.data_dir,".phone-auth-Ab1234");CHECK(!mkdir(path,0700));path_join(child,path,"rclone.conf");CHECK(!fl_write_atomic(child,"MOCK",4,&e));
    fl_account_recover(&c);CHECK(lstat(path,&st)<0&&errno==ENOENT);CHECK(same(c.credential_path,old));
    CHECK(!mkdir(path,0700));path_join(child,path,"user-data.txt");CHECK(!fl_write_atomic(child,"KEEP",4,&e));fl_account_recover(&c);CHECK(same(child,"KEEP"));
    CHECK(!unlink(c.credential_path));CHECK(!symlink(child,c.credential_path));CHECK(fl_account_stage(&a,&c,&profile,&e)<0&&e.code==FL_ERR_PATH);CHECK(same(child,"KEEP"));
    printf("account: %u checks passed (first install, staging cleanup, readonly validation, concurrency, atomic failure and symlink protection)\n",checks);return 0;
}
