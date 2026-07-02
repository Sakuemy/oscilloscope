// ============================================================
//  Осциллограф на Arduino Nano — потоковая передача чанками
//  Вход: A7
//
//  Скорость порта: 250000 бод.
//  Это НЕ случайное число: при кварце 16 МГц делитель UART для
//  250000 получается целым (0% ошибки бита), тогда как популярные
//  115200 дают ~2.1% ошибки. Т.е. 250000 одновременно и быстрее,
//  и надёжнее — это практический максимум для штатного аппаратного
//  UART ATmega328 через переходник CH340/FTDI на Nano.
//
//  Протокол:
//    <- D:489,501,...\n   чанк из CHUNK_SIZE сэмплов (0-1023),
//                         отправляется сразу по готовности —
//                         без ожидания полного "кадра"
//    -> DELAY:500\n       задержка между сэмплами
//    -> RANGE:US\n        диапазон задержки: US=мкс, MS=мс, S=секунды
//    <- OK:500:US\n       подтверждение применённых параметров
//
//  Ширина графика (сколько точек показывать на экране) — это чисто
//  визуальная настройка веб-интерфейса. Arduino про неё не знает и
//  ничего не решает — он просто непрерывно шлёт поток чанков с
//  максимальной скоростью, какую позволяют текущая задержка между
//  сэмплами и пропускная способность порта.
// ============================================================

#define ANALOG_PIN   A7
#define CHUNK_SIZE   16      // сэмплов в одном чанке.
                             // Компромисс: чем меньше чанк — тем плавнее
                             // "едет" график (чаще приходят новые точки),
                             // но тем больше доля накладных расходов
                             // (префикс "D:", запятые, "\n") в трафике.
                             // 16 — хороший баланс для 250000 бод.
#define BAUD_RATE    250000

// Диапазоны задержки
#define RANGE_US  0   // 1–16383 мкс
#define RANGE_MS  1   // 1–1000 мс
#define RANGE_S   2   // 1–10 с (в мс)

uint16_t samples[CHUNK_SIZE];
uint32_t delayValue  = 500;   // текущая задержка между сэмплами
uint8_t  delayRange  = RANGE_US;

// ---- применить задержку между сэмплами ----
// delayValue = 1 (мкс) — практический максимум скорости выборки:
// дальше упираемся уже не в код, а в физику одного analogRead()
// (~100 мкс на конвертацию АЦП на Nano при стандартном делителе).
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
      // автоклампинг по диапазону
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

// ---- собрать один маленький чанк сэмплов и сразу отправить ----
// В отличие от прежней версии (сбор всех 512 точек в буфер и отправка
// одной большой строкой), здесь чанк уходит в Serial немедленно, как
// только набран. Это и даёт "движение" графика на клиенте вместо
// редких скачков, и держит задержку до появления новой точки на
// экране минимальной — на уровне времени одного чанка, а не всего
// кадра.
void collectAndSendChunk() {
  for (uint8_t i = 0; i < CHUNK_SIZE; i++) {
    samples[i] = analogRead(ANALOG_PIN);
    applyDelay();
  }

  Serial.print(F("D:"));
  for (uint8_t i = 0; i < CHUNK_SIZE; i++) {
    Serial.print(samples[i]);
    if (i < CHUNK_SIZE - 1) Serial.print(',');
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
  // Проверяем входящие команды (неблокирующе).
  // Делаем это перед КАЖДЫМ чанком (а не раз в 512 сэмплов, как раньше) —
  // так изменение задержки/диапазона из веб-интерфейса подхватывается
  // почти мгновенно, а не с задержкой в целый кадр.
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() > 0) parseCommand(cmd);
  }

  collectAndSendChunk();
}
