// CO2_Plotter.ino
// Улучшенная и упрощённая версия скетча самописца CO2
// Работает с MH-Z19 (PWM), шаговым мотором BYJ-48 через ULN2003, сервоприводом SG90 и OLED SSD1306.

#include <SPI.h>
#include <Wire.h>
#include <AccelStepper.h>
#include <Servo.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- Настройки дисплея ---
#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);

// --- Шаговый мотор (BYJ-48 через ULN2003) ---
#define STEP_PIN_1 8
#define STEP_PIN_2 9
#define STEP_PIN_3 10
#define STEP_PIN_4 11
// AccelStepper: 8 = FULL4WIRE (для 4 проводного шагового мотора)
AccelStepper stepper(AccelStepper::FULL4WIRE, STEP_PIN_1, STEP_PIN_3, STEP_PIN_2, STEP_PIN_4);

// --- Серво пера ---
#define SERVO_PIN 6
Servo penServo;

// --- Датчик CO2 (PWM) ---
#define CO2_PWM_PIN 5   // цифровой вход, читаем HIGH/LOW и вычисляем длительность сигналов

// --- Потенциометр для регулирования скорости протяжки ---
#define POT_PIN A0

// --- Светодиод (индикатор) ---
#define LED_PIN 13

// Переменные для измерения PWM MH-Z19
unsigned long tHigh = 0;
unsigned long tLow = 0;
unsigned long lastChangeTime = 0;
int prevState = LOW;
unsigned long highStart = 0;
unsigned long lowStart = 0;
long ppm = 0;

// Параметры серво
int servoAngle = 0; // 0..30 град

void setup() {
  Serial.begin(9600);
  pinMode(CO2_PWM_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);

  stepper.setMaxSpeed(800.0);
  stepper.setSpeed(200.0);

  penServo.attach(SERVO_PIN);
  penServo.write(0);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.display();

  delay(200);
}

void loop() {
  // 1) Управление мотором протяжки (скорость через потенциометр)
  int pot = analogRead(POT_PIN);               // 0..1023
  float motorSpeed = map(pot, 0, 1023, 50, 600); // настраиваем диапазон
  stepper.setSpeed(motorSpeed);
  stepper.runSpeed();

  // 2) Считываем PWM с MH-Z19
  readCO2PWM();

  // 3) Отображаем на OLED
  drawDisplay(motorSpeed, ppm);

  // 4) Управляем сервоприводом пера в зависимости от ppm
  movePenByPPM(ppm);
}

void readCO2PWM() {
  int state = digitalRead(CO2_PWM_PIN);
  unsigned long now = micros();

  if (state != prevState) {
    // изменение состояния сигнала
    if (state == HIGH) {
      // низ -> высокий: финиш low
      tLow = now - lowStart;
      lowStart = 0;
      highStart = now;
    } else {
      // высокий -> низкий: финиш high
      tHigh = now - highStart;
      highStart = 0;
      lowStart = now;
    }
    prevState = state;
  }

  // если измерены обе длительности, вычисляем ppm
  if (tHigh > 0 && tLow > 0) {
    // формула из оригинала: ppm = 5000 * (th - 2) / (th + tl - 4);
    // но осторожно: значения в микроcекундах, лучше конвертировать в миллисек
    float th = tHigh / 1000.0;
    float tl = tLow / 1000.0;
    // защищаем от деления на ноль и нереальных значений
    if (th + tl > 5.0) {
      ppm = (long)(5000.0 * (th - 2.0) / (th + tl - 4.0));
      if (ppm < 0) ppm = 0;
      if (ppm > 10000) ppm = 10000; // ограничение
    }
    // обнулим для следующего расчёта (будет заполнено при следующем переходе)
    tHigh = 0;
    tLow = 0;
    // индикатор изменения
    digitalWrite(LED_PIN, HIGH);
    delay(20);
    digitalWrite(LED_PIN, LOW);
    Serial.print("CO2 ppm: ");
    Serial.println(ppm);
  }
}

void drawDisplay(float motorSpeed, long co2val) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("MotorSpeed:");
  display.setCursor(80, 0);
  display.print((int)motorSpeed);

  display.setCursor(0, 16);
  display.print("CO2:");
  display.setCursor(40, 16);
  display.print(co2val);
  display.print(" ppm");

  display.display();
}

void movePenByPPM(long co2) {
  // Простая шкала: логарифмируем чувствительность
  // предотвращаем деление на ноль
  int v = (co2 <= 0) ? 300 : (int)constrain(100000L / co2, 20, 300);
  // map значение v (20..300) в угол сервы 0..30
  int angle = map(v, 300, 20, 0, 30);
  angle = constrain(angle, 0, 30);
  if (abs(angle - servoAngle) > 0) {
    servoAngle = angle;
    penServo.write(servoAngle);
  }
}
