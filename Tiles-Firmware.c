/**
 * STEPS Input Tile Firmware (Enhanced)
 * MCU: ATmega328PB
 * Features: FSR Polling, I2C Communication, Custom 3x3 LED Animations
 */

#include <Wire.h>
#include <FastLED.h>

// --- Hardware Configuration ---
#define TILE_ADDRESS 1        // CHANGE THIS for each tile (1-9)
#define NUM_LEDS 9            // 3x3 Grid
#define LED_PIN 4             // PD4 on ATmega328PB maps to Digital Pin 4
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
#define BRIGHTNESS 150        // 0-255

// --- Sensor Configuration ---
const int fsrPins[4] = {A0, A1, A2, A3};
const int PRESSURE_THRESHOLD = 200;

// --- State Definitions ---
enum TileState {
  STATE_IDLE,     // Rainbow swirl
  STATE_PRESSED,  // White (Local feedback when stepping)
  STATE_PERFECT,  // Green Flash (Game command)
  STATE_GREAT,    // Blue Flash (Game command)
  STATE_MISS      // Red Fade (Game command)
};

// Global Variables
CRGB leds[NUM_LEDS];
volatile bool isStepped = false;
volatile TileState currentState = STATE_IDLE;
unsigned long stateStartTime = 0;

void setup() {
  // 1. Initialize I2C
  Wire.begin(TILE_ADDRESS);
  Wire.onRequest(requestEvent);
  Wire.onReceive(receiveEvent); // Listen for commands from Master

  // 2. Initialize Sensors
  for (int i = 0; i < 4; i++) pinMode(fsrPins[i], INPUT);

  // 3. Initialize LEDs
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
}

void loop() {
  // --- 1. Read Sensors ---
  int totalPressure = 0;
  for (int i = 0; i < 4; i++) totalPressure += analogRead(fsrPins[i]);

  bool currentStep = (totalPressure > PRESSURE_THRESHOLD);

  // Logic: Only override state to PRESSED if we aren't currently showing a game animation
  // (like a "Perfect" flash), OR if the game animation has finished (e.g. > 500ms ago)
  if (currentStep && (currentState == STATE_IDLE || millis() - stateStartTime > 500)) {
    if (!isStepped) { // Rising edge
      setAnimation(STATE_PRESSED);
    }
    isStepped = true;
  } else if (!currentStep && isStepped) {
    isStepped = false;
    if (currentState == STATE_PRESSED) setAnimation(STATE_IDLE); 
  }

  // --- 2. Update Animations ---
  runAnimation();
  FastLED.show();
  
  // High refresh rate for smooth visuals
  FastLED.delay(16); // ~60 FPS
}

// --- I2C Handlers ---

// Master asks for data: Send 1 if pressed, 0 if not
void requestEvent() {
  Wire.write(isStepped ? 1 : 0);
}

// Master sends a command (e.g., Game Scoring Event)
void receiveEvent(int howMany) {
  if (Wire.available()) {
    char command = Wire.read();
    
    switch (command) {
      case 'P': setAnimation(STATE_PERFECT); break; // 'P' for Perfect
      case 'G': setAnimation(STATE_GREAT);   break; // 'G' for Great
      case 'M': setAnimation(STATE_MISS);    break; // 'M' for Miss
      case 'R': setAnimation(STATE_IDLE);    break; // 'R' for Reset
    }
  }
}

// --- Animation Engine ---

void setAnimation(TileState newState) {
  currentState = newState;
  stateStartTime = millis();
}

void runAnimation() {
  unsigned long elapsed = millis() - stateStartTime;

  switch (currentState) {
    case STATE_IDLE:
      // Gentle Rainbow Cycle
      fill_rainbow(leds, NUM_LEDS, millis() / 20, 7); 
      break;

    case STATE_PRESSED:
      // Bright White Flash (Instant feedback)
      fill_solid(leds, NUM_LEDS, CRGB::White);
      break;

    case STATE_PERFECT:
      // Green Ripple / Flash
      if (elapsed < 300) {
        fill_solid(leds, NUM_LEDS, CRGB::Green);
      } else {
        // Fade back to idle automatically
        setAnimation(STATE_IDLE);
      }
      break;

    case STATE_GREAT:
      // Blue Flash
      if (elapsed < 300) {
        fill_solid(leds, NUM_LEDS, CRGB::Blue);
      } else {
        setAnimation(STATE_IDLE);
      }
      break;

    case STATE_MISS:
      // Red Decay
      if (elapsed < 500) {
        // Fade from Red to Black
        uint8_t brightness = map(elapsed, 0, 500, 255, 0);
        fill_solid(leds, NUM_LEDS, CRGB(brightness, 0, 0));
      } else {
        setAnimation(STATE_IDLE);
      }
      break;
  }
}