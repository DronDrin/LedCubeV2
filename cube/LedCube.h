#ifndef LedCube_h
#define LedCube_h
#include "Arduino.h"
#include <stdint.h>
#include <SPI.h>
#include <Wire.h>
#include <EEPROM.h>

struct LEDpointer;
class LedCube {
private:
  unsigned long TM_draw;
  unsigned long TM_mode;
  byte shift_latch;
  byte field[8][8];
  byte zeros[9];
  byte mode;
  void render();
  byte settings[128];
  byte variant;
public:
  LedCube(byte _shift_latch);
  void begin();
  void tick();
  void setMode(byte mode);
  void (*modes[128][2])(LedCube* c);
  int modeDelays[128];
  void clear();
  void light();
  void set(byte x, byte y, byte z, bool val);
  bool get(byte x, byte y, byte z);
  byte registry[128];
  byte setting(byte i);
  void setSetting(byte i, byte value);
  byte brightness();
  byte getMode();
  byte getVariant();
  void setVariant(byte variant);
  LEDpointer led(int x, int y, int z);
};

struct LEDpointer {
private:
  int x, y, z;
public:
  static LedCube* cube;
  LEDpointer(int x, int y, int z) {
    this->x = x;
    this->y = y;
    this->z = z;
  }

  operator bool() const {
    return cube->get(x, y, z);
  }

  void operator=(bool v) {
    cube->set(x, y, z, v);
  }

  void operator=(LEDpointer pt) {
    cube->set(x, y, z, cube->get(pt.x, pt.y, pt.z));
  }
};

#endif