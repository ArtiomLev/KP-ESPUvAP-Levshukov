#include <Arduino.h>
#include "Wire.h"
#include "GyverBME280.h"

#define GREEN_LED_PIN 2
#define YELLOW_LED_PIN 3
#define RED_LED_PIN 4
#define BUZZER_PIN 5
#define CO_SENSOR_PIN A7
#define GAS_SENSOR_PIN A6
#define LIGHT_SENSOR_PIN A3

GyverBME280 bme;

enum class colors {
    RED,
    YELLOW,
    GREEN
} color = colors::GREEN;

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

    static unsigned long tmr1;
    if (millis() - tmr1 >= 1000) {
        tmr1 = millis();
        if (color == colors::GREEN) {
            color = colors::YELLOW;
            digitalWrite(RED_LED_PIN, 0);
            digitalWrite(YELLOW_LED_PIN, 1);
            digitalWrite(GREEN_LED_PIN, 0);
        } else if (color == colors::YELLOW) {
            color = colors::RED;
            digitalWrite(RED_LED_PIN, 1);
            digitalWrite(YELLOW_LED_PIN, 0);
            digitalWrite(GREEN_LED_PIN, 0);
        } else if (color == colors::RED) {
            color = colors::GREEN;
            digitalWrite(RED_LED_PIN, 0);
            digitalWrite(YELLOW_LED_PIN, 0);
            digitalWrite(GREEN_LED_PIN, 1);
        }
    }

    static bool blinkFlag;
    static unsigned long tmr2;
    if (millis() - tmr2 >= 100) {
        tmr2 = millis();
        blinkFlag = !blinkFlag;
        digitalWrite(LED_BUILTIN, blinkFlag);
    }

    static unsigned long tmr3;
    if (millis() - tmr3 >= 1000) {
        tmr3 = millis();
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