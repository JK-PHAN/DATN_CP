#include <SPI.h>
#include <LoRa.h>
#include <SoftwareSerial.h>

// Cấu hình LoRa
#define ss 10
#define rst 9
#define dio0 2

// Cảm biến LM335
const int lm335Pin = A0;

// Cảm biến PMS5003 (RX=6, TX=7)
SoftwareSerial pmsSerial(6, 7);

// Biến toàn cục
const unsigned long sendInterval = 30000;
const unsigned long loopInterval = 15000;
unsigned long previousSendTime = 0;
unsigned long previousLoopTime = 0;
#define NUM_SAMPLES 10

void setup() {
  Serial.begin(9600);
  while (!Serial);

  // Khởi động PMS5003
  pmsSerial.begin(9600);
  wakeupSensor();

  // Khởi động LoRa
  LoRa.setPins(ss, rst, dio0);
  while (!LoRa.begin(433E6)) {
    Serial.println("❌ Không thể khởi tạo LoRa!");
    delay(1000);
  }
  LoRa.setSyncWord(0xF3);
  Serial.println("✅ LoRa khởi tạo thành công!");
}

// Biến lưu trữ dữ liệu cảm biến
float avgTemp = 0.0;
unsigned int avgPM1 = 0, avgPM2_5 = 0, avgPM10 = 0;
bool hasValidData = false;

void loop() {
  unsigned long currentMillis = millis();

  // Đọc dữ liệu cảm biến mỗi 10 giây
  if (currentMillis - previousLoopTime >= loopInterval) {
    previousLoopTime = currentMillis;
    float temp;
    unsigned int pm1, pm2_5, pm10;

    bool successTemp = readTemperature(temp);
    bool successPM = readPMSData(pm1, pm2_5, pm10);

    if (successTemp && successPM) {
      avgTemp = temp;
      avgPM1 = pm1;
      avgPM2_5 = pm2_5;
      avgPM10 = pm10;
      hasValidData = true;

      // In dữ liệu mới
      Serial.println("--------------------------------");
      printData(avgTemp, avgPM1, avgPM2_5, avgPM10);
    } else {
      Serial.println("⚠️ Một hoặc nhiều cảm biến đọc lỗi, bỏ qua chu kỳ này.");
      //hasValidData = false;
    }
  }

  // Gửi dữ liệu mỗi 30 giây nếu đã có dữ liệu hợp lệ
  if (hasValidData && (currentMillis - previousSendTime >= sendInterval)) {
    previousSendTime = currentMillis;
    sendLoRaData(avgTemp, avgPM1, avgPM2_5, avgPM10);
  }
}

// Hàm đánh thức cảm biến PMS5003
void wakeupSensor() {
  byte wakeup[] = {0x42, 0x4D, 0xE4, 0x00, 0x01, 0x01, 0x74};
  pmsSerial.write(wakeup, sizeof(wakeup));
  Serial.println("🌀 Quạt PMS5003 đã bật!");
}
/// Hàm đọc nhiệt độ từ cảm biến LM335
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

  avgTemp = sum / success;
  return true;  // Đọc thành công
}

// Hàm đọc dữ liệu từ cảm biến PMS5003
// Hàm đọc dữ liệu từ cảm biến PMS5003 kèm in buffer để debug
bool readPMSData(unsigned int &avgPM1, unsigned int &avgPM2_5, unsigned int &avgPM10) {
  const int packetSize = 32;
  byte buffer[packetSize];
  int pmSuccess = 0;
  unsigned long pm1Sum = 0, pm2_5Sum = 0, pm10Sum = 0;

  for (int i = 0; i < 2; i++) {
    // Tìm đầu gói dữ liệu
    while (pmsSerial.available() >= packetSize) {
      if (pmsSerial.read() == 0x42 && pmsSerial.peek() == 0x4D) {
        pmsSerial.read(); // Đọc 0x4D
        buffer[0] = 0x42;
        buffer[1] = 0x4D;
        pmsSerial.readBytes(buffer + 2, packetSize - 2); // Đọc 30 byte còn lại

        // Tính checksum
        unsigned int checksum = 0;
        for (int j = 0; j < packetSize - 2; j++) {
          checksum += buffer[j];
        }

        unsigned int receivedChecksum = (buffer[30] << 8) | buffer[31];

        if (checksum != receivedChecksum) {
          Serial.println("⚠️ Checksum không khớp, bỏ qua gói dữ liệu.");
          continue;
        }

        // Lấy giá trị PM (môi trường thực)
        unsigned int pm1Val   = (buffer[10] << 8) | buffer[11];
        unsigned int pm2_5Val = (buffer[12] << 8) | buffer[13];
        unsigned int pm10Val  = (buffer[14] << 8) | buffer[15];

        if (pm1Val < 2000 && pm2_5Val < 2000 && pm10Val < 2000) {
          pm1Sum += pm1Val;
          pm2_5Sum += pm2_5Val;
          pm10Sum += pm10Val;
          pmSuccess++;
        }

        break; // Đọc một gói là đủ trong vòng for
      } else {
        pmsSerial.read(); // Loại bỏ byte không đúng
      }
    }

    delay(1000); // Đợi gói tiếp theo
  }

  if (pmSuccess == 0) {
    avgPM1 = avgPM2_5 = avgPM10 = 0;
    return false;
  }

  avgPM1 = pm1Sum / pmSuccess;
  avgPM2_5 = pm2_5Sum / pmSuccess;
  avgPM10 = pm10Sum / pmSuccess;
  return true;
}






// Hàm gửi dữ liệu qua LoRa
void sendLoRaData(float temp, unsigned int pm1, unsigned int pm2_5, unsigned int pm10) {
  String data = "ID1,T:" + String(temp, 2) + ","
                + "PM1:" + String(pm1) + ","
                + "PM2.5:" + String(pm2_5) + ","
                + "PM10:" + String(pm10);

  LoRa.beginPacket();
  LoRa.print(data);
  LoRa.endPacket();

  Serial.println("📡 Đã gửi qua LoRa: " + data);
  Serial.println("--------------------------------");
}
// Hàm in dữ liệu trung bình
void printData(float temp, unsigned int pm1, unsigned int pm2_5, unsigned int pm10) {
  Serial.println("=== Dữ liệu môi trường (trung bình) ===");
  Serial.print("🌡️ Nhiệt độ: ");
  Serial.print(temp, 2);
  Serial.println(" °C");

  Serial.print("💨 PM1.0: ");
  Serial.print(pm1);
  Serial.print(" | PM2.5: ");
  Serial.print(pm2_5);
  Serial.print(" | PM10: ");
  Serial.println(pm10);
}


