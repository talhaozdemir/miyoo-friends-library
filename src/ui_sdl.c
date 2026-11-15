#include "fl_ui.h"
#include "qrcodegen.h"

#include <SDL/SDL.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Original 5 x 7 bitmap lettering. No font engine or external font is required. */
typedef struct { char ch; unsigned char row[7]; } Glyph;
static const Glyph font[] = {
    {'A',{14,17,17,31,17,17,17}}, {'B',{30,17,17,30,17,17,30}},
    {'C',{14,17,16,16,16,17,14}}, {'D',{30,17,17,17,17,17,30}},
    {'E',{31,16,16,30,16,16,31}}, {'F',{31,16,16,30,16,16,16}},
    {'G',{14,17,16,23,17,17,15}}, {'H',{17,17,17,31,17,17,17}},
    {'I',{14,4,4,4,4,4,14}}, {'J',{7,2,2,2,18,18,12}},
    {'K',{17,18,20,24,20,18,17}}, {'L',{16,16,16,16,16,16,31}},
    {'M',{17,27,21,21,17,17,17}}, {'N',{17,25,25,21,19,19,17}},
    {'O',{14,17,17,17,17,17,14}}, {'P',{30,17,17,30,16,16,16}},
    {'Q',{14,17,17,17,21,18,13}}, {'R',{30,17,17,30,20,18,17}},
    {'S',{15,16,16,14,1,1,30}}, {'T',{31,4,4,4,4,4,4}},
    {'U',{17,17,17,17,17,17,14}}, {'V',{17,17,17,17,17,10,4}},
    {'W',{17,17,17,21,21,21,10}}, {'X',{17,17,10,4,10,17,17}},
    {'Y',{17,17,10,4,4,4,4}}, {'Z',{31,1,2,4,8,16,31}},
    {'0',{14,17,19,21,25,17,14}}, {'1',{4,12,4,4,4,4,14}},
    {'2',{14,17,1,2,4,8,31}}, {'3',{30,1,1,14,1,1,30}},
    {'4',{2,6,10,18,31,2,2}}, {'5',{31,16,16,30,1,1,30}},
    {'6',{14,16,16,30,17,17,14}}, {'7',{31,1,2,4,8,8,8}},
    {'8',{14,17,17,14,17,17,14}}, {'9',{14,17,17,15,1,1,14}},
    {' ',{0,0,0,0,0,0,0}}, {'.',{0,0,0,0,0,12,12}},
    {',',{0,0,0,0,4,4,8}}, {':',{0,12,12,0,12,12,0}},
    {';',{0,12,12,0,4,4,8}}, {'-',{0,0,0,31,0,0,0}},
    {'_',{0,0,0,0,0,0,31}}, {'/',{1,2,2,4,8,8,16}},
    {'\\',{16,8,8,4,2,2,1}}, {'(',{2,4,8,8,8,4,2}},
    {')',{8,4,2,2,2,4,8}}, {'[',{14,8,8,8,8,8,14}},
    {']',{14,2,2,2,2,2,14}}, {'+', {0,4,4,31,4,4,0}},
    {'=',{0,0,31,0,31,0,0}}, {'%',{17,2,4,8,16,17,0}},
    {'!',{4,4,4,4,4,0,4}}, {'?',{14,17,1,2,4,0,4}},
    {'>',{16,8,4,2,4,8,16}}, {'<',{1,2,4,8,4,2,1}},
    {'*',{0,21,14,31,14,21,0}}, {'#',{10,10,31,10,31,10,10}},
    {'&',{12,18,20,8,21,18,13}}, {'@',{14,17,23,21,23,16,14}},
    {'\'',{4,4,8,0,0,0,0}}, {'"',{10,10,20,0,0,0,0}},
    {'|',{4,4,4,4,4,4,4}}, {'~',{0,0,9,22,0,0,0}},
    {'^',{4,10,17,0,0,0,0}}, {'$',{4,15,20,14,5,30,4}}
};

struct FlUi {
    SDL_Surface *display;
    SDL_Surface *present;
    bool rotate;
    /* Always draw in system RAM; the Miyoo SDL hardware fill path and native
     * framebuffer format must not determine individual glyph writes. */
    SDL_Surface *screen;
    Uint32 bg, panel, text, muted, accent, focus, danger;
};

static void rect(FlUi *ui, int x, int y, int w, int h, Uint32 color)
{
    SDL_Rect r;
    if (w <= 0 || h <= 0) return;
    r.x = (Sint16)x; r.y = (Sint16)y;
    r.w = (Uint16)w; r.h = (Uint16)h;
    SDL_FillRect(ui->screen, &r, color);
}

/* Decode one complete UTF-8 sequence so clipping never splits a filename byte
 * sequence. Unsupported code points display '?'; filesystem names are unchanged. */
static unsigned next_cp(const char **text)
{
    const unsigned char *p = (const unsigned char *)*text;
    unsigned n, cp, min;
    if (!*p) return 0;
    if (*p < 128) { *text += 1; return *p; }
    if (*p >= 0xc2 && *p <= 0xdf) { n=2; cp=*p & 31; min=128; }
    else if (*p >= 0xe0 && *p <= 0xef) { n=3; cp=*p & 15; min=2048; }
    else if (*p >= 0xf0 && *p <= 0xf4) { n=4; cp=*p & 7; min=65536; }
    else { *text += 1; return '?'; }
    for (unsigned i=1; i<n; i++) {
        if (!p[i] || (p[i] & 0xc0) != 0x80) { *text += 1; return '?'; }
        cp=(cp<<6)|(p[i]&63);
    }
    *text += n;
    if (cp < min || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) return '?';
    return cp;
}

static char glyph_char(unsigned cp)
{
    if (cp >= 'a' && cp <= 'z') return (char)(cp-'a'+'A');
    if (cp >= 32 && cp < 127) return (char)cp;
    switch (cp) {
    case 0xc0: case 0xc1: case 0xc2: case 0xc3: case 0xc4: case 0xc5:
    case 0xe0: case 0xe1: case 0xe2: case 0xe3: case 0xe4: case 0xe5: return 'A';
    case 0xc7: case 0xe7: return 'C';
    case 0xc8: case 0xc9: case 0xca: case 0xcb:
    case 0xe8: case 0xe9: case 0xea: case 0xeb: return 'E';
    case 0x11e: case 0x11f: return 'G';
    case 0xcc: case 0xcd: case 0xce: case 0xcf: case 0x130: case 0x131:
    case 0xec: case 0xed: case 0xee: case 0xef: return 'I';
    case 0xd1: case 0xf1: return 'N';
    case 0xd2: case 0xd3: case 0xd4: case 0xd5: case 0xd6: case 0xd8:
    case 0xf2: case 0xf3: case 0xf4: case 0xf5: case 0xf6: case 0xf8: return 'O';
    case 0x15e: case 0x15f: return 'S';
    case 0xd9: case 0xda: case 0xdb: case 0xdc:
    case 0xf9: case 0xfa: case 0xfb: case 0xfc: return 'U';
    case 0xdd: case 0xfd: case 0xff: return 'Y';
    case 0x2018: case 0x2019: return '\'';
    case 0x201c: case 0x201d: return '"';
    case 0x2013: case 0x2014: return '-';
    case 0x2192: return '>';
    case 0x2190: return '<';
    default: return '?';
    }
}

static const unsigned char *glyph(char ch)
{
    size_t i;
    for (i=0; i<sizeof font/sizeof font[0]; i++)
        if (font[i].ch == ch) return font[i].row;
    return glyph('?');
}

static void draw_char(FlUi *ui, int x, int y, char ch, int scale, Uint32 color)
{
    const unsigned char *bits = glyph(ch);
    for (int row=0; row<7; row++)
        for (int col=0; col<5; col++)
            if (bits[row] & (1 << (4-col)))
                rect(ui, x+col*scale, y+row*scale, scale, scale, color);
}

static void text(FlUi *ui, int x, int y, int width, const char *s, int scale, Uint32 color)
{
    const char *cursor = s ? s : "";
    int count=0, slots=width/(6*scale);
    if (slots <= 0) return;
    while (*cursor && count<slots) {
        unsigned cp=next_cp(&cursor);
        if (cp=='\n' || cp=='\r') cp=' ';
        if (count==slots-1 && *cursor) {
            draw_char(ui,x+count*6*scale,y,'>',scale,color);
            break;
        }
        draw_char(ui,x+count*6*scale,y,glyph_char(cp),scale,color);
        count++;
    }
}

/* Word wrap without creating temporary copies of untrusted filenames. */
static void wrapped(FlUi *ui, int x, int y, int width, const char *s,
                    int scale, int lines, Uint32 color)
{
    const char *p=s?s:"";
    const int columns=width/(6*scale), line_height=9*scale;
    for (int line=0; line<lines && *p; line++) {
        const char *start=p, *end=p, *space=NULL, *after=NULL;
        int n=0;
        while (*end && *end!='\n' && n<columns) {
            const char *before=end;
            unsigned cp=next_cp(&end);
            if (cp==' ') { space=before; after=end; }
            n++;
        }
        if (*end && *end!='\n' && space && line+1<lines) {
            end=space; p=after;
        } else p=(*end=='\n')?end+1:end;
        int col=0;
        while (start<end && col<columns) {
            unsigned cp=next_cp(&start);
            if (line==lines-1 && col==columns-1 && *p) cp='>';
            draw_char(ui,x+col*6*scale,y+line*line_height,glyph_char(cp),scale,color);
            col++;
        }
        while (*p==' ') p++;
    }
}

FlUi *fl_ui_open(FlError *error)
{
    FlUi *ui;
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER) != 0) {
        fl_error(error, FL_ERR_IO, "Could not initialize SDL video.");
        return NULL;
    }
    ui=calloc(1,sizeof *ui);
    if (!ui) {
        SDL_Quit();
        fl_error(error, FL_ERR_IO, "Not enough memory for the display.");
        return NULL;
    }
    /* Onion's Miyoo display initialization uses 32 bpp. Requesting 16 bpp can
     * produce a corrupt scanout on vendor SDL/framebuffer combinations. */
    ui->display=SDL_SetVideoMode(640,480,32,SDL_SWSURFACE);
    if (!ui->display) {
        free(ui); SDL_Quit();
        fl_error(error, FL_ERR_IO, "Could not open the 640 x 480 display.");
        return NULL;
    }
    ui->screen=SDL_CreateRGBSurface(SDL_SWSURFACE,640,480,32,
                                    0x00ff0000,0x0000ff00,0x000000ff,0);
    ui->present=SDL_CreateRGBSurface(SDL_SWSURFACE,640,480,32,
                                    0x00ff0000,0x0000ff00,0x000000ff,0);
    if (!ui->screen || !ui->present) {
        if(ui->screen)SDL_FreeSurface(ui->screen);
        if(ui->present)SDL_FreeSurface(ui->present);
        free(ui); SDL_Quit();
        fl_error(error,FL_ERR_IO,"Could not allocate the display drawing buffer.");
        return NULL;
    }
#ifdef __arm__
    /* The physical Mini Plus panel scans out upside down. Keep the logical
     * canvas and input unchanged, rotate only the final presentation. */
    ui->rotate=true;
#endif
    const char *rotation=getenv("FL_DISPLAY_ROTATION");
    if(rotation && !strcmp(rotation,"180"))ui->rotate=true;
    else if(rotation && !strcmp(rotation,"0"))ui->rotate=false;
    SDL_WM_SetCaption("Friends Library",NULL);
    SDL_ShowCursor(SDL_DISABLE);
    SDL_EnableKeyRepeat(350,90);
    ui->bg=SDL_MapRGB(ui->screen->format,14,20,29);
    ui->panel=SDL_MapRGB(ui->screen->format,23,33,46);
    ui->text=SDL_MapRGB(ui->screen->format,238,244,250);
    ui->muted=SDL_MapRGB(ui->screen->format,156,176,193);
    ui->accent=SDL_MapRGB(ui->screen->format,99,218,184);
    ui->focus=SDL_MapRGB(ui->screen->format,34,68,77);
    ui->danger=SDL_MapRGB(ui->screen->format,255,164,135);
    return ui;
}

void fl_ui_close(FlUi *ui)
{
    if (!ui) return;
    SDL_FreeSurface(ui->screen);
    SDL_FreeSurface(ui->present);
    free(ui);
    SDL_Quit();
}

void fl_ui_describe(FlUi *ui,char *out,size_t capacity)
{
    char driver[80]="unknown";
    SDL_VideoDriverName(driver,sizeof(driver));
    if(!ui||!out||!capacity)return;
    snprintf(out,capacity,"version=%s\ndriver=%s\ndisplay=%dx%d bpp=%u pitch=%u flags=0x%08lx\ncanvas=%dx%d bpp=%u pitch=%u\nrotation=%u\n",
        FL_VERSION,driver,ui->display->w,ui->display->h,
        ui->display->format->BitsPerPixel,ui->display->pitch,(unsigned long)ui->display->flags,
        ui->screen->w,ui->screen->h,ui->screen->format->BitsPerPixel,ui->screen->pitch,
        ui->rotate?180:0);
}

int fl_ui_draw(FlUi *ui, const FlView *view, FlError *error)
{
    if (!ui || !view) {
        fl_error(error,FL_ERR_CONFIG,"Missing display or view.");
        return -1;
    }
    rect(ui,0,0,640,480,ui->bg);
    rect(ui,0,0,640,92,ui->panel);
    rect(ui,20,21,4,29,ui->accent);
    text(ui,36,22,584,view->title,3,ui->text);
    text(ui,22,64,596,view->subtitle,2,ui->muted);

    if(view->qr){
        int size=qrcodegen_getSize(view->qr),scale=264/(size+8);
        if(size<21||size>57||scale<1){fl_error(error,FL_ERR_FORMAT,"Cannot display the login QR.");return -1;}
        int width=(size+8)*scale,x=24+(264-width)/2,y=110+(276-width)/2;
        Uint32 white=SDL_MapRGB(ui->screen->format,255,255,255),black=SDL_MapRGB(ui->screen->format,0,0,0);
        rect(ui,x,y,width,width,white);
        for(int row=0;row<size;row++)for(int col=0;col<size;col++)
            if(qrcodegen_getModule(view->qr,col,row))rect(ui,x+(col+4)*scale,y+(row+4)*scale,scale,scale,black);
        if(view->details)for(size_t i=0;i<view->detail_count&&i<9;i++)text(ui,308,115+(int)i*30,306,view->details[i],2,i?ui->text:ui->accent);
    } else if (view->details && view->detail_count) {
        size_t lines=view->detail_count;
        if (lines>9) lines=9;
        for (size_t i=0; i<lines; i++)
            text(ui,24,112+(int)i*27,590,view->details[i],2,
                 (i==0)?ui->accent:ui->text);
    } else if (view->row_count && view->rows) {
        size_t scroll=view->scroll;
        if (scroll>=view->row_count) scroll=0;
        if (view->row_count>FL_UI_VISIBLE_ROWS &&
            scroll>view->row_count-FL_UI_VISIBLE_ROWS)
            scroll=view->row_count-FL_UI_VISIBLE_ROWS;
        for (size_t i=0; i<FL_UI_VISIBLE_ROWS && scroll+i<view->row_count; i++) {
            size_t row=scroll+i;
            bool chosen=(row==view->selected);
            const char *marker=view->markers?view->markers[row]:NULL;
            int y=104+(int)i*36;
            if (chosen) {
                rect(ui,16,y-3,600,33,ui->focus);
                rect(ui,16,y-3,4,33,ui->accent);
                text(ui,27,y+3,18,">",3,ui->accent);
            }
            text(ui,51,y+3,marker&&*marker?414:550,view->rows[row],3,ui->text);
            if (marker&&*marker) text(ui,476,y+7,132,marker,2,chosen?ui->accent:ui->muted);
        }
        if (view->row_count>FL_UI_VISIBLE_ROWS) {
            int track=288;
            int thumb=(int)(track*FL_UI_VISIBLE_ROWS/view->row_count);
            if (thumb<8) thumb=8;
            int top=(int)((track-thumb)*scroll/(view->row_count-FL_UI_VISIBLE_ROWS));
            rect(ui,626,101,3,track,ui->panel);
            rect(ui,626,101+top,3,thumb,ui->muted);
        }
    } else if (!view->progress) {
        text(ui,26,135,590,view->busy?"Loading...":
             view->error?"See the message below.":"No items to display.",3,ui->muted);
    }

    if (view->progress) {
        int fill=0;
        rect(ui,24,368,592,23,ui->panel);
        if (view->progress_fraction>=0) {
            double p=view->progress_fraction;
            if (p>1) p=1;
            fill=(int)(p*588);
            rect(ui,26,370,fill,19,ui->accent);
        } else {
            /* Deliberately static: unavailable totals are not fake progress. */
            for (int x=26; x<614; x+=24) rect(ui,x,375,12,9,ui->muted);
        }
    }
    rect(ui,20,405,600,1,ui->panel);
    wrapped(ui,22,416,596,view->status,2,2,view->error?ui->danger:ui->muted);
    rect(ui,0,458,640,22,ui->panel);
    text(ui,22,462,596,view->footer,2,ui->text);
    SDL_Surface *output=ui->screen;
    if(ui->rotate){
        /* Both surfaces are owned 32-bit software buffers. Honor pitch rather
         * than assuming tightly packed rows. SDL handles display conversion. */
        if(SDL_LockSurface(ui->screen)<0){fl_error(error,FL_ERR_IO,"Cannot lock drawing buffer.");return -1;}
        if(SDL_LockSurface(ui->present)<0){SDL_UnlockSurface(ui->screen);fl_error(error,FL_ERR_IO,"Cannot lock presentation buffer.");return -1;}
        for(int y=0;y<480;y++){
            const Uint32 *src=(const Uint32*)((const Uint8*)ui->screen->pixels+(479-y)*ui->screen->pitch);
            Uint32 *dst=(Uint32*)((Uint8*)ui->present->pixels+y*ui->present->pitch);
            for(int x=0;x<640;x++)dst[x]=src[639-x];
        }
        SDL_UnlockSurface(ui->present);SDL_UnlockSurface(ui->screen);output=ui->present;
    }
    if (SDL_BlitSurface(output,NULL,ui->display,NULL)<0 || SDL_Flip(ui->display)<0) {
        fl_error(error,FL_ERR_IO,"Could not update the display.");
        return -1;
    }
    return 0;
}

static FlKey key_event(const SDL_Event *event)
{
    if (event->type==SDL_QUIT) return FL_KEY_EXIT;
    if (event->type!=SDL_KEYDOWN) return FL_KEY_NONE;
    /* Miyoo mapping: Onion v4.3.0 src/common/system/keymap_sw.h. */
    switch (event->key.keysym.sym) {
    case SDLK_UP: return FL_KEY_UP;
    case SDLK_DOWN: return FL_KEY_DOWN;
    case SDLK_LEFT: return FL_KEY_LEFT;
    case SDLK_RIGHT: return FL_KEY_RIGHT;
    case SDLK_SPACE: case SDLK_RETURN: case SDLK_a: return FL_KEY_OPEN;
    case SDLK_LCTRL: case SDLK_ESCAPE: case SDLK_b: return FL_KEY_BACK;
    case SDLK_LSHIFT: case SDLK_x: case SDLK_r: return FL_KEY_REFRESH;
    case SDLK_RCTRL: case SDLK_d: return FL_KEY_DISCONNECT;
    case SDLK_TAB: case SDLK_e: case SDLK_PAGEUP: return FL_KEY_PAGE_UP;
    case SDLK_BACKSPACE: case SDLK_t: case SDLK_PAGEDOWN: return FL_KEY_PAGE_DOWN;
    case SDLK_q: return FL_KEY_EXIT;
    default: return FL_KEY_NONE;
    }
}

FlKey fl_ui_wait_event(FlUi *ui, int timeout_ms)
{
    SDL_Event event;
    Uint32 start=SDL_GetTicks();
    (void)ui;
    for (;;) {
        if (timeout_ms<0) {
            if (!SDL_WaitEvent(&event)) return FL_KEY_EXIT;
            FlKey key=key_event(&event);
            if (key!=FL_KEY_NONE) return key;
        } else {
            while (SDL_PollEvent(&event)) {
                FlKey key=key_event(&event);
                if (key!=FL_KEY_NONE) return key;
            }
            Uint32 elapsed=SDL_GetTicks()-start;
            if (elapsed>=(Uint32)timeout_ms) return FL_KEY_NONE;
            Uint32 remaining=(Uint32)timeout_ms-elapsed;
            SDL_Delay(remaining>10?10:remaining);
        }
    }
}

int fl_ui_screenshot(FlUi *ui, const char *path, FlError *error)
{
    if (!ui || !path || SDL_SaveBMP(ui->screen,path)<0) {
        fl_error(error,FL_ERR_IO,"Could not save the display preview.");
        return -1;
    }
    return 0;
}
