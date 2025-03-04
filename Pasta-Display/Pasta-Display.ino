#include <Button2.h>
#include <AccelStepper.h>

// Specs of the 28BYJ-48 stepper motor
#define MOTORS_STEPS_PER_REVOLUTION 2048
#define MOTORS_MAXSPEED 360

// The pause in millis for the AUTO mode
#define PAUSE_LENGTH 4000

#define BTN_PIN D2

// Define a stepper and the pins it will use
// D5 - IN1
// D6 - IN2
// D7 - IN3
// D1 - IN4
AccelStepper stepper(AccelStepper::FULL4WIRE, D5, D7, D6, D1);
int pos_idx;
int position_coords[] = {
  0,
  (int) (MOTORS_STEPS_PER_REVOLUTION / 2.0),
  // (int) (2 * MOTORS_STEPS_PER_REVOLUTION / 3.0)
};

Button2 btn;

enum BarMode {
  MANUAL,
  AUTO
};
int mode = BarMode::MANUAL;

unsigned long pauseStarted = 0;

void nextPosition() {
  pos_idx = (pos_idx + 1) % 2;
  // Serial.print(pos_idx);
  // Serial.print(": ");
  // Serial.println(position_coords[pos_idx]);
  stepper.moveTo(position_coords[pos_idx]);
}

void onBtnClick(Button2& b) {
  // Go to next position
  if (stepper.distanceToGo() == 0) {
    nextPosition();
  }
}

void switchModes(Button2& b) {
  if (mode == BarMode::MANUAL) {
    mode = BarMode::AUTO;
  } else {
    mode = BarMode::MANUAL;
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial)  // Wait for the serial connection to be establised.
    delay(50);
  Serial.println("Inited");

  stepper.setMaxSpeed(MOTORS_MAXSPEED);
  stepper.setAcceleration(300);

  btn.begin(BTN_PIN);
  btn.setClickHandler(onBtnClick);
  btn.setLongClickHandler(switchModes);
}

void loop() {
  stepper.run();
  btn.loop();

  if (stepper.distanceToGo() == 0) {
    // Idle all stepper coils to not overheat
    stepper.disableOutputs();
    
    if (mode == BarMode::AUTO) {
      if (pauseStarted == 0) {
        pauseStarted = millis();
      } else if (millis() - pauseStarted > PAUSE_LENGTH) {
        nextPosition();
        pauseStarted = 0;
      }
    }
  }
}
