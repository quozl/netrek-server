/* Wlib.h
 *
 * Include file for the Windowing interface.
 *
 * Kevin P. Smith  6/11/89
 * 
 * The deal is this:
 *   Call W_Initialize(), and then you may call any of the listed fuinctions.
 *   Also, externals you are allowed to pass or use include W_BigFont,
 *     W_RegularFont, W_UnderlineFont, W_HighlightFont, W_White, W_Black,
 *     W_Red, W_Green, W_Yellow, W_Cyan, W_Grey, W_Textwidth, and W_Textheight.
 */
#include "copyright2.h"

typedef char *W_Window;
typedef char *W_Icon;
typedef char *W_Font;
typedef int W_Color;

extern W_Font W_BigFont, W_RegularFont, W_UnderlineFont, W_HighlightFont;
extern W_Color W_White, W_Black, W_Red, W_Green, W_Yellow, W_Cyan, W_Grey;
extern int W_Textwidth, W_Textheight;
extern int W_FastClear;

/* x11window.c */
void W_Initialize(/*char *str*/);
int GetFonts(/*void*/);
int GetColors(/*void*/);
W_Window W_MakeWindow(/*char *name, int x, int y, int width, int height, W_Window parent, int border, W_Color color*/);
void W_ChangeBorder(/*W_Window window, int color*/);
void W_MapWindow(/*W_Window window*/);
void W_UnmapWindow(/*W_Window window*/);
int W_IsMapped(/*W_Window window*/);
void W_ClearArea(/*W_Window window, int x, int y, int width, int height, W_Color color*/);
void W_FillArea(/*W_Window window, int x, int y, int width, int height, W_Color color*/);
void W_ClearWindow(/*W_Window window*/);
int W_EventsPending(/*void*/);
void W_NextEvent(/*W_Event *wevent*/);
int W_SpNextEvent(/*W_Event *wevent*/);
void W_MakeLine(/*W_Window window, int x0, int y0, int x1, int y1, W_Color color*/);
void W_MakeTractLine(/*W_Window window, int x0, int y0, int x1, int y1, W_Color color*/);
void W_MakePoint(/*W_Window window, int x, int y, W_Color color*/);
void W_WriteText(/*W_Window window, int x, int y, W_Color color, char *str, int len, W_Font font*/);
void W_MaskText(/*W_Window window, int x, int y, W_Color color, char *str, int len, W_Font font*/);
W_Icon W_StoreBitmap(/*int width, int height, char *data, W_Window window*/);
void W_WriteBitmap(/*int x, int y, W_Icon bit, W_Color color*/);
void W_TileWindow(/*W_Window window, W_Icon bit*/);
void W_UnTileWindow(/*W_Window window*/);
W_Window W_MakeTextWindow(/*char *name, int x, int y, int width, int height, W_Window parent, int border*/);
struct window *newWindow(/*Window window, int type*/);
struct window *findWindow(/*Window window*/);
int addToHash(/*struct window *win*/);
W_Window W_MakeScrollingWindow(/*char *name, int x, int y, int width, int height, W_Window parent, int border*/);
int AddToScrolling(/*struct window *win, W_Color color, char *str, int len*/);
int redrawScrolling(/*struct window *win*/);
int resizeScrolling(/*struct window *win, int width, int height*/);
W_Window W_MakeMenu(/*char *name, int x, int y, int width, int height, W_Window parent, int border*/);
int redrawMenu(/*struct window *win*/);
int redrawMenuItem(/*struct window *win, int n*/);
int changeMenuItem(/*struct window *win, int n, char *str, int len, W_Color color*/);
void W_DefineCursor(/*W_Window window, int width, int height, char *bits, char *mask, int xhot, int yhot*/);
void W_Beep(/*void*/);
int W_WindowWidth(/*W_Window window*/);
int W_WindowHeight(/*W_Window window*/);
int W_Socket(/*void*/);
void W_DestroyWindow(/*W_Window window*/);
int deleteWindow(/*struct window *window*/);
void W_SetIconWindow(/*W_Window main, W_Window icon*/);
int checkGeometry(/*char *name, int *x, int *y, int *width, int *height*/);
int checkParent(/*char *name, W_Window *parent*/);
int checkMapped(/*char *name*/);
void W_WarpPointer(/*W_Window window, int x, int y*/);
int findMouse(/*int *x, int *y*/);
int W_Flush(/*void*/);
int W_Sync(/*void*/);
void W_CacheClearArea(/*W_Window window, int x, int y, int width, int height*/);
void W_FlushClearAreaCache(/*W_Window window*/);
void W_CacheLine(/*W_Window window, int x0, int y0, int x1, int y1, int color*/);
void W_FlushLineCaches(/*W_Window window*/);
void W_CachePoint(/*W_Window window, int x, int y, int color*/);
void W_FlushPointCaches(/*W_Window window*/);
void W_ResizeWindow(/*W_Window window, int neww, int newh*/);
void W_ResizeMenu(/*W_Window window, int neww, int newh*/);

#define W_EV_EXPOSE	1
#define W_EV_KEY	2
#define W_EV_BUTTON	3

#define W_LBUTTON	1
#define W_MBUTTON	2
#define W_RBUTTON	3

typedef struct event {
	int	type;
	W_Window Window;
	int	key;
	int	x,y;
} W_Event;

#define W_BoldFont W_HighlightFont
