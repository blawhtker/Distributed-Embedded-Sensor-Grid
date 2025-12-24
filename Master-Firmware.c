/**
 * STEPS Master Controller Firmware v2.0
 * MCU: ATmega32U4 (Arduino Leonardo compatible)
 * Function: 
 * 1. Polls 9 I2C tiles for sensor data -> Sends USB HID Keystrokes
 * 2. Listens to PC via USB Serial -> Sends I2C Animation Commands to Tiles
 * 3. Controls NIR LED Driver (PWM)
 */

#include <Wire.h>
#include <Keyboard.h>

// --- Configuration ---
#define NUM_TILES 9
#define NIR_LED_PIN 5      // Report Section 6.4.3: PWM on pin PC6 (Arduino Pin 5)
#define NIR_BRIGHTNESS 127 // 50% Duty Cycle

// Report Table 7.1: Game Arrow Mapping (Q, W, E, A, S, D, Z, X, C)
const char keyMap[9] = {'q', 'w', 'e', 'a', 's', 'd', 'z', 'x', 'c'};

// Track previous state to avoid spamming USB bus
bool wasPressed[9] = {false}; 

void setup() {
  // 1. Initialize USB HID & Serial
  Keyboard.begin();
  Serial.begin(115200); // Fast serial for game communication
  
  // 2. Initialize I2C Bus as Master
  Wire.begin();
  Wire.setClock(400000); // 400kHz Fast Mode for lower latency

  // 3. Setup NIR LED Control
  pinMode(NIR_LED_PIN, OUTPUT);
  analogWrite(NIR_LED_PIN, NIR_BRIGHTNESS);
}

void loop() {
  // --- Task A: Input Polling (Tile -> PC) ---
  for (int id = 1; id <= NUM_TILES; id++) {
    pollTile(id);
  }

  // --- Task B: Output Feedback (PC -> Tile) ---
  // Protocol: PC sends string "T5:P" (Tile 5, Perfect) or "T1:M" (Tile 1, Miss)
  if (Serial.available() > 0) {
    parsePCCommand(); 
  }
  
  // Minimal delay to prevent bus saturation
  delay(2); 
}

// Reads sensor state from a specific tile
void pollTile(int id) {
  Wire.requestFrom(id, 1); // Request 1 byte

  if (Wire.available()) {
    bool isPressed = Wire.read();
    int arrayIndex = id - 1;

    // Detect State Change (Rising/Falling Edge)
    if (isPressed && !wasPressed[arrayIndex]) {
      Keyboard.press(keyMap[arrayIndex]);
      wasPressed[arrayIndex] = true;
    } 
    else if (!isPressed && wasPressed[arrayIndex]) {
      Keyboard.release(keyMap[arrayIndex]);
      wasPressed[arrayIndex] = false;
    }
  }
}

// Sends animation command to a specific tile via I2C
void sendFeedback(int tileID, char command) {
  // Report Section 6.4.2: Master relays commands to Input Tile
  Wire.beginTransmission(tileID);
  Wire.write(command); // 'P'=Perfect, 'G'=Great, 'M'=Miss, 'R'=Reset
  Wire.endTransmission();
}

// Reads USB Serial data from the Game Engine
void parsePCCommand() {
  // Expected Format: "5P" (Tile 5, Perfect) or "9M" (Tile 9, Miss)
  // Simple blocking read for robustness in this demo
  int tileChar = Serial.read(); 
  int cmdChar = Serial.read();

  if (tileChar >= '1' && tileChar <= '9') {
    int targetTile = tileChar - '0'; // Convert ASCII '1' to int 1
    sendFeedback(targetTile, (char)cmdChar);
  }
}