#include <Arduino.h>
#include "CytronMotorDriver.h"
#include "usbh_helper.h"
#include "gamepad.h"

// Motors Config
// configured so that a positive value is forward and a negative is backward
CytronMD motor1(PWM_PWM, 9, 8);   // front left
CytronMD motor2(PWM_PWM, 11, 10); // front right
CytronMD motor3(PWM_PWM, 13, 12); // rear left
CytronMD motor4(PWM_PWM, 15, 14); // rear right

// Shared gamepad state
GamepadState gamepad;
GamepadState lastGamepad;
volatile bool newGamepadData = false;

// 1 = max, 0.5 = half
// max speed is 255, so 1 is 255
float currentSpeedMultiplier = 0.25;

// Button names for logging
const struct
{
  ButtonID id;
  const char *name;
} BUTTON_LIST[] = {
    {BTN_A, "A"}, {BTN_B, "B"}, {BTN_X, "X"}, {BTN_Y, "Y"}, {BTN_L1, "L1"}, {BTN_R1, "R1"}, {BTN_L2, "L2"}, {BTN_R2, "R2"}, {BTN_SELECT, "SELECT"}, {BTN_START, "START"}, {BTN_L3, "L3"}, {BTN_R3, "R3"}, {BTN_DPAD_UP, "DPad-UP"}, {BTN_DPAD_DOWN, "DPad-DOWN"}, {BTN_DPAD_LEFT, "DPad-LEFT"}, {BTN_DPAD_RIGHT, "DPad-RIGHT"}};

void driveArcade(int forward, int turn)
{
  // Invert steering when reversing for natural car-like reverse handling
  if (forward < 0)
  {
    turn = -turn;
  }

  // Calculate raw differential motor speeds
  int leftSpeed = forward + turn;
  int rightSpeed = forward - turn;

  // Scale down proportionately if speed exceeds max limits (-255 to 255)
  int maxVal = max(abs(leftSpeed), abs(rightSpeed));
  if (maxVal > 255)
  {
    leftSpeed = (leftSpeed * 255) / maxVal;
    rightSpeed = (rightSpeed * 255) / maxVal;
  }

  // Drive motors
  motor1.setSpeed(leftSpeed);  // Front Left
  motor2.setSpeed(rightSpeed); // Front Right
  motor3.setSpeed(leftSpeed);  // Rear Left
  motor4.setSpeed(rightSpeed); // Rear Right
}

// =================================================================
// CORE 0: Application Logic, Serial Monitor & Motor Control
// =================================================================
void setup()
{
  Serial.begin(115200);
  delay(1000); // Allow USB-CDC Serial to initialize

  Serial.println("==========================================");
  Serial.println(" Cytron Motion 2350 Pro - Dual-Core Ready ");
  Serial.println(" Core 0: Application & Robot Logic        ");
  Serial.println(" Core 1: Dedicated USB Host Task          ");
  Serial.println("==========================================");
}

void loop()
{
  // Check if Core 1 received new gamepad data
  if (newGamepadData)
  {
    newGamepadData = false;

    // Check stick movements
    if (gamepad.lx != lastGamepad.lx || gamepad.ly != lastGamepad.ly ||
        gamepad.rx != lastGamepad.rx || gamepad.ry != lastGamepad.ry)
    {
      Serial.printf("[STICKS] L:(%4d, %4d) | R:(%4d, %4d)\r\n",
                    gamepad.lx, gamepad.ly, gamepad.rx, gamepad.ry);
    }

    // ============================================
    // SPEED TOGGLE
    // Y release: 0.25 -> 0.5 -> 0.75 -> 1 -> 0.25
    // ============================================
    if (justPressed(BTN_R2, gamepad, lastGamepad))
    {
      currentSpeedMultiplier = 1.0f;

      Serial.printf("[SPEED] Multiplier: %.2f\r\n",
                    currentSpeedMultiplier);
    }
    if (justReleased(BTN_R2, gamepad, lastGamepad))
    {
      currentSpeedMultiplier = 0.25f;

      Serial.printf("[SPEED] Multiplier: %.2f\r\n",
                    currentSpeedMultiplier);
    }

    if (justReleased(BTN_L1, gamepad, lastGamepad))
    {
      // ============================================
      // 180-DEGREE QUICK TURN
      // ============================================
      const int TURN_SPEED = 200;  // Spin speed (-255 to 255)
      const int TURN_180_MS = 150; // Milliseconds required to turn 180°

      driveArcade(0, TURN_SPEED); // Spin motors in opposite directions
      delay(TURN_180_MS);         // Hold turn for calibrated time
      driveArcade(0, 0);          // Stop motors
    }

    lastGamepad = gamepad;
  }

  // -------------------------------------------------------------
  // Sumo bot motor / combat logic can run here freely without
  // ever freezing or lagging the USB Host!
  // -------------------------------------------------------------

  // Map Left Stick Y (Forward/Reverse) and Right Stick X (Turning)
  driveArcade(gamepad.ly * currentSpeedMultiplier, gamepad.rx * currentSpeedMultiplier);

  delay(5); // Small yield for Core 0
}

// =================================================================
// CORE 1: Dedicated USB Host Controller (100% Timing-Safe)
// =================================================================
void setup1()
{
  // Configure PIO USB on Core 1
  rp2040_configure_pio_usb();

  // Run USB Host on Controller 1 (Core 1)
  USBHost.begin(1);
}

void loop1()
{
  // Keep USB host alive with zero blocking calls
  USBHost.task();
}

// =================================================================
// TinyUSB Host Callbacks (Run in Core 1 context)
// =================================================================
extern "C"
{

  void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *desc_report, uint16_t desc_len)
  {
    (void)desc_report;
    (void)desc_len;

    if (!tuh_hid_receive_report(dev_addr, instance))
    {
      // Retry if busy
    }
  }

  void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
  {
    (void)dev_addr;
    (void)instance;
    gamepad = GamepadState();
    newGamepadData = true;
  }

  void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *report, uint16_t len)
  {
    GamepadState current;
    if (parseGamepadReport(report, len, current))
    {
      if (current != gamepad)
      {
        gamepad = current;
        newGamepadData = true; // Signal Core 0 to process
      }
    }

    // Immediately request the next report without blocking
    tuh_hid_receive_report(dev_addr, instance);
  }
}