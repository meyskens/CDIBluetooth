#include <Arduino.h>
#include <Bluepad32.h>
#include "wiring_private.h"
#include <CdiController.h>

#define JOY_TRESHOLD 30
#define JOY_DEVIDER 16
#define BTN_SPEED 5

// CD-i main connection
#define PIN_RTS 6
#define PIN_RXD 5

// CD-i second connection
#define PIN_RTS_2 3
#define PIN_RXD_2 4

CdiController Cdi1(PIN_RTS, PIN_RXD, MANEUVER, 0);
CdiController Cdi2(PIN_RTS_2, PIN_RXD_2, MANEUVER, 1);

// BT Gamepad
GamepadPtr btGamepad[2] = {nullptr, nullptr};

// This callback gets called any time a new gamepad is connected.
void onConnectedGamepad(GamepadPtr gp)
{
  Serial.println("CALLBACK: Gamepad is connected! which one?");
  if (btGamepad[0] == nullptr)
  {
    btGamepad[0] = gp;
    Serial.println("CALLBACK: Gamepad 0 is connected!");
  }
  else if (btGamepad[1] == nullptr)
  {
    btGamepad[1] = gp;
    Serial.println("CALLBACK: Gamepad 1 is connected!");
  }
  
}

void onDisconnectedGamepad(GamepadPtr gp)
{
  if (btGamepad[0] == gp)
  {
    btGamepad[0] = nullptr;
    Serial.println("CALLBACK: Gamepad 0 is disconnected!");
  }
  else if (btGamepad[1] == gp)
  {
    btGamepad[1] = nullptr;
    Serial.println("CALLBACK: Gamepad 1 is disconnected!");
  }
  
}

unsigned long lastBTConnCheckMillis = 0;
bool led = true;

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
  // update BT info
  BP32.update();

  for (int i = 0; i < 2; i++)
  {

    bool btn_1 = false, btn_2 = false;
    byte x = 0, y = 0;

    // It is safe to always do this before using the gamepad API.
    // This guarantees that the gamepad is valid and connected.
    if (btGamepad[i] != nullptr && btGamepad[i]->isConnected())
    {
      if (btGamepad[i]->x())
      {
        // send button 1
        btn_1 = true;
      }

      if (btGamepad[i]->a())
      {
        // send btn 2
        btn_2 = true;
      }

      if (btGamepad[i]->b() || btGamepad[i]->y())
      {
        // sent btn 1+2
        btn_1 = true;
        btn_2 = true;
      }
      if (btGamepad[i]->dpad() == 0x01)
      {
        // send up
        y = -1 * BTN_SPEED;
      }
      else if (btGamepad[i]->dpad() == 0x02)
      {
        // send dowm
        y = BTN_SPEED;
      }
      else if (btGamepad[i]->dpad() == 0x08)
      {
        // send left
        x = -1 * BTN_SPEED;
      }
      else if (btGamepad[i]->dpad() == 0x04)
      {
        // send right
        x = BTN_SPEED;
      }
      else if (btGamepad[i]->dpad() == 0x09)
      {
        // send up left
        y = -1 * BTN_SPEED;
        x = -1 * BTN_SPEED;
      }
      else if (btGamepad[i]->dpad() == 0x05)
      {
        // send up right
        y = -1 * BTN_SPEED;
        x = BTN_SPEED;
      }
      else if (btGamepad[i]->dpad() == 0x06)
      {
        // send down right
        y = BTN_SPEED;
        x = BTN_SPEED;
      }
      else if (btGamepad[i]->dpad() == 0x0a)
      {
        // send down left
        y = BTN_SPEED;
        x = -1 * BTN_SPEED;
      }

      if (btGamepad[i]->axisX() > JOY_TRESHOLD || btGamepad[i]->axisX() < -1 * JOY_TRESHOLD)
      {
        x = btGamepad[i]->axisX() / JOY_DEVIDER;
      }
      if (btGamepad[i]->axisY() > JOY_TRESHOLD || btGamepad[i]->axisY() < -1 * JOY_TRESHOLD)
      {
        y = btGamepad[i]->axisY() / JOY_DEVIDER;
      }

      if (btGamepad[i]->axisRX() > JOY_TRESHOLD || btGamepad[i]->axisRX() < -1 * JOY_TRESHOLD)
      {
        x = btGamepad[i]->axisRX() / JOY_DEVIDER;
      }
      if (btGamepad[i]->axisRY() > JOY_TRESHOLD || btGamepad[i]->axisRY() < -1 * JOY_TRESHOLD)
      {
        y = btGamepad[i]->axisRY() / JOY_DEVIDER;
      }

      if (i == 0)
      {
        Cdi1.JoyInput(x, y, btn_1, btn_2);
      }
      else
      {
        Cdi2.JoyInput(x, y, btn_1, btn_2);
      }
    }
    
  }
}

void setup()
{
  // Initialize serial
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
}
