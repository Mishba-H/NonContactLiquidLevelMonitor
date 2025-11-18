#include "Tank.h"

Tank::Tank()
  : m_tankCrossSectionAreaCm2(0.0f),
    m_tankDepthCm(0.0f),
    m_liquidHeightCm(0.0f),
    m_sensorOffsetCm(0.0f),
    m_heightErrorMarginCm(0.0f) {
}

void Tank::begin(const char* filename) {
  Tank::loadConfig(filename);
}

void Tank::loadConfig(const char* filename) {
  File configFile = SPIFFS.open(filename, "r");
  if (!configFile) {
    Serial.print("Failed to open tank config file: ");
    Serial.println(filename);
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, configFile);
  configFile.close();
  
  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }

  m_tankCrossSectionAreaCm2 = doc["tankCrossSectionAreaCm2"] | 500.0f;
  m_tankDepthCm = doc["tankDepthCm"] | 100.0f;
  m_sensorOffsetCm = doc["sensorOffsetCm"] | 5.0f;
  m_heightErrorMarginCm = doc["heightErrorMarginCm"] | 2.0f;
  
  Serial.println("Tank parameters loaded successfully.");
}

void Tank::updateLiquidLevel(float sensorDistance) {
  float calculatedHeight = m_tankDepthCm - (sensorDistance - m_sensorOffsetCm);

  if (calculatedHeight <= m_heightErrorMarginCm) {
    m_liquidHeightCm = 0.0;
  } else if (calculatedHeight > m_tankDepthCm) {
    m_liquidHeightCm = m_tankDepthCm;
  } else {
    m_liquidHeightCm = calculatedHeight;
  }
}

// --- Getters ---
float Tank::getLiquidVolumeCm3() {
  return m_liquidHeightCm * m_tankCrossSectionAreaCm2;
}
float Tank::getLiquidVolumeLitre() {
  return getLiquidVolumeCm3() * Cm3_To_Litre;
}
float Tank::getLiquidPercentage() {
  if (m_tankDepthCm <= 0) return 0.0f;
  float percentage = (m_liquidHeightCm / m_tankDepthCm) * 100.0f;
  return constrain(percentage, 0.0, 100.0);
}
float Tank::getFullTankVolumeCm3() {
    return m_tankCrossSectionAreaCm2 * m_tankDepthCm;
}

float Tank::getFullTankVolumeLitre() {
  return getFullTankVolumeCm3() * Cm3_To_Litre;
}
