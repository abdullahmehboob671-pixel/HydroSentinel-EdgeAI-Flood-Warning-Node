#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- OLED Display Settings ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- LoRa SPI Pins (ESP32) ---
#define SCK     18
#define MISO    19
#define MOSI    23
#define SS      5
#define RST     14
#define DIO0    2

// --- Actuators & Indicator Pins ---
#define BUZZER_PIN    12
#define GREEN_LED_PIN 13
#define RED_LED_PIN   15

// --- Flood Risk Distance Thresholds (in cm) ---
// Adjust these to match the physical mounting height of your sensor
const int SAFE_DISTANCE_CM = 500;     // Safe water level (0% risk)
const int CRITICAL_DISTANCE_CM = 50;  // Dangerously high water level (100% risk)

void processIncomingData(String data, int rssi);

void setup() {
  Serial.begin(115200);

  // Initialize Peripheral Pins
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);

  // Initialize OLED Screen
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("[ERROR] OLED Initialization Failed!"));
    for (;;);
  }
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(10, 20);
  display.println(F("FLOOD EARLY WARNING"));
  display.setCursor(20, 35);
  display.println(F("RECEIVER ACTIVE"));
  display.setCursor(15, 50);
  display.println(F("Connecting LoRa..."));
  display.display();

  // Initialize SPI LoRa Module (433 MHz)
  SPI.begin(SCK, MISO, MOSI, SS);
  LoRa.setPins(SS, RST, DIO0);

  if (!LoRa.begin(433E6)) {
    display.clearDisplay();
    display.setCursor(0, 25);
    display.setTextSize(1);
    display.println(F("[ERROR] LoRa Init Failed!"));
    display.println(F("Check Wiring & Power"));
    display.display();
    while (1);
  }

  // Startup Indicator Blink
  digitalWrite(GREEN_LED_PIN, HIGH);
  delay(300);
  digitalWrite(GREEN_LED_PIN, LOW);
}

void loop() {
  // Check for incoming LoRa packet
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String incomingData = "";
    while (LoRa.available()) {
      incomingData += (char)LoRa.read();
    }

    int rssi = LoRa.packetRssi();

    // Serial Logging
    Serial.print("Payload Received: ");
    Serial.print(incomingData);
    Serial.print(" | RSSI: ");
    Serial.println(rssi);

    // Update screen and alert status
    processIncomingData(incomingData, rssi);
  }
}

// =========================================================================
// DATA PARSING & RISK CALCULATION
// =========================================================================
void processIncomingData(String data, int rssi) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  int distance = 0;
  int floodProbability = 0;

  // 1. EXTRACT NUMERIC DISTANCE FROM PAYLOAD
  int distIndex = data.indexOf("Distance:");
  if (distIndex != -1) {
    // String contains formatted payload e.g. "Distance: 450cm"
    String distSub = data.substring(distIndex + 9);
    distSub.trim();
    distance = distSub.toInt();
  } else {
    // Fallback if payload is sent as a direct numeric string
    distance = data.toInt(); 
  }

  // 2. CHECK FOR WARNING / HAZARD KEYWORDS
  bool hasWarningFlag = (data.indexOf("warning") >= 0 || data.indexOf("ALERT") >= 0 || data.indexOf("HAZARD") >= 0);

  // 3. DYNAMIC PROBABILITY MAPPING (0% - 100%)
  if (distance > 0) {
    if (distance >= SAFE_DISTANCE_CM) {
      floodProbability = 0;
    } else if (distance <= CRITICAL_DISTANCE_CM) {
      floodProbability = 100;
    } else {
      // Inversely map distance: smaller distance = higher risk
      floodProbability = map(distance, SAFE_DISTANCE_CM, CRITICAL_DISTANCE_CM, 0, 100);
      floodProbability = constrain(floodProbability, 0, 100);
    }
  }

  // Override probability if ML classifier on transmitter flags warning state
  if (hasWarningFlag && floodProbability < 50) {
    floodProbability = 85; 
  }

  // 4. DISPLAY LINE 1: Distance Metric
  display.setCursor(0, 2);
  display.setTextSize(2);
  if (distance > 0) {
    display.print(distance);
    display.print(" cm");
  } else {
    display.print("PKT RX");
  }

  // 5. DISPLAY LINE 2: Risk Percentage
  display.setCursor(0, 24);
  display.setTextSize(2);
  display.print("RISK: ");
  display.print(floodProbability);
  display.print("%");

  // 6. DISPLAY LINE 3 & ACTUATORS: Multi-Stage Alert States
  display.setCursor(0, 48);
  display.setTextSize(1);

  if (floodProbability < 30) {
    // SAFE / LOW RISK
    display.print("STATUS: SAFE / LOW");
    digitalWrite(GREEN_LED_PIN, HIGH);
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);

  } else if (floodProbability >= 30 && floodProbability < 70) {
    // MODERATE RISK
    display.print("STATUS: MODERATE RISK");
    digitalWrite(GREEN_LED_PIN, HIGH);
    digitalWrite(RED_LED_PIN, HIGH);
    
    // Short warning beep
    digitalWrite(BUZZER_PIN, HIGH);
    delay(50);
    digitalWrite(BUZZER_PIN, LOW);

  } else {
    // CRITICAL FLOOD HAZARD
    display.print("STATUS: CRITICAL FLOOD!");
    digitalWrite(GREEN_LED_PIN, LOW);
    digitalWrite(RED_LED_PIN, HIGH);
    
    // Continuous urgent alarm pulse
    digitalWrite(BUZZER_PIN, HIGH);
    delay(150);
    digitalWrite(BUZZER_PIN, LOW);
  }

  // Push frame buffer to display
  display.display();
}
