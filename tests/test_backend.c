#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include "fl_backend.h"

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/prctl.h>
#endif

static unsigned checks;
#define CHECK(x) do { ++checks; if(!(x)) { fprintf(stderr,"backend assertion %s:%d: %s\n",__FILE__,__LINE__,#x); exit(1); } } while(0)
static void pause_tick(void) { struct timespec t={0,2000000L}; nanosleep(&t,NULL); }
static int finish(FlBackend *b,FlError *e) { int r=0; unsigned n=0; do { r=fl_backend_poll(b,e); if(!r) pause_tick(); } while(!r && ++n<5000); CHECK(n<5000); return r; }
static void write_text(const char *path,const char *s) { FILE *f=fopen(path,"wb"); CHECK(f!=NULL); CHECK(fwrite(s,1,strlen(s),f)==strlen(s)); CHECK(fclose(f)==0); }
static void credentials(const FlConfig *c,const char *scope) {
    char s[2048];
    snprintf(s,sizeof(s),"[friends]\ntype = drive\nscope = %s\nclient_id = fake-test.apps.googleusercontent.com\ntoken = {\"access_token\":\"fake-access\",\"refresh_token\":\"fake-refresh\"}\n",scope);
    write_text(c->credential_path,s);
}

static void auth_recovery_tests(FlConfig *c,const char *directory) {
    FlError e={0};FlBackend b={0};FlListing list={0};char count_path[FL_PATH],*text=NULL;
    struct {const char *input;FlCode code;} errors[]={
        {"googleapi: Error 401: Unauthorized",FL_ERR_AUTH_ACCESS},
        {"oauth2: invalid_grant: NEVER_SHOW_THIS_TOKEN",FL_ERR_AUTH_EXPIRED},
        {"oauth2: invalid_client: Unauthorized",FL_ERR_CONFIG},
        {"unauthorized_client",FL_ERR_CONFIG},{"invalid_scope",FL_ERR_CONFIG},
        {"no refresh token",FL_ERR_AUTH_REQUIRED},
        {"Failed saving config token: no space left on device",FL_ERR_IO},
        {"file abc401xyz is unavailable",FL_ERR_IO},{"unrecognized request failure",FL_ERR_IO}
    };
    for(size_t i=0;i<sizeof(errors)/sizeof(*errors);i++) {
        fl_backend_classify_error(1,errors[i].input,&e);CHECK(e.code==errors[i].code);
        CHECK(!strstr(e.message,"NEVER_SHOW_THIS_TOKEN"));
        if(e.code!=FL_ERR_AUTH_EXPIRED&&e.code!=FL_ERR_AUTH_REQUIRED)CHECK(!strstr(e.message,"Reconnect on a computer"));
    }
    const char *original="# keep comment\r\n[friends]\r\ntype=drive\r\nscope=drive.readonly\r\nclient_id=fake-test\r\ntoken={\"access_token\":\"stale\",\"refresh_token\":\"fake-refresh\",\"expiry\":\"2099-01-01T00:00:00Z\",\"ACCESS_TOKEN\":\"keep-case\"}\r\n[other]\r\nuntouched = yes\r\n";
    write_text(c->credential_path,original);
    CHECK(!fl_credentials_expire_access(c,&e));CHECK(!fl_read_file(c->credential_path,131072,&text,&e));
    CHECK(strstr(text,"\"access_token\":\"stale\"")!=NULL);CHECK(strstr(text,"\"refresh_token\":\"fake-refresh\"")!=NULL);
    CHECK(strstr(text,"\"expiry\":\"1970-01-01T00:00:01Z\"")!=NULL);CHECK(strstr(text,"\"ACCESS_TOKEN\":\"keep-case\"")!=NULL);
    CHECK(!strncmp(text,"# keep comment\r\n[friends]\r\n",27));CHECK(strstr(text,"\r\n[other]\r\nuntouched = yes\r\n")!=NULL);
    free(text);text=NULL;
    /* A failed atomic write must leave the old authorization byte-identical. */
    write_text(c->credential_path,original);
    struct rlimit old_limit,small_limit;struct sigaction old_signal,ignore_signal;
    CHECK(!getrlimit(RLIMIT_FSIZE,&old_limit));small_limit=old_limit;small_limit.rlim_cur=1;
    memset(&ignore_signal,0,sizeof(ignore_signal));ignore_signal.sa_handler=SIG_IGN;sigemptyset(&ignore_signal.sa_mask);
    CHECK(!sigaction(SIGXFSZ,&ignore_signal,&old_signal));CHECK(!setrlimit(RLIMIT_FSIZE,&small_limit));
    int failed_write=fl_credentials_expire_access(c,&e);
    CHECK(!setrlimit(RLIMIT_FSIZE,&old_limit));CHECK(!sigaction(SIGXFSZ,&old_signal,NULL));
    CHECK(failed_write==-1&&e.code==FL_ERR_IO);CHECK(!fl_read_file(c->credential_path,131072,&text,&e));
    CHECK(!strcmp(text,original));free(text);text=NULL;
    CHECK(snprintf(count_path,sizeof(count_path),"%s/auth-calls",directory)>0);
    CHECK(!setenv("FL_MOCK_CALLS",count_path,1));
    const char *modes[]={"renew","always401","renew_revoke","client"};
    const FlCode expected[]={FL_OK,FL_ERR_AUTH_ACCESS,FL_ERR_AUTH_EXPIRED,FL_ERR_CONFIG};
    for(size_t i=0;i<4;i++) {
        credentials(c,"drive.readonly");write_text(count_path,"");CHECK(!setenv("FL_MOCK_MODE",modes[i],1));
        CHECK(!fl_backend_list_start(&b,c,"auth_refresh_folder",true,&e));
        CHECK(finish(&b,&e)==(i==0?1:-1));CHECK(e.code==expected[i]);CHECK(b.auth_retried==(i<3));
        CHECK(!fl_read_file(count_path,1024,&text,&e));CHECK(!strcmp(text,i==3?"call\n":"call\ncall\n"));free(text);text=NULL;
        CHECK(!fl_credentials_check(c,&e));CHECK(!fl_read_file(c->credential_path,131072,&text,&e));
        CHECK(strstr(text,"fake-refresh")!=NULL);if(!i)CHECK(strstr(text,"fake-access-renewed")!=NULL);free(text);text=NULL;
        if(!i){CHECK(!fl_backend_take_list(&b,&list,&e));CHECK(list.count==3);fl_listing_free(&list);}
        fl_backend_destroy(&b);
    }
    credentials(c,"drive.readonly");CHECK(!setenv("FL_MOCK_MODE","renew_slow",1));
    CHECK(!fl_backend_list_start(&b,c,"cancel_refresh_folder",true,&e));
    unsigned n=0;while(!b.auth_retried&&n++<1500){CHECK(fl_backend_poll(&b,&e)==0);pause_tick();}
    CHECK(b.auth_retried&&b.active);fl_backend_cancel(&b);CHECK(finish(&b,&e)==-1);CHECK(e.code==FL_ERR_CANCELLED);
    fl_backend_destroy(&b);CHECK(!unsetenv("FL_MOCK_CALLS"));credentials(c,"drive.readonly");
}

static void parent_death_test(void) {
#ifdef __linux__
    int fds[2],status; pid_t worker,subprocess=0;
    CHECK(prctl(PR_SET_CHILD_SUBREAPER,1)==0); CHECK(pipe(fds)==0);
    worker=fork(); CHECK(worker>=0);
    if(!worker) {
        FlProcess p={0}; FlError e={0};
        const char *args[]={"/bin/sleep","20",NULL};
        close(fds[0]);
        if(fl_process_start(&p,args,256,&e)) _exit(1);
        if(write(fds[1],&p.pid,sizeof(p.pid))!=(ssize_t)sizeof(p.pid)) _exit(2);
        close(fds[1]); for(;;) pause();
    }
    close(fds[1]); CHECK(read(fds[0],&subprocess,sizeof(subprocess))==(ssize_t)sizeof(subprocess)); close(fds[0]);
    CHECK(subprocess>0); CHECK(kill(worker,SIGKILL)==0); CHECK(waitpid(worker,&status,0)==worker);
    { unsigned n=0; pid_t ended;
      do { ended=waitpid(subprocess,&status,WNOHANG); if(!ended) pause_tick(); } while(!ended && ++n<1500);
      CHECK(ended==subprocess); CHECK((WIFSIGNALED(status) && WTERMSIG(status)==SIGKILL) || (WIFEXITED(status) && WEXITSTATUS(status)==126));
    }
#endif
}

int main(int argc,char **argv) {
    char temp[]="/tmp/friends-backend-XXXXXX",app[FL_PATH],data[FL_PATH],settings[FL_PATH],fixture[FL_PATH],listingfile[FL_PATH];
    FlConfig c={0}; FlError e={0}; FlBackend b={0}; FlListing list={0}; FlProcess p={0};
    const char *base=argc>1?argv[1]:".";
    const char *printf_binary=access("/usr/bin/printf",X_OK)==0?"/usr/bin/printf":"/bin/printf";
    CHECK(mkdtemp(temp)!=NULL);
    CHECK(snprintf(app,sizeof(app),"%s/App/FriendsLibrary",temp)>0);
    CHECK(snprintf(data,sizeof(data),"%s/.config/friends-library",temp)>0);
    CHECK(fl_mkdirs(app,&e)==0); CHECK(fl_mkdirs(data,&e)==0);
    CHECK(strlen(data)+strlen("/settings.json")<sizeof(settings)); strcpy(settings,data); strcat(settings,"/settings.json");
    write_text(settings,"{\"root_folder_id\":\"library_root_01\",\"remote\":\"friends\",\"cache_ttl_seconds\":900,\"timeout_seconds\":5}");
    CHECK(fl_config_load(temp,app,&c,&e)==0); CHECK(c.cache_ttl==900); CHECK(c.timeout_seconds==5);
    CHECK(!strcmp(c.root_id,"library_root_01")); CHECK(!strcmp(c.data_dir,data));
    CHECK(fl_credentials_check(&c,&e)==-1); CHECK(e.code==FL_ERR_AUTH_REQUIRED);
    credentials(&c,"drive"); CHECK(fl_credentials_check(&c,&e)==-1);
    credentials(&c,"drive.readonly"); CHECK(fl_credentials_check(&c,&e)==0);
    CHECK(realpath(base,fixture)!=NULL);
    CHECK(snprintf(listingfile,sizeof(listingfile),"%s/tests/fixtures/backend-listing.json",fixture)>0);
    CHECK(setenv("FL_MOCK_LISTING",listingfile,1)==0);
    CHECK(strlen(fixture)+strlen("/tests/fixtures/mock-rclone.sh")<sizeof(c.rclone_path));
    strcpy(c.rclone_path,fixture); strcat(c.rclone_path,"/tests/fixtures/mock-rclone.sh");
    CHECK(chmod(c.rclone_path,0700)==0);
    auth_recovery_tests(&c,temp);
    CHECK(setenv("FL_MOCK_MODE","ok",1)==0);
    CHECK(fl_backend_list_start(&b,&c,c.root_id,true,&e)==0); CHECK(finish(&b,&e)==1);
    CHECK(fl_backend_take_list(&b,&list,&e)==0); CHECK(list.count==3); CHECK(!list.from_cache);
    CHECK(list.items[0].is_dir || list.items[1].is_dir); fl_listing_free(&list);
    CHECK(fl_backend_list_start(&b,&c,c.root_id,false,&e)==0); CHECK(b.done);
    CHECK(fl_backend_take_list(&b,&list,&e)==0); CHECK(list.from_cache && !list.stale); fl_listing_free(&list);
    CHECK(setenv("FL_MOCK_MODE","offline",1)==0);
    CHECK(fl_backend_list_start(&b,&c,c.root_id,true,&e)==0); CHECK(finish(&b,&e)==1);
    CHECK(e.code==FL_ERR_OFFLINE); CHECK(fl_backend_take_list(&b,&list,&e)==0);
    CHECK(list.count==3 && list.from_cache && list.stale); fl_listing_free(&list); CHECK(fl_backend_poll(&b,&e)==1);
    /* Account identity changes must never reuse the previous account's cache. */
    write_text(c.credential_path,"[friends]\ntype=drive\nscope=drive.readonly\nclient_id=another-client\ntoken={\"refresh_token\":\"another-account\"}\n");
    CHECK(fl_backend_list_start(&b,&c,c.root_id,false,&e)==0); CHECK(!b.done);
    CHECK(finish(&b,&e)==-1); CHECK(e.code==FL_ERR_OFFLINE);
    credentials(&c,"drive.readonly");
    { struct {const char *mode; FlCode code;} cases[]={
        {"auth",FL_ERR_AUTH_EXPIRED},{"permission",FL_ERR_PERMISSION},{"timeout",FL_ERR_TIMEOUT},{"rate",FL_ERR_RATE_LIMIT}
      }; size_t i;
      for(i=0;i<sizeof(cases)/sizeof(cases[0]);++i) {
        CHECK(setenv("FL_MOCK_MODE",cases[i].mode,1)==0);
        CHECK(fl_backend_list_start(&b,&c,"uncached_folder",true,&e)==0);
        CHECK(finish(&b,&e)==-1); CHECK(e.code==cases[i].code);
        CHECK(strstr(e.message,"NEVER_SHOW_THIS_TOKEN")==NULL);
      }
    }
    CHECK(setenv("FL_MOCK_MODE","empty",1)==0);
    CHECK(fl_backend_list_start(&b,&c,"empty_folder",true,&e)==0); CHECK(finish(&b,&e)==1);
    CHECK(fl_backend_take_list(&b,&list,&e)==0); CHECK(list.count==0); fl_listing_free(&list);
    CHECK(fl_backend_list_start(&b,&c,"../escape",true,&e)==-1);
    CHECK(setenv("FL_MOCK_MODE","slow",1)==0);
    CHECK(fl_backend_list_start(&b,&c,"slow_folder",true,&e)==0);
    { pid_t child=b.process.pid; fl_backend_cancel(&b); CHECK(finish(&b,&e)==-1); CHECK(e.code==FL_ERR_CANCELLED); errno=0; CHECK(kill(child,0)==-1 && errno==ESRCH); }
    /* argv is not parsed by a shell, even when it contains shell metacharacters. */
    { const char *args[]={printf_binary,"%s","$(touch /tmp/FL_MUST_NOT_EXIST); `id`",NULL}; int r;
      CHECK(fl_process_start(&p,args,256,&e)==0);
      do {r=fl_process_poll(&p,&e); if(!r) pause_tick();} while(!r);
      CHECK(r==1 && p.exit_code==0); CHECK(!strcmp(p.out,args[2])); fl_process_destroy(&p);
    }
    { const char *args[]={printf_binary,"%0100d","0",NULL}; int r;
      CHECK(fl_process_start(&p,args,8,&e)==0);
      do {r=fl_process_poll(&p,&e); if(p.running) pause_tick();} while(p.running);
      CHECK(r==-1 && p.overflow); fl_process_destroy(&p);
    }
    CHECK(setenv("FL_MOCK_MODE","offline",1)==0);
    CHECK(fl_backend_disconnect_start(&b,&c,true,&e)==0); CHECK(finish(&b,&e)==1);
    CHECK(b.revoke_failed); CHECK(access(c.credential_path,F_OK)==-1);
    credentials(&c,"drive.readonly");
    CHECK(fl_backend_disconnect_start(&b,&c,false,&e)==0); CHECK(access(c.credential_path,F_OK)==-1);
    fl_backend_destroy(&b);
    parent_death_test();
    write_text(settings,"{\"root_folder_id\":\"../bad\"}"); CHECK(fl_config_load(temp,app,&c,&e)==-1);
    printf("backend: %u assertions passed (mock Google Drive only)\n",checks);
    /* Temporary test evidence is left in /tmp for failure diagnosis; no user data is touched. */
    return 0;
}
