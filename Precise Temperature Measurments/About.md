## Precision Temperature Measurement System

Designed and implemented a high-accuracy temperature measurement system using a discrete analog front-end combined with digital signal processing techniques. The objective was to achieve measurement precision comparable to the DHT11 temperature and humidity sensor, while improving response time and signal stability.

### Hardware Design

The sensing stage is based on a 10KΩ NTC thermistor configured in a voltage divider with a 10KΩ reference resistor. A 0.1µF capacitor was added for analog signal filtering to reduce noise before ADC acquisition. The design prioritizes simplicity, stability, and cost-effectiveness while maintaining measurement integrity.

### Embedded Processing

An ESP32 microcontroller was used for high-resolution ADC sampling and real-time signal processing. The firmware includes:

- Multi-sample averaging to reduce quantization and random noise  
- Digital low-pass filtering for enhanced signal stability  
- Temperature computation using the Steinhart–Hart equation to accurately model thermistor nonlinearity  

### Results

The system delivers stable, fast-response temperature readings with precision comparable to commercial digital sensors, while offering improved responsiveness and deeper control over calibration and signal processing parameters.
