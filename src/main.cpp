#include <Arduino.h>
#include <Bluepad32.h>
#include "wiring_private.h"
#include <CdiController.h>

#include "avdweb_SAMDtimer.h"

SAMDtimer MouseTimer = SAMDtimer(4, TC_COUNTER_SIZE_16BIT, 3, 830 * 25); // 830 microseconds per bit, 24 bits per command + 1 tick delay

#define JOY_TRESHOLD 50 // used to prevent joystick drift
#define JOY_DEVIDER 16  // reduces bluepad's big precision int32 to CD-i int8
#define BTN_SPEED 5     // standard speed of a button press

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

void fetchMouse()
{
  BP32.update();

  if (Cdi1.commBusy())
  {
    Serial.println("Fuck that was a wasted interupt");
    return;
  }

  for (int i = 0; i < 2; i++)
  {

    bool btn_1 = false, btn_2 = false;
    int x = 0, y = 0;

    // It is safe to always do this before using the gamepad API.
    // This guarantees that the gamepad is valid and connected.
    if (btGamepad[i] != nullptr && btGamepad[i]->isConnected())
    {
      if (isMouse[i])
        parseMouse(i, &x, &y, &btn_1, &btn_2);
      else
        parseController(i, &x, &y, &btn_1, &btn_2);

      bool res = false;

      if (i == 0)
        res = Cdi1.JoyInput(x, y, btn_1, btn_2);
      else
        res = Cdi2.JoyInput(x, y, btn_1, btn_2);

      // store delta of not sent for mice, reset it when data got sent to CD-i
      if (res && isMouse[i])
      {
        deltaX[i] = 0;
        deltaY[i] = 0;
      }
      else if ((x != 0 && y != 0) && isMouse[i]) // CD-i controller library will not sent if x,y is zero (and no buttons pressed), no waste cpu here on stoing 0s
      {
        deltaX[i] = x;
        deltaY[i] = y;
      }
    }
  }
}

void enableDisableScan()
{
  if (btGamepad[0] != nullptr && btGamepad[1] != nullptr)
  {
    Serial.println("Both gamepads connected! Stopping scan.");
    BP32.enableNewBluetoothConnections(false);
  }
  else
  {
    Serial.println("One gamepad connected! Will keep scanning.");
    BP32.enableNewBluetoothConnections(true);
  }
}

// This callback gets called any time a new gamepad is connected.
void onConnectedGamepad(GamepadPtr gp)
{
  Serial.println("CALLBACK: Gamepad is connected! which one?");
  int i = 0;
  if (btGamepad[0] == nullptr)
  {
    btGamepad[0] = gp;
    Serial.println("CALLBACK: Gamepad 0 is connected!");
  }
  else if (btGamepad[1] == nullptr)
  {
    Serial.println("CALLBACK: Gamepad 1 is connected!");
    i = 1;
  }

  GamepadProperties properties = gp->getProperties();
  char buf[80];
  sprintf(buf,
          "BTAddr: %02x:%02x:%02x:%02x:%02x:%02x, VID/PID: %04x:%04x, "
          "flags: 0x%02x",
          properties.btaddr[0], properties.btaddr[1], properties.btaddr[2],
          properties.btaddr[3], properties.btaddr[4], properties.btaddr[5],
          properties.vendor_id, properties.product_id, properties.flags);
  Serial.println(buf); // logging this to help bluepad devs identify mice

  btGamepad[i] = gp;
  if (gp->getProperties().flags & GAMEPAD_PROPERTY_FLAG_MOUSE == GAMEPAD_PROPERTY_FLAG_MOUSE) // check if flag contains mouse
  {
    isMouse[i] = true;
    Serial.println("Mouse!");
    MouseTimer.attachInterrupt(fetchMouse);
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

  enableDisableScan();
}

unsigned long lastBTConnCheckMillis = 0;
bool led = true;

// used for mouse parsing
int deltaX[2];
int deltaY[2];
int32_t lastReported[2][2]; // 1st is device 2nd is x 0 y 1

void parseMouse(int i, int *x, int *y, bool *btn_1, bool *btn_2)
{
  if (btGamepad[i]->a())
    *btn_1 = true; // send btn 1
  if (btGamepad[i]->b())
    *btn_2 = true; // sent btn 2

  int32_t reportedX = btGamepad[i]->axisX();
  int32_t reportedY = btGamepad[i]->axisY();

  // mouse reports same xy when standing still
  if (reportedX != lastReported[i][0] || reportedY != lastReported[i][1])
  {
    *x = reportedX * 2;
    lastReported[i][0] = reportedX;

    *y = reportedY * 2;
    lastReported[i][1] = reportedY;
  }

  // add correction for not sent data + constrain to max CD-i values
  *x = constrain(*x + deltaX[i], -127, 127);
  *y = constrain(*y + deltaY[i], -127, 127);
}

void parseController(int i, int *x, int *y, bool *btn_1, bool *btn_2)
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
    *y = -1 * BTN_SPEED;
  }
  else if (btGamepad[i]->dpad() == 0x02)
  {
    // send down
    *y = BTN_SPEED;
  }
  else if (btGamepad[i]->dpad() == 0x08)
  {
    // send left
    *x = -1 * BTN_SPEED;
  }
  else if (btGamepad[i]->dpad() == 0x04)
  {
    // send right
    *x = BTN_SPEED;
  }
  else if (btGamepad[i]->dpad() == 0x09)
  {
    // send up left
    *y = -1 * BTN_SPEED;
    *x = -1 * BTN_SPEED;
  }
  else if (btGamepad[i]->dpad() == 0x05)
  {
    // send up right
    *y = -1 * BTN_SPEED;
    *x = BTN_SPEED;
  }
  else if (btGamepad[i]->dpad() == 0x06)
  {
    // send down right
    *y = BTN_SPEED;
    *x = BTN_SPEED;
  }
  else if (btGamepad[i]->dpad() == 0x0a)
  {
    // send down left
    *y = BTN_SPEED;
    *x = -1 * BTN_SPEED;
  }

  // JOY_TRESHOLD prevents drift on controllers
  if (btGamepad[i]->axisX() > JOY_TRESHOLD || btGamepad[i]->axisX() < -1 * JOY_TRESHOLD)
  {
    *x = btGamepad[i]->axisX() / JOY_DEVIDER;
  }
  if (btGamepad[i]->axisY() > JOY_TRESHOLD || btGamepad[i]->axisY() < -1 * JOY_TRESHOLD)
  {
    *y = btGamepad[i]->axisY() / JOY_DEVIDER;
  }

  if (btGamepad[i]->axisRX() > JOY_TRESHOLD || btGamepad[i]->axisRX() < -1 * JOY_TRESHOLD)
  {
    *x = btGamepad[i]->axisRX() / JOY_DEVIDER;
  }
  if (btGamepad[i]->axisRY() > JOY_TRESHOLD || btGamepad[i]->axisRY() < -1 * JOY_TRESHOLD)
  {
    *y = btGamepad[i]->axisRY() / JOY_DEVIDER;
  }
}
void loop()
{
  Cdi1.Task();
  Cdi2.Task();

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

  digitalWrite(LED_BUILTIN, 0);

  if (isMouse[0] || isMouse[1])
  {
    return; // there is a mouse TODO HANDLE BOTH DEVICES
  }
  // update BT info
  BP32.update();

  for (int i = 0; i < 2; i++)
  {

    bool btn_1 = false, btn_2 = false;
    int x = 0, y = 0;

    // It is safe to always do this before using the gamepad API.
    // This guarantees that the gamepad is valid and connected.
    if (btGamepad[i] != nullptr && btGamepad[i]->isConnected())
    {
      if (isMouse[i])
        parseMouse(i, &x, &y, &btn_1, &btn_2);
      else
        parseController(i, &x, &y, &btn_1, &btn_2);

      bool res = false;

      if (i == 0)
        res = Cdi1.JoyInput(x, y, btn_1, btn_2);
      else
        res = Cdi2.JoyInput(x, y, btn_1, btn_2);

      // store delta of not sent for mice, reset it when data got sent to CD-i
      if (res && isMouse[i])
      {
        deltaX[i] = 0;
        deltaY[i] = 0;
      }
      else if ((x != 0 && y != 0) && isMouse[i]) // CD-i controller library will not sent if x,y is zero (and no buttons pressed), no waste cpu here on stoing 0s
      {
        deltaX[i] = x;
        deltaY[i] = y;
      }
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
  BP32.forgetBluetoothKeys();
  BP32.enableNewBluetoothConnections(true);
}
