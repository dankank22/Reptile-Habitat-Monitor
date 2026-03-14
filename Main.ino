/**
 * @file ReptileHabitat_FreeRTOS.ino
 * @brief Dual-core FreeRTOS reptile habitat monitor on ESP32-S3 with ultrasonic distance sensing and LCD display.
 * @author Ankith Tunuguntla, Anushka Misra
 * @date 03/07/2026
 * @details
 * Core 0 handles sensor acquisition:
 *   - AM2320 temperature/humidity
 *   - photoresistor light sensing
 *   - water level sensing
 *   - PIR motion sensing
 *   - ultrasonic distance sensing
 *
 * Core 1 handles outputs:
 *   - red LED for temp/humidity alerts
 *   - yellow LED for light alerts
 *   - blue LED for water alerts
 *   - passive buzzer beeping for motion alerts
 *   - LCD display output
 *   - serial monitor reporting for remaining sensor values
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_AM2320.h>
#include <LiquidCrystal_I2C.h>

Adafruit_AM2320 am2320 = Adafruit_AM2320();

// Change 0x27 to 0x3F if your LCD uses that address
LiquidCrystal_I2C lcd(0x27, 16, 2);

// -------------------- Pins --------------------
const int Temp_LED         = 18;   // red LED
const int Bright_LED       = 10;   // yellow LED
const int Water_LED        = 13;   // blue LED

const int LIGHT_SENSOR_PIN = 2;
const int WATER_SENSOR_PIN = 4;
const int PIR_SENSOR_PIN   = 1;

const int BUZZER_PIN       = 36;   // passive buzzer

// Ultrasonic sensor pins
const int ULTRASONIC_ECHO  = 47;
const int ULTRASONIC_TRIG  = 48;

// -------------------- Safe thresholds --------------------
const float TEMP_LOW_ALERT  = 22.0;
const float TEMP_HIGH_ALERT = 34.0;

const float HUM_LOW_ALERT   = 35.0;
const float HUM_HIGH_ALERT  = 65.0;

const int LIGHT_LOW_ALERT   = 800;
const int LIGHT_HIGH_ALERT  = 3000;

const int WATER_LOW_THRESHOLD = 1000;

// -------------------- Task periods --------------------
const TickType_t TEMP_HUM_PERIOD    = 2000 / portTICK_PERIOD_MS;
const TickType_t LIGHT_PERIOD       = 200  / portTICK_PERIOD_MS;
const TickType_t WATER_PERIOD       = 500  / portTICK_PERIOD_MS;
const TickType_t PIR_PERIOD         = 100  / portTICK_PERIOD_MS;
const TickType_t ULTRASONIC_PERIOD  = 300  / portTICK_PERIOD_MS;

const TickType_t LED_FLASH_PERIOD   = 150  / portTICK_PERIOD_MS;
const TickType_t BUZZER_PERIOD      = 200  / portTICK_PERIOD_MS;
const TickType_t SERIAL_PERIOD      = 1000 / portTICK_PERIOD_MS;
const TickType_t LCD_PERIOD         = 250  / portTICK_PERIOD_MS;

// -------------------- Shared data --------------------
volatile float temperature    = 0.0f;
volatile float humidity       = 0.0f;
volatile int lightLevel       = 0;
volatile int waterLevel       = 0;
volatile int motionState      = LOW;
volatile float objectDistance = 0.0f;   // meters

// Alert flags
volatile bool tempHumAlertActive = false;
volatile bool lightAlertActive   = false;
volatile bool waterAlertActive   = false;
volatile bool motionAlertActive  = false;

// Sensor validity
volatile bool tempHumValid      = false;
volatile bool ultrasonicValid   = false;

// -------------------- Synchronization --------------------
portMUX_TYPE dataMux = portMUX_INITIALIZER_UNLOCKED;

// -------------------- Task handles --------------------
TaskHandle_t tempHumHandle      = nullptr;
TaskHandle_t lightHandle        = nullptr;
TaskHandle_t waterHandle        = nullptr;
TaskHandle_t pirHandle          = nullptr;
TaskHandle_t ultrasonicHandle   = nullptr;

TaskHandle_t redLedHandle       = nullptr;
TaskHandle_t yellowLedHandle    = nullptr;
TaskHandle_t blueLedHandle      = nullptr;
TaskHandle_t buzzerHandle       = nullptr;
TaskHandle_t serialHandle       = nullptr;
TaskHandle_t lcdHandle          = nullptr;

// -------------------- Helper check functions --------------------
bool checkTempHumidity(float t, float h)
{
  bool alert = false;

  if (t < TEMP_LOW_ALERT || t > TEMP_HIGH_ALERT) {
    alert = true;
  }

  if (h < HUM_LOW_ALERT || h > HUM_HIGH_ALERT) {
    alert = true;
  }

  return alert;
}

bool checkLightLevel(int level)
{
  return (level < LIGHT_LOW_ALERT || level > LIGHT_HIGH_ALERT);
}

bool checkWaterLevel(int level)
{
  return (level < WATER_LOW_THRESHOLD);
}

bool checkMotion(int state)
{
  return (state == HIGH);
}

// -------------------- Sensor tasks (Core 0) --------------------

/**
 * @brief Reads AM2320 temperature and humidity periodically.
 */
void tempHumidityTask(void *arg)
{
  (void)arg;

  while (true) {
    float t = am2320.readTemperature();
    float h = am2320.readHumidity();

    portENTER_CRITICAL(&dataMux);
    if (isnan(t) || isnan(h)) {
      tempHumValid = false;
      tempHumAlertActive = false;
    } else {
      temperature = t;
      humidity = h;
      tempHumValid = true;
      tempHumAlertActive = checkTempHumidity(t, h);
    }
    portEXIT_CRITICAL(&dataMux);

    vTaskDelay(TEMP_HUM_PERIOD);
  }
}

/**
 * @brief Reads photoresistor brightness periodically.
 */
void lightTask(void *arg)
{
  (void)arg;

  while (true) {
    int level = analogRead(LIGHT_SENSOR_PIN);

    portENTER_CRITICAL(&dataMux);
    lightLevel = level;
    lightAlertActive = checkLightLevel(level);
    portEXIT_CRITICAL(&dataMux);

    vTaskDelay(LIGHT_PERIOD);
  }
}

/**
 * @brief Reads water level sensor periodically.
 */
void waterTask(void *arg)
{
  (void)arg;

  while (true) {
    int level = analogRead(WATER_SENSOR_PIN);

    portENTER_CRITICAL(&dataMux);
    waterLevel = level;
    waterAlertActive = checkWaterLevel(level);
    portEXIT_CRITICAL(&dataMux);

    vTaskDelay(WATER_PERIOD);
  }
}

/**
 * @brief Reads PIR motion sensor periodically.
 */
void pirTask(void *arg)
{
  (void)arg;

  while (true) {
    int state = digitalRead(PIR_SENSOR_PIN);

    portENTER_CRITICAL(&dataMux);
    motionState = state;
    motionAlertActive = checkMotion(state);
    portEXIT_CRITICAL(&dataMux);

    vTaskDelay(PIR_PERIOD);
  }
}

/**
 * @brief Measures distance using ultrasonic sensor and stores it in meters.
 */
void ultrasonicTask(void *arg)
{
  (void)arg;

  while (true) {
    digitalWrite(ULTRASONIC_TRIG, LOW);
    delayMicroseconds(2);

    digitalWrite(ULTRASONIC_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(ULTRASONIC_TRIG, LOW);

    unsigned long duration = pulseIn(ULTRASONIC_ECHO, HIGH, 30000);

    portENTER_CRITICAL(&dataMux);
    if (duration == 0) {
      ultrasonicValid = false;
      objectDistance = 0.0f;
    } else {
      objectDistance = (duration * 0.000343f) / 2.0f;
      ultrasonicValid = true;
    }
    portEXIT_CRITICAL(&dataMux);

    vTaskDelay(ULTRASONIC_PERIOD);
  }
}

// -------------------- Output tasks (Core 1) --------------------

/**
 * @brief Flashes red LED for temperature/humidity alert.
 */
void redLedTask(void *arg)
{
  (void)arg;
  bool ledState = false;

  while (true) {
    bool alert;

    portENTER_CRITICAL(&dataMux);
    alert = tempHumAlertActive;
    portEXIT_CRITICAL(&dataMux);

    if (alert) {
      ledState = !ledState;
      digitalWrite(Temp_LED, ledState ? HIGH : LOW);
    } else {
      ledState = false;
      digitalWrite(Temp_LED, LOW);
    }

    vTaskDelay(LED_FLASH_PERIOD);
  }
}

/**
 * @brief Flashes yellow LED for brightness alert.
 */
void yellowLedTask(void *arg)
{
  (void)arg;
  bool ledState = false;

  while (true) {
    bool alert;

    portENTER_CRITICAL(&dataMux);
    alert = lightAlertActive;
    portEXIT_CRITICAL(&dataMux);

    if (alert) {
      ledState = !ledState;
      digitalWrite(Bright_LED, ledState ? HIGH : LOW);
    } else {
      ledState = false;
      digitalWrite(Bright_LED, LOW);
    }

    vTaskDelay(LED_FLASH_PERIOD);
  }
}

/**
 * @brief Flashes blue LED for water-level alert.
 */
void blueLedTask(void *arg)
{
  (void)arg;
  bool ledState = false;

  while (true) {
    bool alert;

    portENTER_CRITICAL(&dataMux);
    alert = waterAlertActive;
    portEXIT_CRITICAL(&dataMux);

    if (alert) {
      ledState = !ledState;
      digitalWrite(Water_LED, ledState ? HIGH : LOW);
    } else {
      ledState = false;
      digitalWrite(Water_LED, LOW);
    }

    vTaskDelay(LED_FLASH_PERIOD);
  }
}

/**
 * @brief Beeps passive buzzer when motion is detected.
 */
void buzzerTask(void *arg)
{
  (void)arg;
  bool buzzerOn = false;

  while (true) {
    bool alert;

    portENTER_CRITICAL(&dataMux);
    alert = motionAlertActive;
    portEXIT_CRITICAL(&dataMux);

    if (alert) {
      buzzerOn = !buzzerOn;

      if (buzzerOn) {
        tone(BUZZER_PIN, 2000);
      } else {
        noTone(BUZZER_PIN);
      }
    } else {
      buzzerOn = false;
      noTone(BUZZER_PIN);
      digitalWrite(BUZZER_PIN, LOW);
    }

    vTaskDelay(BUZZER_PERIOD);
  }
}

/**
 * @brief Displays ultrasonic distance on LCD row 1 and motion status on LCD row 2.
 */
void lcdTask(void *arg)
{
  (void)arg;

  while (true) {
    float distance;
    bool usValid;
    bool motionDetected;

    portENTER_CRITICAL(&dataMux);
    distance       = objectDistance;
    usValid        = ultrasonicValid;
    motionDetected = motionAlertActive;
    portEXIT_CRITICAL(&dataMux);

    lcd.setCursor(0, 0);
    if (!usValid) {
      lcd.print("Dist: No Read   ");
    } else {
      lcd.print("Dist:");
      lcd.print(distance, 2);
      lcd.print(" m   ");
    }

    lcd.setCursor(0, 1);
    if (motionDetected) {
      lcd.print("Motion Detected ");
    } else {
      lcd.print("No Motion       ");
    }

    vTaskDelay(LCD_PERIOD);
  }
}

/**
 * @brief Prints remaining sensor values and alert status to Serial Monitor.
 * Distance and motion are shown on the LCD instead.
 */
void serialTask(void *arg)
{
  (void)arg;

  while (true) {
    float t, h;
    int light, water;
    bool thAlert, lAlert, wAlert, thValid;

    portENTER_CRITICAL(&dataMux);
    t       = temperature;
    h       = humidity;
    light   = lightLevel;
    water   = waterLevel;

    thAlert = tempHumAlertActive;
    lAlert  = lightAlertActive;
    wAlert  = waterAlertActive;
    thValid = tempHumValid;
    portEXIT_CRITICAL(&dataMux);

    if (!thValid) {
      Serial.println("AM2320 sensor read failed");
    } else {
      Serial.print("Temp: ");
      Serial.print(t);
      Serial.print(" C  Humidity: ");
      Serial.print(h);
      Serial.println(" %");

      if (t < TEMP_LOW_ALERT)  Serial.println("ALERT: Temperature TOO LOW");
      if (t > TEMP_HIGH_ALERT) Serial.println("ALERT: Temperature TOO HIGH");
      if (h < HUM_LOW_ALERT)   Serial.println("ALERT: Humidity TOO LOW");
      if (h > HUM_HIGH_ALERT)  Serial.println("ALERT: Humidity TOO HIGH");
    }

    Serial.print("Light Level: ");
    Serial.println(light);
    if (lAlert) {
      if (light < LIGHT_LOW_ALERT)  Serial.println("ALERT: Brightness TOO LOW");
      if (light > LIGHT_HIGH_ALERT) Serial.println("ALERT: Brightness TOO HIGH");
    }

    Serial.print("Water Sensor Value: ");
    Serial.println(water);
    if (wAlert) {
      Serial.println("ALERT: Water Level LOW");
    }

    Serial.println("----------------------------");

    vTaskDelay(SERIAL_PERIOD);
  }
}

// -------------------- Setup --------------------

void setup()
{
  Serial.begin(115200);
  delay(200);

  Wire.begin(21, 20);
  am2320.begin();

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Habitat Monitor");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");

  pinMode(Temp_LED, OUTPUT);
  pinMode(Bright_LED, OUTPUT);
  pinMode(Water_LED, OUTPUT);

  pinMode(LIGHT_SENSOR_PIN, INPUT);
  pinMode(WATER_SENSOR_PIN, INPUT);
  pinMode(PIR_SENSOR_PIN, INPUT);

  pinMode(BUZZER_PIN, OUTPUT);

  pinMode(ULTRASONIC_TRIG, OUTPUT);
  pinMode(ULTRASONIC_ECHO, INPUT);

  digitalWrite(Temp_LED, LOW);
  digitalWrite(Bright_LED, LOW);
  digitalWrite(Water_LED, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(ULTRASONIC_TRIG, LOW);

  Serial.println("Reptile Habitat Monitor Started");

  // -------- Core 0: sensing --------
  xTaskCreatePinnedToCore(tempHumidityTask, "TempHumidity Task", 4096, nullptr, 2, &tempHumHandle,    0);
  xTaskCreatePinnedToCore(lightTask,        "Light Task",        4096, nullptr, 2, &lightHandle,      0);
  xTaskCreatePinnedToCore(waterTask,        "Water Task",        4096, nullptr, 2, &waterHandle,      0);
  xTaskCreatePinnedToCore(pirTask,          "PIR Task",          4096, nullptr, 3, &pirHandle,        0);
  xTaskCreatePinnedToCore(ultrasonicTask,   "Ultrasonic Task",   4096, nullptr, 2, &ultrasonicHandle, 0);

  // -------- Core 1: outputs --------
  xTaskCreatePinnedToCore(redLedTask,       "Red LED Task",      4096, nullptr, 1, &redLedHandle,     1);
  xTaskCreatePinnedToCore(yellowLedTask,    "Yellow LED Task",   4096, nullptr, 1, &yellowLedHandle,  1);
  xTaskCreatePinnedToCore(blueLedTask,      "Blue LED Task",     4096, nullptr, 1, &blueLedHandle,    1);
  xTaskCreatePinnedToCore(buzzerTask,       "Buzzer Task",       4096, nullptr, 1, &buzzerHandle,     1);
  xTaskCreatePinnedToCore(lcdTask,          "LCD Task",          4096, nullptr, 1, &lcdHandle,        1);
  xTaskCreatePinnedToCore(serialTask,       "Serial Task",       4096, nullptr, 1, &serialHandle,     1);
}

void loop() {}