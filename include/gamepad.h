#pragma once
#include <Arduino.h>
#include <stdint.h>
#include <string.h>

// Button Identifier Enum
enum ButtonID : uint8_t {
  BTN_A,
  BTN_B,
  BTN_X,
  BTN_Y,
  BTN_L1,
  BTN_R1,
  BTN_L2,
  BTN_R2,
  BTN_SELECT,
  BTN_START,
  BTN_L3,
  BTN_R3,
  BTN_DPAD_UP,
  BTN_DPAD_DOWN,
  BTN_DPAD_LEFT,
  BTN_DPAD_RIGHT
};

struct GamepadState {
  // Analog sticks
  int16_t lx = 0;
  int16_t ly = 0;
  int16_t rx = 0;
  int16_t ry = 0;

  // D-Pad
  bool dpad_up    = false;
  bool dpad_down  = false;
  bool dpad_left  = false;
  bool dpad_right = false;

  // Face Buttons
  bool btn_a = false;
  bool btn_b = false;
  bool btn_x = false;
  bool btn_y = false;

  // Shoulders & Triggers
  bool btn_l1 = false;
  bool btn_r1 = false;
  bool btn_l2 = false;
  bool btn_r2 = false;

  // Auxiliary Buttons
  bool btn_select = false;
  bool btn_start  = false;
  bool btn_l3     = false;
  bool btn_r3     = false;

  uint16_t raw_btn_mask = 0;
  bool connected = false;

  // Helper to query any button state by enum
  bool getButton(ButtonID btn) const {
    switch (btn) {
      case BTN_A:          return btn_a;
      case BTN_B:          return btn_b;
      case BTN_X:          return btn_x;
      case BTN_Y:          return btn_y;
      case BTN_L1:         return btn_l1;
      case BTN_R1:         return btn_r1;
      case BTN_L2:         return btn_l2;
      case BTN_R2:         return btn_r2;
      case BTN_SELECT:     return btn_select;
      case BTN_START:      return btn_start;
      case BTN_L3:         return btn_l3;
      case BTN_R3:         return btn_r3;
      case BTN_DPAD_UP:    return dpad_up;
      case BTN_DPAD_DOWN:  return dpad_down;
      case BTN_DPAD_LEFT:  return dpad_left;
      case BTN_DPAD_RIGHT: return dpad_right;
      default:             return false;
    }
  }
};

inline bool operator==(const GamepadState& a, const GamepadState& b) {
  return memcmp(&a, &b, sizeof(GamepadState)) == 0;
}
inline bool operator!=(const GamepadState& a, const GamepadState& b) {
  return !(a == b);
}

// Edge Detection Functions
inline bool isHeld(ButtonID btn, const GamepadState& curr) {
  return curr.getButton(btn);
}

inline bool justPressed(ButtonID btn, const GamepadState& curr, const GamepadState& prev) {
  return curr.getButton(btn) && !prev.getButton(btn);
}

inline bool justReleased(ButtonID btn, const GamepadState& curr, const GamepadState& prev) {
  return !curr.getButton(btn) && prev.getButton(btn);
}

// Deadzone filtering and scaling
static inline int16_t mapAxis(uint8_t raw, bool invert = false, int deadzone = 12) {
  int diff = (int)raw - 128;
  if (abs(diff) <= deadzone) return 0;

  int val = 0;
  if (diff > 0) {
    val = ((diff - deadzone) * 255) / (127 - deadzone);
  } else {
    val = ((diff + deadzone) * 255) / (128 - deadzone);
  }
  val = constrain(val, -255, 255);
  return invert ? -val : val;
}

// Corrected Parser for Cytron USB Gamepad
static inline bool parseGamepadReport(const uint8_t *report, uint16_t len, GamepadState &gp) {
  if (len < 7) return false;

  uint16_t btns = (uint16_t)report[0] | ((uint16_t)report[1] << 8);
  gp.raw_btn_mask = btns;

  // Face Buttons
  gp.btn_a = (btns & (1 << 0)) != 0; // Bit 0
  gp.btn_b = (btns & (1 << 1)) != 0; // Bit 1
  gp.btn_x = ((btns & (1 << 3)) != 0) || ((btns & (1 << 2)) != 0); // Bit 3 or 2
  gp.btn_y = (btns & (1 << 4)) != 0; // Bit 4

  // Shoulders & Triggers
  gp.btn_l1 = (btns & (1 << 6)) != 0; // Bit 6
  gp.btn_r1 = (btns & (1 << 7)) != 0; // Bit 7
  gp.btn_l2 = (btns & (1 << 8)) != 0; // Bit 8
  gp.btn_r2 = (btns & (1 << 9)) != 0; // Bit 9

  // Menu Buttons
  gp.btn_select = (btns & (1 << 10)) != 0; // Bit 10
  gp.btn_start  = (btns & (1 << 11)) != 0; // Bit 11

  // Thumbstick Clicks
  gp.btn_l3 = (btns & (1 << 13)) != 0; // Bit 13
  gp.btn_r3 = (btns & (1 << 14)) != 0; // Bit 14

  // D-Pad
  uint8_t hat = report[2] & 0x0F;
  gp.dpad_up    = (hat == 0 || hat == 1 || hat == 7);
  gp.dpad_right = (hat == 1 || hat == 2 || hat == 3);
  gp.dpad_down  = (hat == 3 || hat == 4 || hat == 5);
  gp.dpad_left  = (hat == 5 || hat == 6 || hat == 7);

  // Analog Sticks
  gp.lx = mapAxis(report[3], false);
  gp.ly = mapAxis(report[4], true);
  gp.rx = mapAxis(report[5], false);
  gp.ry = mapAxis(report[6], true);

  gp.connected = true;
  return true;
}