#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include "fl_core.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"core %s:%d failed: %s\n",__FILE__,__LINE__,#x);exit(1);}}while(0)
static void write_text(const char *path,const char *text){FILE *f=fopen(path,"wb");CHECK(f);CHECK(fwrite(text,1,strlen(text),f)==strlen(text));CHECK(fclose(f)==0);}
static const char *systems_path="package/App/FriendsLibrary/config/systems.json";
int main(void) {
    FlSystems systems={0};FlError error={0};FlListing list={0};char clean[FL_NAME];
    CHECK(fl_systems_load(systems_path,&systems,&error)==0);CHECK(systems.count==8);
    struct {const char *alias,*id;} aliases[]={{"NES","FC"},{" fc ","FC"},{"Famicom","FC"},{"SNES","SFC"},{"SUPER FAMICOM","SFC"},{"GB","GB"},{"game boy color","GBC"},{"GBA","GBA"},{"ps1","PS"},{"PSX","PS"},{"DS","NDS"},{"Genesis","MD"},{"Mega Drive","MD"}};
    for(size_t i=0;i<sizeof(aliases)/sizeof(*aliases);i++){const FlSystem *s=fl_system_find(&systems,aliases[i].alias);CHECK(s);CHECK(!strcmp(s->id,aliases[i].id));}
    CHECK(!fl_system_find(&systems,"UNKNOWN"));CHECK(!fl_system_find(&systems,"NES/../../"));CHECK(!fl_system_find(&systems,""));
    const FlSystem *fc=fl_system_find(&systems,"FC"),*ps=fl_system_find(&systems,"PS");
    CHECK(fl_extension_allowed(fc,"Game.NES"));CHECK(fl_extension_allowed(fc,"Game.zip"));CHECK(!fl_extension_allowed(fc,"Game.exe"));CHECK(!fl_extension_allowed(fc,"game"));
    CHECK(fl_extension_allowed(ps,"Complete.chd"));CHECK(fl_extension_allowed(ps,"Complete.PBP"));CHECK(!fl_extension_allowed(ps,"Incomplete.bin"));CHECK(!fl_extension_allowed(ps,"Playlist.m3u"));CHECK(!fl_extension_allowed(ps,"Disc.cue"));
    CHECK(fl_sanitize_filename("Normal Game (USA).nes",clean,&error)==0);CHECK(!strcmp(clean,"Normal Game (USA).nes"));
    CHECK(fl_sanitize_filename("Türkçe Oyun.nes",clean,&error)==0);CHECK(!strcmp(clean,"Türkçe Oyun.nes"));
    CHECK(fl_sanitize_filename("A:B?.nes",clean,&error)==0);CHECK(!strcmp(clean,"A_B_.nes"));
    CHECK(fl_sanitize_filename("$(touch injected).nes",clean,&error)==0); /* Kept as data, never shell source. */
    const char *bad[]={"",".","..","../escape.nes","sub/game.nes","sub\\game.nes","/abs.nes",".hidden.nes"," leading.nes","trailing.nes ","dot.nes.","a\nb.nes","bad\xff.nes"};
    for(size_t i=0;i<sizeof(bad)/sizeof(*bad);i++)CHECK(fl_sanitize_filename(bad[i],clean,&error)==-1);
    char longname[300];memset(longname,'a',sizeof(longname));longname[299]=0;CHECK(fl_sanitize_filename(longname,clean,&error)==-1);
    CHECK(fl_id_valid("AbC-123_xyz"));CHECK(!fl_id_valid("../../id"));CHECK(!fl_id_valid("x:y"));CHECK(!fl_id_valid(""));

    const char *hierarchy="[{\"ID\":\"file_1\",\"Name\":\"Readme.txt\",\"Size\":3,\"IsDir\":false},{\"ID\":\"friend_b\",\"Name\":\"FRIEND_B\",\"Size\":-1,\"IsDir\":true},{\"ID\":\"friend_a\",\"Name\":\"FRIEND_A\",\"Size\":-1,\"IsDir\":true}]";
    CHECK(fl_listing_parse(hierarchy,&list,&error)==0);CHECK(list.count==3);fl_listing_directories(&list);CHECK(list.count==2);CHECK(!strcmp(list.items[0].name,"FRIEND_A"));CHECK(!strcmp(list.items[1].id,"friend_b"));fl_listing_free(&list);
    CHECK(fl_listing_parse("[]",&list,&error)==0);CHECK(list.count==0);fl_listing_free(&list);
    CHECK(fl_listing_parse("not json",&list,&error)==-1);CHECK(fl_listing_parse("{}",&list,&error)==-1);
    const char *files="[{\"ID\":\"game_one\",\"Name\":\"A:B.nes\",\"IsDir\":false,\"Size\":\"0\",\"Hashes\":{\"MD5\":\"d41d8cd98f00b204e9800998ecf8427e\"}},{\"ID\":\"game_two\",\"Name\":\"A_B.nes\",\"IsDir\":false,\"Size\":4294967295},{\"ID\":\"shortcut\",\"Name\":\"OUTSIDE\",\"IsDir\":true,\"Size\":-1,\"MimeType\":\"application/vnd.google-apps.shortcut\"},{\"ID\":\"traversal\",\"Name\":\"../escape.nes\",\"IsDir\":false,\"Size\":100}]";
    CHECK(fl_listing_parse(files,&list,&error)==0);CHECK(list.count==2);CHECK(list.items[0].size==0);CHECK(list.items[1].size==INT64_C(4294967295));CHECK(fl_listing_collision(&list,0));CHECK(fl_listing_collision(&list,1));fl_listing_free(&list);
    const char *bad_listings[]={
      "[{\"ID\":\"same\",\"Name\":\"a.nes\",\"Size\":1,\"IsDir\":false},{\"ID\":\"same\",\"Name\":\"b.nes\",\"Size\":2,\"IsDir\":false}]",
      "[{\"ID\":\"a\",\"Name\":\"a.nes\",\"Size\":-1,\"IsDir\":false}]",
      "[{\"ID\":\"a\",\"Name\":\"a.nes\",\"Size\":0.5,\"IsDir\":false}]",
      "[{\"ID\":\"a\",\"Name\":\"a.nes\",\"Size\":9007199254740993,\"IsDir\":false}]",
      "[{\"ID\":\"a\",\"Name\":\"a.nes\",\"Size\":\"99999999999999999999999999\",\"IsDir\":false}]",
      "[{\"ID\":\"a\",\"Name\":\"a.nes\",\"Size\":3,\"IsDir\":false,\"Hashes\":{\"MD5\":\"bad\"}}]",
      "[{\"Name\":\"a.nes\",\"Size\":1,\"IsDir\":false}]",
      "[{\"ID\":\"a\",\"Name\":\"a\\u0000.nes\",\"Size\":1,\"IsDir\":false}]"};
    for(size_t i=0;i<sizeof(bad_listings)/sizeof(*bad_listings);i++)CHECK(fl_listing_parse(bad_listings[i],&list,&error)==-1);
    char temp[]="/tmp/friends-core-XXXXXX";CHECK(mkdtemp(temp));FlConfig config={0};snprintf(config.sd_root,sizeof(config.sd_root),"%s",temp);snprintf(config.data_dir,sizeof(config.data_dir),"%s/.config/friends-library",temp);snprintf(config.root_id,sizeof(config.root_id),"root_test");
    char target[FL_PATH],path[FL_PATH],md5[33];CHECK(fl_mkdirs(config.data_dir,&error)==0);
    CHECK(fl_rom_target(&config,fc,"Example.nes",true,target,&error)==0);snprintf(path,sizeof(path),"%s/Roms/FC/Example.nes",temp);CHECK(!strcmp(target,path));CHECK(access(target,F_OK)==-1);
    CHECK(fl_rom_target(&config,NULL,"Example.nes",true,target,&error)==-1);CHECK(error.code==FL_ERR_UNSUPPORTED);
    CHECK(fl_rom_target(&config,fc,"../escape.nes",true,target,&error)==-1);CHECK(error.code==FL_ERR_PATH);
    CHECK(fl_rom_target(&config,fc,"Example.exe",true,target,&error)==-1);CHECK(error.code==FL_ERR_FORMAT);
    write_text(path,"abc");CHECK(fl_md5_file(path,md5,&error)==0);CHECK(!strcmp(md5,"900150983cd24fb0d6963f7d28e17f72"));
    FlItem item={0};strcpy(item.id,"existing_rom");strcpy(item.name,"Example.nes");strcpy(item.md5,md5);item.size=3;
    CHECK(fl_local_check(&config,fc,&item,false,&error)==FL_LOCAL_PRESENT);CHECK(fl_local_check(&config,fc,&item,true,&error)==FL_LOCAL_VERIFIED);
    write_text(path,"def");CHECK(fl_local_check(&config,fc,&item,true,&error)==FL_LOCAL_CHANGED);write_text(path,"longer");CHECK(fl_local_check(&config,fc,&item,false,&error)==FL_LOCAL_CHANGED);
    CHECK(fl_rom_target(&config,fc,"example.nes",true,target,&error)==-1);CHECK(error.code==FL_ERR_EXISTS);
    CHECK(fl_rom_target(&config,fc,"Example.nes",true,target,&error)==0);CHECK(fl_receipt_write(&config,&item,target,&error)==0);
    uint64_t available=0;snprintf(path,sizeof(path),"%s/Roms/FC",temp);CHECK(fl_space_check(path,0,&available,&error)==0);CHECK(available>0);CHECK(fl_space_check(path,INT64_MAX,NULL,&error)==-1);CHECK(error.code==FL_ERR_SPACE);CHECK(fl_space_check(path,-1,NULL,&error)==-1);
    snprintf(path,sizeof(path),"%s/empty",temp);write_text(path,"");CHECK(fl_md5_file(path,md5,&error)==0);CHECK(!strcmp(md5,"d41d8cd98f00b204e9800998ecf8427e"));
    write_text(path,"abcdefghijklmnopqrstuvwxyz");CHECK(fl_md5_file(path,md5,&error)==0);CHECK(!strcmp(md5,"c3fcd3d76192e4007dfb496cca67e13b"));
    char large[1001];memset(large,'a',1000);large[1000]=0;write_text(path,large);CHECK(fl_md5_file(path,md5,&error)==0);CHECK(!strcmp(md5,"cabe45dcc9ae5b66ba86600cca6b8ba8"));
    CHECK(fl_write_atomic(path,"updated",7,&error)==0);char *read=NULL;CHECK(fl_read_file(path,100,&read,&error)==0);CHECK(!strcmp(read,"updated"));free(read);CHECK(fl_read_file(path,1,&read,&error)==-1);CHECK(error.code==FL_ERR_LIMIT);
    char linkpath[FL_PATH];snprintf(linkpath,sizeof(linkpath),"%s/link",temp);CHECK(symlink(path,linkpath)==0);CHECK(fl_read_file(linkpath,100,&read,&error)==-1);CHECK(fl_md5_file(linkpath,md5,&error)==-1);
    snprintf(linkpath,sizeof(linkpath),"%s/Roms/GBA",temp);CHECK(symlink("/tmp",linkpath)==0);CHECK(fl_rom_target(&config,fl_system_find(&systems,"GBA"),"Game.gba",true,target,&error)==-1);CHECK(error.code==FL_ERR_PATH);
    FlSystem malicious={0};strcpy(malicious.id,"../BAD");strcpy(malicious.extensions,"nes");CHECK(fl_rom_target(&config,&malicious,"a.nes",true,target,&error)==-1);
    CHECK(!strcmp(fl_code_name(FL_ERR_CANCELLED),"cancelled"));CHECK(!strcmp(fl_code_name((FlCode)999),"unknown"));
    fl_error(&error,FL_ERR_IO,"Preserve aliased error text");fl_error(&error,FL_ERR_IO,error.message);CHECK(!strcmp(error.message,"Preserve aliased error text"));
    fl_systems_free(&systems);printf("core: %u checks passed\n",checks);return 0;
}
