#include <Arduino.h>
#include "Wire.h"
#include "GyverBME280.h"
#include "MQUnifiedsensor.h"

#define GREEN_LED_PIN 2
#define YELLOW_LED_PIN 3
#define RED_LED_PIN 4
#define BUZZER_PIN 5
#define CO_SENSOR_PIN A7
#define GAS_SENSOR_PIN A6
#define LIGHT_SENSOR_PIN A3

/* ========== Пороговые значения ========== */
// Температура (°C)

#define TEMP_LOW_CRITICAL_VALUE       18    // Критический низкий порог температуры
#define TEMP_LOW_WARNING_VALUE        20    // Предупреждающий низкий порог температуры
#define TEMP_HIGH_WARNING_VALUE       25    // Предупреждающий высокий порог температуры
#define TEMP_HIGH_CRITICAL_VALUE      27    // Критический высокий порог температуры

// Влажность (%)

#define HUM_LOW_CRITICAL_VALUE        30    // Критический низкий порог влажности
#define HUM_LOW_WARNING_VALUE         40    // Предупреждающий низкий порог влажности
#define HUM_HIGH_WARNING_VALUE        60    // Предупреждающий высокий порог влажности
#define HUM_HIGH_CRITICAL_VALUE       70    // Критический высокий порог влажности

// Освещённость ()

#define LIGHT_LOW_CRITICAL_VALUE      100   // Критический низкий порог освещённости
#define LIGHT_LOW_WARNING_VALUE       200   // Предупреждающий низкий порог освещённости
#define LIGHT_HIGH_WARNING_VALUE      800   // Предупреждающий высокий порог освещённости
#define LIGHT_HIGH_CRITICAL_VALUE     1000  // Критический высокий порог освещённости

// Горючие газы в воздухе ()

#define GAS_CRITICAL_VALUE            600   // Уровень срабатывания тревоги по горючим газам

// CO в воздухе ()

#define CO_EMERGENCY_VALUE            150   // Уровень срабатывания тревоги по угарному газу

/* ========== Пороговые значения ========== */

GyverBME280 bme;


/**
 * Значения полученные с датчиков
 */
struct sensors_values {
    float temp; // Температура
    float hum; // Влажность
    float light; // Освещённость
    float gas; // Горючие газы
    float CO; // Угарный газ
} measured_values{};

/**
 * Реакция системы
 */
enum class Reaction {
    // Всё в норме
    NORMAL,
    // Лёгкое отклонение одного параметра
    WARNING_SINGLE,
    // Несколько лёгких отклонений
    WARNING_MULTIPLE,
    // Критическое отклонение температуры/влажности
    CRITICAL_TEMP_HUMID,
    // Опасный уровень горючих газов
    CRITICAL_GAS,
    // Обнаружен угарный газ (максимальная тревога)
    EMERGENCY_CO
} current_reaction = Reaction::WARNING_MULTIPLE;

sensors_values get_values();
Reaction get_reaction(sensors_values values);
void update_indicators(Reaction reaction);

void setup() {
    Serial.begin(115200);
    bme.begin();

    Serial.println("Start!");

    pinMode(GREEN_LED_PIN, OUTPUT);
    pinMode(YELLOW_LED_PIN, OUTPUT);
    pinMode(RED_LED_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);

    pinMode(CO_SENSOR_PIN, INPUT);
    pinMode(GAS_SENSOR_PIN, INPUT);
    pinMode(LIGHT_SENSOR_PIN, INPUT);

    pinMode(LED_BUILTIN, OUTPUT);

    tone(BUZZER_PIN, 2000, 20);
    delay(100);
    tone(BUZZER_PIN, 2000, 20);
}

void loop() {

    // Чтение данных и реакция

    static uint32_t read_sensors_tmr;
    if (millis() - read_sensors_tmr >= 1000) {
        read_sensors_tmr = millis();
        measured_values = get_values();
        current_reaction = get_reaction(measured_values);
    }
    update_indicators(current_reaction);


    static unsigned long serial_log_tmr;
    if (millis() - serial_log_tmr >= 1000) {
        serial_log_tmr = millis();
        Serial.println("==============================");
        Serial.print("Temperature: "); Serial.println(bme.readTemperature());
        Serial.print("Humidity: "); Serial.println(bme.readHumidity());
        Serial.println("==============================");
        Serial.print("CO: "); Serial.println(analogRead(CO_SENSOR_PIN));
        Serial.print("GAS: ");  Serial.println(analogRead(GAS_SENSOR_PIN));
        Serial.println("==============================");
        Serial.print("Light: "); Serial.println(analogRead(LIGHT_SENSOR_PIN));
        Serial.println("==============================");
        Serial.println("");
    }


}

/**
 * Получение данных с датчиков
 * @return sensors_values values
 */
sensors_values get_values() {
    sensors_values values{};
    values.temp = bme.readTemperature();
    values.hum = bme.readHumidity();
    values.light = analogRead(LIGHT_SENSOR_PIN);
    values.gas = analogRead(GAS_SENSOR_PIN);
    values.CO = analogRead(CO_SENSOR_PIN);
    return values;
}

/**
 * На основе показателей датчиков возвращает требуемую реакцию
 * @param sensors_values values
 * @return Reaction reaction
 */
Reaction get_reaction(sensors_values values) {
    struct {
        struct {
            bool temp = false;
            bool hum = false;
            bool light = false;
        } warning;
        struct {
            bool temp = false;
            bool hum = false;
            bool light = false;
            bool gas = false;
            bool CO = false;
        } critical;
    } flags{};

    if (values.CO >= CO_EMERGENCY_VALUE)
        flags.critical.CO = true;
    if (values.gas >= GAS_CRITICAL_VALUE)
        flags.critical.gas = true;
    if (values.temp < TEMP_LOW_CRITICAL_VALUE or values.temp > TEMP_HIGH_CRITICAL_VALUE)
        flags.critical.temp = true;
    if (values.hum < HUM_LOW_CRITICAL_VALUE or values.hum > HUM_HIGH_CRITICAL_VALUE)
        flags.critical.hum = true;
    if (values.light < LIGHT_LOW_CRITICAL_VALUE or values.light > LIGHT_HIGH_CRITICAL_VALUE)
        flags.critical.light = true;
    if (values.temp < TEMP_LOW_WARNING_VALUE or values.temp > TEMP_HIGH_WARNING_VALUE)
        flags.warning.temp = true;
    if (values.hum < HUM_LOW_WARNING_VALUE or values.hum > HUM_HIGH_WARNING_VALUE)
        flags.warning.hum = true;
    if (values.light < LIGHT_LOW_WARNING_VALUE or values.light > LIGHT_HIGH_WARNING_VALUE)
        flags.warning.light = true;

    if (flags.critical.CO)
        return Reaction::EMERGENCY_CO;
    if (flags.critical.gas)
        return Reaction::CRITICAL_GAS;
    if (flags.critical.temp or flags.critical.hum)
        return Reaction::CRITICAL_TEMP_HUMID;

    uint8_t warning_count = 0;
    if (flags.warning.temp)
        warning_count++;
    if (flags.warning.hum)
        warning_count++;
    if (flags.warning.light)
        warning_count++;
    if (warning_count > 1 )
        return Reaction::WARNING_MULTIPLE;
    if (warning_count > 0 )
        return Reaction::WARNING_SINGLE;

    return Reaction::NORMAL;
}

/**
 * Реализует требуемую реакцию
 * @param Reaction reaction
 */
void update_indicators(Reaction reaction) {
    switch (current_reaction) {
        case Reaction::NORMAL:
            digitalWrite(GREEN_LED_PIN, 1);
            digitalWrite(YELLOW_LED_PIN, 0);
            digitalWrite(RED_LED_PIN, 0);
            noTone(BUZZER_PIN);
            break;
        case Reaction::WARNING_SINGLE:
            digitalWrite(GREEN_LED_PIN, 0);
            digitalWrite(YELLOW_LED_PIN, 1);
            digitalWrite(RED_LED_PIN, 0);
            noTone(BUZZER_PIN);
            break;
        case Reaction::WARNING_MULTIPLE:
            digitalWrite(GREEN_LED_PIN, 0);
            digitalWrite(YELLOW_LED_PIN, millis() % 1000 < 500);
            digitalWrite(RED_LED_PIN, 0);
            noTone(BUZZER_PIN);
            break;
        case Reaction::CRITICAL_TEMP_HUMID:
            digitalWrite(GREEN_LED_PIN, 0);
            digitalWrite(YELLOW_LED_PIN, 0);
            digitalWrite(RED_LED_PIN, millis() % 2000 < 1000);
            noTone(BUZZER_PIN);
            break;
        case Reaction::CRITICAL_GAS:
            digitalWrite(GREEN_LED_PIN, 0);
            digitalWrite(YELLOW_LED_PIN, 0);
            digitalWrite(RED_LED_PIN, 1);
            if (millis() % 1000 < 500) {
                tone(BUZZER_PIN, 1500);
            } else {
                noTone(BUZZER_PIN);
            }
            break;
        case Reaction::EMERGENCY_CO:
            digitalWrite(GREEN_LED_PIN, 0);
            digitalWrite(YELLOW_LED_PIN, 0);
            digitalWrite(RED_LED_PIN, millis() % 1000 < 500);
            if (millis() % 400 < 200) {
                tone(BUZZER_PIN, 1500);
            } else {
                tone(BUZZER_PIN, 2000);
            }
            break;
    }
}