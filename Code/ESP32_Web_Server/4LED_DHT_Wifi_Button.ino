#include <WiFi.h>
#include <WebServer.h>
#include "DHT.h"

const char* ssid = ".?";  // Tên của mạng WiFi
const char* password = "hhhhhhhh";  // Mật khẩu WiFi

WebServer server(80);

// Cấu hình chân GPIO cho 4 đèn LED và các nút nhấn
const int NUM_BUTTONS = 4;
const int BUTTON_PINS[NUM_BUTTONS] = {4, 5, 18, 19};
const int LED_PINS[NUM_BUTTONS] = {12, 13, 14, 27};

// Các biến trạng thái
bool LEDStatus[NUM_BUTTONS] = {LOW, LOW, LOW, LOW};
boolean button_states[NUM_BUTTONS] = {HIGH, HIGH, HIGH, HIGH};
unsigned long lastDebounceTimes[NUM_BUTTONS] = {0, 0, 0, 0};
const unsigned long debounceDelay = 50;  // Thời gian trễ để chống rung nút

// Cấu hình cảm biến DHT
#define DHTPIN 15
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

float temperature;
float humidity;

void setup() {
  Serial.begin(115200);
  delay(100);

  // Cấu hình các chân
  for (int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(BUTTON_PINS[i], INPUT_PULLUP);
    pinMode(LED_PINS[i], OUTPUT);
    digitalWrite(LED_PINS[i], LOW); // Ban đầu tắt tất cả các đèn LED
  }

  // Khởi động cảm biến DHT
  dht.begin();

  // Kết nối với Wi-Fi
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Cấu hình các URL endpoint
  server.on("/", handleRoot);
  server.on("/led1", []() { handleLED(0); });
  server.on("/led2", []() { handleLED(1); });
  server.on("/led3", []() { handleLED(2); });
  server.on("/led4", []() { handleLED(3); });
  server.on("/status", handleStatus);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();

  // Kiểm tra trạng thái các nút nhấn và cập nhật LED tương ứng
  for (int i = 0; i < NUM_BUTTONS; i++) {
    handleButton(i);
    digitalWrite(LED_PINS[i], LEDStatus[i] ? HIGH : LOW);
  }
}

void handleRoot() {
  server.send(200, "text/html", SendHTML());
}

void handleLED(int index) {
  LEDStatus[index] = !LEDStatus[index];  // Chuyển đổi trạng thái của LED chỉ định
  digitalWrite(LED_PINS[index], LEDStatus[index] ? HIGH : LOW); // Cập nhật trạng thái LED
  server.send(200, "text/html", SendHTML());
}

void handleStatus() {
  // Đọc dữ liệu từ cảm biến DHT
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();

  // Kiểm tra xem dữ liệu đọc từ cảm biến có hợp lệ không
  if (isnan(temperature) || isnan(humidity)) {
    server.send(500, "text/plain", "Failed to read from DHT sensor!");
    return;
  }

  // Tạo HTML để trả về trạng thái LED và thông tin cảm biến
  String statusHTML = "<div class=\"container\">\n";

  // Khối điều khiển LED (2 cái 1 hàng)
  statusHTML += "<div class=\"led-controls\">\n";
  statusHTML += "<div class=\"led-row\">\n";
  statusHTML += "<div class=\"led\">\n";
  statusHTML += "<p>Living Room: " + String(LEDStatus[0] ? "ON" : "OFF") + "</p>\n";
  statusHTML += "<a class=\"button " + String(LEDStatus[0] ? "button-off" : "button-on") + "\" href=\"/led1\">" + String(LEDStatus[0] ? "OFF" : "ON") + "</a>\n";
  statusHTML += "</div>\n";

  statusHTML += "<div class=\"led\">\n";
  statusHTML += "<p>Dining Room: " + String(LEDStatus[1] ? "ON" : "OFF") + "</p>\n";
  statusHTML += "<a class=\"button " + String(LEDStatus[1] ? "button-off" : "button-on") + "\" href=\"/led2\">" + String(LEDStatus[1] ? "OFF" : "ON") + "</a>\n";
  statusHTML += "</div>\n";
  statusHTML += "</div>\n";

  statusHTML += "<div class=\"led-row\">\n";
  statusHTML += "<div class=\"led\">\n";
  statusHTML += "<p>Bedroom 1: " + String(LEDStatus[2] ? "ON" : "OFF") + "</p>\n";
  statusHTML += "<a class=\"button " + String(LEDStatus[2] ? "button-off" : "button-on") + "\" href=\"/led3\">" + String(LEDStatus[2] ? "OFF" : "ON") + "</a>\n";
  statusHTML += "</div>\n";

  statusHTML += "<div class=\"led\">\n";
  statusHTML += "<p>Bedroom 2: " + String(LEDStatus[3] ? "ON" : "OFF") + "</p>\n";
  statusHTML += "<a class=\"button " + String(LEDStatus[3] ? "button-off" : "button-on") + "\" href=\"/led4\">" + String(LEDStatus[3] ? "OFF" : "ON") + "</a>\n";
  statusHTML += "</div>\n";
  statusHTML += "</div>\n";

  statusHTML += "</div>\n";

  // Khối thông tin cảm biến
  statusHTML += "<div class=\"sensor-info\">\n";
  statusHTML += "<div class=\"sensor-row\">\n";
  statusHTML += "<div class=\"temperature-data\">\n";
  statusHTML += "<p class=\"temperature\">Temperature: " + String(temperature) + "&deg;C</p>\n";
  statusHTML += "</div>\n";

  statusHTML += "<div class=\"humidity-data\">\n";
  statusHTML += "<p class=\"humidity\">Humidity: " + String(humidity) + "%</p>\n";
  statusHTML += "</div>\n";
  statusHTML += "</div>\n";
  statusHTML += "</div>\n";

  statusHTML += "</div>\n"; // Đóng khối chứa các nút và dữ liệu cảm biến
  server.send(200, "text/html", statusHTML);
}

void handleNotFound() {
  server.send(404, "text/plain", "404: Not Found");
}

void handleButton(int index) {
  int buttonPin = BUTTON_PINS[index];
  int ledPin = LED_PINS[index];

  // Đọc trạng thái của nút nhấn
  boolean currentButtonState = digitalRead(buttonPin);

  // Kiểm tra thời gian hiện tại
  unsigned long currentTime = millis();

  // Nếu thời gian hiện tại vượt quá thời gian debounce
  if ((currentTime - lastDebounceTimes[index]) > debounceDelay) {
    // Nếu nút nhấn đang được nhấn (trạng thái LOW)
    if (currentButtonState == LOW) {
      // Nếu trạng thái nút nhấn trước đó là HIGH (tức là vừa nhấn nút)
      if (button_states[index] == HIGH) {
        // Chuyển đổi trạng thái của LED
        LEDStatus[index] = !LEDStatus[index];
        digitalWrite(ledPin, LEDStatus[index] ? HIGH : LOW);
        // In ra thông báo nút nhấn đã được nhấn
        Serial.print("Button at pin ");
        Serial.print(buttonPin);
        Serial.println(" pressed");

        // Cập nhật trạng thái nút nhấn thành LOW
        button_states[index] = LOW;

        // Cập nhật trạng thái LED mới
        handleStatus();
      }
    } else {
      // Nếu nút nhấn không được nhấn, cập nhật trạng thái nút nhấn thành HIGH
      button_states[index] = HIGH;
    }
    // Cập nhật thời gian cuối cùng
    lastDebounceTimes[index] = currentTime;
  }
}

String SendHTML() {
  String html = "<!DOCTYPE html> <html>\n";
  html += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  html += "<title>ESP32 Weather Report</title>\n";
  html += "<style>\n";
  html += "    html { font-family: 'Open Sans', sans-serif; text-align: center; color: #333; margin: 0; padding: 0; }\n";
  html += "    body { margin-top: 50px; padding: 0; }\n";
  html += "    h1 { margin: 50px auto 30px; font-size: 36px; }\n";
  html += "    .container { display: flex; justify-content: space-between; padding: 20px; }\n";
  html += "    .led-controls, .sensor-info { flex: 1; padding: 10px; }\n";
  html += "    .led-controls { background-color: #f5f5f5; border-right: 2px solid #ddd; }\n";
  html += "    .sensor-info { background-color: #e0f7fa; }\n";
  html += "    .led-row { display: flex; justify-content: space-around; margin-bottom: 10px; }\n";
  html += "    .led { margin: 10px; }\n";
  html += "    .button { background-color: #3498db; color: white; padding: 13px 30px; font-size: 20px; border-radius: 4px; text-decoration: none; }\n";
  html += "    .button-on { background-color: #3498db; }\n";
  html += "    .button-on:active { background-color: #2980b9; }\n";
  html += "    .button-off { background-color: #34495e; }\n";
  html += "    .button-off:active { background-color: #2c3e50; }\n";
  html += "    .sensor-row { display: flex; justify-content: center; gap: 20px; }\n";
  html += "    .temperature-data, .humidity-data { padding: 10px; }\n";
  html += "    .temperature { font-size: 36px; font-weight: 300; color: #f39c12; }\n";
  html += "    .humidity { font-size: 36px; font-weight: 300; color: #3498db; }\n";
  html += "</style>\n";
  html += "<script>\n";
  html += "function updateStatus() {\n";
  html += "  fetch('/status')\n";
  html += "    .then(response => response.text())\n";
  html += "    .then(html => {\n";
  html += "      document.getElementById('status').innerHTML = html;\n";
  html += "    });\n";
  html += "}\n";
  html += "window.onload = updateStatus; // Cập nhật trạng thái ngay khi tải trang\n";
  html += "setInterval(updateStatus, 1000); // Cập nhật trạng thái mỗi 1 giây\n";
  html += "</script>\n";
  html += "</head>\n";
  html += "<body>\n";
  html += "<h1>ESP32 Web Server</h1>\n";
  html += "<div id=\"status\">\n";
  html += "</div>\n";
  html += "</body>\n";
  html += "</html>\n";
  return html;
}
