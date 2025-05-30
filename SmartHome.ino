#define BLYNK_TEMPLATE_ID "TMPL6poLTvrtB"
#define BLYNK_TEMPLATE_NAME "SmartHome"
#define BLYNK_AUTH_TOKEN "hqmOcvi4GJRrbYo_utN5ssNASTFS8uKB"
#define BLYNK_PRINT Serial

#include <Keypad.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>

char ssid[] = "Iphone";
char pass[] = "ntquang13";

const byte ROWS = 4;
const byte COLS = 3;
char keys[ROWS][COLS] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};
byte rowPins[ROWS] = {12, 14, 27, 26};
byte colPins[COLS] = {25, 33, 32};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

Servo lockServo;
const int SERVO_PIN = 15;
const int LOCK_POSITION = 90;
const int UNLOCK_POSITION = 0;
const int PIR_PIN = 2;
const int BUZZER_PIN = 4;
const int LIGHT_PIN = 16;
const int FAN_PIN = 5;
const int REED_PIN = 17;

bool isSecured = false; // Trạng thái bảo vệ
bool isAlerted = false; // Trạng thái báo động 
bool lightState = false; // Trạng thái đèn
bool fanState = false; // Trạng thái quạt
bool isLocked = false; // Trạng thái khóa bàn phím
bool doorPushed = false; // Trạng thái đẩy cửa

const String correctPassword = "1234"; // Mật khẩu
String inputPassword = "";
int wrongAttempts = 0; // Số lần nhập sai
unsigned long lockoutTime = 0; // Thời gian bắt đầu khóa
const unsigned long LOCKOUT_DURATION = 15000; // 15 giây

hd44780_I2Cexp lcd;

BLYNK_WRITE(V0) {
  int value = param.asInt();
  lightState = value;
  digitalWrite(LIGHT_PIN, lightState ? HIGH : LOW);
  Serial.print("Trạng thái đèn: ");
  Serial.println(lightState ? "ON" : "OFF");
}

BLYNK_WRITE(V1) {
  int value = param.asInt();
  fanState = value;
  digitalWrite(FAN_PIN, fanState ? LOW : HIGH);
  Serial.print("Trạng thái quạt: ");
  Serial.println(fanState ? "ON" : "OFF");
}

BLYNK_WRITE(V2) {
  int value = param.asInt();
  isSecured = value;
  Serial.print("Trạng thái bảo vệ: ");
  Serial.println(isSecured ? "ON" : "OFF");
}

BLYNK_CONNECTED() {
  Serial.println("Đã kết nối với Blynk, đang đồng bộ trạng thái...");
  Blynk.syncVirtual(V0);
  Blynk.syncVirtual(V1);
  Blynk.syncVirtual(V2);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Wire.begin(21, 22);
  Wire.setTimeout(1000);

  int status = lcd.begin(16, 2);
  if (status) {
    while (1);
  }
  
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Password: ");
  lcd.setCursor(0, 1);
  lcd.print("Enter password");
  delay(500);

  pinMode(PIR_PIN, INPUT_PULLDOWN);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LIGHT_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(REED_PIN, INPUT_PULLUP);

  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LIGHT_PIN, LOW);
  digitalWrite(FAN_PIN, HIGH);
  lightState = false;
  fanState = false;

  lockServo.attach(SERVO_PIN);
  lockServo.write(LOCK_POSITION);

  connectToWiFiAndBlynk();
}

void loop() {
  Blynk.run();

  if (WiFi.status() != WL_CONNECTED || !Blynk.connected()) {
    Serial.println("Mất kết nối, đang kết nối lại...");
    connectToWiFiAndBlynk();
    delay(1000);
  }

  int sensorState = digitalRead(PIR_PIN);
  Blynk.virtualWrite(V2, sensorState);

  if (isSecured) {
    if (sensorState == HIGH && !isAlerted) {
      Serial.println("Phát hiện xâm nhập! Kích hoạt báo động!");
      triggerAlarm();
      isAlerted = true;
    } else if (sensorState == LOW) {
      isAlerted = false;
    }
  }

  unsigned long currentTime = millis();
  if (isLocked && (currentTime - lockoutTime >= LOCKOUT_DURATION)) {
    isLocked = false;
    updateLCD();
    lcd.setCursor(0, 1);
    lcd.print("Enter password  ");
    Serial.println("Mở khóa bàn phím");
  }

  char key = keypad.getKey();
  if (key && !isLocked) {
    Serial.print("Key pressed: ");
    Serial.println(key);

    if (key == '#') {
      checkPassword();
    } else if (key == '*') {
      inputPassword = "";
      wrongAttempts = 0;
      updateLCD();
      Serial.println("Password cleared");
    } else if (key >= '0' && key <= '9') {
      inputPassword += key;
      updateLCD();
      Serial.print("Current input: ");
      Serial.println(inputPassword);
    }
  } else if (isLocked) {
    unsigned long remainingTime = (lockoutTime + LOCKOUT_DURATION - currentTime) / 1000;
    lcd.setCursor(0, 1);
    lcd.print("Locked:         ");
    lcd.setCursor(8, 1);
    if (remainingTime <= 9) lcd.print(" ");
    lcd.print(remainingTime);
    lcd.print("s");
  }

  int doorState = digitalRead(REED_PIN);

  // Kiểm tra nếu chưa mở khóa mà cửa bị đẩy
  if (!isAlerted && inputPassword != correctPassword && doorState == LOW && !isLocked && isSecured) {
    Serial.println("Cảnh báo: Có người đẩy cửa khi chưa mở khóa!");
    triggerAlarm();
    isAlerted = true;

    lcd.setCursor(0, 1);
    lcd.print("Door breached!");
    delay(2000);
    updateLCD();
  }

  if (sensorState == HIGH) {
  isAlerted = false;
  }

  if (doorState == HIGH) {
  doorPushed = false;
  isAlerted = false;
  }
}

void updateLCD() {
  if (Wire.endTransmission() != 0) {
    Wire.begin(21, 22);
    lcd.begin(16, 2);
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Password: ");
    lcd.setCursor(0, 1);
    lcd.print("Enter password");
  }
  lcd.setCursor(0, 0);
  lcd.print("Password:       ");
  lcd.setCursor(10, 0);
  lcd.print(inputPassword);
}

void checkPassword() {
  Serial.print("Checking password: ");
  Serial.println(inputPassword);

  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);

  if (inputPassword == correctPassword) {
    lcd.print("Correct!        ");
    Serial.println("Mật khẩu đúng! Mở cửa ...");
    bool wasArmed = isSecured;
    isSecured = false;
    lockServo.write(UNLOCK_POSITION);
    unsigned long startTime = millis();
    while (millis() - startTime < 5000) {
      Blynk.run();
    }
    lockServo.write(LOCK_POSITION);
    isSecured = wasArmed;
    wrongAttempts = 0;
    Serial.println("Mở khóa bàn phím");
  } else {
    lcd.print("Wrong!          ");
    Serial.println("Mật khẩu sai!");
    wrongAttempts++;
    if (wrongAttempts >= 3) {
      lcd.setCursor(0, 1);
      lcd.print("Trigger Warning!");
      Serial.println("Nhập sai mật khẩu quá 3 lần! Kích hoạt báo động!");
      triggerAlarm();
      isLocked = true;
      lockoutTime = millis(); // Bắt đầu thời gian khóa 15 giây
      wrongAttempts = 0; // Đặt lại sau khi khóa
    }
    unsigned long startTime = millis();
    while (millis() - startTime < 2000) {
      Blynk.run();
    }
  }
  inputPassword = "";
  updateLCD();
  if (!isLocked) {
    lcd.setCursor(0, 1);
    lcd.print("Enter password  ");
  }
}

void triggerAlarm() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(1000);
  digitalWrite(BUZZER_PIN, LOW);
}

void connectToWiFiAndBlynk() {
  int retryCount = 0;
  const int maxRetries = 20;

  WiFi.begin(ssid, pass);
  Serial.println("Đang kết nối WiFi...");

  while (WiFi.status() != WL_CONNECTED && retryCount < maxRetries) {
    delay(1000);
    Serial.print(".");
    retryCount++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nĐã kết nối WiFi!");
    Serial.println(WiFi.localIP());

    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
    Serial.println("Đang kết nối với Blynk...");
    retryCount = 0;

    while (!Blynk.connected() && retryCount < maxRetries) {
      delay(1000);
      Serial.print(".");
      retryCount++;
    }

    if (Blynk.connected()) {
      Serial.println("\nĐã kết nối với Blynk!");
    } else {
      Serial.println("\nKết nối Blynk thất bại!");
    }
  } else {
    Serial.println("\nKết nối Blynk thất bại!");
  }
}