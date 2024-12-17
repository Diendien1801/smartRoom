#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h> 
#include <Keypad.h>
#include <WiFiManager.h>
#include <HTTPClient.h>


#define ROW_NUM 4 
#define COLUMN_NUM 4 

char keys[ROW_NUM][COLUMN_NUM] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte pin_rows[ROW_NUM] = {19, 18, 5, 17}; 
byte pin_column[COLUMN_NUM] = {16, 4, 0, 2}; 
Keypad keypad = Keypad(makeKeymap(keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM);

// Khởi tạo LCD với địa chỉ I2C (0x27 hoặc 0x3F)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Chân kết nối Ultrasonic
const int trigPin = 26;
const int echoPin = 25;

// Chân kết nối Servo
Servo myServo;
const int servoPin = 33;

// Đặt khoảng cách tối thiểu để LCD sáng lên (đơn vị cm)
const int distanceThreshold = 100;

String inputCode = "";
const String correctCode = "123"; 

unsigned long lastDistanceCheck = 0; // Thời gian lần cuối đo khoảng cách
const unsigned long distanceInterval = 200; // Kiểm tra khoảng cách mỗi 200ms
unsigned long grantTime = 0;
bool accessGranted = false;
int distance = 0;
// Thông tin Wi-Fi
const char* ssid = "Wokwi-GUEST";     
const char* password = ""; 

void setup() {
  Serial.begin(115200);
  
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("Connected to Wi-Fi!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Khởi tạo LCD và bật đèn nền
  Wire.begin(14, 22); // SDA = GPIO 14, SCL = GPIO 22
  lcd.init();
  lcd.noBacklight();  // Đèn nền tắt khi bắt đầu

  // Cấu hình chân Trig và Echo
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Gán servo vào chân GPIO
  myServo.attach(servoPin);
  myServo.write(90);  // Servo bắt đầu ở góc 90 độ
}

void loop() {
  unsigned long currentMillis = millis();

  // Kiểm tra khoảng cách mỗi khoảng thời gian nhất định
  if (currentMillis - lastDistanceCheck >= distanceInterval) {
    lastDistanceCheck = currentMillis;
    checkDistance();
  }

  // Xử lý bàn phím
  handleKeypadInput();

  // Quản lý trạng thái cấp quyền
  if (accessGranted && (millis() - grantTime >= 2000)) {
    resetAccess();
  }
}

void checkDistance() {
  // Gửi tín hiệu sóng siêu âm
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Đo thời gian phản hồi
  long duration = pulseIn(echoPin, HIGH);

  // Tính toán khoảng cách (tính bằng cm)
  distance = duration * 0.034 / 2;

  if (distance <= distanceThreshold) {
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Xin chao      "); // Hiển thị "Xin chao"
  } else {
    lcd.noBacklight();
    lcd.clear();
  }
}

void handleKeypadInput() {
  char key = keypad.getKey();
  if (key) {
    Serial.println(key);
    if (key == '#') { // Reset mã nhập
      inputCode = "";
      lcd.setCursor(0, 1);
      lcd.print("Reset code   ");
    } else {
      inputCode += key;
      lcd.setCursor(0, 1);
      lcd.print("Code: " + inputCode + "      ");

      if (inputCode == correctCode) { // Kiểm tra mã nhập đúng
        sendHttpRequest(); // Gửi yêu cầu HTTP khi mã đúng
      } else if (inputCode.length() >= correctCode.length() && inputCode != correctCode) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Sai ma!      ");
        delay(1000); // Hiển thị thông báo 1 giây
        inputCode = ""; // Reset mã nhập
        lcd.clear();
      }
    }
  }
}

void sendHttpRequest() {
  HTTPClient http;

  // URL API
  String url = "http://192.168.1.2:3000/api/validate-booking?otp=22127323&room=2&mock=true";  

  // Gửi yêu cầu GET
  http.begin(url);
  int httpCode = http.GET();  // Thực hiện yêu cầu GET

  if (httpCode > 0) { // Kiểm tra mã phản hồi HTTP
    String payload = http.getString(); // Lấy dữ liệu trả về
    Serial.println(payload); // In kết quả

    // Kiểm tra phản hồi từ server
    if (payload == "true") {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Thành công   ");
      myServo.write(0); // Servo quay về góc 0 độ
      accessGranted = true;
      grantTime = millis();
    } else {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Thất bại     ");
      delay(1000); // Hiển thị thông báo thất bại
    }
  } else {
    Serial.println("Error on HTTP request");
  }

  http.end(); // Kết thúc yêu cầu HTTP
}

void resetAccess() {
  accessGranted = false;
  inputCode = ""; // Reset mã nhập
  myServo.write(90); // Servo quay về góc 90 độ
  lcd.clear();
}
