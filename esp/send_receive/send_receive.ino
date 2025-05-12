#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <HTTPClient.h>

// Định nghĩa các chân kết nối với mô-đun LoRa cho ESP32
#define LORA_SCK  18   // SPI Clock
#define LORA_MISO 19   // SPI MISO
#define LORA_MOSI 23   // SPI MOSI
#define LORA_SS   5    // Chip select
#define LORA_RST  14   // Reset
#define LORA_DIO0 2    // DIO0

// WiFi thông tin
const char* ssid = "Nha Giau";
const char* password = "Meomeo1999@";

// Địa chỉ server HTTP
const char* serverUrl = "http://192.168.1.100:3001/data"; // Thay bằng địa chỉ server của bạn

String processData(String data) {
  String result = "";
  if (data.startsWith("ID1")) {
    // Xử lý dữ liệu từ out_send
    int tempStart = data.indexOf("T:") + 2;
    int tempEnd = data.indexOf(",PM1:");
    float temperature = data.substring(tempStart, tempEnd).toFloat();

    int pm1Start = data.indexOf("PM1:") + 5;
    int pm1End = data.indexOf(",PM2.5:");
    unsigned int pm1 = data.substring(pm1Start, pm1End).toInt();

    int pm2_5Start = data.indexOf("PM2.5:") + 6;
    int pm2_5End = data.indexOf(",PM10:");
    unsigned int pm2_5 = data.substring(pm2_5Start, pm2_5End).toInt();

    int pm10Start = data.indexOf("PM10:") + 5;
    unsigned int pm10 = data.substring(pm10Start).toInt();

    result = "{\"ID\": \"ID1\", \"Temperature\": " + String(temperature) +
             ", \"PM1\": " + String(pm1) +
             ", \"PM2.5\": " + String(pm2_5) +
             ", \"PM10\": " + String(pm10) + "}";
  } 
  else if (data.startsWith("ID2")) {
    // Xử lý dữ liệu từ in_send
    int tempStart = data.indexOf("T:") + 2;
    int tempEnd = data.indexOf(";H:");
    float temperature = data.substring(tempStart, tempEnd).toFloat();

    int humidityStart = data.indexOf("H:") + 2;
    int humidityEnd = data.indexOf(";UV:");
    float humidity = data.substring(humidityStart, humidityEnd).toFloat();

    int uvStart = data.indexOf("UV:") + 3;
    unsigned int uvLevel = data.substring(uvStart).toInt();

    result = "{\"ID\": \"ID2\", \"Temperature\": " + String(temperature) +
             ", \"Humidity\": " + String(humidity) +
             ", \"UV\": " + String(uvLevel) + "}";
  }
  return result;
}

void sendDataToServer(String jsonData) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverUrl); // Đặt URL server
    http.addHeader("Content-Type", "application/json"); // Đặt header JSON

    int httpResponseCode = http.POST(jsonData); // Gửi dữ liệu qua HTTP POST
    if (httpResponseCode > 0) {
      Serial.println("Data sent successfully!");
      Serial.println("Response: " + http.getString());
    } else {
      Serial.println("Error sending data: " + String(httpResponseCode));
    }
    http.end();
  } else {
    Serial.println("WiFi not connected!");
  }
}

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);
  while (!Serial);
  Serial.println("ESP32 LoRa Receiver");

  // Cấu hình SPI thủ công
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);

  // Gán chân cho LoRa
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  // Bắt đầu LoRa ở tần số 433 MHz 
  while (!LoRa.begin(433E6)) { // Tần số phải khớp với bên gửi
    Serial.println(".");
    delay(500);
  }

  // Đặt Sync Word để khớp với bên gửi
  LoRa.setSyncWord(0xF3); // Sync Word phải giống với bên gửi
  Serial.println("LoRa Initializing OK!");

  // WiFi setup
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Connected!");

  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  // Kiểm tra xem có gói tin nào được nhận không
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    // Nhận gói tin
    String LoRaData = "";
    while (LoRa.available()) {
      LoRaData += (char)LoRa.read();
    }

    Serial.println("Received packet: " + LoRaData);

    // Xử lý dữ liệu nhận được
    String jsonData = processData(LoRaData);
    if (jsonData.length() > 0) {
      sendDataToServer(jsonData); // Gửi dữ liệu qua HTTP
    }
  }
}