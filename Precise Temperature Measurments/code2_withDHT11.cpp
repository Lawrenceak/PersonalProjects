#include <Arduino.h>
#include <cmath>
#include "esp_adc_cal.h"
#include <DHT.h>

// ------------------------
// PINS
// ------------------------
#define INPUT_PIN 35     // ADC1 → safe
#define DHTPIN 4         // DHT11
#define DHTTYPE DHT11

// ------------------------
// CONSTANTS
// ------------------------
const float Vs = 3.35f;
const float R1 = 10000.0f;

// Steinhart–Hart constants
double A = 0.001129148;
double B = 0.000234125;
double C = 0.0000000876741;

// ADC calibration
esp_adc_cal_characteristics_t adc_chars;
static const adc_attenuation_t ARDUINO_ATTEN = ADC_11db;
static const adc_atten_t ESP_ATTEN = ADC_ATTEN_DB_11;
static const adc_bits_width_t ADC_WIDTH = ADC_WIDTH_BIT_12;

// ------------------------
// VARIABLES
// ------------------------
double kelvinTemp = 0;
double thermistorTemp = 0;

float V2 = 0.0f;
float R2 = 0.0f;

unsigned long timer = 0;
int delayTime = 2000;

// ------------------------
// SAFE MOVING AVERAGE BUFFER
// ------------------------
#define valueQueueLength 10

float readedValues[valueQueueLength];
int bufferIndex = 0;
bool bufferFull = false;

// ------------------------
// Low pass filter
// ------------------------
const float alpha = 0.1f;
float lowPassFilter(float previousValue, float newValue) {
  return (alpha * newValue) + ((1 - alpha) * previousValue);
}

// ------------------------
// DHT
// ------------------------
DHT dht(DHTPIN, DHTTYPE);
float dhtTemperature = 0;
float dhtHumidity = 0;

// ------------------------
// ADC TASK
// ------------------------
void InputValue(void* pvParameters) {
  while (true) {

    analogRead(INPUT_PIN);
    analogRead(INPUT_PIN);

    uint32_t raw = analogRead(INPUT_PIN);
    uint32_t voltage_mv = esp_adc_cal_raw_to_voltage(raw, &adc_chars);

    float newV2Value = voltage_mv / 1000.0f;

    // Circular buffer write (NO dynamic memory)
    readedValues[bufferIndex] = newV2Value;
    bufferIndex++;

    if (bufferIndex >= valueQueueLength) {
      bufferIndex = 0;
      bufferFull = true;
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ------------------------
// DHT TASK
// ------------------------
void dhtTask(void *parameter) {
  while (true) {

    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (!isnan(h) && !isnan(t)) {
      dhtHumidity = h;
      dhtTemperature = t;
    }

    Serial.print("DHT Temp: ");
    Serial.print(dhtTemperature);
    Serial.println(" °C");

    Serial.print("DHT Humidity: ");
    Serial.print(dhtHumidity);
    Serial.println(" %");

    vTaskDelay(pdMS_TO_TICKS(2000));  
  }
}

// ------------------------
// Get Average Voltage
// ------------------------
float getAvgValue() {

  if (!bufferFull) return NAN;

  float sum = 0.0f;

  for (int i = 0; i < valueQueueLength; i++)
    sum += readedValues[i];

  return sum / valueQueueLength;
}

// ------------------------
// SETUP
// ------------------------
void setup() {

  Serial.begin(115200);

  pinMode(INPUT_PIN, INPUT);

  analogReadResolution(12);
  analogSetPinAttenuation(INPUT_PIN, ARDUINO_ATTEN);

  esp_adc_cal_characterize(
    ADC_UNIT_1,
    ESP_ATTEN,
    ADC_WIDTH,
    1100,
    &adc_chars
  );

  dht.begin();

  xTaskCreatePinnedToCore(
    InputValue,
    "ADC Task",
    4096,
    NULL,
    1,
    NULL,
    1
  );

  xTaskCreate(
    dhtTask,
    "DHT Task",
    4096,
    NULL,
    1,
    NULL
  );
}

// ------------------------
// LOOP
// ------------------------
void loop() {

  if (bufferFull) {

    V2 = getAvgValue();

    // Voltage protection
    if (V2 <= 0.01f || V2 >= (Vs - 0.01f)) {
      Serial.println("Thermistor voltage out of range");
      return;
    }

    R2 = (V2 * R1) / (Vs - V2);

    if (R2 <= 0) {
      Serial.println("Invalid resistance");
      return;
    }

    double logR = log(R2);

    kelvinTemp = 1.0 / (A + B * logR + C * pow(logR, 3));

    if (!isnan(kelvinTemp) && !isinf(kelvinTemp)) {
      thermistorTemp = lowPassFilter(thermistorTemp, kelvinTemp - 273.15);
    }

    if (millis() - timer > delayTime) {
      timer = millis();

      Serial.println("-----");
      Serial.print("Thermistor: ");
      Serial.print(thermistorTemp);
      Serial.println(" °C");
    }
  }
}