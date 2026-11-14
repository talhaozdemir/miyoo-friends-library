#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "fl_download.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
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

static double now(void) {
    struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);
    return t.tv_sec+t.tv_nsec/1e9;
}
static void pause_ms(int ms) {struct timespec t={ms/1000,(ms%1000)*1000000};nanosleep(&t,NULL);}
static void join(char out[FL_PATH],const char *base,const char *suffix) {
    size_t a=strlen(base),b=strlen(suffix);assert(a+b<FL_PATH);
    memcpy(out,base,a);memcpy(out+a,suffix,b+1);
}
static void save(const char *path,const char *data) {
    FILE *file=fopen(path,"wb");assert(file);assert(fwrite(data,1,strlen(data),file)==strlen(data));assert(!fclose(file));
}
static void expect_content(const char *path,const char *expected) {
    char data[128]={0};FILE *file=fopen(path,"rb");assert(file);
    size_t length=fread(data,1,sizeof(data)-1,file);fclose(file);
    assert(length==strlen(expected)&&!strcmp(data,expected));
}
static int finish(FlDownload *download,FlError *error,double limit) {
    double started=now();int result;
    do {result=fl_download_poll(download,error);if(result)return result;pause_ms(10);}while(now()-started<limit);
    fl_download_destroy(download);assert(!"download did not finish within its test deadline");return 0;
}
static void wait_file(const char *path) {
    double started=now();struct stat st;
    while(stat(path,&st)&&now()-started<2)pause_ms(10);
    assert(!stat(path,&st));
}
static pid_t read_pid(const char *path) {
    long value=0;FILE *file=fopen(path,"r");assert(file);assert(fscanf(file,"%ld",&value)==1);fclose(file);return(pid_t)value;
}
static bool is_live(pid_t pid) {
    if(kill(pid,0)<0&&errno==ESRCH)return false;
#ifdef __linux__
    char path[64],line[1024];snprintf(path,sizeof(path),"/proc/%ld/stat",(long)pid);
    FILE *file=fopen(path,"r");if(!file)return false;
    if(!fgets(line,sizeof(line),file)){fclose(file);return false;}fclose(file);
    char *end=strrchr(line,')');if(end&&end[1]==' '&&end[2]=='Z')return false;
#endif
    return true;
}
static void expect_stopped(pid_t pid) {
    double started=now();while(is_live(pid)&&now()-started<3)pause_ms(10);
    assert(!is_live(pid));while(waitpid(-1,NULL,WNOHANG)>0){}
}
static FlItem item(const char *id,const char *name,int64_t size,const char *md5) {
    FlItem result={0};snprintf(result.id,sizeof(result.id),"%s",id);snprintf(result.name,sizeof(result.name),"%s",name);
    result.size=size;if(md5)snprintf(result.md5,sizeof(result.md5),"%s",md5);return result;
}

int main(void) {
    char workspace[FL_PATH],fixture[FL_PATH],source[FL_PATH],pidfile[FL_PATH],target[FL_PATH],path[FL_PATH];
    char temp[]="build/test-download-XXXXXX";FlConfig config={0};FlError error={0};FlDownload download={0};
    FlSystem system={0};FlItem game;FlSystems systems={&system,1};
    assert(mkdtemp(temp));assert(realpath(temp,workspace));
    assert(realpath("tests/fixtures/download-rclone.sh",fixture));assert(!chmod(fixture,0755));
    snprintf(config.sd_root,sizeof(config.sd_root),"%s",workspace);
    join(config.data_dir,workspace,"/config");assert(!fl_mkdirs(config.data_dir,&error));
    join(config.credential_path,config.data_dir,"/rclone.conf");
    snprintf(config.rclone_path,sizeof(config.rclone_path),"%s",fixture);
    snprintf(config.remote,sizeof(config.remote),"friends");snprintf(config.root_id,sizeof(config.root_id),"test_root");
    config.timeout_seconds=5;
    save(config.credential_path,"[friends]\ntype = drive\nscope = drive.readonly\nclient_id = test\ntoken = {\"refresh_token\":\"test-only\"}\n");
    snprintf(system.id,sizeof(system.id),"FC");snprintf(system.label,sizeof(system.label),"NES");snprintf(system.extensions,sizeof(system.extensions),"nes");
    join(source,workspace,"/source");save(source,"complete ROM bytes");
    join(pidfile,workspace,"/rclone.pid");
    assert(!setenv("FL_TEST_SOURCE",source,1));assert(!setenv("FL_TEST_PID_FILE",pidfile,1));
    char md5[33];assert(!fl_md5_file(source,md5,&error));
    const int64_t payload_size=(int64_t)strlen("complete ROM bytes");

    game=item("copy_ok","Installed.nes",payload_size,md5);
    assert(!fl_download_start(&download,&config,&system,&game,false,&error));
    assert(finish(&download,&error,3)==1);assert(error.code==FL_OK);
    join(target,workspace,"/Roms/FC/Installed.nes");expect_content(target,"complete ROM bytes");
    assert(download.downloaded==payload_size);assert(access(download.temporary,F_OK));
    assert(fl_local_check(&config,&system,&game,false,&error)==FL_LOCAL_PRESENT);
    assert(fl_local_check(&config,&system,&game,true,&error)==FL_LOCAL_VERIFIED);
    join(path,config.data_dir,"/receipts/copy_ok.json");assert(!access(path,F_OK));
    fl_download_destroy(&download);

    /* No overwrite is the default, and successful explicit overwrite remains valid. */
    save(target,"existing ROM");assert(fl_download_start(&download,&config,&system,&game,false,&error)==-1);
    assert(error.code==FL_ERR_EXISTS);expect_content(target,"existing ROM");
    assert(!fl_download_start(&download,&config,&system,&game,true,&error));assert(finish(&download,&error,3)==1);
    expect_content(target,"complete ROM bytes");fl_download_destroy(&download);

    game=item("copy_badsize","Badsize.nes",payload_size,md5);
    assert(!fl_download_start(&download,&config,&system,&game,false,&error));assert(finish(&download,&error,3)==-1);
    assert(error.code==FL_ERR_INTEGRITY);assert(access(download.target,F_OK));assert(access(download.temporary,F_OK));fl_download_destroy(&download);
    game=item("copy_ok","Badmd5.nes",payload_size,"00000000000000000000000000000000");
    assert(!fl_download_start(&download,&config,&system,&game,false,&error));assert(finish(&download,&error,3)==-1);
    assert(error.code==FL_ERR_INTEGRITY);assert(access(download.target,F_OK));fl_download_destroy(&download);

    const char *ids[]={"copy_auth","copy_offline","copy_permission"};
    const FlCode expected[]={FL_ERR_AUTH_EXPIRED,FL_ERR_OFFLINE,FL_ERR_PERMISSION};
    for(size_t i=0;i<3;i++) {
        game=item(ids[i],"Failed.nes",payload_size,md5);
        assert(!fl_download_start(&download,&config,&system,&game,false,&error));assert(finish(&download,&error,3)==-1);
        assert(error.code==expected[i]);assert(access(download.target,F_OK));assert(access(download.temporary,F_OK));fl_download_destroy(&download);
    }

    game=item("copy_ok","Huge.nes",INT64_MAX,NULL);
    assert(fl_download_start(&download,&config,&system,&game,false,&error)==-1);assert(error.code==FL_ERR_SPACE);

    /* Cancel an explicit replacement; the original file must remain byte-identical. */
    game=item("copy_slow","Cancel.nes",payload_size,md5);
    join(target,workspace,"/Roms/FC/Cancel.nes");save(target,"keep this ROM");
    unlink(pidfile);assert(!fl_download_start(&download,&config,&system,&game,true,&error));wait_file(pidfile);
    pid_t network_pid=read_pid(pidfile);assert(is_live(network_pid));
    assert(fl_download_start(&download,&config,&system,&game,true,&error)==-1);assert(error.code==FL_ERR_BUSY);
    double started=now();fl_download_cancel(&download);assert(finish(&download,&error,3)==-1);
    assert(error.code==FL_ERR_CANCELLED);assert(now()-started<2.5);expect_content(target,"keep this ROM");
    assert(access(download.temporary,F_OK));fl_download_destroy(&download);expect_stopped(network_pid);

    /* A worker crash must not block pipe reads, or leave its rclone alive. */
    game=item("copy_crash","Crashed.nes",payload_size,md5);unlink(pidfile);
    assert(!fl_download_start(&download,&config,&system,&game,false,&error));
    assert(finish(&download,&error,3)==-1);assert(error.code==FL_ERR_IO);wait_file(pidfile);
    expect_stopped(read_pid(pidfile));assert(access(download.temporary,F_OK));fl_download_destroy(&download);

    /* A concurrent destination cannot be overwritten by a normal download. */
    game=item("copy_race","Concurrent.nes",payload_size,md5);
    join(target,workspace,"/Roms/FC/Concurrent.nes");assert(!setenv("FL_TEST_RACE_TARGET",target,1));
    assert(!fl_download_start(&download,&config,&system,&game,false,&error));assert(finish(&download,&error,3)==-1);
    assert(error.code==FL_ERR_EXISTS);expect_content(target,"concurrent ROM");fl_download_destroy(&download);

#ifdef __linux__
    /* Simulate the app disappearing without its normal shutdown handler. */
    assert(!prctl(PR_SET_CHILD_SUBREAPER,1));unlink(pidfile);int pid_pipe[2];assert(!pipe(pid_pipe));
    pid_t owner=fork();assert(owner>=0);
    if(!owner) {
        close(pid_pipe[0]);FlDownload owned={0};FlError child_error={0};
        FlItem owned_item=item("copy_slow","Orphan.nes",payload_size,md5);
        if(fl_download_start(&owned,&config,&system,&owned_item,false,&child_error))_exit(2);
        wait_file(pidfile);assert(write(pid_pipe[1],&owned.pid,sizeof(owned.pid))==(ssize_t)sizeof(owned.pid));_exit(0);
    }
    close(pid_pipe[1]);pid_t worker_pid=0;assert(read(pid_pipe[0],&worker_pid,sizeof(worker_pid))==(ssize_t)sizeof(worker_pid));close(pid_pipe[0]);
    int owner_status;assert(waitpid(owner,&owner_status,0)==owner);assert(WIFEXITED(owner_status)&&WEXITSTATUS(owner_status)==0);
    network_pid=read_pid(pidfile);expect_stopped(worker_pid);expect_stopped(network_pid);
    join(path,workspace,"/Roms/FC/.friends-library-copy_slow.part");assert(access(path,F_OK));
#endif

    /* Recovery only removes regular files matching the exact owned temp format. */
    join(path,workspace,"/Roms/FC/.friends-library-recover_id.part");save(path,"partial");
    join(target,workspace,"/Roms/FC/user.part");save(target,"user data");
    char link[FL_PATH];join(link,workspace,"/Roms/FC/.friends-library-symlink.part");assert(!symlink(target,link));
    assert(!fl_download_recover(&config,&systems,&error));assert(access(path,F_OK));expect_content(target,"user data");
    struct stat st;assert(!lstat(link,&st)&&S_ISLNK(st.st_mode));
    puts("Download integration: install, overwrite, integrity, errors, space, cancellation, worker/parent death, race and recovery passed");
    return 0;
}
