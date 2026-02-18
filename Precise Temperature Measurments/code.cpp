#include <Arduino.h>
#include <cmath>
#include <deque>
#include "esp_adc_cal.h"

// Pins
// ----
#define INPUT_PIN 35

// Constants
// ---------
const float Vs = 3.35f;
const float R1 = 10000.0f;

// Steinhart–Hart constants
double A = 0.001129148;
double B = 0.000234125;
double C = 0.0000000876741;

// ADC
const float ADC_MAX = 4095.0f;

// ADC calibration
esp_adc_cal_characteristics_t adc_chars;
static const adc_attenuation_t ARDUINO_ATTEN = ADC_11db;
static const adc_atten_t ESP_ATTEN = ADC_ATTEN_DB_11;
static const adc_bits_width_t ADC_WIDTH = ADC_WIDTH_BIT_12;

// Temperature variables
double kelvinTemp = 0;
double temp = 0;

// Reading variables
float V2 = 0.0f;
float R2 = 0.0f;

// Moving average buffer
const int valueQueueLength = 10;
std::deque<float> readedValues;

// --------------------
// ADC Reading Task
// --------------------
void InputValue(void* pvParameters) {
  while (true) {

    // Dummy reads for ADC stabilization
    analogRead(INPUT_PIN);
    analogRead(INPUT_PIN);

    // Raw ADC value
    uint32_t raw = analogRead(INPUT_PIN);

    // Convert to calibrated voltage (mV)
    uint32_t voltage_mv = esp_adc_cal_raw_to_voltage(raw, &adc_chars);

    // Convert to volts
    float newV2Value = voltage_mv / 1000.0f;

    if(readedValues.size() >= valueQueueLength) {
      readedValues.pop_front();
    }
    readedValues.push_back(newV2Value);

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// Removing ADC quantization noise, random electrical spikes
float getAvgValue() {
  if (readedValues.size() < valueQueueLength) return NAN;
  float sum = 0.0f;
  for (int i = 0; i < valueQueueLength; i++) sum += readedValues[i];
  return sum / valueQueueLength;
}

// Low-pass filter (not currently used, kept as-is)
const float alpha = 0.1f;
float lowPassFilter(float previousValue, float newValue) {
  return (alpha * newValue) + ((1 - alpha) * previousValue);
}

void setup() {
  Serial.begin(115200);

  pinMode(INPUT_PIN, INPUT);

  // ADC configuration
  analogReadResolution(12);
  analogSetPinAttenuation(INPUT_PIN, ARDUINO_ATTEN);

  // ADC calibration
  esp_adc_cal_characterize(
    ADC_UNIT_1,
    ESP_ATTEN,
    ADC_WIDTH,
    1100,   // Default Vref if eFuse not present
    &adc_chars
  );

  // Start ADC task
  xTaskCreatePinnedToCore(
    InputValue,
    "Input Task",
    4096,
    NULL,
    1,
    NULL,
    1
  );
}

void loop() {
  if (readedValues.size() >= valueQueueLength) {

    // Averaged voltage
    V2 = getAvgValue();

    // Thermistor resistance
    R2 = (V2 * R1) / (Vs - V2);
    R2 = constrain(R2, 100.0f, 1000000.0f);

    // Temperature (Kelvin)
    kelvinTemp = 1.0 / (A + B * logf(R2) + C * pow(logf(R2), 3));

    // Celsius
    if (!isnan(kelvinTemp) && !isinf(kelvinTemp)) {
        temp = lowPassFilter(temp, kelvinTemp - 273.15);
    }
    
    Serial.println(temp);
  }
}