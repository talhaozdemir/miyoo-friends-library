#include "fl_ui.h"
#include "fl_https.h"
#include <SDL/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void press(SDLKey key, FlKey expected)
{
    SDL_Event e;
    memset(&e,0,sizeof e);
    e.type=SDL_KEYDOWN;
    e.key.keysym.sym=key;
    assert(SDL_PushEvent(&e)==0);
    assert(fl_ui_wait_event(NULL,100)==expected);
}

static void verify_rotation(FlUi *ui,const FlView *view)
{
    assert(fl_ui_draw(ui,view,NULL)==0);
    assert(fl_ui_screenshot(ui,"build/ui-rotation-logical.bmp",NULL)==0);
    SDL_Surface *display=SDL_GetVideoSurface();
    SDL_Surface *bitmap=SDL_LoadBMP("build/ui-rotation-logical.bmp");assert(bitmap);
    SDL_Surface *reference=SDL_ConvertSurface(bitmap,display->format,SDL_SWSURFACE);assert(reference);
    SDL_FreeSurface(bitmap);
    assert(display->format->BytesPerPixel==4 && reference->format->BytesPerPixel==4);
    assert(SDL_LockSurface(reference)==0);assert(SDL_LockSurface(display)==0);
    Uint32 rgb=display->format->Rmask|display->format->Gmask|display->format->Bmask;
    /* Compare every presented pixel, including asymmetric title/footer and
     * focus. This catches flips, wrong row stride and unrotated presentation. */
    for(int y=0;y<480;y++)for(int x=0;x<640;x++){
        const Uint32 *expected=(const Uint32*)((const Uint8*)reference->pixels+(479-y)*reference->pitch);
        const Uint32 *actual=(const Uint32*)((const Uint8*)display->pixels+y*display->pitch);
        assert((actual[x]&rgb)==(expected[639-x]&rgb));
    }
    SDL_UnlockSurface(display);SDL_UnlockSurface(reference);SDL_FreeSurface(reference);
    assert(SDL_SaveBMP(display,"build/ui-rotation-presented.bmp")==0);
}

int main(void)
{
    static char driver[]="SDL_VIDEODRIVER=dummy";
    FlError error={0};
    FlView view={0};
    const char *rows[]={"TALHA", "FRIEND_2", "FRIEND_3",
        "Tiny Toon Adventures", "Doki Doki Yuuenchi", "\xc4\xb0\xc4\x9f\xc5\x9f\xc3\xa7",
        "A deliberately long filename which must be clipped without overflowing",
        "\xf0\x9f\x8e\xae Library", "\xe3\x81\x82", "\xf0\x9f", "Final item"};
    const char *markers[]={"", "", "", "INSTALLED", "EXISTS", "", "", "", "", "", ""};
    const char *details[]={"Tiny Toon Adventures", "Console: NES",
        "Shared by: TALHA", "Size: 256 KiB", "Free: 7.5 GiB",
        "Destination: Roms/FC/", "Download: 128 KiB / 256 KiB", "Speed: 42 KiB/s"};
    SDL_putenv(driver);
    FlUi *ui=fl_ui_open(&error);
    assert(ui!=NULL);
    /* Hardware bug regression: host dummy accepts 16 bpp, but the Miyoo
     * framebuffer path expects 32 bpp. Make the chosen mode explicit. */
    SDL_Surface *display=SDL_GetVideoSurface();
    assert(display && display->w==640 && display->h==480);
    assert(display->format->BitsPerPixel==32);

    view.title="Friends Library";
    view.subtitle="Private shared collection";
    view.footer="A Open   B Back   X Refresh";
    view.status="Cached library. Connect Wi-Fi to download.";
    view.rows=rows; view.markers=markers;
    view.row_count=sizeof rows/sizeof rows[0];
    view.selected=9; view.scroll=7;
    assert(fl_ui_draw(ui,&view,&error)==0);
    assert(fl_ui_screenshot(ui,"build/ui-list.bmp",&error)==0);

    /* Check the physical Miyoo events, rather than desktop-only controls. */
    press(SDLK_SPACE,FL_KEY_OPEN);
    press(SDLK_LCTRL,FL_KEY_BACK);
    press(SDLK_LSHIFT,FL_KEY_REFRESH);
    press(SDLK_RCTRL,FL_KEY_DISCONNECT);
    press(SDLK_UP,FL_KEY_UP);
    press(SDLK_DOWN,FL_KEY_DOWN);
    press(SDLK_TAB,FL_KEY_PAGE_UP);
    press(SDLK_BACKSPACE,FL_KEY_PAGE_DOWN);
    assert(fl_ui_wait_event(ui,0)==FL_KEY_NONE);

    view.rows=NULL; view.row_count=0; view.details=details;
    view.detail_count=sizeof details/sizeof details[0];
    view.progress=true; view.progress_fraction=0.5;
    view.status="Downloading...";
    view.footer="B Cancel";
    assert(fl_ui_draw(ui,&view,&error)==0);
    assert(fl_ui_screenshot(ui,"build/ui-download.bmp",&error)==0);
    view.progress_fraction=-1;
    assert(fl_ui_draw(ui,&view,&error)==0);
    view.progress_fraction=99;
    assert(fl_ui_draw(ui,&view,&error)==0);

    view.details=NULL; view.detail_count=0; view.progress=false;
    view.error=true; view.busy=false;
    view.title="Authentication required";
    view.status="Authorize Google on your computer, then copy your private configuration to the SD card.";
    assert(fl_ui_draw(ui,&view,&error)==0);
    assert(fl_ui_screenshot(ui,"build/ui-error.bmp",&error)==0);
    assert(fl_ui_draw(NULL,&view,&error)==-1);
    assert(fl_ui_screenshot(NULL,"build/invalid.bmp",&error)==-1);
    uint8_t login_qr[FL_QR_BYTES];assert(fl_https_qr("https://test.workers.dev/pair/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",login_qr)==0);
    const char *qr_lines[]={"SCAN AND OPEN","Compare the code.","Use phone browser.","Invited Google account.","","QR lasts 5 minutes.","Keep this QR private."};
    view.error=false;view.qr=login_qr;view.details=qr_lines;view.detail_count=7;view.title="FRIENDS LIBRARY";view.subtitle="GOOGLE LOGIN / PHONE";view.status="First time? X: phone setup guide.";view.footer="B: CANCEL LOGIN   X: PHONE GUIDE";
    assert(fl_ui_draw(ui,&view,&error)==0);assert(fl_ui_screenshot(ui,"build/ui-phone-qr.bmp",&error)==0);view.qr=NULL;
    fl_ui_close(ui);
    static char rotation[]="FL_DISPLAY_ROTATION=180";
    SDL_putenv(rotation);ui=fl_ui_open(&error);assert(ui);
    char diagnostic[512];fl_ui_describe(ui,diagnostic,sizeof(diagnostic));
    assert(strstr(diagnostic,"rotation=180"));
    verify_rotation(ui,&view);
    press(SDLK_UP,FL_KEY_UP);press(SDLK_SPACE,FL_KEY_OPEN);press(SDLK_LCTRL,FL_KEY_BACK);
    fl_ui_close(ui);
    static char upright[]="FL_DISPLAY_ROTATION=0";SDL_putenv(upright);
    puts("UI smoke: physical buttons, UTF-8 clipping, list/details/error and progress passed");
    puts("UI rotation: all 307200 presented pixels match 180-degree reference; controls unchanged");
    return 0;
}
