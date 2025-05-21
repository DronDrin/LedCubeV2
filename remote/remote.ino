#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>
MCUFRIEND_kbv tft;
#include <TouchScreen.h>
#include <Fonts/FreeSerif12pt7b.h>
#define MINPRESSURE 10
#define MAXPRESSURE 1000

// radio
#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "DigitalIO.h"

RF24 radio(12, 11);
// --- end radio


const int modesCount = 9;
struct mode {
  char name[25];
  int variationsCount;
} modes[modesCount] = {
  { "mode set 1", 1 },
  { "rain", 1 },
  { "light", 1 },
  { "planes", 1 },
  { "spirals", 8 },
  { "breathing", 3 },
  { "fill", 3 },
  { "wave", 3 },
  { "labyrinth", 1 },
};

const int XP = 8, XM = A2, YP = A3, YM = 9;  //240x320 ID=0x9595
const int TS_LEFT = 869, TS_RT = 118, TS_TOP = 90, TS_BOT = 899;

TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

bool sentFlag = false;

bool initState = false;

struct cube_state {
  int mode, variant;
  inline bool operator==(const cube_state &a) {
    return a.mode == mode && a.variant == variant;
  }
  inline bool operator!=(const cube_state &a) {
    return a.mode != mode || a.variant != variant;
  }
  inline void operator=(const cube_state &a) {
    mode = a.mode;
    variant = a.variant;
  }
  int modeI() {
    return mode - 1;
  }
};

cube_state state = { 0, 0 };
cube_state pre_settings_state = { 0, 0 };
cube_state RM_state = { 0, 0 };

int press_x, press_y;
bool pressed = false;
bool connected = true;
bool clicked = false;

long TM_click = 0;
long clickThreshold = 300;

long TM_heartbeat = 0;
long heartbeatThreshold = 1000;

long TM_send;
long minSendDelay = 300;

unsigned long pressStart = -1;
unsigned long pressThreshold = 20;
void touchUpdate(void) {
  TSPoint p = ts.getPoint();
  pinMode(YP, OUTPUT);
  pinMode(XM, OUTPUT);
  digitalWrite(YP, HIGH);
  digitalWrite(XM, HIGH);
  bool _pressed = (p.z > MINPRESSURE && p.z < MAXPRESSURE);
  if (_pressed) {
    if (pressStart == -1) pressStart = millis();
    press_x = map(p.x, TS_LEFT, TS_RT, 0, tft.width());
    press_y = map(p.y, TS_TOP, TS_BOT, 0, tft.height());
  } else
    pressStart = -1;
  bool pp = pressed;
  pressed = _pressed && millis() - pressStart >= pressThreshold;
  if (pressed && !pp && (millis() - TM_click > clickThreshold)) {
    clicked = true;
    TM_click = millis();
  }
}
#define BLACK 0x0000
#define BLUE 0x001F
#define RED 0xF800
#define GREEN 0x07E0
#define CYAN 0x07FF
#define MAGENTA 0xF81F
#define YELLOW 0xFFE0
#define WHITE 0xFFFF


int screen = 0;
int prevScreen = 0;
int connectedScreen = 0;
int backScreen[] = { 0, 1, 0, 0 };
char screenNames[][16] = { "home", "disconnected", "modes", "settings" };
char brightnessLevelNames[][12] = { "very dim", "dim", "bright", "very bright" };

byte settings[2];
bool settingsFetched[2];
int settingsSize = 2;



int page = 0;
int pageSize = 7;

void setup(void) {
  for (int i = 0; i < settingsSize; ++i) {
    settingsFetched[i] = false;
  }

  Serial.begin(9600);

  uint16_t ID = tft.readID();
  tft.begin(ID);
  tft.setRotation(0);  //PORTRAIT
  tft.fillScreen(BLACK);


  // radio
  radio.begin();
  radio.setAutoAck(1);
  radio.setRetries(5, 15);
  radio.setPayloadSize(3);
  radio.enableAckPayload();
  radio.enableDynamicPayloads();
  radio.openWritingPipe(*"1Node");
  radio.openReadingPipe(1, *"2Node");
  radio.setChannel(0x60);
  radio.powerUp();
  radio.stopListening();

  renderScreen();
}

void renderScreen() {
  tft.fillScreen(BLACK);
  if (screen > 1) {
    tft.fillRect(0, 0, 240, 50, BLUE);
    tft.setCursor(10, 10);
    tft.setTextSize(4);
    tft.print("<");
    tft.setTextSize(3);
    tft.setCursor(40, 10);
    tft.print(screenNames[screen]);
  }
  switch (screen) {
    case 0:
      {

        tft.setCursor(10, 10);
        tft.setTextSize(3);
        tft.print("Modes");
        tft.setCursor(10, 40);
        tft.print("Settings");
        break;
      }
    case 1:
      {

        tft.setCursor(10, 10);
        tft.setTextSize(3);
        tft.print("disconnected");
        break;
      }
    case 2:
      {
        if (state.modeI() == 8) {
          tft.setCursor(210, 10);
          tft.setTextSize(4);
          tft.print("X");
          tft.setTextSize(3);

          tft.fillTriangle(120, 60, 90, 130, 150, 130, WHITE);
          tft.fillTriangle(90, 130, 90, 190, 20, 160, WHITE);
          tft.fillTriangle(150, 130, 150, 190, 220, 160, WHITE);
          tft.fillTriangle(90, 190, 150, 190, 120, 260, WHITE);

          tft.fillTriangle(20, 220, 80, 220, 50, 290, WHITE);
          tft.fillTriangle(160, 290, 220, 290, 190, 220, WHITE);
        } else {
          int inc = 30;
          tft.setTextSize(3);
          tft.fillTriangle(200 - 40, 40, 215 - 40, 10, 230 - 40, 40, WHITE);
          tft.fillTriangle(200, 10, 215, 40, 230, 10, WHITE);
          for (int i = page * pageSize; i < ((page + 1) * pageSize) && i < modesCount; i++) {
            int hi = i % pageSize;
            tft.setCursor(10, hi * 30 + 60);
            if (i == state.modeI())
              tft.setTextColor(GREEN);
            tft.print(modes[i].name);
            if (i == state.modeI())
              tft.setTextColor(WHITE);
          }
          if (state.modeI() != -1 && modes[state.modeI()].variationsCount > 1) {
            tft.fillRect(0, 285, 240, 35, BLUE);
            tft.setCursor(10, 290);
            tft.print("variant ");
            tft.print(state.variant + 1);
          }
        }
        break;
      }
    case 3:
      {
        fetchSetting(page);
        tft.setTextSize(3);

        tft.setCursor(10, 60);
        switch (page) {
          case 0:
            {
              tft.print(brightnessLevelNames[settings[0]]);
              break;
            }
          case 1:
            {
              byte cross_loc = settings[1] & 15;
              byte square_loc = (settings[1] & 48) >> 4;
              byte dot_loc = (settings[1] & 64) >> 6;
              tft.print("cross bot: ");
              tft.print(cross_loc + 1);
              tft.setCursor(10, 90);
              tft.print("square l: ");
              tft.print(square_loc + 1);
              tft.setCursor(10, 120);
              tft.print("dot back: ");
              tft.print(dot_loc + 1);
              break;
            }
        }

        // arrows
        tft.fillTriangle(40, 310, 10, 295, 40, 280, page > 0 ? WHITE : tft.color565(50, 50, 50));
        tft.fillTriangle(200, 310, 230, 295, 200, 280, page < settingsSize - 1 ? WHITE : tft.color565(50, 50, 50));
        break;
      }
  }
}

void screenSwitch(int i) {
  if (screen == i) return;
  if (screen != 1 && i != 1)
    prevScreen = screen;
  screen = i;
  page = 0;
  renderScreen();
}

bool send(byte command, byte info1, byte info2, byte *ack) {
  while (millis() - TM_send < minSendDelay) {  // do not send too often!
    delayMicroseconds(300);
  }
  TM_send = millis();
  sentFlag = true;
  byte data[] = { command, info1, info2 };
  if (radio.write(data, 3)) {
    connected = true;
    uint8_t pipe;
    if (radio.available(&pipe)) {
      radio.read(ack, 1);
    }
    return true;
  }
  if (connected) {
    connectedScreen = screen;
    screenSwitch(1);
  }
  connected = false;
  RM_state = { 0, 0 };
  return false;
}

bool send(int command, byte info1, byte info2 = 0) {
  byte *ack;
  return send(command, info1, info2, ack);
}

bool clickedArea(int xf, int yf, int xt, int yt) {
  return clicked && press_x >= xf - 10 && press_y >= yf - 10 && press_x <= xt + 10 && press_y <= yt + 10;
}

void tickScreen() {
  if (screen > 1) {
    tft.setTextSize(4);
    int x = 10, y = 10, x1, y1, w, h;
    tft.getTextBounds("<", &x, &y, &x1, &y1, &w, &h);
    if (clickedArea(x, y, x + w, y + h)) {
      if (screen == 3) {
        state = pre_settings_state;
        if (!send(1, state.mode, state.variant)) return;
      }
      screenSwitch(prevScreen);
      return;
    }
  }
  switch (screen) {
    case 0:
      {
        int x = 10, y = 10, x1, y1, w, h;
        tft.getTextBounds("Modes", &x, &y, &x1, &y1, &w, &h);
        if (clickedArea(x, y, x + w, y + h)) {
          screenSwitch(2);
          return;
        }
        radio.enableDynamicPayloads();
        x = 10, y = 40;
        tft.getTextBounds("Settings", &x, &y, &x1, &y1, &w, &h);
        if (clickedArea(x, y, x + w, y + h)) {
          pre_settings_state = state;
          send(1, 127);  // system mode: blinking bottom plane
          screenSwitch(3);
          return;
        }
        break;
      }
    case 2:
      {
        if (state.modeI() == 8) {
          if (clickedArea(210, 10, 240, 40)) {
            state.mode = 0;
            state.variant = 0;
            renderScreen();
          } else if (clickedArea(90, 60, 150, 130))
            send(6, 2);
          else if (clickedArea(90, 130, 150, 260))
            send(6, 3);
          else if (clickedArea(20, 130, 90, 190))
            send(6, 0);
          else if (clickedArea(150, 130, 220, 190))
            send(6, 1);
          else if (clickedArea(20, 220, 80, 290))
            send(6, 4);
          else if (clickedArea(160, 220, 220, 290))
            send(6, 5);
        } else {
          if (clickedArea(160, 10, 190, 40)) {
            page--;
            if (page < 0) {
              page = (modesCount / pageSize + (modesCount % pageSize != 0)) - 1;
            }
            renderScreen();
          } else if (clickedArea(200, 10, 230, 40)) {
            page = ++page % (modesCount / pageSize + (modesCount % pageSize != 0));
            renderScreen();
          } else if (clickedArea(0, 290, 320, 320) && modes[state.modeI()].variationsCount > 1) {
            state.variant = ++state.variant % modes[state.modeI()].variationsCount;
            renderScreen();
          } else if (clickedArea(0, 70, 240, 320)) {
            int modeClicked = (press_y - 60) / 30 + page * pageSize;
            if (state.modeI() == modeClicked) {
              state.mode = 0;
              state.variant = 0;
            } else {
              state.mode = modeClicked + 1;
              state.variant = 0;
            }
            renderScreen();
          }
        }
        break;
      }
    case 3:
      {
        int x = 10, y = 60, x1, y1, w, h;
        switch (page) {
          case 0:
            {
              tft.getTextBounds(brightnessLevelNames[settings[0]], &x, &y, &x1, &y1, &w, &h);
              if (clickedArea(x, y, x + w, y + h)) {
                settings[0] = ++settings[0] % 4;
                renderScreen();
                sendSetting(0);
                return;
              }
              break;
            }
          case 1:
            {
              if (clickedArea(10, 90, 200, 120)) {  // square
                settings[1] = (settings[1] & 0b1001111) | (((((settings[1] & 48) >> 4) + 1) % 4) << 4);
                renderScreen();
                sendSetting(1);
                return;
              }
              if (clickedArea(10, 60, 200, 90)) {  // cross
                settings[1] = (settings[1] & 0b1110000) | (((settings[1] & 15) + 1) % 6);
                renderScreen();
                sendSetting(1);
                return;
              }
              if (clickedArea(10, 120, 200, 150)) {  // dot
                settings[1] ^= 64;
                renderScreen();
                sendSetting(1);
                return;
              }
              break;
            }
        }

        if (page > 0 && clickedArea(10, 280, 40, 310)) {
          page--;
          renderScreen();
        }
        if (page < settingsSize - 1 && clickedArea(200, 280, 230, 310)) {
          page++;
          renderScreen();
        }
        break;
      }
  }
}

void sendSetting(byte i) {
  send(2, i, settings[i]);
}

void fetchSetting(byte i) {
  if (settingsFetched[i]) return;
  byte settingValue = -1;
  if (send(3, i, 0, &settingValue) && send(3, i, 0, &settingValue)) {
    settings[i] = settingValue;
    settingsFetched[i] = true;
  }
}

void loop(void) {
  touchUpdate();
  tickScreen();

  if (connected) {
    if (RM_state != state && send(1, state.mode, state.variant) && initState)
      RM_state = state;
    if (!initState) {
      byte mod, var;
      if (send(4, 0, 0, &mod) && send(5, 0, 0, &mod) && send(0, 0, 0, &var)) {
        state.mode = mod;
        state.variant = var;
        initState = true;
      }
    }
  }

  // heartbeat
  if (millis() - TM_heartbeat > heartbeatThreshold && !sentFlag) {
    if (!connected) {
      if (send(0, 0))
        screenSwitch(connectedScreen);
    } else
      send(0, 0);
    TM_heartbeat = millis();
  }

  clicked = false;
  sentFlag = false;
}
