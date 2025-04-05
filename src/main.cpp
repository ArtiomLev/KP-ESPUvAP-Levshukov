#include <Arduino.h>
#include "GyverBME280.h"
#include "MQUnifiedsensor.h"

/* =============== Настройки ============== */

#define SENSORS_PREHEAT_TIME        90000   // Длительность нагрева датчиков MQ

#define DEBUG                       1       // Отладка

#if DEBUG == 1

#define SERIAL_LOG_OUTPUT           1       // Вывод логов в Serial
#define SERIAL_LOG_PERIOD           1000    // Период отправки логов в Serial
#define LOG_MEASURED_VALUES         1       // Логировать измеренные значения
#define LOG_CURRENT_REACTION        1       // Логировать текущую реакцию

#define DISABLE_BUZZER              0       // Отключить зуммер

#define DISABLE_REACTION            0       // Отключить реакцию на измеренные значения

#define FAST_PREHEAT                1       // Уменьшение времени преднагрева датчиков для упрощения отладки
#define FAST_SENSORS_PREHEAT_TIME   1000    // Длительность быстрого преднагрева датчиков MQ

#if FAST_PREHEAT == 1
#undef SENSORS_PREHEAT_TIME
#define SENSORS_PREHEAT_TIME FAST_SENSORS_PREHEAT_TIME
#endif
#endif

/* =============== Настройки ============== */


/* ================= Пины ================= */
/*                > Выходы <                */

#define GREEN_LED_PIN               2       // Зелёный светодиод индикатора
#define YELLOW_LED_PIN              3       // Жёлтый светодиод индикатора
#define RED_LED_PIN                 4       // Красный светодиод индикатора

#define BUZZER_PIN                  5       // Зуммер

/*                > Входы <                 */

#define CO_SENSOR_PIN               A7      // Датчик угарного газа
#define GAS_SENSOR_PIN              A6      // Датчик горючих газов
#define LIGHT_SENSOR_PIN            A3      // Датчик освещённости

/* ================= Пины ================= */


/* ========== Пороговые значения ========== */
// Температура (°C)

#define TEMP_LOW_CRITICAL_VALUE     17      // Критический низкий порог температуры
#define TEMP_LOW_WARNING_VALUE      19      // Предупреждающий низкий порог температуры
#define TEMP_HIGH_WARNING_VALUE     26      // Предупреждающий высокий порог температуры
#define TEMP_HIGH_CRITICAL_VALUE    29      // Критический высокий порог температуры

// Влажность (%)

#define HUM_LOW_CRITICAL_VALUE      25      // Критический низкий порог влажности
#define HUM_LOW_WARNING_VALUE       30      // Предупреждающий низкий порог влажности
#define HUM_HIGH_WARNING_VALUE      55      // Предупреждающий высокий порог влажности
#define HUM_HIGH_CRITICAL_VALUE     60      // Критический высокий порог влажности

// Освещённость (лк)

#define LIGHT_LOW_CRITICAL_VALUE    200     // Критический низкий порог освещённости
#define LIGHT_LOW_WARNING_VALUE     290     // Предупреждающий низкий порог освещённости
#define LIGHT_HIGH_WARNING_VALUE    600     // Предупреждающий высокий порог освещённости
#define LIGHT_HIGH_CRITICAL_VALUE   900     // Критический высокий порог освещённости

// Горючие газы в воздухе (ppm)

#define GAS_CRITICAL_VALUE          20000     // Уровень срабатывания тревоги по горючим газам

// CO в воздухе (ppm)

#define CO_EMERGENCY_VALUE          7000     // Уровень срабатывания тревоги по угарному газу

/* ========== Пороговые значения ========== */


/* === Инициализация объектов библиотек === */

GyverBME280 bme;
MQUnifiedsensor MQ5("Arduino Nano", 5, 10, GAS_SENSOR_PIN, "MQ-5");
MQUnifiedsensor MQ7("Arduino Nano", 5, 10, CO_SENSOR_PIN, "MQ-7");

/* === Инициализация объектов библиотек === */


/* ========= Глобальные переменные ======== */

/**
 * Значения полученные с датчиков
 */
struct sensors_values {
    float temp;     // Температура
    float hum;      // Влажность
    float light;    // Освещённость
    float gas;      // Горючие газы
    float CO;       // Угарный газ
    struct {
        //float light;
        float gas;
        float CO;
    } raw;
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

/* ========= Глобальные переменные ======== */


/* ========== Определение функций ========= */

sensors_values get_values();
Reaction get_reaction(sensors_values values);
void update_indicators(Reaction reaction);
void sensors_serial_log(sensors_values values);

/* ========== Определение функций ========= */

void setup() {

    /* Инициализация пинов */

#if DISABLE_REACTION != 1
    pinMode(GREEN_LED_PIN, OUTPUT);
    pinMode(YELLOW_LED_PIN, OUTPUT);
    pinMode(RED_LED_PIN, OUTPUT);
#if DISABLE_BUZZER != 1
    pinMode(BUZZER_PIN, OUTPUT);
#endif
#endif

    pinMode(LIGHT_SENSOR_PIN, INPUT);

    pinMode(LED_BUILTIN, OUTPUT);

    /* Функции инициализации */

#if SERIAL_LOG_OUTPUT == 1
    Serial.begin(115200);
    Serial.println("Start!\n");
#endif

    bme.begin();

    // Инициализация датчика MQ-5
    MQ5.setRegressionMethod(1);
    MQ5.setA(1163.8);
    MQ5.setB(-3.874);
    MQ5.init();
    // Инициализация датчика MQ-7
    MQ7.setRegressionMethod(1);
    MQ7.setA(99.042);
    MQ7.setB(-1.518);
    MQ7.init();

    /* Сигнал при включении */
#if DISABLE_BUZZER != 1
    tone(BUZZER_PIN, 2000, 50);
#endif
    /* Ожидание нагрева датчиков */

    uint32_t sensors_preheat_start_time = millis();
    while (millis() - sensors_preheat_start_time < SENSORS_PREHEAT_TIME) {
        digitalWrite(GREEN_LED_PIN, (millis() % 900 > 0) and (millis() % 900 < 300));
        digitalWrite(YELLOW_LED_PIN, (millis() % 900 > 300) and (millis() % 900 < 600));
        digitalWrite(RED_LED_PIN, (millis() % 900 > 600) and (millis() % 900 < 900));
    }
    digitalWrite(GREEN_LED_PIN, 0);
    digitalWrite(YELLOW_LED_PIN, 0);
    digitalWrite(RED_LED_PIN, 0);
#if DISABLE_BUZZER != 1
    tone(BUZZER_PIN, 2000, 200);
#endif

    /* Калибровка датчиков */

    float calcR0;

    // Калибровка MQ-5

    calcR0 = 0;
    for (uint8_t i = 1 ; i <= 10 ; i++ ) {
        MQ5.update();
        calcR0 += MQ5.calibrate(6.5);
    }
    MQ5.setR0(calcR0/10);

    // Калибровка MQ-7

    calcR0 = 0;
    for (uint8_t i = 1 ; i <= 10 ; i++ ) {
        MQ7.update();
        calcR0 += MQ7.calibrate(6.5);
    }
    MQ7.setR0(calcR0/10);

}

void loop() {

    /* Чтение данных и реакция */

    static uint32_t read_sensors_tmr;
    if (millis() - read_sensors_tmr >= 1000) {
        read_sensors_tmr = millis();
        measured_values = get_values();
        current_reaction = get_reaction(measured_values);
    }
#if DISABLE_REACTION != 1
    update_indicators(current_reaction);
#endif


    /* Логирование показателей датчиков в Serial */
#if SERIAL_LOG_OUTPUT == 1
    static unsigned long serial_log_tmr;
    if (millis() - serial_log_tmr >= SERIAL_LOG_PERIOD) {
        serial_log_tmr = millis();

#if LOG_CURRENT_REACTION == 1

#endif
    }
#endif

}

/**
 * Вывод в Serial значений с датчиков
 * @param values
 */
void sensors_serial_log(sensors_values values) {
        Serial.println("==============================");
        Serial.print("Temperature:\t"); Serial.println(values.temp);
        Serial.print("Humidity:\t"); Serial.println(values.hum);
        Serial.println("==============================");
        Serial.print("Raw CO:\t"); Serial.println(values.raw.CO);
        Serial.print("CO:\t"); Serial.println(values.CO);
        Serial.print("Raw GAS:\t");  Serial.println(values.raw.gas);
        Serial.print("GAS:\t");  Serial.println(values.gas);
        Serial.println("==============================");
        Serial.print("Light:\t"); Serial.println(values.light);
        Serial.println("==============================");
        Serial.println("");
}

/**
 * Получение данных с датчиков
 * @return sensors_values values
 */
sensors_values get_values() {
    sensors_values values{};

    // Получение температуры и влажности
    values.temp = bme.readTemperature();
    values.hum = bme.readHumidity();
    values.light = analogRead(LIGHT_SENSOR_PIN);

    // Получение концентрации горючих газов
    MQ5.update();
    values.gas = MQ5.readSensor();
    values.raw.gas = analogRead(GAS_SENSOR_PIN);

    // Получение концентрации угарного газа
    MQ7.update();
    values.CO = MQ7.readSensor();
    values.raw.CO = analogRead(CO_SENSOR_PIN);

    // Логирование измеренных значений
#if LOG_MEASURED_VALUES == 1 && SERIAL_LOG_OUTPUT == 1
    sensors_serial_log(values);
#endif
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

    // Сверка показателей

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

    // Возврат реакции

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
    switch (reaction) {
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
#if DISABLE_BUZZER != 1
            if (millis() % 1000 < 500) {
                tone(BUZZER_PIN, 1500);
            } else {
                noTone(BUZZER_PIN);
            }
#endif
            break;
        case Reaction::EMERGENCY_CO:
            digitalWrite(GREEN_LED_PIN, 0);
            digitalWrite(YELLOW_LED_PIN, 0);
            digitalWrite(RED_LED_PIN, millis() % 1000 < 500);
#if DISABLE_BUZZER != 1
            if (millis() % 400 < 200) {
                tone(BUZZER_PIN, 1500);
            } else {
                tone(BUZZER_PIN, 2000);
            }
#endif
            break;
    }
}