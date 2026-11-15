#include "fl_account.h"
#include "cJSON.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int join(char *out,const char *base,const char *name){return snprintf(out,FL_PATH,"%s/%s",base,name)>=FL_PATH?-1:0;}
static char *trim_phone(char *s){while(isspace((unsigned char)*s))s++;char *e=s+strlen(s);while(e>s&&isspace((unsigned char)e[-1]))*--e=0;return s;}
static int snapshot(const char *path,size_t limit,char **out,bool *exists,FlError *error){
    struct stat st;*out=NULL;*exists=false;
    if(lstat(path,&st)){if(errno==ENOENT)return 0;goto bad;}
    if(!S_ISREG(st.st_mode)||S_ISLNK(st.st_mode))goto bad;
    *exists=true;return fl_read_file(path,limit,out,error);
bad:fl_error(error,FL_ERR_PATH,"Account files must be regular files on the SD card.");return -1;
}

static void cleanup_workspace(const char *workspace){
    char path[FL_PATH];struct stat st;
    if(!*workspace||lstat(workspace,&st)||!S_ISDIR(st.st_mode)||S_ISLNK(st.st_mode))return;
    const char *names[]={"rclone.conf","rclone.conf.old","verify.conf","request.conf","request.json"};
    for(unsigned i=0;i<5;i++)if(!join(path,workspace,names[i])&&!lstat(path,&st)&&S_ISREG(st.st_mode)&&!S_ISLNK(st.st_mode))unlink(path);
    rmdir(workspace); /* Never recursively remove unknown files. */
}
void fl_account_recover(const FlConfig *c){
    DIR *dir=opendir(c->data_dir);struct dirent *entry;char path[FL_PATH];if(!dir)return;
    while((entry=readdir(dir)))if(!strncmp(entry->d_name,".phone-auth-",12)&&strlen(entry->d_name)==18){
        bool valid=true;for(unsigned i=12;i<18;i++)if(!isalnum((unsigned char)entry->d_name[i]))valid=false;
        if(valid&&!join(path,c->data_dir,entry->d_name))cleanup_workspace(path);
    }
    closedir(dir);
}
void fl_account_destroy(FlAccount *p){
    if(!p)return;
    cleanup_workspace(p->workspace);
    if(p->old_credentials){memset(p->old_credentials,0,strlen(p->old_credentials));free(p->old_credentials);}
    free(p->old_settings);memset(p,0,sizeof(*p));
}
int fl_account_stage(FlAccount *p,const FlConfig *c,const FlAccountProfile *profile,FlError *error){
    struct stat st;
    if(p->active){fl_error(error,FL_ERR_BUSY,"Account setup is already running.");return -1;}
    fl_account_destroy(p);p->config=*c;p->profile=*profile;
    if(snapshot(c->credential_path,131072,&p->old_credentials,&p->had_credentials,error)||snapshot(c->settings_path,16384,&p->old_settings,&p->had_settings,error))goto fail;
    if(p->had_settings&&!fl_id_valid(c->root_id)){fl_error(error,FL_ERR_CONFIG,"Repair the existing library settings with computer setup first.");goto fail;}
    if(lstat(c->data_dir,&st)||!S_ISDIR(st.st_mode)||S_ISLNK(st.st_mode)){fl_error(error,FL_ERR_PATH,"App data directory is unsafe.");goto fail;}
    if(!*c->remote||strlen(c->remote)>=64){fl_error(error,FL_ERR_CONFIG,"Invalid remote name.");goto fail;}
    for(const char *s=c->remote;*s;s++)if(!isalnum((unsigned char)*s)&&*s!='_'&&*s!='-'){fl_error(error,FL_ERR_CONFIG,"Invalid remote name.");goto fail;}
    if(join(p->workspace,c->data_dir,".phone-auth-XXXXXX")||!mkdtemp(p->workspace)){p->workspace[0]=0;fl_error(error,FL_ERR_IO,"Cannot create private login workspace. Check SD space and write access.");goto fail;}
    p->staged=*c;strcpy(p->staged.root_id,p->profile.root_id);
    if(join(p->staged.credential_path,p->workspace,"rclone.conf")){fl_error(error,FL_ERR_PATH,"Login workspace path is too long.");goto fail;}
    p->active=true;fl_error(error,FL_OK,"");return 0;
fail:fl_account_destroy(p);return -1;
}
static bool unchanged(const char *path,const char *old,bool existed){
    char *now=NULL;bool exists=false;bool same=snapshot(path,131072,&now,&exists,NULL)==0&&exists==existed&&(!exists||!strcmp(now,old));free(now);return same;
}
static cJSON *account_token(const FlAccount *p){
    char *text=NULL,*line,*save=NULL;cJSON *token=NULL;bool selected=false;
    if(fl_read_file(p->staged.credential_path,131072,&text,NULL))return NULL;
    for(line=strtok_r(text,"\n",&save);line;line=strtok_r(NULL,"\n",&save)){
        char *s=trim_phone(line),*eq;
        if(*s=='['){char *end=strchr(s,']');if(!end)break;*end=0;selected=!strcmp(s+1,p->config.remote);continue;}
        if(!selected||(eq=strchr(s,'='))==NULL)continue;
        *eq++=0;if(!strcmp(trim_phone(s),"token")){token=cJSON_Parse(trim_phone(eq));break;}
    }
    free(text);return token;
}
int fl_account_commit(FlAccount *p,const char *token_url,FlError *error){
    char *new_credentials=NULL;char settings[512];bool created_settings=false;
    if(fl_credentials_check(&p->staged,error))return -1;
    cJSON *token=account_token(p);char *json=token?cJSON_PrintUnformatted(token):NULL;cJSON_Delete(token);
    if(!json){fl_error(error,FL_ERR_IO,"Cannot prepare the new account.");return -1;}
    if(token_url&&(strncmp(token_url,"https://",8)||strlen(token_url)>400||strpbrk(token_url,"\r\n\"\\ "))){free(json);fl_error(error,FL_ERR_CONFIG,"Unsafe authorization service address.");return -1;}
    new_credentials=malloc(strlen(json)+2560);
    if(!new_credentials){free(json);fl_error(error,FL_ERR_LIMIT,"Not enough memory to save account.");return -1;}
    sprintf(new_credentials,"[%s]\ntype = drive\nscope = drive.readonly\nclient_id = %s\nclient_secret = %s\nroot_folder_id = %s\ntoken = %s\n",p->config.remote,p->profile.client_id,p->profile.client_secret,p->profile.root_id,json);
    if(token_url){strcat(new_credentials,"token_url = ");strcat(new_credentials,token_url);strcat(new_credentials,"\n");}
    memset(json,0,strlen(json));free(json);
    if(!unchanged(p->config.credential_path,p->old_credentials,p->had_credentials)||!unchanged(p->config.settings_path,p->old_settings,p->had_settings)){
        free(new_credentials);fl_error(error,FL_ERR_BUSY,"Account settings changed during login. Existing files were kept; retry.");return -1;
    }
    if(!p->had_settings){
        int n=snprintf(settings,sizeof(settings),"{\"root_folder_id\":\"%s\",\"remote\":\"%s\",\"cache_ttl_seconds\":900,\"timeout_seconds\":30}\n",p->profile.root_id,p->config.remote);
        if(n<0||n>=(int)sizeof(settings)||fl_write_atomic(p->config.settings_path,settings,(size_t)n,error)){free(new_credentials);return -1;}
        created_settings=true;
    }
    if(fl_write_atomic(p->config.credential_path,new_credentials,strlen(new_credentials),error)){
        if(created_settings&&unchanged(p->config.settings_path,settings,true))unlink(p->config.settings_path);
        free(new_credentials);return -1;
    }
    memset(new_credentials,0,strlen(new_credentials));free(new_credentials);strcpy(p->config.root_id,p->profile.root_id);return 0;
}
