// Host-only mock of LEDC: exercise the actual mixer and MotorDriver together.
#include "../MotionControl.h"
#include "../MotorDriver.h"
#include <stdio.h>
#include <stdlib.h>
static int checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
static void outputs(int m1, int m2, int m3, int m4) {
  const int pins[][2] = {{27,13},{4,2},{17,12},{14,15}};
  const int requested[] = {m1,m2,m3,m4};
  for (int i = 0; i < 4; ++i) {
    const int speed = requested[i] * CarConfig::MOTOR_POLARITY[i];
    CHECK(testDuty[pins[i][0]] == (speed > 0 ? speed : 0));
    CHECK(testDuty[pins[i][1]] == (speed < 0 ? -speed : 0));
  }
}
int main() {
  MotorDriver driver;
  CHECK(driver.begin());
  outputs(0,0,0,0); CHECK(testAttached[32]); CHECK(testDuty[32] == 0);
  driver.setAuxLevel(2);
  const ControlSource sources[] = {ControlSource::WiFi, ControlSource::PS3};
  for (const ControlSource source : sources) {
    MotionControl control(500,100);
    control.receivePS3(0,0,false,false,0);
    if (source == ControlSource::PS3) control.receivePS3(0,0,true,false,1);
    for (int power = 1; power <= 255; ++power) {
      const int turns[] = {-power, power};
      for (const int turn : turns) {
        if (source == ControlSource::PS3) control.receivePS3(0,turn,false,false,2);
        else control.submit(source,0,turn,2,500);
        const MotionState state = control.update(2);
        driver.setWheels(state.left,state.right);
        // Every wheel is powered. Each side has the opposite direction.
        outputs(turn,-turn,turn,-turn);
        CHECK(testDuty[32] == CarConfig::AUX_MOTOR_PWM[2]);
      }
    }
  }
  // Default 80% joystick speed must still drive all four wheels when spinning.
  MotionControl reduced;
  reduced.receivePS3(0,0,false,false,0);
  reduced.receivePS3(0,0,true,false,1);
  const int normalizedTurns[] = {-255,255};
  for (const int turn : normalizedTurns) {
    reduced.receivePS3(255,turn,false,false,2);
    const MotionState state = reduced.update(2);
    driver.setWheels(state.left,state.right);
    const int duty = turn > 0 ? 204 : -204;
    outputs(duty,-duty,duty,-duty);
    CHECK(testDuty[32] == CarConfig::AUX_MOTOR_PWM[2]);
  }
  driver.setWheels(160,160); outputs(160,160,160,160);
  driver.setWheels(-160,-160); outputs(-160,-160,-160,-160);
  driver.stop(); outputs(0,0,0,0); CHECK(testDuty[32] == 0);
  printf("PASS GPIO output test, core %d: %d checks; both sources spin all four wheels in opposite side directions; aux independent\n", TEST_ESP32_MAJOR, checks);
}
