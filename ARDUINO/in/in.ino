#include <SPI.h>
#include <LoRa.h>
#include "Adafruit_VEML6070.h"

// Chân LoRa
#define ss 10
#define rst 9
#define dio0 2

// Chân cảm biến
#define lm335Pin A0
#define inputPin 3  // NE555 tần số đầu ra

// Chân điều khiển thiết bị
#define QUAT_PIN 5
#define REM_PIN 4

// UV sensor
Adafruit_VEML6070 uv = Adafruit_VEML6070();

// Biến toàn cục
volatile unsigned long pulseCount = 0;
const unsigned long duration = 1000;
const unsigned long sendInterval = 30000;
const unsigned long loopInterval = 15000;
unsigned long previousSendTime = 0;
unsigned long previousLoopTime = 0;
#define NUM_SAMPLES 10

// Trạng thái rèm
bool remState = false; // Biến lưu trạng thái rèm, ban đầu là chưa kéo
// Trạng thái quạt
bool quatState = false; // Biến lưu trạng thái quạt, ban đầu là chưa bật

void setup() {
  Serial.begin(9600);
  while (!Serial);
  // Khởi động LoRa
  Serial.println("🚀 LoRa Sender Starting...");

  LoRa.setPins(ss, rst, dio0);
  while (!LoRa.begin(433E6)) {
    Serial.println("❌ LoRa init failed. Đang thử lại...");
    delay(1000);
  }
  LoRa.setSyncWord(0xF3);
  Serial.println("✅ LoRa khởi tạo thành công!");
  // Khởi động cảm biến UV, chân quạt và rèm
  pinMode(inputPin, INPUT);
  pinMode(QUAT_PIN, OUTPUT);
  digitalWrite(QUAT_PIN, LOW); // Đảm bảo quạt tắt lúc bắt đầu

  pinMode(REM_PIN, OUTPUT);
  digitalWrite(REM_PIN, HIGH); // Đảm bảo rèm tắt lúc đầu đang mở
  
  attachInterrupt(digitalPinToInterrupt(inputPin), countPulse, RISING);

  uv.begin(VEML6070_1_T);
}

void loop() {
  unsigned long currentMillis = millis(); // Lấy thời gian hiện tại
  // Biến lưu trữ dữ liệu cảm biến
  static float temperatureC = 0.0;
  static float humidity = 0.0;
  static float uvLevel = 0.0;

  // Cứ 10 giây đo lại sensor
  if (currentMillis - previousLoopTime >= loopInterval) { // Kiểm tra thời gian đã trôi qua
    previousLoopTime = currentMillis;

    bool successTemp = readTemperature(temperatureC);
    bool successHum = readHumidity(humidity);
    bool successUV = readUV(uvLevel);

    if (!successTemp || !successHum || !successUV) {
      Serial.println("⚠️ Một hoặc nhiều cảm biến đọc lỗi, bỏ qua chu kỳ này.");
      return;
    }
    // In dữ liệu trung bình
    Serial.println("--------------------------------");
    Serial.println("=== Dữ liệu môi trường (trung bình) ===");
    Serial.print("🌡️ Nhiệt độ: "); Serial.print(temperatureC, 1); Serial.println(" °C");
    Serial.print("💧 Độ ẩm: "); Serial.print(humidity, 1); Serial.println(" %");
    Serial.print("🌞 UV: "); Serial.println(uvLevel);

    controlDevices(temperatureC, humidity, uvLevel);
  }

  // Cứ 30 giây gửi dữ liệu qua LoRa
  if (currentMillis - previousSendTime >= sendInterval) {
    previousSendTime = currentMillis;
    sendLoRaData(temperatureC, humidity, uvLevel);
  }
}
// Hàm đọc nhiệt độ từ cảm biến LM335
bool readTemperature(float &avgTemp) {
  float sum = 0;
  int success = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    int val = analogRead(lm335Pin); // Đọc giá trị từ cảm biến LM335
    if (val > 0 && val < 1024) {
      float voltage = val * (5.0 / 1023.0);
      sum += voltage * 20.0;  // Vout=V/5*100: nhân chéo chia ngược ta được nhiệt độ C
      success++;
    }
    delay(10);
  }

  if (success == 0) {
    avgTemp = 0;
    return false;  // Không đọc được mẫu hợp lệ
  }
  // Tính giá trị trung bình
  avgTemp = sum / success;
  return true;  // Đọc thành công
}

// Hàm đọc độ ẩm từ cảm biến NE555
bool readHumidity(float &avgHum) {
  float sum = 0;
  int success = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    pulseCount = 0;
    unsigned long start = millis();
    while (millis() - start < duration);
    float freq = (pulseCount / 2.0) * (1000.0 / duration);
    if (freq > 0) {
      sum += convertFrequencyToHumidity(freq);
      success++;
    }
    delay(10);
  }

  if (success == 0) {
    avgHum = 0;
    return false;  // Không đọc được mẫu hợp lệ
  }
  avgHum = sum / success;
  return true;
}

// Hàm đọc UV từ cảm biến VEML6070
bool readUV(float &avgUV) {
  float sum = 0;
  int success = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    float uvVal = uv.readUV() * 0.125; // Chuyển đổi giá trị UV
    if (uvVal >= 0) {
      sum += uvVal;
      success++;
    }
    delay(10);
  }

  if (success == 0) {
    avgUV = 0;  
    return false;  // Không đọc được mẫu hợp lệ
  } 
  avgUV = round((sum / success) * 100) / 100.0;
  return true;
  
}

// Hàm gửi dữ liệu qua LoRa
void sendLoRaData(float temp, float hum, float uv) {
  String data = "ID2,T:" + String(temp, 1) +
                ",H:" + String(hum, 1) +
                ",UV:" + String(uv);

  LoRa.beginPacket();
  LoRa.print(data);
  LoRa.endPacket();

  Serial.println("📡 Đã gửi qua LoRa: " + data);
  Serial.println("--------------------------------");
}

// Hàm điều khiển thiết bị dựa trên dữ liệu cảm biến
void controlDevices(float temp, float hum, float uv) {
  // Quạt
  if (!quatState && (temp > 27 || hum > 70)) {
    digitalWrite(QUAT_PIN, HIGH);
    quatState = true;
    Serial.println("BẬT QUẠT");
  } else if (quatState && (temp < 25 || hum < 65)) {
    digitalWrite(QUAT_PIN, LOW);
    quatState = false;
    Serial.println("TẮT QUẠT");
  }

  // Rèm
  if (!remState && uv > 4.5) {
    digitalWrite(REM_PIN, LOW);
    remState = true;
    Serial.println("BẬT KÉO RÈM"); // keo ra
  } else if (remState && uv < 3.0) {
    digitalWrite(REM_PIN, HIGH);
    remState = false;
    Serial.println("TẮT KÉO RÈM"); //keo vao
  }
}

// Hàm đếm xung từ cảm biến NE555
void countPulse() {
  pulseCount++;
}
// Hàm chuyển đổi tần số sang độ ẩm
float convertFrequencyToHumidity(float frequency) {
  const int RH[] =     {10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 95};
  const float Fout[] = {7155,7080,7010,6945,6880,6820,6760,6705,6650,6600,6550,6500,6450,6400,6355,6305,6260,6210};
  int size = sizeof(Fout) / sizeof(Fout[0]);

  int i;
  for (i = 0; i < size - 1; i++) {
    if (frequency > Fout[i]) break;
  }

  if (i == 0) return RH[0];
  if (i >= size) return RH[size - 1];

  float freq1 = Fout[i], freq2 = Fout[i - 1];
  float rh1 = RH[i], rh2 = RH[i - 1];
  return rh1 + (frequency - freq1) * (rh2 - rh1) / (freq2 - freq1);
}
