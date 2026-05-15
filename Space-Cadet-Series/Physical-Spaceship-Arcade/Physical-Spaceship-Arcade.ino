#include <MD_MAX72xx.h> // https://github.com/MajicDesigns/MD_MAX72XX
#include <SPI.h>

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define DEFAULT_BRIGHTNESS 8 // 0-15
#define MAX_DEVICES 4

#define CLK_PIN   7  // or SCK
#define DATA_PIN  11  // or MOSI
#define CS_PIN    12  // or SS

#define HALL_SENSOR_1 1
#define HALL_SENSOR_2 2

#define WAITING_MIN 5000
#define WAITING_MAX 5500

#define MAX_ASTEROIDS 10
#define TOTAL_COLUMNS (MAX_DEVICES * 8)


MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

enum State {
  INACTIVE,
  WAITING,
  RUNNING,
  DONE
};

State currentState = WAITING;
uint8_t score = 0;   // 0–99
uint8_t highscore = 0;   // 0–99

// For pulsating effect
unsigned long lastPulseUpdate = 0;
int pulsePhase = 0;
const int pulseDuration = 80; // milliseconds per phase

// Asteroid structure
struct Asteroid {
  float col;      // Column position (0-31)
  int row;      // Row position (0-7)
  float speed;  // Speed in columns per frame
  bool active;  // Is this asteroid active
};

Asteroid asteroids[MAX_ASTEROIDS];
unsigned long lastAsteroidUpdate = 0;
unsigned long lastSpawnTime = 0;
const int asteroidUpdateInterval = 50; // milliseconds
const int minSpawnInterval = 800;      // milliseconds
const int maxSpawnInterval = 2000;     // milliseconds
int nextSpawnInterval = 1000;

// Explosion animation
const int EXPLOSION_FRAMES = 20;
const float EXPLOSION_MAX_RADIUS = 6.5;
int explosionFrame = 0;
unsigned long explosionStartTime = 0;
int explosionRow = 0;
const int explosionDuration = 1500; // milliseconds for full explosion

const uint8_t digitFont[10][5] = { // rotated clockwise
  {0b111, 0b101, 0b101, 0b101, 0b111}, // 0
  {0b001, 0b001, 0b001, 0b001, 0b001}, // 1
  {0b111, 0b001, 0b111, 0b100, 0b111}, // 2
  {0b111, 0b001, 0b011, 0b001, 0b111}, // 3
  {0b101, 0b101, 0b111, 0b001, 0b001}, // 4
  {0b111, 0b100, 0b111, 0b001, 0b111}, // 5
  {0b111, 0b100, 0b111, 0b101, 0b111}, // 6
  {0b111, 0b001, 0b001, 0b001, 0b001}, // 7
  {0b111, 0b101, 0b111, 0b101, 0b111}, // 8
  {0b111, 0b101, 0b111, 0b001, 0b111} //  9
};

const int lut[] = {
  3000,
  3950,
  4930,
  5180,
  5340,
  5580,
  6110,
  6600
};

int linearize(int adc) {
  for (int i = 0; i < 7; i++) {
    if (adc < lut[i+1]) {
      return i;
    }
  }
  return 6;
}

void drawPulsatingColumns() {
  // Create pulsating brightness effect
  unsigned long currentTime = millis();
  if (currentTime - lastPulseUpdate >= pulseDuration) {
    pulsePhase = (pulsePhase + 1) % 11; // 0-15 for smooth pulse
    lastPulseUpdate = currentTime;
  }
  
  // Calculate brightness (pulse up and down, range 0-8)
  int brightness = pulsePhase < 5 ? pulsePhase : 10 - pulsePhase;
  
  // Set LED intensity for pulsing effect
  mx.control(MD_MAX72XX::INTENSITY, brightness);
  
  int LAST_COL = MAX_DEVICES * 8 - 1;
  
  // Draw two downward chevrons (pointing right when matrix is horizontal)
  // First chevron (full width, 8 rows)
  mx.setColumn(LAST_COL - 7, 0b10000001); // Top and bottom
  mx.setColumn(LAST_COL - 6, 0b01000010); // 
  mx.setColumn(LAST_COL - 5, 0b00100100); // 
  mx.setColumn(LAST_COL - 4, 0b00011000); // Middle (point)
  
  // Second chevron
  mx.setColumn(LAST_COL - 12, 0b10000001);
  mx.setColumn(LAST_COL - 11, 0b01000010);
  mx.setColumn(LAST_COL - 10, 0b00100100);
  mx.setColumn(LAST_COL -  9, 0b00011000);
  
  // Draw full columns (all 8 LEDs on) at the end
  mx.setColumn(LAST_COL, 0xFF);
  mx.setColumn(LAST_COL - 1, 0xFF);
}

void initAsteroids() {
  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    asteroids[i].active = false;
  }
}

void spawnAsteroid() {
  // Find an inactive asteroid slot
  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    if (!asteroids[i].active) {
      asteroids[i].col = 6; // Start at left edge
      asteroids[i].row = random(0, 8); // Random row
      asteroids[i].speed = random(1, 24) / 10.0; // Speed between 0.1 and 2.4 columns/frame
      asteroids[i].active = true;
      break;
    }
  }
  
  // Set next spawn time randomly
  nextSpawnInterval = random(minSpawnInterval, maxSpawnInterval);
}

void updateAsteroids() {
  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    if (asteroids[i].active) {
      asteroids[i].col += asteroids[i].speed;
      
      // Deactivate if it goes off screen
      if (asteroids[i].col >= TOTAL_COLUMNS) {
        asteroids[i].active = false;
        // Add to score
        if (currentState == RUNNING && score < 99) {
          score++;
        }
      }
    }
  }
}

void drawAsteroids() {
  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    if (asteroids[i].active) {
      int col = (int)asteroids[i].col;
      if (col >= 0 && col < TOTAL_COLUMNS) {
        mx.setPoint(asteroids[i].row, col, 1);
      }
      if (asteroids[i].speed > 1.2) {
        int ceil_col = ceil(asteroids[i].col);
        if (ceil_col != col && ceil_col >= 0 && ceil_col < TOTAL_COLUMNS) {
          mx.setPoint(asteroids[i].row, ceil_col, 1);
        }
      }
    }
  }
}

bool checkCollision(int shipRow) {
  int shipCol1 = MAX_DEVICES * 8 - 1; // last
  int shipCol2 = MAX_DEVICES * 8 - 2; // one before last
  
  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    if (asteroids[i].active) {
      int asteroidCol = (int)asteroids[i].col;
      int asteroidRow = asteroids[i].row;
      
      // Check if asteroid hits the 2x2 ship
      if ((asteroidCol == shipCol1 || asteroidCol == shipCol2) &&
          (asteroidRow == shipRow || asteroidRow == shipRow + 1)) {
        return true;
      }
    }
  }
  return false;
}

void drawExplosion(int centerRow, int frame) {
  int centerCol = TOTAL_COLUMNS - 2; // ship center (between the 2 columns)

  // Normalized progress 0.0 → 1.0
  float t = frame / float(EXPLOSION_FRAMES - 1);

  // Expanding radius with slight ease-out
  float radius = EXPLOSION_MAX_RADIUS * (1.0 - pow(1.0 - t, 2));

  // Core fades after first third
  bool drawCore = frame < EXPLOSION_FRAMES / 3;

  for (int r = 0; r < 8; r++) {
    for (int c = centerCol - 8; c <= centerCol + 8; c++) {
      if (c < 0 || c >= TOTAL_COLUMNS) continue;

      float dx = c - centerCol + 0.5;
      float dy = r - (centerRow + 0.5);
      float dist = sqrt(dx * dx + dy * dy);

      bool on = false;

      // Solid core
      if (drawCore && dist < 1.5) {
        on = true;
      }
      // Main shockwave ring
      else if (abs(dist - radius) < 0.6) {
        on = true;
      }
      // Debris cloud (outer, sparse, noisy)
      else if (dist < radius && dist > radius - 2.0) {
        int debrisChance = 8 + frame; // higher = fewer pixels
        if (((r * 13 + c * 7 + frame * 5) & debrisChance) == 0) {
          on = true;
        }
      }

      if (on) {
        mx.setPoint(r, c, 1);
      }
    }
  }
}

void drawDigit(int digit, int topRow, int leftCol) {
  if (digit < 0 || digit > 9) return;

  // Iterate through the 5 columns of the digit
  for (int col = 0; col < 5; col++) {
    uint8_t colBits = digitFont[digit][col];
    // Iterate through the 3 rows of the digit
    for (int row = 0; row < 3; row++) {
      if (colBits & (1 << (2 - row))) {
        mx.setPoint(topRow + row, leftCol + col, 1);
      }
    }
  }
}

// Draw score vertically at the top-left of the matrix
void drawScore(uint8_t scr) {
  int tens = scr / 10;
  int ones = scr % 10;

  int topRow = 0;    // Start at row 0 (top of matrix)
  int leftCol = 1;   // Start at column 1 (leave 1 column margin)

  if (tens > 0) {
    drawDigit(tens, topRow, leftCol);
  }
  drawDigit(ones, topRow + 4, leftCol); // 3 pixels + 1 spacing = 4 rows down
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();

  mx.begin();
  mx.clear();
  mx.control(MD_MAX72XX::INTENSITY, DEFAULT_BRIGHTNESS); // Set brightness (0-15)
  
  // Configure ADC pins
  analogReadResolution(12); // 12-bit resolution (0-4095)
  pinMode(HALL_SENSOR_1, INPUT);
  pinMode(HALL_SENSOR_2, INPUT);

  // Initialize game
  randomSeed(analogRead(0));
  initAsteroids();
}

void loop() {
  int sensor1Value = analogRead(HALL_SENSOR_1);
  int sensor2Value = analogRead(HALL_SENSOR_2);
  int combinedValue = sensor1Value + sensor2Value;

  unsigned long currentTime = millis();

  // State machine
  switch (currentState) {
    case INACTIVE:
      mx.clear();
      Serial.println("State: INACTIVE");
      break;
      
    case WAITING:
      mx.clear();
      drawPulsatingColumns();
      
      // Check if ship has moved out of waiting zone
      if (combinedValue < WAITING_MIN || combinedValue > WAITING_MAX) {
        initAsteroids(); // Reset game
        lastSpawnTime = currentTime;
        mx.control(MD_MAX72XX::INTENSITY, DEFAULT_BRIGHTNESS); // Reset brightness
        currentState = RUNNING;
        Serial.println("Transition: WAITING -> RUNNING");
      }
      
      Serial.print("State: WAITING | Value: ");
      Serial.println(combinedValue);
      break;
      
    case RUNNING: {
      // Update asteroids
      if (currentTime - lastAsteroidUpdate >= asteroidUpdateInterval) {
        updateAsteroids();
        lastAsteroidUpdate = currentTime;
      }
      
      // Spawn new asteroids
      if (currentTime - lastSpawnTime >= nextSpawnInterval) {
        spawnAsteroid();
        lastSpawnTime = currentTime;
      }

      // Get ship position
      int row = 6 - linearize(combinedValue);

      // Check collision
      if (checkCollision(row)) {
        currentState = DONE;
        explosionStartTime = currentTime;
        explosionFrame = 0;
        explosionRow = row;
        Serial.println("COLLISION! Transition: RUNNING -> DONE");
        break;
      }
      
      // Draw everything
      mx.clear();
      drawScore(score);
      drawAsteroids();
      
      // Display ship position
      mx.setPoint(row, MAX_DEVICES * 8 - 1, 1);
      mx.setPoint(row + 1, MAX_DEVICES * 8 - 1, 1);
      mx.setPoint(row, MAX_DEVICES * 8 - 2, 1);
      mx.setPoint(row + 1, MAX_DEVICES * 8 - 2, 1);
      
      Serial.print("0,6696,"); // 4095
      Serial.print(sensor1Value + sensor2Value);
      Serial.print(",");
      Serial.print(sensor1Value);
      Serial.print(",");
      Serial.println(sensor2Value);
      break;
    }
      
    case DONE:
      // Explosion animation
      unsigned long elapsed = currentTime - explosionStartTime;
      explosionFrame = min(EXPLOSION_FRAMES - 1, (int)(elapsed < 600 ? elapsed / 70 : elapsed / 130));
      
      mx.clear();
      drawScore(score);
      // TODO: Also display highscore at some point
      
      if (explosionFrame < 19) {
        mx.control(MD_MAX72XX::INTENSITY, explosionFrame < 3 ? 15 : DEFAULT_BRIGHTNESS);
        drawExplosion(explosionRow, explosionFrame);
      } else if (explosionFrame < 26) {
        mx.control(MD_MAX72XX::INTENSITY, 25 - explosionFrame); // Reset brightness
        explosionFrame++;
      } else {
        // Display some completion pattern
        mx.clear();
      }

      if (millis() - explosionStartTime > 6000) {
        // Restart the game after a while
        if (score > highscore) {
          highscore = score;
        }
        score = 0;
        currentState = WAITING;
      }
      
      // Serial.println("State: DONE - Game Over!");
      break;
  }
  
  delay(50);
}
