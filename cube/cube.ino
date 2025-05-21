#include "LedCube.h";
#define SOFT_SPI_MISO_PIN 4
#define SOFT_SPI_MOSI_PIN 5
#define SOFT_SPI_SCK_PIN 6
// radio
#include "nRF24L01.h"
#include "RF24.h"
#include "DigitalIO.h"

RF24 radio(8, 7);
// --- end radio
byte gameControl = 0;
LedCube cube(10);
#define MODEDEF(delay, setup, update) \
  cube.modeDelays[modeNumber] = delay; \
  cube.modes[modeNumber][0] = [](LedCube* c) { \
    setup; \
  }; \
  cube.modes[modeNumber++][1] = [](LedCube* c) { \
    update; \
  };
#define STATIC_MODEDEF(setup) MODEDEF(500, setup, setup)
#define REG c->registry
#define SET c->set
#define GET c->get
#define USERMODES(a) \
  void userModes() { \
    int modeNumber = 0; \
    if (true) a; \
  }
#define DARKNESS \
  { c->clear(); }
#define VAR c->getVariant()
int PROG_i;
int PROG_counter;
int to_update;
bool needStart = false;
struct prog_entry {
  byte mode, var;
};
prog_entry* curr_prog = nullptr;
void loadProgram(int switchDelay, LedCube* c, int n, ...) {
  PROG_i = 0;
  PROG_counter = 0;
  needStart = true;
  to_update = 0;
  if (curr_prog != nullptr)
    delete[] curr_prog;
  va_list prog;
  va_start(prog, n);
  curr_prog = new prog_entry[n];
  for (int i = 0; i < n; ++i) {
    curr_prog[i] = { (byte)(va_arg(prog, int)), (byte)(va_arg(prog, int)) };
  }
  va_end(prog);
}

template<typename T>
void swap(T* arr, int i, int j) {
  if (i == j)
    return;
  T t = arr[i];
  arr[i] = arr[j];
  arr[j] = t;
}

template<typename T>
void shuffle(T* arr, int n) {
  if (n <= 1)
    return;
  for (int i = 0; i < n; i++) {
    swap(arr, i, random(0, n));
  }
}

void programRun(int switchDelay, LedCube* c, int n) {
  c->setVariant(curr_prog[PROG_i].var);
  if (to_update <= 0) {
    if (needStart) {
      c->modes[curr_prog[PROG_i].mode][0](c);
      needStart = false;
    }
    c->modes[curr_prog[PROG_i].mode][1](c);
    c->modeDelays[c->getMode()] = c->modeDelays[curr_prog[PROG_i].mode];
  }
  PROG_counter++;
  if (PROG_counter >= switchDelay / c->modeDelays[c->getMode()]) {
    needStart = true;
    PROG_i = ++PROG_i % n;
    PROG_counter = 0;
  }
}
#define PROGDEF(switchDelay, ...) MODEDEF( \
  10, { loadProgram(switchDelay, c, (sizeof((int[]){ __VA_ARGS__ }) / sizeof(int)) / 2, __VA_ARGS__); }, { programRun(switchDelay, c, (sizeof((int[]){ __VA_ARGS__ }) / sizeof(int)) / 2); })
#define PM(a, b) a, b
#define PMD(a) a, 0

struct cord {
  byte x, y, z, d;
  cord avg(cord c) {
    return cord{ (x + c.x) / 2, (y + c.y) / 2, (z + c.z) / 2, 0 };
  }
  inline cord operator=(cord c) {
    x = c.x;
    y = c.y;
    z = c.z;
    d = c.d;
    return *this;
  }
  inline bool operator==(cord c) {
    return x == c.x && y == c.y && z == c.z;
  }
  inline bool operator!=(cord c) {
    return x != c.x || y != c.y || z != c.z;
  }
};

USERMODES({
  // darkness ,_,
  STATIC_MODEDEF(DARKNESS);
  PROGDEF(15000, PMD(2), PMD(4), PM(5, 0), PM(5, 1), PM(5, 4), PM(5, 5), PM(6, 0), PM(6, 1), PM(7, 1), PM(7, 2), PM(8, 2));
  // rain
  MODEDEF(140, DARKNESS, {
    for (int i = 6; i >= 0; i--) {
      for (int j = 0; j < 8; j++) {
        for (int k = 0; k < 8; k++) {
          SET(j, i + 1, k, GET(j, i, k));
        }
      }
    }
    for (int j = 0; j < 8; j++) {
      for (int k = 0; k < 8; k++) {
        SET(j, 0, k, false);
      }
    }
    byte newCount = random(2, 7);
    for (int i = 0; i < newCount; ++i) {
      int x = random(0, 8);
      int z = random(0, 8);
      SET(x, 0, z, true);
    }
  });
  // light
  STATIC_MODEDEF({ c->light(); });
  // planes
  MODEDEF(
    100,
    {
      c->clear();
      REG[0] = 0;
      REG[1] = 1;
      REG[2] = 0;
    },
    {
      REG[0] += (REG[1] == 1) ? 1 : -1;
      if (REG[0] == 7 && (REG[1] == 1)) {
        REG[1] = 0;
      } else if (REG[0] == 0 && REG[1] == 0) {
        REG[1] = 1;
        REG[2] = ++(REG[2]) % 3;
      }
      c->clear();
      for (int i = 0; i < 8; i++)
        for (int j = 0; j < 8; j++)
          if (REG[2] == 0)
            SET(i, REG[0], j, true);
          else if (REG[2] == 1)
            SET(REG[0], i, j, true);
          else
            SET(i, j, REG[0], true);
    });


  // spirals
  MODEDEF(
    90,
    {
      c->clear();
      REG[0] = 2;
      REG[1] = 0;
    },
    {
      // shift down & clear last layer
      for (int i = 6; i >= 0; i--) {
        for (int j = 0; j < 8; j++) {
          for (int k = 0; k < 8; k++) {
            SET(j, i + 1, k, GET(j, i, k));
          }
        }
      }
      for (int j = 0; j < 8; j++) {
        for (int k = 0; k < 8; k++) {
          c->set(j, 0, k, false);
        }
      }
      // draw new last layer
      byte p = REG[0] = ++(REG[0]) % 7;
      if (p == 0) {
        REG[1] = ((REG[1] == 0) ? 1 : 0);
      }
      for (int i = 0; i < min(4, (VAR > 3 ? VAR - 4 : VAR) + 1); ++i) {
        byte x = min(p + 4, 7 - i);
        byte y = max(p - 3, 0 + i);
        if (REG[1] == 1 || VAR > 3) {
          SET(x, 0, y, true);
          SET(7 - x, 0, 7 - y, true);
        }
        if (REG[1] == 0 || VAR > 3) {
          SET(7 - y, 0, x, true);
          SET(y, 0, 7 - x, true);
        }
      }
    });


  // breathing
  MODEDEF(
    130,
    {
      c->clear();
      REG[0] = 0;
      REG[1] = 0;
    },
    {
      if (VAR == 0) {
        REG[1] = ++REG[1] % 4;
        if (REG[1] == 0)
          REG[0] = !REG[0];
      } else if (VAR == 1) {
        REG[1] = ++REG[1] % 4;
      } else if (VAR == 2) {
        if (REG[1] == 0)
          REG[1] = 3;
        else
          REG[1]--;
      }
      byte pos = REG[0] ? REG[1] : 3 - REG[1];
      c->clear();
      byte f = 3 - pos;
      byte t = 4 + pos;
      for (byte i = f; i <= t; i++) {
        for (byte j = f; j <= t; j += (t - f)) {
          for (byte k = f; k <= t; k += (t - f)) {
            SET(i, j, k, true);
            SET(j, i, k, true);
            SET(j, k, i, true);
          }
        }
      }
    });

  // fill
  MODEDEF(
    90,
    {
      c->clear();
      REG[0] = 0;
      REG[1] = 1;
      REG[2] = random(0, 8);
    },
    {
      if (VAR == 0) {
        if (REG[0] < 8) {
          for (int i = 0; i < 8; ++i) {
            for (int j = 0; j < 8; ++j) {
              SET(i, REG[0], j, REG[1]);
            }
          }
        }
        REG[0]++;
        if (REG[0] >= 14) {
          REG[1] = !REG[1];
          REG[0] = 0;
        }
      } else if (VAR == 1 || VAR == 2) {
        REG[0]++;
        if (REG[0] < 22) {
          for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
              for (int k = 0; k < 8; k++) {
                if (VAR == 1 && j != 0 && j != 7 && k != 0 && k != 7 && i != 0 && i != 7)
                  continue;
                if (((REG[2] & 1) ? i : 7 - i) + ((REG[2] & 2) ? j : 7 - j) + ((REG[2] & 4) ? k : 7 - k) <= REG[0]) {
                  SET(i, j, k, REG[1]);
                }
              }
            }
          }
        }
        if (REG[0] >= 30) {
          REG[1] = !REG[1];
          REG[0] = 0;
          if (REG[1])
            REG[2] = random(0, 8);
        }
      }
    });
  // wave
  MODEDEF(
    80,
    {
      c->clear();
      REG[0] = 0;

      REG[1] = random(0, 2);
      REG[2] = REG[1] ? random(15, 20) : random(0, 3);
      REG[3] = random(0, 2);
      REG[4] = REG[1] ? random(15, 20) : random(0, 3);
    },
    {
      if (VAR == 0) {
        REG[0] = ++REG[0] % 30;
        c->clear();
        for (int i = 0; i < 8; ++i) {
          for (int j = 0; j < 8; ++j) {
            SET(i, min(5, abs((int)REG[0] - i - j) + 2), j, true);
          }
        }
      } else if (VAR == 1) {
        if (REG[1])
          REG[2]--;
        else
          REG[2]++;
        if (REG[2] == 0 || REG[2] >= 25) {
          REG[1] = random(0, 2);
          REG[2] = REG[1] ? random(15, 20) : random(0, 3);
        }

        if (REG[3])
          REG[4]--;
        else
          REG[4]++;
        if (REG[4] == 0 || REG[4] >= 25) {
          REG[3] = random(0, 2);
          REG[4] = REG[4] ? random(15, 20) : random(0, 3);
        }

        c->clear();
        for (int i = 0; i < 8; ++i) {
          for (int j = 0; j < 8; ++j) {
            SET(i, 5 - min(4, min(2, 3 - min(3, abs((int)REG[2] - (i + 5)))) + min(2, 3 - min(3, abs((int)REG[4] - (j + 5))))), j, true);
          }
        }
      } else if (VAR == 2) {
        for (int i = 0; i < 8; ++i) {
          for (int j = 0; j < 8; ++j) {
            byte surface = 7 - min(4, min(2, 3 - min(3, abs((int)REG[2] - (i + 5)))) + min(2, 3 - min(3, abs((int)REG[4] - (j + 5)))));
            SET(i, surface, j, false);
          }
        }

        if (REG[1])
          REG[2]--;
        else
          REG[2]++;
        if (REG[2] == 0 || REG[2] >= 25) {
          REG[1] = random(0, 2);
          REG[2] = REG[1] ? random(15, 20) : random(0, 3);
        }

        if (REG[3])
          REG[4]--;
        else
          REG[4]++;
        if (REG[4] == 0 || REG[4] >= 25) {
          REG[3] = random(0, 2);
          REG[4] = REG[4] ? random(15, 20) : random(0, 3);
        }

        for (int i = 0; i < 8; ++i) {
          for (int j = 0; j < 8; ++j) {
            byte surface = 7 - min(4, min(2, 3 - min(3, abs((int)REG[2] - (i + 5)))) + min(2, 3 - min(3, abs((int)REG[4] - (j + 5)))));
            SET(i, surface, j, true);
            for (int k = surface - 1; k >= 0; --k) {
              SET(i, k, j, k != 0 && GET(i, k - 1, j));
            }
            for (int k = surface + 1; k < 8; ++k) {
              SET(i, k, j, false);
            }
          }
        }

        byte newCount = random(0, 3);
        for (int i = 0; i < newCount; ++i) {
          int x = random(0, 8);
          int z = random(0, 8);
          SET(x, 0, z, true);
        }
      }
    });

  // labyrinth
  MODEDEF(
    100,
    ({
      c->clear();
      for (int i = 0; i < 70; ++i) {
        REG[i] = 0;
      }
      cord stack[64];
      cord to[6];
      byte si, tos;
      tos = si = 0;
      cord C, finish = { 0, 0, 0, 0 };
      stack[si++] = { 0, 0, 0, 0 };
      REG[0] = 1;
      while (si > 0) {
        C = stack[--si];
        if (C.d > finish.d)
          finish = C;
        tos = 0;
        if (C.z > 1 && (!(REG[C.y * 8 + C.x] & (1 << (C.z - 2))) || !random(8)))
          to[tos++] = { C.x, C.y, C.z - 2, C.d + 1 };
        if (C.z < 6 && (!(REG[C.y * 8 + C.x] & (1 << (C.z + 2))) || !random(8)))
          to[tos++] = { C.x, C.y, C.z + 2, C.d + 1 };

        if (C.x > 1 && (!(REG[C.y * 8 + C.x - 2] & (1 << C.z)) || !random(8)))
          to[tos++] = { C.x - 2, C.y, C.z, C.d + 1 };
        if (C.x < 6 && (!(REG[C.y * 8 + C.x + 2] & (1 << C.z)) || !random(8)))
          to[tos++] = { C.x + 2, C.y, C.z, C.d + 1 };

        if (C.y > 1 && (!(REG[C.y * 8 - 16 + C.x] & (1 << C.z)) || !random(8)))
          to[tos++] = { C.x, C.y - 2, C.z, C.d + 1 };
        if (C.y < 6 && (!(REG[C.y * 8 + 16 + C.x] & (1 << C.z)) || !random(8)))
          to[tos++] = { C.x, C.y + 2, C.z, C.d + 1 };
        shuffle(to, tos);
        for (int i = 0; i < tos; ++i) {
          cord avg = C.avg(to[i]);
          if (!(REG[to[i].y * 8 + to[i].x] & (1 << to[i].z))) {
            stack[si++] = to[i];
            REG[to[i].y * 8 + to[i].x] |= (1 << to[i].z);
          }
          REG[avg.y * 8 + avg.x] |= (1 << avg.z);
        }
      };
      REG[70] = finish.x;
      REG[71] = finish.y;
      REG[72] = finish.z;
      gameControl = 0;
    }),
    ({
      if (REG[65] == REG[70] && REG[66] == REG[71] && REG[67] == REG[72] && !REG[69]) {
        REG[69] = 1;
        for (int x = 0; x < 8; ++x) {
          for (int y = 0; y < 8; ++y) {
            for (int z = 0; z < 8; ++z) {
              SET(x, y, z, REG[y * 8 + x] & (1 << z));
            }
          }
        }
      }
      if (REG[69]) {
        SET(REG[65], REG[66], REG[67], true);
      }
      // move player
      if (gameControl != 0) {
        gameControl--;
        switch (gameControl) {
          case 0:
            {
              if (REG[65] > 0 && (REG[REG[66] * 8 + REG[65] - 1] & (1 << REG[67])))
                REG[65]--;
              break;
            }
          case 1:
            {
              if (REG[65] < 7 && (REG[REG[66] * 8 + REG[65] + 1] & (1 << REG[67])))
                REG[65]++;
              break;
            }
          case 2:
            {
              if (REG[67] > 0 && (REG[REG[66] * 8 + REG[65]] & (1 << (REG[67] - 1))))
                REG[67]--;
              break;
            }
          case 3:
            {
              if (REG[67] < 7 && (REG[REG[66] * 8 + REG[65]] & (1 << (REG[67] + 1))))
                REG[67]++;
              break;
            }
          case 4:
            {
              if (REG[66] < 7 && (REG[REG[66] * 8 + 8 + REG[65]] & (1 << REG[67])))
                REG[66]++;
              break;
            }
          case 5:
            {
              if (REG[66] > 0 && (REG[REG[66] * 8 - 8 + REG[65]] & (1 << REG[67])))
                REG[66]--;
              break;
            }
        }
        gameControl = 0;
      }
      REG[68] = ++REG[68] % 8;
      if (!REG[69]) {
        // display field
        c->clear();
        cord stack[20];
        cord C;
        byte si = 0;
        stack[si++] = { REG[65], REG[66], REG[67], 0 };
        while (si > 0) {
          C = stack[--si];
          if (C.d > 2)
            continue;
          if (C.x == REG[70] && C.y == REG[71] && C.z == REG[72]) {
            SET(C.x, C.y, C.z, REG[68] % 2);

          } else
            SET(C.x, C.y, C.z, true);
          if (C.x > 0 && (REG[C.y * 8 + C.x - 1] & (1 << C.z)))
            stack[si++] = { C.x - 1, C.y, C.z, C.d + 1 };
          if (C.x < 7 && (REG[C.y * 8 + C.x + 1] & (1 << C.z)))
            stack[si++] = { C.x + 1, C.y, C.z, C.d + 1 };

          if (C.y > 0 && (REG[C.y * 8 - 8 + C.x] & (1 << C.z)))
            stack[si++] = { C.x, C.y - 1, C.z, C.d + 1 };
          if (C.y < 7 && (REG[C.y * 8 + 8 + C.x] & (1 << C.z)))
            stack[si++] = { C.x, C.y + 1, C.z, C.d + 1 };

          if (C.z > 0 && (REG[C.y * 8 + C.x] & (1 << (C.z - 1))))
            stack[si++] = { C.x, C.y, C.z - 1, C.d + 1 };
          if (C.z < 7 && (REG[C.y * 8 + C.x] & (1 << (C.z + 1))))
            stack[si++] = { C.x, C.y, C.z + 1, C.d + 1 };
        }
      }
      if (REG[69])
        SET(REG[70], REG[71], REG[72], REG[68] % 2);
      SET(REG[65], REG[66], REG[67], REG[68] > 2);
    }));
});

void setupModes() {
  userModes();

  // settings system mode - 127
  cube.modeDelays[127] = 500;
  cube.modes[127][0] = [](LedCube* c) {
    c->clear();
  };
  cube.modes[127][1] = [](LedCube* c) {
    c->clear();
    for (int i = 1; i <= 6; ++i) {
      SET(i, 7, i, true);
      SET(i, 7, 7 - i, true);
    }
    for (int i = 1; i <= 6; ++i) {
      SET(0, 1, i, true);
      SET(0, 6, i, true);
      SET(0, i, 1, true);
      SET(0, i, 6, true);
    }
    for (int i = 3; i <= 4; ++i) {
      SET(i, i, 0, true);
      SET(i, 7 - i, 0, true);
    }
  };
}

void setup(void) {
  Serial.begin(9600);

  uint32_t seed = 0;
  for (int i = 0; i < 16; i++) {
    seed *= 4;
    seed += analogRead(A0) & 3;
    randomSeed(seed);
    srandom(seed);
  }

  setupModes();
  cube.begin();

  radio.begin();
  radio.setAutoAck(1);
  radio.setRetries(5, 15);
  radio.setPayloadSize(3);
  radio.enableDynamicPayloads();
  radio.enableAckPayload();
  radio.openWritingPipe(*"2Node");
  radio.openReadingPipe(1, *"1Node");
  radio.setChannel(0x60);
  radio.powerUp();
  radio.startListening();
}

void readRadio() {
  byte pipeNo;
  while (radio.available(&pipeNo)) {
    byte received[3];
    radio.read(received, 3);
    byte command = received[0];
    switch (command) {
      case 0:
        {
          // heartbeat
          break;
        }
      case 1:
        {
          cube.setMode(received[1]);
          cube.setVariant(received[2]);
          break;
        }
      case 2:
        {
          cube.setSetting(received[1], received[2]);
          break;
        }
      case 3:
        {
          byte buf = cube.setting(received[1]);
          radio.writeAckPayload(pipeNo, &buf, 1);
          break;
        }
      case 4:
        {
          byte buf = cube.getMode();
          radio.writeAckPayload(pipeNo, &buf, 1);
          break;
        }
      case 5:
        {
          byte buf = cube.getVariant();
          radio.writeAckPayload(pipeNo, &buf, 1);
          break;
        }
      case 6:
        {
          gameControl = received[1] + 1;
          break;
        }
    }
  }
}

void loop(void) {
  cube.tick();
  readRadio();
}
