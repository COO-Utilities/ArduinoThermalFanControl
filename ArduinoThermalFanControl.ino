/*
 * This code controls a fan to cool and a heater to heat in order to maintain thermal control of the cryoscope cryocooler's reject surface.
 * The central computer handles PID and sends commands over Serial to determine the fan percentage (from 0-100) and the heater status (ON or OFF).
 * The Arduino implements a 5 minute watchdog timer to ensure that if communication is lost, the Arduino will soft-reboot and set the fan and heater to safe values (fan off, heater off) until communication is restored.
 */

#include <pwm.h>

#define FAN_PIN 10
#define RELAY_HEATER_PIN1 6
#define RELAY_HEATER_PIN2 7
#define WATCHDOG_TIMEOUT 900000UL  // 15 minutes in milliseconds

PwmOut fanPwm(FAN_PIN);
unsigned long lastCommandTime = 0;


void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  fanPwm.begin(25000.0f, 0.0f);
  pinMode(RELAY_HEATER_PIN1, OUTPUT);
  pinMode(RELAY_HEATER_PIN2, OUTPUT);
  digitalWrite(RELAY_HEATER_PIN1, LOW);
  digitalWrite(RELAY_HEATER_PIN2, LOW);
  lastCommandTime = millis();
}

void loop() {
  // Check watchdog timer
  if (millis() - lastCommandTime > WATCHDOG_TIMEOUT) {
    // Watchdog timeout - set safe state and soft reboot
    fanPwm.pulse_perc(0);
    digitalWrite(RELAY_HEATER_PIN1, LOW);
    digitalWrite(RELAY_HEATER_PIN2, LOW);
    Serial.println("WATCHDOG TIMEOUT - Soft reboot");
    delay(100);
    NVIC_SystemReset();
  }
  
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    lastCommandTime = millis();  // Kick the watchdog
    
    // Parse command format: "FAN:value" or "HEATER:state"
    // Example: "FAN:75" sets fan to 75%, "HEATER:ON" or "HEATER:OFF"
    
    if (command.startsWith("FAN:")) {
      int fanSpeed = command.substring(4).toInt();
      fanSpeed = constrain(fanSpeed, 0, 100);
      fanPwm.pulse_perc(fanSpeed);
      Serial.print("FAN set to ");
      Serial.print(fanSpeed);
      Serial.println("%");
    }
    else if (command.startsWith("HEATER:")) {
      String heaterState = command.substring(7);
      if (heaterState == "ON") {
        digitalWrite(RELAY_HEATER_PIN1, HIGH);
        digitalWrite(RELAY_HEATER_PIN2, HIGH);
        Serial.println("HEATER ON");
      }
      else if (heaterState == "OFF") {
        digitalWrite(RELAY_HEATER_PIN1, LOW);
        digitalWrite(RELAY_HEATER_PIN2, LOW);
        Serial.println("HEATER OFF");
      }
    }
  }
}
