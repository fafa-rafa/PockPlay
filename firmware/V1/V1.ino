// ============================================================
//  PockPlay V1
//  ESP32-C3 SuperMini + SSD1306 0.96" OLED (128x64, I2C) + 2 buttons (UP / DOWN)
//
//  Games: Dodger, Dino Run, Pong, Frogger, Flappy, Snake, Breakout
//
//  Libraries (Library Manager): Adafruit SSD1306, Adafruit GFX
//  Board: ESP32 Arduino core 3.x (needed for tone()), "ESP32C3 Dev Module"
//
//  Wiring:
//    OLED SDA -> GPIO5, SCL -> GPIO6, VCC -> 3V3, GND -> GND
//    UP button   -> GPIO3 to GND
//    DOWN button -> GPIO4 to GND
//    Passive buzzer (optional) -> GPIO10 to GND
//
//  Controls:
//    Menu:    UP / DOWN = scroll, press BOTH together = start game
//    In game: hold BOTH for ~0.7 s = back to menu
// ============================================================

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>

// ---------- Pins ----------
#define PIN_SDA     5
#define PIN_SCL     6
#define BTN_UP      3
#define BTN_DOWN    4
#define PIN_BUZZER  10

#define OLED_ADDR   0x3C
#define SCREEN_W    128
#define SCREEN_H    64
#define FIELD_TOP   12          // play area starts under the HUD line
#define FRAME_MS    33          // ~30 fps
#define DEBOUNCE_MS 12

Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);
Preferences prefs;

// ---------- Game table ----------
const int NUM_GAMES = 7;

struct Game {
  const char* name;
  void (*init)();
  void (*loop)();
};

void cdInit();  void cdLoop();
void dnInit();  void dnLoop();
void pgInit();  void pgLoop();
void fgInit();  void fgLoop();
void fbInit();  void fbLoop();
void snInit();  void snLoop();
void boInit();  void boLoop();

Game games[NUM_GAMES] = {
  { "Dodger", cdInit, cdLoop },
  { "Dino Run",   dnInit, dnLoop },
  { "Pong",       pgInit, pgLoop },
  { "Frogger",    fgInit, fgLoop },
  { "Flappy",     fbInit, fbLoop },
  { "Snake",      snInit, snLoop },
  { "Breakout",   boInit, boLoop },
};

int hs[NUM_GAMES];
bool soundOn = true;

int current = -1;               // -1 = menu
int sel = 0;
bool menuDirty = true;
unsigned long lastFrame = 0;
unsigned long bothStart = 0;

// ---------- Buttons ----------
bool stUp = false, stDn = false;          // debounced state
bool latchUp = false, latchDn = false;    // press events since last frame
bool hUp = false, hDn = false;            // held (per frame)
bool eUp = false, eDn = false;            // new press (per frame)

void pollButtons() {
  static bool rU = false, rD = false;
  static unsigned long tU = 0, tD = 0;
  unsigned long now = millis();
  bool u = digitalRead(BTN_UP) == LOW;
  bool d = digitalRead(BTN_DOWN) == LOW;
  if (u != rU) { rU = u; tU = now; }
  if (d != rD) { rD = d; tD = now; }
  if (now - tU >= DEBOUNCE_MS && stUp != rU) { stUp = rU; if (stUp) latchUp = true; }
  if (now - tD >= DEBOUNCE_MS && stDn != rD) { stDn = rD; if (stDn) latchDn = true; }
}

void frameInput() {
  hUp = stUp; hDn = stDn;
  eUp = latchUp; eDn = latchDn;
  latchUp = latchDn = false;
}

void waitRelease() {
  do { pollButtons(); delay(5); } while (stUp || stDn);
  latchUp = latchDn = false;
}

void waitAnyPress() {
  latchUp = latchDn = false;
  while (!(latchUp || latchDn)) { pollButtons(); delay(5); }
  waitRelease();
}

// ---------- Helpers ----------
void beep(int freq, int ms) {
  if (soundOn) tone(PIN_BUZZER, freq, ms);
}

void drawHud(const char* name, int score) {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(name);
  char b[12];
  snprintf(b, sizeof(b), "%d", score);
  display.setCursor(SCREEN_W - 6 * strlen(b), 0);
  display.print(b);
  display.drawFastHLine(0, 11, SCREEN_W, SSD1306_WHITE);
}

void startGame(int i) {
  current = i;
  games[i].init();
  lastFrame = millis();
  bothStart = 0;
  latchUp = latchDn = false;
}

void goMenu() {
  current = -1;
  waitRelease();
  menuDirty = true;
}

void gameOver(int score) {
  bool newBest = score > hs[current];
  if (newBest) {
    hs[current] = score;
    char key[8];
    snprintf(key, sizeof(key), "hs%d", current);
    prefs.putInt(key, score);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(10, 2);
  display.print("GAME OVER");
  display.setTextSize(1);
  display.setCursor(0, 28);
  display.print("Score: ");
  display.print(score);
  display.setCursor(0, 40);
  display.print("Best:  ");
  display.print(hs[current]);
  if (newBest) {
    display.setCursor(0, 54);
    display.print("NEW BEST!");
  }
  display.setCursor(72, 54);
  display.print("press btn");
  display.display();

  if (soundOn) {
    tone(PIN_BUZZER, 196, 400);
    delay(450);
    tone(PIN_BUZZER, 164, 800);
    delay(850);
  } else {
    delay(1300);
  }

  waitRelease();
  waitAnyPress();
  games[current].init();
  lastFrame = millis();
  bothStart = 0;
  stUp = stDn = false;
  latchUp = latchDn = false;
}

// ============================================================
//  0) DODGER
// ============================================================
const int CD_LANE_H = 24;
const int CD_LANE_Y[2] = { 14, 40 };
int cdPlayer, cdObs, cdWall, cdSpeed, cdScore;
bool cdTop;
unsigned long cdLast;

void cdInit() {
  cdPlayer = 1; cdObs = 11; cdWall = 12;
  cdSpeed = 300; cdScore = 0;
  cdTop = random(2);
  cdLast = millis();
}

void cdDrawPlayer(int lane) {
  int x = 0, y = CD_LANE_Y[lane] + 4;
  display.drawCircle(x + 4, y + 3, 3, SSD1306_WHITE);
  display.drawLine(x + 4, y + 6, x + 4, y + 14, SSD1306_WHITE);
  display.drawLine(x + 4, y + 8, x + 0, y + 11, SSD1306_WHITE);
  display.drawLine(x + 4, y + 8, x + 7, y + 11, SSD1306_WHITE);
  display.drawLine(x + 4, y + 14, x + 1, y + 19, SSD1306_WHITE);
  display.drawLine(x + 4, y + 14, x + 7, y + 19, SSD1306_WHITE);
}

void cdDrawObstacle(int col, int lane) {
  int x = col * 8;
  int y = CD_LANE_Y[lane] + CD_LANE_H / 2 - 3;
  display.fillRect(x + 3, y, 5, 6, SSD1306_WHITE);
  display.fillTriangle(x + 3, y, x + 3, y + 5, x, y + 3, SSD1306_WHITE);
}

void cdLoop() {
  if (hUp) cdPlayer = 0;
  if (hDn) cdPlayer = 1;

  if (millis() - cdLast >= (unsigned long)cdSpeed) {
    cdLast = millis();
    cdObs--;
    if (cdObs < 0) {
      beep(1200, 15);
      if (cdScore <= 30) {
        cdObs = 11;
        cdSpeed -= 10;
        if (cdSpeed < 100) cdSpeed = 100;
      } else {
        cdWall = 14 - cdScore / 10;
        if (cdWall < 3) cdWall = 3;
        cdObs = cdWall - 1;
      }
      cdTop = random(2);
      cdScore++;
    }
    if (cdObs == 0 && cdPlayer == (cdTop ? 0 : 1)) {
      gameOver(cdScore);
      return;
    }
  }

  display.clearDisplay();
  drawHud("Dodger", cdScore);
  for (int x = 0; x < SCREEN_W; x += 4) display.drawPixel(x, 39, SSD1306_WHITE);
  int wx = cdWall * 8 + 4;
  for (int y = 14; y < SCREEN_H; y += 4) display.drawFastVLine(wx, y, 2, SSD1306_WHITE);
  cdDrawPlayer(cdPlayer);
  if (cdObs >= 0) cdDrawObstacle(cdObs, cdTop ? 0 : 1);
  display.display();
}

// ============================================================
//  1) DINO RUN   (UP = jump, DOWN = duck / fast fall)
// ============================================================
const int DN_GROUND = 58;
const int DN_X = 12;
float dnY, dnV, dnDist, dnObsX;
int dnScore, dnObsType, dnObsW, dnObsH;
bool dnDuck;

void dnSpawn() {
  dnObsX = 128 + random(0, 50);
  if (dnScore >= 30 && random(4) == 0) {
    dnObsType = 1; dnObsW = 10; dnObsH = 6;      // bird
  } else {
    dnObsType = 0; dnObsW = random(5, 9); dnObsH = random(9, 17);  // cactus
  }
}

void dnInit() {
  dnY = 0; dnV = 0; dnDist = 0; dnScore = 0; dnDuck = false;
  dnSpawn();
  dnObsX = 150;
}

void dnLoop() {
  float spd = 2.5f + dnScore * 0.02f;
  if (spd > 6.0f) spd = 6.0f;

  bool onGround = (dnY <= 0);
  dnDuck = hDn && onGround;
  if (hUp && onGround && !dnDuck) { dnV = 6.8f; beep(700, 25); }
  dnV -= (hDn && !onGround) ? 2.2f : 0.8f;
  dnY += dnV;
  if (dnY <= 0) { dnY = 0; dnV = 0; }

  dnDist += spd;
  dnScore = (int)(dnDist / 8);
  dnObsX -= spd;
  if (dnObsX + dnObsW < 0) dnSpawn();

  // collision
  int dh = dnDuck ? 8 : 14;
  int dBot = DN_GROUND - (int)dnY;
  int dTop = dBot - dh;
  int oTop, oBot;
  if (dnObsType == 0) { oTop = DN_GROUND - dnObsH; oBot = DN_GROUND; }
  else { oTop = DN_GROUND - 18; oBot = DN_GROUND - 12; }
  int ox = (int)dnObsX;
  if (DN_X + 2 < ox + dnObsW && DN_X + 8 > ox && dBot - 1 > oTop && dTop + 1 < oBot) {
    gameOver(dnScore);
    return;
  }

  // draw
  display.clearDisplay();
  drawHud("Dino Run", dnScore);
  display.drawFastHLine(0, DN_GROUND + 1, SCREEN_W, SSD1306_WHITE);
  for (int i = 0; i < 6; i++) {
    int gx = (i * 31 - (int)dnDist) % 128;
    if (gx < 0) gx += 128;
    display.drawPixel(gx, DN_GROUND + 4, SSD1306_WHITE);
  }

  int x = DN_X, top = dTop;
  bool step = (((int)dnDist / 12) & 1) && onGround;
  if (!dnDuck) {
    display.fillRect(x + 4, top, 7, 5, SSD1306_WHITE);        // head
    display.drawPixel(x + 8, top + 1, SSD1306_BLACK);         // eye
    display.fillRect(x + 1, top + 5, 7, 5, SSD1306_WHITE);    // body
    display.fillRect(x - 1, top + 6, 2, 2, SSD1306_WHITE);    // tail
    display.fillRect(x + 2, top + 10, 2, step ? 4 : 2, SSD1306_WHITE);
    display.fillRect(x + 6, top + 10, 2, step ? 2 : 4, SSD1306_WHITE);
  } else {
    display.fillRect(x, top + 1, 10, 4, SSD1306_WHITE);       // body
    display.fillRect(x + 7, top, 6, 4, SSD1306_WHITE);        // head
    display.drawPixel(x + 10, top + 1, SSD1306_BLACK);
    display.fillRect(x + 2, top + 5, 2, 3, SSD1306_WHITE);
    display.fillRect(x + 6, top + 5, 2, 3, SSD1306_WHITE);
  }

  if (dnObsType == 0) {
    int stemX = ox + dnObsW / 2 - 1;
    display.fillRect(stemX, DN_GROUND - dnObsH, 3, dnObsH, SSD1306_WHITE);
    display.fillRect(ox, DN_GROUND - dnObsH + 4, 2, 4, SSD1306_WHITE);
    display.fillRect(ox + dnObsW - 2, DN_GROUND - dnObsH + 6, 2, 3, SSD1306_WHITE);
  } else {
    int by = DN_GROUND - 18;
    display.fillRect(ox + 2, by + 2, 8, 3, SSD1306_WHITE);
    bool wing = (((int)dnDist / 8) & 1);
    display.drawLine(ox + 4, by + 2, ox + 6, wing ? by - 2 : by + 6, SSD1306_WHITE);
    display.fillRect(ox, by + 3, 3, 1, SSD1306_WHITE);
  }
  display.display();
}

// ============================================================
//  2) PONG   (UP / DOWN move paddle, score = points won vs CPU)
// ============================================================
const int PG_H = 14, PG_W = 3, PG_PX = 3, PG_CX = 122;
float pgBx, pgBy, pgVx, pgVy, pgPy, pgCy, pgErr;
int pgScore;
unsigned long pgPause;

void pgServe() {
  pgBx = 64; pgBy = 37;
  float sp = 2.2f + pgScore * 0.08f;
  if (sp > 3.6f) sp = 3.6f;
  pgVx = sp;
  pgVy = (random(0, 2) ? 1 : -1) * (0.6f + random(0, 12) / 10.0f);
  pgPause = millis() + 700;
}

void pgInit() {
  pgScore = 0; pgPy = 30; pgCy = 30; pgErr = 0;
  pgServe();
}

void pgLoop() {
  if (hUp) pgPy -= 3;
  if (hDn) pgPy += 3;
  if (pgPy < FIELD_TOP + 1) pgPy = FIELD_TOP + 1;
  if (pgPy > 63 - PG_H) pgPy = 63 - PG_H;

  // CPU paddle
  if (pgVx > 0) {
    float cs = 1.3f + pgScore * 0.07f;
    if (cs > 2.9f) cs = 2.9f;
    float target = pgBy - PG_H / 2 + pgErr;
    if (pgCy < target - 1) pgCy += cs;
    else if (pgCy > target + 1) pgCy -= cs;
  } else {
    if (pgCy < 30) pgCy += 0.6f;
    else if (pgCy > 30) pgCy -= 0.6f;
  }
  if (pgCy < FIELD_TOP + 1) pgCy = FIELD_TOP + 1;
  if (pgCy > 63 - PG_H) pgCy = 63 - PG_H;

  if (millis() >= pgPause) {
    pgBx += pgVx; pgBy += pgVy;
    if (pgBy < FIELD_TOP + 2) { pgBy = FIELD_TOP + 2; pgVy = -pgVy; }
    if (pgBy > 62) { pgBy = 62; pgVy = -pgVy; }

    // player paddle
    if (pgVx < 0 && pgBx <= PG_PX + PG_W + 1 && pgBx > PG_PX - 2 &&
        pgBy >= pgPy - 1 && pgBy <= pgPy + PG_H + 1) {
      pgBx = PG_PX + PG_W + 2;
      pgVx = fabs(pgVx) * 1.05f;
      if (pgVx > 4.5f) pgVx = 4.5f;
      pgVy = (pgBy - (pgPy + PG_H / 2)) / (PG_H / 2) * 2.4f;
      pgErr = random(-6, 7);
      beep(600, 20);
    }
    // CPU paddle
    if (pgVx > 0 && pgBx >= PG_CX - 1 && pgBx <= PG_CX + PG_W &&
        pgBy >= pgCy - 1 && pgBy <= pgCy + PG_H + 1) {
      pgBx = PG_CX - 2;
      pgVx = -fabs(pgVx);
      pgVy = (pgBy - (pgCy + PG_H / 2)) / (PG_H / 2) * 2.4f;
      beep(450, 20);
    }
    if (pgVy > 3.2f) pgVy = 3.2f;
    if (pgVy < -3.2f) pgVy = -3.2f;

    if (pgBx > 127) { pgScore++; beep(1400, 60); pgServe(); }
    if (pgBx < 0) { gameOver(pgScore); return; }
  }

  display.clearDisplay();
  drawHud("Pong", pgScore);
  for (int y = FIELD_TOP + 2; y < SCREEN_H; y += 6) display.drawFastVLine(64, y, 3, SSD1306_WHITE);
  display.fillRect(PG_PX, (int)pgPy, PG_W, PG_H, SSD1306_WHITE);
  display.fillRect(PG_CX, (int)pgCy, PG_W, PG_H, SSD1306_WHITE);
  display.fillRect((int)pgBx - 1, (int)pgBy - 1, 3, 3, SSD1306_WHITE);
  display.display();
}

// ============================================================
//  3) FROGGER   (UP = hop forward, DOWN = hop back)
// ============================================================
// rows 0..5: 0 = goal, 1..4 = car lanes, 5 = start
int fgRow, fgScore, fgN;
float fgX[4][3], fgSpd[4];
int fgW[4][3], fgDir[4];

int fgRowY(int r) { return 14 + r * 8; }

void fgSetup() {
  fgN = 2 + (fgScore >= 6 ? 1 : 0);
  int sp = 128 / fgN;
  for (int l = 0; l < 4; l++) {
    fgDir[l] = (l & 1) ? 1 : -1;
    float s = 1.0f + random(0, 10) / 10.0f + fgScore * 0.05f;
    fgSpd[l] = s > 3.2f ? 3.2f : s;
    for (int c = 0; c < fgN; c++) {
      fgX[l][c] = c * sp + random(0, 12);
      fgW[l][c] = random(12, 19);
    }
  }
}

void fgInit() {
  fgScore = 0; fgRow = 5;
  fgSetup();
}

void fgLoop() {
  if (eUp && fgRow > 0) {
    fgRow--; beep(600, 20);
    if (fgRow == 0) {
      fgScore++; beep(1500, 80);
      fgRow = 5;
      fgSetup();
    }
  }
  if (eDn && fgRow < 5) { fgRow++; beep(400, 20); }

  // move cars
  for (int l = 0; l < 4; l++) {
    for (int c = 0; c < fgN; c++) {
      fgX[l][c] += fgDir[l] * fgSpd[l];
      if (fgDir[l] > 0 && fgX[l][c] > 128) fgX[l][c] = -fgW[l][c] - random(0, 20);
      if (fgDir[l] < 0 && fgX[l][c] < -fgW[l][c]) fgX[l][c] = 128 + random(0, 20);
    }
  }

  // collision
  if (fgRow >= 1 && fgRow <= 4) {
    int l = fgRow - 1;
    for (int c = 0; c < fgN; c++) {
      if (fgX[l][c] < 66 && fgX[l][c] + fgW[l][c] > 62) {
        gameOver(fgScore);
        return;
      }
    }
  }

  display.clearDisplay();
  drawHud("Frogger", fgScore);
  for (int x = 0; x < SCREEN_W; x += 8) display.fillRect(x + 2, 16, 4, 4, SSD1306_WHITE);
  display.drawFastHLine(0, 22, SCREEN_W, SSD1306_WHITE);
  display.drawFastHLine(0, 53, SCREEN_W, SSD1306_WHITE);
  for (int r = 2; r <= 4; r++)
    for (int x = 0; x < SCREEN_W; x += 4) display.drawPixel(x, fgRowY(r) + 0 - 0, SSD1306_WHITE);

  for (int l = 0; l < 4; l++) {
    int y = fgRowY(l + 1) + 1;
    for (int c = 0; c < fgN; c++) {
      int x = (int)fgX[l][c], w = fgW[l][c];
      display.fillRect(x, y, w, 6, SSD1306_WHITE);
      int fx = fgDir[l] > 0 ? x + w - 3 : x + 1;
      display.fillRect(fx, y + 1, 2, 2, SSD1306_BLACK);
    }
  }
  int fy = fgRowY(fgRow) + 1;
  display.fillRect(61, fy, 6, 6, SSD1306_WHITE);
  display.drawPixel(62, fy + 1, SSD1306_BLACK);
  display.drawPixel(65, fy + 1, SSD1306_BLACK);
  display.display();
}

// ============================================================
//  4) FLAPPY   (UP = flap, DOWN = dive)
// ============================================================
const int FB_GAP = 24, FB_PW = 10, FB_X = 28;
float fbY, fbV;
bool fbStarted;
float fbPx[3];
int fbPg[3];
bool fbPs[3];
int fbScore;

void fbInit() {
  fbY = 34; fbV = 0; fbScore = 0; fbStarted = false;
  for (int i = 0; i < 3; i++) {
    fbPx[i] = 140 + i * 54;
    fbPg[i] = random(16, 36);
    fbPs[i] = false;
  }
}

void fbDraw() {
  display.clearDisplay();
  drawHud("Flappy", fbScore);
  for (int i = 0; i < 3; i++) {
    int px = (int)fbPx[i], g = fbPg[i];
    display.drawRect(px, FIELD_TOP, FB_PW, g - FIELD_TOP, SSD1306_WHITE);
    display.drawRect(px, g + FB_GAP, FB_PW, SCREEN_H - (g + FB_GAP), SSD1306_WHITE);
    display.fillRect(px - 1, g - 3, FB_PW + 2, 3, SSD1306_WHITE);
    display.fillRect(px - 1, g + FB_GAP, FB_PW + 2, 3, SSD1306_WHITE);
  }
  int by = (int)fbY;
  display.fillCircle(FB_X, by, 3, SSD1306_WHITE);
  display.drawPixel(FB_X + 1, by - 1, SSD1306_BLACK);
  display.fillRect(FB_X + 3, by, 3, 1, SSD1306_WHITE);
  if (!fbStarted) {
    display.setCursor(40, 52);
    display.print("press UP");
  }
  display.display();
}

void fbLoop() {
  if (!fbStarted) {
    fbY = 34 + sin(millis() / 200.0f) * 3;
    if (eUp) { fbStarted = true; fbV = -3.1f; }
    fbDraw();
    return;
  }

  if (eUp) { fbV = -3.1f; beep(900, 20); }
  fbV += 0.35f;
  if (hDn) fbV += 0.25f;
  if (fbV > 4.5f) fbV = 4.5f;
  fbY += fbV;

  float spd = 1.5f + fbScore * 0.03f;
  if (spd > 2.5f) spd = 2.5f;

  for (int i = 0; i < 3; i++) {
    fbPx[i] -= spd;
    if (fbPx[i] < -FB_PW - 2) {
      fbPx[i] += 162;
      fbPg[i] = random(16, 36);
      fbPs[i] = false;
    }
    if (!fbPs[i] && fbPx[i] + FB_PW < FB_X - 3) {
      fbPs[i] = true; fbScore++; beep(1300, 30);
    }
    if (FB_X + 3 > fbPx[i] && FB_X - 3 < fbPx[i] + FB_PW) {
      if (fbY - 2 < fbPg[i] || fbY + 2 > fbPg[i] + FB_GAP) {
        gameOver(fbScore);
        return;
      }
    }
  }
  if (fbY + 3 > 63 || fbY - 3 < FIELD_TOP) {
    gameOver(fbScore);
    return;
  }
  fbDraw();
}

// ============================================================
//  5) SNAKE   (UP = turn left, DOWN = turn right; edges wrap)
// ============================================================
const int SN_W = 32, SN_H = 13, SN_C = 4, SN_Y = 12;
uint8_t snX[SN_W * SN_H], snY[SN_W * SN_H];
int snLen, snDir, snNext, snFx, snFy, snScore;
unsigned long snLast;
const int SN_DX[4] = { 1, 0, -1, 0 };   // right, down, left, up
const int SN_DY[4] = { 0, 1, 0, -1 };

void snFood() {
  bool ok;
  do {
    snFx = random(SN_W); snFy = random(SN_H);
    ok = true;
    for (int i = 0; i < snLen; i++)
      if (snX[i] == snFx && snY[i] == snFy) { ok = false; break; }
  } while (!ok);
}

void snInit() {
  snLen = 3; snDir = 0; snNext = 0; snScore = 0;
  for (int i = 0; i < snLen; i++) { snX[i] = 10 - i; snY[i] = 6; }
  snFood();
  snLast = millis();
}

void snLoop() {
  if (eUp && snNext == snDir) snNext = (snDir + 3) & 3;   // counter-clockwise
  if (eDn && snNext == snDir) snNext = (snDir + 1) & 3;   // clockwise

  int interval = 160 - snScore * 4;
  if (interval < 60) interval = 60;

  if (millis() - snLast >= (unsigned long)interval) {
    snLast = millis();
    snDir = snNext;
    int nx = (snX[0] + SN_DX[snDir] + SN_W) % SN_W;
    int ny = (snY[0] + SN_DY[snDir] + SN_H) % SN_H;
    bool grow = (nx == snFx && ny == snFy);
    int limit = grow ? snLen : snLen - 1;
    for (int i = 0; i < limit; i++) {
      if (snX[i] == nx && snY[i] == ny) { gameOver(snScore); return; }
    }
    if (grow) snLen++;
    for (int i = snLen - 1; i > 0; i--) { snX[i] = snX[i - 1]; snY[i] = snY[i - 1]; }
    snX[0] = nx; snY[0] = ny;
    if (grow) { snScore++; beep(1000, 30); snFood(); }
  }

  display.clearDisplay();
  drawHud("Snake", snScore);
  for (int i = 0; i < snLen; i++)
    display.fillRect(snX[i] * SN_C, SN_Y + snY[i] * SN_C, 3, 3, SSD1306_WHITE);
  display.drawRect(snFx * SN_C, SN_Y + snFy * SN_C, 3, 3, SSD1306_WHITE);
  display.display();
}

// ============================================================
//  6) BREAKOUT (sideways: paddle on left, bricks on right)
// ============================================================
const int BO_COLS = 4, BO_ROWS = 8, BO_BX = 100, BO_BY = 14, BO_PITCH = 6;
const int BO_PX = 2, BO_PW = 3, BO_PH = 14;
bool boBrick[BO_COLS][BO_ROWS];
float boX, boY, boVx, boVy, boPad;
int boLives, boScore, boLevel;
bool boStuck;

float boSpeed() {
  float s = 2.0f + boLevel * 0.35f;
  return s > 3.6f ? 3.6f : s;
}

void boFill() {
  for (int c = 0; c < BO_COLS; c++)
    for (int r = 0; r < BO_ROWS; r++) boBrick[c][r] = true;
}

void boInit() {
  boLives = 3; boScore = 0; boLevel = 0;
  boPad = 30; boStuck = true;
  boFill();
}

bool boHit(float x, float y) {
  if (x < BO_BX) return false;
  int c = ((int)x - BO_BX) / BO_PITCH;
  int r = ((int)y - BO_BY) / BO_PITCH;
  if (y < BO_BY || c < 0 || c >= BO_COLS || r < 0 || r >= BO_ROWS) return false;
  if (!boBrick[c][r]) return false;
  boBrick[c][r] = false;
  boScore++;
  beep(800 + r * 60, 20);
  return true;
}

void boLoop() {
  if (hUp) boPad -= 3;
  if (hDn) boPad += 3;
  if (boPad < FIELD_TOP + 1) boPad = FIELD_TOP + 1;
  if (boPad > 63 - BO_PH) boPad = 63 - BO_PH;

  if (boStuck) {
    boX = BO_PX + BO_PW + 2;
    boY = boPad + BO_PH / 2;
    if (eUp || eDn) {
      boStuck = false;
      boVx = boSpeed();
      boVy = (random(2) ? 1 : -1) * (0.8f + random(0, 10) / 10.0f);
    }
  } else {
    float nx = boX + boVx, ny = boY + boVy;
    if (ny < FIELD_TOP + 2) { ny = FIELD_TOP + 2; boVy = -boVy; }
    if (ny > 62) { ny = 62; boVy = -boVy; }
    if (nx > 126) { nx = 126; boVx = -boVx; }

    if (boHit(nx, boY)) { boVx = -boVx; nx = boX; }
    else if (boHit(boX, ny)) { boVy = -boVy; ny = boY; }
    else if (boHit(nx, ny)) { boVx = -boVx; boVy = -boVy; nx = boX; ny = boY; }

    if (boVx < 0 && nx <= BO_PX + BO_PW + 1 && nx >= BO_PX - 1 &&
        ny >= boPad - 1 && ny <= boPad + BO_PH + 1) {
      nx = BO_PX + BO_PW + 2;
      boVx = boSpeed();
      boVy = (ny - (boPad + BO_PH / 2)) / (BO_PH / 2) * 2.4f;
      beep(500, 20);
    }
    if (boVy > 3.0f) boVy = 3.0f;
    if (boVy < -3.0f) boVy = -3.0f;

    boX = nx; boY = ny;

    if (boX < 0) {
      boLives--;
      beep(200, 150);
      if (boLives <= 0) { gameOver(boScore); return; }
      boStuck = true;
    }

    bool any = false;
    for (int c = 0; c < BO_COLS && !any; c++)
      for (int r = 0; r < BO_ROWS; r++) if (boBrick[c][r]) { any = true; break; }
    if (!any) {
      boLevel++;
      boFill();
      boStuck = true;
      beep(1500, 120);
    }
  }

  display.clearDisplay();
  drawHud("Breakout", boScore);
  display.setCursor(66, 0);
  display.print("L");
  display.print(boLives);
  for (int c = 0; c < BO_COLS; c++)
    for (int r = 0; r < BO_ROWS; r++)
      if (boBrick[c][r])
        display.fillRect(BO_BX + c * BO_PITCH, BO_BY + r * BO_PITCH, 5, 5, SSD1306_WHITE);
  display.fillRect(BO_PX, (int)boPad, BO_PW, BO_PH, SSD1306_WHITE);
  display.fillRect((int)boX - 1, (int)boY - 1, 3, 3, SSD1306_WHITE);
  display.display();
}

// ============================================================
//  MENU
// ============================================================
void drawMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("PockPlay v1");
  display.setCursor(SCREEN_W - 7 * 6, 0);
  display.print("BOTH=OK");
  display.drawFastHLine(0, 10, SCREEN_W, SSD1306_WHITE);

  int first = sel - 1;
  if (first < 0) first = 0;
  if (first > NUM_GAMES - 4) first = NUM_GAMES - 4;
  for (int i = 0; i < 4; i++) {
    int g = first + i;
    int y = 13 + i * 12;
    if (g == sel) {
      display.fillRect(0, y - 1, SCREEN_W, 11, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(4, y + 1);
    display.print(games[g].name);
    char b[12];
    snprintf(b, sizeof(b), "%d", hs[g]);
    display.setCursor(SCREEN_W - 4 - 6 * strlen(b), y + 1);
    display.print(b);
  }
  display.setTextColor(SSD1306_WHITE);
  display.display();
}

void runMenu() {
  static bool pu = false, pd = false, both = false;
  static int last = 1;
  bool u = stUp, d = stDn;
  if (u && d) both = true;
  else if (u) last = -1;
  else if (d) last = 1;

  if (!u && !d && (pu || pd)) {
    if (both) {
      both = false;
      pu = pd = false;
      startGame(sel);
      return;
    }
    sel = (sel + last + NUM_GAMES) % NUM_GAMES;
    menuDirty = true;
  }
  pu = u; pd = d;

  if (menuDirty) { menuDirty = false; drawMenu(); }
}

// ============================================================
//  SETUP / LOOP
// ============================================================
void setup() {
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);

  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(400000);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    while (true) delay(1000);   // OLED not found
  }
  randomSeed(esp_random());

  prefs.begin("pockplay", false);
  for (int i = 0; i < NUM_GAMES; i++) {
    char key[8];
    snprintf(key, sizeof(key), "hs%d", i);
    hs[i] = prefs.getInt(key, 0);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(16, 16);
  display.print("PockPlay");
  display.setTextSize(1);
  display.setCursor(56, 40);
  display.print("V1");
  display.display();
  beep(880, 80);
  delay(1500);
}

void loop() {
  pollButtons();

  if (current < 0) {
    runMenu();
    delay(1);
    return;
  }

  unsigned long now = millis();
  if (now - lastFrame < FRAME_MS) return;
  lastFrame = now;
  frameInput();

  // hold both buttons -> back to menu
  if (hUp && hDn) {
    if (bothStart == 0) bothStart = now;
    else if (now - bothStart > 700) { bothStart = 0; goMenu(); return; }
  } else {
    bothStart = 0;
  }

  games[current].loop();
}
