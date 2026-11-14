#include "fl_core.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static bool favorite_name(const char *name) {
    char normalized[32];size_t n=0;
    const unsigned char *p=(const unsigned char*)name;
    while(*p&&n<sizeof(normalized)-1){
        if(p[0]==0xc4&&(p[1]==0xb0||p[1]==0xb1)){normalized[n++]='i';p+=2;}
        else {normalized[n++]=(char)tolower(*p);p++;}
    }
    normalized[n]=0;
    return !*p&&(!strcmp(normalized,"favoriler")||!strcmp(normalized,"favorites")||!strcmp(normalized,"favourites"));
}
int fl_favorites_folder(const FlListing *listing,char id[FL_ID],FlError *error) {
    id[0]=0;
    for(size_t i=0;i<listing->count;i++){
        const FlItem *item=&listing->items[i];
        if(!item->is_dir||!favorite_name(item->name))continue;
        if(*id){fl_error(error,FL_ERR_EXISTS,"Multiple Favorites folders found. Keep one Favorites folder per console.");return -1;}
        memcpy(id,item->id,FL_ID);
    }
    return *id?1:0;
}
static bool collection_folder(const FlItem *item){return item->is_dir&&favorite_name(item->name);}
static int id_compare(const void *a,const void *b){
    const FlItem *x=a,*y=b;int n=strcmp(x->id,y->id);
    return n?n:(int)y->favorite-(int)x->favorite;
}
static int name_compare(const void *a,const void *b){
    const FlItem *x=a,*y=b;int n=strcasecmp(x->name,y->name);return n?n:strcmp(x->id,y->id);
}
int fl_library_merge(const FlListing *base,const FlListing *favorites,FlListing *out,FlError *error){
    FlListing result={0};size_t count=0;
    for(size_t i=0;i<base->count;i++)if(!collection_folder(&base->items[i]))count++;
    if(count>FL_MAX_LIST||favorites->count>FL_MAX_LIST-count){fl_error(error,FL_ERR_LIMIT,"Combined console and Favorites listing exceeds 5000 items.");return -1;}
    result.items=calloc(count+favorites->count?count+favorites->count:1,sizeof(FlItem));
    if(!result.items){fl_error(error,FL_ERR_LIMIT,"Not enough memory for the game library.");return -1;}
    for(size_t i=0;i<base->count;i++)if(!collection_folder(&base->items[i])){
        result.items[result.count]=base->items[i];result.items[result.count++].favorite=false;
    }
    for(size_t i=0;i<favorites->count;i++){
        result.items[result.count]=favorites->items[i];result.items[result.count++].favorite=true;
    }
    qsort(result.items,result.count,sizeof(FlItem),id_compare);
    size_t n=0;
    for(size_t i=0;i<result.count;i++)if(!n||strcmp(result.items[i].id,result.items[n-1].id))result.items[n++]=result.items[i];
    result.count=n;qsort(result.items,result.count,sizeof(FlItem),name_compare);
    result.from_cache=base->from_cache||favorites->from_cache;result.stale=base->stale||favorites->stale;
    result.fetched=base->fetched;
    if(favorites->fetched&&(!result.fetched||favorites->fetched<result.fetched))result.fetched=favorites->fetched;
    *out=result;return 0;
}
int fl_library_view(const FlListing *all,bool favorites_only,FlListing *out,FlError *error){
    FlListing view=*all;view.count=0;view.items=NULL;
    for(size_t i=0;i<all->count;i++)if(!favorites_only||all->items[i].favorite)view.count++;
    view.items=calloc(view.count?view.count:1,sizeof(FlItem));
    if(!view.items){fl_error(error,FL_ERR_LIMIT,"Not enough memory for the selected view.");return -1;}
    size_t n=0;for(size_t i=0;i<all->count;i++)if(!favorites_only||all->items[i].favorite)view.items[n++]=all->items[i];
    *out=view;return 0;
}
