#include "EEPROM.h"
#include "LedCube.h"

const int settings_count = 128;
const byte initKey = 240;
const byte modeAddress = 300;
const byte variantAddress = 301;

LedCube* LEDpointer::cube;

void LedCube::render() {
  if (micros() - TM_draw <= (128 - brightness()) * 120) {
    return;
  }
  TM_draw = micros();
  for (int i = 0; i < 8; ++i) {
    digitalWrite(shift_latch, LOW);
    SPI.transfer(1 << i);
    byte b = 0, x = 0, y = 0, z = 0;
    for (int j = 0; j < 8; ++j) {
      b = 0;
      for (int k = 0; k < 8; ++k) {
        byte cross_loc = this->settings[1] & 15;
        byte square_loc = (this->settings[1] & 48) >> 4;
        byte dot_loc = (this->settings[1] & 64) >> 6;
        // cross loc
        switch (cross_loc) {
          case 0:
            {
              x = i;
              y = j;
              z = k;
              break;
            }
          case 1:
            {
              x = k;
              y = i;
              z = j;
              break;
            }
          case 2:
            {
              x = j;
              y = k;
              z = i;
              break;
            }
          case 3:
            {
              x = 7 - i;
              y = j;
              z = k;
              break;
            }
          case 4:
            {
              x = 7 - k;
              y = i;
              z = j;
              break;
            }
          case 5:
            {
              x = 7 - j;
              y = k;
              z = i;
              break;
            }
        }
        // square loc
        if (square_loc > 1) {
          byte t = z;
          z = y;
          y = t;
        }
        if (square_loc % 2) {
          y = 7 - y;
        }
        // dot loc
        if (dot_loc)
          z = 7 - z;

        if ((field[x][y] & (1 << z)) > 0)
          b |= (1 << k);
      }
      SPI.transfer(b);
    }
    digitalWrite(shift_latch, HIGH);
    delayMicroseconds(brightness() * 6);
    digitalWrite(shift_latch, LOW);

    for (int j = 0; j < 9; j++) {
      SPI.transfer(zeros[i]);
    }
    digitalWrite(shift_latch, HIGH);
  }
}

byte LedCube::brightness() {
  switch (settings[0]) {
    case 0:
      return 0;
    case 1:
      return 10;
    case 2:
      return 40;
    case 3:
      return 128;
  }
}

void LedCube::clear() {
  for (int i = 0; i < 8; ++i) {
    for (int j = 0; j < 8; ++j) {
      field[i][j] = 0;
    }
  }
}

void LedCube::light() {
  for (int i = 0; i < 8; ++i) {
    for (int j = 0; j < 8; ++j) {
      field[i][j] = 0b11111111;
    }
  }
}

void LedCube::set(byte x, byte y, byte z, bool val) {
  if (val) {
    field[y][x] = field[y][x] | (1 << z);
  } else {
    field[y][x] = field[y][x] & ~(1 << z);
  }
}

bool LedCube::get(byte x, byte y, byte z) {
  return (field[y][x] & (1 << z)) != 0;
}


LedCube::LedCube(byte _shift_latch) {
  shift_latch = _shift_latch;
  TM_draw = 0;
}

void LedCube::begin() {
  LEDpointer::cube = this;
  SPI.begin();
  SPI.beginTransaction(SPISettings(8000000, LSBFIRST, SPI_MODE0));
  pinMode(shift_latch, OUTPUT);
  digitalWrite(shift_latch, HIGH);
  if (EEPROM[511] != initKey) {
    for (int i = 0; i < settings_count; ++i) {
      EEPROM[i] = 0;
      settings[i] = 0;
    }
    EEPROM[511] = initKey;
  } else {
    for (int i = 0; i < settings_count; ++i) {
      settings[i] = EEPROM[i];
    }
  }
}

void LedCube::tick() {
  if (millis() - TM_mode > modeDelays[mode]) {
    TM_mode = millis();
    modes[mode][1](this);
  }
  render();
}

void LedCube::setMode(byte mode) {
  this->mode = mode;
  clear();
  modes[mode][0](this);
  TM_mode = 0;
}

byte LedCube::setting(byte i) {
  return this->settings[i];
}

void LedCube::setSetting(byte i, byte value) {
  this->settings[i] = value;
  EEPROM[i] = value;
}

byte LedCube::getMode() {
  return mode;
}

void LedCube::setVariant(byte variant) {
  this->variant = variant;
}

byte LedCube::getVariant() {
  return this->variant;
}

LEDpointer LedCube::led(int x, int y, int z) {
  LEDpointer pt(x, y, z);
  return pt;
}
