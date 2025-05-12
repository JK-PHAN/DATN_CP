#include <SPI.h>
#include <LoRa.h>
#include <SoftwareSerial.h>

//define the pins used by the transceiver module
#define ss 5
#define rst 14
#define dio0 2

// Define pins and variables for sensors
#define lm335Pin A0
SoftwareSerial mySerial(0, 1); // RX, TX for dust sensor

unsigned int pm1 = 0, pm2_5 = 0, pm10 = 0;

void wakeupSensor() {
  byte wakeup[] = {0x42, 0x4D, 0xE4, 0x00, 0x01, 0x01, 0x74};
  mySerial.write(wakeup, sizeof(wakeup));
}

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200); //115200: Dùng để giao tiếp với máy tính (Serial Monitor).
   mySerial.begin(9600); //9600: Dùng để giao tiếp với cảm biến bụi qua cổng SoftwareSerial.
  wakeupSensor();

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
}

void loop() {
  // Read temperature from LM335
  int sensorValue = analogRead(lm335Pin);
  float voltage = sensorValue * (5.0 / 1023.0);
  float temperature = voltage * 20; // LM335: 10mV/K v/5/100

  // Read dust data from PMS sensor
  int index = 0;
  char value, previousValue;
  while (mySerial.available()) {
    value = mySerial.read();
    if ((index == 0 && value != 0x42) || (index == 1 && value != 0x4D)) break;
    if (index == 4 || index == 6 || index == 8) previousValue = value;
    else if (index == 5) pm1 = 256 * previousValue + value;
    else if (index == 7) pm2_5 = 256 * previousValue + value;
    else if (index == 9) pm10 = 256 * previousValue + value;
    else if (index > 15) break;
    index++;
  }
  while (mySerial.available()) mySerial.read(); // Clear buffer

  // Format and send data via LoRa
  char msg[100];
  snprintf(msg, sizeof(msg), "ID1,T:%.2f,PM1:%u,PM2.5:%u,PM10:%u", temperature, pm1, pm2_5, pm10);
  Serial.println("Sending LoRa message:");
  Serial.println(msg); // Debug message to check the data being sent

  LoRa.beginPacket();
  LoRa.print(msg);
  LoRa.endPacket();

  delay(1000); // Delay before sending the next packet
}