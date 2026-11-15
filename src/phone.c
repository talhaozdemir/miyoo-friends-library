#include "fl_phone.h"
#include "cJSON.h"
#include "qrcodegen.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static double phone_clock(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
static int join(char *out,const char *base,const char *name){return snprintf(out,FL_PATH,"%s/%s",base,name)>=FL_PATH?-1:0;}
static char *trim_phone(char *s){while(isspace((unsigned char)*s))s++;char *e=s+strlen(s);while(e>s&&isspace((unsigned char)e[-1]))*--e=0;return s;}
static bool field_valid(const char *s,size_t max){size_t n=s?strlen(s):0;if(!n||n>=max)return false;for(size_t i=0;i<n;i++)if(!isalnum((unsigned char)s[i])&&s[i]!='.'&&s[i]!='_'&&s[i]!='-')return false;return true;}
static bool profile_valid(const FlPhoneProfile *p){
    const char *suffix=".apps.googleusercontent.com";size_t n=strlen(p->client_id),k=strlen(suffix);
    return field_valid(p->client_id,256)&&n>k&&!strcmp(p->client_id+n-k,suffix)&&field_valid(p->client_secret,256)&&fl_id_valid(p->root_id);
}
static int snapshot(const char *path,size_t limit,char **out,bool *exists,FlError *error){
    struct stat st;*out=NULL;*exists=false;
    if(lstat(path,&st)){if(errno==ENOENT)return 0;goto bad;}
    if(!S_ISREG(st.st_mode)||S_ISLNK(st.st_mode))goto bad;
    *exists=true;return fl_read_file(path,limit,out,error);
bad:fl_error(error,FL_ERR_PATH,"Account files must be regular files on the SD card.");return -1;
}

int fl_phone_profile(const FlConfig *c,FlPhoneProfile *p,FlError *error){
    char *text=NULL,*line,*save=NULL;bool exists=false,selected=false,found=false,id=false,secret=false;
    memset(p,0,sizeof(*p));
    if(snapshot(c->credential_path,131072,&text,&exists,error))return -1;
    if(exists){
        if(!fl_id_valid(c->root_id)){free(text);goto invalid;}
        snprintf(p->root_id,sizeof(p->root_id),"%s",c->root_id);
        for(line=strtok_r(text,"\n",&save);line;line=strtok_r(NULL,"\n",&save)){
            char *s=trim_phone(line),*eq;
            if(*s=='['){char *end=strchr(s,']');if(!end||*trim_phone(end+1))goto parse_bad;*end=0;selected=!strcmp(s+1,c->remote);if(selected){if(found)goto parse_bad;found=true;}continue;}
            if(!selected||!*s||*s=='#'||*s==';')continue;
            eq=strchr(s,'=');if(!eq)goto parse_bad;*eq++=0;s=trim_phone(s);eq=trim_phone(eq);
            if(!strcmp(s,"client_id")){if(id||!field_valid(eq,256))goto parse_bad;id=true;strcpy(p->client_id,eq);}
            if(!strcmp(s,"client_secret")){if(secret||!field_valid(eq,256))goto parse_bad;secret=true;strcpy(p->client_secret,eq);}
        }
        free(text);text=NULL;
        if(!found||!profile_valid(p))goto invalid;
    }else{
        char path[FL_PATH];cJSON *j=NULL,*v;unsigned keys=0;
        if(join(path,c->app_dir,"config/phone-setup.json")||fl_read_file(path,8192,&text,error))goto unavailable;
        j=cJSON_Parse(text);free(text);text=NULL;if(!cJSON_IsObject(j)){cJSON_Delete(j);goto invalid;}
        for(v=j->child;v;v=v->next){unsigned bit=0;
            if(!strcmp(v->string,"schema_version")){bit=1;if(!cJSON_IsNumber(v)||v->valuedouble!=1)goto json_bad;}
            else if(!strcmp(v->string,"client_type")){bit=2;if(!cJSON_IsString(v)||strcmp(v->valuestring,"desktop_public"))goto json_bad;}
            else if(!strcmp(v->string,"root_folder_id")){bit=4;if(!cJSON_IsString(v)||!fl_id_valid(v->valuestring))goto json_bad;strcpy(p->root_id,v->valuestring);}
            else if(!strcmp(v->string,"client_id")){bit=8;if(!cJSON_IsString(v)||!field_valid(v->valuestring,256))goto json_bad;strcpy(p->client_id,v->valuestring);}
            else if(!strcmp(v->string,"client_secret")){bit=16;if(!cJSON_IsString(v)||!field_valid(v->valuestring,256))goto json_bad;strcpy(p->client_secret,v->valuestring);}
            else goto json_bad;
            if(keys&bit)goto json_bad;
            keys|=bit;
        }
        if(keys!=31||!profile_valid(p))goto json_bad;
        cJSON_Delete(j);
        /* Existing settings choose the library, never overwrite them from bundle. */
        if(*c->root_id){if(!fl_id_valid(c->root_id))goto invalid;strcpy(p->root_id,c->root_id);}
        fl_error(error,FL_OK,"");return 0;
json_bad:cJSON_Delete(j);goto invalid;
    }
    fl_error(error,FL_OK,"");return 0;
parse_bad:free(text);
invalid:memset(p,0,sizeof(*p));fl_error(error,FL_ERR_CONFIG,"Phone setup definition is invalid. Use the computer setup or reinstall the friends bundle.");return -1;
unavailable:free(text);fl_error(error,FL_ERR_CONFIG,"Phone setup needs the friends bundle's public app definition. Computer setup is still available.");return -1;
}

int fl_phone_url(const char *output,char url[192]){
    const char *prefix="http://127.0.0.1:53682/auth?state=";const char *s=output?strstr(output,prefix):NULL;
    if(!s)return 0;
    const char *p=s+strlen(prefix),*end=p;
    while(isalnum((unsigned char)*end)||*end=='_'||*end=='-')end++;
    if(!*end)return 0; /* A pipe read may end halfway through a state. */
    if(end-p<16||end-p>96||!isspace((unsigned char)*end))return -1;
    size_t n=(size_t)(end-s);if(n>=192)return -1;memcpy(url,s,n);url[n]=0;return 1;
}
int fl_phone_qr(const char *text,uint8_t qr[FL_QR_BYTES]){
    uint8_t temp[FL_QR_BYTES];
    return text&&qrcodegen_encodeText(text,temp,qr,qrcodegen_Ecc_MEDIUM,1,10,qrcodegen_Mask_AUTO,true)?0:-1;
}

static void cleanup_workspace(const char *workspace){
    char path[FL_PATH];struct stat st;
    if(!*workspace||lstat(workspace,&st)||!S_ISDIR(st.st_mode)||S_ISLNK(st.st_mode))return;
    const char *names[]={"rclone.conf","rclone.conf.old","verify.conf"};
    for(unsigned i=0;i<3;i++)if(!join(path,workspace,names[i])&&!lstat(path,&st)&&S_ISREG(st.st_mode)&&!S_ISLNK(st.st_mode))unlink(path);
    rmdir(workspace); /* Never recursively remove unknown files. */
}
void fl_phone_recover(const FlConfig *c){
    DIR *dir=opendir(c->data_dir);struct dirent *entry;char path[FL_PATH];if(!dir)return;
    while((entry=readdir(dir)))if(!strncmp(entry->d_name,".phone-auth-",12)&&strlen(entry->d_name)==18){
        bool valid=true;for(unsigned i=12;i<18;i++)if(!isalnum((unsigned char)entry->d_name[i]))valid=false;
        if(valid&&!join(path,c->data_dir,entry->d_name))cleanup_workspace(path);
    }
    closedir(dir);
}
void fl_phone_destroy(FlPhone *p){
    if(!p)return;
    fl_process_destroy(&p->worker);cleanup_workspace(p->workspace);
    if(p->old_credentials){memset(p->old_credentials,0,strlen(p->old_credentials));free(p->old_credentials);}
    free(p->old_settings);memset(p,0,sizeof(*p));
}
static int finish_phone(FlPhone *p,FlCode code,const char *message,FlError *error){
    fl_process_destroy(&p->worker);cleanup_workspace(p->workspace);p->workspace[0]=0;p->url[0]=0;
    p->active=false;p->done=true;fl_error(&p->result,code,message);if(error)*error=p->result;
    return code==FL_OK?1:-1;
}

int fl_phone_start(FlPhone *p,const FlConfig *c,FlError *error){
    char seed[2048],remote[80];struct stat st;
    if(p->active){fl_error(error,FL_ERR_BUSY,"Phone login is already running.");return -1;}
    fl_phone_destroy(p);p->config=*c;
    if(fl_phone_profile(c,&p->profile,error))return -1;
    if(snapshot(c->credential_path,131072,&p->old_credentials,&p->had_credentials,error)||snapshot(c->settings_path,16384,&p->old_settings,&p->had_settings,error))goto fail;
    if(p->had_settings&&!fl_id_valid(c->root_id)){fl_error(error,FL_ERR_CONFIG,"Repair the existing library settings with computer setup first.");goto fail;}
    if(lstat(c->data_dir,&st)||!S_ISDIR(st.st_mode)||S_ISLNK(st.st_mode)){fl_error(error,FL_ERR_PATH,"App data directory is unsafe.");goto fail;}
    if(!*c->remote||strlen(c->remote)>=64){fl_error(error,FL_ERR_CONFIG,"Invalid remote name.");goto fail;}
    for(const char *s=c->remote;*s;s++)if(!isalnum((unsigned char)*s)&&*s!='_'&&*s!='-'){fl_error(error,FL_ERR_CONFIG,"Invalid remote name.");goto fail;}
    if(join(p->workspace,c->data_dir,".phone-auth-XXXXXX")||!mkdtemp(p->workspace)){p->workspace[0]=0;fl_error(error,FL_ERR_IO,"Cannot create private login workspace. Check SD space and write access.");goto fail;}
    p->staged=*c;strcpy(p->staged.root_id,p->profile.root_id);
    if(join(p->staged.credential_path,p->workspace,"rclone.conf")){fl_error(error,FL_ERR_PATH,"Login workspace path is too long.");goto fail;}
    int n=snprintf(seed,sizeof(seed),"[%s]\ntype = drive\nscope = drive.readonly\nclient_id = %s\nclient_secret = %s\nroot_folder_id = %s\nconfig_is_local = true\nconfig_auth_no_browser = true\nauth_url = https://accounts.google.com/o/oauth2/v2/auth?prompt=consent\n",
                   c->remote,p->profile.client_id,p->profile.client_secret,p->profile.root_id);
    if(n<0||n>=(int)sizeof(seed)||fl_write_atomic(p->staged.credential_path,seed,(size_t)n,error))goto fail;
    snprintf(remote,sizeof(remote),"%s:",c->remote);
    const char *args[]={c->rclone_path,"config","reconnect",remote,"--config",p->staged.credential_path,"--auto-confirm","--retries","1","--low-level-retries","1",NULL};
    if(fl_process_start(&p->worker,args,32768,error))goto fail;
    p->active=true;p->phase=1;p->deadline=phone_clock()+300;fl_error(error,FL_OK,"");return 0;
fail:fl_phone_destroy(p);return -1;
}
static bool unchanged(const char *path,const char *old,bool existed){
    char *now=NULL;bool exists=false;bool same=snapshot(path,131072,&now,&exists,NULL)==0&&exists==existed&&(!exists||!strcmp(now,old));free(now);return same;
}
static cJSON *phone_token(const FlPhone *p){
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
static int verify_start(FlPhone *p,FlError *error){
    cJSON *token=phone_token(p),*access=cJSON_GetObjectItemCaseSensitive(token,"access_token");
    bool valid=cJSON_IsString(access)&&*access->valuestring&&strlen(access->valuestring)<=8192;
    if(valid)for(const unsigned char *s=(const unsigned char*)access->valuestring;*s;s++)
        if(!isalnum(*s)&&!strchr("-._~+/=",*s)){valid=false;break;}
    if(!valid){cJSON_Delete(token);fl_error(error,FL_ERR_AUTH_REQUIRED,"Google did not provide a valid access token. Existing account was kept.");return -1;}
    char path[FL_PATH],*text=malloc(strlen(access->valuestring)+1024);
    if(!text||join(path,p->workspace,"verify.conf")){free(text);cJSON_Delete(token);fl_error(error,FL_ERR_IO,"Cannot prepare shared folder check.");return -1;}
    /* Fixed Google HTTPS endpoint. Bearer value stays in an owned staging file,
     * never in argv, a QR, a browser URL, or output/log messages. */
    int n=sprintf(text,"[fl_phone_verify]\ntype = http\nno_head = true\nno_escape = true\nurl = https://www.googleapis.com/drive/v3/files/\nheaders = Authorization,Bearer %s\n",access->valuestring);
    int rc=fl_write_atomic(path,text,(size_t)n,error);memset(text,0,(size_t)n);free(text);cJSON_Delete(token);if(rc)return -1;
    char remote[FL_ID+100];snprintf(remote,sizeof(remote),"fl_phone_verify:%s?fields=id,mimeType&supportsAllDrives=true",p->profile.root_id);
    const char *args[]={p->config.rclone_path,"cat",remote,"--config",path,"--retries","1","--low-level-retries","1","--contimeout","8s","--timeout","20s","--log-level","ERROR",NULL};
    return fl_process_start(&p->worker,args,8192,error);
}
static int install_phone(FlPhone *p,FlError *error){
    char *new_credentials=NULL;char settings[512];bool created_settings=false;
    if(fl_credentials_check(&p->staged,error))return -1;
    cJSON *token=phone_token(p);char *json=token?cJSON_PrintUnformatted(token):NULL;cJSON_Delete(token);
    if(!json){fl_error(error,FL_ERR_IO,"Cannot prepare the new account.");return -1;}
    new_credentials=malloc(strlen(json)+2048);
    if(!new_credentials){free(json);fl_error(error,FL_ERR_LIMIT,"Not enough memory to save account.");return -1;}
    sprintf(new_credentials,"[%s]\ntype = drive\nscope = drive.readonly\nclient_id = %s\nclient_secret = %s\nroot_folder_id = %s\ntoken = %s\n",p->config.remote,p->profile.client_id,p->profile.client_secret,p->profile.root_id,json);
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
void fl_phone_cancel(FlPhone *p){if(p&&p->active){p->cancelled=true;fl_process_cancel(&p->worker);}}

int fl_phone_poll(FlPhone *p,FlError *error){
    if(!p||!p->active){if(p&&p->done){if(error)*error=p->result;return p->result.code? -1:1;}fl_error(error,FL_ERR_CONFIG,"Phone login is not running.");return -1;}
    if(phone_clock()>=p->deadline&&!p->cancelled)return finish_phone(p,FL_ERR_TIMEOUT,"Phone login timed out. Check the tunnel, then create a new QR.",error);
    FlError e={0};int rc=fl_process_poll(&p->worker,&e);
    if(p->cancelled){if(rc==0)return 0;return finish_phone(p,FL_ERR_CANCELLED,"Phone login cancelled. Existing account was kept.",error);}
    if(rc<0)return finish_phone(p,e.code,e.message,error);
    if(p->phase==1&&!*p->url){
        char candidate[192]="";int found=fl_phone_url(p->worker.err,candidate);
        if(!found)found=fl_phone_url(p->worker.out,candidate);
        if(found<0)return finish_phone(p,FL_ERR_CONFIG,"Unsafe login URL rejected. Computer setup is still available.",error);
        if(found>0)strcpy(p->url,candidate);
    }
    if(!rc)return 0;
    if(p->worker.exit_code){
        if(p->phase==1&&strstr(p->worker.err,"access_denied"))return finish_phone(p,FL_ERR_PERMISSION,"Google login was declined. Existing account was kept.",error);
        if(p->phase==1&&strstr(p->worker.err,"failed to start auth webserver"))return finish_phone(p,FL_ERR_BUSY,"Login port 53682 is busy. Close other login attempts and create a new QR.",error);
        fl_backend_classify_error(p->worker.exit_code,p->worker.err,&e);
        return finish_phone(p,e.code,e.message,error);
    }
    if(p->phase==1){
        if(fl_credentials_check(&p->staged,&e))return finish_phone(p,e.code,"Google did not provide a read-only refresh permission. Retry phone login or use computer setup.",error);
        fl_process_destroy(&p->worker);p->url[0]=0;p->phase=2;p->deadline=phone_clock()+45;
        if(verify_start(p,&e))return finish_phone(p,e.code,e.message,error);
        return 0;
    }
    cJSON *j=cJSON_Parse(p->worker.out);cJSON *type=cJSON_GetObjectItemCaseSensitive(j,"mimeType");cJSON *id=cJSON_GetObjectItemCaseSensitive(j,"id");
    bool valid=cJSON_IsObject(j)&&cJSON_IsString(type)&&!strcmp(type->valuestring,"application/vnd.google-apps.folder")&&cJSON_IsString(id)&&!strcmp(id->valuestring,p->profile.root_id);cJSON_Delete(j);
    if(!valid)return finish_phone(p,FL_ERR_FOLDER,"The selected Google account cannot verify the shared library folder. Existing account was kept.",error);
    if(install_phone(p,&e))return finish_phone(p,e.code,e.message,error);
    return finish_phone(p,FL_OK,"Connected. Close the phone tunnel. Turn SSH off if used only for setup.",error);
}
