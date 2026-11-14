#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define main fl_application_main
#include "../src/main.c"
#undef main

static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"favorites %s:%d: %s\n",__FILE__,__LINE__,#x);exit(1);}}while(0)
static FlListing parse(const char *json){FlListing l={0};FlError e={0};CHECK(fl_listing_parse(json,&l,&e)==0);return l;}
static void text_file(const char *path,const char *value){FILE *f=fopen(path,"wb");CHECK(f);CHECK(fwrite(value,1,strlen(value),f)==strlen(value));CHECK(!fclose(f));}
static void finish(App *a){unsigned n=0;while(a->loading&&n++<2000){pump_backend(a);struct timespec t={0,2000000};nanosleep(&t,NULL);}CHECK(n<2000);}
int main(void){
    FlError e={0};char id[FL_ID];
    FlListing root=parse("[{\"ID\":\"normal\",\"Name\":\"Normal.nes\",\"Size\":3,\"IsDir\":false},{\"ID\":\"favs\",\"Name\":\"Favorites\",\"Size\":-1,\"IsDir\":true}]");
    FlListing favorites=parse("[{\"ID\":\"best\",\"Name\":\"Best.nes\",\"Size\":3,\"IsDir\":false}]");
    FlListing all={0},view={0},empty={0};
    CHECK(fl_favorites_folder(&root,id,&e)==1);CHECK(!strcmp(id,"favs"));
    CHECK(!fl_library_merge(&root,&favorites,&all,&e));CHECK(all.count==2);
    CHECK(!strcmp(all.items[0].id,"best")&&all.items[0].favorite);CHECK(!all.items[1].favorite);
    CHECK(!fl_library_view(&all,true,&view,&e));CHECK(view.count==1&&!strcmp(view.items[0].id,"best"));fl_listing_free(&view);
    CHECK(!fl_library_view(&all,false,&view,&e));CHECK(view.count==2);fl_listing_free(&view);fl_listing_free(&all);
    FlListing nofolder=parse("[{\"ID\":\"normal\",\"Name\":\"Normal.nes\",\"Size\":3,\"IsDir\":false}]");
    CHECK(fl_favorites_folder(&nofolder,id,&e)==0);CHECK(!fl_library_merge(&nofolder,&empty,&all,&e));CHECK(all.count==1);
    CHECK(!fl_library_view(&all,true,&view,&e));CHECK(view.count==0);fl_listing_free(&view);fl_listing_free(&all);
    CHECK(!fl_library_merge(&empty,&empty,&all,&e));CHECK(all.count==0);fl_listing_free(&all);
    FlListing aliases=parse("[{\"ID\":\"turkish\",\"Name\":\"FAVORİLER\",\"Size\":-1,\"IsDir\":true}]");
    CHECK(fl_favorites_folder(&aliases,id,&e)==1);fl_listing_free(&aliases);
    aliases=parse("[{\"ID\":\"a\",\"Name\":\"Favorites\",\"Size\":-1,\"IsDir\":true},{\"ID\":\"b\",\"Name\":\"favoriler\",\"Size\":-1,\"IsDir\":true}]");
    CHECK(fl_favorites_folder(&aliases,id,&e)==-1);CHECK(e.code==FL_ERR_EXISTS);fl_listing_free(&aliases);
    FlListing sameid=parse("[{\"ID\":\"normal\",\"Name\":\"Moved.nes\",\"Size\":4,\"IsDir\":false}]");
    CHECK(!fl_library_merge(&nofolder,&sameid,&all,&e));CHECK(all.count==1&&all.items[0].favorite&&all.items[0].size==4);fl_listing_free(&all);fl_listing_free(&sameid);
    FlListing collision=parse("[{\"ID\":\"different\",\"Name\":\"Normal.nes\",\"Size\":3,\"IsDir\":false}]");
    CHECK(!fl_library_merge(&nofolder,&collision,&all,&e));CHECK(all.count==2);CHECK(fl_listing_collision(&all,0));CHECK(fl_listing_collision(&all,1));
    all.from_cache=true;all.stale=true;all.fetched=time(NULL)-1000;
    CHECK(!fl_library_view(&all,true,&view,&e));CHECK(view.from_cache&&view.stale&&view.fetched==all.fetched);fl_listing_free(&view);
    App app={0};app.all_games=all;memset(&all,0,sizeof(all));app.systems=(FlSystems){0};
    CHECK(!fl_systems_load("package/App/FriendsLibrary/config/systems.json",&app.systems,&e));app.system=fl_system_find(&app.systems,"NES");strcpy(app.console_id,"console_test");
    CHECK(!open_collection(&app,1));CHECK(app.frames[2].listing.count==1);selected_game(&app);CHECK(app.screen==SCREEN_MESSAGE&&app.error.code==FL_ERR_EXISTS);
    frame_free(&app.frames[2]);library_clear(&app);
    FlListing huge={0};huge.count=FL_MAX_LIST+1;CHECK(fl_library_merge(&empty,&huge,&all,&e)==-1);CHECK(e.code==FL_ERR_LIMIT);

    char temp[]="/tmp/friends-favorites-XXXXXX",appdir[FL_PATH],fixture[FL_PATH];CHECK(mkdtemp(temp));
    snprintf(appdir,sizeof(appdir),"%s/App/FriendsLibrary",temp);CHECK(!fl_mkdirs(appdir,&e));
    char configdir[FL_PATH];snprintf(configdir,sizeof(configdir),"%s/.config/friends-library",temp);CHECK(!fl_mkdirs(configdir,&e));
    char settings[FL_PATH];CHECK(snprintf(settings,sizeof(settings),"%s/settings.json",configdir)<(int)sizeof(settings));text_file(settings,"{\"root_folder_id\":\"library_root\",\"remote\":\"friends\",\"cache_ttl_seconds\":900}");
    CHECK(!fl_config_load(temp,appdir,&app.config,&e));
    text_file(app.config.credential_path,"[friends]\ntype=drive\nscope=drive.readonly\nclient_id=fake-test-client\ntoken={\"refresh_token\":\"FAKE_ONLY\"}\n");
    CHECK(realpath("tests/fixtures/favorites-rclone.sh",fixture));CHECK(!chmod(fixture,0700));strcpy(app.config.rclone_path,fixture);strcpy(app.friend_name,"DYNAMIC FRIEND");
    app.system=fl_system_find(&app.systems,"NES");app.level=1;app.screen=SCREEN_LIST;
    setenv("FL_FAV_MODE","ok",1);load_folder(&app,2,"console_test",true);finish(&app);
    CHECK(app.screen==SCREEN_COLLECTIONS&&app.all_games.count==2);CHECK(!strcmp(app.favorites_id,"favorites_test"));
    setenv("SDL_VIDEODRIVER","dummy",1);app.ui=fl_ui_open(&e);CHECK(app.ui);
    draw(&app);CHECK(!fl_ui_screenshot(app.ui,"build/favorites-menu.bmp",&e));
    CHECK(!open_collection(&app,1));CHECK(app.frames[2].listing.count==1);CHECK(!strcmp(app.frames[2].listing.items[0].id,"fav_game"));
    handle_key(&app,FL_KEY_BACK);CHECK(app.screen==SCREEN_COLLECTIONS);handle_key(&app,FL_KEY_DOWN);CHECK(app.collection==0);handle_key(&app,FL_KEY_OPEN);CHECK(app.frames[2].listing.count==2);
    handle_key(&app,FL_KEY_DOWN);size_t selected=app.frames[2].selected;
    handle_key(&app,FL_KEY_BACK);handle_key(&app,FL_KEY_OPEN);CHECK(app.frames[2].selected==selected);
    setenv("FL_FAV_MODE","offline",1);load_folder(&app,2,"console_test",true);finish(&app);CHECK(app.all_games.count==2&&app.all_games.stale&&app.offline);
    /* A missing required child with no cache must not turn into a successful partial All. */
    snprintf(app.config.root_id,sizeof(app.config.root_id),"another_namespace");setenv("FL_FAV_MODE","denied",1);
    load_folder(&app,2,"console_test",true);finish(&app);CHECK(app.screen==SCREEN_MESSAGE&&app.error.code==FL_ERR_PERMISSION);CHECK(app.all_games.count==2);
    setenv("FL_FAV_MODE","nofolder",1);app.screen=SCREEN_LIST;load_folder(&app,2,"console_test",true);finish(&app);CHECK(app.all_games.count==1&&!*app.favorites_id);
    CHECK(!open_collection(&app,1));CHECK(app.frames[2].listing.count==0);
    draw(&app);CHECK(!fl_ui_screenshot(app.ui,"build/favorites-empty.bmp",&e));
    handle_key(&app,FL_KEY_BACK);CHECK(!open_collection(&app,0));CHECK(app.frames[2].listing.count==1);
    setenv("FL_FAV_MODE","empty",1);load_folder(&app,2,"console_test",true);finish(&app);CHECK(app.all_games.count==1);
    setenv("FL_FAV_MODE","slow",1);load_folder(&app,2,"console_test",true);
    unsigned n=0;while(app.console_phase!=2&&app.loading&&n++<2000){pump_backend(&app);struct timespec t={0,2000000};nanosleep(&t,NULL);}CHECK(n<2000&&app.loading);
    handle_key(&app,FL_KEY_BACK);finish(&app);CHECK(app.screen==SCREEN_MESSAGE&&app.error.code==FL_ERR_CANCELLED);CHECK(app.all_games.count==1);
    fl_ui_close(app.ui);fl_backend_destroy(&app.backend);for(unsigned i=0;i<3;i++)frame_free(&app.frames[i]);library_clear(&app);fl_systems_free(&app.systems);
    fl_listing_free(&root);fl_listing_free(&favorites);fl_listing_free(&nofolder);fl_listing_free(&collision);
    printf("favorites: %u checks passed (optional folders, union, identity, collisions, async/cache/cancel)\n",checks);return 0;
}
