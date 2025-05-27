#include <TFT_eSPI.h>
#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <HTTPClient.h>

// TFT LCD
TFT_eSPI tft = TFT_eSPI();

// Chân LoRa
#define LORA_SS 5
#define LORA_RST 14
#define LORA_DIO0 27

// WiFi
const char* ssid = "Nha Giau";
const char* password = "Meomeo1999@";

// URL server nhận dữ liệu
const char* serverUrl = "https://datn-8k1d.onrender.com/data";

void setup() {
  Serial.begin(115200);

  // Kết nối WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");

  // Khởi tạo màn hình TFT
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_WHITE);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.setTextSize(2);
  tft.println("Initializing...");

  // Khởi tạo LoRa
  SPI.begin(18, 19, 23, LORA_SS); // SCK, MISO, MOSI, SS
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa failed.");
    tft.println("LoRa failed!");
    while (1);
  }
  LoRa.setSyncWord(0xF3);
  Serial.println("LoRa initialized.");
  tft.println("LoRa ready.");
}

void loop() {
  // Kiểm tra kết nối WiFi
  int packetSize = LoRa.parsePacket();
  // Nếu có gói tin LoRa đến
  if (packetSize) {
    String incoming = "";
    while (LoRa.available()) {
      incoming += (char)LoRa.read();
    }

    // Kiểm tra gói tin rỗng
    if (incoming.length() == 0) {
      Serial.println("Warning: Empty LoRa packet received."); // Thông báo cảnh báo nếu gói tin rỗng
      tft.fillScreen(TFT_WHITE);
      tft.setCursor(0, 0);
      tft.setTextColor(TFT_BLACK, TFT_WHITE);
      tft.println("Empty packet!");
      return;
    }
    // Hiển thị dữ liệu nhận được
    Serial.println("Received: " + incoming);
    String json = processData(incoming);

    // Kiểm tra dữ liệu hợp lệ
    if (json.length()) {
      sendData(json);
    } else {
      Serial.println("Warning: Invalid LoRa data format.");
      tft.fillScreen(TFT_WHITE);
      tft.setCursor(0, 0);
      tft.setTextColor(TFT_BLACK, TFT_WHITE);
      tft.println("Invalid format!");
    }
  }
}

// Xử lý dữ liệu từ LoRa
String processData(String data) {
  String result = "";

  if (data.startsWith("ID1")) {
    float temp = data.substring(data.indexOf("T:") + 2, data.indexOf(",PM1:")).toFloat();
    int pm1 = data.substring(data.indexOf("PM1:") + 5, data.indexOf(",PM2.5:")).toInt();
    int pm25 = data.substring(data.indexOf("PM2.5:") + 6, data.indexOf(",PM10:")).toInt();
    int pm10 = data.substring(data.indexOf("PM10:") + 5).toInt();

    // VÙNG HIỂN THỊ CHO ID1 (trên)
    tft.fillRect(0, 0, 320, 110, TFT_WHITE); // Xóa vùng trên
    tft.setCursor(0, 0);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.setTextSize(2);
    tft.println("ID1 - OUTDOOR");
    tft.println("Temp: " + String(temp) + " C");
    tft.println("PM1: " + String(pm1));
    tft.println("PM2.5: " + String(pm25));
    tft.println("PM10: " + String(pm10));

    result = "{\"ID\":\"ID1\",\"Temperature\":" + String(temp) +
             ",\"PM1\":" + String(pm1) +
             ",\"PM2.5\":" + String(pm25) +
             ",\"PM10\":" + String(pm10) + "}";

  } else if (data.startsWith("ID2")) {
    float temp = data.substring(data.indexOf("T:") + 2, data.indexOf(";H:")).toFloat();
    float hum = data.substring(data.indexOf("H:") + 2, data.indexOf(";UV:")).toFloat();
    int uv = data.substring(data.indexOf("UV:") + 3).toInt();

    // VÙNG HIỂN THỊ CHO ID2 (dưới)
    tft.fillRect(0, 120, 320, 120, TFT_WHITE); // Xóa vùng dưới
    tft.setCursor(0, 120);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.setTextSize(2);
    tft.println("ID2 - INDOOR");
    tft.println("Temp: " + String(temp) + " C");
    tft.println("Humidity: " + String(hum) + " %");
    tft.println("UV Index: " + String(uv));

    result = "{\"ID\":\"ID2\",\"Temperature\":" + String(temp) +
             ",\"Humidity\":" + String(hum) +
             ",\"UV\":" + String(uv) + "}";
  }

  return result;
}


// Gửi dữ liệu đến server
void sendData(String jsonData) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http; // Tạo đối tượng HTTPClient
    http.begin(serverUrl); // Thiết lập URL server
    http.addHeader("Content-Type", "application/json"); // Thiết lập header cho JSON
    // Gửi dữ liệu JSON
    int code = http.POST(jsonData);
    if (code > 0) {
      Serial.println("Server response: " + http.getString());
    } else {
      Serial.println("Failed to send, code: " + String(code));
    }

    http.end();
  } else {
    Serial.println("WiFi disconnected. Reconnecting...");
    WiFi.begin(ssid, password);
    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 10) {
      delay(500);
      Serial.print(".");
      retry++;
    }
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("\nReconnect failed. Skipping this data.");
    } else {
      Serial.println("\nReconnected. Sending data...");
      sendData(jsonData); // Gửi lại sau khi kết nối thành công
    }
  }

}
