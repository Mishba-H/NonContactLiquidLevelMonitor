#include <SoftwareSerial.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>

// --- RS485 & Serial Configuration ---
#define RS485_RX_PIN 4
#define RS485_TX_PIN 5
#define RS485_DE_RE_PIN 3 // Direction control pin
#define RS485_TRANSMIT HIGH
#define RS485_RECEIVE  LOW
SoftwareSerial rs485(RS485_RX_PIN, RS485_TX_PIN);

// --- HC-SR04 Configuration ---
const int TRIG_PIN = 9;
const int ECHO_PIN = 10;
const unsigned long ECHO_TIMEOUT_US = 30000;

// --- DHT11 Configuration ---
#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// --- State Machine ---
enum SensorState {
  IDLE,
  TRIGGER_PULSE_START,
  WAIT_FOR_ECHO_START,
  WAIT_FOR_ECHO_END
};
SensorState currentState = IDLE;

// --- Timing & Data Variables ---
unsigned long stateStartTime = 0;
unsigned long echoStartTime = 0;
float currentTemperature = 20.0;
float currentHumidity = 50.0;

void setup() {
  Serial.begin(9600);
  dht.begin();
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);
  beginSerialCommunication();
}

void loop() {
  listenForCommand();
  runMeasurement();
}

void beginSerialCommunication() {
  rs485.begin(9600);    // RS485 communication speed
  
  pinMode(RS485_DE_RE_PIN, OUTPUT);
  digitalWrite(RS485_DE_RE_PIN, RS485_RECEIVE); // Start in receive mode

  Serial.println("Arduino Sensor Slave Node Initialized. Waiting for commands...");
}

void listenForCommand() {
  if (currentState == IDLE && rs485.available() > 0) {
    char cmd = rs485.read();
    if (cmd == 'M') {
      Serial.println("Received 'Measure' command. Starting process.");
      
      // Read sensors first
      float h = dht.readHumidity();
      float t = dht.readTemperature();

      if (isnan(h) || isnan(t)) {
        Serial.println("Failed to read from DHT. Sending error.");
        sendError("DHT_ERROR");
        return; // Stay in IDLE state
      }
      currentHumidity = h;
      currentTemperature = t;

      // Start the HC-SR04 measurement process
      digitalWrite(TRIG_PIN, HIGH);
      stateStartTime = micros();
      currentState = TRIGGER_PULSE_START;
    }
  }
}

void runMeasurement() {
  unsigned long currentTimeUs = micros();

  switch (currentState) {
    case IDLE: 
      return;
      break;

    case TRIGGER_PULSE_START:
      if (currentTimeUs - stateStartTime >= 10) {
        digitalWrite(TRIG_PIN, LOW);
        stateStartTime = currentTimeUs;
        currentState = WAIT_FOR_ECHO_START;
      }
      break;

    case WAIT_FOR_ECHO_START:
      if (digitalRead(ECHO_PIN) == HIGH) {
        echoStartTime = currentTimeUs;
        currentState = WAIT_FOR_ECHO_END;
      } else if (currentTimeUs - stateStartTime > ECHO_TIMEOUT_US) {
        sendError("TIMEOUT");
        currentState = IDLE; // Abort
      }
      break;

    case WAIT_FOR_ECHO_END:
      if (digitalRead(ECHO_PIN) == LOW) {
        // Calculate the result
        unsigned long echoDuration = currentTimeUs - echoStartTime;
        float speedOfSound = 331.4 + (0.606 * currentTemperature) + (0.0124 * currentHumidity);
        float sound_speed_cm_us = speedOfSound / 10000.0;
        float distanceCm = (echoDuration * sound_speed_cm_us) / 2.0;

        // Now, format and send the response
        sendDistance(distanceCm);
        currentState = IDLE; // Done, return to waiting state
      } else if (currentTimeUs - echoStartTime > ECHO_TIMEOUT_US) {
        sendError("TIMEOUT");
        currentState = IDLE; // Abort
      }
      break;
  }
}

void sendDistance(float dist) {
  String response = "D:" + String(dist, 2) + "\n";
  digitalWrite(RS485_DE_RE_PIN, RS485_TRANSMIT); // Enable transmit
  delay(1); // Allow driver to switch
  rs485.print(response);
  rs485.flush(); // Wait for data to be sent
  delay(1); // Allow last bits to leave
  digitalWrite(RS485_DE_RE_PIN, RS485_RECEIVE);   // Disable transmit
  Serial.print("Sent: " + response);
}

void sendError(String errorCode) {
  String response = "E:" + errorCode + "\n";
  digitalWrite(RS485_DE_RE_PIN, RS485_TRANSMIT);
  delay(1);
  rs485.print(response);
  rs485.flush();
  delay(1);
  digitalWrite(RS485_DE_RE_PIN, RS485_RECEIVE);
  Serial.print("Sent: " + response);
}