#include <SPI.h>
#include <LoRa.h>
#include "Adafruit_VEML6070.h"

//define the pins used by the transceiver module
#define ss 5
#define rst 14
#define dio0 2

// Cảm biến và chân kết nối
const int lm335Pin = A0;  // Cảm biến nhiệt độ LM335
const int inputPin = 2;   // Đầu vào từ NE555
Adafruit_VEML6070 uv = Adafruit_VEML6070();

// Biến toàn cục
volatile unsigned long pulseCount = 0;
unsigned long duration = 1000; // Thời gian đo tần số NE555: 1 giây

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);
  while (!Serial);
  Serial.println("LoRa Sender");

  // Setup LoRa transceiver module
  LoRa.setPins(ss, rst, dio0);

   while (!LoRa.begin(433E6)) { // Adjust frequency for your region
    Serial.println(".");
    delay(500);
  }

  // Change sync word (0xF3) to match the receiver
  LoRa.setSyncWord(0xF3);
  Serial.println("LoRa Initializing OK!");

  // NE555 đo độ ẩm
  pinMode(inputPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(inputPin), countPulse, RISING);

  // UV
  uv.begin(VEML6070_1_T);
}

void loop() {
  // Đọc nhiệt độ từ LM335
  int sensorValue = analogRead(lm335Pin);
  float voltage = sensorValue * (5.0 / 1023.0);
  float temperature = voltage * 20; // LM335: 10mV/K, 0°C = 273K

  // Đo độ ẩm bằng tần số NE555
  pulseCount = 0;
  unsigned long startTime = millis();
  while (millis() - startTime < duration); // đợi 1 giây
  float frequency = (pulseCount / 2.0) * (1000.0 / duration);
  float humidity = convertFrequencyToHumidity(frequency);

  // Đọc UV
  uint16_t uvLevel = uv.readUV();

  // Gửi dữ liệu qua LoRa với ID2
  char msg[100];
  snprintf(msg, sizeof(msg), "ID2,T:%.1f;H:%.1f;UV:%u", temperature, humidity, uvLevel);

  Serial.println("Sending LoRa message:");
  Serial.println(msg); // Debug message to kiểm tra dữ liệu gửi

  LoRa.beginPacket();
  LoRa.print(msg);
  LoRa.endPacket();

  delay(1000); // Delay before sending the next packet
}

void countPulse() {
  pulseCount++;
}

float convertFrequencyToHumidity(float frequency) {
  const int RH[] =     {10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 95};
  const float Fout[] = {7155,7080,7010,6945,6880,6820,6760,6705,6650,6600,6550,6500,6450,6400,6355,6305,6260,6210};
  int i;
  for (i = 0; i < sizeof(Fout)/sizeof(Fout[0]) - 1; i++) {
    if (frequency > Fout[i]) break;
  }

  float humidity;
  if (i == 0) humidity = RH[0];
  else if (i == sizeof(Fout)/sizeof(Fout[0])) humidity = RH[sizeof(RH)/sizeof(RH[0]) - 1];
  else {
    float freq1 = Fout[i], freq2 = Fout[i - 1];
    float rh1 = RH[i], rh2 = RH[i - 1];
    humidity = rh1 + (frequency - freq1) * (rh2 - rh1) / (freq2 - freq1);
  }
  return humidity;
}