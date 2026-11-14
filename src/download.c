#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include "fl_download.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/prctl.h>
#include <sys/syscall.h>
#endif

typedef struct {FlError error;char warning[192];} Completion;
static volatile sig_atomic_t cancelled=0;
static void stop_child(int sig){(void)sig;cancelled=1;}
static void sleep_ms(unsigned ms){struct timespec delay={(time_t)(ms/1000),(long)(ms%1000)*1000000};while(nanosleep(&delay,&delay)&&errno==EINTR&&!cancelled){}}
static double elapsed(const struct timespec *a,const struct timespec *b){return(double)(b->tv_sec-a->tv_sec)+(b->tv_nsec-a->tv_nsec)/1e9;}
static void child_finish(int fd,const char *temp,Completion *done) {
    if(temp&&*temp)unlink(temp);
    size_t offset=0;while(offset<sizeof(*done)) {ssize_t n=write(fd,(char*)done+offset,sizeof(*done)-offset);if(n<0&&errno==EINTR)continue;if(n<=0)break;offset+=(size_t)n;}
    close(fd);_exit(done->error.code==FL_OK?0:1);
}
static int install_file(int dirfd,const char *temp,const char *target,bool overwrite) {
    if(overwrite) return renameat(dirfd,temp,dirfd,target);
#if defined(__linux__) && defined(SYS_renameat2)
    if(syscall(SYS_renameat2,dirfd,temp,dirfd,target,1u)==0)return 0; /* RENAME_NOREPLACE */
    if(errno!=ENOSYS && errno!=EINVAL && errno!=EOPNOTSUPP)return -1;
#endif
    /* Atomic no-replace fallback for filesystems with hard links. FAT32 needs
     * renameat2 support; fail safely if neither operation is available. */
    if(linkat(dirfd,temp,dirfd,target,0))return -1;
    (void)unlinkat(dirfd,temp,0);
    return 0;
}
static void worker(int fd,pid_t parent_pid,const FlConfig *c,const FlItem *item,const char *target,const char *temp,bool overwrite) {
    Completion done={0};FlProcess process={0};struct stat initial,st;
    bool existed=!lstat(target,&initial);
    struct sigaction action;memset(&action,0,sizeof(action));action.sa_handler=stop_child;sigemptyset(&action.sa_mask);
    sigaction(SIGTERM,&action,NULL);sigaction(SIGINT,&action,NULL);cancelled=0;
#ifdef __linux__
    if(prctl(PR_SET_PDEATHSIG,SIGTERM)<0 || getppid()!=parent_pid) {
        fl_error(&done.error,FL_ERR_CANCELLED,"Download owner exited.");child_finish(fd,temp,&done);
    }
#else
    (void)parent_pid;
#endif
    char parent[FL_PATH];strcpy(parent,target);char *target_name=strrchr(parent,'/');
    if(!target_name){fl_error(&done.error,FL_ERR_PATH,"Invalid ROM destination.");child_finish(fd,temp,&done);}
    *target_name++=0;
    char target_file[FL_NAME],temp_file[FL_NAME];
    snprintf(target_file,sizeof(target_file),"%s",target_name);
    snprintf(temp_file,sizeof(temp_file),"%s",strrchr(temp,'/')+1);
    int parent_fd=open(parent,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    struct stat initial_directory;
    if(parent_fd<0 || fstat(parent_fd,&initial_directory)) {
        if(parent_fd>=0)close(parent_fd);
        fl_error(&done.error,FL_ERR_PATH,"Cannot open the allowed ROM directory.");child_finish(fd,temp,&done);
    }
    if(existed&&!overwrite){fl_error(&done.error,FL_ERR_EXISTS,"Destination exists. Explicit replacement confirmation is required.");child_finish(fd,temp,&done);}
    if(existed&&(!S_ISREG(initial.st_mode)||S_ISLNK(initial.st_mode))){fl_error(&done.error,FL_ERR_PATH,"Unsafe ROM destination.");child_finish(fd,temp,&done);}
    char remote[80],timeout[32];snprintf(remote,sizeof(remote),"%s:",c->remote);snprintf(timeout,sizeof(timeout),"%us",c->timeout_seconds);
    const char *argv[]={c->rclone_path,"backend","copyid",remote,item->id,temp,
        "--config",c->credential_path,"--drive-skip-shortcuts","--drive-scope","drive.readonly",
        "--inplace","--local-no-preallocate","--transfers","1","--checkers","1","--multi-thread-streams","0",
        "--buffer-size","1M","--timeout",timeout,"--contimeout","10s","--retries","2",
        "--low-level-retries","2","--retries-sleep","2s","--stats","0","--log-level","ERROR",NULL};
    if(fl_process_start(&process,argv,4096,&done.error))child_finish(fd,temp,&done);
    for(;;) {
        if(cancelled) {fl_process_cancel(&process);fl_process_destroy(&process);fl_error(&done.error,FL_ERR_CANCELLED,"Download cancelled. Existing ROMs were preserved.");child_finish(fd,temp,&done);}
        int rc=fl_process_poll(&process,&done.error);
        if(rc<0){fl_process_destroy(&process);child_finish(fd,temp,&done);}
        if(rc==1)break;
        sleep_ms(80);
    }
    if(process.exit_code)fl_backend_classify_error(process.exit_code,process.err,&done.error);
    fl_process_destroy(&process);
    if(done.error.code!=FL_OK)child_finish(fd,temp,&done);
    if(lstat(temp,&st)||!S_ISREG(st.st_mode)||S_ISLNK(st.st_mode)||(int64_t)st.st_size!=item->size){fl_error(&done.error,FL_ERR_INTEGRITY,"Download size does not match the library. Refresh and try again.");child_finish(fd,temp,&done);}
    if(cancelled){fl_error(&done.error,FL_ERR_CANCELLED,"Download cancelled.");child_finish(fd,temp,&done);}
    if(*item->md5) {
        char md5[33];if(fl_md5_file(temp,md5,&done.error))child_finish(fd,temp,&done);
        if(strcasecmp(md5,item->md5)){fl_error(&done.error,FL_ERR_INTEGRITY,"Checksum mismatch. The source may have changed; refresh the library.");child_finish(fd,temp,&done);}
    } else snprintf(done.warning,sizeof(done.warning),"Size verified; source did not provide a checksum.");
    if(cancelled){fl_error(&done.error,FL_ERR_CANCELLED,"Download cancelled.");child_finish(fd,temp,&done);}
    int file=openat(parent_fd,temp_file,O_RDONLY|O_NOFOLLOW);
    if(file<0||fsync(file)){if(file>=0)close(file);fl_error(&done.error,FL_ERR_IO,"Cannot flush downloaded ROM to SD card.");child_finish(fd,temp,&done);}close(file);
    /* Confirm the selected parent still resolves to the original allowed directory. */
    char resolved[FL_PATH];struct stat current_directory;
    if(!realpath(parent,resolved)||strcmp(parent,resolved)||stat(parent,&current_directory)||
       current_directory.st_dev!=initial_directory.st_dev||current_directory.st_ino!=initial_directory.st_ino){fl_error(&done.error,FL_ERR_PATH,"ROM directory changed during download.");child_finish(fd,temp,&done);}
    bool now_exists=!fstatat(parent_fd,target_file,&st,AT_SYMLINK_NOFOLLOW);
    if(now_exists && (!overwrite||!existed||st.st_dev!=initial.st_dev||st.st_ino!=initial.st_ino||st.st_size!=initial.st_size||st.st_mtime!=initial.st_mtime)) {
        fl_error(&done.error,FL_ERR_EXISTS,"Destination changed during download; existing ROM preserved.");child_finish(fd,temp,&done);
    }
    if(existed&&!now_exists){fl_error(&done.error,FL_ERR_EXISTS,"Destination changed during download. Please retry.");child_finish(fd,temp,&done);}
    if(install_file(parent_fd,temp_file,target_file,overwrite)) {
        fl_error(&done.error,errno==EEXIST?FL_ERR_EXISTS:FL_ERR_IO,
                 errno==EEXIST?"Destination appeared during download; existing ROM preserved.":
                 "Cannot safely install ROM on this filesystem. Existing ROM preserved.");
        child_finish(fd,temp,&done);
    }
    fsync(parent_fd);close(parent_fd);
    FlError receipt_error={0};
    if(fl_receipt_write(c,item,target,&receipt_error))snprintf(done.warning,sizeof(done.warning),"ROM installed; could not save its download record.");
    child_finish(fd,NULL,&done);
}
int fl_download_start(FlDownload *d,const FlConfig *c,const FlSystem *s,const FlItem *item,bool overwrite,FlError *e) {
    if(d->active){fl_error(e,FL_ERR_BUSY,"A download is already active.");return -1;}
    if(!fl_id_valid(item->id)||item->is_dir){fl_error(e,FL_ERR_FORMAT,"Select a complete ROM file.");return -1;}
    if(fl_credentials_check(c,e))return -1;
    memset(d,0,sizeof(*d));d->status_fd=-1;d->total=item->size;
    if(fl_rom_target(c,s,item->name,true,d->target,e))return -1;
    char directory[FL_PATH];strcpy(directory,d->target);*strrchr(directory,'/')=0;
    if(fl_space_check(directory,item->size,NULL,e))return -1;
    struct stat st;
    if(!lstat(d->target,&st)&&!overwrite){fl_error(e,FL_ERR_EXISTS,"Existing ROM requires explicit replacement confirmation.");return -1;}
    if(snprintf(d->temporary,sizeof(d->temporary),"%s/.friends-library-%s.part",directory,item->id)>=(int)sizeof(d->temporary)){fl_error(e,FL_ERR_PATH,"Temporary path is too long.");return -1;}
    int temp=open(d->temporary,O_CREAT|O_EXCL|O_WRONLY|O_NOFOLLOW,0600);
    if(temp<0){fl_error(e,FL_ERR_IO,"An incomplete transfer exists. Relaunch to recover it.");return -1;}close(temp);
    int pipefd[2];if(pipe(pipefd)){unlink(d->temporary);fl_error(e,FL_ERR_IO,"Cannot create download worker.");return -1;}
    if(fcntl(pipefd[0],F_SETFD,FD_CLOEXEC)||fcntl(pipefd[1],F_SETFD,FD_CLOEXEC)||
       fcntl(pipefd[0],F_SETFL,O_NONBLOCK)) {
        close(pipefd[0]);close(pipefd[1]);unlink(d->temporary);
        fl_error(e,FL_ERR_IO,"Cannot configure download communication.");return -1;
    }
    pid_t parent_pid=getpid();
    pid_t pid=fork();
    if(pid<0){close(pipefd[0]);close(pipefd[1]);unlink(d->temporary);fl_error(e,FL_ERR_IO,"Cannot start download worker.");return -1;}
    if(!pid){close(pipefd[0]);worker(pipefd[1],parent_pid,c,item,d->target,d->temporary,overwrite);}
    close(pipefd[1]);d->pid=pid;d->status_fd=pipefd[0];d->active=true;clock_gettime(CLOCK_MONOTONIC,&d->last_sample);return 0;
}
int fl_download_poll(FlDownload *d,FlError *e) {
    if(!d->active){if(d->done){if(e)*e=d->result;return d->result.code==FL_OK?1:-1;}return 0;}
    struct stat st;if(!lstat(d->temporary,&st))d->downloaded=(int64_t)st.st_size;
    struct timespec now;clock_gettime(CLOCK_MONOTONIC,&now);double seconds=elapsed(&d->last_sample,&now);
    if(seconds>=0.3){d->bytes_per_second=(d->downloaded-d->last_bytes)/seconds;if(d->bytes_per_second<0)d->bytes_per_second=0;d->last_bytes=d->downloaded;d->last_sample=now;}
    if(d->cancelling && elapsed(&d->cancel_started,&now)>=1.5)kill(d->pid,SIGKILL);
    int status;pid_t result=waitpid(d->pid,&status,WNOHANG);
    if(!result)return 0;
    if(result<0&&errno==EINTR)return 0;
    Completion done={0};size_t got=0;
    while(got<sizeof(done)) {ssize_t n=read(d->status_fd,(char*)&done+got,sizeof(done)-got);if(n<0&&errno==EINTR)continue;if(n<=0)break;got+=(size_t)n;}
    close(d->status_fd);d->status_fd=-1;d->active=false;d->done=true;
    if(got!=sizeof(done)){fl_error(&d->result,d->cancelling?FL_ERR_CANCELLED:FL_ERR_IO,d->cancelling?"Download cancelled.":"Download worker stopped unexpectedly; incomplete file removed.");unlink(d->temporary);}
    else {d->result=done.error;snprintf(d->warning,sizeof(d->warning),"%s",done.warning);}
    if(d->result.code==FL_OK)d->downloaded=d->total;
    if(e)*e=d->result;
    return d->result.code==FL_OK?1:-1;
}
void fl_download_cancel(FlDownload *d){if(d->active&&!d->cancelling){d->cancelling=true;clock_gettime(CLOCK_MONOTONIC,&d->cancel_started);kill(d->pid,SIGTERM);}}
void fl_download_destroy(FlDownload *d){
    if(!d)return;
    if(d->active){
        struct timespec started,now;clock_gettime(CLOCK_MONOTONIC,&started);
        fl_download_cancel(d);
        while(fl_download_poll(d,NULL)==0){
            clock_gettime(CLOCK_MONOTONIC,&now);
            if(elapsed(&started,&now)>=2.5){
                kill(d->pid,SIGKILL);waitpid(d->pid,NULL,WNOHANG);
                fl_error(&d->result,FL_ERR_CANCELLED,"Download stopped during application shutdown.");
                d->active=false;d->done=true;break;
            }
            sleep_ms(20);
        }
    }
    if(d->status_fd>=0&&d->pid){close(d->status_fd);d->status_fd=-1;}
}
int fl_download_recover(const FlConfig *c,const FlSystems *systems,FlError *e) {
    for(size_t i=0;i<systems->count;i++) {
        char dummy[FL_PATH],example[FL_NAME];const FlSystem *s=&systems->systems[i];
        const char *sep=strchr(s->extensions,'|');int n=sep?(int)(sep-s->extensions):(int)strlen(s->extensions);
        snprintf(example,sizeof(example),"recover.%.*s",n,s->extensions);
        if(fl_rom_target(c,s,example,false,dummy,NULL))continue;
        *strrchr(dummy,'/')=0;DIR *dir=opendir(dummy);if(!dir)continue;
        struct dirent *de;int dfd=dirfd(dir);
        while((de=readdir(dir))) {
            const char *name=de->d_name;size_t len=strlen(name);const char *prefix=".friends-library-";size_t start=strlen(prefix);
            if(strncmp(name,prefix,start)||len<=start+5||strcmp(name+len-5,".part"))continue;
            char id[FL_ID];size_t id_len=len-start-5;if(id_len>=sizeof(id))continue;memcpy(id,name+start,id_len);id[id_len]=0;if(!fl_id_valid(id))continue;
            struct stat st;if(fstatat(dfd,name,&st,AT_SYMLINK_NOFOLLOW)||!S_ISREG(st.st_mode))continue;
            if(unlinkat(dfd,name,0)){closedir(dir);fl_error(e,FL_ERR_IO,"Cannot remove incomplete transfer; check SD card.");return -1;}
        }
        closedir(dir);
    }
    return 0;
}
