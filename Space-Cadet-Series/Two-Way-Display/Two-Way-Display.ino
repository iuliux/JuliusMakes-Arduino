//I2C device found at address 0x3C  - screen 1
//I2C device found at address 0x3D  - screen 2

// App specific imports
#include <Wire.h>
#include <U8g2lib.h>  // https://github.com/olikraus/u8g2

// I2C
#define SCREEN1_ADDRESS 0x3D
#define SCREEN2_ADDRESS 0x3C

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define SIDE_MARGIN 24


U8G2_SSD1306_128X64_NONAME_F_HW_I2C screen1(U8G2_MIRROR, /* reset=*/ U8X8_PIN_NONE);
U8G2_SSD1306_128X64_NONAME_F_HW_I2C screen2(U8G2_MIRROR, /* reset=*/ U8X8_PIN_NONE);


// Application  -------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println("Starting Two-Way Display...");

  delay(100);
  Wire.begin();
  delay(100);

  screen1.setI2CAddress(SCREEN1_ADDRESS * 2);
  screen1.begin();
  screen1.clearDisplay();

  screen2.setI2CAddress(SCREEN2_ADDRESS * 2);
  screen2.begin();
  screen2.clearDisplay();
}

void loop() {
  // Start SCREEN 1 drawing
  screen1.clearBuffer();  // clear the internal memory
  screen1.setFont(u8g2_font_inb49_mf);
  screen1.drawStr(SIDE_MARGIN + 20, SCREEN_HEIGHT - 8, "B");
  screen1.sendBuffer();

  // Start SCREEN 2 drawing
  screen2.clearBuffer();
  screen2.setFont(u8g2_font_inb49_mf);
  screen2.drawStr(SIDE_MARGIN + 20, SCREEN_HEIGHT - 8, "A");
  screen2.sendBuffer();					// transfer internal memory to the display
}
