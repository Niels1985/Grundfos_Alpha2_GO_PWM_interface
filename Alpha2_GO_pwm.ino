// Niels Derksen 2026
// DEBUG info: PIN5 = (tegenover bolletje) Serieel TTL UART uit 9600 boud.
// Gebruik een TTL UART adapter op 5V voor uitlezing.
// Uit te lezen: Stand potmeter, PWM uitsturing, status digitaal in.


#include <Arduino.h>
#include <SoftwareSerial.h>

// ===== Pin mapping (ATtiny85 DIP-8) =====
// 1 = PB5/RESET  (niet gebruiken)
// 2 = PB3/ADC3   (vrij)
// 3 = PB4/ADC2   -> Potmeter wiper (ANALOG IN)
// 4 = GND
// 5 = PB0        -> Debug TX (SoftwareSerial TX-only)
// 6 = PB1/OC1A   -> PWM OUT naar pomp IN
// 7 = PB2        -> Digitale ingang (INPUT_PULLUP)
// 8 = VCC 5V

const uint8_t PIN_POT         = A2; // PB4, fysiek pin 3
const uint8_t PIN_PWM_OUT     = 1;  // PB1/OC1A, fysiek pin 6
const uint8_t PIN_DIGITAL_IN  = 2;  // PB2, fysiek pin 7
const uint8_t PIN_DBG_TX      = 0;  // PB0, fysiek pin 5

// Grundfos profiel A:
// 10..84% regelen, 95..100% standby/stop -> wij kiezen 98%
static const uint8_t DUTY_STANDBY = 98;
static const uint8_t DUTY_MIN     = 10;
static const uint8_t DUTY_MAX     = 84;

// 8 MHz -> 1 kHz: 8e6 / (64*(124+1)) = 1000 Hz
static const uint8_t TOP = 124;

// SoftwareSerial: RX pin is "dummy" (we gebruiken alleen TX)
SoftwareSerial dbg(255, PIN_DBG_TX); // RX=255 (niet gebruikt), TX=PB0

static inline uint8_t clamp_u8(int v, int lo, int hi) {
  if (v < lo) return (uint8_t)lo;
  if (v > hi) return (uint8_t)hi;
  return (uint8_t)v;
}

static void pwm_init_1kHz_OC1A() {
  pinMode(PIN_PWM_OUT, OUTPUT);

  TCCR1 = 0;
  GTCCR = 0;

  OCR1C = TOP;   // TOP bepaalt frequentie
  OCR1A = 0;     // duty

  // PWM op OC1A + prescaler /64
  TCCR1 = (1 << PWM1A) | (1 << COM1A1) | (1 << CS12) | (1 << CS11);
}

static void pwm_set_duty_percent(uint8_t dutyPercent) {
  dutyPercent = clamp_u8(dutyPercent, 0, 100);

  uint16_t ocr = (uint16_t)((uint32_t)dutyPercent * (uint32_t)(TOP + 1) + 50) / 100;
  if (ocr > TOP) ocr = TOP;
  OCR1A = (uint8_t)ocr;
}

static uint16_t read_pot_filtered() {
  uint32_t sum = 0;
  for (uint8_t i = 0; i < 8; i++) {
    sum += (uint16_t)analogRead(PIN_POT);
    delay(2);
  }
  return (uint16_t)(sum / 8);
}

void setup() {
  pinMode(PIN_DIGITAL_IN, INPUT_PULLUP); // los = HIGH, naar GND = LOW (actief)
  analogReference(DEFAULT);

  pwm_init_1kHz_OC1A();
  pwm_set_duty_percent(DUTY_STANDBY); // veilig starten: standby/stop

  dbg.begin(9600);
  dbg.println(F("ATtiny85 Alpha2GO PWM start"));
}

void loop() {
  // Inverted logic:
  // LOW (naar GND)  => regelmodus
  // HIGH (los)      => standby/stop
  const bool regelmodus = (digitalRead(PIN_DIGITAL_IN) == LOW);

  uint16_t adc = read_pot_filtered();           // 0..1023
  uint8_t potPercent = (uint8_t)((uint32_t)adc * 100UL / 1023UL);

  uint8_t dutyOut;

  if (!regelmodus) {
    dutyOut = DUTY_STANDBY;
  } else {
    int duty = DUTY_MIN + (int)((uint32_t)adc * (DUTY_MAX - DUTY_MIN) / 1023UL);
    dutyOut = clamp_u8(duty, DUTY_MIN, DUTY_MAX);
  }

  pwm_set_duty_percent(dutyOut);

  // Debug elke 3 seconden
  static uint32_t tLast = 0;
  uint32_t now = millis();
  if (now - tLast >= 3000UL) {
    tLast = now;

    dbg.print(F("IN="));
    dbg.print(regelmodus ? F("LOW(regel)") : F("HIGH(standby)"));

    dbg.print(F("  POT="));
    dbg.print(potPercent);
    dbg.print(F("%"));

    dbg.print(F("  PWM="));
    dbg.print(dutyOut);
    dbg.println(F("%"));
  }

  delay(20);
}
