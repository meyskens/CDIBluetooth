#include <Arduino.h>
#include <Bluepad32.h>
#include "wiring_private.h"
#include <CdiController.h>


#define JOY_TRESHOLD 30
#define JOY_DEVIDER 16
#define BTN_SPEED 5

// CD-i connection
#define PIN_RTS 6
#define PIN_RXD 5
CdiController Cdi(PIN_RTS, PIN_RXD, MANEUVER);

// BT Gamepad
GamepadPtr myGamepad;

// This callback gets called any time a new gamepad is connected.
void onConnectedGamepad(GamepadPtr gp) {
  // In this example we only use one gamepad at the same time.
  myGamepad = gp;
  Serial.println("CALLBACK: Gamepad is connected!");
}

void onDisconnectedGamepad(GamepadPtr gp) {
  Serial.println("CALLBACK: Gamepad is disconnected!");
  myGamepad = nullptr;
}

unsigned long lastBTConnCheckMillis = 0;
bool led = true;

void loop() {  
  Cdi.Task();

  if (!myGamepad || !myGamepad->isConnected()) {
    // if not connected probe status every 1s
    // we slow this down to give the CD-i bus compute priority
    if (lastBTConnCheckMillis+1000 < millis()) {
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

  bool btn_1 = false, btn_2 = false;
  byte x = 0, y = 0;

  // It is safe to always do this before using the gamepad API.
  // This guarantees that the gamepad is valid and connected.
  if (myGamepad && myGamepad->isConnected()) {
    if (myGamepad->x()) {
      // send button 1
      btn_1 = true;
    }

    if (myGamepad->a()) {
     // send btn 2
     btn_2 = true;
    }

    if (myGamepad->b() || myGamepad->y()) {
      // sent btn 1+2
      btn_1 = true;
      btn_2 = true;
    }
    if (myGamepad->dpad() == 0x01) {
      // send up
      y = -1* BTN_SPEED;
    } else if (myGamepad->dpad() == 0x02) {
      // send dowm
      y = BTN_SPEED;
    } else if (myGamepad->dpad() == 0x08) {
      // send left
      x = -1* BTN_SPEED;
    } else if (myGamepad->dpad() == 0x04) {
        // send right 
      x = BTN_SPEED;
    } else if (myGamepad->dpad() == 0x09) {
      // send up left
      y = -1* BTN_SPEED;
      x = -1* BTN_SPEED; 
    } else if (myGamepad->dpad() == 0x05) {
      // send up right
      y = -1* BTN_SPEED;
      x = BTN_SPEED;
    } else if (myGamepad->dpad() == 0x06) {
      // send down right
      y = BTN_SPEED;
      x = BTN_SPEED;
    } else if (myGamepad->dpad() == 0x0a) {
      // send down left
      y = BTN_SPEED;
      x = -1 * BTN_SPEED;
    }

    if (myGamepad->axisX() > JOY_TRESHOLD || myGamepad->axisX() < -1*JOY_TRESHOLD) {
      x = myGamepad->axisX()/JOY_DEVIDER;
    }
    if (myGamepad->axisY() > JOY_TRESHOLD || myGamepad->axisY() < -1*JOY_TRESHOLD) {
      y = myGamepad->axisY()/JOY_DEVIDER;
    }

    if (myGamepad->axisRX() > JOY_TRESHOLD || myGamepad->axisRX() < -1*JOY_TRESHOLD) {
      x = myGamepad->axisRX()/JOY_DEVIDER;
    }
    if (myGamepad->axisRY() > JOY_TRESHOLD || myGamepad->axisRY() < -1*JOY_TRESHOLD) {
      y = myGamepad->axisRY()/JOY_DEVIDER;
    }
  }

  
  Cdi.JoyInput(x, y, btn_1, btn_2);
}

void setup() {
  // Initialize serial
  Serial.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.println("Connecting to CD-i");
  Cdi.Init(); // open serial interface to send data to the CD-i
  

  // for the first 2 seconds only talk to CD-i so we get to send data in time
  while (millis() < 2000) {
     Cdi.Task(); 
  }

  BP32.setup(&onConnectedGamepad, &onDisconnectedGamepad);
  BP32.forgetBluetoothKeys();
}
