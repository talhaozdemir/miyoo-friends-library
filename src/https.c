#include "fl_https.h"
#include "cJSON.h"
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

enum { HTTPS_CREATE=1,HTTPS_WAIT,HTTPS_STATUS,HTTPS_READY,HTTPS_CLAIM,HTTPS_ACK,HTTPS_CANCEL };
static double https_clock(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
static int https_path(char out[FL_PATH],const char *a,const char *b){return snprintf(out,FL_PATH,"%s/%s",a,b)>=FL_PATH?-1:0;}
static bool http_id(const char *s){if(!s||strlen(s)!=43)return false;for(;*s;s++)if(!isalnum((unsigned char)*s)&&*s!='-'&&*s!='_')return false;return true;}
static bool origin_valid(const char *s){
    size_t n=s?strlen(s):0;const char *suffix=".workers.dev";size_t k=strlen(suffix);
    if(n<8+k+1||n>=256||strncmp(s,"https://",8)||strcmp(s+n-k,suffix)||strstr(s,"REPLACE"))return false;
    for(const char *p=s+8;*p;p++)if(!((*p>='a'&&*p<='z')||isdigit((unsigned char)*p)||*p=='.'||*p=='-'))return false;
    return true;
}
int fl_https_profile(const FlConfig *c,FlHttpsProfile *p,FlError *error){
    char path[FL_PATH],*text=NULL;cJSON *j=NULL,*v;unsigned keys=0;memset(p,0,sizeof(*p));
    if(https_path(path,c->app_dir,"config/https-setup.json")||fl_read_file(path,8192,&text,NULL)){
        fl_error(error,FL_ERR_CONFIG,"HTTPS phone login is not configured in this package. Computer setup remains available.");return -1;}
    j=cJSON_Parse(text);free(text);if(!cJSON_IsObject(j))goto bad;
    for(v=j->child;v;v=v->next){unsigned bit=0;
        if(!strcmp(v->string,"schema_version")){bit=1;if(!cJSON_IsNumber(v)||v->valuedouble!=1)goto bad;}
        else if(!strcmp(v->string,"service_url")){bit=2;if(!cJSON_IsString(v)||!origin_valid(v->valuestring))goto bad;strcpy(p->origin,v->valuestring);}
        else if(!strcmp(v->string,"client_id")){bit=4;
            const char *suffix=".apps.googleusercontent.com";size_t n=cJSON_IsString(v)?strlen(v->valuestring):0,k=strlen(suffix);
            if(n<=k||n>=256||strcmp(v->valuestring+n-k,suffix))goto bad;
            for(const char *s=v->valuestring;*s;s++)if(!isalnum((unsigned char)*s)&&!strchr("._-",*s))goto bad;
            strcpy(p->client_id,v->valuestring);}
        else if(!strcmp(v->string,"root_folder_id")){bit=8;if(!cJSON_IsString(v)||!fl_id_valid(v->valuestring))goto bad;strcpy(p->root_id,v->valuestring);}
        else goto bad;
        if(keys&bit)goto bad;
        keys|=bit;
    }
    if(keys!=15||(*c->root_id&&strcmp(c->root_id,p->root_id)))goto bad;
    cJSON_Delete(j);fl_error(error,FL_OK,"");return 0;
bad:cJSON_Delete(j);memset(p,0,sizeof(*p));fl_error(error,FL_ERR_CONFIG,"Invalid HTTPS setup or a different shared library. Existing settings were kept.");return -1;
}
static int random_secret(char out[44]){
    static const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";unsigned char bytes[32];size_t done=0;int fd=open("/dev/urandom",O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return -1;
    while(done<sizeof(bytes)){ssize_t n=read(fd,bytes+done,sizeof(bytes)-done);if(n<=0){close(fd);return -1;}done+=(size_t)n;}close(fd);
    unsigned bits=0,value=0;size_t pos=0;for(unsigned i=0;i<32;i++){value=(value<<8)|bytes[i];bits+=8;while(bits>=6){bits-=6;out[pos++]=alphabet[(value>>bits)&63];}}
    if(bits)out[pos++]=alphabet[(value<<(6-bits))&63];
    out[pos]=0;memset(bytes,0,sizeof(bytes));return pos==43?0:-1;
}
/* curl config quoting, not shell interpolation. No control characters allowed. */
static int curl_quote(const char *s,char out[FL_PATH*2+4]){
    size_t n=0;out[n++]='"';for(;*s;s++){if((unsigned char)*s<32)return -1;if(*s=='"'||*s=='\\')out[n++]='\\';if(n>=FL_PATH*2)return -1;out[n++]=*s;}out[n++]='"';out[n]=0;return 0;
}
static int http_start(FlHttps *h,const char *operation,unsigned phase,FlError *error){
    char target[512],cert[FL_PATH],body_file[FL_PATH+2];char qurl[FL_PATH*2+4],qcert[FL_PATH*2+4],qbody[FL_PATH*2+4],conf[8192];cJSON *body=cJSON_CreateObject();char *json=NULL;
    if(!body)goto fail;
    if(phase==HTTPS_CREATE){cJSON_AddStringToObject(body,"device_secret",h->secret);cJSON_AddStringToObject(body,"root_folder_id",h->profile.root_id);}
    else {cJSON_AddStringToObject(body,"pairing_id",h->id);cJSON_AddStringToObject(body,"device_secret",h->secret);}
    json=cJSON_PrintUnformatted(body);cJSON_Delete(body);body=NULL;if(!json)goto fail;
    if(fl_write_atomic(h->body_path,json,strlen(json),error)){free(json);return -1;}memset(json,0,strlen(json));free(json);
    if(snprintf(target,sizeof(target),"%s/v1/pair/%s",h->profile.origin,operation)>=(int)sizeof(target)||https_path(cert,h->account.config.app_dir,"certs/cacert.pem"))goto fail;
    snprintf(body_file,sizeof(body_file),"@%s",h->body_path);
    if(curl_quote(target,qurl)||curl_quote(cert,qcert)||curl_quote(body_file,qbody))goto fail;
    int n=snprintf(conf,sizeof(conf),"url = %s\ncacert = %s\nrequest = \"POST\"\nheader = \"Content-Type: application/json\"\ndata-binary = %s\nsilent\nshow-error\nconnect-timeout = 8\nmax-time = %u\nproto = \"=https\"\nproto-redir = \"=https\"\nwrite-out = \"\\nMFL_STATUS:%%{http_code}\\n\"\n",qurl,qcert,qbody,phase==HTTPS_ACK||phase==HTTPS_CANCEL?3:20);
    if(n<0||n>=(int)sizeof(conf)||fl_write_atomic(h->request_path,conf,(size_t)n,error))return -1;
    fl_process_destroy(&h->worker);
    const char *args[]={h->curl,"--config",h->request_path,NULL};
    if(fl_process_start(&h->worker,args,32768,error))return -1;
    h->phase=phase;return 0;
fail:cJSON_Delete(body);fl_error(error,FL_ERR_CONFIG,"Cannot prepare the secure phone request.");return -1;
}
void fl_https_destroy(FlHttps *h){if(!h)return;fl_process_destroy(&h->worker);fl_phone_destroy(&h->account);memset(h,0,sizeof(*h));}
static int https_finish(FlHttps *h,FlCode code,const char *text,FlError *error){
    fl_process_destroy(&h->worker);fl_phone_destroy(&h->account);h->url[0]=0;memset(h->secret,0,sizeof(h->secret));h->active=false;h->done=true;h->ready=false;fl_error(&h->result,code,text);if(error)*error=h->result;return code==FL_OK?1:-1;
}
int fl_https_start(FlHttps *h,const FlConfig *c,FlError *error){
    if(h->active){fl_error(error,FL_ERR_BUSY,"Phone login is already running.");return -1;}
    fl_https_destroy(h);if(fl_https_profile(c,&h->profile,error))return -1;
    FlPhoneProfile pub={0};strcpy(pub.client_id,h->profile.client_id);strcpy(pub.root_id,h->profile.root_id);
    if(fl_account_stage(&h->account,c,&pub,error))return -1;
    if(https_path(h->curl,c->sd_root,".tmp_update/bin/curl")||access(h->curl,X_OK)){
        fl_error(error,FL_ERR_CONFIG,"OnionOS HTTPS client is unavailable. Use OnionOS 4.3 or computer setup.");goto fail;}
    if(https_path(h->request_path,h->account.workspace,"request.conf")||https_path(h->body_path,h->account.workspace,"request.json")||random_secret(h->secret)){fl_error(error,FL_ERR_IO,"Cannot prepare a private phone session.");goto fail;}
    if(http_start(h,"create",HTTPS_CREATE,error))goto fail;
    h->active=true;h->deadline=https_clock()+300;return 0;
fail:fl_https_destroy(h);return -1;
}
void fl_https_cancel(FlHttps *h){if(h&&h->active&&!h->installed){h->cancelled=true;fl_process_cancel(&h->worker);}}
int fl_https_confirm(FlHttps *h,FlError *error){
    if(!h||!h->active||!h->ready||h->cancelled){fl_error(error,FL_ERR_CONFIG,"Phone approval is not ready.");return -1;}
    h->ready=false;return http_start(h,"claim",HTTPS_CLAIM,error);
}
static cJSON *http_result(FlHttps *h,int *status){
    char *marker=h->worker.out?strstr(h->worker.out,"\nMFL_STATUS:"):NULL,*end=NULL;
    if(!marker)return NULL;
    long n=strtol(marker+12,&end,10);if(n<100||n>599||!end||(*end!='\n'&&*end!=0))return NULL;
    *marker=0;*status=(int)n;return cJSON_Parse(h->worker.out);
}
static const char *string(cJSON *j,const char *name){cJSON *v=cJSON_GetObjectItemCaseSensitive(j,name);return cJSON_IsString(v)?v->valuestring:NULL;}
static int save_claim(FlHttps *h,cJSON *j,FlError *error){
    const char *access=string(j,"access_token"),*refresh=string(j,"refresh_token"),*client=string(j,"client_id"),*root=string(j,"root_folder_id"),*url=string(j,"token_url"),*kind=string(j,"token_type");
    cJSON *expires=cJSON_GetObjectItemCaseSensitive(j,"expires_in");char token_url[400];snprintf(token_url,sizeof(token_url),"%s/v1/token",h->profile.origin);
    if(!access||!*access||strlen(access)>8192||!refresh||!*refresh||strlen(refresh)>4096||!client||strcmp(client,h->profile.client_id)||!root||strcmp(root,h->profile.root_id)||!url||strcmp(url,token_url)||!kind||strcmp(kind,"Bearer")||!cJSON_IsNumber(expires)||expires->valuedouble<30||expires->valuedouble>86400||expires->valuedouble!=(double)expires->valueint){fl_error(error,FL_ERR_AUTH_REQUIRED,"Invalid phone authorization response. Existing account was kept.");return -1;}
    cJSON *token=cJSON_CreateObject();if(!token)return -1;
    cJSON_AddStringToObject(token,"access_token",access);cJSON_AddStringToObject(token,"refresh_token",refresh);cJSON_AddStringToObject(token,"token_type","Bearer");
    time_t expiry=time(NULL)+(time_t)expires->valueint;struct tm utc;char stamp[64];gmtime_r(&expiry,&utc);strftime(stamp,sizeof(stamp),"%Y-%m-%dT%H:%M:%SZ",&utc);cJSON_AddStringToObject(token,"expiry",stamp);
    char *json=cJSON_PrintUnformatted(token);cJSON_Delete(token);if(!json)return -1;
    size_t cap=strlen(json)+2048;char *conf=malloc(cap);if(!conf){free(json);fl_error(error,FL_ERR_LIMIT,"Not enough memory to save account.");return -1;}
    int n=snprintf(conf,cap,"[%s]\ntype = drive\nscope = drive.readonly\nclient_id = %s\nroot_folder_id = %s\ntoken = %s\n",h->account.config.remote,h->profile.client_id,h->profile.root_id,json);memset(json,0,strlen(json));free(json);
    int rc=n<0||(size_t)n>=cap?-1:fl_write_atomic(h->account.staged.credential_path,conf,(size_t)n,error);memset(conf,0,cap);free(conf);
    if(rc||fl_account_commit(&h->account,token_url,error))return -1;
    h->installed=true;return 0;
}
int fl_https_poll(FlHttps *h,FlError *error){
    if(!h||!h->active){if(h&&h->done){if(error)*error=h->result;return h->result.code?-1:1;}fl_error(error,FL_ERR_CONFIG,"HTTPS login is not running.");return -1;}
    double now=https_clock();
    if(h->installed&&h->phase!=HTTPS_ACK)return https_finish(h,FL_OK,"Connected. Phone is no longer needed for normal use.",error);
    if(now>=h->deadline)return https_finish(h,h->installed?FL_OK:FL_ERR_TIMEOUT,h->installed?"Connected. Phone is no longer needed.":"Phone login timed out. Existing account was kept. Create a new QR.",error);
    if(h->cancelled&&h->phase!=HTTPS_CANCEL&&!h->worker.running){
        if(*h->id&&!http_start(h,"cancel",HTTPS_CANCEL,error)){h->deadline=now+4;return 0;}
        return https_finish(h,FL_ERR_CANCELLED,"Phone login cancelled. Existing account was kept.",error);
    }
    if(!h->cancelled&&(h->phase==HTTPS_READY||h->phase==HTTPS_WAIT)){
        if(h->phase==HTTPS_READY||now<h->next_poll)return 0;
        if(http_start(h,"status",HTTPS_STATUS,error))return https_finish(h,error?error->code:FL_ERR_IO,"Could not check phone approval. Existing account was kept.",error);
    }
    FlError e={0};int rc=fl_process_poll(&h->worker,&e);if(!rc)return 0;
    if(h->cancelled&&h->phase!=HTTPS_CANCEL){fl_process_destroy(&h->worker);return 0;}
    if(h->phase==HTTPS_ACK)return https_finish(h,FL_OK,"Connected. Phone is no longer needed for normal use.",error);
    if(h->phase==HTTPS_CANCEL)return https_finish(h,FL_ERR_CANCELLED,"Phone login cancelled. Existing account was kept.",error);
    if(rc<0||h->worker.exit_code)return https_finish(h,FL_ERR_OFFLINE,"Cannot reach the secure login service. Check Wi-Fi and device date/time; existing account was kept.",error);
    int status=0;cJSON *j=http_result(h,&status);if(!cJSON_IsObject(j)){cJSON_Delete(j);return https_finish(h,FL_ERR_IO,"Invalid login service response. Existing account was kept.",error);}
    const char *reason=string(j,"error");
    if(status==429&&h->phase==HTTPS_STATUS){cJSON_Delete(j);fl_process_destroy(&h->worker);h->phase=HTTPS_WAIT;h->next_poll=now+5;return 0;}
    if(status<200||status>=300){
        FlCode code=status==404||status==410?FL_ERR_AUTH_EXPIRED:status==429?FL_ERR_RATE_LIMIT:FL_ERR_IO;
        const char *text=status==429?"Login service rate limit reached. Retry later; existing account was kept.":reason&&!strcmp(reason,"service_not_configured")?"The HTTPS service is not configured yet. Computer setup remains available.":"Phone session is unavailable or expired. Existing account was kept. Create a new QR.";
        cJSON_Delete(j);return https_finish(h,code,text,error);
    }
    if(h->phase==HTTPS_CREATE){
        const char *id=string(j,"pairing_id"),*url=string(j,"verification_uri"),*code=string(j,"display_code");char expected[400];
        if(id)snprintf(expected,sizeof(expected),"%s/pair/%s",h->profile.origin,id);else expected[0]=0;
        bool valid=http_id(id)&&url&&!strcmp(url,expected)&&strlen(url)<sizeof(h->url)&&code&&strlen(code)==6;
        if(valid)for(const char *s=code;*s;s++)if(!isdigit((unsigned char)*s))valid=false;
        if(!valid){cJSON_Delete(j);return https_finish(h,FL_ERR_CONFIG,"Unsafe phone pairing response rejected. Existing account was kept.",error);}
        strcpy(h->id,id);strcpy(h->url,url);strcpy(h->code,code);h->phase=HTTPS_WAIT;h->next_poll=now+5;
    }else if(h->phase==HTTPS_STATUS){
        const char *state=string(j,"status");if(state&&!strcmp(state,"ready")){h->ready=true;h->phase=HTTPS_READY;}
        else if(state&&(!strcmp(state,"pending")||!strcmp(state,"authorizing")||!strcmp(state,"processing"))){h->phase=HTTPS_WAIT;h->next_poll=now+5;}
        else {cJSON_Delete(j);return https_finish(h,FL_ERR_AUTH_REQUIRED,"Google approval or shared-folder access failed. Existing account was kept. Try an invited account.",error);}
    }else if(h->phase==HTTPS_CLAIM){
        if(save_claim(h,j,&e)){cJSON_Delete(j);return https_finish(h,e.code?e.code:FL_ERR_IO,e.message[0]?e.message:"Cannot save phone connection. Existing account was kept.",error);}
        cJSON_Delete(j);fl_process_destroy(&h->worker);if(http_start(h,"ack",HTTPS_ACK,&e))return https_finish(h,FL_OK,"Connected. Phone is no longer needed for normal use.",error);h->deadline=now+4;return 0;
    }
    cJSON_Delete(j);fl_process_destroy(&h->worker);return 0;
}
