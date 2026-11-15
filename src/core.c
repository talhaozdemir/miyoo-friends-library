#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include "fl_core.h"
#include "cJSON.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/vfs.h>
#endif

void fl_error(FlError *e, FlCode code, const char *message) {
    if (e) {
        e->code = code;
        if(!message)message="";
        size_t n=strnlen(message,sizeof(e->message)-1);
        memmove(e->message,message,n);e->message[n]=0;
    }
}
const char *fl_code_name(FlCode code) {
    static const char *names[] = {"ok","configuration","authentication_required","authentication_expired",
        "offline","permission_denied","folder_unavailable","rate_limit","timeout","unsupported_format",
        "unsupported_console","unsafe_path","destination_exists","insufficient_space","io_error",
        "cancelled","integrity_error","busy","limit_exceeded","access_token_rejected"};
    return code >= 0 && (size_t)code < sizeof(names)/sizeof(*names) ? names[code] : "unknown";
}
void fl_listing_free(FlListing *l) { if(l) { free(l->items); memset(l,0,sizeof(*l)); } }
bool fl_id_valid(const char *s) {
    if (!s || !*s || strlen(s) >= FL_ID) return false;
    for(;*s;s++) if(!isalnum((unsigned char)*s) && *s != '-' && *s != '_') return false;
    return true;
}
static int failure(FlError *e, FlCode code, const char *s) { fl_error(e,code,s); return -1; }
int fl_mkdirs(const char *path, FlError *e) {
    char temp[FL_PATH]; struct stat st;
    if(!path || !*path || strlen(path)>=sizeof(temp)) return failure(e,FL_ERR_PATH,"Directory path is too long.");
    strcpy(temp,path);
    for(char *p=temp+1;;p++) if(*p=='/' || !*p) {
        char save=*p; *p=0;
        if(mkdir(temp,0700) && errno!=EEXIST) return failure(e,FL_ERR_IO,"Cannot create directory on SD card.");
        if(lstat(temp,&st) || !S_ISDIR(st.st_mode) || S_ISLNK(st.st_mode))
            return failure(e,FL_ERR_PATH,"Directory contains an unsafe link or non-directory.");
        *p=save; if(!save) break;
    }
    return 0;
}
int fl_read_file(const char *path, size_t limit, char **out, FlError *e) {
    int fd=open(path,O_RDONLY|O_NOFOLLOW); struct stat st; char *data; size_t done=0;
    *out=NULL;
    if(fd<0) return failure(e,FL_ERR_IO,"Cannot read local file.");
    if(fstat(fd,&st) || !S_ISREG(st.st_mode) || st.st_size<0 || (uint64_t)st.st_size>limit) {
        close(fd); return failure(e,FL_ERR_LIMIT,"Local file is invalid or exceeds its size limit.");
    }
    data=malloc((size_t)st.st_size+1);
    if(!data) { close(fd); return failure(e,FL_ERR_LIMIT,"Not enough memory."); }
    while(done<(size_t)st.st_size) {
        ssize_t n=read(fd,data+done,(size_t)st.st_size-done);
        if(n<0 && errno==EINTR) continue;
        if(n<=0) {free(data);close(fd);return failure(e,FL_ERR_IO,"Local file read was interrupted.");}
        done+=(size_t)n;
    }
    close(fd); data[done]=0; *out=data; return 0;
}
int fl_write_atomic(const char *path, const void *data, size_t len, FlError *e) {
    char tmp[FL_PATH]; size_t done=0;
    if(snprintf(tmp,sizeof(tmp),"%s.tmp-XXXXXX",path)>=(int)sizeof(tmp)) return failure(e,FL_ERR_PATH,"Local path is too long.");
    int fd=mkstemp(tmp);
    if(fd<0) return failure(e,FL_ERR_IO,"Cannot create local temporary file.");
    while(done<len) {
        ssize_t n=write(fd,(const char*)data+done,len-done);
        if(n<0 && errno==EINTR) continue;
        if(n<=0) {close(fd);unlink(tmp);return failure(e,FL_ERR_IO,"Cannot save local file; check SD space.");}
        done+=(size_t)n;
    }
    int ok=fsync(fd); if(close(fd)) ok=-1;
    if(ok || rename(tmp,path)) {unlink(tmp);return failure(e,FL_ERR_IO,"Cannot finish saving local file.");}
    return 0;
}
static int copy_string(char *dst,size_t size,const cJSON *value) {
    if(!cJSON_IsString(value) || !value->valuestring || strlen(value->valuestring)>=size) return -1;
    strcpy(dst,value->valuestring); return 0;
}
static bool token_has(const char *tokens,const char *word) {
    size_t n=strlen(word);
    for(const char *p=tokens;*p;) {
        const char *end=strchr(p,'|'); size_t len=end?(size_t)(end-p):strlen(p);
        if(len==n && !strncasecmp(p,word,n)) return true;
        if(!end)break;
        p=end+1;
    }
    return false;
}
int fl_systems_load(const char *path, FlSystems *out, FlError *e) {
    char *text=NULL; cJSON *root=NULL,*array,*v; FlSystems systems={0};
    if(fl_read_file(path,65536,&text,e)) return -1;
    root=cJSON_Parse(text);free(text);
    if(!root || !cJSON_IsArray(array=cJSON_GetObjectItemCaseSensitive(root,"systems")) || cJSON_GetArraySize(array)>64) goto invalid;
    systems.count=(size_t)cJSON_GetArraySize(array);
    systems.systems=calloc(systems.count?systems.count:1,sizeof(FlSystem));
    if(!systems.systems) goto invalid;
    for(size_t i=0;i<systems.count;i++) {
        FlSystem *s=&systems.systems[i]; v=cJSON_GetArrayItem(array,(int)i);
        if(copy_string(s->id,sizeof(s->id),cJSON_GetObjectItemCaseSensitive(v,"id")) ||
           copy_string(s->label,sizeof(s->label),cJSON_GetObjectItemCaseSensitive(v,"label")) ||
           copy_string(s->aliases,sizeof(s->aliases),cJSON_GetObjectItemCaseSensitive(v,"aliases")) ||
           copy_string(s->extensions,sizeof(s->extensions),cJSON_GetObjectItemCaseSensitive(v,"extensions")) || !*s->id) goto invalid;
        for(const char *p=s->id;*p;p++) if(!(*p>='A'&&*p<='Z') && !isdigit((unsigned char)*p)) goto invalid;
        for(size_t j=0;j<i;j++) if(!strcmp(s->id,systems.systems[j].id)) goto invalid;
    }
    cJSON_Delete(root);*out=systems;return 0;
invalid:
    cJSON_Delete(root);free(systems.systems);return failure(e,FL_ERR_CONFIG,"Invalid systems.json mapping.");
}
void fl_systems_free(FlSystems *s) {free(s->systems);memset(s,0,sizeof(*s));}
const FlSystem *fl_system_find(const FlSystems *systems, const char *name) {
    if(!name) return NULL;
    char normalized[FL_NAME]; size_t n=strlen(name);
    if(n>=sizeof(normalized))return NULL;
    while(n && isspace((unsigned char)name[n-1]))n--;
    while(n && isspace((unsigned char)*name)) {name++;n--;}
    memcpy(normalized,name,n);normalized[n]=0;
    for(size_t i=0;i<systems->count;i++)
        if(!strcasecmp(systems->systems[i].id,normalized)||token_has(systems->systems[i].aliases,normalized)) return &systems->systems[i];
    return NULL;
}
bool fl_extension_allowed(const FlSystem *s, const char *name) {
    if(!s||!name)return false;
    const char *dot=strrchr(name,'.');return dot && dot!=name && token_has(s->extensions,dot+1);
}
static bool utf8_valid(const unsigned char *s) {
    while(*s) {
        unsigned c=*s++; if(c<0x80)continue;
        unsigned need,cp;
        if(c>=0xC2&&c<=0xDF){need=1;cp=c&31;}
        else if(c>=0xE0&&c<=0xEF){need=2;cp=c&15;}
        else if(c>=0xF0&&c<=0xF4){need=3;cp=c&7;}
        else return false;
        unsigned count=need;
        while(need--) {if((*s&0xC0)!=0x80)return false;cp=(cp<<6)|(*s++&63);}
        if((count==2&&cp<0x800)||(count==3&&cp<0x10000)||cp>0x10FFFF||(cp>=0xD800&&cp<=0xDFFF))return false;
    }
    return true;
}
int fl_sanitize_filename(const char *name, char out[FL_NAME], FlError *e) {
    if(!name||!*name||strlen(name)>240||!utf8_valid((const unsigned char*)name))return failure(e,FL_ERR_PATH,"Invalid or overlong filename.");
    if(name[0]=='.'||name[0]==' '||strchr(name,'/')||strchr(name,'\\'))return failure(e,FL_ERR_PATH,"Unsafe filename or path traversal rejected.");
    size_t n=strlen(name);
    if(name[n-1]==' '||name[n-1]=='.')return failure(e,FL_ERR_PATH,"Filename has a trailing dot or space.");
    for(size_t i=0;i<n;i++) {
        unsigned char c=(unsigned char)name[i];
        if(c<32||c==127)return failure(e,FL_ERR_PATH,"Control characters in filename rejected.");
        out[i]=strchr(":*?\"<>|",c)?'_':(char)c;
    }
    out[n]=0;return 0;
}
static int safe_directory(const char *path,bool create,FlError *e) {
    struct stat st;
    if(create && mkdir(path,0755) && errno!=EEXIST) return failure(e,FL_ERR_IO,"Cannot create ROM directory.");
    if(lstat(path,&st)||!S_ISDIR(st.st_mode)||S_ISLNK(st.st_mode))return failure(e,FL_ERR_PATH,"ROM directory is missing or contains an unsafe link.");
    return 0;
}
int fl_rom_target(const FlConfig *c,const FlSystem *s,const char *name,bool create,char out[FL_PATH],FlError *e) {
    char clean[FL_NAME],sd[FL_PATH],roms[FL_PATH],dir[FL_PATH];
    if(!s||!*s->id)return failure(e,FL_ERR_UNSUPPORTED,"Console is not supported.");
    for(const char *p=s->id;*p;p++)if(!(*p>='A'&&*p<='Z')&&!isdigit((unsigned char)*p))return failure(e,FL_ERR_PATH,"Invalid console directory.");
    if(fl_sanitize_filename(name,clean,e))return -1;
    if(!fl_extension_allowed(s,clean))return failure(e,FL_ERR_FORMAT,"ROM extension is not supported for this console.");
    if(!realpath(c->sd_root,sd) || !strcmp(sd,"/"))return failure(e,FL_ERR_PATH,"Invalid SD card root.");
    if(snprintf(roms,sizeof(roms),"%s/Roms",sd)>=(int)sizeof(roms)||safe_directory(roms,create,e))return -1;
    if(snprintf(dir,sizeof(dir),"%s/%s",roms,s->id)>=(int)sizeof(dir)||safe_directory(dir,create,e))return -1;
    DIR *d=opendir(dir);struct dirent *de;
    if(!d)return failure(e,FL_ERR_IO,"Cannot inspect ROM directory.");
    while((de=readdir(d))) if(!strcasecmp(de->d_name,clean) && strcmp(de->d_name,clean)) {
        closedir(d);return failure(e,FL_ERR_EXISTS,"A ROM with the same name and different letter case already exists.");
    }
    closedir(d);
    if(snprintf(out,FL_PATH,"%s/%s",dir,clean)>=FL_PATH)return failure(e,FL_ERR_PATH,"ROM destination is too long.");
    struct stat st;
    if(!lstat(out,&st)&&(!S_ISREG(st.st_mode)||S_ISLNK(st.st_mode)))return failure(e,FL_ERR_PATH,"Destination is not a regular ROM file.");
    return 0;
}
int fl_space_check(const char *directory,int64_t bytes,uint64_t *available,FlError *e) {
    struct statvfs st;
    if(bytes<0)return failure(e,FL_ERR_FORMAT,"Unknown file size; refresh the library.");
    if(statvfs(directory,&st))return failure(e,FL_ERR_IO,"Cannot read available SD card space.");
    uint64_t unit=st.f_frsize?st.f_frsize:st.f_bsize;
    uint64_t free_bytes=unit && (uint64_t)st.f_bavail>UINT64_MAX/unit?UINT64_MAX:(uint64_t)st.f_bavail*unit;
    if(available)*available=free_bytes;
#ifdef __linux__
    struct statfs fs;
    if(!statfs(directory,&fs) && (unsigned long)fs.f_type==0x4d44UL && (uint64_t)bytes>UINT32_MAX)
        return failure(e,FL_ERR_SPACE,"File exceeds the FAT32 maximum file size.");
#endif
    if((uint64_t)bytes>free_bytes || free_bytes-(uint64_t)bytes<1024*1024)return failure(e,FL_ERR_SPACE,"Not enough SD card space (1 MiB safety reserve required).");
    return 0;
}
static bool md5_valid(const char *s) {
    if(strlen(s)!=32)return false;
    for(;*s;s++)if(!isxdigit((unsigned char)*s))return false;
    return true;
}
static int compare_items(const void *a,const void *b) {
    const FlItem *x=a,*y=b;
    if(x->is_dir!=y->is_dir)return x->is_dir?-1:1;
    int n=strcasecmp(x->name,y->name);return n?n:strcmp(x->id,y->id);
}
static int compare_ids(const void *a,const void *b) {return strcmp(((const FlItem*)a)->id,((const FlItem*)b)->id);}
int fl_listing_parse(const char *text,FlListing *out,FlError *e) {
    FlListing l={0};cJSON *root=cJSON_Parse(text);
    if(!root||!cJSON_IsArray(root)||cJSON_GetArraySize(root)>FL_MAX_LIST||strstr(text,"\\u0000"))goto invalid;
    size_t capacity=(size_t)cJSON_GetArraySize(root);
    l.items=calloc(capacity?capacity:1,sizeof(FlItem));if(!l.items)goto invalid;
    for(size_t i=0;i<capacity;i++) {
        const cJSON *o=cJSON_GetArrayItem(root,(int)i), *n,*hashes,*h;
        FlItem item={0};item.size=-1;
        if(copy_string(item.id,sizeof(item.id),cJSON_GetObjectItemCaseSensitive(o,"ID"))||!fl_id_valid(item.id)||
           copy_string(item.name,sizeof(item.name),cJSON_GetObjectItemCaseSensitive(o,"Name")))goto invalid;
        n=cJSON_GetObjectItemCaseSensitive(o,"IsDir");if(!cJSON_IsBool(n))goto invalid;
        item.is_dir=cJSON_IsTrue(n);
        n=cJSON_GetObjectItemCaseSensitive(o,"MimeType");if(n&&copy_string(item.mime,sizeof(item.mime),n))goto invalid;
        if(!strcmp(item.mime,"application/vnd.google-apps.shortcut"))continue;
        char name[FL_NAME];if(fl_sanitize_filename(item.name,name,NULL))continue;
        n=cJSON_GetObjectItemCaseSensitive(o,"Size");
        if(cJSON_IsNumber(n)) {
            if(!isfinite(n->valuedouble)||n->valuedouble>=9007199254740992.0||n->valuedouble < -1||floor(n->valuedouble)!=n->valuedouble)goto invalid;
            item.size=(int64_t)n->valuedouble;
        } else if(cJSON_IsString(n)) {
            char *end;errno=0;long long size=strtoll(n->valuestring,&end,10);
            if(errno||!*n->valuestring||*end||size< -1)goto invalid;
            item.size=(int64_t)size;
        } else if(!item.is_dir)goto invalid;
        if(!item.is_dir&&item.size<0)goto invalid;
        n=cJSON_GetObjectItemCaseSensitive(o,"ModTime");if(n&&copy_string(item.modified,sizeof(item.modified),n))goto invalid;
        hashes=cJSON_GetObjectItemCaseSensitive(o,"Hashes");h=cJSON_GetObjectItemCaseSensitive(hashes,"MD5");
        if(h){if(copy_string(item.md5,sizeof(item.md5),h)||!md5_valid(item.md5))goto invalid;}
        l.items[l.count++]=item;
    }
    qsort(l.items,l.count,sizeof(FlItem),compare_ids);
    for(size_t i=1;i<l.count;i++)if(!strcmp(l.items[i-1].id,l.items[i].id))goto invalid;
    qsort(l.items,l.count,sizeof(FlItem),compare_items);l.fetched=time(NULL);
    cJSON_Delete(root);*out=l;return 0;
invalid:
    cJSON_Delete(root);fl_listing_free(&l);return failure(e,FL_ERR_FORMAT,"Invalid or oversized library listing; no files were installed.");
}
void fl_listing_directories(FlListing *l) {
    size_t j=0;for(size_t i=0;i<l->count;i++)if(l->items[i].is_dir)l->items[j++]=l->items[i];l->count=j;
}
bool fl_listing_collision(const FlListing *l,size_t selected) {
    char a[FL_NAME],b[FL_NAME];if(selected>=l->count)return true;
    if(fl_sanitize_filename(l->items[selected].name,a,NULL))return true;
    for(size_t i=0;i<l->count;i++)if(i!=selected&&!l->items[i].is_dir&&!fl_sanitize_filename(l->items[i].name,b,NULL)&&!strcasecmp(a,b))return true;
    return false;
}

FlLocal fl_local_check(const FlConfig *c,const FlSystem *s,const FlItem *item,bool hash,FlError *e) {
    char path[FL_PATH],digest[33];struct stat st;
    if(fl_rom_target(c,s,item->name,false,path,e))return FL_LOCAL_NONE;
    if(lstat(path,&st))return FL_LOCAL_NONE;
    if((int64_t)st.st_size!=item->size)return FL_LOCAL_CHANGED;
    if(hash&&*item->md5) {
        if(fl_md5_file(path,digest,e))return FL_LOCAL_PRESENT;
        return !strcasecmp(digest,item->md5)?FL_LOCAL_VERIFIED:FL_LOCAL_CHANGED;
    }
    /* A receipt alone cannot prove a ROM was not edited after installation. */
    return FL_LOCAL_PRESENT;
}
int fl_receipt_write(const FlConfig *c,const FlItem *item,const char *target,FlError *e) {
    char dir[FL_PATH],path[FL_PATH];
    if(!fl_id_valid(item->id)||snprintf(dir,sizeof(dir),"%s/receipts",c->data_dir)>=(int)sizeof(dir)||fl_mkdirs(dir,e))return -1;
    if(snprintf(path,sizeof(path),"%s/%s.json",dir,item->id)>=(int)sizeof(path))return failure(e,FL_ERR_PATH,"Receipt path is too long.");
    cJSON *o=cJSON_CreateObject();if(!o)return failure(e,FL_ERR_LIMIT,"Not enough memory for receipt.");
    cJSON_AddStringToObject(o,"id",item->id);cJSON_AddStringToObject(o,"name",item->name);
    cJSON_AddStringToObject(o,"root_id",c->root_id);cJSON_AddStringToObject(o,"path",target);
    cJSON_AddStringToObject(o,"md5",item->md5);cJSON_AddStringToObject(o,"modified",item->modified);
    cJSON_AddNumberToObject(o,"size",(double)item->size);
    char *json=cJSON_PrintUnformatted(o);cJSON_Delete(o);
    if(!json)return failure(e,FL_ERR_LIMIT,"Not enough memory for receipt.");
    int rc=fl_write_atomic(path,json,strlen(json),e);free(json);return rc;
}
