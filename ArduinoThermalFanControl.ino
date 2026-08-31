/*
 * This code controls a fan to cool and a heater to heat in order to maintain thermal control of the cryoscope cryocooler's reject surface.
 * The central computer handles PID and sends commands over Serial to determine the fan percentage (from 0-100) and the heater status (ON or OFF).
 * The Arduino implements a 5 minute watchdog timer to ensure that if communication is lost, the Arduino will soft-reboot and set the fan and heater to safe values (fan off, heater off) until communication is restored.
 */

#include <pwm.h>

#define FAN1_PIN 10
#define FAN2_PIN 11
#define RELAY_HEATER_PIN1 6
#define RELAY_HEATER_PIN2 7
#define RELAY_FAN1_PIN 4
#define RELAY_FAN2_PIN 5
#define RELAY_STARTUP_COMPUTER 3
// 15 minutes timeout
#define WATCHDOG_TIMEOUT 900000UL

// Startup temperature threshold for the computer relay
#define STARTUP_TEMP (273 - 40)

PwmOut fan1Pwm(FAN1_PIN);
PwmOut fan2Pwm(FAN2_PIN);
unsigned long lastCommandTime = 0;
unsigned long lastStartupCheckTime = 0;
// 5 second interval between automatic startup-relay temperature checks
#define STARTUP_CHECK_INTERVAL 5000UL

// Tracked relay/output states, since the PWM and relay drivers can't be read back
int fan1Speed = 0;
int fan2Speed = 0;
bool heaterOn = false;
bool startupRelayOn = false;

#define PT1000_PIN A4
#define PT1000_RREF 1333.0  // Reference resistor value for PT1000 voltage divider with the PT1000 forming the bottom part of the divider
#define PT1000_BETA 3.85  // change in resistance per degree Kelvin
#define PT1000_T0 298.15  // Reference temperature for the PT1000 thermistor (25°C in Kelvin)
#define PT1000_REFVOLTAGE 5.0  // Reference voltage for the PT1000 voltage divider

float getPT1000Temperature() {
  int rawValue = analogRead(PT1000_PIN);
  float voltage = (rawValue / 1023.0) * PT1000_REFVOLTAGE;
  float resistance = (PT1000_RREF * voltage) / (PT1000_REFVOLTAGE - voltage);
  // float temperature = 1.0 / (1.0 / PT1000_T0 + (1.0 / PT1000_BETA) * log(resistance / PT1000_RREF)); //in Kelvin
  float temperature = (resistance - PT1000_RREF) / PT1000_BETA + PT1000_T0; //in Kelvin
  //TODO WIP
  return temperature;
}

void readPT1000() {
  Serial.print(getPT1000Temperature());
}

// Turns on the computer startup relay once the reject surface is warm enough
void updateStartupRelay() {
  float temperature = getPT1000Temperature();
  startupRelayOn = true;//temperature > STARTUP_TEMP;
  digitalWrite(RELAY_STARTUP_COMPUTER, startupRelayOn ? HIGH : LOW);
}

void printStatus() {
  Serial.print("STATUS:TEMP=");
  Serial.print(getPT1000Temperature());
  Serial.print(",HEATER=");
  Serial.print(heaterOn ? "ON" : "OFF");
  Serial.print(",FAN1_PWM=");
  Serial.print(fan1Speed);
  Serial.print(",FAN2_PWM=");
  Serial.print(fan2Speed);
  Serial.print(",STARTUP_RELAY=");
  Serial.println(startupRelayOn ? "ON" : "OFF");
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  fan1Pwm.begin(25000.0f, 0.0f);
  fan2Pwm.begin(25000.0f, 0.0f);
  pinMode(RELAY_HEATER_PIN1, OUTPUT);
  pinMode(RELAY_HEATER_PIN2, OUTPUT);
  pinMode(RELAY_FAN1_PIN, OUTPUT);
  pinMode(RELAY_FAN2_PIN, OUTPUT);
  pinMode(RELAY_STARTUP_COMPUTER, OUTPUT);
  digitalWrite(RELAY_HEATER_PIN1, LOW);
  digitalWrite(RELAY_HEATER_PIN2, LOW);
  digitalWrite(RELAY_FAN1_PIN, LOW);
  digitalWrite(RELAY_FAN2_PIN, LOW);
  digitalWrite(RELAY_STARTUP_COMPUTER, LOW);
  lastCommandTime = millis();
  lastStartupCheckTime = millis();
}

void loop() {
  // Check watchdog timer
  if (millis() - lastCommandTime > WATCHDOG_TIMEOUT) {
    // Watchdog timeout - set safe state and soft reboot
    fan1Pwm.pulse_perc(0);
    fan2Pwm.pulse_perc(0);
    digitalWrite(RELAY_HEATER_PIN1, LOW);
    digitalWrite(RELAY_HEATER_PIN2, LOW);
    digitalWrite(RELAY_FAN1_PIN, LOW);
    digitalWrite(RELAY_FAN2_PIN, LOW);
    Serial.println("WATCHDOG TIMEOUT - Soft reboot");
    delay(100);
    NVIC_SystemReset();
  }

  if (millis() - lastStartupCheckTime > STARTUP_CHECK_INTERVAL) {
    updateStartupRelay();
    lastStartupCheckTime = millis();
  }

  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    lastCommandTime = millis();  // Kick the watchdog
    
    // Parse command format: "FAN:value" or "HEATER:state"
    // Example: "FAN:75" sets fan to 75%, "HEATER:ON" or "HEATER:OFF"
    
    if (command.startsWith("FAN1:")) {
      int fanSpeed = command.substring(5).toInt();
      fanSpeed = constrain(fanSpeed, 0, 100);
      fan1Speed = fanSpeed;
      fan1Pwm.pulse_perc(fanSpeed);
      if (fanSpeed == 0) {
        digitalWrite(RELAY_FAN1_PIN, HIGH);
      } else {
        digitalWrite(RELAY_FAN1_PIN, LOW);
      }
      Serial.print("FAN1 set to ");
      Serial.print(fanSpeed);
      Serial.println("%");
    }
    else if (command.startsWith("FAN2:")) {
      int fanSpeed = command.substring(5).toInt();
      fanSpeed = constrain(fanSpeed, 0, 100);
      fan2Speed = fanSpeed;
      fan2Pwm.pulse_perc(fanSpeed);
      if (fanSpeed == 0) {
        digitalWrite(RELAY_FAN2_PIN, HIGH);
      } else {
        digitalWrite(RELAY_FAN2_PIN, LOW);
      }
      Serial.print("FAN2 set to ");
      Serial.print(fanSpeed);
      Serial.println("%");
    }
    else if (command.startsWith("HEATER:")) {
      String heaterState = command.substring(7);
      if (heaterState == "ON") {
        heaterOn = true;
        digitalWrite(RELAY_HEATER_PIN1, HIGH);
        digitalWrite(RELAY_HEATER_PIN2, HIGH);
        Serial.println("HEATER ON");
      }
      else if (heaterState == "OFF") {
        heaterOn = false;
        digitalWrite(RELAY_HEATER_PIN1, LOW);
        digitalWrite(RELAY_HEATER_PIN2, LOW);
        Serial.println("HEATER OFF");
      }
    }
    else if (command == "STATUS") {
      updateStartupRelay();
      printStatus();
    }
  }
}
