#include <Arduino.h>
#include <Bluepad32.h>
#include "wiring_private.h"
#include <CdiController.h>

#include "avdweb_SAMDtimer.h"

SAMDtimer MouseTimer = SAMDtimer(4, TC_COUNTER_SIZE_16BIT, 3, 831 * 20); // 830 microseconds per bit, 24 bits per command + 1 tick delay

#define JOY_TRESHOLD 50 // used to prevent joystick drift

int8_t joy_devider = 16; // reduces bluepad's big precision int32 to CD-i int8
int8_t btn_speed = 4; // speed of a button press
unsigned long lastSpeedChageMillis = 0;

// CD-i main connection1
#define PIN_RTS 6
#define PIN_RXD 5

// CD-i second connection, when port splitting is supported
#define PIN_RTS_2 3
#define PIN_RXD_2 4

CdiController Cdi1(PIN_RTS, PIN_RXD, MANEUVER, 0);
CdiController Cdi2(PIN_RTS_2, PIN_RXD_2, MANEUVER, 1);

// BT Gamepad
GamepadPtr btGamepad[2] = {nullptr, nullptr};
bool isMouse[2];

// used for mouse parsing
int32_t lastReported[2][2]; // 1st is device 2nd is x 0 y 1
void parseMouse(int i, int32_t *x, int32_t *y, bool *btn_1, bool *btn_2)
{
  if (btGamepad[i]->a())
    *btn_1 = true; // send btn 1
  if (btGamepad[i]->b())
    *btn_2 = true; // sent btn 2


  int32_t reportedX = btGamepad[i]->deltaX();
  int32_t reportedY = btGamepad[i]->deltaY();

  // mouse reports same xy when standing still
  if (reportedX != lastReported[i][0] || reportedY != lastReported[i][1])
  {
    *x = reportedX * 2;
    lastReported[i][0] = reportedX;

    *y = reportedY * 2;
    lastReported[i][1] = reportedY;
  }
}

void parseController(int i, int32_t *x, int32_t *y, bool *btn_1, bool *btn_2)
{
  // Gamepad
  if (btGamepad[i]->x())
    *btn_1 = true; // send button 1

  if (btGamepad[i]->a())
    *btn_2 = true; // send btn 2

  if (btGamepad[i]->b() || btGamepad[i]->y())
  {
    // sent btn 1+2
    *btn_1 = true;
    *btn_2 = true;
  }
  if (btGamepad[i]->dpad() == 0x01)
  {
    // send up
    *y = -1 * btn_speed;
  }
  else if (btGamepad[i]->dpad() == 0x02)
  {
    // send down
    *y = btn_speed;
  }
  else if (btGamepad[i]->dpad() == 0x08)
  {
    // send left
    *x = -1 * btn_speed;
  }
  else if (btGamepad[i]->dpad() == 0x04)
  {
    // send right
    *x = btn_speed;
  }
  else if (btGamepad[i]->dpad() == 0x09)
  {
    // send up left
    *y = -1 * btn_speed;
    *x = -1 * btn_speed;
  }
  else if (btGamepad[i]->dpad() == 0x05)
  {
    // send up right
    *y = -1 * btn_speed;
    *x = btn_speed;
  }
  else if (btGamepad[i]->dpad() == 0x06)
  {
    // send down right
    *y = btn_speed;
    *x = btn_speed;
  }
  else if (btGamepad[i]->dpad() == 0x0a)
  {
    // send down left
    *y = btn_speed;
    *x = -1 * btn_speed;
  }

  // JOY_TRESHOLD prevents drift on controllers
  if (btGamepad[i]->axisX() > JOY_TRESHOLD || btGamepad[i]->axisX() < -1 * JOY_TRESHOLD)
  {
    *x = btGamepad[i]->axisX() / joy_devider;
  }
  if (btGamepad[i]->axisY() > JOY_TRESHOLD || btGamepad[i]->axisY() < -1 * JOY_TRESHOLD)
  {
    *y = btGamepad[i]->axisY() / joy_devider;
  }

  if (btGamepad[i]->axisRX() > JOY_TRESHOLD || btGamepad[i]->axisRX() < -1 * JOY_TRESHOLD)
  {
    *x = btGamepad[i]->axisRX() / joy_devider;
  }
  if (btGamepad[i]->axisRY() > JOY_TRESHOLD || btGamepad[i]->axisRY() < -1 * JOY_TRESHOLD)
  {
    *y = btGamepad[i]->axisRY() / joy_devider;
  }

  if (
  (btGamepad[i]->miscHome() || btGamepad[i]->miscBack()) &&
  (lastSpeedChageMillis + 1000 < millis() ||  millis() - lastSpeedChageMillis < 0) &&
  (btGamepad[i]->l1() || btGamepad[i]->r1() || btGamepad[i]->l2() || btGamepad[i]->r2())
  )
  {
    // change speed on l1 and r1
    if (btGamepad[i]->l1() || btGamepad[i]->l2()) {
      btn_speed = constrain(btn_speed - 1, 1, 20);
      joy_devider = constrain(joy_devider + 4, 1, 50);
    }
    else if (btGamepad[i]->r1() || btGamepad[i]->r2()) {
      btn_speed = constrain(btn_speed + 1, 1, 20);
      joy_devider = constrain(joy_devider - 4, 1, 50);
    }

    Serial.print("Speed: ");
    Serial.println(btn_speed);
    Serial.print("Joy devider: ");
    Serial.println(joy_devider);

    lastSpeedChageMillis = millis();
  }
}

void fetchMouseInterupt()
{
  if (Cdi1.commBusy() || !Cdi1.IsConnected())
  {
    // on busy input interupts will be wasted but it's not a big deal
    return;
  }

  BP32.update();

  for (int i = 0; i < 2; i++)
  {
    bool btn_1 = false, btn_2 = false;
    int32_t x = 0, y = 0;

    // It is safe to always do this before using the gamepad API.
    // This guarantees that the gamepad is valid and connected.
    if (btGamepad[i] != nullptr && btGamepad[i]->isConnected())
    {
      if (isMouse[i])
        parseMouse(i, &x, &y, &btn_1, &btn_2);
      else
        parseController(i, &x, &y, &btn_1, &btn_2);

      if (i == 0)
        Cdi1.JoyInput(x, y, btn_1, btn_2);
      else
        Cdi2.JoyInput(x, y, btn_1, btn_2);
    }
  }
}

void enableDisableScan()
{
  if (btGamepad[0] != nullptr && btGamepad[1] != nullptr && btGamepad[0]->isConnected() && btGamepad[0]->isConnected())
  {
    Serial.println("Both gamepads connected! Stopping scan.");
    BP32.enableNewBluetoothConnections(false);
  }
  else
  {
    Serial.println("Zero or one gamepad connected! Will keep scanning.");
    BP32.enableNewBluetoothConnections(true);
  }
}

// This callback gets called any time a new gamepad is connected.
void onConnectedGamepad(GamepadPtr gp)
{
  Serial.println("CALLBACK: Gamepad is connected! which one?");
 
 ControllerProperties properties = gp->getProperties();
  char buf[80];
  sprintf(buf,
    "BTAddr: %02x:%02x:%02x:%02x:%02x:%02x, VID/PID: %04x:%04x, "
    "flags: 0x%02x, type: %d",
    properties.btaddr[0], properties.btaddr[1], properties.btaddr[2],
    properties.btaddr[3], properties.btaddr[4], properties.btaddr[5],
    properties.vendor_id, properties.product_id, properties.flags, properties.type);

  Serial.println(buf); // logging this to help bluepad devs identify mice
  int i = 0;
  if (btGamepad[0] == nullptr)
  {
    btGamepad[0] = gp;
    Serial.println("CALLBACK: Gamepad 0 is connected!");
  }
  else if (btGamepad[1] == nullptr)
  {
    if (btGamepad[0] && btGamepad[0]->isConnected())
    { 
      bool isSame = true;
      for (int j = 0; j < 6; j++)
      {
        if (gp->getProperties().btaddr[j] != btGamepad[0]->getProperties().btaddr[j]) {
          isSame = false;
          break;
        }
      }
      if (isSame)
      {
        Serial.println("CALLBACK: Gamepad 1 is duplicate!");
        return;
      }
    }
      
    Serial.println("CALLBACK: Gamepad 1 is connected!");
    i = 1;
  }

  btGamepad[i] = gp;
  Serial.println(gp->getClass());
  Serial.println(gp->getModel());
  // check if flag contains mouse as the class is well not reliable
  if (gp->isMouse() || (gp->getProperties().flags & CONTROLLER_PROPERTY_FLAG_MOUSE) == CONTROLLER_PROPERTY_FLAG_MOUSE) 
  {
    isMouse[i] = true;
    Serial.println("Mouse!");
    MouseTimer.attachInterrupt(fetchMouseInterupt);
    MouseTimer.enable(true);
  }
  else
    Serial.println(gp->getProperties().flags);

  enableDisableScan();
}

void onDisconnectedGamepad(GamepadPtr gp)
{
  int i = 0;
  if (btGamepad[0] == gp)
  {
    Serial.println("CALLBACK: Gamepad 0 is disconnected!");
  }
  else if (btGamepad[1] == gp)
  {
    i = 1;
    Serial.println("CALLBACK: Gamepad 1 is disconnected!");
  }

  btGamepad[i] = nullptr;
  isMouse[i] = false;

  if (!isMouse[0] && !isMouse[1])
  {
    // switch back to polling mode for gamepads
    Serial.println("No mice connected! Switching back to polling mode.");
    MouseTimer.enable(false);
  }

  enableDisableScan();
}

unsigned long lastBTConnCheckMillis = 0;
bool led = true;

void loop()
{
  Cdi1.Task();
  Cdi2.Task();

  if (isMouse[0] || isMouse[1])
  {
    // if one is a mouse we use interupts for input
    // so the loop only should be used to command the CD-i input library tasks

    digitalWrite(LED_BUILTIN, 1); // keep LED on to indicate mouse mode
    return;
  }

  if ((!btGamepad[0] || !btGamepad[0]->isConnected()) && (!btGamepad[1] || !btGamepad[1]->isConnected()))
  {
    // if not connected to any BT probe status every 1s
    // we slow this down to give the CD-i bus compute priority
    if (lastBTConnCheckMillis + 1000 < millis())
    {
      BP32.update();
      lastBTConnCheckMillis = millis();

      digitalWrite(LED_BUILTIN, led);
      led = !led;
    }
    return;
  }

  // polling Gamepad
  digitalWrite(LED_BUILTIN, 0); // keep LED off to indicate gamepad mode
  // update BT info
  BP32.update();

  for (int i = 0; i < 2; i++)
  {
    bool btn_1 = false, btn_2 = false;
    int32_t x = 0, y = 0;

    // It is safe to always do this before using the gamepad API.
    // This guarantees that the gamepad is valid and connected.
    if (btGamepad[i] != nullptr && btGamepad[i]->isConnected())
    {
      parseController(i, &x, &y, &btn_1, &btn_2);
      if (i == 0)
        Cdi1.JoyInput(x, y, btn_1, btn_2);
      else
        Cdi2.JoyInput(x, y, btn_1, btn_2);
    }
  }
}

void setup()
{
  // Initialize serial for debugging, it will start later but CD-i output has priority!
  Serial.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.println("Connecting to CD-i");
  Cdi1.Init(); // open serial interface to send data to the CD-i
  Cdi2.Init();

  // for the first 2 seconds only talk to CD-i so we get to send data in time
  while (millis() < 2000)
  {
    Cdi1.Task();
    Cdi2.Task();
  }

  BP32.setup(&onConnectedGamepad, &onDisconnectedGamepad);
  // BP32.forgetBluetoothKeys(); // re-enable if you have paring troubles
  BP32.enableNewBluetoothConnections(true);
}
