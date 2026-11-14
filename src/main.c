#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include "fl_core.h"
#include "fl_backend.h"
#include "fl_download.h"
#include "fl_ui.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
    FlListing listing;
    char folder[FL_ID],title[FL_NAME];
    const char **rows,**markers;
    char (*labels)[96];
    size_t selected,scroll;
} Frame;
typedef enum { SCREEN_LIST,SCREEN_COLLECTIONS,SCREEN_DETAIL,SCREEN_REPLACE,SCREEN_DOWNLOAD,SCREEN_MESSAGE,SCREEN_DISCONNECT,SCREEN_SETUP } Screen;
typedef struct {
    FlConfig config;FlSystems systems;FlUi *ui;FlBackend backend;FlDownload download;FlProcess hash;
    Frame frames[3];unsigned level;Screen screen,return_screen;
    bool loading,refresh,offline,demo,quit,dirty,hash_active,disconnected;
    unsigned load_level;FlError error;FlItem selected;const FlSystem *system;FlLocal local;
    char friend_name[FL_NAME],destination[FL_PATH],message[256],notice[256],executable[FL_PATH];
    uint64_t available;
    struct timespec last_paint;
    FlListing all_games,pending_base;
    char console_id[FL_ID],favorites_id[FL_ID];
    unsigned console_phase,collection;
    size_t game_selected[2],game_scroll[2];
    Screen console_return;
    FlError pending_warning;
} App;
static volatile sig_atomic_t shutdown_requested=0;
static int frame_rows(App *a,Frame *f,unsigned level);
static void console_accept(App *a,FlListing *part,FlError *warning);
static void request_shutdown(int sig){(void)sig;shutdown_requested=1;}
static void bytes_text(uint64_t bytes,char out[48]) {
    static const char *units[]={"B","KiB","MiB","GiB","TiB"};unsigned unit=0;double n=(double)bytes;
    while(n>=1024&&unit<4){n/=1024;unit++;}snprintf(out,48,unit?"%.1f %s":"%.0f %s",n,units[unit]);
}
static void frame_free(Frame *f){fl_listing_free(&f->listing);free(f->rows);free(f->markers);free(f->labels);memset(f,0,sizeof(*f));}
static void message(App *a,const char *text,FlCode code,Screen back) {
    a->return_screen=back;a->screen=SCREEN_MESSAGE;snprintf(a->message,sizeof(a->message),"%s",text);fl_error(&a->error,code,text);a->dirty=true;
}
static void library_clear(App *a){
    fl_listing_free(&a->all_games);fl_listing_free(&a->pending_base);
    a->console_phase=0;a->favorites_id[0]=0;a->console_id[0]=0;a->collection=0;
    memset(a->game_selected,0,sizeof(a->game_selected));memset(a->game_scroll,0,sizeof(a->game_scroll));
}
static int open_collection(App *a,unsigned collection){
    FlListing view={0};
    if(fl_library_view(&a->all_games,collection==1,&view,&a->error))return -1;
    Frame replacement={0};replacement.listing=view;
    snprintf(replacement.folder,sizeof(replacement.folder),"%s",a->console_id);
    snprintf(replacement.title,sizeof(replacement.title),"%.350s / %s / %s",a->friend_name,a->system->label,collection?"Favorites":"All");
    replacement.selected=a->game_selected[collection];replacement.scroll=a->game_scroll[collection];
    if(frame_rows(a,&replacement,2)){frame_free(&replacement);fl_error(&a->error,FL_ERR_LIMIT,"Not enough memory to display games.");return -1;}
    frame_free(&a->frames[2]);a->frames[2]=replacement;a->collection=collection;a->level=2;a->screen=SCREEN_LIST;a->dirty=true;
    return 0;
}
static bool network_available(void) {
    struct ifaddrs *list=NULL;bool found=false;
    if(getifaddrs(&list))return true; /* Unknown state: let the backend report the error. */
    for(struct ifaddrs *p=list;p;p=p->ifa_next)if(p->ifa_addr&&p->ifa_addr->sa_family==AF_INET&&strcmp(p->ifa_name,"lo")){
        const struct sockaddr_in *address=(const struct sockaddr_in*)p->ifa_addr;
        if(address->sin_addr.s_addr)found=true;
    }
    freeifaddrs(list);return found;
}
static int frame_rows(App *a,Frame *f,unsigned level) {
    size_t count=f->listing.count;
    f->rows=calloc(count?count:1,sizeof(*f->rows));f->markers=calloc(count?count:1,sizeof(*f->markers));
    if(level==1)f->labels=calloc(count?count:1,sizeof(*f->labels));
    if(!f->rows||!f->markers||(level==1&&!f->labels))return -1;
    for(size_t i=0;i<count;i++) {
        f->rows[i]=f->listing.items[i].name;
        if(level==1){const FlSystem *s=fl_system_find(&a->systems,f->listing.items[i].name);
            if(s){snprintf(f->labels[i],96,"%s",s->label);f->rows[i]=f->labels[i];}
            else f->markers[i]="UNSUPPORTED";
        }
    }
    if(f->selected>=count)f->selected=count?count-1:0;
    if(f->scroll>f->selected)f->scroll=f->selected;
    return 0;
}
static int demo_listing(unsigned level,FlListing *l,FlError *e) {
    static const char *json[]={
      "[{\"ID\":\"friend_demo\",\"Name\":\"DEMO FRIEND\",\"IsDir\":true,\"Size\":-1}]",
      "[{\"ID\":\"console_nes\",\"Name\":\"FC\",\"IsDir\":true,\"Size\":-1},{\"ID\":\"console_nds\",\"Name\":\"NDS\",\"IsDir\":true,\"Size\":-1},{\"ID\":\"console_ps\",\"Name\":\"PS\",\"IsDir\":true,\"Size\":-1}]",
      "[{\"ID\":\"demo_game_01\",\"Name\":\"Example Homebrew.nes\",\"IsDir\":false,\"Size\":262144},{\"ID\":\"demo_favorites\",\"Name\":\"Favorites\",\"IsDir\":true,\"Size\":-1}]"};
    return fl_listing_parse(json[level],l,e);
}
static void load_folder(App *a,unsigned level,const char *id,bool force) {
    a->load_level=level;a->refresh=force;a->dirty=true;
    if(level==2){
        a->console_return=(a->level==2&&a->screen==SCREEN_LIST)?SCREEN_LIST:SCREEN_COLLECTIONS;
        if(a->level==2&&a->screen==SCREEN_LIST){a->game_selected[a->collection]=a->frames[2].selected;a->game_scroll[a->collection]=a->frames[2].scroll;}
        fl_listing_free(&a->pending_base);a->console_phase=0;memset(&a->pending_warning,0,sizeof(a->pending_warning));
        snprintf(a->console_id,sizeof(a->console_id),"%s",id);
    }
    if(a->demo&&level==2){FlListing list={0};FlError e={0};if(demo_listing(2,&list,&e))message(a,e.message,e.code,SCREEN_LIST);else console_accept(a,&list,&e);return;}
    if(a->demo){Frame *f=&a->frames[level];size_t selected=f->selected,scroll=f->scroll;char title[FL_NAME];snprintf(title,sizeof(title),"%s",f->title);
        frame_free(f);snprintf(f->folder,sizeof(f->folder),"%s",id);snprintf(f->title,sizeof(f->title),"%s",title);f->selected=selected;f->scroll=scroll;
        if(demo_listing(level,&f->listing,&a->error)||frame_rows(a,f,level)){message(a,"Cannot create demo listing.",FL_ERR_LIMIT,SCREEN_LIST);return;}
        a->level=level;a->screen=SCREEN_LIST;snprintf(a->notice,sizeof(a->notice),"DEMO: sample metadata; Google Drive is not connected.");return;}
    if(fl_backend_list_start(&a->backend,&a->config,id,force,&a->error)){
        message(a,a->error.message,a->error.code,a->error.code==FL_ERR_AUTH_REQUIRED?SCREEN_SETUP:SCREEN_LIST);return;
    }
    a->loading=true;a->screen=SCREEN_LIST;
}
static void console_abort(App *a,FlError *error){
    fl_listing_free(&a->pending_base);a->loading=false;a->console_phase=0;
    message(a,error->message,error->code,a->level==2?a->console_return:SCREEN_LIST);
}
static void console_request(App *a,const char *id,unsigned phase){
    a->console_phase=phase;
    if(a->demo){
        FlListing list={0};FlError e={0};
        const char *json="[{\"ID\":\"demo_game_02\",\"Name\":\"Another Homebrew.nes\",\"IsDir\":false,\"Size\":524288}]";
        if(fl_listing_parse(json,&list,&e))console_abort(a,&e);else console_accept(a,&list,&e);
        return;
    }
    FlError e={0};
    if(fl_backend_list_start(&a->backend,&a->config,id,a->refresh,&e)){console_abort(a,&e);return;}
    a->loading=true;a->dirty=true;
}
static void console_accept(App *a,FlListing *part,FlError *warning){
    FlError e={0};
    if(warning->code==FL_ERR_CANCELLED){fl_listing_free(part);console_abort(a,warning);return;}
    if(warning->code)a->pending_warning=*warning;
    if(a->console_phase==0){
        if(fl_favorites_folder(part,a->favorites_id,&e)<0){fl_listing_free(part);console_abort(a,&e);return;}
        a->pending_base=*part;memset(part,0,sizeof(*part));
        if(*a->favorites_id){console_request(a,a->favorites_id,2);return;}
    }
    FlListing combined={0},empty={0};
    const FlListing *favorites=a->console_phase==2?part:&empty;
    if(fl_library_merge(&a->pending_base,favorites,&combined,&e)){fl_listing_free(part);console_abort(a,&e);return;}
    fl_listing_free(part);fl_listing_free(&a->pending_base);
    fl_listing_free(&a->all_games);a->all_games=combined;a->loading=false;a->console_phase=0;
    a->offline=a->pending_warning.code==FL_ERR_OFFLINE||a->pending_warning.code==FL_ERR_TIMEOUT||(!a->demo&&!network_available());
    if(a->pending_warning.code)snprintf(a->notice,sizeof(a->notice),"Cached library: %.210s",a->pending_warning.message);
    else if(a->demo)snprintf(a->notice,sizeof(a->notice),"DEMO: metadata only; Google Drive is not connected.");
    else if(combined.from_cache)snprintf(a->notice,sizeof(a->notice),"Cached library. X refreshes console and optional collections.");
    else snprintf(a->notice,sizeof(a->notice),"All includes Favorites. X refreshes this console.");
    a->level=2;a->screen=SCREEN_COLLECTIONS;a->dirty=true;
    if(a->console_return==SCREEN_LIST&&open_collection(a,a->collection))message(a,a->error.message,a->error.code,SCREEN_COLLECTIONS);
    Frame *consoles=&a->frames[1];size_t files=0;
    for(size_t i=0;i<combined.count;i++)if(!combined.items[i].is_dir&&fl_extension_allowed(a->system,combined.items[i].name))files++;
    if(consoles->labels&&consoles->selected<consoles->listing.count)snprintf(consoles->labels[consoles->selected],96,"%s (%zu)",a->system->label,files);
}
static void pump_backend(App *a) {
    FlError error={0};int result=fl_backend_poll(&a->backend,&error);if(!result)return;
    a->loading=false;
    if(a->backend.disconnect){
        if(error.code)message(a,error.message,error.code,SCREEN_SETUP);
        else {a->disconnected=true;for(unsigned i=0;i<3;i++)frame_free(&a->frames[i]);library_clear(a);
            message(a,a->backend.revoke_failed?"Credentials deleted. Online revocation was not confirmed; remove app access in your Google account.":"Account disconnected and local credentials deleted.",FL_OK,SCREEN_SETUP);}
        fl_backend_destroy(&a->backend);return;
    }
    if(result<0){fl_backend_destroy(&a->backend);if(a->load_level==2)console_abort(a,&error);else message(a,error.message,error.code,SCREEN_LIST);return;}
    FlListing list={0};
    if(fl_backend_take_list(&a->backend,&list,&error)){message(a,"Could not read the library listing.",FL_ERR_IO,SCREEN_LIST);return;}
    if(a->load_level==2){fl_backend_destroy(&a->backend);console_accept(a,&list,&error);return;}
    unsigned level=a->load_level;Frame *f=&a->frames[level];size_t old_index=f->selected,old_scroll=f->scroll;char selected_id[FL_ID]="";
    if(old_index<f->listing.count)snprintf(selected_id,sizeof(selected_id),"%s",f->listing.items[old_index].id);
    char folder[FL_ID],title[FL_NAME];snprintf(folder,sizeof(folder),"%s",a->backend.folder_id);snprintf(title,sizeof(title),"%s",f->title);
    frame_free(f);f->listing=list;snprintf(f->folder,sizeof(f->folder),"%s",folder);snprintf(f->title,sizeof(f->title),"%s",title);
    if(level<2)fl_listing_directories(&f->listing);
    f->selected=old_index;f->scroll=old_scroll;
    for(size_t i=0;i<f->listing.count;i++)if(!strcmp(f->listing.items[i].id,selected_id)){f->selected=i;break;}
    if(frame_rows(a,f,level)){message(a,"Not enough memory to display the folder.",FL_ERR_LIMIT,SCREEN_LIST);fl_backend_destroy(&a->backend);return;}
    a->level=level;a->screen=SCREEN_LIST;
    if(level==2&&a->system){
        Frame *consoles=&a->frames[1];size_t files=0;
        for(size_t i=0;i<f->listing.count;i++)if(!f->listing.items[i].is_dir&&fl_extension_allowed(a->system,f->listing.items[i].name))files++;
        if(consoles->labels&&consoles->selected<consoles->listing.count)
            snprintf(consoles->labels[consoles->selected],96,"%s (%zu)",a->system->label,files);
    }
    a->offline=error.code==FL_ERR_OFFLINE||error.code==FL_ERR_TIMEOUT||!network_available();
    if(error.code)snprintf(a->notice,sizeof(a->notice),"Cached library: %.210s",error.message);
    else if(f->listing.from_cache){char date[64];struct tm t;localtime_r(&f->listing.fetched,&t);strftime(date,sizeof(date),"%Y-%m-%d %H:%M",&t);snprintf(a->notice,sizeof(a->notice),"Cached %s. X: refresh%s",date,a->offline?" | Wi-Fi unavailable":"");}
    else snprintf(a->notice,sizeof(a->notice),"%zu items. X: refresh%s",f->listing.count,a->offline?" | Wi-Fi unavailable":"");
    fl_backend_destroy(&a->backend);a->dirty=true;
}
static void selected_game(App *a) {
    Frame *f=&a->frames[2];if(f->selected>=f->listing.count)return;
    a->selected=f->listing.items[f->selected];a->local=FL_LOCAL_NONE;a->available=0;a->destination[0]=0;
    if(a->selected.is_dir){message(a,"Nested game folders require a complete-file layout. See the library preparation guide.",FL_ERR_UNSUPPORTED,SCREEN_LIST);return;}
    const FlListing *collision_list=a->all_games.items?&a->all_games:&f->listing;size_t collision_index=f->selected;
    if(a->all_games.items)for(size_t i=0;i<a->all_games.count;i++)if(!strcmp(a->all_games.items[i].id,a->selected.id)){collision_index=i;break;}
    if(fl_listing_collision(collision_list,collision_index)){message(a,"Ambiguous destination: multiple library files map to the same filename. Rename them in Drive.",FL_ERR_EXISTS,SCREEN_LIST);return;}
    if(!fl_extension_allowed(a->system,a->selected.name)){message(a,"Unsupported ROM format. PS requires a complete single-file CHD/PBP; multifile sets are excluded in V1.",FL_ERR_FORMAT,SCREEN_LIST);return;}
    if(fl_rom_target(&a->config,a->system,a->selected.name,true,a->destination,&a->error)){message(a,a->error.message,a->error.code,SCREEN_LIST);return;}
    char directory[FL_PATH];snprintf(directory,sizeof(directory),"%s",a->destination);*strrchr(directory,'/')=0;
    (void)fl_space_check(directory,a->selected.size,&a->available,&a->error);
    a->local=fl_local_check(&a->config,a->system,&a->selected,false,NULL);
    if(a->local!=FL_LOCAL_NONE&&*a->selected.md5){const char *argv[]={a->executable,"--verify",a->destination,NULL};
        if(!fl_process_start(&a->hash,argv,128,NULL))a->hash_active=true;
    }
    a->screen=SCREEN_DETAIL;a->dirty=true;
}
static void start_download(App *a,bool replace) {
    if(a->demo){message(a,"Demo contains metadata only. Configure your account to download.",FL_ERR_CONFIG,SCREEN_DETAIL);return;}
    if(a->offline||!network_available()){a->offline=true;message(a,"Wi-Fi is unavailable. Connect Wi-Fi, then press X to refresh the library.",FL_ERR_OFFLINE,SCREEN_DETAIL);return;}
    if(a->hash_active){fl_process_destroy(&a->hash);a->hash_active=false;}
    if(fl_download_start(&a->download,&a->config,a->system,&a->selected,replace,&a->error)){
        message(a,a->error.message,a->error.code,SCREEN_DETAIL);return;
    }
    a->screen=SCREEN_DOWNLOAD;a->dirty=true;
}
static void update_markers(App *a,Frame *f) {
    if(a->level!=2)return;
    size_t end=f->scroll+FL_UI_VISIBLE_ROWS;if(end>f->listing.count)end=f->listing.count;
    for(size_t i=f->scroll;i<end;i++) {
        FlItem *item=&f->listing.items[i];f->markers[i]="";
        if(item->is_dir)f->markers[i]="FOLDER";
        else if(!fl_extension_allowed(a->system,item->name))f->markers[i]="FORMAT";
        else { /* Display-only stat; definitive safe path and identity checks occur on selection. */
            char clean[FL_NAME],path[FL_PATH];struct stat st;
            if(!fl_sanitize_filename(item->name,clean,NULL)&&snprintf(path,sizeof(path),"%s/Roms/%s/%s",a->config.sd_root,a->system->id,clean)<(int)sizeof(path)&&!lstat(path,&st)&&S_ISREG(st.st_mode))
                f->markers[i]=(int64_t)st.st_size==item->size?"PRESENT":"DIFFERENT";
        }
    }
}
static void draw(App *a) {
    Frame *f=&a->frames[a->level];FlView v={0};char lines[9][FL_PATH];const char *details[9];
    char collection_labels[2][96],collection_title[FL_NAME];const char *collection_rows[2]={collection_labels[0],collection_labels[1]};
    for(unsigned i=0;i<9;i++){lines[i][0]=0;details[i]=lines[i];}
    v.title="FRIENDS LIBRARY";v.footer="A: OPEN   B: BACK   X: REFRESH";v.status=a->notice;
    if(a->screen==SCREEN_COLLECTIONS){
        size_t all=0,favorites=0;
        for(size_t i=0;i<a->all_games.count;i++)if(!a->all_games.items[i].is_dir&&fl_extension_allowed(a->system,a->all_games.items[i].name)){all++;if(a->all_games.items[i].favorite)favorites++;}
        snprintf(collection_labels[0],96,"Favorites (%zu)",favorites);snprintf(collection_labels[1],96,"All (%zu)",all);
        snprintf(collection_title,sizeof(collection_title),"%.390s / %s",a->friend_name,a->system->label);
        v.subtitle=collection_title;v.rows=collection_rows;v.row_count=2;v.selected=1-a->collection;
        if(!*a->favorites_id&&!a->pending_warning.code)v.status="Favorites is optional. Games work without collection folders.";
    }else if(a->screen==SCREEN_LIST){
        v.subtitle=a->level?f->title:"PRIVATE GOOGLE DRIVE LIBRARY";v.rows=f->rows;v.row_count=f->listing.count;v.markers=f->markers;v.selected=f->selected;v.scroll=f->scroll;
        if(!a->level)v.footer="A: OPEN  B: EXIT  X: REFRESH  SELECT: ACCOUNT";
        if(a->level==2)v.footer="A: DETAILS   B: BACK   X: REFRESH";
        if(a->loading){v.busy=true;v.status="Loading library. B cancels the request.";}
        else update_markers(a,f);
        if(a->level==2&&a->collection==1&&!f->listing.count&&!a->loading){
            snprintf(lines[0],sizeof(lines[0]),"No favorites yet.");snprintf(lines[2],sizeof(lines[2]),"Optional Drive folder: Favorites/");
            snprintf(lines[3],sizeof(lines[3]),"Move selected ROMs there in Google Drive.");v.details=details;v.detail_count=4;
            if(!a->pending_warning.code)v.status="You can keep using All without creating a Favorites folder.";
        }
    } else if(a->screen==SCREEN_DETAIL||a->screen==SCREEN_REPLACE){
        char size[48],free_space[48];bytes_text(a->selected.size>=0?(uint64_t)a->selected.size:0,size);bytes_text(a->available,free_space);
        v.subtitle=a->screen==SCREEN_REPLACE?"CONFIRM REPLACEMENT":"DOWNLOAD DETAILS";
        snprintf(lines[0],sizeof(lines[0]),"%s",a->selected.name);
        snprintf(lines[1],sizeof(lines[1]),"Console: %s",a->system->label);
        snprintf(lines[2],sizeof(lines[2]),"Shared by: %s",a->friend_name);
        snprintf(lines[3],sizeof(lines[3]),"Size: %s    Free: %s",size,free_space);
        snprintf(lines[4],sizeof(lines[4]),"Destination: Roms/%s/",a->system->id);
        snprintf(lines[5],sizeof(lines[5]),"Existing: %s",a->hash_active?"Checking checksum...":a->local==FL_LOCAL_VERIFIED?"Verified installed":a->local==FL_LOCAL_PRESENT?"Same name and size (unverified)":a->local==FL_LOCAL_CHANGED?"Different content or size":"No");
        snprintf(lines[6],sizeof(lines[6]),"%s",a->screen==SCREEN_REPLACE?"The existing ROM will be replaced.":a->selected.md5[0]?"Download will be checksum verified.":"Checksum unavailable; size will be verified.");
        v.details=details;v.detail_count=7;v.footer=a->screen==SCREEN_REPLACE?"A: REPLACE EXISTING ROM   B: CANCEL":"A: DOWNLOAD   B: BACK";
        v.status=a->offline?"Offline. Return to the list and refresh after connecting Wi-Fi.":"Onion handles game launching and save data.";
        v.error=a->screen==SCREEN_REPLACE;
    } else if(a->screen==SCREEN_DOWNLOAD){
        char done[48],total[48],speed[48];bytes_text(a->download.downloaded>0?(uint64_t)a->download.downloaded:0,done);bytes_text(a->download.total>0?(uint64_t)a->download.total:0,total);bytes_text(a->download.bytes_per_second>0?(uint64_t)a->download.bytes_per_second:0,speed);
        v.subtitle=a->download.cancelling?"CANCELLING":"DOWNLOADING";
        snprintf(lines[0],sizeof(lines[0]),"%s",a->selected.name);
        snprintf(lines[2],sizeof(lines[2]),"%s / %s",done,total);snprintf(lines[3],sizeof(lines[3]),"Speed: %s/s",speed);
        double p=a->download.total>0?(double)a->download.downloaded/a->download.total:-1;if(p>1)p=1;
        if(p>=0)snprintf(lines[4],sizeof(lines[4]),"%.0f%% transferred",p*100);else snprintf(lines[4],sizeof(lines[4]),"Waiting for file data...");
        v.details=details;v.detail_count=5;v.progress=true;v.progress_fraction=p;v.footer="B: CANCEL DOWNLOAD";
        v.status=p>=1?"Verifying file and saving to SD card. Please wait.":"Existing ROM is preserved until the new file is verified.";
    } else if(a->screen==SCREEN_DISCONNECT){
        v.subtitle="DISCONNECT GOOGLE ACCOUNT";snprintf(lines[0],sizeof(lines[0]),"Delete this device's stored credentials?");snprintf(lines[2],sizeof(lines[2]),"Online authorization revocation will be tried.");snprintf(lines[3],sizeof(lines[3]),"Downloaded games will remain on the SD card.");v.details=details;v.detail_count=4;v.footer="A: DISCONNECT   B: CANCEL";v.error=true;v.status="Each friend uses their own Google authorization.";
    } else if(a->screen==SCREEN_SETUP){
        v.subtitle="INITIAL SETUP REQUIRED";snprintf(lines[0],sizeof(lines[0]),"Authorize your account on a computer.");snprintf(lines[2],sizeof(lines[2]),"Use scripts/setup-account.ps1 from the source");snprintf(lines[3],sizeof(lines[3]),"or follow the included authentication guide.");snprintf(lines[5],sizeof(lines[5]),"Settings: .config/friends-library/");v.details=details;v.detail_count=6;v.footer="X: RELOAD SETUP   B: EXIT";v.status=a->message[0]?a->message:"See README and docs/AUTHENTICATION.md.";
    } else {
        v.subtitle=a->error.code==FL_OK?"COMPLETED":"NOTICE";snprintf(lines[0],sizeof(lines[0]),"%s",a->error.code==FL_OK?"Operation completed.":fl_code_name(a->error.code));
        v.details=details;v.detail_count=1;v.error=a->error.code!=FL_OK;v.status=a->message;v.footer="A / B: RETURN";
    }
    if(fl_ui_draw(a->ui,&v,&a->error))a->quit=true;
    clock_gettime(CLOCK_MONOTONIC,&a->last_paint);
    a->dirty=false;
}
static void move_selection(Frame *f,int change) {
    if(!f->listing.count)return;
    long next=(long)f->selected+change;if(next<0)next=0;if(next>=(long)f->listing.count)next=(long)f->listing.count-1;f->selected=(size_t)next;
    if(f->selected<f->scroll)f->scroll=f->selected;
    if(f->selected>=f->scroll+FL_UI_VISIBLE_ROWS)f->scroll=f->selected-FL_UI_VISIBLE_ROWS+1;
}
static void handle_key(App *a,FlKey key) {
    if(key==FL_KEY_NONE)return;
    a->dirty=true;
    if(key==FL_KEY_EXIT){a->quit=true;return;}
    if(a->loading){if(key==FL_KEY_BACK){fl_backend_cancel(&a->backend);}return;}
    if(a->screen==SCREEN_DOWNLOAD){if(key==FL_KEY_BACK)fl_download_cancel(&a->download);return;}
    if(a->screen==SCREEN_MESSAGE){if(key==FL_KEY_OPEN||key==FL_KEY_BACK){a->screen=a->return_screen;if(a->screen==SCREEN_DETAIL)a->local=fl_local_check(&a->config,a->system,&a->selected,false,NULL);}return;}
    if(a->screen==SCREEN_SETUP){if(key==FL_KEY_BACK)a->quit=true;else if(key==FL_KEY_REFRESH){
        char sd[FL_PATH],appdir[FL_PATH];snprintf(sd,sizeof(sd),"%s",a->config.sd_root);snprintf(appdir,sizeof(appdir),"%s",a->config.app_dir);
        if(fl_config_load(sd,appdir,&a->config,&a->error))snprintf(a->message,sizeof(a->message),"%s",a->error.message);
        else {a->level=0;load_folder(a,0,a->config.root_id,true);}}
        return;}
    if(a->screen==SCREEN_DISCONNECT){if(key==FL_KEY_BACK)a->screen=SCREEN_LIST;else if(key==FL_KEY_OPEN){
        if(a->demo){message(a,"Demo has no stored account.",FL_OK,SCREEN_LIST);return;}
        if(fl_backend_disconnect_start(&a->backend,&a->config,true,&a->error))message(a,a->error.message,a->error.code,SCREEN_LIST);
        else {a->loading=true;a->screen=SCREEN_LIST;}}
        return;}
    if(a->screen==SCREEN_REPLACE){if(key==FL_KEY_BACK)a->screen=SCREEN_DETAIL;else if(key==FL_KEY_OPEN)start_download(a,true);return;}
    if(a->screen==SCREEN_DETAIL){if(key==FL_KEY_BACK){fl_process_destroy(&a->hash);a->hash_active=false;a->screen=SCREEN_LIST;}
        else if(key==FL_KEY_OPEN){if(a->local!=FL_LOCAL_NONE)a->screen=SCREEN_REPLACE;else start_download(a,false);}
        return;}
    if(a->screen==SCREEN_COLLECTIONS){
        if(key==FL_KEY_UP||key==FL_KEY_DOWN)a->collection=1-a->collection;
        else if(key==FL_KEY_BACK){a->level=1;a->screen=SCREEN_LIST;}
        else if(key==FL_KEY_OPEN){if(open_collection(a,a->collection))message(a,a->error.message,a->error.code,SCREEN_COLLECTIONS);}
        else if(key==FL_KEY_REFRESH){char id[FL_ID];snprintf(id,sizeof(id),"%s",a->console_id);load_folder(a,2,id,true);}
        return;
    }
    Frame *f=&a->frames[a->level];
    if(key==FL_KEY_UP)move_selection(f,-1);else if(key==FL_KEY_DOWN)move_selection(f,1);
    else if(key==FL_KEY_PAGE_UP||key==FL_KEY_LEFT)move_selection(f,-FL_UI_VISIBLE_ROWS);
    else if(key==FL_KEY_PAGE_DOWN||key==FL_KEY_RIGHT)move_selection(f,FL_UI_VISIBLE_ROWS);
    else if(key==FL_KEY_BACK){if(a->level==2){a->game_selected[a->collection]=f->selected;a->game_scroll[a->collection]=f->scroll;a->screen=SCREEN_COLLECTIONS;}else if(a->level)a->level--;else a->quit=true;}
    else if(key==FL_KEY_DISCONNECT&&a->level==0)a->screen=SCREEN_DISCONNECT;
    else if(key==FL_KEY_REFRESH){char id[FL_ID];snprintf(id,sizeof(id),"%s",f->folder[0]?f->folder:a->config.root_id);load_folder(a,a->level,id,true);}
    else if(key==FL_KEY_OPEN&&f->listing.count){FlItem *item=&f->listing.items[f->selected];
        if(a->level==0){snprintf(a->friend_name,sizeof(a->friend_name),"%s",item->name);frame_free(&a->frames[1]);frame_free(&a->frames[2]);library_clear(a);snprintf(a->frames[1].title,sizeof(a->frames[1].title),"%s",item->name);load_folder(a,1,item->id,false);}
        else if(a->level==1){a->system=fl_system_find(&a->systems,item->name);if(!a->system){message(a,"This console folder has no verified OnionOS mapping.",FL_ERR_UNSUPPORTED,SCREEN_LIST);return;}
            frame_free(&a->frames[2]);library_clear(a);snprintf(a->frames[2].title,sizeof(a->frames[2].title),"%.390s / %s",a->friend_name,a->system->label);load_folder(a,2,item->id,false);}
        else selected_game(a);
    }
}
int main(int argc,char **argv) {
    App app={0};char sd[FL_PATH]="/mnt/SDCARD",appdir[FL_PATH],systems_path[FL_PATH],screenshot[FL_PATH]="";int lock=-1;bool probe=false;const char *verify=NULL;
    if(!getcwd(appdir,sizeof(appdir))){fprintf(stderr,"Cannot determine app directory.\n");return 1;}
    static const struct option options[]={{"sd-root",required_argument,0,'s'},{"app-dir",required_argument,0,'a'},{"demo",no_argument,0,'d'},{"screenshot",required_argument,0,'p'},{"verify",required_argument,0,'v'},{"probe",no_argument,0,'t'},{"help",no_argument,0,'h'},{0,0,0,0}};
    int opt;while((opt=getopt_long(argc,argv,"",options,NULL))!=-1){switch(opt){case 's':snprintf(sd,sizeof(sd),"%s",optarg);break;case 'a':snprintf(appdir,sizeof(appdir),"%s",optarg);break;case 'd':app.demo=true;break;case 'p':snprintf(screenshot,sizeof(screenshot),"%s",optarg);break;case 'v':verify=optarg;break;case 't':probe=true;break;default:puts("Friends Library " FL_VERSION "\n--sd-root PATH --app-dir PATH [--demo] [--screenshot FILE] [--probe]");return opt=='h'?0:1;}}
    if(verify){char md5[33];if(fl_md5_file(verify,md5,NULL))return 1;puts(md5);return 0;}
    if(probe){printf("{\"version\":\"%s\",\"off_t_bits\":%zu,\"target\":\"OnionOS 4.3 / SDL1.2\",\"hardware_tested\":false}\n",FL_VERSION,sizeof(off_t)*8);return 0;}
    ssize_t executable_len=readlink("/proc/self/exe",app.executable,sizeof(app.executable)-1);
    if(executable_len>0)app.executable[executable_len]=0;else if(!realpath(argv[0],app.executable)){fprintf(stderr,"Cannot locate own executable.\n");return 1;}
    bool configured=fl_config_load(sd,appdir,&app.config,&app.error)==0;
    if(!configured){/* config_load retains path defaults when settings are missing. */snprintf(app.message,sizeof(app.message),"%s",app.error.message);app.screen=SCREEN_SETUP;}
    if(app.demo){configured=true;snprintf(app.config.sd_root,sizeof(app.config.sd_root),"%s",sd);snprintf(app.config.app_dir,sizeof(app.config.app_dir),"%s",appdir);snprintf(app.config.root_id,sizeof(app.config.root_id),"demo_root");app.screen=SCREEN_LIST;}
    if(snprintf(systems_path,sizeof(systems_path),"%s/config/systems.json",appdir)>=(int)sizeof(systems_path)||fl_systems_load(systems_path,&app.systems,&app.error)){fprintf(stderr,"Cannot load console mapping: %s\n",app.error.message);return 1;}
    if(!app.demo){
        if(fl_mkdirs(app.config.data_dir,&app.error)){fprintf(stderr,"Cannot access app data: %s\n",app.error.message);fl_systems_free(&app.systems);return 1;}
        char lockpath[FL_PATH];
        if(snprintf(lockpath,sizeof(lockpath),"%s/app.lock",app.config.data_dir)>=(int)sizeof(lockpath)){fprintf(stderr,"App data path is too long.\n");fl_systems_free(&app.systems);return 1;}
        lock=open(lockpath,O_CREAT|O_RDWR|O_NOFOLLOW,0600);
        if(lock<0||flock(lock,LOCK_EX|LOCK_NB)){fprintf(stderr,"Friends Library is already running or app data is not writable.\n");if(lock>=0)close(lock);fl_systems_free(&app.systems);return 1;}fcntl(lock,F_SETFD,FD_CLOEXEC);
        if(fl_download_recover(&app.config,&app.systems,&app.error))snprintf(app.notice,sizeof(app.notice),"%s",app.error.message);
    }
    struct sigaction action;memset(&action,0,sizeof(action));action.sa_handler=request_shutdown;sigemptyset(&action.sa_mask);sigaction(SIGTERM,&action,NULL);sigaction(SIGINT,&action,NULL);signal(SIGPIPE,SIG_IGN);
    app.ui=fl_ui_open(&app.error);if(!app.ui){fprintf(stderr,"Display could not start: %s\n",app.error.message);if(lock>=0)close(lock);fl_systems_free(&app.systems);return 1;}
    if(!app.demo){
        char diagnostic[512],path[FL_PATH];fl_ui_describe(app.ui,diagnostic,sizeof(diagnostic));
        if(snprintf(path,sizeof(path),"%s/display-info.txt",app.config.data_dir)<(int)sizeof(path))
            (void)fl_write_atomic(path,diagnostic,strlen(diagnostic),NULL);
    }
    if(configured)load_folder(&app,0,app.config.root_id,false);
    app.dirty=true;
    if(*screenshot){draw(&app);int rc=fl_ui_screenshot(app.ui,screenshot,&app.error);app.quit=true;if(rc)fprintf(stderr,"Preview failed: %s\n",app.error.message);}
    while(!app.quit&&!shutdown_requested){
        if(app.loading)pump_backend(&app);
        if(app.hash_active){int rc=fl_process_poll(&app.hash,NULL);if(rc){if(rc>0&&app.hash.exit_code==0){app.local=!strncasecmp(app.hash.out,app.selected.md5,32)?FL_LOCAL_VERIFIED:FL_LOCAL_CHANGED;}fl_process_destroy(&app.hash);app.hash_active=false;app.dirty=true;}}
        if(app.download.active){FlError error={0};int rc=fl_download_poll(&app.download,&error);
            struct timespec now;clock_gettime(CLOCK_MONOTONIC,&now);
            double since=(double)(now.tv_sec-app.last_paint.tv_sec)+(now.tv_nsec-app.last_paint.tv_nsec)/1e9;
            if(rc||since>=0.3)app.dirty=true;
            if(rc){
            if(rc>0){char result[256];snprintf(result,sizeof(result),"Installed. Games > SELECT > Refresh all roms. %.140s",app.download.warning);message(&app,result,FL_OK,SCREEN_LIST);}
            else {if(error.code==FL_ERR_OFFLINE||error.code==FL_ERR_TIMEOUT)app.offline=true;message(&app,error.message,error.code,SCREEN_LIST);}
        }}
        if(app.dirty)draw(&app);
        bool working=app.loading||app.download.active||app.hash_active;
        /* A finite idle wait also lets SIGTERM/INT reach orderly worker cleanup. */
        handle_key(&app,fl_ui_wait_event(app.ui,working?80:500));
    }
    fl_backend_destroy(&app.backend);fl_process_destroy(&app.hash);fl_download_destroy(&app.download);fl_ui_close(app.ui);
    for(unsigned i=0;i<3;i++)frame_free(&app.frames[i]);
    library_clear(&app);
    fl_systems_free(&app.systems);if(lock>=0)close(lock);return 0;
}
