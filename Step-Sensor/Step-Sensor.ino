#include <FastLED.h>
FASTLED_USING_NAMESPACE

#define LED_PIN   D1
#define RELAY_PIN D2

#define NUM_LEDS 36

int sensor_val;
const int SENSOR_THRESH = 150;

unsigned long step_start = 0;

CRGB leds[NUM_LEDS];
CRGB::HTMLColorCode colors[] = {CRGB::Red, CRGB::Blue, CRGB::White};
int curr_color = 0;
int sec = 1;

void relayTrigger() {
    // Blocking version
    digitalWrite(RELAY_PIN, HIGH);
    delay(20);
    digitalWrite(RELAY_PIN, LOW);
}

void setup() {
  Serial.begin(115200);
  while (!Serial)  // Wait for the serial connection to be establised.
    delay(50);
  Serial.println();

  LEDS.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  LEDS.setBrightness(255);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
}

void loop() {
  // delay(2);
  sensor_val = analogRead(A0);

  // Plotting for debugging
  // delay(5);
  // Serial.print("90,300,");
  // Serial.println(sensor_val);

  if (sensor_val > SENSOR_THRESH) {
    if (step_start == 0) {
      step_start = millis();
      relayTrigger();
    }
    EVERY_N_MILLISECONDS(20) {
      pacifica_loop();
      FastLED.show();
    }
  } else {
    if (step_start > 0) {
      relayTrigger();
      step_start = 0;
    }
    FastLED.clear();
    FastLED.show();
  }
}

//////////////////////////////////////////////////////////////////////////
//
// In this animation, there are four "layers" of waves of light.  
//
// Each layer moves independently, and each is scaled separately.
//
// All four wave layers are added together on top of each other, and then 
// another filter is applied that adds "whitecaps" of brightness where the 
// waves line up with each other more.  Finally, another pass is taken
// over the led array to 'deepen' (dim) the blues and greens.
//
// The speed and scale and motion each layer varies slowly within independent 
// hand-chosen ranges, which is why the code has a lot of low-speed 'beatsin8' functions
// with a lot of oddly specific numeric ranges.
//
// These three custom blue-green color palettes were inspired by the colors found in
// the waters off the southern coast of California, https://goo.gl/maps/QQgd97jjHesHZVxQ7
//

// Gradient palette "YlOrRd_09_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/cb/seq/tn/YlOrRd_09.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 72 bytes of program space.
DEFINE_GRADIENT_PALETTE( YlOrRd_09_gp ) {
    0, 255,255,145,
   28, 255,255,145,
   28, 255,217, 79,
   56, 255,217, 79,
   56, 252,178, 37,
   84, 252,178, 37,
   84, 252,115, 12,
  113, 252,115, 12,
  113, 249, 69,  6,
  141, 249, 69,  6,
  141, 247, 18,  2,
  170, 247, 18,  2,
  170, 188,  1,  1,
  198, 188,  1,  1,
  198, 117,  0,  2,
  226, 117,  0,  2,
  226,  42,  0,  2,
  255,  42,  0,  2};
CRGBPalette16 pacifica_palette_1 = YlOrRd_09_gp;
CRGBPalette16 pacifica_palette_2 = YlOrRd_09_gp;
CRGBPalette16 pacifica_palette_3 = YlOrRd_09_gp;


void pacifica_loop()
{
  // Increment the four "color index start" counters, one for each wave layer.
  // Each is incremented at a different speed, and the speeds vary over time.
  static uint16_t sCIStart1, sCIStart2, sCIStart3, sCIStart4;
  static uint32_t sLastms = 0;
  uint32_t ms = GET_MILLIS();
  uint32_t deltams = ms - sLastms;
  sLastms = ms;
  uint16_t speedfactor1 = beatsin16(3, 179, 269);
  uint16_t speedfactor2 = beatsin16(4, 179, 269);
  uint32_t deltams1 = (deltams * speedfactor1) / 256;
  uint32_t deltams2 = (deltams * speedfactor2) / 256;
  uint32_t deltams21 = (deltams1 + deltams2) / 2;
  sCIStart1 += (deltams1 * beatsin88(1011,10,13));
  sCIStart2 -= (deltams21 * beatsin88(777,8,11));
  sCIStart3 -= (deltams1 * beatsin88(501,5,7));
  sCIStart4 -= (deltams2 * beatsin88(257,4,6));

  // Clear out the LED array to a dim background blue-green
//  fill_solid( leds, NUM_LEDS, CRGB( 20, 60, 100));
  fill_solid( leds, NUM_LEDS, CRGB( 160, 0, 0));

  // Render each of four layers, with different scales and speeds, that vary over time
  pacifica_one_layer( pacifica_palette_1, sCIStart1, beatsin16( 3, 11 * 256, 14 * 256), beatsin8( 10, 70, 130), 0-beat16( 301) );
  pacifica_one_layer( pacifica_palette_2, sCIStart2, beatsin16( 4,  6 * 256,  9 * 256), beatsin8( 17, 40,  80), beat16( 401) );
  pacifica_one_layer( pacifica_palette_3, sCIStart3, 6 * 256, beatsin8( 9, 10,38), 0-beat16(503));
  pacifica_one_layer( pacifica_palette_3, sCIStart4, 5 * 256, beatsin8( 8, 10,28), beat16(601));

  // Add brighter 'whitecaps' where the waves lines up more
//  pacifica_add_whitecaps();

  // Deepen the blues and greens a bit
  pacifica_deepen_colors();
}

// Add one layer of waves into the led array
void pacifica_one_layer( CRGBPalette16& p, uint16_t cistart, uint16_t wavescale, uint8_t bri, uint16_t ioff)
{
  uint16_t ci = cistart;
  uint16_t waveangle = ioff;
  uint16_t wavescale_half = (wavescale / 2) + 20;
  for( uint16_t i = 0; i < NUM_LEDS; i++) {
    waveangle += 250;
    uint16_t s16 = sin16( waveangle ) + 32768;
    uint16_t cs = scale16( s16 , wavescale_half ) + wavescale_half;
    ci += cs;
    uint16_t sindex16 = sin16( ci) + 32768;
    uint8_t sindex8 = scale16( sindex16, 240);
    CRGB c = ColorFromPalette( p, sindex8, bri, LINEARBLEND);
    leds[i] += c;
  }
}

// Add extra 'white' to areas where the four layers of light have lined up brightly
void pacifica_add_whitecaps()
{
  uint8_t basethreshold = beatsin8( 9, 55, 65);
  uint8_t wave = beat8( 7 );
  
  for( uint16_t i = 0; i < NUM_LEDS; i++) {
    uint8_t threshold = scale8( sin8( wave), 20) + basethreshold;
    wave += 7;
    uint8_t l = leds[i].getAverageLight();
    if( l > threshold) {
      uint8_t overage = l - threshold;
      uint8_t overage2 = qadd8( overage, overage);
      leds[i] += CRGB( qadd8( overage2, overage2), overage2, overage );
    }
  }
}

// Deepen the blues and greens
void pacifica_deepen_colors()
{
  for( uint16_t i = 0; i < NUM_LEDS; i++) {
    leds[i].green = scale8( leds[i].green,  100);
    leds[i].blue = scale8( leds[i].blue,  30);
    leds[i].red= scale8( leds[i].red, 200); 
    leds[i] |= CRGB( 2, 5, 7);
  }
}
