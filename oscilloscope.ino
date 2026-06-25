// ============================================================
//  Осциллограф на Arduino Nano
//  Вход: A7
//  Serial: 115200 baud
//
//  Протокол:
//    <- DATA:512,489,501,...\n   (512 значений 0-1023)
//    -> DELAY:500\n              (задержка в мкс/мс зависит от диапазона)
//    -> RANGE:US\n               (диапазон: US=мкс, MS=мс, S=секунды)
//    <- OK:500:US\n              (подтверждение)
// ============================================================

#define ANALOG_PIN   A7
#define NUM_SAMPLES  512
#define BAUD_RATE    115200

// Диапазоны задержки
#define RANGE_US  0   // 1–1000 мкс
#define RANGE_MS  1   // 1–1000 мс
#define RANGE_S   2   // 1–10 с (в мс)

uint16_t samples[NUM_SAMPLES];
uint32_t delayValue  = 500;   // текущая задержка
uint8_t  delayRange  = RANGE_US;

// ---- применить задержку между сэмплами ----
void applyDelay() {
  switch (delayRange) {
    case RANGE_US:
      if (delayValue > 0) delayMicroseconds(delayValue);
      break;
    case RANGE_MS:
      delay(delayValue);
      break;
    case RANGE_S:
      delay(delayValue * 1000UL);
      break;
  }
}

// ---- отправить подтверждение ----
void sendOK() {
  Serial.print(F("OK:"));
  Serial.print(delayValue);
  Serial.print(':');
  switch (delayRange) {
    case RANGE_US: Serial.println(F("US")); break;
    case RANGE_MS: Serial.println(F("MS")); break;
    case RANGE_S:  Serial.println(F("S"));  break;
  }
}

// ---- разобрать команду из Serial ----
void parseCommand(const String& cmd) {
  if (cmd.startsWith("DELAY:")) {
    uint32_t val = cmd.substring(6).toInt();
    if (val > 0) {
      delayValue = val;
      // автоклampинг по диапазону
      if (delayRange == RANGE_US  && delayValue > 16383) delayValue = 16383;
      if (delayRange == RANGE_MS  && delayValue > 1000)  delayValue = 1000;
      if (delayRange == RANGE_S   && delayValue > 10)    delayValue = 10;
      sendOK();
    }
  }
  else if (cmd.startsWith("RANGE:")) {
    String r = cmd.substring(6);
    r.trim();
    if      (r == "US") { delayRange = RANGE_US; delayValue = 500; }
    else if (r == "MS") { delayRange = RANGE_MS; delayValue = 10;  }
    else if (r == "S")  { delayRange = RANGE_S;  delayValue = 1;   }
    sendOK();
  }
}

// ---- собрать и отправить пачку сэмплов ----
void collectAndSend() {
  for (uint16_t i = 0; i < NUM_SAMPLES; i++) {
    samples[i] = analogRead(ANALOG_PIN);
    applyDelay();
  }

  Serial.print(F("DATA:"));
  for (uint16_t i = 0; i < NUM_SAMPLES; i++) {
    Serial.print(samples[i]);
    if (i < NUM_SAMPLES - 1) Serial.print(',');
  }
  Serial.println();
}

// ============================================================
void setup() {
  Serial.begin(BAUD_RATE);
  analogReference(DEFAULT);   // 5 В (Nano 5V) или 3.3 В (Nano 33)
  sendOK();                   // сообщить стартовые параметры
}

void loop() {
  // Проверяем входящие команды (неблокирующе)
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() > 0) parseCommand(cmd);
  }

  collectAndSend();
}
