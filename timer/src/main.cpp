#include <Arduino.h>
#include <TM1637TinyDisplay.h>


#define BTN_10MIN   2
#define BTN_1MIN    3
#define BTN_10SEC   4
#define BTN_OK    5


#define BUZZER_PIN  8

// дисплей TM1637
#define CLK_PIN   9
#define DIO_PIN   10


#define DEBOUNCE  80
#define MAX_TIME  (99 * 60 + 59)   // максимум 99:59


TM1637TinyDisplay display(CLK_PIN, DIO_PIN);


uint32_t setSeconds = 0; // набранное время

uint32_t leftSeconds = 0; // оставшееся время


bool isRunning  = false;   // идёт отсчёт
bool isFinished = false;   // время вышло пищим и мигаем


uint32_t lastTickMillis = 0;


uint32_t lastBlinkMillis = 0;
bool blinkState = true;   // true = видно false = погашено
bool colonState  = true;   // двоеточие вкл выкл

// антидребезг
bool lastRead10Min = false;
bool stable10Min   = false;
uint32_t time10Min  = 0;

bool lastRead1Min  = false;
bool stable1Min = false;
uint32_t time1Min = 0;

bool lastRead10Sec = false;
bool stable10Sec = false;
uint32_t time10Sec = 0;

bool lastReadOk = false;
bool stableOk = false;
uint32_t timeOk = 0;

// мелодия на бузере
uint16_t melody[]  = { 880, 0, 880, 0, 880, 0, 1175 };
uint8_t  melodyPos = 0;
uint32_t lastNoteMillis = 0;


// универсальная проверка нажатия кнопки, нежели 4 раза писать для 4 кнопок
bool checkButton(uint8_t pin, bool &lastRead, bool &stable, uint32_t &lastChangeMs) {
    bool reading = !digitalRead(pin);   // нажата = low, значит инвертируем
    uint32_t now = millis();

    // если только что изменилось, то запоминаем время
    if (reading != lastRead) {
        lastRead = reading;
        lastChangeMs = now;
    }

    if ((now - lastChangeMs) > DEBOUNCE) {
        if (reading != stable) {
            stable = reading;
            if (reading == true) {
                return true;   // момент нажатия
            }
        }
    }
    return false;
}



void showTime(uint32_t totalSeconds) {
    // если время вышло и сейчас "темная" фаза мигания, то гасим экран
    if (isFinished == true && blinkState == false) {
        display.clear();
        return;
    }

    // разбиваем на минуты и секунды
    uint8_t minutes = totalSeconds / 60;
    uint8_t seconds = totalSeconds % 60;

    // готовим 4 цифры
    uint8_t digits[4];
    digits[0] = display.encodeDigit(minutes / 10);
    digits[1] = display.encodeDigit(minutes % 10);
    digits[2] = display.encodeDigit(seconds / 10);
    digits[3] = display.encodeDigit(seconds % 10);

    if (isRunning == false || colonState == true) {
        digits[1] = digits[1] | 0x80; // добавляем двоеточие, при наборе горит, при отсчете мигает.
    }

    display.setSegments(digits);
}


void startMelody() {
    melodyPos = 0;
    lastNoteMillis  = millis();
    tone(BUZZER_PIN, melody[0]);   // сразу играем первую ноту
}


void stopMelody() {
    noTone(BUZZER_PIN);
}


void playMelody() {
    uint32_t now = millis();

    // каждые 200 мс следующая нота
    if (now - lastNoteMillis >= 200) {
        lastNoteMillis = now;

        // идем по массиву по кругу
        melodyPos = melodyPos + 1;
        if (melodyPos >= 7) {
            melodyPos = 0;
        }

        uint16_t note = melody[melodyPos];
        if (note == 0) {
            noTone(BUZZER_PIN);   // пауза
        } else {
            tone(BUZZER_PIN, note);
        }
    }
}


void setup() {
    Serial.begin(115200);

    pinMode(BTN_10MIN, INPUT_PULLUP);
    pinMode(BTN_1MIN, INPUT_PULLUP);
    pinMode(BTN_10SEC, INPUT_PULLUP);
    pinMode(BTN_OK, INPUT_PULLUP);

    pinMode(BUZZER_PIN, OUTPUT);


    display.begin();
    display.setBrightness(5);
    display.clear();

    // покажет нули вначале
    showTime(0);
}


void loop() {
    uint32_t now = millis();

    bool pressed10Min = checkButton(BTN_10MIN, lastRead10Min, stable10Min, time10Min);
    bool pressed1Min = checkButton(BTN_1MIN, lastRead1Min, stable1Min, time1Min);
    bool pressed10Sec = checkButton(BTN_10SEC, lastRead10Sec, stable10Sec, time10Sec);
    bool pressedOk = checkButton(BTN_OK, lastReadOk, stableOk, timeOk);

    // логика таймера

    // если сейчас НЕ идет отсчет и НЕ мигает финиш,то набираем время
    if (isRunning == false && isFinished == false) {

        if (pressed10Min == true) {
            setSeconds = setSeconds + (10 * 60);
            if (setSeconds > MAX_TIME) {
                setSeconds = MAX_TIME;
            }
            showTime(setSeconds);
        }

        if (pressed1Min == true) {
            setSeconds = setSeconds + 60;
            if (setSeconds > MAX_TIME) {
                setSeconds = MAX_TIME;
            }
            showTime(setSeconds);
        }


        if (pressed10Sec == true) {
            setSeconds = setSeconds + 10;
            if (setSeconds > MAX_TIME) {
                setSeconds = MAX_TIME;
            }
            showTime(setSeconds);
        }

        // кнопка OK - старт
        if (pressedOk == true) {
            if (setSeconds > 0) {
                leftSeconds = setSeconds;
                lastTickMillis = millis();
                colonState = true;
                isRunning = true;
                showTime(leftSeconds);
            }
        }
    }

    // если идет обратный отсчет
    if (isRunning == true) {
        // вычитаем секунду
        if (now - lastTickMillis >= 1000) {
            lastTickMillis = lastTickMillis + 1000;

            if (leftSeconds > 0) {
                leftSeconds = leftSeconds - 1;
            }

            // когда время вышло
            if (leftSeconds == 0) {
                isRunning = false;
                isFinished = true;
                blinkState = true;
                lastBlinkMillis = now;
                startMelody();
            }

            showTime(leftSeconds);
        }

        // двоеточие мигает раз в 500мс
        if (now - lastBlinkMillis >= 500) {
            lastBlinkMillis = now;
            if (colonState == true) {
                colonState = false;
            } 
            else {
                colonState = true;
            }
            showTime(leftSeconds);
        }

        // если нажали OK, то отменяем отсчет и возвращаемся в набор времени
        if (pressedOk == true) {
            isRunning = false;
            colonState = true;
            showTime(setSeconds);
        }
    }

    // когда время закончилось, пищим и мигаем
    if (isFinished == true) {

        playMelody();

        if (now - lastBlinkMillis >= 500) {
            lastBlinkMillis = now;
            if (blinkState == true) {
                blinkState = false;
            }
            else {
                blinkState = true;
            }
            showTime(0);
        }

        // при нажатии ОК, сбрасываем
        if (pressedOk == true) {
            stopMelody();
            setSeconds = 0;
            leftSeconds = 0;
            isFinished = false;
            blinkState = true;
            colonState = true;
            showTime(0);
        }
    }
}