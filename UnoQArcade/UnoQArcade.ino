/*
 * UNO Q Arcade Pack
 *
 * Games included:
 *   1) Snake
 *   2) Tetris (8x13 vertical playfield mapped to 13x8 display)
 *   3) Breakout
 *
 * Joystick wiring:
 *   VRx -> A0
 *   VRy -> A1
 *   SW  -> D7
 *
 * Startup flow:
 *   - Joystick auto-calibration
 *   - Difficulty selection from Serial (0..9)
 *   - LED matrix menu: choose game with joystick left/right, press SW to start
 *
 * In any game, hold SW for ~1.2s to return to menu.
 */

#include "ArduinoGraphics.h"
#include "Arduino_LED_Matrix.h"

// ----------------------------- Hardware setup --------------------------------
static constexpr uint8_t JOY_X = A0;
static constexpr uint8_t JOY_Y = A1;
static constexpr uint8_t JOY_SW = 7;

static constexpr int W = 13;
static constexpr int H = 8;

// ----------------------------- Shared tuning ---------------------------------
static constexpr int JOY_DEADZONE = 140;
static constexpr unsigned long JOY_CAL_MS = 1000;
static constexpr bool INVERT_Y = false;

static constexpr unsigned long MENU_INPUT_REPEAT_MS = 170;
static constexpr unsigned long SW_DEBOUNCE_MS = 45;
static constexpr unsigned long SW_HOLD_MENU_MS = 1200;

ArduinoLEDMatrix matrix;

int joyCenterX = 512;
int joyCenterY = 512;

// Forward declaration needed for Arduino .ino auto-generated prototypes.
enum class MenuGame : uint8_t;

// ----------------------------- Framebuffer -----------------------------------
static uint8_t fb[H][W] = {{0}};

static void clearFB() {
  memset(fb, 0, sizeof(fb));
}

static void setPX(int x, int y, bool on = true) {
  if (x < 0 || x >= W || y < 0 || y >= H) return;
  fb[y][x] = on ? 1 : 0;
}

static void pushFB() {
  matrix.loadPixels(&fb[0][0], W * H);
}

// ----------------------------- Input state -----------------------------------
struct InputState {
  int xDelta;
  int yDelta;
  int horizIntent;     // -1 / 0 / +1
  int vertIntent;      // -1 / 0 / +1
  bool swDown;
  bool swPressedEdge;
  bool swReleasedEdge;
};

static InputState input = {0, 0, 0, 0, false, false, false};

static bool swPrev = false;
static unsigned long swChangedMs = 0;
static unsigned long swDownStartMs = 0;
static bool swHoldTriggered = false;

static unsigned long lastHorizEdgeMs = 0;
static int prevHorizIntent = 0;

static void calibrateJoystick() {
  unsigned long t0 = millis();
  unsigned long sx = 0;
  unsigned long sy = 0;
  unsigned long n = 0;

  while (millis() - t0 < JOY_CAL_MS) {
    sx += analogRead(JOY_X);
    sy += analogRead(JOY_Y);
    n++;
    delay(2);
  }

  if (n > 0) {
    joyCenterX = (int)(sx / n);
    joyCenterY = (int)(sy / n);
  }
}

static void updateInput() {
  int rawX = analogRead(JOY_X) - joyCenterX;
  int rawY = analogRead(JOY_Y) - joyCenterY;

  if (INVERT_Y) rawY = -rawY;

  input.xDelta = rawX;
  input.yDelta = rawY;

  input.horizIntent = 0;
  if (rawX < -JOY_DEADZONE) input.horizIntent = -1;
  else if (rawX > JOY_DEADZONE) input.horizIntent = 1;

  input.vertIntent = 0;
  if (rawY < -JOY_DEADZONE) input.vertIntent = -1;
  else if (rawY > JOY_DEADZONE) input.vertIntent = 1;

  bool nowDown = (digitalRead(JOY_SW) == LOW);
  unsigned long now = millis();

  input.swPressedEdge = false;
  input.swReleasedEdge = false;

  if (nowDown != swPrev && (now - swChangedMs) >= SW_DEBOUNCE_MS) {
    swChangedMs = now;
    swPrev = nowDown;
    if (nowDown) {
      input.swPressedEdge = true;
      swDownStartMs = now;
      swHoldTriggered = false;
    } else {
      input.swReleasedEdge = true;
      swHoldTriggered = false;
    }
  }

  input.swDown = swPrev;
}

static bool swHoldToMenuRequested(bool inMenu) {
  if (inMenu) return false;
  if (!input.swDown || swHoldTriggered) return false;

  unsigned long heldMs = millis() - swDownStartMs;
  if (heldMs >= SW_HOLD_MENU_MS) {
    swHoldTriggered = true;
    return true;
  }
  return false;
}

static bool horizontalEdgeWithRepeat() {
  unsigned long now = millis();
  int h = input.horizIntent;

  if (h == 0) {
    prevHorizIntent = 0;
    return false;
  }

  if (h != prevHorizIntent) {
    prevHorizIntent = h;
    lastHorizEdgeMs = now;
    return true;
  }

  if (now - lastHorizEdgeMs >= MENU_INPUT_REPEAT_MS) {
    lastHorizEdgeMs = now;
    return true;
  }

  return false;
}

// ----------------------------- App state -------------------------------------
enum class AppMode {
  MENU,
  SNAKE,
  TETRIS,
  BREAKOUT
};

enum class MenuGame : uint8_t {
  Snake = 0,
  Tetris = 1,
  Breakout = 2
};

static AppMode mode = AppMode::MENU;
static MenuGame menuSelection = MenuGame::Snake;
static int difficulty = 4;
static unsigned long menuLastScrollMs = 0;
static int menuScrollX = 0;

static constexpr unsigned long MENU_SCROLL_MS = 120;

// ----------------------------- Shared helpers --------------------------------
static void showScrollText(const char* text, unsigned long speed = 85) {
  matrix.textScrollSpeed(speed);
  matrix.textFont(Font_5x7);
  matrix.beginText(0, 0, 0xFFFFFF);
  matrix.println(text);
  matrix.endText(SCROLL_LEFT);
}

static const char* menuGameName(MenuGame g) {
  if (g == MenuGame::Snake) return "SNAKE";
  if (g == MenuGame::Tetris) return "TETRIS";
  return "BREAKOUT";
}

// 3x5 uppercase font encoded as 5 rows of 3 bits.
static uint8_t tinyGlyphRow(char ch, int row) {
  switch (ch) {
    case 'A': { const uint8_t g[5] = {0b010, 0b101, 0b111, 0b101, 0b101}; return g[row]; }
    case 'B': { const uint8_t g[5] = {0b110, 0b101, 0b110, 0b101, 0b110}; return g[row]; }
    case 'E': { const uint8_t g[5] = {0b111, 0b100, 0b110, 0b100, 0b111}; return g[row]; }
    case 'I': { const uint8_t g[5] = {0b111, 0b010, 0b010, 0b010, 0b111}; return g[row]; }
    case 'K': { const uint8_t g[5] = {0b101, 0b101, 0b110, 0b101, 0b101}; return g[row]; }
    case 'N': { const uint8_t g[5] = {0b101, 0b111, 0b111, 0b111, 0b101}; return g[row]; }
    case 'O': { const uint8_t g[5] = {0b111, 0b101, 0b101, 0b101, 0b111}; return g[row]; }
    case 'R': { const uint8_t g[5] = {0b110, 0b101, 0b110, 0b101, 0b101}; return g[row]; }
    case 'S': { const uint8_t g[5] = {0b111, 0b100, 0b111, 0b001, 0b111}; return g[row]; }
    case 'T': { const uint8_t g[5] = {0b111, 0b010, 0b010, 0b010, 0b010}; return g[row]; }
    case 'U': { const uint8_t g[5] = {0b101, 0b101, 0b101, 0b101, 0b111}; return g[row]; }
    default: return 0;
  }
}

static int tinyTextWidth(const char* s) {
  int len = 0;
  while (s[len] != '\0') len++;
  // 3 pixels per char + 1 spacing.
  return len * 4;
}

static void drawTinyText(const char* text, int x0, int y0) {
  for (int i = 0; text[i] != '\0'; i++) {
    int cx = x0 + i * 4;
    for (int row = 0; row < 5; row++) {
      uint8_t bits = tinyGlyphRow(text[i], row);
      for (int col = 0; col < 3; col++) {
        if (bits & (1 << (2 - col))) {
          setPX(cx + col, y0 + row, true);
        }
      }
    }
  }
}

// =============================== SNAKE =======================================
static constexpr int S_MAX_LEN = W * H;
static constexpr unsigned long S_FOOD_BLINK_MS = 130;
static const unsigned long S_SPEED_MS[10] = {
  600, 520, 450, 390, 330, 280, 230, 180, 130, 80
};

struct SPoint { int8_t x, y; };

static SPoint sBody[S_MAX_LEN];
static int sHead = 0;
static int sLen = 0;
static int8_t sDx = 1;
static int8_t sDy = 0;
static int8_t sNextDx = 1;
static int8_t sNextDy = 0;
static SPoint sFood = {0, 0};
static int sScore = 0;
static unsigned long sStartMs = 0;
static unsigned long sLastStepMs = 0;

static inline int sSegIdx(int i) {
  return ((sHead - i) % S_MAX_LEN + S_MAX_LEN) % S_MAX_LEN;
}

static bool sOnSnake(int8_t x, int8_t y) {
  for (int i = 0; i < sLen; i++) {
    if (sBody[sSegIdx(i)].x == x && sBody[sSegIdx(i)].y == y) return true;
  }
  return false;
}

static void sPlaceFood() {
  do {
    sFood.x = (int8_t)random(W);
    sFood.y = (int8_t)random(H);
  } while (sOnSnake(sFood.x, sFood.y));
}

static void sInit() {
  sLen = 3;
  sHead = 2;
  sBody[0] = {(int8_t)(W / 2 - 1), (int8_t)(H / 2)};
  sBody[1] = {(int8_t)(W / 2), (int8_t)(H / 2)};
  sBody[2] = {(int8_t)(W / 2 + 1), (int8_t)(H / 2)};

  sDx = 1; sDy = 0;
  sNextDx = 1; sNextDy = 0;
  sScore = 0;
  sStartMs = millis();
  sLastStepMs = millis();

  sPlaceFood();

  Serial.println();
  Serial.println("--- SNAKE ---");
}

static void sRender() {
  clearFB();
  bool foodVisible = ((millis() / S_FOOD_BLINK_MS) % 2) == 0;

  for (int i = 0; i < sLen; i++) {
    setPX(sBody[sSegIdx(i)].x, sBody[sSegIdx(i)].y, true);
  }
  if (foodVisible) {
    setPX(sFood.x, sFood.y, true);
  }

  pushFB();
}

static bool sStep() {
  sDx = sNextDx;
  sDy = sNextDy;

  int8_t nx = (int8_t)((sBody[sHead].x + sDx + W) % W);
  int8_t ny = (int8_t)((sBody[sHead].y + sDy + H) % H);

  for (int i = 0; i < sLen - 1; i++) {
    if (sBody[sSegIdx(i)].x == nx && sBody[sSegIdx(i)].y == ny) return false;
  }

  sHead = (sHead + 1) % S_MAX_LEN;
  sBody[sHead] = {nx, ny};

  if (nx == sFood.x && ny == sFood.y) {
    sLen++;
    sScore++;
    Serial.print("Snake score: ");
    Serial.println(sScore);
    sPlaceFood();
  }

  return true;
}

static void sGameOverToMenu() {
  for (int i = 0; i < 3; i++) {
    matrix.clear();
    delay(140);
    sRender();
    delay(140);
  }

  unsigned long elapsedSec = (millis() - sStartMs) / 1000UL;
  Serial.println("Snake game over");
  Serial.print("Play time: ");
  Serial.print(elapsedSec);
  Serial.println(" s");
  Serial.print("Final score: ");
  Serial.println(sScore);

  showScrollText("  GAME OVER  ");
  mode = AppMode::MENU;
}

static void sUpdate() {
  if (input.xDelta < -JOY_DEADZONE && sDx != 1) { sNextDx = -1; sNextDy = 0; }
  else if (input.xDelta > JOY_DEADZONE && sDx != -1) { sNextDx = 1; sNextDy = 0; }
  else if (input.yDelta < -JOY_DEADZONE && sDy != 1) { sNextDx = 0; sNextDy = -1; }
  else if (input.yDelta > JOY_DEADZONE && sDy != -1) { sNextDx = 0; sNextDy = 1; }

  if (millis() - sLastStepMs >= S_SPEED_MS[difficulty]) {
    sLastStepMs = millis();
    if (!sStep()) {
      sGameOverToMenu();
      return;
    }
  }

  sRender();
}

// =============================== TETRIS ======================================
static constexpr int T_GW = 8;
static constexpr int T_GH = 13;
static constexpr unsigned long T_HORIZ_REPEAT_MS = 120;

static const unsigned long T_GRAVITY_MS[10] = {
  700, 620, 540, 470, 410, 350, 290, 230, 170, 120
};

static const uint16_t T_SHAPES[7][4] = {
  {0x0F00, 0x2222, 0x00F0, 0x4444},
  {0x0660, 0x0660, 0x0660, 0x0660},
  {0x0E40, 0x4C40, 0x4E00, 0x4640},
  {0x06C0, 0x8C40, 0x06C0, 0x8C40},
  {0x0C60, 0x4C80, 0x0C60, 0x4C80},
  {0x8E00, 0x6440, 0x0E20, 0x44C0},
  {0x2E00, 0x4460, 0x0E80, 0xC440}
};

struct TPiece {
  int8_t type;
  int8_t rot;
  int8_t x;
  int8_t y;
};

static bool tBoard[T_GH][T_GW] = {{false}};
static TPiece tCur = {0, 0, 0, 0};
static int tScore = 0;
static int tLines = 0;
static unsigned long tStartMs = 0;
static unsigned long tLastGravityMs = 0;
static unsigned long tLastHorizMs = 0;
static int tPrevHorizIntent = 0;

static bool tShapeCell(uint16_t mask, int r, int c) {
  int bit = 15 - (r * 4 + c);
  return ((mask >> bit) & 1U) != 0;
}

static bool tCanPlace(int type, int rot, int x, int y) {
  uint16_t m = T_SHAPES[type][rot & 3];
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      if (!tShapeCell(m, r, c)) continue;
      int gx = x + c;
      int gy = y + r;
      if (gx < 0 || gx >= T_GW || gy >= T_GH) return false;
      if (gy >= 0 && tBoard[gy][gx]) return false;
    }
  }
  return true;
}

static void tClearBoard() {
  memset(tBoard, 0, sizeof(tBoard));
}

static bool tSpawn() {
  tCur.type = (int8_t)random(7);
  tCur.rot = 0;
  tCur.x = 2;
  tCur.y = -2;
  return tCanPlace(tCur.type, tCur.rot, tCur.x, tCur.y);
}

static void tLock() {
  uint16_t m = T_SHAPES[tCur.type][tCur.rot & 3];
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      if (!tShapeCell(m, r, c)) continue;
      int gx = tCur.x + c;
      int gy = tCur.y + r;
      if (gx >= 0 && gx < T_GW && gy >= 0 && gy < T_GH) {
        tBoard[gy][gx] = true;
      }
    }
  }
}

static void tClearLines() {
  int cleared = 0;

  for (int y = T_GH - 1; y >= 0; y--) {
    bool full = true;
    for (int x = 0; x < T_GW; x++) {
      if (!tBoard[y][x]) { full = false; break; }
    }
    if (!full) continue;

    for (int yy = y; yy > 0; yy--) {
      for (int x = 0; x < T_GW; x++) {
        tBoard[yy][x] = tBoard[yy - 1][x];
      }
    }
    for (int x = 0; x < T_GW; x++) tBoard[0][x] = false;

    cleared++;
    y++;
  }

  if (cleared == 0) return;

  tLines += cleared;
  if (cleared == 1) tScore += 100;
  else if (cleared == 2) tScore += 300;
  else if (cleared == 3) tScore += 500;
  else tScore += 800;

  Serial.print("Tetris lines: ");
  Serial.print(tLines);
  Serial.print(" | score: ");
  Serial.println(tScore);
}

static void tPlotGameToDisplay(int gx, int gy, bool on) {
  int dx = (T_GH - 1) - gy;
  int dy = gx;
  setPX(dx, dy, on);
}

static void tRender() {
  clearFB();

  for (int y = 0; y < T_GH; y++) {
    for (int x = 0; x < T_GW; x++) {
      if (tBoard[y][x]) tPlotGameToDisplay(x, y, true);
    }
  }

  uint16_t m = T_SHAPES[tCur.type][tCur.rot & 3];
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      if (!tShapeCell(m, r, c)) continue;
      int gx = tCur.x + c;
      int gy = tCur.y + r;
      if (gy >= 0) tPlotGameToDisplay(gx, gy, true);
    }
  }

  pushFB();
}

static void tInit() {
  tClearBoard();
  tScore = 0;
  tLines = 0;
  tStartMs = millis();
  tLastGravityMs = millis();
  tLastHorizMs = millis();
  tPrevHorizIntent = 0;

  tSpawn();

  Serial.println();
  Serial.println("--- TETRIS ---");
}

static void tGameOverToMenu() {
  for (int i = 0; i < 3; i++) {
    matrix.clear();
    delay(130);
    tRender();
    delay(130);
  }

  unsigned long elapsedSec = (millis() - tStartMs) / 1000UL;
  Serial.println("Tetris game over");
  Serial.print("Play time: ");
  Serial.print(elapsedSec);
  Serial.println(" s");
  Serial.print("Lines: ");
  Serial.print(tLines);
  Serial.print(" | score: ");
  Serial.println(tScore);

  showScrollText("  GAME OVER  ");
  mode = AppMode::MENU;
}

static void tTryRotate() {
  int nextRot = (tCur.rot + 1) & 3;
  if (tCanPlace(tCur.type, nextRot, tCur.x, tCur.y)) {
    tCur.rot = nextRot;
  } else if (tCanPlace(tCur.type, nextRot, tCur.x - 1, tCur.y)) {
    tCur.x -= 1;
    tCur.rot = nextRot;
  } else if (tCanPlace(tCur.type, nextRot, tCur.x + 1, tCur.y)) {
    tCur.x += 1;
    tCur.rot = nextRot;
  }
}

static bool tStepGravity() {
  if (tCanPlace(tCur.type, tCur.rot, tCur.x, tCur.y + 1)) {
    tCur.y += 1;
    return true;
  }

  tLock();
  tClearLines();
  if (!tSpawn()) return false;
  return true;
}

static void tUpdate() {
  unsigned long now = millis();

  int h = input.horizIntent;
  if (h != 0) {
    if (h != tPrevHorizIntent || (now - tLastHorizMs) >= T_HORIZ_REPEAT_MS) {
      if (tCanPlace(tCur.type, tCur.rot, tCur.x + h, tCur.y)) {
        tCur.x += h;
      }
      tLastHorizMs = now;
    }
  }
  tPrevHorizIntent = h;
  if (h == 0) tPrevHorizIntent = 0;

  if (input.swPressedEdge) {
    tTryRotate();
  }

  if (input.vertIntent > 0) {
    if (tCanPlace(tCur.type, tCur.rot, tCur.x, tCur.y + 1)) {
      tCur.y += 1;
      tLastGravityMs = now;
    }
  }

  if (now - tLastGravityMs >= T_GRAVITY_MS[difficulty]) {
    tLastGravityMs = now;
    if (!tStepGravity()) {
      tGameOverToMenu();
      return;
    }
  }

  tRender();
}

// ============================== BREAKOUT =====================================
static const unsigned long B_BALL_MS[10] = {
  220, 205, 190, 175, 160, 145, 130, 115, 100, 85
};

static constexpr unsigned long B_PADDLE_CTRL_MS = 30;
// Speeds are in subpixels (1 pixel = 256 subpixels) per control tick.
static constexpr int B_PADDLE_MIN_SPEED_SUB = 24;
static constexpr int B_PADDLE_MAX_SPEED_SUB = 220;

static constexpr int B_ROWS = 3;
static bool bBricks[B_ROWS][W] = {{false}};
static int bBricksLeft = 0;

static int bPaddleX = 5;
static int bPaddleSubX = 5 * 256;
static constexpr int B_PADDLE_W = 3;
static constexpr int B_PADDLE_Y = H - 1;

static int bBallX = 6;
static int bBallY = H - 2;
static int8_t bBallDx = 1;
static int8_t bBallDy = -1;
static bool bBallStuck = true;

static int bScore = 0;
static int bLives = 3;
static unsigned long bStartMs = 0;
static unsigned long bLastBallMs = 0;
static unsigned long bLastPaddleMs = 0;

static void bInitBricks() {
  bBricksLeft = 0;
  for (int r = 0; r < B_ROWS; r++) {
    for (int x = 0; x < W; x++) {
      bool on = ((x + r) % 2 == 0) || (r == 0);
      bBricks[r][x] = on;
      if (on) bBricksLeft++;
    }
  }
}

static void bResetBallOnPaddle() {
  bBallX = bPaddleX + B_PADDLE_W / 2;
  bBallY = B_PADDLE_Y - 1;
  bBallDx = (random(2) == 0) ? -1 : 1;
  bBallDy = -1;
  bBallStuck = true;
}

static void bInit() {
  bPaddleX = 5;
  bPaddleSubX = bPaddleX * 256;
  bScore = 0;
  bLives = 3;
  bStartMs = millis();
  bLastBallMs = millis();
  bLastPaddleMs = millis();

  bInitBricks();
  bResetBallOnPaddle();

  Serial.println();
  Serial.println("--- BREAKOUT ---");
}

static void bRender() {
  clearFB();

  for (int r = 0; r < B_ROWS; r++) {
    for (int x = 0; x < W; x++) {
      if (bBricks[r][x]) setPX(x, r, true);
    }
  }

  for (int i = 0; i < B_PADDLE_W; i++) {
    setPX(bPaddleX + i, B_PADDLE_Y, true);
  }

  setPX(bBallX, bBallY, true);

  pushFB();
}

static void bBreakBrickAt(int x, int y) {
  if (y < 0 || y >= B_ROWS || x < 0 || x >= W) return;
  if (!bBricks[y][x]) return;

  bBricks[y][x] = false;
  bBricksLeft--;
  bScore += 10;

  Serial.print("Breakout score: ");
  Serial.println(bScore);
}

static bool bStepBall() {
  int nx = bBallX + bBallDx;
  int ny = bBallY + bBallDy;

  // Side walls.
  if (nx < 0 || nx >= W) {
    bBallDx = -bBallDx;
    nx = bBallX + bBallDx;
  }

  // Top wall.
  if (ny < 0) {
    bBallDy = -bBallDy;
    ny = bBallY + bBallDy;
  }

  // Brick collisions, axis-separated for more stable rebounds.
  bool hitX = false;
  bool hitY = false;
  bool hitDiag = false;

  if (ny >= 0 && ny < B_ROWS && bBricks[ny][bBallX]) {
    bBreakBrickAt(bBallX, ny);
    hitY = true;
  }
  if (nx >= 0 && nx < W && bBallY >= 0 && bBallY < B_ROWS && bBricks[bBallY][nx]) {
    bBreakBrickAt(nx, bBallY);
    hitX = true;
  }

  // Corner case: diagonal brick hit (target cell), where axis checks may miss.
  if (!hitX && !hitY && nx >= 0 && nx < W && ny >= 0 && ny < B_ROWS && bBricks[ny][nx]) {
    bBreakBrickAt(nx, ny);
    hitDiag = true;
  }

  if (hitDiag) {
    bBallDx = -bBallDx;
    bBallDy = -bBallDy;
  } else {
    if (hitX) bBallDx = -bBallDx;
    if (hitY) bBallDy = -bBallDy;
  }

  nx = bBallX + bBallDx;
  ny = bBallY + bBallDy;

  // Paddle collision (only when moving downward).
  if (bBallDy > 0 && ny == B_PADDLE_Y && nx >= bPaddleX && nx < (bPaddleX + B_PADDLE_W)) {
    bBallDy = -1;

    int hitPos = nx - bPaddleX; // 0,1,2
    if (hitPos == 0) bBallDx = -1;
    else if (hitPos == 2) bBallDx = 1;

    nx = bBallX + bBallDx;
    ny = bBallY + bBallDy;
  }

  bBallX = nx;
  bBallY = ny;

  // Ball fell below paddle.
  if (bBallY >= H) {
    bLives--;
    Serial.print("Breakout lives: ");
    Serial.println(bLives);

    if (bLives <= 0) return false;
    bResetBallOnPaddle();
  }

  return true;
}

static void bEndToMenu(bool win) {
  for (int i = 0; i < 3; i++) {
    matrix.clear();
    delay(120);
    bRender();
    delay(120);
  }

  unsigned long elapsedSec = (millis() - bStartMs) / 1000UL;
  Serial.println(win ? "Breakout win" : "Breakout game over");
  Serial.print("Play time: ");
  Serial.print(elapsedSec);
  Serial.println(" s");
  Serial.print("Final score: ");
  Serial.println(bScore);

  if (win) showScrollText("  BREAKOUT WIN  ");
  else showScrollText("  GAME OVER  ");

  mode = AppMode::MENU;
}

static void bUpdate() {
  // Smooth analog paddle movement with fine control near center.
  unsigned long now = millis();
  if (now - bLastPaddleMs >= B_PADDLE_CTRL_MS) {
    bLastPaddleMs = now;

    int dx = input.xDelta;
    int mag = abs(dx) - JOY_DEADZONE;
    if (mag < 0) mag = 0;

    if (mag > 0) {
      const int span = 512 - JOY_DEADZONE;
      int speedSub = B_PADDLE_MIN_SPEED_SUB +
                     (mag * (B_PADDLE_MAX_SPEED_SUB - B_PADDLE_MIN_SPEED_SUB)) / span;
      if (speedSub > B_PADDLE_MAX_SPEED_SUB) speedSub = B_PADDLE_MAX_SPEED_SUB;

      if (dx < 0) bPaddleSubX -= speedSub;
      else bPaddleSubX += speedSub;

      const int maxSub = (W - B_PADDLE_W) * 256;
      if (bPaddleSubX < 0) bPaddleSubX = 0;
      if (bPaddleSubX > maxSub) bPaddleSubX = maxSub;

      // Rounded conversion from subpixels to LED cell coordinate.
      bPaddleX = (bPaddleSubX + 128) / 256;
    }
  }

  // Launch ball on SW tap.
  if (bBallStuck) {
    bBallX = bPaddleX + B_PADDLE_W / 2;
    bBallY = B_PADDLE_Y - 1;
    if (input.swPressedEdge) {
      bBallStuck = false;
    }
  }

  if (!bBallStuck && now - bLastBallMs >= B_BALL_MS[difficulty]) {
    bLastBallMs = now;
    if (!bStepBall()) {
      bEndToMenu(false);
      return;
    }
  }

  if (bBricksLeft <= 0) {
    bEndToMenu(true);
    return;
  }

  bRender();
}

// =============================== MENU ========================================
static void menuRender() {
  clearFB();

  // Selector dots at top (rows 0..1)
  const int slotX[3] = {2, 6, 10};
  for (int i = 0; i < 3; i++) {
    bool on = (i == (int)menuSelection);
    if (on) {
      bool blink = ((millis() / 240) % 2) == 0;
      setPX(slotX[i], 0, blink);
      setPX(slotX[i], 1, blink);
    } else {
      setPX(slotX[i], 0, true);
    }
  }

  // Row 2 intentionally left empty as visual gap.

  const char* name = menuGameName(menuSelection);
  unsigned long now = millis();
  if (now - menuLastScrollMs >= MENU_SCROLL_MS) {
    menuLastScrollMs = now;
    menuScrollX--;
    int width = tinyTextWidth(name);
    if (menuScrollX < -width) {
      menuScrollX = W;
    }
  }

  // Draw scrolling game name on rows 3..7.
  drawTinyText(name, menuScrollX, 3);

  pushFB();
}

static void menuPrintSelection() {
  Serial.print("Menu -> ");
  if (menuSelection == MenuGame::Snake) Serial.println("Snake");
  else if (menuSelection == MenuGame::Tetris) Serial.println("Tetris");
  else Serial.println("Breakout");
}

static void enterMenu() {
  mode = AppMode::MENU;
  menuScrollX = W;
  menuLastScrollMs = millis();
  menuPrintSelection();
}

static void startSelectedGame() {
  if (menuSelection == MenuGame::Snake) {
    mode = AppMode::SNAKE;
    sInit();
  } else if (menuSelection == MenuGame::Tetris) {
    mode = AppMode::TETRIS;
    tInit();
  } else {
    mode = AppMode::BREAKOUT;
    bInit();
  }
}

static void menuUpdate() {
  if (horizontalEdgeWithRepeat()) {
    int sel = (int)menuSelection + input.horizIntent;
    if (sel < 0) sel = 2;
    if (sel > 2) sel = 0;
    menuSelection = (MenuGame)sel;
    menuScrollX = W;
    menuLastScrollMs = millis();
    menuPrintSelection();
  }

  if (input.swPressedEdge) {
    startSelectedGame();
    return;
  }

  menuRender();
}

// ============================== Setup / Loop =================================
void setup() {
  pinMode(JOY_SW, INPUT_PULLUP);

  Serial.begin(9600);
  delay(1000);

  if (!matrix.begin()) {
    Serial.println("Fatal error: LED matrix init failed.");
    while (true) {
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      delay(500);
    }
  }

  Serial.println("======= UNO Q ARCADE PACK =======");
  Serial.println("Keep joystick centered: calibrating...");
  calibrateJoystick();
  Serial.print("Center X=");
  Serial.print(joyCenterX);
  Serial.print(" Y=");
  Serial.println(joyCenterY);

  Serial.println("Choose difficulty (0=easy ... 9=hard):");
  while (Serial.available() == 0) {}
  difficulty = constrain(Serial.parseInt(), 0, 9);
  while (Serial.available()) Serial.read();

  randomSeed(millis() ^ analogRead(JOY_X) ^ analogRead(JOY_Y));

  Serial.print("Difficulty ");
  Serial.print(difficulty);
  Serial.println(" selected.");
  Serial.println("Menu: left/right to choose, SW to start.");
  Serial.println("In-game: hold SW for ~1.2s to return to menu.");

  enterMenu();
}

void loop() {
  updateInput();

  if (swHoldToMenuRequested(mode == AppMode::MENU)) {
    showScrollText("  MENU  ", 100);
    enterMenu();
    return;
  }

  if (mode == AppMode::MENU) {
    menuUpdate();
  } else if (mode == AppMode::SNAKE) {
    sUpdate();
  } else if (mode == AppMode::TETRIS) {
    tUpdate();
  } else {
    bUpdate();
  }
}
