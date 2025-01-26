/* Scribe - Arduino Uno Side
 *
 * By Sam Liu
 * Last updated Dec 5, 2024
 */

#include <SPI.h>
#include <AccelStepper.h>
#include <MultiStepper.h>
#include <Servo.h>

// this allows serial to be "turned off" to improve performance
#define DEBUG true
#if (DEBUG)
  #define DEBUG_SERIAL Serial
#endif

// pin definitions
#define PIN_ARDUINO_READY 3
#define PIN_STEPPER_EN 2
#define PIN_LSTEPPER_STEP 8
#define PIN_LSTEPPER_DIR 9
#define PIN_RSTEPPER_STEP 6
#define PIN_RSTEPPER_DIR 7
#define PIN_SERVO 5

// constants
#define ROBOT_DIAMETER 1500 // in 0.1 mm
#define BOARD_WIDTH 5000 // in 0.1 mm
#define SPOOL_DIAMETER 200 // inner radius (for cables) in 0.1 mm
#define STEPPER_STEPS_PER_REV 200 // steps per 360 degrees of rotation
#define STEPPER_MAX_SPEED 100 // steps per second
#define INTERPOLATION_STEP_SIZE 10 // in 0.1 mm 
#define SERVO_PEN_UP_POS 170
#define SERVO_PEN_DOWN_POS 130

// computed constants
#define ROBOT_RADIUS ROBOT_DIAMETER/2
#define SPOOL_RADIUS SPOOL_DIAMETER/2
#define TAU PI*2

SPIClass *vspi;
AccelStepper lStepper(AccelStepper::DRIVER, PIN_LSTEPPER_STEP, PIN_LSTEPPER_DIR);
AccelStepper rStepper(AccelStepper::DRIVER, PIN_RSTEPPER_STEP, PIN_RSTEPPER_DIR);
MultiStepper steppers;

Servo servo;

byte SPIMsg[2] = {0b00000000, 0b00000000}; // buffer for SPI data
int SPIBytesReciev = 0;
unsigned long SPIPrevRecievTime = 0;

enum States {GOTO_TARGET, TOOL, IDLE, FINISHED};
struct Position {
  int x;
  int y;
};
struct CableLengths {
  double l;
  double r;
};
struct MotorPositions {
  long l;
  long r;
};

Position targetPos = {2000,3750};
Position currentPos = {2000,3750};
short toolID = 0;
States state = IDLE;

long distanceBetween(Position a, Position b){
  return sqrt(sq((long) a.x - b.x) + sq(long(a.y - b.y)));
}

long lengthToSteps(double length) {
  return (long) (STEPPER_STEPS_PER_REV * ((length / SPOOL_RADIUS) / TAU));
}

CableLengths calculateCableLengths(Position p) {
  CableLengths cl = {
    sqrt(sq((long) p.x) + sq((long) p.y - (long) ROBOT_RADIUS)),
    sqrt(sq((long) BOARD_WIDTH - (long) p.x) + sq((long) p.y - (long) ROBOT_RADIUS))
  }; 
  return cl;
}

MotorPositions calculateMotorPos(CableLengths cl) {
  MotorPositions mp = {
    lengthToSteps(cl.l),
    lengthToSteps(cl.r)
  };
  return mp;
}

// goto some position
bool gotoPos(Position p) {
  DEBUG_SERIAL.print("Moving to: ");
  DEBUG_SERIAL.print(p.x);
  DEBUG_SERIAL.print(", ");
  DEBUG_SERIAL.println(p.y);

  CableLengths tcl = calculateCableLengths(p);

  DEBUG_SERIAL.print("Cable lengths: ");
  DEBUG_SERIAL.print(tcl.l);
  DEBUG_SERIAL.print(", ");
  DEBUG_SERIAL.println(tcl.r);

  MotorPositions tmp = calculateMotorPos(tcl);

  DEBUG_SERIAL.print("Motor Positions: ");
  DEBUG_SERIAL.print(tmp.l);
  DEBUG_SERIAL.print(", ");
  DEBUG_SERIAL.println(tmp.r);

  long tmpArr[2] = {tmp.l, tmp.r};
  steppers.moveTo(tmpArr);

  steppers.runSpeedToPosition();

  currentPos = p;

  return true;
}


// go to position in a straight line
bool gotoPosInterpolated(Position p) {
  Position starting = {
    currentPos.x,
    currentPos.y
  };
  long dist = distanceBetween(currentPos, p);
  int distX = p.x - currentPos.x;
  int distY = p.y - currentPos.y;

  long steps = ceil(dist/ (double) INTERPOLATION_STEP_SIZE);
  double stepX = distX / (double) steps;
  double stepY = distY / (double) steps;

  for (long i = 1; i <= steps; i++) {
    Position pos = {
      round(starting.x + stepX * i),
      round(starting.y + stepY * i)
    };
    gotoPos(pos);
  }

  return true;
}

// switch to tool by id
bool switchTool(short id) {
  DEBUG_SERIAL.print("Switching to tool: ");
  DEBUG_SERIAL.println(id+1);

  delay(1000);
  if (id == 7) {
    DEBUG_SERIAL.println("Pen up");
    servo.write(SERVO_PEN_UP_POS);
  } else if (id == 0) {
    DEBUG_SERIAL.println("Pen down");
    servo.write(SERVO_PEN_DOWN_POS);
  }

  return true;
}

// goto target position
bool gotoTarget() {
  return gotoPosInterpolated(targetPos);
}

// switch to currently selected tool
bool switchToCurrentTool() {
  return switchTool(toolID);
}

void recalculateStepperPos() {
  CableLengths cl = calculateCableLengths(currentPos);
  MotorPositions mp = calculateMotorPos(cl);

  lStepper.setCurrentPosition(mp.l);
  rStepper.setCurrentPosition(mp.r);
}

bool parseESPData(byte data[], int ready_pin) {
  // debugging data. note that leading zeros are not printed
  DEBUG_SERIAL.print("Byte 1: ");
  DEBUG_SERIAL.println(SPIMsg[0], BIN);
  DEBUG_SERIAL.print("Byte 2: ");
  DEBUG_SERIAL.println(SPIMsg[1], BIN);

  // parsing of the command
  if (SPIMsg[0] >> 6 == 0b00) { // compare the first 2 bits
    targetPos.x = (SPIMsg[0] & 0b00111111) << 8 | SPIMsg[1]; // take the last 6 + 8 bits
    state = FINISHED;

    DEBUG_SERIAL.print("Target X position set to: ");
    DEBUG_SERIAL.println(targetPos.x);
  } else if (SPIMsg[0] >> 6 == 0b01) {
    targetPos.y = (SPIMsg[0] & 0b00111111) << 8 | SPIMsg[1]; // take the last 6 + 8 bits
    state = FINISHED;

    DEBUG_SERIAL.print("Target Y position set to: ");
    DEBUG_SERIAL.println(targetPos.y);
  } else if (SPIMsg[0] >> 5 == 0b100) { // compare the first 3 bits
    state = GOTO_TARGET;

    DEBUG_SERIAL.println("Starting move to target position");
  } else if (SPIMsg[0] >> 5 == 0b101) { // compare the first 3 bits
    currentPos = targetPos;
    recalculateStepperPos();
    state = FINISHED;

    DEBUG_SERIAL.println("Current position set to target");
  } else if (SPIMsg[0] >> 6 == 0b11) { // compare the first 3 bits
    toolID = (SPIMsg[0] & 0b00111000 ) >> 3; // get tool id
    state = TOOL;

    if (toolID == 0b111) { 
      DEBUG_SERIAL.println("Starting lifting all tools");
    } else {
      DEBUG_SERIAL.print("Starting switching to tool: ");
      DEBUG_SERIAL.println(toolID+1); // tools start counting at 1
    }
  } else {
    DEBUG_SERIAL.println("Invalid command");
    return false;
  }
  return true;
}

void loop() {
  // when a command has started
  if (state != IDLE) {
    DEBUG_SERIAL.println("Running command");
    bool finished = false; // true when the command is done 

    switch (state) {
      case GOTO_TARGET:
        finished = gotoTarget();
        break;
      case TOOL:
        finished = switchToCurrentTool();
        break;
      case FINISHED:
        // delay(100);
        finished = true;
        break;
    }

    if (finished) {
      // command finished
      state = IDLE;
      digitalWrite(PIN_ARDUINO_READY, HIGH);
      DEBUG_SERIAL.println("Command done");
    }
  }
}

void setup() {
  DEBUG_SERIAL.begin(115200); // init serial

  // debugging: ports of SPI
  DEBUG_SERIAL.print("MOSI: ");
  DEBUG_SERIAL.println(MOSI);
  DEBUG_SERIAL.print("MISO: ");
  DEBUG_SERIAL.println(MISO);
  DEBUG_SERIAL.print("SCK: ");
  DEBUG_SERIAL.println(SCK);
  DEBUG_SERIAL.print("SS: ");
  DEBUG_SERIAL.println(SS);

  // init SPI
  SPCR |= _BV(SPE); // set to slave mode
  SPI.attachInterrupt(); // use interrupt so data will always be read

  // init pins
  pinMode(PIN_ARDUINO_READY, OUTPUT);
  digitalWrite(PIN_ARDUINO_READY, HIGH);
  
  // init steppers
  lStepper.setMaxSpeed(STEPPER_MAX_SPEED);
  rStepper.setMaxSpeed(STEPPER_MAX_SPEED);
  // enable pin (only required on one as they share enable pins)
  lStepper.setEnablePin(PIN_STEPPER_EN);
  lStepper.setPinsInverted(false, false, true);
  lStepper.enableOutputs();
  rStepper.setPinsInverted(true, false, true);
  // add to MultiStepper
  steppers.addStepper(lStepper);
  steppers.addStepper(rStepper);

  recalculateStepperPos();

  servo.attach(PIN_SERVO);
  servo.write(SERVO_PEN_UP_POS);
}

ISR (SPI_STC_vect) {
  byte data = SPDR; // make sure that the data is obtained asap

  unsigned long recievTime = millis();
  // if the prev byte is more than 5ms old, discard the prev byte
  if (recievTime - SPIPrevRecievTime > 5) {
    SPIBytesReciev = 0;
  }
  SPIPrevRecievTime = recievTime;

  SPIMsg[SPIBytesReciev] = data;
  SPIBytesReciev++;

  if (state != IDLE) {
    DEBUG_SERIAL.println("Busy");
  } else if (SPIBytesReciev == 2){ // one command contains 2 bytes
    SPIBytesReciev = 0;

    if (parseESPData(SPIMsg, PIN_ARDUINO_READY)) {
      digitalWrite(PIN_ARDUINO_READY, LOW);
    }
  }
}
