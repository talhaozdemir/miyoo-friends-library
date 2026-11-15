#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define main fl_application_main
#include "../src/main.c"
#undef main

static unsigned checks;
#define CHECK(x) do { ++checks; if(!(x)) { fprintf(stderr,"app assertion %s:%d: %s\n",__FILE__,__LINE__,#x); exit(1); } } while(0)

static void dismiss(App *a) { CHECK(a->screen==SCREEN_MESSAGE); handle_key(a,FL_KEY_BACK); }

static void large_games(App *a) {
    /* Standalone controller fixture; collection union is tested separately. */
    fl_listing_free(&a->all_games);
    Frame *f=&a->frames[2];
    frame_free(f); f->listing.count=30; f->listing.items=calloc(30,sizeof(FlItem)); CHECK(f->listing.items!=NULL);
    snprintf(f->folder,sizeof(f->folder),"console_nes"); snprintf(f->title,sizeof(f->title),"RANDOM FRIEND 42 / NES");
    for(size_t i=0;i<30;++i) {
        FlItem *item=&f->listing.items[i];
        snprintf(item->id,sizeof(item->id),"homebrew_%02zu",i);
        snprintf(item->name,sizeof(item->name),"Homebrew %02zu.nes",i); item->size=1024;
    }
    CHECK(frame_rows(a,f,2)==0);
}

int main(int argc,char **argv) {
    App a={0}; FlError error={0}; char temp[]="/tmp/friends-app-XXXXXX",mapping[FL_PATH];
    const char *root=argc>1?argv[1]:".";
    CHECK(mkdtemp(temp)!=NULL);
    CHECK(snprintf(mapping,sizeof(mapping),"%s/package/App/FriendsLibrary/config/systems.json",root)<(int)sizeof(mapping));
    CHECK(fl_systems_load(mapping,&a.systems,&error)==0);
    snprintf(a.config.sd_root,sizeof(a.config.sd_root),"%s",temp);
    snprintf(a.config.app_dir,sizeof(a.config.app_dir),"%s",root);
    snprintf(a.config.root_id,sizeof(a.config.root_id),"demo_root");
    a.demo=true; a.screen=SCREEN_LIST;
    load_folder(&a,0,a.config.root_id,false);
    CHECK(a.level==0 && a.frames[0].listing.count==1);
    snprintf(a.frames[0].listing.items[0].name,FL_NAME,"RANDOM FRIEND 42");
    handle_key(&a,FL_KEY_OPEN);
    CHECK(a.level==1 && a.frames[1].listing.count==3);
    CHECK(!strcmp(a.friend_name,"RANDOM FRIEND 42"));
    CHECK(!strcmp(a.frames[1].rows[0],"Nintendo Entertainment System") || !strcmp(a.frames[1].rows[0],"NES"));
    handle_key(&a,FL_KEY_OPEN);
    CHECK(a.screen==SCREEN_COLLECTIONS && a.all_games.count==2);
    handle_key(&a,FL_KEY_OPEN);
    CHECK(a.level==2 && a.frames[2].listing.count==2); CHECK(a.system && !strcmp(a.system->id,"FC"));
    large_games(&a);
    for(unsigned i=0;i<20;++i)handle_key(&a,FL_KEY_DOWN);
    CHECK(a.frames[2].selected==20); CHECK(a.frames[2].scroll>0);
    size_t selected=a.frames[2].selected,scroll=a.frames[2].scroll;
    handle_key(&a,FL_KEY_OPEN); CHECK(a.screen==SCREEN_DETAIL); CHECK(!strcmp(a.selected.id,"homebrew_20"));
    CHECK(strstr(a.destination,"/Roms/FC/Homebrew 20.nes")!=NULL);
    handle_key(&a,FL_KEY_BACK);
    CHECK(a.screen==SCREEN_LIST && a.level==2); CHECK(a.frames[2].selected==selected && a.frames[2].scroll==scroll);
    handle_key(&a,FL_KEY_PAGE_DOWN); CHECK(a.frames[2].selected==28);
    handle_key(&a,FL_KEY_PAGE_UP); CHECK(a.frames[2].selected==20);
    /* Complete-file format policy and unknown consoles remain explicit errors. */
    snprintf(a.frames[2].listing.items[20].name,FL_NAME,"Incomplete.cue");
    handle_key(&a,FL_KEY_OPEN); CHECK(a.error.code==FL_ERR_FORMAT); dismiss(&a); CHECK(a.level==2);
    snprintf(a.frames[2].listing.items[20].name,FL_NAME,"Homebrew 20.nes");
    a.frames[2].listing.items[20].is_dir=true;
    handle_key(&a,FL_KEY_OPEN); CHECK(a.error.code==FL_ERR_UNSUPPORTED); dismiss(&a);
    a.frames[2].listing.items[20].is_dir=false;
    snprintf(a.frames[2].listing.items[21].name,FL_NAME,"Homebrew 20.nes");
    handle_key(&a,FL_KEY_OPEN); CHECK(a.error.code==FL_ERR_EXISTS); dismiss(&a);
    snprintf(a.frames[2].listing.items[21].name,FL_NAME,"Homebrew 21.nes");
    handle_key(&a,FL_KEY_OPEN); CHECK(a.screen==SCREEN_DETAIL);
    a.local=FL_LOCAL_PRESENT;
    handle_key(&a,FL_KEY_OPEN); CHECK(a.screen==SCREEN_REPLACE);
    handle_key(&a,FL_KEY_BACK); CHECK(a.screen==SCREEN_DETAIL);
    /* No network/download subprocess starts while the controller knows it is offline. */
    a.demo=false; a.offline=true; a.local=FL_LOCAL_NONE;
    handle_key(&a,FL_KEY_OPEN); CHECK(a.error.code==FL_ERR_OFFLINE); CHECK(!a.download.active);
    dismiss(&a); CHECK(a.screen==SCREEN_DETAIL); a.demo=true;
    handle_key(&a,FL_KEY_BACK); CHECK(a.screen==SCREEN_LIST && a.frames[2].selected==20);
    handle_key(&a,FL_KEY_BACK); CHECK(a.screen==SCREEN_COLLECTIONS && a.level==2);
    handle_key(&a,FL_KEY_BACK); CHECK(a.level==1 && a.frames[1].selected==0);
    snprintf(a.frames[1].listing.items[0].name,FL_NAME,"UNVERIFIED CONSOLE");
    handle_key(&a,FL_KEY_OPEN); CHECK(a.error.code==FL_ERR_UNSUPPORTED); dismiss(&a); CHECK(a.level==1);
    handle_key(&a,FL_KEY_BACK); CHECK(a.level==0);
    handle_key(&a,FL_KEY_DISCONNECT); CHECK(a.screen==SCREEN_ACCOUNT);
    handle_key(&a,FL_KEY_OPEN);CHECK(a.screen==SCREEN_MESSAGE&&a.error.code==FL_ERR_CONFIG);dismiss(&a);CHECK(a.screen==SCREEN_ACCOUNT);
    handle_key(&a,FL_KEY_DOWN);handle_key(&a,FL_KEY_OPEN);CHECK(a.screen==SCREEN_HTTPS_GUIDE);
    for(unsigned i=0;i<10;i++)handle_key(&a,FL_KEY_RIGHT);CHECK(a.guide_page==3);
    handle_key(&a,FL_KEY_BACK);CHECK(a.screen==SCREEN_ACCOUNT&&a.account_selected==1);
    for(unsigned i=0;i<10;i++)handle_key(&a,FL_KEY_DOWN);CHECK(a.account_selected==2);
    handle_key(&a,FL_KEY_OPEN);CHECK(a.screen==SCREEN_DISCONNECT);
    handle_key(&a,FL_KEY_BACK);CHECK(a.screen==SCREEN_ACCOUNT);
    handle_key(&a,FL_KEY_BACK); CHECK(a.screen==SCREEN_LIST); CHECK(!a.disconnected);
    handle_key(&a,FL_KEY_DISCONNECT);for(unsigned i=0;i<2;i++)handle_key(&a,FL_KEY_DOWN);handle_key(&a,FL_KEY_OPEN);handle_key(&a,FL_KEY_OPEN);
    CHECK(a.screen==SCREEN_MESSAGE && a.error.code==FL_OK); CHECK(!a.disconnected); dismiss(&a);
    /* Initial setup opens phone login directly; Help returns to its caller. */
    a.screen=SCREEN_SETUP;handle_key(&a,FL_KEY_REFRESH);CHECK(a.screen==SCREEN_HTTPS_GUIDE);
    handle_key(&a,FL_KEY_BACK);CHECK(a.screen==SCREEN_SETUP);handle_key(&a,FL_KEY_OPEN);CHECK(a.screen==SCREEN_MESSAGE);dismiss(&a);CHECK(a.screen==SCREEN_ACCOUNT&&a.account_back==SCREEN_SETUP);
    a.screen=SCREEN_LIST;
    /* Help does not approve/cancel a running phone login or lose its screen. */
    a.screen=SCREEN_HTTPS;a.https.active=true;
    handle_key(&a,FL_KEY_REFRESH);CHECK(a.screen==SCREEN_HTTPS_GUIDE&&a.https.active&&!a.https.cancelled);
    handle_key(&a,FL_KEY_BACK);CHECK(a.screen==SCREEN_HTTPS&&!a.https.cancelled);
    handle_key(&a,FL_KEY_BACK);CHECK(a.https.cancelled);fl_https_destroy(&a.https);a.screen=SCREEN_LIST;
    /* Simulated stale cache is visible after a failed refresh, with offline status. */
    a.backend.initialized=true; a.backend.done=true; a.backend.cache_fallback=true;
    fl_error(&a.backend.result,FL_ERR_OFFLINE,"Simulated offline state.");
    CHECK(fl_listing_parse("[{\"ID\":\"cached_friend\",\"Name\":\"Cached Friend\",\"IsDir\":true,\"Size\":-1}]",&a.backend.listing,&error)==0);
    a.backend.listing.from_cache=true; a.backend.listing.stale=true; a.backend.listing.fetched=time(NULL)-3600;
    snprintf(a.backend.folder_id,sizeof(a.backend.folder_id),"demo_root");
    a.load_level=0; a.loading=true;
    pump_backend(&a);
    CHECK(!a.loading && a.level==0 && a.frames[0].listing.count==1);
    CHECK(a.frames[0].listing.from_cache && a.frames[0].listing.stale && a.offline);
    CHECK(strstr(a.notice,"Cached library")!=NULL);
    /* B cancels only the owned listing subprocess and keeps the current view. */
    { const char *args[]={"/bin/sleep","20",NULL};
      a.backend.initialized=true; a.backend.active=true;
      CHECK(fl_process_start(&a.backend.process,args,256,&error)==0);
      a.loading=true; handle_key(&a,FL_KEY_BACK);
      CHECK(a.backend.process.cancelled); CHECK(a.loading);
      fl_backend_destroy(&a.backend); a.loading=false;
    }
    for(unsigned i=0;i<3;++i)frame_free(&a.frames[i]);
    /* Render every offline guide page, account menu and initial setup. */
    CHECK(setenv("SDL_VIDEODRIVER","dummy",1)==0);a.ui=fl_ui_open(&error);CHECK(a.ui!=NULL);a.level=0;
    const Screen screens[]={SCREEN_ACCOUNT,SCREEN_SETUP,SCREEN_HTTPS,SCREEN_DISCONNECT};
    for(unsigned i=0;i<4;i++){a.screen=screens[i];draw(&a);CHECK(!a.quit);char shot[128];snprintf(shot,sizeof(shot),"build/ui-simple-screen-%u.bmp",i);CHECK(!fl_ui_screenshot(a.ui,shot,&error));}
    a.screen=SCREEN_HTTPS_GUIDE;
    for(unsigned page=0;page<4;page++){
        a.guide_page=page;draw(&a);CHECK(!a.quit);char shot[128];snprintf(shot,sizeof(shot),"build/ui-https-guide-%u.bmp",page+1);CHECK(!fl_ui_screenshot(a.ui,shot,&error));
        if(page<3)for(unsigned line=0;line<8;line++)CHECK(strlen(https_guide[page][line])<=49);
    }
    strcpy(a.qr_url,"https://test.workers.dev/pair/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");CHECK(!fl_phone_qr(a.qr_url,a.phone_qr));strcpy(a.https.code,"123456");a.https.ready=true;a.screen=SCREEN_HTTPS;draw(&a);CHECK(!fl_ui_screenshot(a.ui,"build/ui-https-qr.bmp",&error));a.qr_url[0]=0;
    a.demo=true;load_folder(&a,0,a.config.root_id,false);draw(&a);CHECK(!fl_ui_screenshot(a.ui,"build/ui-simple-library.bmp",&error));
    for(unsigned i=0;i<3;++i)frame_free(&a.frames[i]);
    fl_ui_close(a.ui);
    library_clear(&a);
    fl_systems_free(&a.systems); fl_process_destroy(&a.hash);
    printf("app: %u controller assertions passed (demo/cache mocks, no Google account)\n",checks);
    return 0;
}
