/*
 * ESP32 Smart Hydration Monitor - DHT22 Only with GSR Curve Analysis
 * 
 * Sensor Drift Compensations:
 * - DS18B20: -1.5°C upward drift compensation
 * - DHT22: -3% humidity correction above 70% RH  
 * - GSR: -110 units upward drift compensation
 * 
 * GSR Curve Analysis based on hydration states:
 * - HYDRATED: Peak region or rising trend after drinking
 * - PARTIALLY_HYDRATED: Declining phase from peak
 * - DEHYDRATED: Flat/low region with minimal changes
 */

#include <U8g2lib.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>

// Pin definitions
#define DHT22_PIN 5        // GPIO5 for DHT22 DATA
#define DS18B20_PIN 4      // GPIO4 for DS18B20 DATA  
#define GSR_PIN 34         // GPIO34 (ADC1) for GSR analog output
#define BUZZER_PIN 14      // GPIO14 for buzzer
#define SDA_PIN 21         // GPIO21 for I2C SDA (OLED)
#define SCL_PIN 22         // GPIO22 for I2C SCL (OLED)

// Initialize sensors
DHT dht(DHT22_PIN, DHT22);
OneWire oneWire(DS18B20_PIN);
DallasTemperature ds18b20(&oneWire);
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, SCL_PIN, SDA_PIN);

// Constants
#define GSR_MAX_THRESHOLD 2690    // GSR maxes out around 2800-2900 (compensated for 110 drift)
#define OUTDOOR_TEMP_THRESHOLD 28 // Above this temp = likely outdoor
#define HISTORY_SIZE 20           // Increased for better trend analysis
#define TREND_WINDOW 5            // Number of readings for trend calculation
#define CALIBRATION_READINGS 30   // More readings for better baseline

// Data structures
struct SensorData {
  float ambientTemp;
  float ambientHumidity;
  float bodyTemp;
  int gsrRaw;
  unsigned long timestamp;
  bool isOutdoor;
};

struct GSRAnalysis {
  float currentValue;
  float currentPercent;      // GSR as percentage of personal range (0-100%)
  float trend;              // Positive = rising, Negative = falling
  float recentPeak;         // Highest GSR in recent history
  float recentLow;          // Lowest GSR in recent history
  unsigned long timeSincePeak;
  unsigned long timeSinceLow;
  bool isPeakDetected;
  bool isInDeclinePhase;
};

struct UserProfile {
  int gsrBaseline;          // Personal baseline GSR
  int gsrRange;             // Personal GSR range
  float hydratedThreshold;  // Percentage threshold for hydrated state (70%)
  float dehydratedThreshold; // Percentage threshold for dehydrated state (30%)
  bool isCalibrated;
};

enum HydrationState {
  DEHYDRATED = 0,          // Flat/low GSR region
  PARTIALLY_HYDRATED = 1,  // Declining GSR region
  HYDRATED = 2,            // Peak/high GSR region  
  OUTDOOR_MODE = 3         // Outdoor conditions detected
};

struct SystemState {
  HydrationState currentState;
  GSRAnalysis gsrAnalysis;
  float confidenceLevel;
  String recommendation;
  bool needsAlert;
  unsigned long lastAlertTime;
  unsigned long lastRecommendationTime;
};

// Global variables
UserProfile profile;
SensorData sensorHistory[HISTORY_SIZE];
int historyIndex = 0;
SystemState systemState;
unsigned long lastReading = 0;
bool buzzerActive = false;

void setup() {
  Serial.begin(115200);
  
  // Initialize sensors
  dht.begin();
  ds18b20.begin();
  ds18b20.setResolution(12);
  
  // Initialize display
  u8g2.begin();
  u8g2.enableUTF8Print();
  
  // Initialize buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  
  // Initialize system state
  systemState.currentState = OUTDOOR_MODE;
  systemState.lastAlertTime = 0;
  systemState.lastRecommendationTime = 0;
  profile.isCalibrated = false;
  
  // Initialize GSR analysis
  systemState.gsrAnalysis.recentPeak = 0;
  systemState.gsrAnalysis.recentLow = 3985;  // 4095 - 110 (max ADC - drift compensation)
  systemState.gsrAnalysis.isPeakDetected = false;
  systemState.gsrAnalysis.isInDeclinePhase = false;
  
  // Show initial OLED state
  updateDisplay();
  
  if (!profile.isCalibrated) {
    Serial.println("=== CALIBRATION MODE ===");
    Serial.println("Wear device normally and stay indoors");
    Serial.println("Calibration will take 30 readings...");
  }
}

void loop() {
  // Read sensors every 30 seconds
  if (millis() - lastReading >= 30000) {
    SensorData newReading = readAllSensors();
    
    if (!profile.isCalibrated) {
      performCalibration(newReading);
    } else {
      processReading(newReading);
      analyzeGSRCurve();
      determineHydrationState();
      
      // Output detailed analysis to serial monitor
      printSystemStatus();
      
      updateDisplay();
      checkForAlerts();
    }
    
    lastReading = millis();
  }
  
  // Check for buzzer timeout
  if (buzzerActive && millis() - systemState.lastAlertTime > 3000) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerActive = false;
  }
  
  delay(1000);
}

SensorData readAllSensors() {
  SensorData data;
  data.timestamp = millis();
  
  // Read DHT22
  data.ambientTemp = dht.readTemperature();
  data.ambientHumidity = dht.readHumidity();
  
  // Apply DHT22 humidity correction for >70% RH
  if (!isnan(data.ambientHumidity) && data.ambientHumidity > 70) {
    float correction = 3.0 * ((data.ambientHumidity - 70) / 30.0);
    data.ambientHumidity -= correction;
  }
  
  // Read DS18B20 body temperature (compensate for 1.5°C drift)
  ds18b20.requestTemperatures();
  data.bodyTemp = ds18b20.getTempCByIndex(0) - 1.5;
  
  // Read GSR (compensate for 110-unit upward drift)
  int gsrRawReading = analogRead(GSR_PIN);
  data.gsrRaw = gsrRawReading - 110;  // Compensate for drift
  data.gsrRaw = max(data.gsrRaw, 0);  // Ensure non-negative values
  
  // Determine if outdoor based on conditions
  data.isOutdoor = isOutdoorCondition(data);
  
  return data;
}

bool isOutdoorCondition(SensorData &data) {
  // Check if GSR is maxed out (sweat condition)
  if (data.gsrRaw >= GSR_MAX_THRESHOLD) {
    Serial.println("Outdoor detected: GSR maxed out (sweat)");
    return true;
  }
  
  // Check ambient temperature from DHT22
  if (!isnan(data.ambientTemp) && data.ambientTemp > OUTDOOR_TEMP_THRESHOLD) {
    Serial.printf("Outdoor detected: High temperature (%.1f°C)\n", data.ambientTemp);
    return true;
  }
  
  // Check for rapid GSR fluctuations (movement/activity)
  if (historyIndex > 3) {
    int recentAvg = 0;
    int validReadings = 0;
    for (int i = 1; i <= 3; i++) {
      int idx = (historyIndex - i + HISTORY_SIZE) % HISTORY_SIZE;
      if (sensorHistory[idx].timestamp > 0) {
        recentAvg += sensorHistory[idx].gsrRaw;
        validReadings++;
      }
    }
    
    if (validReadings > 0) {
      recentAvg /= validReadings;
      if (abs(data.gsrRaw - recentAvg) > 200) { // Large GSR variations (drift-compensated)
        Serial.println("Outdoor detected: Rapid GSR fluctuations (movement)");
        return true;
      }
    }
  }
  
  return false;
}

void startCalibration() {
  // Function removed - status now shown via serial monitor only
}

void startCalibration() {
  showCalibrationScreen();
  profile.isCalibrated = false;
}

void performCalibration(SensorData &data) {
  static int calibrationCount = 0;
  static long gsrSum = 0;
  static int gsrMin = 3985;  // 4095 - 110 (compensated max)
  static int gsrMax = 0;
  
  // Only calibrate if not outdoor
  if (!data.isOutdoor) {
    gsrSum += data.gsrRaw;
    gsrMin = min(gsrMin, data.gsrRaw);
    gsrMax = max(gsrMax, data.gsrRaw);
    calibrationCount++;
    
    // Show calibration progress in serial monitor only
    Serial.printf("Calibration Progress: %d/%d readings (GSR: %d)\n", 
                  calibrationCount, CALIBRATION_READINGS, data.gsrRaw);
    
    if (calibrationCount >= CALIBRATION_READINGS) {
      // Complete calibration
      profile.gsrBaseline = gsrSum / calibrationCount;
      profile.gsrRange = gsrMax - gsrMin;
      
      // Set percentage-based thresholds
      profile.hydratedThreshold = 70.0;    // 70% of range = hydrated
      profile.dehydratedThreshold = 30.0;  // 30% of range = dehydrated
      
      // Ensure minimum range for sensitivity
      if (profile.gsrRange < 100) {
        profile.gsrRange = 100;
      }
      
      profile.isCalibrated = true;
      
      Serial.println("=== CALIBRATION COMPLETE! ===");
      Serial.printf("GSR Baseline (drift-compensated): %d, Range: %d\n", profile.gsrBaseline, profile.gsrRange);
      Serial.printf("Hydrated Threshold: %.0f%%, Dehydrated Threshold: %.0f%%\n", 
                    profile.hydratedThreshold, profile.dehydratedThreshold);
      Serial.println("Starting normal monitoring...");
    }
  } else {
    Serial.println("Move to indoor conditions for calibration");
  }
}

void processReading(SensorData &newReading) {
  // Store in history
  sensorHistory[historyIndex] = newReading;
  historyIndex = (historyIndex + 1) % HISTORY_SIZE;
}

void analyzeGSRCurve() {
  SensorData current = sensorHistory[(historyIndex - 1 + HISTORY_SIZE) % HISTORY_SIZE];
  
  if (current.isOutdoor) {
    // Skip GSR analysis for outdoor conditions
    return;
  }
  
  // Calculate GSR trend over recent readings
  if (historyIndex >= TREND_WINDOW) {
    float trendSum = 0;
    int validTrendPoints = 0;
    
    for (int i = 1; i < TREND_WINDOW; i++) {
      int currentIdx = (historyIndex - 1 + HISTORY_SIZE) % HISTORY_SIZE;
      int previousIdx = (historyIndex - 1 - i + HISTORY_SIZE) % HISTORY_SIZE;
      
      if (sensorHistory[currentIdx].timestamp > 0 && sensorHistory[previousIdx].timestamp > 0) {
        trendSum += (sensorHistory[currentIdx].gsrRaw - sensorHistory[previousIdx].gsrRaw);
        validTrendPoints++;
      }
    }
    
    systemState.gsrAnalysis.trend = validTrendPoints > 0 ? trendSum / validTrendPoints : 0;
  }
  
  // Update current GSR values
  systemState.gsrAnalysis.currentValue = current.gsrRaw;
  
  // Calculate GSR percentage (0-100% of personal range)
  if (profile.isCalibrated && profile.gsrRange > 0) {
    float gsrPercent = ((float)(current.gsrRaw - profile.gsrBaseline) / profile.gsrRange) * 100.0;
    systemState.gsrAnalysis.currentPercent = constrain(gsrPercent, 0.0, 100.0);
  } else {
    systemState.gsrAnalysis.currentPercent = 0;
  }
  
  // Check for new peak (rising trend + high value)
  if (current.gsrRaw > systemState.gsrAnalysis.recentPeak) {
    systemState.gsrAnalysis.recentPeak = current.gsrRaw;
    systemState.gsrAnalysis.timeSincePeak = 0;
    systemState.gsrAnalysis.isPeakDetected = true;
    
    // Reset decline phase when new peak is found
    if (systemState.gsrAnalysis.trend > 0) {
      systemState.gsrAnalysis.isInDeclinePhase = false;
    }
  } else {
    systemState.gsrAnalysis.timeSincePeak += 30000; // 30 seconds per reading
  }
  
  // Check for new low
  if (current.gsrRaw < systemState.gsrAnalysis.recentLow) {
    systemState.gsrAnalysis.recentLow = current.gsrRaw;
    systemState.gsrAnalysis.timeSinceLow = 0;
  } else {
    systemState.gsrAnalysis.timeSinceLow += 30000;
  }
  
  // Detect decline phase (negative trend after peak)
  if (systemState.gsrAnalysis.isPeakDetected && 
      systemState.gsrAnalysis.trend < -2 && 
      systemState.gsrAnalysis.timeSincePeak > 120000) { // 2 minutes after peak
    systemState.gsrAnalysis.isInDeclinePhase = true;
  }
  
  // Reset peak tracking if too old (2 hours)
  if (systemState.gsrAnalysis.timeSincePeak > 7200000) {
    systemState.gsrAnalysis.recentPeak = current.gsrRaw;
    systemState.gsrAnalysis.isPeakDetected = false;
    systemState.gsrAnalysis.isInDeclinePhase = false;
  }
}

void determineHydrationState() {
  SensorData current = sensorHistory[(historyIndex - 1 + HISTORY_SIZE) % HISTORY_SIZE];
  
  if (current.isOutdoor) {
    analyzeOutdoorHydration(current);
    return;
  }
  
  // Indoor GSR-based analysis using percentage and trend
  GSRAnalysis &gsr = systemState.gsrAnalysis;
  
  // Determine state based on GSR percentage and curve analysis
  if (gsr.currentPercent >= profile.hydratedThreshold || 
      (gsr.trend > 5 && gsr.currentPercent > 60)) {
    // High GSR percentage or strong upward trend = HYDRATED
    systemState.currentState = HYDRATED;
    systemState.recommendation = "Well hydrated!";
    systemState.needsAlert = false;
    systemState.confidenceLevel = 0.9;
    
  } else if (gsr.isInDeclinePhase && gsr.currentPercent > profile.dehydratedThreshold) {
    // In declining phase but not yet at dehydration threshold = PARTIALLY_HYDRATED
    systemState.currentState = PARTIALLY_HYDRATED;
    
    // Adjust recommendation based on decline rate and percentage
    if (gsr.trend < -10 || gsr.currentPercent < 45) {
      systemState.recommendation = "Hydration declining";
      systemState.needsAlert = true;
    } else {
      systemState.recommendation = "Have a sip soon";
      systemState.needsAlert = false;
    }
    systemState.confidenceLevel = 0.7;
    
  } else if (gsr.currentPercent <= profile.dehydratedThreshold || 
            (abs(gsr.trend) < 2 && gsr.timeSincePeak > 3600000)) {
    // Low GSR percentage or flat trend for long time = DEHYDRATED
    systemState.currentState = DEHYDRATED;
    systemState.recommendation = "Drink water now!";
    systemState.needsAlert = true;
    systemState.confidenceLevel = 0.8;
    
  } else {
    // Transitional state - use trend to decide
    if (gsr.trend > 0) {
      systemState.currentState = PARTIALLY_HYDRATED;
      systemState.recommendation = "Improving hydration";
    } else {
      systemState.currentState = PARTIALLY_HYDRATED;
      systemState.recommendation = "Monitor hydration";
    }
    systemState.needsAlert = false;
    systemState.confidenceLevel = 0.6;
  }
  
  // Environmental and physiological adjustments
  if (!isnan(current.bodyTemp) && current.bodyTemp > 37.3) {
    if (systemState.currentState == HYDRATED) {
      systemState.currentState = PARTIALLY_HYDRATED;
    }
    systemState.recommendation = "Body temp high - drink";
    systemState.needsAlert = true;
  }
  
  if (!isnan(current.ambientTemp) && current.ambientTemp > 26) {
    if (systemState.currentState == PARTIALLY_HYDRATED) {
      systemState.recommendation = "Warm room - stay hydrated";
    }
  }
  
  // Periodic reminders based on state
  unsigned long reminderInterval;
  switch (systemState.currentState) {
    case HYDRATED:
      reminderInterval = 1800000; // 30 minutes
      break;
    case PARTIALLY_HYDRATED:
      reminderInterval = 900000;  // 15 minutes
      break;
    case DEHYDRATED:
      reminderInterval = 300000;  // 5 minutes
      break;
    default:
      reminderInterval = 900000;
      break;
  }
  
  if (millis() - systemState.lastRecommendationTime > reminderInterval) {
    systemState.recommendation = "Time for hydration!";
    systemState.needsAlert = true;
    systemState.lastRecommendationTime = millis();
  }
}

void analyzeOutdoorHydration(SensorData &data) {
  systemState.currentState = OUTDOOR_MODE;
  
  // Use environmental factors and body temperature for outdoor assessment
  float riskScore = 0;
  
  // Temperature risk
  if (!isnan(data.ambientTemp)) {
    if (data.ambientTemp > 32) riskScore += 0.4;
    else if (data.ambientTemp > 28) riskScore += 0.2;
    else if (data.ambientTemp > 25) riskScore += 0.1;
  }
  
  if (!isnan(data.bodyTemp)) {
    if (data.bodyTemp > 38.0) riskScore += 0.5;
    else if (data.bodyTemp > 37.5) riskScore += 0.3;
    else if (data.bodyTemp > 37.2) riskScore += 0.1;
  }
  
  // Humidity risk (low humidity increases dehydration)
  if (!isnan(data.ambientHumidity)) {
    if (data.ambientHumidity < 20) riskScore += 0.3;
    else if (data.ambientHumidity < 40) riskScore += 0.1;
  }
  
  systemState.confidenceLevel = riskScore;
  
  // Generate outdoor recommendations
  if (riskScore > 0.6) {
    systemState.recommendation = "DRINK WATER NOW!";
    systemState.needsAlert = true;
  } else if (riskScore > 0.3) {
    systemState.recommendation = "Take a sip soon";
    systemState.needsAlert = true;
  } else {
    systemState.recommendation = "Stay aware outdoors";
    systemState.needsAlert = false;
  }
  
  // Periodic outdoor reminders (every 10 minutes in high risk, 20 minutes otherwise)
  unsigned long outdoorInterval = (riskScore > 0.3) ? 600000 : 1200000;
  if (millis() - systemState.lastRecommendationTime > outdoorInterval) {
    systemState.recommendation = "Did you hydrate?";
    systemState.needsAlert = true;
    systemState.lastRecommendationTime = millis();
  }
}

void checkForAlerts() {
  if (systemState.needsAlert && millis() - systemState.lastAlertTime > 15000) {
    triggerAlert();
    systemState.lastAlertTime = millis();
    systemState.needsAlert = false;
  }
}

void triggerAlert() {
  // Activate buzzer
  digitalWrite(BUZZER_PIN, HIGH);
  buzzerActive = true;
  
  // Flash display for emphasis
  for (int i = 0; i < 2; i++) {
    u8g2.clearBuffer();
    u8g2.sendBuffer();
    delay(300);
    updateDisplay();
    delay(300);
  }
}

void updateDisplay() {
  u8g2.clearBuffer();
  
  SensorData current = sensorHistory[(historyIndex - 1 + HISTORY_SIZE) % HISTORY_SIZE];
  
  if (!profile.isCalibrated) {
    // Show calibration message
    u8g2.setFont(u8g2_font_7x13B_tr);
    u8g2.drawStr(20, 30, "CALIBRATING");
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(30, 45, "Please wait...");
  } else {
    // Show recommendation/tip (main focus)
    u8g2.setFont(u8g2_font_7x13B_tr);
    
    // Split long recommendations into multiple lines
    String rec = systemState.recommendation;
    if (rec.length() > 16) {
      int spacePos = rec.indexOf(' ', 10);
      if (spacePos > 0 && spacePos < rec.length()) {
        String line1 = rec.substring(0, spacePos);
        String line2 = rec.substring(spacePos + 1);
        u8g2.drawStr(5, 15, line1.c_str());
        u8g2.drawStr(5, 30, line2.c_str());
      } else {
        u8g2.drawStr(5, 20, rec.c_str());
      }
    } else {
      u8g2.drawStr(5, 20, rec.c_str());
    }
    
    // Show live sensor data
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(0, 45, "LIVE DATA:");
    
    // Temperature and Humidity
    char tempHumStr[25];
    if (!isnan(current.ambientTemp) && !isnan(current.ambientHumidity)) {
      sprintf(tempHumStr, "Temp: %.1fC  Hum: %.0f%%", current.ambientTemp, current.ambientHumidity);
    } else {
      sprintf(tempHumStr, "Temp: --C  Hum: --%");
    }
    u8g2.drawStr(0, 55, tempHumStr);
    
    // Body temperature and GSR
    char bodyGsrStr[25];
    if (!isnan(current.bodyTemp) && profile.isCalibrated) {
      sprintf(bodyGsrStr, "Body: %.1fC GSR: %.0f%%", current.bodyTemp, systemState.gsrAnalysis.currentPercent);
    } else if (!isnan(current.bodyTemp)) {
      sprintf(bodyGsrStr, "Body: %.1fC GSR: %d", current.bodyTemp, current.gsrRaw);
    } else {
      sprintf(bodyGsrStr, "Body: --C  GSR: %d", current.gsrRaw);
    }
    u8g2.drawStr(0, 65, bodyGsrStr);
  }
  
  u8g2.sendBuffer();
}

void printSystemStatus() {
  SensorData current = sensorHistory[(historyIndex - 1 + HISTORY_SIZE) % HISTORY_SIZE];
  
  Serial.println("=== HYDRATION MONITOR STATUS ===");
  
  // System state
  String stateStr = "";
  switch (systemState.currentState) {
    case DEHYDRATED:
      stateStr = "DEHYDRATED";
      break;
    case PARTIALLY_HYDRATED:
      stateStr = "PARTIALLY_HYDRATED";
      break;
    case HYDRATED:
      stateStr = "HYDRATED";
      break;
    case OUTDOOR_MODE:
      stateStr = "OUTDOOR_MODE";
      break;
  }
  Serial.printf("Current State: %s\n", stateStr.c_str());
  Serial.printf("Recommendation: %s\n", systemState.recommendation.c_str());
  
  // Sensor readings
  Serial.printf("Ambient Temp: %.1f°C\n", current.ambientTemp);
  Serial.printf("Ambient Humidity: %.1f%%\n", current.ambientHumidity);
  Serial.printf("Body Temp: %.1f°C\n", current.bodyTemp);
  Serial.printf("GSR Raw: %d (drift-compensated)\n", current.gsrRaw);
  Serial.printf("Outdoor Mode: %s\n", current.isOutdoor ? "YES" : "NO");
  
  // GSR analysis (only for indoor)
  if (!current.isOutdoor && profile.isCalibrated) {
    Serial.printf("GSR Percentage: %.1f%% of personal range\n", systemState.gsrAnalysis.currentPercent);
    Serial.printf("GSR Trend: %.1f\n", systemState.gsrAnalysis.trend);
    Serial.printf("GSR Peak: %d\n", (int)systemState.gsrAnalysis.recentPeak);
    Serial.printf("Time since peak: %lu minutes\n", systemState.gsrAnalysis.timeSincePeak / 60000);
    Serial.printf("In decline phase: %s\n", systemState.gsrAnalysis.isInDeclinePhase ? "YES" : "NO");
    Serial.printf("Confidence Level: %.2f\n", systemState.confidenceLevel);
    Serial.printf("Thresholds - Hydrated: %.0f%%, Dehydrated: %.0f%%\n", 
                  profile.hydratedThreshold, profile.dehydratedThreshold);
  }
  
  Serial.println("=====================================\n");
}
