#define _POSIX_C_SOURCE 200809L
#include "fl_backend.h"
#include "cJSON.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/prctl.h>
#endif

#define STDERR_LIMIT 16384u
#define LIST_LIMIT (8u * 1024u * 1024u)

static double monotime(void) {
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t) != 0) return (double)time(NULL);
    return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}

static void close_fd(int *fd) { if (*fd >= 0) { close(*fd); *fd = -1; } }

int fl_process_start(FlProcess *p, const char *const argv[], size_t limit, FlError *error) {
    int out[2] = {-1,-1}, err[2] = {-1,-1};
    pid_t pid, parent=getpid();
    if (!p || !argv || !argv[0] || !strchr(argv[0], '/') || limit > LIST_LIMIT || !limit) {
        fl_error(error, FL_ERR_CONFIG, "Invalid subprocess configuration."); return -1;
    }
    if (p->running) { fl_error(error, FL_ERR_BUSY, "An operation is already running."); return -1; }
    fl_process_destroy(p);
    memset(p, 0, sizeof(*p)); p->out_fd = p->err_fd = -1; p->exit_code = -1;
    p->out_limit = limit; p->out_cap = limit < 4096 ? limit + 1 : 4096;
    p->out = calloc(1, p->out_cap); p->err = calloc(1, STDERR_LIMIT + 1);
    if (!p->out || !p->err || pipe(out) || pipe(err)) goto fail;
    pid = fork();
    if (pid < 0) goto fail;
    if (!pid) {
        int input;
#ifdef __linux__
        /* In particular, an abruptly stopped download worker must not orphan rclone. */
        if(prctl(PR_SET_PDEATHSIG,SIGKILL) || getppid()!=parent) _exit(126);
#else
        (void)parent;
#endif
        setpgid(0, 0);
        input = open("/dev/null", O_RDONLY);
        if (input >= 0) { dup2(input, STDIN_FILENO); close(input); }
        if (dup2(out[1], STDOUT_FILENO) < 0 || dup2(err[1], STDERR_FILENO) < 0) _exit(126);
        close(out[0]); close(out[1]); close(err[0]); close(err[1]);
        execv(argv[0], (char *const *)argv);
        _exit(127);
    }
    p->pid = pid; p->running = true;
    setpgid(pid, pid);
    close_fd(&out[1]); close_fd(&err[1]);
    p->out_fd = out[0]; p->err_fd = err[0];
    if (fcntl(p->out_fd, F_SETFL, O_NONBLOCK) || fcntl(p->err_fd, F_SETFL, O_NONBLOCK)) {
        fl_process_destroy(p); fl_error(error, FL_ERR_IO, "Cannot configure worker pipes."); return -1;
    }
    fcntl(p->out_fd, F_SETFD, FD_CLOEXEC); fcntl(p->err_fd, F_SETFD, FD_CLOEXEC);
    fl_error(error, FL_OK, ""); return 0;
fail:
    close_fd(&out[0]); close_fd(&out[1]); close_fd(&err[0]); close_fd(&err[1]);
    free(p->out); free(p->err); memset(p, 0, sizeof(*p)); p->out_fd=p->err_fd=-1;
    fl_error(error, FL_ERR_IO, "Cannot start worker process."); return -1;
}

/* Bounded stderr tail; never forwarded to UI or application logs. */
static int drain(FlProcess *p, bool standard_error) {
    char buffer[4096];
    int *fd = standard_error ? &p->err_fd : &p->out_fd;
    unsigned passes = 0;
    if (*fd < 0) return 0;
    while (passes++ < 64) {
        ssize_t count = read(*fd, buffer, sizeof(buffer));
        size_t n;
        if (!count) { close_fd(fd); return 0; }
        if (count < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
            close_fd(fd); return -1;
        }
        n = (size_t)count;
        if (standard_error) {
            if (p->err_len+n > STDERR_LIMIT) {
                size_t drop=p->err_len+n-STDERR_LIMIT;
                memmove(p->err, p->err+drop, p->err_len-drop); p->err_len-=drop;
            }
            memcpy(p->err+p->err_len,buffer,n); p->err_len+=n; p->err[p->err_len]=0;
        } else {
            size_t needed=p->out_len+n+1;
            if (p->out_len+n > p->out_limit) { p->overflow=true; fl_process_cancel(p); return -1; }
            if (needed > p->out_cap) {
                size_t cap=p->out_cap * 2;
                char *grown;
                if (cap < needed) cap=needed;
                if (cap > p->out_limit+1) cap=p->out_limit+1;
                grown=realloc(p->out,cap); if (!grown) { fl_process_cancel(p); return -1; }
                p->out=grown; p->out_cap=cap;
            }
            memcpy(p->out+p->out_len,buffer,n); p->out_len+=n; p->out[p->out_len]=0;
        }
    }
    return 0;
}

int fl_process_poll(FlProcess *p, FlError *error) {
    int status, failed=0;
    pid_t waited;
    if (!p || p->pid <= 0) { fl_error(error,FL_ERR_CONFIG,"No worker process."); return -1; }
    if (drain(p,false) < 0 || drain(p,true) < 0) failed=1;
    if (p->running) {
        if (p->cancelled && monotime()-p->cancelled_at >= 1.0) kill(-p->pid,SIGKILL);
        waited=waitpid(p->pid,&status,WNOHANG);
        if (waited==p->pid) {
            p->running=false;
            p->exit_code=WIFEXITED(status) ? WEXITSTATUS(status) : 128+(WIFSIGNALED(status)?WTERMSIG(status):0);
        } else if (waited < 0 && errno != EINTR) { p->running=false; failed=1; }
    }
    if (!p->running) { if(drain(p,false)<0 || drain(p,true)<0) failed=1; }
    if (failed || p->overflow) {
        fl_error(error,p->overflow?FL_ERR_LIMIT:FL_ERR_IO,p->overflow?"Folder listing exceeds the safe memory limit.":"Worker communication failed."); return -1;
    }
    fl_error(error,FL_OK,""); return p->running ? 0 : 1;
}

void fl_process_cancel(FlProcess *p) {
    if (p && p->running && !p->cancelled) {
        p->cancelled=true; p->cancelled_at=monotime(); kill(-p->pid,SIGTERM);
    }
}

void fl_process_destroy(FlProcess *p) {
    if (!p) return;
    if (p->pid > 0) {
        if (p->running) {
            int status;
            struct timespec pause={0,10000000L};
            double deadline=monotime()+1.2;
            fl_process_cancel(p);
            while (monotime()<deadline) {
                pid_t waited=waitpid(p->pid,&status,WNOHANG);
                if (waited==p->pid || (waited<0 && errno==ECHILD)) { p->running=false; break; }
                nanosleep(&pause,NULL);
            }
            if (p->running) { kill(-p->pid,SIGKILL); while(waitpid(p->pid,&status,0)<0 && errno==EINTR) {} }
        }
        /* Remove grandchildren which inherited this owned process group. */
        if (p->cancelled) kill(-p->pid,SIGKILL);
        close_fd(&p->out_fd); close_fd(&p->err_fd);
    }
    free(p->out); free(p->err); memset(p,0,sizeof(*p)); p->out_fd=p->err_fd=-1;
}

static int path_join(char out[FL_PATH], const char *a, const char *b, FlError *error) {
    int n=snprintf(out,FL_PATH,"%s/%s",a,b);
    if(n<0 || n>=FL_PATH) { fl_error(error,FL_ERR_PATH,"Configuration path is too long."); return -1; }
    return 0;
}
static bool remote_valid(const char *s) {
    size_t n=0;
    if(!s || !*s) return false;
    while(*s) { if(!isalnum((unsigned char)*s) && *s!='_' && *s!='-') return false; ++s; ++n; }
    return n<64;
}

int fl_config_load(const char *sd_root,const char *app_dir,FlConfig *c,FlError *error) {
    char *data=NULL;
    cJSON *j=NULL,*v;
    if(!sd_root || !app_dir || !c || sd_root[0]!='/' || app_dir[0]!='/') goto invalid;
    memset(c,0,sizeof(*c));
    if(strlen(sd_root)>=FL_PATH || strlen(app_dir)>=FL_PATH) goto invalid;
    strcpy(c->sd_root,sd_root); strcpy(c->app_dir,app_dir);
    if(path_join(c->data_dir,sd_root,".config/friends-library",error) ||
       path_join(c->settings_path,c->data_dir,"settings.json",error) ||
       path_join(c->credential_path,c->data_dir,"rclone.conf",error) ||
       path_join(c->rclone_path,app_dir,"bin/rclone",error)) return -1;
    strcpy(c->remote,"friends"); c->cache_ttl=900; c->timeout_seconds=30;
    if(fl_mkdirs(c->data_dir,error)) return -1;
    if(fl_read_file(c->settings_path,16384,&data,error)) { fl_error(error,FL_ERR_CONFIG,"Initial setup is required. See the authentication guide."); return -1; }
    j=cJSON_Parse(data); free(data); data=NULL;
    if(!cJSON_IsObject(j)) goto invalid;
    v=cJSON_GetObjectItemCaseSensitive(j,"root_folder_id");
    if(!cJSON_IsString(v) || !fl_id_valid(v->valuestring)) goto invalid;
    strcpy(c->root_id,v->valuestring);
    v=cJSON_GetObjectItemCaseSensitive(j,"remote");
    if(v) { if(!cJSON_IsString(v) || !remote_valid(v->valuestring)) goto invalid; strcpy(c->remote,v->valuestring); }
    v=cJSON_GetObjectItemCaseSensitive(j,"cache_ttl_seconds");
    if(v) { if(!cJSON_IsNumber(v) || v->valuedouble<0 || v->valuedouble>86400 || v->valuedouble!=(double)v->valueint) goto invalid; c->cache_ttl=(unsigned)v->valueint; }
    v=cJSON_GetObjectItemCaseSensitive(j,"timeout_seconds");
    if(v) { if(!cJSON_IsNumber(v) || v->valueint<5 || v->valueint>120 || v->valuedouble!=(double)v->valueint) goto invalid; c->timeout_seconds=(unsigned)v->valueint; }
    cJSON_Delete(j); fl_error(error,FL_OK,""); return 0;
invalid:
    free(data); cJSON_Delete(j); fl_error(error,FL_ERR_CONFIG,"Invalid settings. Check root_folder_id, remote and timeout values."); return -1;
}

static char *trim(char *p) {
    char *end;
    while(isspace((unsigned char)*p)) ++p;
    end=p+strlen(p); while(end>p && isspace((unsigned char)end[-1])) --end;
    *end=0; return p;
}

int fl_credentials_check(const FlConfig *c,FlError *error) {
    char *text=NULL,*line,*save=NULL;
    bool selected=false,found=false,type=false,scope=false,client=false,token=false;
    struct stat st;
    if(!c || !remote_valid(c->remote) || !fl_id_valid(c->root_id)) goto bad;
    if(lstat(c->credential_path,&st) || !S_ISREG(st.st_mode) || S_ISLNK(st.st_mode)) {
        fl_error(error,FL_ERR_AUTH_REQUIRED,"Account setup is required. Copy your personal rclone.conf."); return -1;
    }
    if(fl_read_file(c->credential_path,131072,&text,error)) goto bad;
    for(line=strtok_r(text,"\n",&save);line;line=strtok_r(NULL,"\n",&save)) {
        char *p=trim(line),*eq;
        if(!*p || *p=='#' || *p==';') continue;
        if(*p=='[') {
            char *end=strchr(p,']');
            if(!end || *trim(end+1)) goto bad;
            *end=0; selected=strcmp(trim(p+1),c->remote)==0;
            if(selected) { if(found) goto bad; found=true; }
            continue;
        }
        if(!selected) continue;
        eq=strchr(p,'='); if(!eq) goto bad; *eq=0; p=trim(p); eq=trim(eq+1);
        if(!strcmp(p,"type")) { if(type || strcmp(eq,"drive")) goto bad; type=true; }
        else if(!strcmp(p,"scope")) { if(scope || strcmp(eq,"drive.readonly")) goto bad; scope=true; }
        else if(!strcmp(p,"client_id")) { if(client || !*eq) goto bad; client=true; }
        else if(!strcmp(p,"token")) {
            cJSON *j=cJSON_Parse(eq),*refresh;
            if(token || !j) { cJSON_Delete(j); goto bad; }
            refresh=cJSON_GetObjectItemCaseSensitive(j,"refresh_token");
            token=cJSON_IsString(refresh) && refresh->valuestring[0]; cJSON_Delete(j);
            if(!token) goto bad;
        }
    }
    free(text);
    if(found && type && scope && client && token) { fl_error(error,FL_OK,""); return 0; }
    text=NULL;
bad:
    free(text); fl_error(error,FL_ERR_AUTH_REQUIRED,"A personal Drive remote with drive.readonly, client ID and refresh token is required."); return -1;
}

static bool contains(const char *s,const char *needle) {
    size_t n=strlen(needle);
    if(!s) return false;
    for(;*s;++s) { size_t i; for(i=0;i<n;++i) if(!s[i] || tolower((unsigned char)s[i])!=tolower((unsigned char)needle[i])) break; if(i==n) return true; }
    return false;
}

void fl_backend_classify_error(int exit_code,const char *s,FlError *error) {
    if(contains(s,"invalid_grant") || contains(s,"token has been expired") || contains(s,"unauthorized") || contains(s,"401"))
        fl_error(error,FL_ERR_AUTH_EXPIRED,"Google authorization expired or was revoked. Reconnect on a computer.");
    else if(contains(s,"rateLimitExceeded") || contains(s,"userRateLimitExceeded") || contains(s,"downloadQuotaExceeded") || contains(s,"429"))
        fl_error(error,FL_ERR_RATE_LIMIT,"Google Drive quota or rate limit reached. Try again later.");
    else if(contains(s,"403") || contains(s,"forbidden") || contains(s,"permission denied") || contains(s,"insufficient permissions"))
        fl_error(error,FL_ERR_PERMISSION,"Permission denied. Check the folder invitation and download access.");
    else if(contains(s,"404") || contains(s,"directory not found") || contains(s,"file not found"))
        fl_error(error,FL_ERR_FOLDER,"Shared folder unavailable. Check its ID and your account access.");
    else if(contains(s,"deadline exceeded") || contains(s,"timeout") || contains(s,"timed out"))
        fl_error(error,FL_ERR_TIMEOUT,"Network request timed out. Try again when Wi-Fi is available.");
    else if(contains(s,"no such host") || contains(s,"network is unreachable") || contains(s,"no route to host") || contains(s,"connection refused") || contains(s,"connection reset") || contains(s,"dial tcp"))
        fl_error(error,FL_ERR_OFFLINE,"Cannot reach Google Drive. Check Wi-Fi and try again.");
    else if(contains(s,"certificate") || contains(s,"x509") || contains(s,"tls"))
        fl_error(error,FL_ERR_OFFLINE,"Secure connection failed. Check device date, time and CA certificates.");
    else if(exit_code==127 || exit_code==126)
        fl_error(error,FL_ERR_CONFIG,"The packaged rclone executable is missing or cannot run.");
    else fl_error(error,FL_ERR_IO,"Google Drive request failed. Retry or reconnect your account.");
}

static int cache_path(FlBackend *b,FlError *error) {
    char digest[33],dir[FL_PATH],name[FL_ID*2+48];
    if(fl_md5_file(b->config.credential_path,digest,error)) return -1;
    if(path_join(dir,b->config.data_dir,"cache",error) || fl_mkdirs(dir,error)) return -1;
    /* A dot cannot occur in validated Drive IDs: tuple boundaries are unambiguous. */
    snprintf(name,sizeof(name),"%s.%s.%s.json",b->config.root_id,b->folder_id,digest);
    return path_join(b->cache_path,dir,name,error);
}

static int cache_load(FlBackend *b) {
    char *text=NULL,*array=NULL;
    cJSON *j=NULL,*f,*items;
    FlError ignored;
    FlListing listing={0};
    if(fl_read_file(b->cache_path,LIST_LIMIT,&text,&ignored)) return -1;
    j=cJSON_Parse(text); free(text);
    f=cJSON_GetObjectItemCaseSensitive(j,"fetched"); items=cJSON_GetObjectItemCaseSensitive(j,"listing");
    if(!cJSON_IsNumber(f) || f->valuedouble<=0 || f->valuedouble>(double)time(NULL)+300 || !cJSON_IsArray(items)) goto fail;
    array=cJSON_PrintUnformatted(items); if(!array || fl_listing_parse(array,&listing,&ignored)) goto fail;
    listing.fetched=(time_t)f->valuedouble; listing.from_cache=true;
    listing.stale=time(NULL)-listing.fetched >= (time_t)b->config.cache_ttl;
    b->listing=listing; free(array); cJSON_Delete(j); return 0;
fail:
    free(array); cJSON_Delete(j); fl_listing_free(&listing); return -1;
}

static void cache_save(FlBackend *b,const char *raw) {
    cJSON *j=cJSON_CreateObject(),*array=cJSON_Parse(raw);
    char *text;
    FlError ignored;
    if(!j || !array) { cJSON_Delete(j); cJSON_Delete(array); return; }
    cJSON_AddNumberToObject(j,"fetched",(double)b->listing.fetched);
    cJSON_AddItemToObject(j,"listing",array);
    text=cJSON_PrintUnformatted(j);
    if(text) { fl_write_atomic(b->cache_path,text,strlen(text),&ignored); free(text); }
    cJSON_Delete(j);
}

int fl_backend_list_start(FlBackend *b,const FlConfig *c,const char *folder,bool force,FlError *error) {
    char remote[FL_ID+96],timeout[24];
    const char *argv[32];
    size_t n=0;
    if(!b || !c || !fl_id_valid(folder)) { fl_error(error,FL_ERR_CONFIG,"Invalid folder identifier."); return -1; }
    if(b->active) { fl_error(error,FL_ERR_BUSY,"Another operation is running."); return -1; }
    fl_backend_destroy(b); b->initialized=true; b->config=*c; strcpy(b->folder_id,folder);
    if(fl_credentials_check(c,error) || cache_path(b,error)) return -1;
    (void)cache_load(b);
    if(b->listing.from_cache && !b->listing.stale && !force) {
        b->done=true; fl_error(&b->result,FL_OK,""); fl_error(error,FL_OK,""); return 0;
    }
    snprintf(remote,sizeof(remote),"%s,root_folder_id=%s:",c->remote,folder);
    snprintf(timeout,sizeof(timeout),"%us",c->timeout_seconds);
    argv[n++]=c->rclone_path; argv[n++]="lsjson"; argv[n++]=remote;
    argv[n++]="--config"; argv[n++]=c->credential_path; argv[n++]="--hash";
    argv[n++]="--drive-skip-shortcuts"; argv[n++]="--drive-skip-gdocs";
    argv[n++]="--drive-shared-with-me=false"; argv[n++]="--contimeout"; argv[n++]="10s";
    argv[n++]="--timeout"; argv[n++]=timeout; argv[n++]="--retries"; argv[n++]="2";
    argv[n++]="--low-level-retries"; argv[n++]="2"; argv[n++]="--log-level"; argv[n++]="ERROR";
    argv[n++]="--retries-sleep"; argv[n++]="1s"; argv[n]=NULL;
    if(fl_process_start(&b->process,argv,LIST_LIMIT,error)) {
        if(b->listing.from_cache) { b->listing.stale=true; b->cache_fallback=true; if(error) b->result=*error; b->done=true; return 0; }
        return -1;
    }
    b->active=true; b->started=time(NULL); b->deadline=monotime()+c->timeout_seconds*3+15; fl_error(error,FL_OK,""); return 0;
}

static int remove_credentials(FlBackend *b,FlError *error) {
    if(unlink(b->config.credential_path) && errno!=ENOENT) { fl_error(error,FL_ERR_IO,"Cannot delete local credentials. Remove rclone.conf from the SD card manually."); return -1; }
    fl_error(error,FL_OK,""); return 0;
}

int fl_backend_disconnect_start(FlBackend *b,const FlConfig *c,bool revoke,FlError *error) {
    char remote[70];
    const char *argv[18];
    if(!b || !c) return -1;
    if(b->active) { fl_error(error,FL_ERR_BUSY,"Cancel the current operation before disconnecting."); return -1; }
    fl_backend_destroy(b); b->initialized=true; b->config=*c; b->disconnect=true;
    if(!revoke) { int rc; b->done=true; rc=remove_credentials(b,&b->result); if(error) *error=b->result; return rc; }
    snprintf(remote,sizeof(remote),"%s:",c->remote);
    argv[0]=c->rclone_path; argv[1]="config"; argv[2]="disconnect"; argv[3]=remote;
    argv[4]="--config"; argv[5]=c->credential_path; argv[6]="--contimeout"; argv[7]="5s";
    argv[8]="--timeout"; argv[9]="10s"; argv[10]="--retries"; argv[11]="1";
    argv[12]="--low-level-retries"; argv[13]="1"; argv[14]="--log-level"; argv[15]="ERROR"; argv[16]=NULL;
    if(fl_process_start(&b->process,argv,65536,error)) {
        int rc; b->revoke_failed=true; b->done=true; rc=remove_credentials(b,&b->result); if(error) *error=b->result; return rc;
    }
    b->active=true; b->started=time(NULL); b->deadline=monotime()+15; return 0;
}

int fl_backend_poll(FlBackend *b,FlError *error) {
    int result;
    FlError process_error={0};
    if(!b || !b->initialized) { fl_error(error,FL_ERR_CONFIG,"No library operation."); return -1; }
    if(b->done) { if(error) *error=b->result; return b->result.code && !b->cache_fallback ? -1:1; }
    if(!b->active) { fl_error(error,FL_ERR_CONFIG,"No active library operation."); return -1; }
    if(monotime()>b->deadline) { b->timed_out=true; fl_process_cancel(&b->process); }
    result=fl_process_poll(&b->process,&process_error);
    if(!result) return 0;
    if(result<0 && b->process.running) { fl_process_cancel(&b->process); return 0; }
    b->active=false; b->done=true;
    if(b->disconnect) {
        b->revoke_failed=result<0 || b->process.exit_code!=0;
        result=remove_credentials(b,&b->result);
        if(error) *error=b->result;
        return result<0 ? -1:1;
    }
    if(b->process.overflow) fl_error(&b->result,FL_ERR_LIMIT,"Folder listing exceeds the safe memory limit.");
    else if(b->timed_out) fl_error(&b->result,FL_ERR_TIMEOUT,"Library request timed out. Check Wi-Fi and try again.");
    else if(b->process.cancelled) fl_error(&b->result,FL_ERR_CANCELLED,"Library request cancelled.");
    else if(result<0) b->result=process_error;
    else if(b->process.exit_code) fl_backend_classify_error(b->process.exit_code,b->process.err,&b->result);
    else {
        FlListing fresh={0};
        if(!fl_listing_parse(b->process.out,&fresh,&b->result)) {
            fresh.fetched=time(NULL); fl_listing_free(&b->listing); b->listing=fresh;
            /* Token refresh can rotate the config while listing; rekey safely. */
            (void)cache_path(b,&process_error); cache_save(b,b->process.out);
            fl_error(&b->result,FL_OK,"");
        }
    }
    if(b->result.code && b->listing.from_cache) { b->listing.stale=true; b->cache_fallback=true; }
    if(error) *error=b->result;
    return b->result.code && !b->cache_fallback ? -1:1;
}

int fl_backend_take_list(FlBackend *b,FlListing *listing,FlError *error) {
    if(!b || !listing || !b->done || b->disconnect || (b->result.code && !b->cache_fallback)) {
        if(b && error) *error=b->result;
        return -1;
    }
    *listing=b->listing; memset(&b->listing,0,sizeof(b->listing));
    if(error) *error=b->result;
    return 0;
}

void fl_backend_cancel(FlBackend *b) { if(b && b->active) fl_process_cancel(&b->process); }
void fl_backend_destroy(FlBackend *b) {
    if(!b) return;
    if(b->initialized) { fl_process_destroy(&b->process); fl_listing_free(&b->listing); }
    memset(b,0,sizeof(*b));
}
