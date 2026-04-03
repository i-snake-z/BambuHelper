/*
 * Pac-Man animated clock screensaver for BambuHelper
 *
 * Classic Pac-Man maze: 21 cols x 21 rows at 9px/tile = 189x189 px play area
 * centred in the 240x184 play area (ox=25, oy=0, small clip at bottom-right).
 * Actually: 21x20 at 9px = 189x180, ox=25 oy=2.
 * Sprites: Pac-Man r=6, ghost 11x13 - bigger than one tile, arcade style.
 * Time strip 56 px at bottom, Font 7, yellow.
 */

#include "clock_pacman.h"
#include "config.h"
#include "settings.h"
#include <TFT_eSPI.h>
#include <time.h>

extern TFT_eSPI tft;

// ─────────────────────────────────────────────────────────────────────────────
//  Layout  — 21 cols x 21 rows, tile = 9 px
//  Play area top = 0, bottom = 184 (screen 240 - 56 time)
//  21*9 = 189 px wide  → ox = (240-189)/2 = 25
//  21*9 = 189 px tall  → 189 > 184 so use 21 rows but clip; oy = 0 is fine
//  Use 21 cols x 20 rows: 189 wide x 180 tall, ox=25 oy=2
// ─────────────────────────────────────────────────────────────────────────────
#define T     9          // tile size px
#define COLS  21         // maze columns
#define ROWS  21         // maze rows — 21*9=189, fits in 184 if oy adjusted
#define OX    25         // (240 - 21*9) / 2
#define OY    0          // start at very top
#define TIME_H 56        // clock strip height
#define PLAY_H (SCREEN_H - TIME_H)   // 184 px
#define STEP_MS 180      // ms per game tick

// Sprite sizes
#define PMR   5          // Pac-Man radius px (11px diam, visible at 9px tile)
#define GHR   5          // ghost half-width

// ─────────────────────────────────────────────────────────────────────────────
//  Colours
// ─────────────────────────────────────────────────────────────────────────────
#define C_BG     0x0000
#define C_WALL   0x0011   // deep blue walls
#define C_DOT    0xF7BE   // cream dots
#define C_PP     0xFFFF   // power pellet white
#define C_DOOR   0xF81F   // magenta ghost-house door
#define C_PM     0xFFE0   // Pac-Man yellow
#define C_BLINK  0xF800   // Blinky red
#define C_PINKY  0xFC18   // Pinky pink
#define C_INKY   0x07FF   // Inky cyan
#define C_CLYDE  0xFD20   // Clyde orange
#define C_FRIGHT 0x001F   // frightened blue
#define C_FLASH  0xFFFF   // flash white

// ─────────────────────────────────────────────────────────────────────────────
//  Tile types
// ─────────────────────────────────────────────────────────────────────────────
#define TW  0   // wall
#define TD  1   // dot
#define TP  2   // power pellet
#define TE  3   // empty passage (no dot)
#define TH  4   // ghost house interior
#define TO  5   // ghost house door (open top)

// ─────────────────────────────────────────────────────────────────────────────
//  Maze  21x21 — based on classic Pac-Man layout, simplified to odd grid
//  Corridors are connected; every TE/TD/TP reachable from any other
// ─────────────────────────────────────────────────────────────────────────────
// Key: W=wall .=dot P=power H=house D=door E=empty
// Verified connected rectangular maze with ghost house in centre rows 8-10
static const uint8_t MTPL[ROWS][COLS] PROGMEM = {
//   0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20
/*0*/{TW,TW,TW,TW,TW,TW,TW,TW,TW,TW,TW,TW,TW,TW,TW,TW,TW,TW,TW,TW,TW},
/*1*/{TW,TD,TD,TD,TD,TD,TW,TD,TD,TD,TP,TD,TD,TD,TW,TD,TD,TD,TD,TD,TW},
/*2*/{TW,TD,TW,TW,TD,TW,TW,TW,TD,TW,TW,TW,TD,TW,TW,TW,TD,TW,TW,TD,TW},
/*3*/{TW,TD,TW,TW,TD,TW,TW,TW,TD,TW,TW,TW,TD,TW,TW,TW,TD,TW,TW,TD,TW},
/*4*/{TW,TD,TD,TD,TD,TD,TD,TD,TD,TD,TD,TD,TD,TD,TD,TD,TD,TD,TD,TD,TW},
/*5*/{TW,TD,TW,TW,TD,TW,TD,TW,TW,TW,TW,TW,TW,TW,TD,TW,TD,TW,TW,TD,TW},
/*6*/{TW,TD,TD,TD,TD,TW,TD,TD,TD,TD,TE,TD,TD,TD,TD,TW,TD,TD,TD,TD,TW},
/*7*/{TW,TW,TW,TW,TD,TW,TW,TW,TE, TW,TW,TW,TE,TW,TW,TW,TD,TW,TW,TW,TW},
/*8*/{TE,TE,TE,TW,TD,TW,TH,TH, TO,TH,TH,TH,TO,TH,TH,TW,TD,TW,TE,TE,TE},
/*9*/{TW,TW,TW,TW,TD,TW,TH,TH,TH, TH,TH,TH,TH,TH,TH,TW,TD,TW,TW,TW,TW},
/*A*/{TE,TE,TE,TE,TD,TE,TH,TH,TH, TH,TH,TH,TH,TH,TH,TE,TD,TE,TE,TE,TE},
/*B*/{TW,TW,TW,TW,TD,TW,TW,TW,TW, TW,TE,TW,TW,TW,TW,TW,TD,TW,TW,TW,TW},
/*C*/{TW,TD,TD,TD,TD,TD,TD,TD,TE, TD,TD,TD,TD,TD,TD,TD,TD,TD,TD,TD,TW},
/*D*/{TW,TD,TW,TW,TD,TW,TW,TW,TD,TW,TW,TW,TD,TW,TW,TW,TD,TW,TW,TD,TW},
/*E*/{TW,TP,TD,TW,TD,TD,TD,TD,TE,TD,TD,TD,TE,TD,TD,TD,TD,TW,TD,TP,TW},
/*F*/{TW,TW,TD,TW,TD,TW,TD,TW,TW,TW,TW,TW,TW,TW,TD,TW,TD,TW,TD,TW,TW},
/*G*/{TW,TD,TD,TD,TD,TW,TD,TD,TD,TD,TE,TD,TD,TD,TD,TW,TD,TD,TD,TD,TW},
/*H*/{TW,TD,TW,TW,TW,TW,TW,TW,TD,TW,TW,TW,TD,TW,TW,TW,TW,TW,TW,TD,TW},
/*I*/{TW,TD,TD,TD,TD,TD,TD,TD,TD,TD,TE,TD,TD,TD,TD,TD,TD,TD,TD,TD,TW},
/*J*/{TW,TW,TD,TW,TD,TW,TW,TW,TD,TW,TW,TW,TD,TW,TW,TW,TD,TW,TD,TW,TW},
/*K*/{TW,TW,TW,TW,TW,TW,TW,TW,TW,TW,TW,TW,TW,TW,TW,TW,TW,TW,TW,TW,TW},
};

// Tunnel row: row 10 (index A=10), open at left and right edge (cols 0,20 are TE)
#define TUNNEL_ROW 10

static uint8_t maze[ROWS][COLS];
static int     dotCount;

// Direction: 0=R 1=L 2=U 3=D
static const int8_t  DX[4]={1,-1,0,0};
static const int8_t  DY[4]={0,0,-1,1};
static const uint8_t RV[4]={1,0,3,2};

// Pixel centre of tile (c,r)
static inline int cx(int c){ return OX + c*T + T/2; }
static inline int cy(int r){ return OY + r*T + T/2; }

static inline bool passablePlayer(int c,int r){
    if(r<0||r>=ROWS) return false;
    // tunnel wrap
    if(c<0) c=COLS-1; else if(c>=COLS) c=0;
    uint8_t t=maze[r][c];
    return t==TD||t==TP||t==TE;
}
static inline bool passableGhost(int c,int r){
    if(r<0||r>=ROWS) return false;
    if(c<0) c=COLS-1; else if(c>=COLS) c=0;
    uint8_t t=maze[r][c];
    return t!=TW;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Actor state
// ─────────────────────────────────────────────────────────────────────────────
static int8_t  pmX,pmY,pmPX=-1,pmPY=-1;
static uint8_t pmDir=0;
static bool    pmMouth=true;

#define NG 4
struct Ghost{ int8_t x,y,px,py; uint8_t dir; uint16_t col; bool inHouse; int relTick; };
static Ghost G[NG];
static const uint16_t GCOL[NG]={C_BLINK,C_PINKY,C_INKY,C_CLYDE};
static const int GREL[NG]={0,8,16,26};

static bool frightened=false;
static int  frightLeft=0,frightFlash=0;
static int  gameTick=0;
static int  deathFrames=0;
static bool pmInited=false;
static unsigned long pmLastStep=0;
static int  prevMin=-1,prevHour=-1;

// ─────────────────────────────────────────────────────────────────────────────
//  Draw primitives
// ─────────────────────────────────────────────────────────────────────────────
static void drawWallTile(int c,int r){
    int x=OX+c*T, y=OY+r*T;
    // Solid wall block with 1px black gap = gives grid separation
    tft.fillRect(x, y, T, T, C_BG);
    tft.fillRect(x+1, y+1, T-2, T-2, C_WALL);
}
static void drawDoorTile(int c,int r){
    int x=OX+c*T, y=OY+r*T;
    tft.fillRect(x, y, T, T, C_BG);
    tft.fillRect(x, y+3, T, 3, C_DOOR);
}
static void drawEmptyTile(int c,int r){
    int x=OX+c*T, y=OY+r*T;
    tft.fillRect(x, y, T, T, C_BG);
}
static void drawDotTile(int c,int r){
    drawEmptyTile(c,r);
    tft.fillRect(cx(c)-1,cy(r)-1,3,3,C_DOT);
}
static void drawPowerTile(int c,int r){
    drawEmptyTile(c,r);
    tft.fillCircle(cx(c),cy(r),3,C_PP);
}
static void drawHouseTile(int c,int r){
    drawEmptyTile(c,r);  // house interior: just black
}

static void redrawTile(int c,int r){
    if(c<0||r<0||c>=COLS||r>=ROWS) return;
    // Clip bottom overflow (21 rows * 9 = 189 > 184)
    if(OY+r*T+T > PLAY_H) return;
    uint8_t t=maze[r][c];
    switch(t){
        case TW: drawWallTile(c,r);  break;
        case TD: drawDotTile(c,r);   break;
        case TP: drawPowerTile(c,r); break;
        case TO: drawDoorTile(c,r);  break;
        case TH: drawHouseTile(c,r); break;
        default: drawEmptyTile(c,r); break;
    }
}

// Restore neighbourhood after sprite erasure
static void restore3(int c,int r){
    for(int dr=-1;dr<=1;dr++) for(int dc=-1;dc<=1;dc++) redrawTile(c+dc,r+dr);
}

static void drawFullMaze(){
    for(int r=0;r<ROWS;r++) for(int c=0;c<COLS;c++) redrawTile(c,r);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Sprite drawing
// ─────────────────────────────────────────────────────────────────────────────
static void drawPacMan(int c,int r){
    int px=cx(c), py=cy(r);
    tft.fillCircle(px,py,PMR,C_PM);
    if(pmMouth){
        switch(pmDir){
            case 0: tft.fillTriangle(px,py,px+PMR+2,py-4,px+PMR+2,py+4,C_BG); break;
            case 1: tft.fillTriangle(px,py,px-PMR-2,py-4,px-PMR-2,py+4,C_BG); break;
            case 2: tft.fillTriangle(px,py,px-4,py-PMR-2,px+4,py-PMR-2,C_BG); break;
            case 3: tft.fillTriangle(px,py,px-4,py+PMR+2,px+4,py+PMR+2,C_BG); break;
        }
    }
}

static void drawGhost(int c,int r,uint16_t col){
    int px=cx(c)-GHR, py=cy(r)-GHR;
    int w=GHR*2, h=GHR*2+3;
    bool fr=(col==C_FRIGHT||col==C_FLASH);
    // Rounded top
    tft.fillCircle(cx(c), cy(r)-GHR+2, GHR, col);
    // Body
    tft.fillRect(px, cy(r)-GHR+2, w+1, h-2, col);
    // Wavy skirt (3 semicircle cuts)
    int sy=cy(r)-GHR+h;
    int sw=(w+1)/3;
    tft.fillCircle(px+sw/2,     sy, sw/2, C_BG);
    tft.fillCircle(px+sw+sw/2,  sy, sw/2, C_BG);
    tft.fillCircle(px+2*sw+sw/2,sy, sw/2, C_BG);
    // Eyes / face
    if(!fr){
        tft.fillCircle(cx(c)-2,cy(r)-GHR+2,2,0xFFFF);
        tft.fillCircle(cx(c)+2,cy(r)-GHR+2,2,0xFFFF);
        tft.fillCircle(cx(c)-2,cy(r)-GHR+2,1,0x001F);
        tft.fillCircle(cx(c)+2,cy(r)-GHR+2,1,0x001F);
    } else {
        // Frightened face
        tft.drawPixel(cx(c)-2,cy(r)-GHR+2,0xFFFF);
        tft.drawPixel(cx(c)+2,cy(r)-GHR+2,0xFFFF);
        tft.drawPixel(cx(c)-3,cy(r)-GHR+5,0xFFFF);
        tft.drawPixel(cx(c)-1,cy(r)-GHR+4,0xFFFF);
        tft.drawPixel(cx(c)+1,cy(r)-GHR+5,0xFFFF);
        tft.drawPixel(cx(c)+3,cy(r)-GHR+4,0xFFFF);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Time strip
// ─────────────────────────────────────────────────────────────────────────────
static void drawTime(bool force=false){
    struct tm now; if(!getLocalTime(&now,0)) return;
    if(!force&&now.tm_min==prevMin&&now.tm_hour==prevHour) return;
    int sy=PLAY_H;
    tft.fillRect(0,sy,SCREEN_W,TIME_H,C_BG);
    char buf[8];
    if(netSettings.use24h) snprintf(buf,sizeof(buf),"%02d:%02d",now.tm_hour,now.tm_min);
    else{ int h=now.tm_hour%12; if(!h)h=12; snprintf(buf,sizeof(buf),"%2d:%02d",h,now.tm_min); }
    tft.setTextFont(7); tft.setTextDatum(MC_DATUM);
    tft.setTextColor(C_PM,C_BG);
    tft.drawString(buf,SCREEN_W/2,sy+TIME_H/2);
    prevMin=now.tm_min; prevHour=now.tm_hour;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Init
// ─────────────────────────────────────────────────────────────────────────────
static void gameInit(){
    dotCount=0;
    for(int r=0;r<ROWS;r++) for(int c=0;c<COLS;c++){
        maze[r][c]=pgm_read_byte(&MTPL[r][c]);
        if(maze[r][c]==TD||maze[r][c]==TP) dotCount++;
    }
    // Pac-Man starts centre bottom corridor (row 18, col 10)
    pmX=10; pmY=18;
    if(maze[pmY][pmX]==TD){ maze[pmY][pmX]=TE; dotCount--; }
    pmDir=0; pmMouth=true; pmPX=-1; pmPY=-1;

    // Ghosts in/around house
    // Blinky starts just above house, already out
    // Pinky/Inky/Clyde start inside house
    static const int8_t GXS[NG]={10, 8,10,12};
    static const int8_t GYS[NG]={ 8, 9, 9, 9};
    for(int i=0;i<NG;i++){
        G[i].x=GXS[i]; G[i].y=GYS[i];
        G[i].px=-1;     G[i].py=-1;
        G[i].dir=(i==0)?3:0;
        G[i].col=GCOL[i];
        G[i].inHouse=(i>0);
        G[i].relTick=GREL[i];
    }
    frightened=false; frightLeft=0; frightFlash=0;
    deathFrames=0; gameTick=0;
}

// ─────────────────────────────────────────────────────────────────────────────
//  AI
// ─────────────────────────────────────────────────────────────────────────────
// Chase: move toward (tx,ty), no reversing
static uint8_t doChase(int8_t x,int8_t y,uint8_t d,int8_t tx,int8_t ty){
    uint8_t rev=RV[d], best=d; int bd=99999; bool any=false;
    for(uint8_t i=0;i<4;i++){
        if(i==rev) continue;
        int8_t nx=x+DX[i], ny=y+DY[i];
        if(!passableGhost(nx,ny)) continue;
        int dist=abs(nx-tx)+abs(ny-ty);
        if(!any||dist<bd){bd=dist;best=i;any=true;}
    }
    // If no valid dir found, allow reverse
    if(!any){
        int8_t nx=x+DX[rev], ny=y+DY[rev];
        if(passableGhost(nx,ny)) return rev;
    }
    return best;
}
// Flee: maximise distance from Pac-Man
static uint8_t doFlee(int8_t x,int8_t y,uint8_t d){
    uint8_t rev=RV[d], best=d; int bd=-1; bool any=false;
    for(uint8_t i=0;i<4;i++){
        if(i==rev) continue;
        int8_t nx=x+DX[i], ny=y+DY[i];
        if(!passableGhost(nx,ny)) continue;
        int dist=abs(nx-pmX)+abs(ny-pmY);
        if(!any||dist>bd){bd=dist;best=i;any=true;}
    }
    if(!any) return rev;
    return best;
}
// Pac-Man autopilot: continue 75%, random 25% at junctions
static uint8_t pmAutopilot(){
    uint8_t rev=RV[pmDir];
    uint8_t val[3]; int nv=0; bool canCont=false;
    for(uint8_t i=0;i<4;i++){
        if(i==rev) continue;
        int8_t nx=pmX+DX[i], ny=pmY+DY[i];
        // tunnel wrap on TUNNEL_ROW
        if(pmY==TUNNEL_ROW){ if(nx<0)nx=COLS-1; else if(nx>=COLS)nx=0; }
        if(!passablePlayer(nx,ny)) continue;
        val[nv++]=i;
        if(i==pmDir) canCont=true;
    }
    if(nv==0) return rev;
    if(canCont&&(nv==1||random(100)<75)) return pmDir;
    return val[random(nv)];
}

// ─────────────────────────────────────────────────────────────────────────────
//  Death
// ─────────────────────────────────────────────────────────────────────────────
static void tickDeath(){
    deathFrames--;
    uint16_t col=(deathFrames%2)?C_PM:C_BG;
    tft.fillCircle(cx(pmX),cy(pmY),PMR,col);
    if(deathFrames==0){
        gameInit();
        drawFullMaze();
        drawPacMan(pmX,pmY);
        for(int i=0;i<NG;i++) if(!G[i].inHouse) drawGhost(G[i].x,G[i].y,G[i].col);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Public API
// ─────────────────────────────────────────────────────────────────────────────
void resetPacManClock(){ pmInited=false; }

void tickPacManClock(){
    if(!dpSettings.showClockAfterFinish&&!dpSettings.keepDisplayOn) return;
    if(!dispSettings.pacmanClock) return;
    struct tm tmp; if(!getLocalTime(&tmp,0)) return;

    if(!pmInited){
        tft.fillScreen(C_BG);
        gameInit();
        drawFullMaze();
        drawPacMan(pmX,pmY);
        for(int i=0;i<NG;i++) if(!G[i].inHouse) drawGhost(G[i].x,G[i].y,G[i].col);
        prevMin=-1; prevHour=-1;
        pmInited=true; pmLastStep=millis();
        drawTime(true);
        return;
    }

    drawTime();

    unsigned long ms=millis();
    if(ms-pmLastStep<STEP_MS) return;
    pmLastStep=ms; gameTick++;

    if(deathFrames>0){ tickDeath(); return; }

    // ── Frighten countdown ────────────────────────────────────────────────
    if(frightened){ frightLeft--; frightFlash++; if(frightLeft<=0){frightened=false;frightFlash=0;} }

    // ── Ghost house release ───────────────────────────────────────────────
    for(int i=0;i<NG;i++){
        if(!G[i].inHouse||gameTick<G[i].relTick) continue;
        // Navigate to door (col 10, row 8 door tile)
        if(G[i].x!=10){ G[i].px=G[i].x; G[i].x+=(G[i].x<10)?1:-1; }
        else if(G[i].y>7){ G[i].py=G[i].y; G[i].y--; }
        else { G[i].inHouse=false; G[i].dir=0; }
    }

    // ── Erase previous sprite positions ──────────────────────────────────
    if(pmPX>=0) restore3(pmPX,pmPY);
    for(int i=0;i<NG;i++) if(G[i].px>=0) restore3(G[i].px,G[i].py);

    // ── Move Pac-Man ─────────────────────────────────────────────────────
    pmPX=pmX; pmPY=pmY;
    pmDir=pmAutopilot();
    int8_t nx=pmX+DX[pmDir], ny=pmY+DY[pmDir];
    if(pmY==TUNNEL_ROW){ if(nx<0)nx=COLS-1; else if(nx>=COLS)nx=0; }
    if(passablePlayer(nx,ny)){ pmX=nx; pmY=ny; }
    pmMouth=!pmMouth;

    // Eat
    uint8_t t=maze[pmY][pmX];
    if(t==TD){ maze[pmY][pmX]=TE; dotCount--; }
    else if(t==TP){ maze[pmY][pmX]=TE; dotCount--; frightened=true; frightLeft=30; frightFlash=0; }
    if(dotCount<=0){
        gameInit(); drawFullMaze();
        drawPacMan(pmX,pmY);
        for(int i=0;i<NG;i++) if(!G[i].inHouse) drawGhost(G[i].x,G[i].y,G[i].col);
        return;
    }

    // ── Move Ghosts ──────────────────────────────────────────────────────
    for(int i=0;i<NG;i++){
        if(G[i].inHouse) continue;
        G[i].px=G[i].x; G[i].py=G[i].y;
        uint8_t gd=frightened?doFlee(G[i].x,G[i].y,G[i].dir):doChase(G[i].x,G[i].y,G[i].dir,pmX,pmY);
        G[i].dir=gd;
        int8_t gx=G[i].x+DX[gd], gy=G[i].y+DY[gd];
        if(G[i].y==TUNNEL_ROW){ if(gx<0)gx=COLS-1; else if(gx>=COLS)gx=0; }
        if(passableGhost(gx,gy)){ G[i].x=gx; G[i].y=gy; }
    }

    // ── Collision ─────────────────────────────────────────────────────────
    bool died=false;
    for(int i=0;i<NG;i++){
        if(G[i].inHouse) continue;
        if(G[i].x==pmX&&G[i].y==pmY){
            if(frightened){
                G[i].x=10; G[i].y=9; G[i].px=-1; G[i].py=-1;
                G[i].inHouse=true; G[i].relTick=gameTick+20;
            } else if(!died){ died=true; deathFrames=10; }
        }
    }

    // ── Draw ──────────────────────────────────────────────────────────────
    if(deathFrames==0){
        drawPacMan(pmX,pmY);
        for(int i=0;i<NG;i++){
            if(G[i].inHouse) continue;
            uint16_t gc=G[i].col;
            if(frightened) gc=(frightLeft<8&&(frightFlash%4<2))?C_FLASH:C_FRIGHT;
            drawGhost(G[i].x,G[i].y,gc);
        }
    }
}
