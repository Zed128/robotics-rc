#include "usbh_helper.h"
#include "gamepad.h"

// Shared gamepad state
GamepadState gamepad;
GamepadState lastGamepad;
volatile bool newGamepadData = false;

// Button names for logging
const struct { ButtonID id; const char* name; } BUTTON_LIST[] = {
  { BTN_A, "A" }, { BTN_B, "B" }, { BTN_X, "X" }, { BTN_Y, "Y" },
  { BTN_L1, "L1" }, { BTN_R1, "R1" }, { BTN_L2, "L2" }, { BTN_R2, "R2" },
  { BTN_SELECT, "SELECT" }, { BTN_START, "START" },
  { BTN_L3, "L3" }, { BTN_R3, "R3" },
  { BTN_DPAD_UP, "DPad-UP" }, { BTN_DPAD_DOWN, "DPad-DOWN" },
  { BTN_DPAD_LEFT, "DPad-LEFT" }, { BTN_DPAD_RIGHT, "DPad-RIGHT" }
};

// =================================================================
// CORE 0: Application Logic, Serial Monitor & Motor Control
// =================================================================
void setup() {
  Serial.begin(115200);
  delay(1000); // Allow USB-CDC Serial to initialize

  Serial.println("==========================================");
  Serial.println(" Cytron Motion 2350 Pro - Dual-Core Ready ");
  Serial.println(" Core 0: Application & Robot Logic        ");
  Serial.println(" Core 1: Dedicated USB Host Task          ");
  Serial.println("==========================================");
}

void loop() {
  // Check if Core 1 received new gamepad data
  if (newGamepadData) {
    newGamepadData = false;

    // Check stick movements
    if (gamepad.lx != lastGamepad.lx || gamepad.ly != lastGamepad.ly ||
        gamepad.rx != lastGamepad.rx || gamepad.ry != lastGamepad.ry) {
      Serial.printf("[STICKS] L:(%4d, %4d) | R:(%4d, %4d)\r\n", 
                    gamepad.lx, gamepad.ly, gamepad.rx, gamepad.ry);
    }

    // Check Button Presses & Releases
    for (const auto &btn : BUTTON_LIST) {
      if (justPressed(btn.id, gamepad, lastGamepad)) {
        Serial.printf("  >>> [PRESSED]  %s\r\n", btn.name);
      }
      if (justReleased(btn.id, gamepad, lastGamepad)) {
        Serial.printf("  <<< [RELEASED] %s\r\n", btn.name);
      }
    }

    lastGamepad = gamepad;
  }

  // -------------------------------------------------------------
  // Sumo bot motor / combat logic can run here freely without
  // ever freezing or lagging the USB Host!
  // -------------------------------------------------------------

  
  delay(5); // Small yield for Core 0
}

// =================================================================
// CORE 1: Dedicated USB Host Controller (100% Timing-Safe)
// =================================================================
void setup1() {
  // Configure PIO USB on Core 1
  rp2040_configure_pio_usb();

  // Run USB Host on Controller 1 (Core 1)
  USBHost.begin(1);
}

void loop1() {
  // Keep USB host alive with zero blocking calls
  USBHost.task();
}

// =================================================================
// TinyUSB Host Callbacks (Run in Core 1 context)
// =================================================================
extern "C" {

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *desc_report, uint16_t desc_len) {
  // Log mount + full HID report descriptor. This is the most useful
  // data for writing a parser for a new/unknown gamepad: it tells you
  // exactly how the device lays out its report bytes.
  Serial.printf("[HID MOUNT] dev_addr=%u instance=%u desc_len=%u\r\n", dev_addr, instance, desc_len);
  Serial.printf("[HID DESCRIPTOR] ");
  for (uint16_t i = 0; i < desc_len; i++) {
    Serial.printf("%02X ", desc_report[i]);
  }
  Serial.println();

  if (!tuh_hid_receive_report(dev_addr, instance)) {
    // Retry if busy
  }
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
  Serial.printf("[HID UNMOUNT] dev_addr=%u instance=%u\r\n", dev_addr, instance);
  (void) dev_addr;
  (void) instance;
  gamepad = GamepadState();
  newGamepadData = true;
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *report, uint16_t len) {
  GamepadState current;
  if (parseGamepadReport(report, len, current)) {
    if (current != gamepad) {
      gamepad = current;
      newGamepadData = true; // Signal Core 0 to process
    }
  } else {
    // Unknown / unparseable report. Log the raw bytes so this data can
    // be used later to add support for the device. De-duplicated so a
    // device stuck sending the same unknown payload doesn't flood the
    // serial monitor at its polling rate.
    static uint8_t lastUnknown[64] = {0};
    static uint16_t lastUnknownLen = 0;
    static uint8_t lastUnknownDevAddr = 0;
    static uint8_t lastUnknownInstance = 0;

    uint16_t compareLen = len < sizeof(lastUnknown) ? len : sizeof(lastUnknown);
    bool changed = (len != lastUnknownLen) ||
                   (dev_addr != lastUnknownDevAddr) ||
                   (instance != lastUnknownInstance) ||
                   (memcmp(report, lastUnknown, compareLen) != 0);

    if (changed) {
      Serial.printf("[UNKNOWN REPORT] dev_addr=%u instance=%u len=%u : ", dev_addr, instance, len);
      for (uint16_t i = 0; i < len; i++) {
        Serial.printf("%02X ", report[i]);
      }
      Serial.println();

      memcpy(lastUnknown, report, compareLen);
      lastUnknownLen = len;
      lastUnknownDevAddr = dev_addr;
      lastUnknownInstance = instance;
    }
  }

  // Immediately request the next report without blocking
  tuh_hid_receive_report(dev_addr, instance);
}

} // extern "C"
