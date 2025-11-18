#pragma once

#include <ArduinoJson.h>
#include <SPIFFS.h>

#define Cm3_To_Litre 0.001

class Tank {
private:
  float m_tankCrossSectionAreaCm2;
  float m_tankDepthCm;
  float m_liquidHeightCm;
  float m_sensorOffsetCm;
  float m_heightErrorMarginCm;

public:
  Tank();
  void begin(const char* filename = "/config/tank_params.json");
  void loadConfig(const char* filename = "/config/tank_params.json");

  void updateLiquidLevel(float sensorDistance);

  float getLiquidVolumeCm3();
  float getLiquidVolumeLitre();
  float getLiquidPercentage();
  float getFullTankVolumeCm3();
  float getFullTankVolumeLitre();
};
