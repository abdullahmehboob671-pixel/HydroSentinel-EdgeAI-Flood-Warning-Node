#include <SPI.h>
#include <LoRa.h>
#include "DHT.h"

// ⚠️ REPLACE THIS HEADER WITH YOUR ACTUAL EDGE IMPULSE PROJECT HEADER ⚠️
#include <Flood_Early_Warning_ESP32S3_DHT22_inferencing.h>

// ============================================================================
// --- Hardware Pin Definitions (ESP32-S3) ---
// ============================================================================

// SX1278 LoRa SPI Pins
#define SCK_PIN     18
#define MISO_PIN    19
#define MOSI_PIN    11
#define SS_PIN      5
#define RST_PIN     14
#define DIO0_PIN    2

// DHT22 Pin
#define DHTPIN      21
#define DHTTYPE     DHT22

// JSN-SR04T Ultrasonic Sensor Pins
#define TRIG_PIN    12
#define ECHO_PIN    13

// LoRa Operating Frequency (433 MHz)
#define LORA_BAND   433E6

// ============================================================================
// --- Global Objects & State Variables ---
// ============================================================================

DHT dht(DHTPIN, DHTTYPE);
unsigned long packetCounter = 0;
float previousDistanceCm = 0.0f;
bool isFirstRun = true;

// ============================================================================
// --- Ultrasonic Speed-of-Sound Temperature Compensation ---
// ============================================================================

float readCompensatedDistanceCm() {
  // Read temperature for acoustic compensation
  float temp = dht.readTemperature();
  if (isnan(temp)) {
    temp = 25.0f; // Default safe fallback if sensor fails
  }

  // Calculate speed of sound in air (cm/us) based on temperature
  // Formula: v = 331.3 + (0.606 * T) m/s
  float speedOfSoundCmUs = (331.3f + (0.606f * temp)) / 10000.0f;

  // Trigger 20us ultrasonic pulse
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(5);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(20);
  digitalWrite(TRIG_PIN, LOW);

  // Measure round-trip echo pulse duration (timeout at 40,000 us ~= 6.8 meters)
  long durationUs = pulseIn(ECHO_PIN, HIGH, 40000);

  if (durationUs == 0) {
    // Timeout safeguard (return safe default distance)
    return 220.0f;
  }

  // Calculate one-way distance in centimeters
  float distanceCm = (durationUs * speedOfSoundCmUs) / 2.0f;
  
  // Constrain reading to physical sensor range bounds (20 cm to 450 cm)
  return constrain(distanceCm, 20.0f, 450.0f);
}

// ============================================================================
// --- Setup Initializations ---
// ============================================================================

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000); // Wait briefly for Serial Monitor

  Serial.println("\n=======================================================");
  Serial.println("   ESP32-S3 Edge-AI Flood Early Warning Transmitter    ");
  Serial.println("=======================================================\n");

  // Initialize GPIOs
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  // Initialize DHT Sensor
  dht.begin();
  Serial.println("[OK] DHT22 Sensor initialized.");

  // Initialize LoRa SPI Transceiver
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
  LoRa.setPins(SS_PIN, RST_PIN, DIO0_PIN);

  if (!LoRa.begin(LORA_BAND)) {
    Serial.println("[FATAL ERROR] SX1278 LoRa initialization failed!");
    while (1) {
      delay(1000);
    }
  }

  // Set LoRa Tx Power to max (20 dBm = 100mW)
  LoRa.setTxPower(20);
  Serial.println("[OK] SX1278 LoRa Active on 433 MHz (Tx Power: 20 dBm).\n");
}

// ============================================================================
// --- Main Acquisition & Inference Loop ---
// ============================================================================

void loop() {
  Serial.println("-------------------------------------------------------");
  Serial.printf("--- Cycle #%lu: Sensor Acquisition & Inference ---\n", packetCounter);

  // 1. Read Raw Sensors
  float currentDistanceCm = readCompensatedDistanceCm();
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  // Handle DHT sensor read fallbacks
  if (isnan(temperature)) temperature = 25.0f;
  if (isnan(humidity))    humidity = 60.0f;

  // 2. Compute Rate of Rise (cm per sample step)
  if (isFirstRun) {
    previousDistanceCm = currentDistanceCm;
    isFirstRun = false;
  }

  float rateOfRise = previousDistanceCm - currentDistanceCm;
  previousDistanceCm = currentDistanceCm;

  // 3. Construct Model Input Array
  // [0]: humidity | [1]: rate_of_rise | [2]: temperature | [3]: water_distance_cm
  float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE] = {
    humidity,
    rateOfRise,
    temperature,
    currentDistanceCm
  };

  Serial.printf("Measured -> Dist: %.1f cm | Rate: %.2f cm/s | Temp: %.1f C | Hum: %.1f%%\n",
                currentDistanceCm, rateOfRise, temperature, humidity);

  // 4. Wrap Features into Edge Impulse Signal Struct
  signal_t signal;
  int signal_err = numpy::signal_from_buffer(features, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);
  if (signal_err != 0) {
    Serial.printf("[ERROR] Failed to create signal buffer (%d)\n", signal_err);
    delay(3000);
    return;
  }

  // 5. Run Local Classifier Model
  ei_impulse_result_t result = { 0 };
  EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);

  if (res != EI_IMPULSE_OK) {
    Serial.printf("[ERROR] Model Classification Failed (%d)\n", res);
    delay(3000);
    return;
  }

  // 6. Parse Top Predicted Class Label
  int topClassIndex = 0;
  float maxConfidence = 0.0f;

  Serial.println("ML Probabilities:");
  for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    float confidence = result.classification[i].value;
    Serial.printf("   - %-12s: %.2f%%\n", result.classification[i].label, confidence * 100.0f);

    if (confidence > maxConfidence) {
      maxConfidence = confidence;
      topClassIndex = i;
    }
  }

  String predictedState = String(result.classification[topClassIndex].label);
  Serial.printf(">>> ACTIVE STATE: [%s] (Confidence: %.1f%%)\n", 
                predictedState.c_str(), maxConfidence * 100.0f);

  // 7. Format Compact Payload & Transmit over LoRa
  String payload = "NODE1|Pkt:" + String(packetCounter) +
                   "|Class:" + predictedState +
                   "|Conf:" + String(maxConfidence * 100.0f, 1) + "%" +
                   "|Dist:" + String(currentDistanceCm, 1) + "cm" +
                   "|Temp:" + String(temperature, 1) + "C" +
                   "|Hum:" + String(humidity, 1) + "%";

  Serial.print("📡 Transmitting via LoRa -> ");
  Serial.println(payload);

  LoRa.beginPacket();
  LoRa.print(payload);
  LoRa.endPacket();

  // Increment Packet Counter & Wait 3 Seconds for Next Cycle
  packetCounter++;
  delay(3000);
}