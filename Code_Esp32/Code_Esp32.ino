#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <math.h>

#include "Tank.h"
#include "Esp32WebServer.h"

// === Pin Definitions ===
#define I2C_SDA 21
#define I2C_SCL 22
#define MODE_BUTTON_PIN 15

// === RS485 Communication ===
#define RS485_SERIAL Serial2
#define RS485_DE_RE_PIN 4
#define RS485_TRANSMIT HIGH
#define RS485_RECEIVE LOW

// === Device State ===
enum DeviceMode {
  Monitor,
  Configure
};
DeviceMode currentMode = Monitor;

// --- Global Objects ---
Tank tank;
// Note: Pointers to these global variables are passed to the web server
unsigned long measurementInterval;
unsigned long responseTimeout;
Esp32WebServer webServer(&tank, &measurementInterval, &responseTimeout);
LiquidCrystal_I2C lcd(0x27, 16, 2);


// --- System Parameters ---
unsigned long lastRequestTime = 0;

// --- Button Handling ---
volatile bool modeChangeRequested = false;
unsigned long buttonPressTime = 0;
bool longPressTriggered = false;
const unsigned long LONG_PRESS_DURATION = 5000; // 5 seconds

// --- Forward Declarations ---
void switchMode(DeviceMode newMode);
void loadSystemConfig();
void resetAllConfigsToDefault();
void onButtonPress();

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n--- Liquid Level Monitor Initializing ---");

  // Initialize I2C for LCD
  Wire.begin(I2C_SDA, I2C_SCL);

  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");

  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("FATAL: SPIFFS Mount Failed");
    lcd.setCursor(0, 1);
    lcd.print("SPIFFS Error!");
    while (true) delay(100);
  }

  // Initialize RS485 Communication
  pinMode(RS485_DE_RE_PIN, OUTPUT);
  digitalWrite(RS485_DE_RE_PIN, RS485_RECEIVE);
  RS485_SERIAL.begin(9600, SERIAL_8N1, 16, 17);

  // Initialize Button
  pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(MODE_BUTTON_PIN), onButtonPress, FALLING);

  // Load all configurations from SPIFFS
  loadSystemConfig();
  tank.begin(); // Uses default filename
  webServer.begin(); // Uses default filename

  // Start in Monitor mode
  switchMode(Monitor);
}

void loop() {
  handleButton();

  if (currentMode == Monitor) {
    // In Monitor mode, request measurements and update LCD
    requestMeasurement();
  } else {
    // In Configure mode, the async web server runs in the background.
    // We can clean up disconnected web socket clients.
    webServer.cleanupClients();
  }
}

void loadSystemConfig() {
  File configFile = SPIFFS.open("/config/system_params.json", "r");
  if (!configFile) {
    Serial.println("Failed to open system_params.json. Using defaults.");
    measurementInterval = 2000;
    responseTimeout = 500;
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, configFile);
  configFile.close();

  if (error) {
    Serial.print("deserializeJson() for system params failed: ");
    Serial.println(error.c_str());
    return;
  }

  measurementInterval = doc["measurementInterval"] | 2000;
  responseTimeout = doc["responseTimeout"] | 500;

  Serial.println("System parameters loaded successfully.");
}


// --- Mode and State Management ---

void switchMode(DeviceMode newMode) {
  currentMode = newMode;
  lcd.clear();
  lcd.setCursor(0, 0);

  if (newMode == Monitor) {
    Serial.println("Switching to MONITOR mode.");
    lcd.print("Mode: Monitor");
    webServer.disable();
    lcd.backlight();
  } else {
    Serial.println("Switching to CONFIGURE mode.");
    lcd.print("Mode: Configure");
    webServer.enable(); // Starts AP and Web Server
    lcd.setCursor(0, 1);
    lcd.print("IP: " + WiFi.softAPIP().toString());
    lcd.backlight();
  }
}

// --- Button Handling Logic ---
void IRAM_ATTR onButtonPress() {
  // Simple flag set in ISR, handled in the main loop
  modeChangeRequested = true;
}

void handleButton() {
  // Check if button is currently pressed
  if (digitalRead(MODE_BUTTON_PIN) == LOW) {
    if (buttonPressTime == 0) {
      // Button was just pressed
      buttonPressTime = millis();
    } else if (!longPressTriggered && (millis() - buttonPressTime > LONG_PRESS_DURATION)) {
      // Long press detected
      longPressTriggered = true;
      Serial.println("Long press detected! Resetting configurations...");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Resetting Config");
      lcd.setCursor(0, 1);
      lcd.print("Rebooting...");
      resetAllConfigsToDefault();
      delay(2000);
      ESP.restart();
    }
  } else { // Button is released
    if (modeChangeRequested && !longPressTriggered) {
      // Short press detected (via interrupt flag)
      Serial.println("Short press detected! Toggling mode.");
      switchMode(currentMode == Monitor ? Configure : Monitor);
    }
    // Reset flags once button is released
    buttonPressTime = 0;
    longPressTriggered = false;
    modeChangeRequested = false;
  }
}

void resetAllConfigsToDefault() {
  // --- Create default network_params.json ---
  File networkFile = SPIFFS.open("/config/network_params.json", "w");
  JsonDocument netDoc;
  netDoc["ssid"] = "Liquid_Level_Monitor";
  netDoc["password"] = "12345678";
  serializeJson(netDoc, networkFile);
  networkFile.close();

  // --- Create default system_params.json ---
  File systemFile = SPIFFS.open("/config/system_params.json", "w");
  JsonDocument sysDoc;
  sysDoc["measurementInterval"] = 2000;
  sysDoc["responseTimeout"] = 500;
  serializeJson(sysDoc, systemFile);
  systemFile.close();

  // --- Create default tank_params.json ---
  File tankFile = SPIFFS.open("/config/tank_params.json", "w");
  JsonDocument tankDoc;
  tankDoc["tankCrossSectionAreaCm2"] = 500.0;
  tankDoc["tankDepthCm"] = 100.0;
  tankDoc["sensorOffsetCm"] = 5.0;
  tankDoc["heightErrorMarginCm"] = 2.0;
  serializeJson(tankDoc, tankFile);
  tankFile.close();

  Serial.println("All configuration files reset to default values.");
}

// --- RS485 Measurement Logic ---

#include <math.h> // Make sure to include math.h for round()

void onReceiveMeasurement(float measuredDistance) {
  tank.updateLiquidLevel(measuredDistance);

  // Update LCD in Monitor Mode
  if (currentMode == Monitor) {
    lcd.setCursor(0, 0);
    // Get current and total volumes
    float currentLitre = tank.getLiquidVolumeLitre();
    float totalLitre = tank.getFullTankVolumeLitre();
    long currentVolRounded = round(currentLitre);
    long totalVolRounded = round(totalLitre);
    String volStr = "Vol:" + String(currentVolRounded) + "/" + String(totalVolRounded) + "L";
    lcd.print(volStr.c_str());
    for (int i = volStr.length(); i < 16; i++) { lcd.print(" "); }

    lcd.setCursor(0, 1);
    String pctStr = "Level: " + String(tank.getLiquidPercentage(), 1) + "%";
    lcd.print(pctStr.c_str());
    for (int i = pctStr.length(); i < 16; i++) { lcd.print(" "); }
  }
}

void requestMeasurement() {
  if (millis() - lastRequestTime < measurementInterval) {
    return;
  }
  lastRequestTime = millis();

  // 1. Send command
  digitalWrite(RS485_DE_RE_PIN, RS485_TRANSMIT);
  delayMicroseconds(100);
  RS485_SERIAL.write('M');
  RS485_SERIAL.flush();
  delay(2); // Wait for last byte to physically leave
  digitalWrite(RS485_DE_RE_PIN, RS485_RECEIVE);

  // 2. Wait for response
  unsigned long waitStartTime = millis();
  bool responseReceived = false;
  while (millis() - waitStartTime < responseTimeout) {
    if (RS485_SERIAL.available() > 0) {
      String response = RS485_SERIAL.readStringUntil('\n');
      response.trim();
      Serial.println("Received: " + response);

      if (response.startsWith("D:")) {
        float distance = response.substring(2).toFloat();
        onReceiveMeasurement(distance);
      } else if (response.startsWith("E:")) {
        String error = response.substring(2);
        Serial.println("Error from slave: " + error);
        if(currentMode == Monitor) {
          lcd.setCursor(0, 1);
          lcd.print("Sensor Error!   ");
        }
      }
      responseReceived = true;
      break;
    }
  }

  if (!responseReceived) {
    Serial.println("Error: No response from slave (timeout).");
    if(currentMode == Monitor) {
        lcd.setCursor(0, 1);
        lcd.print("Comm Timeout!   ");
    }
  }
}