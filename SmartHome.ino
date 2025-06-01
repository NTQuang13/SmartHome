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

// =============== CẤU HÌNH WIFI ===============
char ssid[] = "Iphone";
char pass[] = "ntquang13";

// =============== CẤU HÌNH BÀN PHÍM ===============
const byte ROWS = 4;
const byte COLS = 3;
char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};
byte rowPins[ROWS] = {12, 14, 27, 26};
byte colPins[COLS] = {25, 33, 32};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// =============== CẤU HÌNH THIẾT BỊ ===============
Servo lockServo;
const int SERVO_PIN = 15;
const int LOCK_POS = 90;
const int UNLOCK_POS = 0;

const int PIR_PIN = 2;
const int BUZZER_PIN = 4;
const int LIGHT_PIN = 16;
const int FAN_PIN = 5;
const int REED_PIN = 17;

// =============== TRẠNG THÁI HỆ THỐNG ===============
struct SystemState {
  bool secured = false;
  bool lightOn = false;
  bool fanOn = false;
  bool doorOpen = false;
  bool alerted = false;
  bool keypadLocked = false;
} state;

// =============== MẬT KHẨU & BẢO MẬT ===============
const String PASSWORD = "1234";
String inputPass = "";
int wrongAttempts = 0;
unsigned long lockStartTime = 0;
const unsigned long LOCK_DURATION = 15000; // 15s

// =============== LCD ===============
hd44780_I2Cexp lcd;
bool lcdReady = false;

// =============== BLYNK CALLBACKS ===============
BLYNK_WRITE(V0) { // Điều khiển đèn
  state.lightOn = param.asInt();
  digitalWrite(LIGHT_PIN, state.lightOn);
  Serial.println("Light: " + String(state.lightOn ? "ON" : "OFF"));
}

BLYNK_WRITE(V1) { // Điều khiển quạt
  state.fanOn = param.asInt();
  digitalWrite(FAN_PIN, state.fanOn ? LOW : HIGH);
  Serial.println("Fan: " + String(state.fanOn ? "ON" : "OFF"));
}

BLYNK_WRITE(V2) { // Chế độ bảo vệ
  state.secured = param.asInt();
  Serial.println("Security: " + String(state.secured ? "ON" : "OFF"));
}

BLYNK_WRITE(V3) { // Điều khiển cửa
  state.doorOpen = param.asInt();
  lockServo.write(state.doorOpen ? UNLOCK_POS : LOCK_POS);
  
  if(lcdReady) {
    lcd.setCursor(0, 1);
    lcd.print(state.doorOpen ? "Door Opened!    " : "Door Closed!    ");
  }
  Serial.println("Door: " + String(state.doorOpen ? "OPEN" : "CLOSED"));
}

BLYNK_CONNECTED() {
  Blynk.syncAll();
  Serial.println("Blynk sync complete!");
}

// =============== HÀM KHỞI TẠO ===============
void initLCD() {
  if(lcd.begin(16, 2) == 0) {
    lcdReady = true;
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Password: ");
    updateLCD();
  } else {
    Serial.println("LCD init failed!");
  }
}

void initPins() {
  pinMode(PIR_PIN, INPUT_PULLDOWN);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LIGHT_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(REED_PIN, INPUT_PULLUP);
  
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LIGHT_PIN, LOW);
  digitalWrite(FAN_PIN, HIGH); // Quạt tắt khi chân HIGH
  
  lockServo.attach(SERVO_PIN);
  lockServo.write(LOCK_POS);
}

void connectBlynk() {
  if(WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid, pass);
    Serial.print("Connecting WiFi");
    while(WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      delay(500);
    }
    Serial.println("\nConnected! IP: " + WiFi.localIP().toString());
  }
  
  if(!Blynk.connected()) {
    Blynk.config(BLYNK_AUTH_TOKEN);
    Blynk.connect(5000);
    Serial.println(Blynk.connected() ? "Blynk connected!" : "Blynk failed!");
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  delay(1000);

  initLCD();
  initPins();
  connectBlynk();
}

// =============== HÀM HIỂN THỊ ===============
void updateLCD() {
  if(!lcdReady) return;
  
  // Dòng 1: Hiển thị mật khẩu nhập
  lcd.setCursor(10, 0);
  for(int i=0; i<inputPass.length(); i++) lcd.print('*');
  for(int i=inputPass.length(); i<6; i++) lcd.print(' ');
  
  // Dòng 2: Trạng thái hệ thống
  lcd.setCursor(0, 1);
  if(state.keypadLocked) {
    unsigned long remain = (LOCK_DURATION - (millis() - lockStartTime)) / 1000;
    lcd.print("Locked: ");
    lcd.print(remain);
    lcd.print("s    ");
  } else if(state.alerted) {
    lcd.print("ALERT!          ");
  } else {
    lcd.print(inputPass.length() ? "                " : "Enter password  ");
  }
}

// =============== HÀM BẢO MẬT ===============
void triggerAlarm() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(1000);
  digitalWrite(BUZZER_PIN, LOW);
  state.alerted = true;
}

void unlockSystem() {
  state.doorOpen = true;
  lockServo.write(UNLOCK_POS);
  state.secured = false;
  state.alerted = false;
  Blynk.virtualWrite(V3, 1);
  wrongAttempts = 0;
  
  if(lcdReady) {
    lcd.setCursor(0, 1);
    lcd.print("Access granted! ");
  }
  Serial.println("System unlocked!");
}

void handleWrongPassword() {
  if(lcdReady) lcd.setCursor(0, 1);
  
  if(++wrongAttempts >= 3) {
    triggerAlarm();
    state.keypadLocked = true;
    lockStartTime = millis();
    wrongAttempts = 0;
    if(lcdReady) lcd.print("LOCKED!         ");
    Serial.println("Keypad locked!");
  } else {
    if(lcdReady) lcd.print("Try again!      ");
    Serial.println("Wrong password!");
  }
}

void checkPassword() {
  Serial.println("Input: " + inputPass);
  
  if(inputPass == PASSWORD) {
    unlockSystem();
  } else {
    handleWrongPassword();
  }
  
  inputPass = "";
  delay(2000);
  updateLCD();
}

// =============== HÀM CHÍNH ===============
void handleSensors() {
  static int lastPIR = LOW;
  static int lastReed = HIGH;
  
  int pirState = digitalRead(PIR_PIN);
  int reedState = digitalRead(REED_PIN);
  
  // Phát hiện chuyển động đáng ngờ
  if(state.secured && !state.alerted) {
    if(pirState == HIGH && lastPIR == LOW) {
      Serial.println("Intrusion detected!");
      triggerAlarm();
    }
    
    if(reedState == LOW && lastReed == HIGH && !state.doorOpen) {
      Serial.println("Door forced!");
      triggerAlarm();
    }
  }
  
  // Reset cảnh báo khi hết nguy hiểm
  if(pirState == LOW && reedState == HIGH) {
    state.alerted = false;
  }
  
  lastPIR = pirState;
  lastReed = reedState;
}

void handleKeypad() {
  char key = keypad.getKey();
  if(!key) return;
  
  if(state.keypadLocked) {
    if(millis() - lockStartTime >= LOCK_DURATION) {
      state.keypadLocked = false;
      Serial.println("Keypad unlocked");
      updateLCD();
    }
    return;
  }
  
  Serial.println("Key: " + String(key));
  
  switch(key) {
    case '#': 
      checkPassword();
      break;
      
    case '*': 
      inputPass = "";
      Serial.println("Input cleared");
      break;
      
    default: 
      if(inputPass.length() < 6) {
        inputPass += key;
      }
  }
  
  updateLCD();
}

void loop() {
  // Kết nối mạng
  static unsigned long lastConnectTry;
  if(millis() - lastConnectTry >= 10000) {
    connectBlynk();
    lastConnectTry = millis();
  }
  
  if(Blynk.connected()) Blynk.run();
  
  // Xử lý chính
  handleSensors();
  handleKeypad();
}