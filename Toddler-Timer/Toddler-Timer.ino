#include <Wire.h>
#include <U8g2lib.h>  // v2.34.22 https://github.com/olikraus/u8g2
#include <ErriezRotaryFullStep.h>  // v1.1.1 https://github.com/Erriez/ErriezRotaryEncoderFullStep
#include <Button2.h>  // v2.3.3 https://github.com/LennartHennigs/Button2
#include <FastLED.h>  // v3.6.0 https://github.com/FastLED/FastLED

// Pins for the Wemos D1 mini
#define ENCODER_BTN D7
#define ENCODER_CLK D5
#define ENCODER_DT D6

#define LEDS_PIN D4
#define NUM_LEDS 6

#define SCREEN_ADDRESS 0x3C
#define SCREEN_WIDTH  128 // OLED screen width, in pixels
#define SCREEN_HEIGHT 32 // OLED screen  height, in pixels

// Disables interrupts while doing X
#define ATOMIC(X) noInterrupts(); X; interrupts();

// SCREEN
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C screen(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// LEDS
CRGB leds[NUM_LEDS];
int lit_leds = 0;

// ENCODER
RotaryFullStep rotary(ENCODER_CLK, ENCODER_DT, true, 255);
Button2 encButton;

volatile int secs = 300;
volatile int total_secs = 300;
unsigned long timer_start = 0;

enum State {
  OFF,
  PAUSE,
  COUNTDOWN
};
volatile int state = State::PAUSE;


ICACHE_RAM_ATTR void encoderInterrupt() {
  if (state != State::PAUSE) {
    return;
  }
  int rotaryState = rotary.read();
  // rotaryState = -3: Counter clockwise turn, multiple notches fast
  // rotaryState = -2: Counter clockwise turn, multiple notches
  // rotaryState = -1: Counter clockwise turn, single notch
  // rotaryState = 0:  No change
  // rotaryState = 1:  Clockwise turn, single notch
  // rotaryState = 2:  Clockwise turn, multiple notches
  // rotaryState = 3:  Clockwise turn, multiple notches fast
  secs += rotaryState * 60;
  if (secs < 0) {
    secs = 0;
  }
  // Reset to minute start
  secs = 60 * (int) (secs / 60);
  total_secs = secs;
}

void onEncoderDblClick(Button2& b) {
  // Start/pause countdown
  ATOMIC(int _state = state);
  if (state == State::COUNTDOWN) {
    ATOMIC(state = State::PAUSE);
    ATOMIC(total_secs = secs);
  } else if (state == State::PAUSE) {
    ATOMIC(state = State::COUNTDOWN);
    timer_start = millis();
  } else {
    // I'm off
    return;
  }
}

void onEncoderLong(Button2& b) {
  // On/off
  ATOMIC(int _state = state);
  if (_state == State::OFF) {
    ATOMIC(state = State::PAUSE);
  } else {
    ATOMIC(state = State::OFF);
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  // --- Encoder ---
  attachInterrupt(digitalPinToInterrupt(ENCODER_CLK), encoderInterrupt, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_DT), encoderInterrupt, CHANGE);
  encButton.begin(ENCODER_BTN);
  encButton.setDoubleClickHandler(onEncoderDblClick);
  encButton.setLongClickDetectedHandler(onEncoderLong);
  encButton.setLongClickTime(2000);

  // --- Screen ---
  screen.setI2CAddress(SCREEN_ADDRESS * 2);
  screen.begin();
  screenRefresh();

  // --- LEDs ---
  pinMode(LEDS_PIN, OUTPUT);
  FastLED.addLeds<NEOPIXEL, LEDS_PIN>(leds, NUM_LEDS);

  FastLED.clear();
  FastLED.show();
}

void loop() {
  encButton.loop();

  ATOMIC(int _state = state);

  if (_state == State::OFF) {
    // Screen OFF
    screen.clearBuffer();
    screen.sendBuffer();
    // Tiles OFF
    FastLED.clear();
    FastLED.show();
    delay(30); // Greatly reduces power consumption
    // 15 millis the loop so far, so it will be idle 2 thirds of the time
    return;
  } else if (_state == State::PAUSE) {
    // Stop countdown, keep everything in place
    // Basically: just don't touch anything
  } else if (_state == State::COUNTDOWN) {
    // Update time
    ATOMIC(int _secs = secs);
    ATOMIC(int _total_secs = total_secs);
    int new_secs = _total_secs - (int) ((millis() - timer_start) / 1000);
    ATOMIC(secs = new_secs);
    // Recompute tiles
    lit_leds = (int) ((max(0, new_secs) / (float) _total_secs) * NUM_LEDS);
    if (max(0, new_secs) % _total_secs != 0) {
      lit_leds += 1;
    }
    // Refresh LEDs
    for(int i = 0; i < NUM_LEDS - lit_leds; i++) {
      leds[i] = CRGB::Black;
    }
    for(int i = NUM_LEDS - lit_leds; i < NUM_LEDS; i++) {
      leds[i] = CRGB::Green;
    }
    FastLED.show();
  }

  screenRefresh();
}

void screenRefresh() {
  // Read volatiles locally
  ATOMIC(int _secs = secs);
  ATOMIC(int _state = state);
  screen.clearBuffer();
  screen.setFont(u8g2_font_maniac_tr);
  screen.setCursor(70, 27);
  int display_min = (int) (_secs / 60);
  if (_secs % 60 != 0) {
    display_min += 1;
  }
  if (_state == State::COUNTDOWN) {
    screen.print(display_min);
    screen.setFont(u8g2_font_streamline_interface_essential_loading_t);
    screen.drawUTF8(20, 26, "\x003d"); // Hourglass
  } else {
    if ((millis() / 400) % 3 < 2) {
      screen.print(display_min);
    }
    screen.setFont(u8g2_font_streamline_interface_essential_cog_t);
    screen.drawUTF8(20, 26, "\x0032"); // Hand and cog
  }
  screen.sendBuffer();
}
