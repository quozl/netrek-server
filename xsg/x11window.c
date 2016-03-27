/* x11window.c
 *
 * Kevin P. Smith  6/11/89
 */
#include "copyright2.h"
#include <stdio.h>
/*
NOTE: If running an older version of DECwindows you may need this.
#define NeedFunctionPrototypes 0
*/
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <assert.h>
#include <stdlib.h>
#include "Wlib.h"
#include "defs.h"		/* TSH */
#include "xsg_defs.h"
#include "struct.h"
#include "localdata.h"

#define FOURPLANEFIX

#define FONTS 4
#define BITGC 4

#define WHITE   0
#define BLACK   1
#define RED     2
#define GREEN   3
#define YELLOW  4
#define CYAN    5
#define GREY	6

static int zero=0;
static int one=1;
static int two=2;
static int three=3;
int W_FastClear=0;
extern char *serverName;

Display *W_Display;
Window W_Root;
Colormap W_Colormap;
int W_Screen;
Visual *W_Visual;
W_Font W_BigFont= (W_Font) &zero, W_RegularFont= (W_Font) &one;
W_Font W_HighlightFont= (W_Font) &two, W_UnderlineFont= (W_Font) &three;
W_Color W_White=WHITE, W_Black=BLACK, W_Red=RED, W_Green=GREEN;
W_Color W_Yellow=YELLOW, W_Cyan=CYAN, W_Grey=GREY;
int W_Textwidth, W_Textheight;
char *getdefault();

static XClassHint class_hint = {
	"xsg", "X Show Galaxy",
};

static XWMHints wm_hint = {
	InputHint|StateHint,
	True,
	NormalState,
	None,
	None,
	0,0,
	None,
	None,
};    

static W_Event W_myevent;
static int W_isEvent=0;

struct fontInfo {
    int baseline;
};

struct colors {
    char *name;
    GC contexts[FONTS+1];
    Pixmap pixmap;
    int pixelValue;
};

struct icon {
    Window window;
    Pixmap bitmap;
    int width, height;
    Pixmap pixmap;
};

#define WIN_GRAPH	1
#define WIN_TEXT	2
#define WIN_MENU	3
#define WIN_SCROLL	4

struct window {
    Window window;
    int type;
    char *data;
    int mapped;
    int width,height;
    char *name;
};

struct stringList {
    char *string;
    W_Color color;
    struct stringList *next;
};

struct menuItem {
    char *string;
    W_Color color;
};

struct colors colortable[] = {
    { "white" 	   },
    { "black"	   },
    { "red" 	   },
    { "green" 	   },
    { "yellow" 	   },
    { "cyan" 	   },
    { "light grey" }
};

struct windowlist {
    struct window *window;
    struct windowlist *next;
};

#define HASHSIZE 29
#define hash(x) (((int) (x)) % HASHSIZE)

struct windowlist *hashtable[HASHSIZE];
struct fontInfo fonts[FONTS];

struct window *newWindow();
struct window *findWindow();
short *x11tox10bits();

struct window myroot;

#define NCOLORS (sizeof(colortable)/sizeof(colortable[0]))
#define W_Void2Window(win) ((win) ? (struct window *) (win) : &myroot)
#define W_Window2Void(window) ((W_Window) (window))
#define W_Void2Icon(bit) ((struct icon *) (bit))
#define W_Icon2Void(bit) ((W_Icon) (bit))
#define fontNum(font) (*((int *) font))
#define TILESIDE 16

#define WIN_EDGE 5	/* border on l/r edges of text windows */
#define MENU_PAD 4	/* border on t/b edges of text windows */
#define MENU_BAR 4	/* width of menu bar */

static unsigned char gray[] = {
	0xaa, 0xaa, 0x55, 0x55, 0xaa, 0xaa, 0x55, 0x55,
	0xaa, 0xaa, 0x55, 0x55, 0xaa, 0xaa, 0x55, 0x55,
	0xaa, 0xaa, 0x55, 0x55, 0xaa, 0xaa, 0x55, 0x55,
	0xaa, 0xaa, 0x55, 0x55, 0xaa, 0xaa, 0x55, 0x55
};

static unsigned char striped[] = {
	0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff,
	0xf0, 0x0f, 0xf0, 0x0f, 0xf0, 0x0f, 0xf0, 0x0f,
	0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00,
	0x0f, 0xf0, 0x0f, 0xf0, 0x0f, 0xf0, 0x0f, 0xf0
};

static unsigned char solid[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

void W_Initialize(str)
char *str;
{
    int i;

#ifdef DEBUG
    printf("Initializing...\n");
#endif
    for (i=0; i<HASHSIZE; i++) {
	hashtable[i]=NULL;
    }
    if ((W_Display = XOpenDisplay(str)) == NULL) {
	fprintf(stderr, "I cannot open your display!\n");
	exit(1);
    }
    /*XSynchronize(W_Display, True);*/
    W_Root=DefaultRootWindow(W_Display);
    W_Visual = DefaultVisual(W_Display, DefaultScreen(W_Display));
    W_Screen=DefaultScreen(W_Display);
    W_Colormap=DefaultColormap(W_Display, W_Screen);
    myroot.window=W_Root;
    myroot.type=WIN_GRAPH;
    GetFonts();
    GetColors();
}

GetFonts()
{
    Font regular, italic, bold, big;
    int i,j;
    XGCValues values;
    XFontStruct *fontinfo;
    char *fontname;

    fontname=getdefault("font");
    if (fontname==NULL) fontname="6x10";
    fontinfo=XLoadQueryFont(W_Display, fontname);
    if (fontinfo==NULL) {
	printf("Couldn't find 6x10 font!\n");
	exit(1);
    }
    regular=fontinfo->fid;
    W_Textwidth=fontinfo->max_bounds.width;
    W_Textheight=fontinfo->max_bounds.descent+fontinfo->max_bounds.ascent;
    fonts[1].baseline=fontinfo->max_bounds.ascent;

    fontname=getdefault("boldfont");
    if (fontname==NULL) fontname="6x10B";
    fontinfo=XLoadQueryFont(W_Display, fontname);
    if (fontinfo==NULL) {
	bold=regular;
	fonts[2].baseline=fonts[1].baseline;
    } else {
	bold=fontinfo->fid;
	fonts[2].baseline=fontinfo->max_bounds.ascent;
	if (fontinfo->max_bounds.width > W_Textwidth) 
	    W_Textwidth=fontinfo->max_bounds.width;
	if (fontinfo->max_bounds.descent+fontinfo->max_bounds.ascent > W_Textheight)
	    W_Textheight=fontinfo->max_bounds.descent+fontinfo->max_bounds.ascent;
    }

    fontname=getdefault("italicfont");
    if (fontname==NULL) fontname="6x10I";
    fontinfo=XLoadQueryFont(W_Display, "6x10I");
    if (fontinfo==NULL) {
	italic=regular;
	fonts[3].baseline=fonts[1].baseline;
    } else {
	italic=fontinfo->fid;
	fonts[3].baseline=fontinfo->max_bounds.ascent;
	if (fontinfo->max_bounds.width > W_Textwidth) 
	    W_Textwidth=fontinfo->max_bounds.width;
	if (fontinfo->max_bounds.descent+fontinfo->max_bounds.ascent > W_Textheight)
	    W_Textheight=fontinfo->max_bounds.descent+fontinfo->max_bounds.ascent;
    }

    fontname=getdefault("bigfont");
    if (fontname==NULL) fontname="charB24";
    fontinfo=XLoadQueryFont(W_Display, fontname);
    if (fontinfo==NULL) {
	big=regular;
	fonts[0].baseline=fonts[1].baseline;
    } else {
	big=fontinfo->fid;
	fonts[0].baseline=fontinfo->max_bounds.ascent;
    }
    for (i=0; i<NCOLORS; i++) {
	values.font=big;
	colortable[i].contexts[0]=XCreateGC(W_Display, W_Root, GCFont, &values);
	XSetGraphicsExposures(W_Display, colortable[i].contexts[0], False);
	values.font=regular;
	colortable[i].contexts[1]=XCreateGC(W_Display, W_Root, GCFont, &values);
	XSetGraphicsExposures(W_Display, colortable[i].contexts[1], False);
	values.font=bold;
	colortable[i].contexts[2]=XCreateGC(W_Display, W_Root, GCFont, &values);
	XSetGraphicsExposures(W_Display, colortable[i].contexts[2], False);
	values.font=italic;
	colortable[i].contexts[3]=XCreateGC(W_Display, W_Root, GCFont, &values);
	XSetGraphicsExposures(W_Display, colortable[i].contexts[3], False);
	values.function=GXor;
	colortable[i].contexts[BITGC]=XCreateGC(W_Display, W_Root, GCFunction,
	    &values);
	XSetGraphicsExposures(W_Display, colortable[i].contexts[BITGC], False);
        {	/* tract/press TSH 2/10/93 */
           static char dl[] = { 1, 8 };
           /* was 3 */
           XSetLineAttributes(W_Display, colortable[i].contexts[BITGC],
            0, LineOnOffDash, CapButt, JoinMiter);
           XSetDashes(W_Display, colortable[i].contexts[BITGC], 0, dl, 2);
        }

    }
}

static unsigned short extrared[8]=   {0x00, 0x20, 0x40, 0x60, 0x80, 0xa0, 0xb0, 
0xc0};
static unsigned short extragreen[8]= {0x40, 0x60, 0x80, 0xa0, 0xb0, 0xc0, 0x00, 
0x20};
static unsigned short extrablue[8]=  {0x80, 0xa0, 0xb0, 0xc0, 0x00, 0x20, 0x40, 
0x60};


GetColors()
{
    int i,j;
    XColor foo;
    int white, black;
    unsigned long pixel;
    unsigned long planes[3];
    char defaultstring[100];
    char *defaults;
    unsigned long extracolors[8];
    XColor colordef;

    if (DisplayCells(W_Display, W_Screen) <= 2) {
	white=WhitePixel(W_Display, W_Screen);
	black=BlackPixel(W_Display, W_Screen);
	for (i=0; i<NCOLORS; i++) {
	    if (i != W_Black) {
		colortable[i].pixelValue=white;
	    } else {
		colortable[i].pixelValue=black;
	    }
	    if (i==W_Red) {
		colortable[i].pixmap=XCreatePixmapFromBitmapData(W_Display,
		    W_Root, (char *) striped, TILESIDE, TILESIDE,
		    white, black,
		    DefaultDepth(W_Display, W_Screen));
	    } else if (i==W_Yellow) {
		colortable[i].pixmap=XCreatePixmapFromBitmapData(W_Display,
		    W_Root, (char *) gray, TILESIDE, TILESIDE,
		    white, black,
		    DefaultDepth(W_Display, W_Screen));
	    } else {
		colortable[i].pixmap=XCreatePixmapFromBitmapData(W_Display,
		    W_Root, (char *) solid, TILESIDE, TILESIDE, 
		    colortable[i].pixelValue,
		    colortable[i].pixelValue, 
		    DefaultDepth(W_Display, W_Screen));
	    }
	    /* We assume white is 0 or 1, and black is 0 or 1.
	     * We adjust graphics function based upon who is who.
	     */
	    if (white==0) {	/* Black is 1 */
		XSetFunction(W_Display, colortable[i].contexts[BITGC], 
		    GXand);
	    }
	}
    } else if (DefaultVisual(W_Display, W_Screen)->class == TrueColor) {
/* Stuff added by sheldon@iastate.edu 5/28/93
 * This is supposed to detect a TrueColor display, and then do a lookup of
 * the colors in default colormap, instead of creating new colormap
 */
        for (i=0; i<NCOLORS; i++) {
          sprintf(defaultstring, "color.%s", colortable[i].name);

          defaults=getdefault(defaultstring);
          if (defaults==NULL) defaults=colortable[i].name;
          XParseColor(W_Display, W_Colormap, defaults, &foo);
          XAllocColor(W_Display, W_Colormap, &foo);
          colortable[i].pixelValue = foo.pixel;
          colortable[i].pixmap = XCreatePixmapFromBitmapData(W_Display,
          W_Root, (char *) solid, TILESIDE, TILESIDE, foo.pixel, foo.pixel, 
          DefaultDepth(W_Display, W_Screen));
        }
    }  else {
        if(!XAllocColorCells(W_Display, W_Colormap, False, planes, 3,
           &pixel, 1)){
           /* couldn't allocate 3 planes, make a new colormap */
           W_Colormap = XCreateColormap(W_Display, W_Root, W_Visual, AllocNone);
           if (! XAllocColorCells(W_Display, W_Colormap, False, planes, 3,
                &pixel, 1)){
               fprintf(stderr,"Cannot create new colormap\n");
               exit(1);
           }
           /* and fill it with at least 8 more colors so when mouse is inside
              netrek windows, use might be able to see his other windows */
           if (XAllocColorCells(W_Display, W_Colormap, False, NULL, 0,
                extracolors, 8)){
               colordef.flags = DoRed|DoGreen|DoBlue;
               for (i=0; i<8; i++){
                   colordef.pixel = extracolors[i];
                   colordef.red = extrared[i]<<8;
                   colordef.green = extragreen[i]<<8;
                   colordef.blue = extrablue[i]<<8;
                   XStoreColor(W_Display, W_Colormap, &colordef);
               }
           }
        }
	for (i=0; i<NCOLORS; i++) {
	    sprintf(defaultstring, "color.%s", colortable[i].name);
	    defaults=getdefault(defaultstring);
	    if (defaults==NULL) defaults=colortable[i].name;
	    XParseColor(W_Display, W_Colormap, defaults, &foo);
	    /* Black must be the color with all the planes off.
	     * That is the only restriction I concerned myself with in
	     *  the following case statement.
	     */
	    switch(i) {
	    case WHITE:
		foo.pixel=pixel|planes[0]|planes[1]|planes[2];
		break;
	    case BLACK:
		foo.pixel=pixel;
		break;
	    case RED:
		foo.pixel=pixel|planes[0];
		break;
	    case CYAN:
		foo.pixel=pixel|planes[1];
		break;
	    case YELLOW:
		foo.pixel=pixel|planes[2];
		break;
	    case GREY:
		foo.pixel=pixel|planes[0]|planes[1];
		break;
	    case GREEN:
		foo.pixel=pixel|planes[1]|planes[2];
		break;
	    }
	    XStoreColor(W_Display, W_Colormap, &foo);
	    colortable[i].pixelValue = foo.pixel;
	    colortable[i].pixmap = XCreatePixmapFromBitmapData(W_Display,
		W_Root, (char *) solid, TILESIDE, TILESIDE, foo.pixel, foo.pixel,
		DefaultDepth(W_Display, W_Screen));
	}
    }
    for (i=0; i<NCOLORS; i++) {
	for (j=0; j<FONTS+1; j++) {
	    XSetForeground(W_Display, colortable[i].contexts[j], 
		colortable[i].pixelValue);
	    XSetBackground(W_Display, colortable[i].contexts[j],
		colortable[W_Black].pixelValue);
	}
    }
}

W_Window W_MakeWindow(name,x,y,width,height,parent,border,color)
char *name;
int x,y,width,height;
W_Window parent;
int border;
W_Color color;
{
    struct window *newwin;
    Window wparent;
    XSetWindowAttributes attrs;
	
#ifdef DEBUG
    printf("New window...\n");
#endif
    checkGeometry(name, &x, &y, &width, &height);
    checkParent(name, &parent);
    wparent=W_Void2Window(parent)->window;
    attrs.override_redirect=False;
    attrs.border_pixel=colortable[color].pixelValue;
    attrs.event_mask=KeyPressMask|ButtonPressMask|ExposureMask;
    attrs.background_pixel=colortable[W_Black].pixelValue;
    attrs.do_not_propagate_mask=KeyPressMask|ButtonPressMask|ExposureMask;
    newwin=newWindow(
	XCreateWindow(W_Display, wparent, x, y, width, height, border,
	    CopyFromParent, InputOutput, CopyFromParent, 
	    CWBackPixel|CWEventMask|CWOverrideRedirect|CWBorderPixel, 
	    &attrs),
	WIN_GRAPH);
    XSetClassHint(W_Display, newwin->window, &class_hint);
    XSetWMHints(W_Display, newwin->window, &wm_hint);
    /*
    if (!strcmp(name, "XShowGalaxy"))
      XStoreName(W_Display, newwin->window, serverName);
   */
    XStoreName(W_Display, newwin->window, name);	/* TSH 2/10/93 */
    newwin->name=strdup(name);
    newwin->width=width;
    newwin->height=height;
    if (checkMapped(name)) W_MapWindow(W_Window2Void(newwin));

#ifdef DEBUG
    printf("New graphics window %d, child of %d\n", newwin, parent);
#endif

    return(W_Window2Void(newwin));
}

void W_ChangeBorder(window, color)
W_Window window;
int color;
{
#ifdef DEBUG
    printf("Changing border of %d\n", window);
#endif
	
    XSetWindowBorderPixmap(W_Display, W_Void2Window(window)->window,
	colortable[color].pixmap);
}

void W_MapWindow(window)
W_Window window;
{
    struct window *win;

#ifdef DEBUG
    printf("Mapping %d\n", window);
#endif
    win=W_Void2Window(window);
    if (win->mapped) return;
    win->mapped=1;
    XMapRaised(W_Display, win->window);
}

void W_UnmapWindow(window)
W_Window window;
{
    struct window *win;

#ifdef DEBUG
    printf("UnMapping %d\n", window);
#endif
    win=W_Void2Window(window);
    if (win->mapped==0) return;
    win->mapped=0;
    XUnmapWindow(W_Display, win->window);
}

int W_IsMapped(window)
W_Window window;
{
    struct window *win;

    win=W_Void2Window(window);
    if (win==NULL) return(0);
    return(win->mapped);
}

void W_ClearArea(window, x, y, width, height, color)
W_Window window;
int x, y, width, height;
W_Color color;
{
    struct window *win;

#ifdef DEBUG
    printf("Clearing (%d %d) x (%d %d) with %d on %d\n", x,y,width,height,
	color,window);
#endif
    win=W_Void2Window(window);
    switch(win->type) {
    case WIN_GRAPH:
        XClearArea(W_Display, win->window, x,y,width,height,False);

	break;
    default:
        XClearArea(W_Display, win->window, WIN_EDGE+x*W_Textwidth, 
         MENU_PAD+y*W_Textheight, width*W_Textwidth,height*W_Textheight, False);
	break;
    }
}

void W_FillArea(window, x, y, width, height, color)
W_Window window;
int x, y, width, height;
W_Color color;
{
    struct window *win;
    
#ifdef DEBUG
    printf("Clearing (%d %d) x (%d %d) with %d on %d\n", x,y,width,height,
           color,window);
#endif
    win=W_Void2Window(window);
    switch(win->type) {
    case WIN_GRAPH:
        XFillRectangle(W_Display, win->window, colortable[color].contexts[0],
                       x, y, width, height);
        break;
    default:
        XFillRectangle(W_Display, win->window, colortable[color].contexts[0],
                       WIN_EDGE+x*W_Textwidth, MENU_PAD+y*W_Textheight, 
                       width*W_Textwidth, height*W_Textheight);
    }
}

void W_ClearWindow(window)
W_Window window;
{
#ifdef DEBUG
    printf("Clearing %d\n", window);
#endif
    XClearWindow(W_Display, W_Void2Window(window)->window);
}


int W_EventsPending()
{
    if (W_isEvent) return(1);
    while (XPending(W_Display)) {
	if (W_SpNextEvent(&W_myevent)) {
	    W_isEvent=1;
	    return(1);
	}
    }
    return(0);
}

void W_NextEvent(wevent)
W_Event *wevent;
{
    if (W_isEvent) {
	*wevent=W_myevent;
	W_isEvent=0;
	return;
    }
    while (W_SpNextEvent(wevent)==0);
}

int W_SpNextEvent(wevent)
W_Event *wevent;
{
    XEvent event;
    XKeyEvent *key;
    XButtonEvent *button;
    XExposeEvent *expose;
    XResizeRequestEvent *resize;
    char ch;
    int nchars;
    struct window *win;

#ifdef DEBUG
    printf("Getting an event...\n");
#endif
    key=(XKeyEvent *) &event;
    button=(XButtonEvent *) &event;
    expose=(XExposeEvent *) &event;
    resize=(XResizeRequestEvent *) &event;
    for (;;) {
	XNextEvent(W_Display, &event);
	win=findWindow(key->window);
        if (win==NULL) return(0);
	if (event.type==KeyPress || event.type==ButtonPress){
	    if(win->type == WIN_MENU) {
	       if (key->y % (W_Textheight + MENU_PAD*2+MENU_BAR) >= 
		   W_Textheight+MENU_PAD*2) return(0);
	       key->y=key->y/(W_Textheight+MENU_PAD*2+MENU_BAR);
	    }
	    else if(win->type == WIN_TEXT) {
	       key->y=(key->y-MENU_PAD)/W_Textheight;
	    }
	}
	switch((int) event.type) {
	case KeyPress:
	    if (XLookupString(key,&ch,1,NULL,NULL)>0) {
		wevent->type=W_EV_KEY;
		wevent->Window=W_Window2Void(win);
		wevent->x=key->x;
		wevent->y=key->y;
		wevent->key=ch;
		return(1);
	    }
	    return(0);
	    break;
	case ButtonPress:
	    wevent->type=W_EV_BUTTON;
	    wevent->Window=W_Window2Void(win);
	    wevent->x=button->x;
	    wevent->y=button->y;
	    switch(button->button & 0xf) {
	    case Button3:
		wevent->key=W_RBUTTON;
		break;
	    case Button1:
		wevent->key=W_LBUTTON;
		break;
	    case Button2:
		wevent->key=W_MBUTTON;
		break;
	    }
	    return(1);
	case Expose:
	    if (expose->count != 0) return(0);
	    if (win->type == WIN_SCROLL) {
		redrawScrolling(win);
		return(0);
	    }
	    if (win->type == WIN_MENU) {
		redrawMenu(win);
		return(0);
	    }
	    wevent->type=W_EV_EXPOSE;
	    wevent->Window=W_Window2Void(win);
	    return(1);
	case ResizeRequest:
	    resizeScrolling(win, resize->width, resize->height);
	    break;
	default:
	    return(0);
	    break;
	}
    }
}

void W_MakeLine(window, x0, y0, x1, y1, color)
W_Window window;
int x0, y0, x1, y1;
W_Color color;
{
    Window win;

#ifdef DEBUG
    printf("Line on %d\n", window);
#endif
    win=W_Void2Window(window)->window;
    XDrawLine(W_Display, win, colortable[color].contexts[0], x0, y0, x1, y1);
}

/* TSH 2/10/93 */
void W_MakeTractLine(window, x0, y0, x1, y1, color)
W_Window window;
int x0, y0, x1, y1;
W_Color color;
{
    Window win;

#ifdef DEBUG
    printf("Line on %d\n", window);
#endif
    win=W_Void2Window(window)->window;
    XDrawLine(W_Display, win, colortable[color].contexts[BITGC], x0, y0, x1, y1)
;
}

void W_MakePoint(window, x, y, color)
W_Window window;
int	x,y;
W_Color color;
{
   Window	win;

   win = W_Void2Window(window)->window;
   XDrawPoint(W_Display, win, colortable[color].contexts[0], x,y);
}


void W_WriteText(window, x, y, color, str, len, font)
W_Window window;
int x, y,len;
W_Color color;
W_Font font;
char *str;
{
    struct window *win;
    int addr;

#ifdef DEBUG
    printf("Text for %d @ (%d, %d) in %d: [%s]\n", window, x,y,color,str);
#endif
    win=W_Void2Window(window);
    switch(win->type) {
    case WIN_GRAPH:
	addr=fonts[fontNum(font)].baseline;
	XDrawImageString(W_Display, win->window, 
	    colortable[color].contexts[fontNum(font)],x,y+addr,str,len);
	break;
    case WIN_SCROLL:
	XCopyArea(W_Display, win->window, win->window,
	    colortable[W_White].contexts[0], WIN_EDGE, MENU_PAD+W_Textheight,
	    win->width*W_Textwidth, (win->height-1)*W_Textheight,
	    WIN_EDGE, MENU_PAD);
	XClearArea(W_Display, win->window, 
	    WIN_EDGE, MENU_PAD+W_Textheight*(win->height-1),
	    W_Textwidth*win->width, W_Textheight, False);
	XDrawImageString(W_Display, win->window,
	    colortable[color].contexts[1],
	    WIN_EDGE, MENU_PAD+W_Textheight*(win->height-1)+fonts[1].baseline,
	    str, len);
	AddToScrolling(win, color, str, len);
	break;
    case WIN_MENU:
	changeMenuItem(win, y, str, len, color);
	break;
    default:
	addr=fonts[fontNum(font)].baseline;
	XDrawImageString(W_Display, win->window,
	    colortable[color].contexts[fontNum(font)],
	    x*W_Textwidth+WIN_EDGE, MENU_PAD+y*W_Textheight+addr,
	    str, len);
	break;
    }
}

void W_MaskText(window, x, y, color, str, len, font)
W_Window window;
int x, y,len;
W_Color color;
W_Font font;
char *str;
{
    struct window *win;
    int addr;

    addr=fonts[fontNum(font)].baseline;
#ifdef DEBUG
    printf("TextMask for %d @ (%d, %d) in %d: [%s]\n", window, x,y,color,str);
#endif
    win=W_Void2Window(window);
    XDrawString(W_Display, win->window, 
	colortable[color].contexts[fontNum(font)], x,y+addr, str, len);
}

W_Icon W_StoreBitmap(width, height, data, window)
int width, height;
W_Window window;
char *data;
{
    struct icon *newicon;
    struct window *win;

#ifdef DEBUG
    printf("Storing bitmap for %d (%d x %d)\n", window,width,height);
    fflush(stdout);
#endif
    win=W_Void2Window(window);
    newicon=(struct icon *) malloc(sizeof(struct icon));
    newicon->width=width;
    newicon->height=height;
    newicon->bitmap=XCreateBitmapFromData(W_Display, win->window,
	data, width, height);
    newicon->window=win->window;
    newicon->pixmap=0;

    return(W_Icon2Void(newicon));
}

void W_WriteBitmap(x, y, bit, color)
int x,y;
W_Icon bit;
W_Color color;
{
    struct icon *icon;

    icon=W_Void2Icon(bit);
#ifdef DEBUG
    printf("Writing bitmap to %d\n", icon->window);
#endif
    XCopyPlane(W_Display, icon->bitmap, icon->window,
	colortable[color].contexts[BITGC], 0, 0, icon->width, icon->height,
	x, y, 1);
#ifdef DEBUG
    printf("Written (icon->width=%d, icon->height=%d).\n", icon->width,
	icon->height);
#endif
}

void W_TileWindow(window, bit)
W_Window window;
W_Icon bit;
{
    Window win;
    struct icon *icon;

#ifdef DEBUG
    printf("Tiling window %d\n", window);
#endif
    icon=W_Void2Icon(bit);
    win=W_Void2Window(window)->window;

    if (icon->pixmap==0) {
	icon->pixmap=XCreatePixmap(W_Display, W_Root,
	    icon->width, icon->height, DefaultDepth(W_Display, W_Screen));
	XCopyPlane(W_Display, icon->bitmap, icon->pixmap, 
	    colortable[W_White].contexts[0], 0, 0, icon->width, icon->height,
	    0, 0, 1);
    }
    XSetWindowBackgroundPixmap(W_Display, win, icon->pixmap);
    XClearWindow(W_Display, win);

/*
    if (icon->pixmap==0) {
	icon->pixmap=XMakePixmap(icon->bitmap, colortable[W_White].pixelValue,
	    colortable[W_Black].pixelValue);
    }
    XChangeBackground(win, icon->pixmap);
    XClear(win);
 */
}

void W_UnTileWindow(window)
W_Window window;
{
    Window win;

#ifdef DEBUG
    printf("Untiling window %d\n", window);
#endif
    win=W_Void2Window(window)->window;

    XSetWindowBackground(W_Display, win, colortable[W_Black].pixelValue);
    XClearWindow(W_Display, win);
}

W_Window W_MakeTextWindow(name,x,y,width,height,parent,border)
char *name;
int x,y,width,height;
W_Window parent;
int border;
{
    struct window *newwin;
    Window wparent;
    XSetWindowAttributes attrs;
	
#ifdef DEBUG
    printf("New window...\n");
#endif
    checkGeometry(name, &x, &y, &width, &height);
    checkParent(name, &parent);
    attrs.override_redirect=False;
    attrs.border_pixel=colortable[W_White].pixelValue;
    attrs.event_mask=ExposureMask | ButtonPressMask;
    attrs.background_pixel=colortable[W_Black].pixelValue;
    attrs.do_not_propagate_mask=ExposureMask;
    wparent=W_Void2Window(parent)->window;
    newwin=newWindow(
	XCreateWindow(W_Display, wparent, x, y, 
	    width*W_Textwidth+WIN_EDGE*2, MENU_PAD*2+height*W_Textheight, 
	    border, CopyFromParent, InputOutput, CopyFromParent,
	    CWBackPixel|CWEventMask|
	    CWOverrideRedirect|CWBorderPixel, 
	    &attrs),
	WIN_TEXT);
    XSetClassHint(W_Display, newwin->window, &class_hint);
    XSetWMHints(W_Display, newwin->window, &wm_hint);
    newwin->name=strdup(name);
    newwin->width=width;
    newwin->height=height;
    if (checkMapped(name)) W_MapWindow(W_Window2Void(newwin));
#ifdef DEBUG
    printf("New text window %d, child of %d\n", newwin, parent);
#endif
    return(W_Window2Void(newwin));
}

struct window *newWindow(window, type)
Window window;
int type;
{
    struct window *newwin;

    newwin=(struct window *) malloc(sizeof(struct window));
    newwin->window=window;
    newwin->type=type;
    newwin->mapped=0;
    addToHash(newwin);
    return(newwin);
}

struct window *findWindow(window)
Window window;
{
    struct windowlist *entry;

    entry=hashtable[hash(window)];
    while (entry!=NULL) {
	if (entry->window->window == window) return(entry->window);
	entry=entry->next;
    }
    return(NULL);
}

addToHash(win)
struct window *win;
{
    struct windowlist **new;

#ifdef DEBUG
    printf("Adding to %d\n", hash(win->window));
#endif
    new= &hashtable[hash(win->window)];
    while (*new != NULL) {
	new= &((*new)->next);
    }
    *new=(struct windowlist *) malloc(sizeof(struct windowlist));
    (*new)->next=NULL;
    (*new)->window=win;
}

W_Window W_MakeScrollingWindow(name,x,y,width,height,parent,border)
char *name;
int x,y,width,height;
W_Window parent;
int border;
{
    struct window *newwin;
    Window wparent;
    XSetWindowAttributes attrs;

#ifdef DEBUG
    printf("New window...\n");
#endif
    checkGeometry(name, &x, &y, &width, &height);
    checkParent(name, &parent);
    wparent=W_Void2Window(parent)->window;
    attrs.override_redirect=False;
    attrs.border_pixel=colortable[W_White].pixelValue;
    attrs.event_mask=ResizeRedirectMask|ExposureMask;
    attrs.background_pixel=colortable[W_Black].pixelValue;
    attrs.do_not_propagate_mask=ResizeRedirectMask|ExposureMask;
    newwin=newWindow(
	XCreateWindow(W_Display, wparent, x, y,
            width*W_Textwidth+WIN_EDGE*2, MENU_PAD*2+height*W_Textheight, 
	    border, CopyFromParent, InputOutput, CopyFromParent,
	    CWBackPixel|CWEventMask|
	    CWOverrideRedirect|CWBorderPixel, 
	    &attrs),
        WIN_SCROLL);
    XSetClassHint(W_Display, newwin->window, &class_hint);
    XSetWMHints(W_Display, newwin->window, &wm_hint);
    newwin->name=strdup(name);
    newwin->data=NULL;
    newwin->width=width;
    newwin->height=height;
    if (checkMapped(name)) W_MapWindow(W_Window2Void(newwin));
#ifdef DEBUG
    printf("New scroll window %d, child of %d\n", newwin, parent);
#endif
    return(W_Window2Void(newwin));
}

/* Add a string to the string list of the scrolling window. 
 */
AddToScrolling(win, color, str, len)
struct window *win;
W_Color color;
char *str;
int len;
{
    struct stringList **strings;
    char *newstring;
    int count;

    strings= (struct stringList **) &(win->data);
    count=0;
    while ((*strings)!=NULL) {
	count++;
	strings= &((*strings)->next);
    }
    (*strings)=(struct stringList *) malloc(sizeof(struct stringList));
    (*strings)->next=NULL;
    (*strings)->color=color;
    newstring=(char *) malloc(len+1);
    strncpy(newstring, str, len);
    newstring[len]=0;
    (*strings)->string=newstring;
    if (count >= 100) {		/* Arbitrary large size. */
	struct stringList *temp;

	temp=(struct stringList *) win->data;
	free(temp->string);
	temp=temp->next;
	free((char *) win->data);
	win->data=(char *) temp;
    }
}

redrawScrolling(win)
struct window *win;
{
    int count;
    struct stringList *list;
    int y;

    XClearWindow(W_Display, win->window);
    count=0;
    list = (struct stringList *) win->data;
    while (list != NULL) {
	list=list->next;
	count++;
    }
    list = (struct stringList *) win->data;
    while (count > win->height) {
	list=list->next;
	count--;
    }
    y=(win->height-count)*W_Textheight+fonts[1].baseline;
    if (count==0) return;
    while (list != NULL) {
	XDrawImageString(W_Display, win->window, 
	    colortable[list->color].contexts[1],
	    WIN_EDGE, MENU_PAD+y, list->string, strlen(list->string));
	list=list->next;
	y=y+W_Textheight;
    }
}

resizeScrolling(win, width, height)
struct window *win;
int width, height;
{
    win->height=(height - MENU_PAD*2)/W_Textheight;
    win->width=(width - WIN_EDGE*2)/W_Textwidth;
    XResizeWindow(W_Display, win->window, win->width*W_Textwidth+WIN_EDGE*2,
	win->height*W_Textheight+MENU_PAD*2);
}

W_Window W_MakeMenu(name,x,y,width,height,parent,border)
char *name;
int x,y,width,height;
W_Window parent;
int border;
{
    struct window *newwin;
    struct menuItem *items;
    Window wparent;
    int i;
    XSetWindowAttributes attrs;

#ifdef DEBUG
    printf("New window...\n");
#endif
    checkGeometry(name, &x, &y, &width, &height);
    checkParent(name, &parent);
    wparent=W_Void2Window(parent)->window;
    attrs.override_redirect=False;
    attrs.border_pixel=colortable[W_White].pixelValue;
    attrs.event_mask=KeyPressMask|ButtonPressMask|ExposureMask;
    attrs.background_pixel=colortable[W_Black].pixelValue;
    attrs.do_not_propagate_mask=KeyPressMask|ButtonPressMask|ExposureMask;
    newwin=newWindow(
	XCreateWindow(W_Display, wparent, x, y,
	    width*W_Textwidth+WIN_EDGE*2,
	    height*(W_Textheight+MENU_PAD*2)+(height-1)*MENU_BAR, border,
	    CopyFromParent, InputOutput, CopyFromParent,
	    CWBackPixel|CWEventMask|
	    CWOverrideRedirect|CWBorderPixel, 
	    &attrs),
        WIN_MENU);
    XSetClassHint(W_Display, newwin->window, &class_hint);
    XSetWMHints(W_Display, newwin->window, &wm_hint);
    newwin->name=strdup(name);
    items=(struct menuItem *) malloc(height*sizeof(struct menuItem));
    for (i=0; i<height; i++) {
	items[i].string=NULL;
	items[i].color=W_White;
    }
    newwin->data=(char *) items;
    newwin->width=width;
    newwin->height=height;
    if (checkMapped(name)) W_MapWindow(W_Window2Void(newwin));
#ifdef DEBUG
    printf("New menu window %d, child of %d\n", newwin, parent);
#endif
    return(W_Window2Void(newwin));
}

redrawMenu(win)
struct window *win;
{
    int count;

    for (count=1; count<win->height; count++) {
	XFillRectangle(W_Display, win->window, 
	    colortable[W_White].contexts[0], 
	    0, count*(W_Textheight+MENU_PAD*2)+(count-1)*MENU_BAR,
	    win->width*W_Textwidth+WIN_EDGE*2, MENU_BAR);
    }
    for (count=0; count<win->height; count++) {
	redrawMenuItem(win,count);
    }
}

redrawMenuItem(win, n)
struct window *win;
int n;
{
    struct menuItem *items;

    items=(struct menuItem *) win->data;
    XFillRectangle(W_Display, win->window,
	colortable[W_Black].contexts[0],
	WIN_EDGE, n*(W_Textheight+MENU_PAD*2+MENU_BAR)+MENU_PAD,
	win->width*W_Textwidth,W_Textheight);
    if (items[n].string) {
	XDrawImageString(W_Display, win->window, 
	    colortable[items[n].color].contexts[1],
	    WIN_EDGE, 	
	    n*(W_Textheight+MENU_PAD*2+MENU_BAR)+MENU_PAD+fonts[1].baseline,
	    items[n].string, strlen(items[n].string));
    }
}

changeMenuItem(win, n, str, len, color)
struct window *win;
int n;
char *str;
int len;
W_Color color;
{
    struct menuItem *items;
    char *news;

    items=(struct menuItem *) win->data;
    if (items[n].string) {
	free(items[n].string);
    }
    news=malloc(len+1);
    strncpy(news,str,len);
    news[len]=0;
    items[n].string=news;
    items[n].color=color;
    redrawMenuItem(win, n);
}

void W_DefineCursor(window,width,height,bits,mask,xhot,yhot)
W_Window window;
int width, height, xhot, yhot;
char *bits, *mask;
{
    static char *oldbits=NULL;
    static Cursor curs;
    Pixmap cursbits;
    Pixmap cursmask;
    short *curdata;
    short *maskdata;
    struct window *win;
    XColor whiteCol, blackCol;

#ifdef DEBUG
    printf("Defining cursor for %d\n", window);
#endif
    win=W_Void2Window(window);
    whiteCol.pixel=colortable[W_White].pixelValue;
    XQueryColor(W_Display, W_Colormap, &whiteCol);
    blackCol.pixel=colortable[W_Black].pixelValue;
    XQueryColor(W_Display, W_Colormap, &blackCol);
    if (!oldbits || oldbits!=bits) {
	cursbits=XCreateBitmapFromData(W_Display, win->window,
	    bits, width, height);
	cursmask=XCreateBitmapFromData(W_Display, win->window,
	    mask, width, height);
	oldbits=bits;
	curs=XCreatePixmapCursor(W_Display, cursbits, cursmask, 
	    &whiteCol, &blackCol, xhot, yhot);
	XFreePixmap(W_Display, cursbits);
	XFreePixmap(W_Display, cursmask);
    }
    XDefineCursor(W_Display, win->window, curs);
}

void W_Beep()
{
    XBell(W_Display, 0);
}

int W_WindowWidth(window)
W_Window window;
{
    return(W_Void2Window(window)->width);
}

int W_WindowHeight(window)
W_Window window;
{
    return(W_Void2Window(window)->height);
}

int W_Socket()
{
    int	i = ConnectionNumber(W_Display);
    if(i< 0){ 
       fprintf(stderr, "X connection failed.\n");
       exit(1);
    }
    return i;
}

void W_DestroyWindow(window)
W_Window window;
{
    struct window *win;

#ifdef DEBUG
    printf("Destroying %d\n", window);
#endif
    win=W_Void2Window(window);
    deleteWindow(win);
    XDestroyWindow(W_Display, win->window);
    free((char *) win);
}

deleteWindow(window)
struct window *window;
{
    struct windowlist **rm;
    struct windowlist *temp;

    rm= &hashtable[hash(window->window)];
    while (*rm != NULL && (*rm)->window!=window) {
	rm= &((*rm)->next);
    }
    if (*rm==NULL) {
	printf("Attempt to delete non-existent window!\n");
	return;
    }
    temp= *rm;
    *rm=temp->next;
    free((char *) temp);
}

void W_SetIconWindow(main, icon)
W_Window main;
W_Window icon;
{
    static XWMHints hints;

    XSetIconName(W_Display,W_Void2Window(icon)->window,
		 W_Void2Window(main)->name);

    hints.flags=IconWindowHint|InputHint;
    hints.input = True; /* just shooting in the dark with twm here */
    hints.icon_window = W_Void2Window(icon)->window;
    XSetWMHints(W_Display, W_Void2Window(main)->window, &hints);
}

checkGeometry(name, x, y, width, height)
char *name;
int *x, *y, *width, *height;
{
    char *adefault;
    char buf[100];
    char *s;

    sprintf(buf, "%s.geometry", name);
    adefault=getdefault(buf);
    if (adefault==NULL) return;
    /* geometry should be of the form 502x885+1+1, 502x885, or +1+1 */
    s=adefault;
    if (*s!='+') {
	while (*s!='x' && *s!=0) s++;
	*width = atoi(adefault);
	if (*s==0) return;
	s++;
	adefault=s;
	while (*s!='+' && *s!=0) s++;
	*height = atoi(adefault);
	if (*s==0) return;
    }
    s++;
    adefault=s;
    while (*s!='+' && *s!=0) s++;
    *x = atoi(adefault);
    if (*s==0) return;
    s++;
    adefault=s;
    *y = atoi(adefault);
    return;
}

checkParent(name, parent)
char *name;
W_Window *parent;
{
    char *adefault;
    char buf[100];
    int i;
    struct windowlist *windows;

    sprintf(buf, "%s.parent", name);
    adefault=getdefault(buf);
    if (adefault==NULL) return;
    /* parent must be name of other window or "root" */
    if (strcmpi(adefault, "root")==0) {
	*parent=W_Window2Void(&myroot);
	return;
    }
    for (i=0; i<HASHSIZE; i++) {
	windows=hashtable[i];
	while (windows != NULL) {
	    if (strcmpi(adefault, windows->window->name)==0) {
		*parent=W_Window2Void(windows->window);
		return;
	    }
	    windows=windows->next;
	}
    }
}

checkMapped(name)
char *name;
{
    char *adefault;
    char buf[100];

    sprintf(buf, "%s.mapped", name);
    return(booleanDefault(buf, 0));
}

/* need to add W_WarpPointer(messagew) to new command in input.c */
/* need to add W_WarpPointer(NULL) to a couple places in smessage() (abort
   message, and when addr_str = 0) and to the end of pmessage() */

int W_in_message=0;

void W_WarpPointer(window, x, y)
W_Window window;
int x, y;
{
    static int warped_from_x = 0, warped_from_y = 0;
    
    if (window == NULL){
	if(W_in_message){
	    XWarpPointer(W_Display, None, W_Root, 0, 0, 0, 0, warped_from_x, warped_from_y);
	    W_in_message=0;
	}
    }
    else {
	findMouse(&warped_from_x, &warped_from_y);
	XWarpPointer(W_Display, None, W_Void2Window(window)->window, 0, 0, 0, 0, 0, 0);
	W_in_message=1;
    }
}

findMouse(x, y)
int *x, *y;
{
    Window		theRoot, theChild;
    int			wX, wY, rootX, rootY, status;
    unsigned int	wButtons;
    
    status = XQueryPointer(W_Display, W_Root, &theRoot, &theChild, &rootX, &rootY, &wX, &wY, &wButtons);
    if (status == True) {
	*x = wX;
	*y = wY;
    } else {
	*x = 0;
	*y = 0;
    }
}

W_Flush()
{
   XFlush(W_Display);
}


W_Sync()
{
   XSync(W_Display, False);
}


/* XSPEEDUP */
#define MAXCACHE        128

static XRectangle       _rcache[MAXCACHE];
static int              _rcache_index;

static void 
FlushClearAreaCache(win)

   Window       win;
{
   XFillRectangles(W_Display, win, colortable[backColor].contexts[0],
      _rcache, _rcache_index);
   _rcache_index = 0;
}

void 
W_CacheClearArea(window, x,y,width,height)
   
   W_Window     window;
   int          x,y,width,height;
{
   Window               win = W_Void2Window(window)->window;
   register XRectangle  *r;

   if(_rcache_index == MAXCACHE)
      FlushClearAreaCache(win);
   
   r = &_rcache[_rcache_index++];
   r->x = (short) x;
   r->y = (short) y;
   r->width = (unsigned short) width;
   r->height = (unsigned short) height;
}

void 
W_FlushClearAreaCache(window)

   W_Window     window;
{
   Window       win = W_Void2Window(window)->window;

   if(_rcache_index)
      FlushClearAreaCache(win);
}

static XSegment         _lcache[NCOLORS][MAXCACHE];
static int              _lcache_index[NCOLORS];

static void 
FlushLineCache(win, color)

   Window       win;
   int          color;
{
   XDrawSegments(W_Display, win, colortable[color].contexts[0],
      _lcache[color], _lcache_index[color]);
   _lcache_index[color] = 0;
}

void 
W_CacheLine(window, x0,y0,x1,y1,color)

   W_Window     window;
   int          x0,y0,x1,y1,color;
{
   Window               win = W_Void2Window(window)->window;
   register XSegment    *s;

   if(_lcache_index[color] == MAXCACHE)
      FlushLineCache(win, color);
   
   s = &_lcache[color][_lcache_index[color]++];
   s->x1 = (short)x0;
   s->y1 = (short)y0;
   s->x2 = (short)x1;
   s->y2 = (short)y1;

   /*
   if((!s->x1 && !s->y1) || (!s->x2 && !s->y2)){
      abort();
      printf("0 line seg\n");
   }
   */
}

void 
W_FlushLineCaches(window)

   W_Window     window;
{
   Window       win = W_Void2Window(window)->window;
   register     i;
   for(i=0; i< NCOLORS; i++){
      if(_lcache_index[i])
         FlushLineCache(win, i);
   }
}

static XPoint		_pcache[NCOLORS][MAXCACHE];
static int		_pcache_index[NCOLORS];

static void 
FlushPointCache(win, color)

   Window       win;
   int          color;
{
   XDrawPoints(W_Display, win, colortable[color].contexts[0],
      _pcache[color], _pcache_index[color], CoordModeOrigin);
   _pcache_index[color] = 0;
}

void 
W_CachePoint(window, x,y, color)

   W_Window     window;
   int          x,y,color;
{
   Window               win = W_Void2Window(window)->window;
   register XPoint      *p;

   if(_pcache_index[color] == MAXCACHE)
      FlushPointCache(win, color);
   
   p = &_pcache[color][_pcache_index[color]++];
   p->x = (short)x;
   p->y = (short)y;
}

void 
W_FlushPointCaches(window)

   W_Window     window;
{
   Window       win = W_Void2Window(window)->window;
   register     i;
   for(i=0; i< NCOLORS; i++){
      if(_pcache_index[i])
         FlushPointCache(win, i);
   }
}


void
W_ResizeWindow(window, neww, newh)		/* TSH 2/93 */

   W_Window	window;
   int		neww, newh;
{
   Window	win = W_Void2Window(window)->window;

   XResizeWindow(W_Display, win, (unsigned int) neww, (unsigned int) newh);
}

void
W_ResizeMenu(window, neww, newh)		/* TSH 2/93 */

   W_Window	window;
   int		neww, newh;
{
   W_ResizeWindow(window, neww*W_Textwidth+WIN_EDGE*2,
	    newh*(W_Textheight+MENU_PAD*2)+(newh-1)*MENU_BAR);
}
