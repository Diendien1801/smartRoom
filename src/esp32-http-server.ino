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
// Chân kết nối PIR
const int PIR_PIN = 15; 
String inputCode = "";
 

unsigned long lastDistanceCheck = 0; // Thời gian lần cuối đo khoảng cách
const unsigned long distanceInterval = 200; // Kiểm tra khoảng cách mỗi 200ms
unsigned long grantTime = 0;
bool accessGranted = false;
int distance = 0;
// Thông tin Wi-Fi
const char* ssid = "Wokwi-GUEST";     
const char* password = ""; 


// Timer intervals
#define CHECK_INTERVAL 1 // 15 minutes in milliseconds
#define DISABLE_INTERVAL 3600000 // 1 hour in milliseconds
unsigned long lastCheckTime = 0;
unsigned long lastCancelTime = 0;
bool pirEnabled = true;
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
  pinMode(PIR_PIN, INPUT); 
}

void loop() {
  unsigned long currentMillis = millis();

  // Check for motion every 15 minutes
  if (currentMillis - lastCheckTime >= CHECK_INTERVAL) {
    lastCheckTime = currentMillis;

    if (pirEnabled && digitalRead(PIR_PIN) == LOW) {
      // No motion detected, call API to cancel room
      cancelRoom();
      pirEnabled = false;
      lastCancelTime = currentMillis;
    }
  }

  // Re-enable PIR sensor after 1 hour
  if (!pirEnabled && currentMillis - lastCancelTime >= DISABLE_INTERVAL) {
    pirEnabled = true;
  }
  // Kiểm tra khoảng cách mỗi khoảng thời gian nhất định
  if (currentMillis - lastDistanceCheck >= distanceInterval) {
    lastDistanceCheck = currentMillis;
    checkDistance();
  }

  // Xử lý bàn phím
  handleKeypadInput();

  // Quản lý trạng thái cấp quyền
  if (accessGranted && (millis() - grantTime >= 1000)) {
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

  if (distance <= distanceThreshold && !accessGranted) {
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Hello      "); // Hiển thị "Xin chao"
  } else if (!accessGranted) {
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

      if (inputCode.length() == 3) { // Kiểm tra mã nhập đủ 3 chữ số
        sendHttpRequest(inputCode); // Gửi yêu cầu HTTP với OTP
        inputCode = ""; // Reset mã nhập sau khi gửi yêu cầu
      } else if (inputCode.length() > 3) {
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

void sendHttpRequest(String otp) {
  HTTPClient http;

  // URL API với tham số OTP
  String url = "http://192.168.1.2:3000/api/validate-booking?room=2&otp=" + otp;  

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
      lcd.print("Access granted  ");
      myServo.write(0); // Servo quay về góc 0 độ
      accessGranted = true;
      grantTime = millis();
      delay(2000); // Hiển thị thông báo 2 giây
      lcd.clear();
    } else {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Wrong code     ");
      delay(1000); // Hiển thị thông báo thất bại
    }
  } else {
    Serial.println("Error on HTTP request");
  }

  http.end();
}

void resetAccess() {
  accessGranted = false;
  inputCode = ""; // Reset mã nhập
  myServo.write(90); // Servo quay về góc 90 độ
  lcd.clear();
}
void cancelRoom() {
  HTTPClient http;
  String url = "http://192.168.1.2:3000/api/cancel-booking?room=2";

  http.begin(url);
  int httpCode = http.GET();

  if (httpCode > 0) {
    String payload = http.getString();
    Serial.println(payload);

    if (payload == "true") {
      lcd.clear();
      lcd.setCursor(0, 0);
      //lcd.print("Room canceled  ");
      myServo.write(0);
      accessGranted = false;
      sendPushNotification("Room 2 canceled ");
    } else {
      lcd.clear();
      lcd.setCursor(0, 0);
      //lcd.print("Error canceling");
      sendPushNotification("Room 2 canceled ");
    }
  } else {
    Serial.println("Lỗi khi gửi yêu cầu HTTP");
  }

  http.end();
}

void sendPushNotification(String message) {
  HTTPClient http;
  String url = "https://www.pushsafer.com/api";
  String postData = "k=W4xYgztg4UbKqlFXboWR&t=Room%20Cancellation&m=" + message;

  http.begin(url);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int httpCode = http.POST(postData);

  if (httpCode > 0) {
    String payload = http.getString();
    Serial.println(payload);
  } else {
    Serial.println("Error sending push notification");
  }

  http.end();
}