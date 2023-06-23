#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <ESP32Servo.h>
#include <SPI.h>
#include <RTClib.h>

#define SEALEVELPRESSURE_HPA (1013.25)

const char *ssid = "Chart";
const char *pass = "Chart123";

const char *mqtt_server = "54.169.143.15";
const char *mqtt_user = "Sahaphop";
const char *mqtt_password = "123456";

unsigned long period = 1000; // ระยะเวลาที่ต้องการรอ
unsigned long last_time = 0; // ประกาศตัวแปรเป็น global เพื่อเก็บค่าไว้ไม่ให้ reset จากการวนloop

bool status;

#define detectorPin 4
#define buzzer 13
#define ldr 32
#define led 12
#define motor1Pin1 2
#define motor1Pin2 15
#define enable1Pin 26
#define Relay1 27
#define levelPin1 36
#define levelPin2 39
#define levelPin3 34
#define levelPin4 35
#define BME_SCK 18
#define BME_MISO 19
#define BME_MOSI 23
#define BME_CS 5
#define SEALEVELPRESSURE_HPA (1013.25)

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
RTC_DS3231 rtc;
Adafruit_BME280 bme(BME_CS, BME_MOSI, BME_MISO, BME_SCK); // software SPI
WiFiClient espClient;
PubSubClient client(espClient);
Servo myservo;

void setupWiFi()
{
    delay(100);
    Serial.println("Connecting to");
    Serial.println(ssid);

    WiFi.begin(ssid, pass);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(100);
        Serial.print("-");
    }

    Serial.println("Connected to");
    Serial.println(ssid);
}

void reconnect()
{
    while (!client.connected())
    {
        Serial.println("Connected to");
        Serial.println(mqtt_server);
        if (client.connect("ESP32Client", mqtt_user, mqtt_password))
        {
            Serial.println("Connected to");
            Serial.println(mqtt_server);
        }
        else
        {
            Serial.println("Trying Connected Again");
        }
    }
}

void setupbme280()
{
    status = bme.begin();
    if (!status)
    {
        Serial.println("Could not find a valid BME280 sensor, check wiring!");
        while (1)
            ;
    }
}

void setuptimer()
{
    Wire.begin();
    if (!rtc.begin())
    {
        Serial.println("Couldn't find RTC");
        while (1)
            ;
    }
}

void Actuator_Sensor()
{
    // Sensor_Temp
    float t = bme.readTemperature();
    client.publish("Temp", String(t).c_str());
    // Sensor_Pressure
    float h = bme.readHumidity();
    client.publish("Humidity", String(h).c_str());
    // Sensor_IR
    String IR;
    int d = digitalRead(detectorPin);
    if (d == 1)
    {
        IR = "Detected";
        client.publish("IR", "Detected");
    }
    else
    {
        IR = "Non-Detected";
        client.publish("IR", "Non-Detected");
    }
    // Sensor_LDR
    String Light;
    String Level;
    int val = analogRead(ldr);
    if (val < 1000)
    {
        Light = "Bright";
        client.publish("LDR", "Bright");
    }
    else if (val < 2000)
    {
        Light = "Light";
        client.publish("LDR", "Light");
    }
    else if (val < 3000)
    {
        Light = "Dim";
        client.publish("LDR", "Dim");
    }
    else
    {
        Light = "Dark";
        client.publish("LDR", "Dark");
    }
    // Sensor_Level
    int l1 = analogRead(levelPin1);
    int l2 = analogRead(levelPin2);
    int l3 = analogRead(levelPin3);
    int l4 = analogRead(levelPin4);

    if (l1 < 1000 && l2 >= 1000 && l3 >= 1000 && l4 >= 1000)
    {
        Level = "Level 1";
        client.publish("level", "Level 1");
    }
    else if (l1 < 1000 && l2 < 1000 && l3 >= 1000 && l4 >= 1000)
    {
        Level = "Level 2";
        client.publish("level", "Level 2");
    }
    else if (l1 < 1000 && l2 < 1000 && l3 < 1000 && l4 >= 1000)
    {
        Level = "Level 3";
        client.publish("level", "Level 3");
    }
    else if (l1 < 1000 && l2 < 1000 && l3 < 1000 && l4 < 1000)
    {
        Level = "Level 4";
        client.publish("level", "Level 4");
    }
    else
    {
        Level = "Non-Detected";
        client.publish("level", "Non-Detected");
    }

    //  ตัวแปรเวลา
    //rtc.adjust(DateTime(__DATE__, __TIME__)); // คำสั่งตั้งเวลา
    DateTime now = rtc.now();
    DateTime future(now + TimeSpan(0, 0, 9, 18));
    int year = now.year();
    int month = now.month();
    int day = now.day();
    int hour = now.hour();
    int minute = now.minute();
    int second = now.second();
    String hourminute_string = String(hour) + "." + String(minute);
    float hourminute = hourminute_string.toFloat();
    String dayofweek = daysOfTheWeek[now.dayOfTheWeek()];
    Serial.print(year);
    Serial.print('/');
    Serial.print(month);
    Serial.print('/');
    Serial.print(day);
    Serial.print(' ');
    Serial.print(hour);
    Serial.print(':');
    Serial.print(minute);
    Serial.print(':');
    Serial.print(second);
    Serial.print(' ');
    Serial.print(dayofweek);
    Serial.println(" ");

    HTTPClient http;
    http.begin("http://54.255.145.75:1880/api/actuator-condition");
    int httpCode = http.GET();
    Serial.println(httpCode);
    if (httpCode > 0)
    {
        String payload = http.getString();
        // Serial.println("payload");
        // Serial.println(payload);
        DynamicJsonDocument doc(8000);
        deserializeJson(doc, payload);

        // ตัวแปร ส่วน condition
        // ตัวแปร ส่วน condition temp_led
        String actuator_temp_led = doc["templed"]["0"]["actuatorName"];
        String status_temp_led = doc["templed"]["0"]["status"];
        String condition_temp_led = doc["templed"]["0"]["condition"];
        float value_temp_led = doc["templed"]["0"]["tempValue"];
        String actuator_temp_led2 = doc["templed"]["1"]["actuatorName"];
        String status_temp_led2 = doc["templed"]["1"]["status"];
        String condition_temp_led2 = doc["templed"]["1"]["condition"];
        float value_temp_led2 = doc["templed"]["1"]["tempValue"];
        String actuator_temp_led3 = doc["templed"]["2"]["actuatorName"];
        String status_temp_led3 = doc["templed"]["2"]["status"];
        String condition_temp_led3 = doc["templed"]["2"]["condition"];
        float value_temp_led3 = doc["templed"]["2"]["tempValue"];

        // ตัวแปร ส่วน condition humidity_led
        String actuator_humidity_led = doc["humidityled"]["0"]["actuatorName"];
        String status_humidity_led = doc["humidityled"]["0"]["status"];
        String condition_humidity_led = doc["humidityled"]["0"]["condition"];
        float value_humidity_led = doc["humidityled"]["0"]["humidityValue"];
        String actuator_humidity_led2 = doc["humidityled"]["1"]["actuatorName"];
        String status_humidity_led2 = doc["humidityled"]["1"]["status"];
        String condition_humidity_led2 = doc["humidityled"]["1"]["condition"];
        float value_humidity_led2 = doc["humidityled"]["1"]["humidityValue"];
        String actuator_humidity_led3 = doc["humidityled"]["2"]["actuatorName"];
        String status_humidity_led3 = doc["humidityled"]["2"]["status"];
        String condition_humidity_led3 = doc["humidityled"]["2"]["condition"];
        float value_humidity_led3 = doc["humidityled"]["2"]["humidityValue"];

        // ตัวแปร ส่วน condition irsensor_led
        String actuator_irsensor_led = doc["irsensorled"]["0"]["actuatorName"];
        String status_irsensor_led = doc["irsensorled"]["0"]["status"];
        String value_irsensor_led = doc["irsensorled"]["0"]["irsensorValue"];
        String actuator_irsensor_led2 = doc["irsensorled"]["1"]["actuatorName"];
        String status_irsensor_led2 = doc["irsensorled"]["1"]["status"];
        String value_irsensor_led2 = doc["irsensorled"]["1"]["irsensorValue"];

        // ตัวแปร ส่วน condition level_led
        String actuator_level_led = doc["levelled"]["0"]["actuatorName"];
        String status_level_led = doc["levelled"]["0"]["status"];
        String value_level_led = doc["levelled"]["0"]["levelValue"];
        String actuator_level_led2 = doc["levelled"]["1"]["actuatorName"];
        String status_level_led2 = doc["levelled"]["1"]["status"];
        String value_level_led2 = doc["levelled"]["1"]["levelValue"];
        String actuator_level_led3 = doc["levelled"]["2"]["actuatorName"];
        String status_level_led3 = doc["levelled"]["2"]["status"];
        String value_level_led3 = doc["levelled"]["2"]["levelValue"];
        String actuator_level_led4 = doc["levelled"]["3"]["actuatorName"];
        String status_level_led4 = doc["levelled"]["3"]["status"];
        String value_level_led4 = doc["levelled"]["3"]["levelValue"];
        String actuator_level_led5 = doc["levelled"]["4"]["actuatorName"];
        String status_level_led5 = doc["levelled"]["4"]["status"];
        String value_level_led5 = doc["levelled"]["4"]["levelValue"];

        // ตัวแปร ส่วน condition light_led
        String actuator_light_led = doc["lightled"]["0"]["actuatorName"];
        String status_light_led = doc["lightled"]["0"]["status"];
        String value_light_led = doc["lightled"]["0"]["lightValue"];
        String actuator_light_led2 = doc["lightled"]["1"]["actuatorName"];
        String status_light_led2 = doc["lightled"]["1"]["status"];
        String value_light_led2 = doc["lightled"]["1"]["lightValue"];
        String actuator_light_led3 = doc["lightled"]["2"]["actuatorName"];
        String status_light_led3 = doc["lightled"]["2"]["status"];
        String value_light_led3 = doc["lightled"]["2"]["lightValue"];
        String actuator_light_led4 = doc["lightled"]["3"]["actuatorName"];
        String status_light_led4 = doc["lightled"]["3"]["status"];
        String value_light_led4 = doc["lightled"]["3"]["lightValue"];

        // ตัวแปร ส่วน condition temp_servo
        String actuator_temp_servo = doc["tempservo"]["0"]["actuatorName"];
        String condition_temp_servo = doc["tempservo"]["0"]["condition"];
        float value_temp_servo = doc["tempservo"]["0"]["tempValue"];
        int status_temp_servo = doc["tempservo"]["0"]["degree"];
        String actuator_temp_servo2 = doc["tempservo"]["1"]["actuatorName"];
        String condition_temp_servo2 = doc["tempservo"]["1"]["condition"];
        float value_temp_servo2 = doc["tempservo"]["1"]["tempValue"];
        int status_temp_servo2 = doc["tempservo"]["1"]["degree"];
        String actuator_temp_servo3 = doc["tempservo"]["2"]["actuatorName"];
        String condition_temp_servo3 = doc["tempservo"]["2"]["condition"];
        float value_temp_servo3 = doc["tempservo"]["2"]["tempValue"];
        int status_temp_servo3 = doc["tempservo"]["2"]["degree"];

        // ตัวแปร ส่วน condition humidity_servo
        String actuator_humidity_servo = doc["humidityservo"]["0"]["actuatorName"];
        String condition_humidity_servo = doc["humidityservo"]["0"]["condition"];
        float value_humidity_servo = doc["humidityservo"]["0"]["humidityValue"];
        int status_humidity_servo = doc["humidityservo"]["0"]["degree"];
        String actuator_humidity_servo2 = doc["humidityservo"]["1"]["actuatorName"];
        String condition_humidity_servo2 = doc["humidityservo"]["1"]["condition"];
        float value_humidity_servo2 = doc["humidityservo"]["1"]["humidityValue"];
        int status_humidity_servo2 = doc["humidityservo"]["1"]["degree"];
        String actuator_humidity_servo3 = doc["humidityservo"]["2"]["actuatorName"];
        String condition_humidity_servo3 = doc["humidityservo"]["2"]["condition"];
        float value_humidity_servo3 = doc["humidityservo"]["2"]["humidityValue"];
        int status_humidity_servo3 = doc["humidityservo"]["2"]["degree"];

        // ตัวแปร ส่วน condition irsensor_servo
        String actuator_irsensor_servo = doc["irsensorservo"]["0"]["actuatorName"];
        String value_irsensor_servo = doc["irsensorservo"]["0"]["irsensorValue"];
        int status_irsensor_servo = doc["irsensorservo"]["0"]["degree"];
        String actuator_irsensor_servo2 = doc["irsensorservo"]["1"]["actuatorName"];
        String value_irsensor_servo2 = doc["irsensorservo"]["1"]["irsensorValue"];
        int status_irsensor_servo2 = doc["irsensorservo"]["1"]["degree"];

        // ตัวแปร ส่วน condition level_servo
        String actuator_level_servo = doc["levelservo"]["0"]["actuatorName"];
        String value_level_servo = doc["levelservo"]["0"]["levelValue"];
        int status_level_servo = doc["levelservo"]["0"]["degree"];
        String actuator_level_servo2 = doc["levelservo"]["1"]["actuatorName"];
        String value_level_servo2 = doc["levelservo"]["1"]["levelValue"];
        int status_level_servo2 = doc["levelservo"]["1"]["degree"];
        String actuator_level_servo3 = doc["levelservo"]["2"]["actuatorName"];
        String value_level_servo3 = doc["levelservo"]["2"]["levelValue"];
        int status_level_servo3 = doc["levelservo"]["2"]["degree"];
        String actuator_level_servo4 = doc["levelservo"]["3"]["actuatorName"];
        String value_level_servo4 = doc["levelservo"]["3"]["levelValue"];
        int status_level_servo4 = doc["levelservo"]["3"]["degree"];
        String actuator_level_servo5 = doc["levelservo"]["4"]["actuatorName"];
        String value_level_servo5 = doc["levelservo"]["4"]["levelValue"];
        int status_level_servo5 = doc["levelservo"]["4"]["degree"];

        // ตัวแปร ส่วน condition light_servo
        String actuator_light_servo = doc["lightservo"]["0"]["actuatorName"];
        String value_light_servo = doc["lightservo"]["0"]["lightValue"];
        int status_light_servo = doc["lightservo"]["0"]["degree"];
        String actuator_light_servo2 = doc["levelservo"]["1"]["actuatorName"];
        String value_light_servo2 = doc["lightservo"]["1"]["lightValue"];
        int status_light_servo2 = doc["lightservo"]["1"]["degree"];
        String actuator_light_servo3 = doc["levelservo"]["2"]["actuatorName"];
        String value_light_servo3 = doc["lightservo"]["2"]["lightValue"];
        int status_light_servo3 = doc["lightservo"]["2"]["degree"];
        String actuator_light_servo4 = doc["levelservo"]["3"]["actuatorName"];
        String value_light_servo4 = doc["lightservo"]["3"]["lightValue"];
        int status_light_servo4 = doc["lightservo"]["3"]["degree"];

        // ตัวแปร ส่วน condition temp_buzzer
        String actuator_temp_buzzer = doc["tempbuzzer"]["0"]["actuatorName"];
        String status_temp_buzzer = doc["tempbuzzer"]["0"]["status"];
        String condition_temp_buzzer = doc["tempbuzzer"]["0"]["condition"];
        float value_temp_buzzer = doc["tempbuzzer"]["0"]["tempValue"];
        String actuator_temp_buzzer2 = doc["tempbuzzer"]["1"]["actuatorName"];
        String status_temp_buzzer2 = doc["tempbuzzer"]["1"]["status"];
        String condition_temp_buzzer2 = doc["tempbuzzer"]["1"]["condition"];
        float value_temp_buzzer2 = doc["tempbuzzer"]["1"]["tempValue"];
        String actuator_temp_buzzer3 = doc["tempbuzzer"]["2"]["actuatorName"];
        String status_temp_buzzer3 = doc["tempbuzzer"]["2"]["status"];
        String condition_temp_buzzer3 = doc["tempbuzzer"]["2"]["condition"];
        float value_temp_buzzer3 = doc["tempbuzzer"]["2"]["tempValue"];

        // ตัวแปร ส่วน condition humidity_buzzer
        String actuator_humidity_buzzer = doc["humiditybuzzer"]["0"]["actuatorName"];
        String status_humidity_buzzer = doc["humiditybuzzer"]["0"]["status"];
        String condition_humidity_buzzer = doc["humiditybuzzer"]["0"]["condition"];
        float value_humidity_buzzer = doc["humiditybuzzer"]["0"]["humidityValue"];
        String actuator_humidity_buzzer2 = doc["humiditybuzzer"]["1"]["actuatorName"];
        String status_humidity_buzzer2 = doc["humiditybuzzer"]["1"]["status"];
        String condition_humidity_buzzer2 = doc["humiditybuzzer"]["1"]["condition"];
        float value_humidity_buzzer2 = doc["humiditybuzzer"]["1"]["humidityValue"];
        String actuator_humidity_buzzer3 = doc["humiditybuzzer"]["2"]["actuatorName"];
        String status_humidity_buzzer3 = doc["humiditybuzzer"]["2"]["status"];
        String condition_humidity_buzzer3 = doc["humiditybuzzer"]["2"]["condition"];
        float value_humidity_buzzer3 = doc["humiditybuzzer"]["2"]["humidityValue"];

        // ตัวแปร ส่วน condition irsensor_buzzer
        String actuator_irsensor_buzzer = doc["irsensorbuzzer"]["0"]["actuatorName"];
        String status_irsensor_buzzer = doc["irsensorbuzzer"]["0"]["status"];
        String value_irsensor_buzzer = doc["irsensorbuzzer"]["0"]["irsensorValue"];
        String actuator_irsensor_buzzer2 = doc["irsensorbuzzer"]["1"]["actuatorName"];
        String status_irsensor_buzzer2 = doc["irsensorbuzzer"]["1"]["status"];
        String value_irsensor_buzzer2 = doc["irsensorbuzzer"]["1"]["irsensorValue"];

        // ตัวแปร ส่วน condition level_buzzer
        String actuator_level_buzzer = doc["levelbuzzer"]["0"]["actuatorName"];
        String status_level_buzzer = doc["levelbuzzer"]["0"]["status"];
        String value_level_buzzer = doc["levelbuzzer"]["0"]["levelValue"];
        String actuator_level_buzzer2 = doc["levelbuzzer"]["1"]["actuatorName"];
        String status_level_buzzer2 = doc["levelbuzzer"]["1"]["status"];
        String value_level_buzzer2 = doc["levelbuzzer"]["1"]["levelValue"];
        String actuator_level_buzzer3 = doc["levelbuzzer"]["2"]["actuatorName"];
        String status_level_buzzer3 = doc["levelbuzzer"]["2"]["status"];
        String value_level_buzzer3 = doc["levelbuzzer"]["2"]["levelValue"];
        String actuator_level_buzzer4 = doc["levelbuzzer"]["3"]["actuatorName"];
        String status_level_buzzer4 = doc["levelbuzzer"]["3"]["status"];
        String value_level_buzzer4 = doc["levelbuzzer"]["3"]["levelValue"];
        String actuator_level_buzzer5 = doc["levelbuzzer"]["4"]["actuatorName"];
        String status_level_buzzer5 = doc["levelbuzzer"]["4"]["status"];
        String value_level_buzzer5 = doc["levelbuzzer"]["4"]["levelValue"];

        // ตัวแปร ส่วน condition light_buzzer
        String actuator_light_buzzer = doc["lightbuzzer"]["0"]["actuatorName"];
        String status_light_buzzer = doc["lightbuzzer"]["0"]["status"];
        String value_light_buzzer = doc["lightbuzzer"]["0"]["lightValue"];
        String actuator_light_buzzer2 = doc["lightbuzzer"]["1"]["actuatorName"];
        String status_light_buzzer2 = doc["lightbuzzer"]["1"]["status"];
        String value_light_buzzer2 = doc["lightbuzzer"]["1"]["lightValue"];
        String actuator_light_buzzer3 = doc["lightbuzzer"]["2"]["actuatorName"];
        String status_light_buzzer3 = doc["lightbuzzer"]["2"]["status"];
        String value_light_buzzer3 = doc["lightbuzzer"]["2"]["lightValue"];
        String actuator_light_buzzer4 = doc["lightbuzzer"]["3"]["actuatorName"];
        String status_light_buzzer4 = doc["lightbuzzer"]["3"]["status"];
        String value_light_buzzer4 = doc["lightbuzzer"]["3"]["lightValue"];

        // ตัวแปร ส่วน condition temp_solenoid
        String actuator_temp_solenoid = doc["tempsolenoid"]["0"]["actuatorName"];
        String status_temp_solenoid = doc["tempsolenoid"]["0"]["status"];
        String condition_temp_solenoid = doc["tempsolenoid"]["0"]["condition"];
        float value_temp_solenoid = doc["tempsolenoid"]["0"]["tempValue"];
        String actuator_temp_solenoid2 = doc["tempsolenoid"]["1"]["actuatorName"];
        String status_temp_solenoid2 = doc["tempsolenoid"]["1"]["status"];
        String condition_temp_solenoid2 = doc["tempsolenoid"]["1"]["condition"];
        float value_temp_solenoid2 = doc["tempsolenoid"]["1"]["tempValue"];
        String actuator_temp_solenoid3 = doc["tempsolenoid"]["2"]["actuatorName"];
        String status_temp_solenoid3 = doc["tempsolenoid"]["2"]["status"];
        String condition_temp_solenoid3 = doc["tempsolenoid"]["2"]["condition"];
        float value_temp_solenoid3 = doc["tempsolenoid"]["2"]["tempValue"];

        // ตัวแปร ส่วน condition humidity_solenoid
        String actuator_humidity_solenoid = doc["humiditysolenoid"]["0"]["actuatorName"];
        String status_humidity_solenoid = doc["humiditysolenoid"]["0"]["status"];
        String condition_humidity_solenoid = doc["humiditysolenoid"]["0"]["condition"];
        float value_humidity_solenoid = doc["humiditysolenoid"]["0"]["humidityValue"];
        String actuator_humidity_solenoid2 = doc["humiditysolenoid"]["1"]["actuatorName"];
        String status_humidity_solenoid2 = doc["humiditysolenoid"]["1"]["status"];
        String condition_humidity_solenoid2 = doc["humiditysolenoid"]["1"]["condition"];
        float value_humidity_solenoid2 = doc["humiditysolenoid"]["1"]["humidityValue"];
        String actuator_humidity_solenoid3 = doc["humiditysolenoid"]["2"]["actuatorName"];
        String status_humidity_solenoid3 = doc["humiditysolenoid"]["2"]["status"];
        String condition_humidity_solenoid3 = doc["humiditysolenoid"]["2"]["condition"];
        float value_humidity_solenoid3 = doc["humiditysolenoid"]["2"]["humidityValue"];

        // ตัวแปร ส่วน condition irsensor_solenoid
        String actuator_irsensor_solenoid = doc["irsensorsolenoid"]["0"]["actuatorName"];
        String status_irsensor_solenoid = doc["irsensorsolenoid"]["0"]["status"];
        String value_irsensor_solenoid = doc["irsensorsolenoid"]["0"]["irsensorValue"];
        String actuator_irsensor_solenoid2 = doc["irsensorsolenoid"]["1"]["actuatorName"];
        String status_irsensor_solenoid2 = doc["irsensorsolenoid"]["1"]["status"];
        String value_irsensor_solenoid2 = doc["irsensorsolenoid"]["1"]["irsensorValue"];

        // ตัวแปร ส่วน condition level_solenoid
        String actuator_level_solenoid = doc["levelsolenoid"]["0"]["actuatorName"];
        String status_level_solenoid = doc["levelsolenoid"]["0"]["status"];
        String value_level_solenoid = doc["levelsolenoid"]["0"]["levelValue"];
        String actuator_level_solenoid2 = doc["levelsolenoid"]["1"]["actuatorName"];
        String status_level_solenoid2 = doc["levelsolenoid"]["1"]["status"];
        String value_level_solenoid2 = doc["levelsolenoid"]["1"]["levelValue"];
        String actuator_level_solenoid3 = doc["levelsolenoid"]["2"]["actuatorName"];
        String status_level_solenoid3 = doc["levelsolenoid"]["2"]["status"];
        String value_level_solenoid3 = doc["levelsolenoid"]["2"]["levelValue"];
        String actuator_level_solenoid4 = doc["levelsolenoid"]["3"]["actuatorName"];
        String status_level_solenoid4 = doc["levelsolenoid"]["3"]["status"];
        String value_level_solenoid4 = doc["levelsolenoid"]["3"]["levelValue"];
        String actuator_level_solenoid5 = doc["levelsolenoid"]["4"]["actuatorName"];
        String status_level_solenoid5 = doc["levelsolenoid"]["4"]["status"];
        String value_level_solenoid5 = doc["levelsolenoid"]["4"]["levelValue"];

        // ตัวแปร ส่วน condition light_solenoid
        String actuator_light_solenoid = doc["lightsolenoid"]["0"]["actuatorName"];
        String status_light_solenoid = doc["lightsolenoid"]["0"]["status"];
        String value_light_solenoid = doc["lightsolenoid"]["0"]["lightValue"];
        String actuator_light_solenoid2 = doc["lightsolenoid"]["1"]["actuatorName"];
        String status_light_solenoid2 = doc["lightsolenoid"]["1"]["status"];
        String value_light_solenoid2 = doc["lightsolenoid"]["1"]["lightValue"];
        String actuator_light_solenoid3 = doc["lightsolenoid"]["2"]["actuatorName"];
        String status_light_solenoid3 = doc["lightsolenoid"]["2"]["status"];
        String value_light_solenoid3 = doc["lightsolenoid"]["2"]["lightValue"];
        String actuator_light_solenoid4 = doc["lightsolenoid"]["3"]["actuatorName"];
        String status_light_solenoid4 = doc["lightsolenoid"]["3"]["status"];
        String value_light_solenoid4 = doc["lightsolenoid"]["3"]["lightValue"];

        // ตัวแปร ส่วน condition temp_motor
        String actuator_temp_motor = doc["tempdc"]["0"]["actuatorName"];
        String condition_temp_motor = doc["tempdc"]["0"]["condition"];
        String direction_temp_motor = doc["tempdc"]["0"]["direction"];
        float value_temp_motor = doc["tempdc"]["0"]["tempValue"];
        int status_temp_motor = doc["tempdc"]["0"]["speed"];
        String actuator_temp_motor2 = doc["tempdc"]["1"]["actuatorName"];
        String condition_temp_motor2 = doc["tempdc"]["1"]["condition"];
        String direction_temp_motor2 = doc["tempdc"]["1"]["direction"];
        float value_temp_motor2 = doc["tempdc"]["1"]["tempValue"];
        int status_temp_motor2 = doc["tempdc"]["1"]["speed"];
        String actuator_temp_motor3 = doc["tempdc"]["2"]["actuatorName"];
        String condition_temp_motor3 = doc["tempdc"]["2"]["condition"];
        String direction_temp_motor3 = doc["tempdc"]["2"]["direction"];
        float value_temp_motor3 = doc["tempdc"]["2"]["tempValue"];
        int status_temp_motor3 = doc["tempdc"]["2"]["speed"];

        // ตัวแปร ส่วน condition humidity_motor
        String actuator_humidity_motor = doc["humiditydc"]["0"]["actuatorName"];
        String condition_humidity_motor = doc["humiditydc"]["0"]["condition"];
        String direction_humidity_motor = doc["humiditydc"]["0"]["direction"];
        float value_humidity_motor = doc["humiditydc"]["0"]["humidityValue"];
        int status_humidity_motor = doc["humiditydc"]["0"]["speed"];
        String actuator_humidity_motor2 = doc["humiditydc"]["1"]["actuatorName"];
        String condition_humidity_motor2 = doc["humiditydc"]["1"]["condition"];
        String direction_humidity_motor2 = doc["humiditydc"]["1"]["direction"];
        float value_humidity_motor2 = doc["humiditydc"]["1"]["humidityValue"];
        int status_humidity_motor2 = doc["humiditydc"]["1"]["speed"];
        String actuator_humidity_motor3 = doc["humiditydc"]["2"]["actuatorName"];
        String condition_humidity_motor3 = doc["humiditydc"]["2"]["condition"];
        String direction_humidity_motor3 = doc["humiditydc"]["2"]["direction"];
        float value_humidity_motor3 = doc["humiditydc"]["2"]["humidityValue"];
        int status_humidity_motor3 = doc["humiditydc"]["2"]["speed"];

        // ตัวแปร ส่วน condition irsensor_motor
        String actuator_irsensor_motor = doc["irsensordc"]["0"]["actuatorName"];
        String value_irsensor_motor = doc["irsensordc"]["0"]["irsensorValue"];
        int status_irsensor_motor = doc["irsensordc"]["0"]["speed"];
        String direction_irsensor_motor = doc["irsensordc"]["0"]["direction"];
        String actuator_irsensor_motor2 = doc["irsensordc"]["1"]["actuatorName"];
        String value_irsensor_motor2 = doc["irsensordc"]["1"]["irsensorValue"];
        int status_irsensor_motor2 = doc["irsensordc"]["1"]["speed"];
        String direction_irsensor_motor2 = doc["irsensordc"]["1"]["direction"];

        // ตัวแปร ส่วน condition level_motor
        String actuator_level_motor = doc["leveldc"]["0"]["actuatorName"];
        String value_level_motor = doc["leveldc"]["0"]["levelValue"];
        int status_level_motor = doc["leveldc"]["0"]["speed"];
        String direction_level_motor = doc["leveldc"]["0"]["direction"];
        String actuator_level_motor2 = doc["leveldc"]["1"]["actuatorName"];
        String value_level_motor2 = doc["leveldc"]["1"]["levelValue"];
        int status_level_motor2 = doc["leveldc"]["1"]["speed"];
        String direction_level_motor2 = doc["leveldc"]["1"]["direction"];
        String actuator_level_motor3 = doc["leveldc"]["2"]["actuatorName"];
        String value_level_motor3 = doc["leveldc"]["2"]["levelValue"];
        int status_level_motor3 = doc["leveldc"]["2"]["speed"];
        String direction_level_motor3 = doc["leveldc"]["2"]["direction"];
        String actuator_level_motor4 = doc["leveldc"]["3"]["actuatorName"];
        String value_level_motor4 = doc["leveldc"]["3"]["levelValue"];
        int status_level_motor4 = doc["leveldc"]["3"]["speed"];
        String direction_level_motor4 = doc["leveldc"]["3"]["direction"];
        String actuator_level_motor5 = doc["leveldc"]["4"]["actuatorName"];
        String value_level_motor5 = doc["leveldc"]["4"]["levelValue"];
        int status_level_motor5 = doc["leveldc"]["4"]["speed"];
        String direction_level_motor5 = doc["leveldc"]["4"]["direction"];

        // ตัวแปร ส่วน condition light_motor
        String actuator_light_motor = doc["lightdc"]["0"]["actuatorName"];
        String value_light_motor = doc["lightdc"]["0"]["lightValue"];
        int status_light_motor = doc["lightdc"]["0"]["speed"];
        String direction_light_motor = doc["lightdc"]["0"]["direction"];
        String actuator_light_motor2 = doc["lightdc"]["1"]["actuatorName"];
        String value_light_motor2 = doc["lightdc"]["1"]["lightValue"];
        int status_light_motor2 = doc["lightdc"]["1"]["speed"];
        String direction_light_motor2 = doc["lightdc"]["1"]["direction"];
        String actuator_light_motor3 = doc["lightdc"]["2"]["actuatorName"];
        String value_light_motor3 = doc["lightdc"]["2"]["lightValue"];
        int status_light_motor3 = doc["lightdc"]["2"]["speed"];
        String direction_light_motor3 = doc["lightdc"]["2"]["direction"];
        String actuator_light_motor4 = doc["lightdc"]["3"]["actuatorName"];
        String value_light_motor4 = doc["lightdc"]["3"]["lightValue"];
        int status_light_motor4 = doc["lightdc"]["3"]["speed"];
        String direction_light_motor4 = doc["lightdc"]["3"]["direction"];

        // ตัวแปร ส่วน manaul
        // ตัวแปร ส่วน manaul_led
        String status_led = doc["led"]["0"]["status"];
        // ตัวแปร ส่วน manaul_buzzer
        String status_buzzer = doc["buzzer"]["0"]["status"];
        // ตัวแปร ส่วน manaul_servo
        int status_servo = doc["servo"]["0"]["degree"];
        String actuator_servo = doc["servo"]["0"]["actuatorName"];
        // ตัวแปร ส่วน manaul_motor
        int status_motor = doc["motor"]["0"]["speed"];
        String actuator_motor = doc["motor"]["0"]["actuatorName"];
        String status_motor_direction = doc["motor"]["0"]["direction"];
        // ตัวแปร ส่วน manaul_solenoid
        String status_solenoid = doc["solenoid"]["0"]["status"];

        // ตัวแปร ส่วน timer
        // ตัวแปร ส่วน timer_led
        String actuator_led_timer = doc["timeled"]["0"]["actuatorName"];
        String status_led_timer = doc["timeled"]["0"]["status"];
        String status_led_timer_daystart = doc["timeled"]["0"]["dayStart"];
        int status_led_timer_hourstart = doc["timeled"]["0"]["hourStart"];
        int status_led_timer_minutestart = doc["timeled"]["0"]["minuteStart"];
        String status_led_timer_string_dayhourstart = String(status_led_timer_hourstart) + "." + String(status_led_timer_minutestart);
        float status_led_timer_dayhourstart = status_led_timer_string_dayhourstart.toFloat();
        String status_led_timer_dayend = doc["timeled"]["0"]["dayEnd"];
        int status_led_timer_hourend = doc["timeled"]["0"]["hourEnd"];
        int status_led_timer_minuteend = doc["timeled"]["0"]["minuteEnd"];
        String status_led_timer_string_dayhourend = String(status_led_timer_hourend) + "." + String(status_led_timer_minuteend);
        float status_led_timer_dayhourend = status_led_timer_string_dayhourend.toFloat();
        String actuator_led_timer2 = doc["timeled"]["1"]["actuatorName"];
        String status_led_timer2 = doc["timeled"]["1"]["status"];
        String status_led_timer_daystart2 = doc["timeled"]["1"]["dayStart"];
        int status_led_timer_hourstart2 = doc["timeled"]["1"]["hourStart"];
        int status_led_timer_minutestart2 = doc["timeled"]["1"]["minuteStart"];
        String status_led_timer_string_dayhourstart2 = String(status_led_timer_hourstart2) + "." + String(status_led_timer_minutestart2);
        float status_led_timer_dayhourstart2 = status_led_timer_string_dayhourstart2.toFloat();
        String status_led_timer_dayend2 = doc["timeled"]["1"]["dayEnd"];
        int status_led_timer_hourend2 = doc["timeled"]["1"]["hourEnd"];
        int status_led_timer_minuteend2 = doc["timeled"]["1"]["minuteEnd"];
        String status_led_timer_string_dayhourend2 = String(status_led_timer_hourend2) + "." + String(status_led_timer_minuteend2);
        float status_led_timer_dayhourend2 = status_led_timer_string_dayhourend2.toFloat();
        // ตัวแปร ส่วน timer_buzzer
        String actuator_buzzer_timer = doc["timebuzzer"]["0"]["actuatorName"];
        String status_buzzer_timer = doc["timebuzzer"]["0"]["status"];
        String status_buzzer_timer_daystart = doc["timebuzzer"]["0"]["dayStart"];
        int status_buzzer_timer_hourstart = doc["timebuzzer"]["0"]["hourStart"];
        int status_buzzer_timer_minutestart = doc["timebuzzer"]["0"]["minuteStart"];
        String status_buzzer_timer_string_dayhourstart = String(status_buzzer_timer_hourstart) + "." + String(status_buzzer_timer_minutestart);
        float status_buzzer_timer_dayhourstart = status_buzzer_timer_string_dayhourstart.toFloat();
        String status_buzzer_timer_dayend = doc["timebuzzer"]["0"]["dayEnd"];
        int status_buzzer_timer_hourend = doc["timebuzzer"]["0"]["hourEnd"];
        int status_buzzer_timer_minuteend = doc["timebuzzer"]["0"]["minuteEnd"];
        String status_buzzer_timer_string_dayhourend = String(status_buzzer_timer_hourend) + "." + String(status_buzzer_timer_minuteend);
        float status_buzzer_timer_dayhourend = status_buzzer_timer_string_dayhourend.toFloat();
        String actuator_buzzer_timer2 = doc["timebuzzer"]["1"]["actuatorName"];
        String status_buzzer_timer2 = doc["timebuzzer"]["1"]["status"];
        String status_buzzer_timer_daystart2 = doc["timebuzzer"]["1"]["dayStart"];
        int status_buzzer_timer_hourstart2 = doc["timebuzzer"]["1"]["hourStart"];
        int status_buzzer_timer_minutestart2 = doc["timebuzzer"]["1"]["minuteStart"];
        String status_buzzer_timer_string_dayhourstart2 = String(status_buzzer_timer_hourstart2) + "." + String(status_buzzer_timer_minutestart2);
        float status_buzzer_timer_dayhourstart2 = status_buzzer_timer_string_dayhourstart2.toFloat();
        String status_buzzer_timer_dayend2 = doc["timebuzzer"]["1"]["dayEnd"];
        int status_buzzer_timer_hourend2 = doc["timebuzzer"]["1"]["hourEnd"];
        int status_buzzer_timer_minuteend2 = doc["timebuzzer"]["1"]["minuteEnd"];
        String status_buzzer_timer_string_dayhourend2 = String(status_buzzer_timer_hourend2) + "." + String(status_buzzer_timer_minuteend2);
        float status_buzzer_timer_dayhourend2 = status_buzzer_timer_string_dayhourend2.toFloat();
        // ตัวแปร ส่วน timer_solenoid
        String actuator_solenoid_timer = doc["timesolenoid"]["0"]["actuatorName"];
        String status_solenoid_timer = doc["timesolenoid"]["0"]["status"];
        String status_solenoid_timer_daystart = doc["timesolenoid"]["0"]["dayStart"];
        int status_solenoid_timer_hourstart = doc["timesolenoid"]["0"]["hourStart"];
        int status_solenoid_timer_minutestart = doc["timesolenoid"]["0"]["minuteStart"];
        String status_solenoid_timer_string_dayhourstart = String(status_solenoid_timer_hourstart) + "." + String(status_solenoid_timer_minutestart);
        float status_solenoid_timer_dayhourstart = status_solenoid_timer_string_dayhourstart.toFloat();
        String status_solenoid_timer_dayend = doc["timesolenoid"]["0"]["dayEnd"];
        int status_solenoid_timer_hourend = doc["timesolenoid"]["0"]["hourEnd"];
        int status_solenoid_timer_minuteend = doc["timesolenoid"]["0"]["minuteEnd"];
        String status_solenoid_timer_string_dayhourend = String(status_solenoid_timer_hourend) + "." + String(status_solenoid_timer_minuteend);
        float status_solenoid_timer_dayhourend = status_solenoid_timer_string_dayhourend.toFloat();
        String actuator_solenoid_timer2 = doc["timesolenoid"]["1"]["actuatorName"];
        String status_solenoid_timer2 = doc["timesolenoid"]["1"]["status"];
        String status_solenoid_timer_daystart2 = doc["timesolenoid"]["1"]["dayStart"];
        int status_solenoid_timer_hourstart2 = doc["timesolenoid"]["1"]["hourStart"];
        int status_solenoid_timer_minutestart2 = doc["timesolenoid"]["1"]["minuteStart"];
        String status_solenoid_timer_string_dayhourstart2 = String(status_solenoid_timer_hourstart2) + "." + String(status_solenoid_timer_minutestart2);
        float status_solenoid_timer_dayhourstart2 = status_solenoid_timer_string_dayhourstart2.toFloat();
        String status_solenoid_timer_dayend2 = doc["timesolenoid"]["1"]["dayEnd"];
        int status_solenoid_timer_hourend2 = doc["timesolenoid"]["1"]["hourEnd"];
        int status_solenoid_timer_minuteend2 = doc["timesolenoid"]["1"]["minuteEnd"];
        String status_solenoid_timer_string_dayhourend2 = String(status_solenoid_timer_hourend2) + "." + String(status_solenoid_timer_minuteend2);
        float status_solenoid_timer_dayhourend2 = status_solenoid_timer_string_dayhourend2.toFloat();
        // ตัวแปร ส่วน timer_servo
        String actuator_servo_timer = doc["timeservo"]["0"]["actuatorName"];
        int status_servo_timer = doc["timeservo"]["0"]["degree"];
        String status_servo_timer_daystart = doc["timeservo"]["0"]["dayStart"];
        int status_servo_timer_hourstart = doc["timeservo"]["0"]["hourStart"];
        int status_servo_timer_minutestart = doc["timeservo"]["0"]["minuteStart"];
        String status_servo_timer_string_dayhourstart = String(status_servo_timer_hourstart) + "." + String(status_servo_timer_minutestart);
        float status_servo_timer_dayhourstart = status_servo_timer_string_dayhourstart.toFloat();
        String status_servo_timer_dayend = doc["timeservo"]["0"]["dayEnd"];
        int status_servo_timer_hourend = doc["timeservo"]["0"]["hourEnd"];
        int status_servo_timer_minuteend = doc["timeservo"]["0"]["minuteEnd"];
        String status_servo_timer_string_dayhourend = String(status_servo_timer_hourend) + "." + String(status_servo_timer_minuteend);
        float status_servo_timer_dayhourend = status_servo_timer_string_dayhourend.toFloat();
        String actuator_servo_timer2 = doc["timeservo"]["1"]["actuatorName"];
        int status_servo_timer2 = doc["timeservo"]["1"]["degree"];
        String status_servo_timer_daystart2 = doc["timeservo"]["1"]["dayStart"];
        int status_servo_timer_hourstart2 = doc["timeservo"]["1"]["hourStart"];
        int status_servo_timer_minutestart2 = doc["timeservo"]["1"]["minuteStart"];
        String status_servo_timer_string_dayhourstart2 = String(status_servo_timer_hourstart2) + "." + String(status_servo_timer_minutestart2);
        float status_servo_timer_dayhourstart2 = status_servo_timer_string_dayhourstart2.toFloat();
        String status_servo_timer_dayend2 = doc["timeservo"]["1"]["dayEnd"];
        int status_servo_timer_hourend2 = doc["timeservo"]["1"]["hourEnd"];
        int status_servo_timer_minuteend2 = doc["timeservo"]["1"]["minuteEnd"];
        String status_servo_timer_string_dayhourend2 = String(status_servo_timer_hourend2) + "." + String(status_servo_timer_minuteend2);
        float status_servo_timer_dayhourend2 = status_servo_timer_string_dayhourend2.toFloat();
        // ตัวแปร ส่วน timer_motor
        String actuator_motor_timer = doc["timedc"]["0"]["actuatorName"];
        int status_motor_timer = doc["timedc"]["0"]["speed"];
        String status_motor_direction_timer = doc["timedc"]["0"]["direction"];
        String status_motor_timer_daystart = doc["timedc"]["0"]["dayStart"];
        int status_motor_timer_hourstart = doc["timedc"]["0"]["hourStart"];
        int status_motor_timer_minutestart = doc["timedc"]["0"]["minuteStart"];
        String status_motor_timer_string_dayhourstart = String(status_motor_timer_hourstart) + "." + String(status_motor_timer_minutestart);
        float status_motor_timer_dayhourstart = status_motor_timer_string_dayhourstart.toFloat();
        String status_motor_timer_dayend = doc["timeservo"]["0"]["dayEnd"];
        int status_motor_timer_hourend = doc["timeservo"]["0"]["hourEnd"];
        int status_motor_timer_minuteend = doc["timeservo"]["0"]["minuteEnd"];
        String status_motor_timer_string_dayhourend = String(status_motor_timer_hourend) + "." + String(status_motor_timer_minuteend);
        float status_motor_timer_dayhourend = status_motor_timer_string_dayhourend.toFloat();
        String actuator_motor_timer2 = doc["timedc"]["1"]["actuatorName"];
        int status_motor_timer2 = doc["timedc"]["1"]["speed"];
        String status_motor_direction_timer2 = doc["timedc"]["1"]["direction"];
        String status_motor_timer_daystart2 = doc["timedc"]["1"]["day"];
        int status_motor_timer_hourstart2 = doc["timedc"]["1"]["hour"];
        int status_motor_timer_minutestart2 = doc["timedc"]["1"]["minute"];
        String status_motor_timer_string_dayhourstart2 = String(status_motor_timer_hourstart2) + "." + String(status_motor_timer_minutestart2);
        float status_motor_timer_dayhourstart2 = status_motor_timer_string_dayhourstart2.toFloat();
        String status_motor_timer_dayend2 = doc["timeservo"]["1"]["dayEnd"];
        int status_motor_timer_hourend2 = doc["timeservo"]["1"]["hourEnd"];
        int status_motor_timer_minuteend2 = doc["timeservo"]["1"]["minuteEnd"];
        String status_motor_timer_string_dayhourend2 = String(status_motor_timer_hourend2) + "." + String(status_motor_timer_minuteend2);
        float status_motor_timer_dayhourend2 = status_motor_timer_string_dayhourend2.toFloat();

        int noconditionled = 0;
        int noconditionservo = 0;
        int noconditionbuzzer = 0;
        int noconditionsolenoid = 0;
        int noconditionmotor = 0;

        // Serial.println(status_buzzer_timer);

        //  code ส่วน timer
        if (status_led == "null" && actuator_led_timer == "LED")
        {
            if (status_led_timer == "on" && (dayofweek == status_led_timer_daystart && hourminute == status_led_timer_dayhourstart))
            {

                digitalWrite(led, HIGH);
                client.publish("led", "online");
                noconditionled = 1;
            }
            if (status_led_timer == "on" && hourminute > status_led_timer_dayhourstart && hourminute < status_led_timer_dayhourend)
            {
                digitalWrite(led, HIGH);
                client.publish("led", "online");
                noconditionled = 1;
            }
            if (status_led_timer == "on" && hourminute > status_led_timer_dayhourstart && (status_led_timer_dayhourstart == status_led_timer_dayhourend ) && (status_led_timer_daystart != status_led_timer_dayend))
            {
                digitalWrite(led, HIGH);
                noconditionled = 1;
            }
            if (status_led_timer == "on" && (dayofweek == status_led_timer_dayend && hourminute == status_led_timer_dayhourend && second == 0))
            {
                digitalWrite(led, LOW);
                client.publish("led", "offline");
                noconditionled = 0;
            }
            if (status_led_timer == "off" && (dayofweek == status_led_timer_daystart && hourminute == status_led_timer_dayhourstart))
            {
                digitalWrite(led, LOW);
                noconditionled = 1;
            }
            if (status_led_timer == "off" && hourminute > status_led_timer_dayhourstart && hourminute < status_led_timer_dayhourend)
            {
                digitalWrite(led, LOW);
                noconditionled = 1;
            }
            if (status_led_timer == "off" && hourminute > status_led_timer_dayhourstart && (status_led_timer_dayhourstart == status_led_timer_dayhourend) && (status_led_timer_daystart != status_led_timer_dayend))
            {
                digitalWrite(led, LOW);
                noconditionled = 1;
            }
            if (status_led_timer == "off" && (dayofweek == status_led_timer_dayend && hourminute == status_led_timer_dayhourend && second == 0))
            {
                noconditionled = 0;
            }

            if (status_led_timer2 == "on" && (dayofweek == status_led_timer_daystart2 && hourminute == status_led_timer_dayhourstart2))
            {
                digitalWrite(led, HIGH);
                noconditionled = 1;
            }
            if (status_led_timer2 == "on" && hourminute > status_led_timer_dayhourstart2 && hourminute < status_led_timer_dayhourend2)
            {
                digitalWrite(led, HIGH);
                noconditionled = 1;
            }
            if (status_led_timer2 == "on" && hourminute > status_led_timer_dayhourstart2 && (status_led_timer_dayhourstart2 == status_led_timer_dayhourend2) && (status_led_timer_daystart2 != status_led_timer_dayend2))
            {
                digitalWrite(led, HIGH);
                noconditionled = 1;
            }
            if (status_led_timer2 == "on" && (dayofweek == status_led_timer_dayend2 && hourminute == status_led_timer_dayhourend2 && second == 0))
            {
                digitalWrite(led, LOW);
                noconditionled = 0;
            }
            if (status_led_timer == "off" && (dayofweek == status_led_timer_daystart2 && hourminute == status_led_timer_dayhourstart2))
            {
                digitalWrite(led, LOW);
                noconditionled = 1;
            }
            if (status_led_timer2 == "off" && hourminute > status_led_timer_dayhourstart2 && hourminute < status_led_timer_dayhourend2)
            {
                noconditionled = 1;
                digitalWrite(led, LOW);
            }
            if (status_led_timer2 == "off" && hourminute > status_led_timer_dayhourstart2 && (status_led_timer_dayhourstart2 == status_led_timer_dayhourend2) && (status_led_timer_daystart2 != status_led_timer_dayend2))
            {
                digitalWrite(led, LOW);
                noconditionled = 1;
            }
            if (status_led_timer2 == "off" && (dayofweek == status_led_timer_dayend2 && hourminute == status_led_timer_dayhourend2 && second == 0))
            {
                noconditionled = 0;
            }
        }
        if (status_buzzer == "null" && actuator_buzzer_timer == "Buzzer")
        {
            if (status_buzzer_timer == "on" && (dayofweek == status_buzzer_timer_daystart && hourminute == status_buzzer_timer_dayhourstart))
            {
                digitalWrite(buzzer, HIGH);
                noconditionbuzzer = 1;
            }
            if (status_buzzer_timer == "on" && hourminute > status_buzzer_timer_dayhourstart && hourminute < status_buzzer_timer_dayhourend)
            {
                digitalWrite(buzzer, HIGH);
                noconditionbuzzer = 1;
            }
            if (status_buzzer_timer == "on" && hourminute > status_buzzer_timer_dayhourstart && (status_buzzer_timer_dayhourstart == status_buzzer_timer_dayhourend) && (status_buzzer_timer_daystart != status_buzzer_timer_dayend))
            {
                digitalWrite(buzzer, HIGH);
                noconditionbuzzer = 1;
            }
            if (status_buzzer_timer == "on" && (dayofweek == status_buzzer_timer_dayend && hourminute == status_buzzer_timer_dayhourend && second == 0))
            {
                digitalWrite(buzzer, LOW);
                noconditionbuzzer = 0;
            }
            if (status_buzzer_timer == "off" && (dayofweek == status_buzzer_timer_daystart && hourminute == status_buzzer_timer_dayhourstart))
            {
                digitalWrite(buzzer, LOW);
                noconditionbuzzer = 1;
            }
            if (status_buzzer_timer == "off" && hourminute > status_buzzer_timer_dayhourstart && hourminute < status_buzzer_timer_dayhourend)
            {
                digitalWrite(buzzer, LOW);
                noconditionbuzzer = 1;
            }
            if (status_buzzer_timer == "off" && hourminute > status_buzzer_timer_dayhourstart && (status_buzzer_timer_dayhourstart == status_buzzer_timer_dayhourend) && (status_buzzer_timer_daystart != status_buzzer_timer_dayend))
            {
                digitalWrite(buzzer, LOW);
                noconditionbuzzer = 1;
            }
            if (status_buzzer_timer == "off" && (dayofweek == status_buzzer_timer_dayend && hourminute == status_buzzer_timer_dayhourend && second == 0))
            {
                noconditionbuzzer = 0;
            }

            if (status_buzzer_timer2 == "on" && (dayofweek == status_buzzer_timer_daystart2 && hourminute == status_buzzer_timer_dayhourstart2))
            {
                digitalWrite(buzzer, HIGH);
                noconditionbuzzer = 1;
            }
            if (status_buzzer_timer2 == "on" && hourminute > status_buzzer_timer_dayhourstart2 && hourminute < status_buzzer_timer_dayhourend2)
            {
                digitalWrite(buzzer, HIGH);
                noconditionbuzzer = 1;
            }
            if (status_buzzer_timer2 == "on" && hourminute > status_buzzer_timer_dayhourstart2 && (status_buzzer_timer_dayhourstart2 == status_buzzer_timer_dayhourend2) && (status_buzzer_timer_daystart2 != status_buzzer_timer_dayend2))
            {
                digitalWrite(buzzer, HIGH);
                noconditionbuzzer = 1;
            }
            if (status_buzzer_timer2 == "on" && (dayofweek == status_buzzer_timer_dayend2 && hourminute == status_buzzer_timer_dayhourend2 && second == 0))
            {
                digitalWrite(buzzer, LOW);
                noconditionbuzzer = 0;
            }
            if (status_buzzer_timer2 == "off" && (dayofweek == status_buzzer_timer_daystart2 && hourminute == status_buzzer_timer_dayhourstart2))
            {
                digitalWrite(buzzer, LOW);
                noconditionbuzzer = 1;
            }
            if (status_buzzer_timer2 == "off" && hourminute > status_buzzer_timer_dayhourstart2 && hourminute < status_buzzer_timer_dayhourend2)
            {
                digitalWrite(buzzer, LOW);
                noconditionbuzzer = 1;
            }
            if (status_buzzer_timer2 == "off" && hourminute > status_buzzer_timer_dayhourstart2 && (status_buzzer_timer_dayhourstart2 == status_buzzer_timer_dayhourend2) && (status_buzzer_timer_daystart2 != status_buzzer_timer_dayend2))
            {
                digitalWrite(buzzer, LOW);
                noconditionbuzzer = 1;
            }
            if (status_buzzer_timer2 == "off" && (dayofweek == status_buzzer_timer_dayend2 && hourminute == status_buzzer_timer_dayhourend2 && second == 0))
            {
                noconditionbuzzer = 0;
            }
        }
        if (status_solenoid == "null" && actuator_solenoid_timer == "Solenoid")
        {
            if (status_solenoid_timer == "on" && (dayofweek == status_solenoid_timer_daystart && hourminute == status_solenoid_timer_dayhourstart))
            {
                digitalWrite(Relay1, HIGH);
                noconditionsolenoid = 1;
            }
            if (status_solenoid_timer == "on" && hourminute > status_solenoid_timer_dayhourstart && hourminute < status_solenoid_timer_dayhourend)
            {
                digitalWrite(Relay1, HIGH);
                noconditionsolenoid = 1;
            }
            if (status_solenoid_timer == "on" && hourminute > status_solenoid_timer_dayhourstart && (status_solenoid_timer_dayhourstart == status_solenoid_timer_dayhourend) && (status_solenoid_timer_daystart != status_solenoid_timer_dayend))
            {
                digitalWrite(Relay1, HIGH);
                noconditionsolenoid = 1;
            }
            if (status_solenoid_timer == "on" && (dayofweek == status_solenoid_timer_dayend && hourminute == status_solenoid_timer_dayhourend && second == 0))
            {
                digitalWrite(Relay1, LOW);
                noconditionsolenoid = 0;
            }
            if (status_solenoid_timer == "off" && (dayofweek == status_solenoid_timer_daystart && hourminute == status_solenoid_timer_dayhourstart))
            {
                digitalWrite(Relay1, LOW);
                noconditionsolenoid = 1;
            }
            if (status_solenoid_timer == "off" && hourminute > status_solenoid_timer_dayhourstart && hourminute < status_solenoid_timer_dayhourend)
            {
                digitalWrite(Relay1, LOW);
                noconditionsolenoid = 1;
            }
            if (status_solenoid_timer == "off" && hourminute > status_solenoid_timer_dayhourstart && (status_solenoid_timer_dayhourstart == status_solenoid_timer_dayhourend) && (status_solenoid_timer_daystart != status_solenoid_timer_dayend))
            {
                digitalWrite(Relay1, LOW);
                noconditionsolenoid = 1;
            }
            if (status_solenoid_timer == "off" && (dayofweek == status_solenoid_timer_dayend && hourminute == status_solenoid_timer_dayhourend && second == 0))
            {
                noconditionsolenoid = 0;
            }

            if (status_solenoid_timer2 == "on" && (dayofweek == status_solenoid_timer_daystart2 && hourminute == status_solenoid_timer_dayhourstart2))
            {
                digitalWrite(Relay1, HIGH);
                noconditionsolenoid = 1;
            }
            if (status_solenoid_timer2 == "on" && hourminute > status_solenoid_timer_dayhourstart2 && hourminute < status_solenoid_timer_dayhourend2)
            {
                digitalWrite(Relay1, HIGH);
                noconditionsolenoid = 1;
            }
            if (status_solenoid_timer2 == "on" && hourminute > status_solenoid_timer_dayhourstart2 && (status_solenoid_timer_dayhourstart2 == status_solenoid_timer_dayhourend2) && (status_solenoid_timer_daystart2 != status_solenoid_timer_dayend2))
            {
                digitalWrite(Relay1, HIGH);
                noconditionsolenoid = 1;
            }
            if (status_solenoid_timer2 == "on" && (dayofweek == status_solenoid_timer_dayend2 && hourminute == status_solenoid_timer_dayhourend2 && second == 0))
            {
                digitalWrite(Relay1, LOW);
                noconditionsolenoid = 0;
            }
            if (status_solenoid_timer2 == "off" && (dayofweek == status_solenoid_timer_daystart2 && hourminute == status_solenoid_timer_dayhourstart2))
            {
                digitalWrite(Relay1, LOW);
                noconditionsolenoid = 1;
            }
            if (status_solenoid_timer2 == "off" && hourminute > status_solenoid_timer_dayhourstart2 && hourminute < status_solenoid_timer_dayhourend2)
            {
                digitalWrite(Relay1, LOW);
                noconditionsolenoid = 1;
            }
            if (status_solenoid_timer2 == "off" && hourminute > status_solenoid_timer_dayhourstart2 && (status_solenoid_timer_dayhourstart2 == status_solenoid_timer_dayhourend2) && (status_solenoid_timer_daystart2 != status_solenoid_timer_dayend2))
            {
                digitalWrite(Relay1, LOW);
                noconditionsolenoid = 1;
            }
            if (status_solenoid_timer2 == "off" && (dayofweek == status_solenoid_timer_dayend2 && hourminute == status_solenoid_timer_dayhourend2 && second == 0))
            {
                noconditionsolenoid = 0;
            }
        }
        if (actuator_servo == "null" && actuator_servo_timer == "Servo Motor")
        {
            if ((status_servo_timer != 0 || status_servo_timer == 0) && (dayofweek == status_servo_timer_daystart && hourminute == status_servo_timer_dayhourstart))
            {
                myservo.write(status_servo_timer);
                noconditionservo = 1;
            }
            if ((status_servo_timer != 0 || status_servo_timer == 0) && hourminute > status_servo_timer_dayhourstart && hourminute < status_servo_timer_dayhourend)
            {
                myservo.write(status_servo_timer);
                noconditionservo = 1;
            }
            if ((status_servo_timer != 0 || status_servo_timer == 0) && hourminute > status_servo_timer_dayhourstart && (status_servo_timer_dayhourstart == status_servo_timer_dayhourend) && (status_servo_timer_daystart != status_servo_timer_dayend))
            {
                myservo.write(status_servo_timer);
                noconditionservo = 1;
            }
            if ((dayofweek == status_servo_timer_dayend && hourminute == status_servo_timer_dayhourend && second == 0))
            {
                myservo.write(0);
                noconditionservo = 0;
            }

            if ((status_servo_timer2 != 0 || status_servo_timer2 == 0) && (dayofweek == status_servo_timer_daystart2 && hourminute == status_servo_timer_dayhourstart2))
            {
                myservo.write(status_servo_timer2);
                noconditionservo = 1;
            }
            if ((status_servo_timer2 != 0 || status_servo_timer2 == 0) && hourminute > status_servo_timer_dayhourstart2 && hourminute < status_servo_timer_dayhourend2)
            {
                myservo.write(status_servo_timer2);
                noconditionservo = 1;
            }
            if ((status_servo_timer2 != 0 || status_servo_timer2 == 0) && hourminute > status_servo_timer_dayhourstart2 && (status_servo_timer_dayhourstart2 == status_servo_timer_dayhourend2) && (status_servo_timer_daystart2 != status_servo_timer_dayend2))
            {
                myservo.write(status_servo_timer2);
                noconditionservo = 1;
            }
            if ((dayofweek == status_servo_timer_dayend2 && hourminute == status_servo_timer_dayhourend2 && second == 0))
            {
                myservo.write(0);
                noconditionservo = 0;
            }
        }
        if (actuator_motor == "null" && actuator_motor_timer == "DC Motor")
        {
            if ((status_motor_timer != 0 || status_motor_timer == 0) && (dayofweek == status_motor_timer_daystart && hourminute == status_motor_timer_dayhourstart))
            {
                if (status_motor_timer != 0 && status_motor_direction_timer == "forward")
                {
                    analogWrite(enable1Pin, status_motor_timer);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, HIGH);
                    noconditionmotor = 1;
                }
                if (status_motor_timer != 0 && status_motor_direction_timer == "backward")
                {
                    analogWrite(enable1Pin, status_motor_timer);
                    digitalWrite(motor1Pin1, HIGH);
                    digitalWrite(motor1Pin2, LOW);
                    noconditionmotor = 1;
                }
                if (status_motor_timer == 0)
                {
                    analogWrite(enable1Pin, status_motor_timer);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, LOW);
                    noconditionmotor = 1;
                }
            }
            if ((status_motor_timer != 0 || status_motor_timer == 0) && hourminute > status_motor_timer_dayhourstart && hourminute < status_motor_timer_dayhourend)
            {
                if (status_motor_timer != 0 && status_motor_direction_timer == "forward")
                {
                    analogWrite(enable1Pin, status_motor_timer);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, HIGH);
                    noconditionmotor = 1;
                }
                if (status_motor_timer != 0 && status_motor_direction_timer == "backward")
                {
                    analogWrite(enable1Pin, status_motor_timer);
                    digitalWrite(motor1Pin1, HIGH);
                    digitalWrite(motor1Pin2, LOW);
                    noconditionmotor = 1;
                }
                if (status_motor_timer == 0)
                {
                    analogWrite(enable1Pin, status_motor_timer);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, LOW);
                    noconditionmotor = 1;
                }
            }
            if ((status_motor_timer != 0 || status_motor_timer == 0) && hourminute > status_motor_timer_dayhourstart && (status_motor_timer_dayhourstart == status_motor_timer_dayhourend) && (status_motor_timer_daystart != status_motor_timer_dayend))
            {
                if (status_motor_timer != 0 && status_motor_direction_timer == "forward")
                {
                    analogWrite(enable1Pin, status_motor_timer);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, HIGH);
                    noconditionmotor = 1;
                }
                if (status_motor_timer != 0 && status_motor_direction_timer == "backward")
                {
                    analogWrite(enable1Pin, status_motor_timer);
                    digitalWrite(motor1Pin1, HIGH);
                    digitalWrite(motor1Pin2, LOW);
                    noconditionmotor = 1;
                }
                if (status_motor_timer == 0)
                {
                    analogWrite(enable1Pin, status_motor_timer);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, LOW);
                    noconditionmotor = 1;
                }
            }
            if ((dayofweek == status_motor_timer_dayend && hourminute == status_motor_timer_dayhourend && second == 0))
            {
                analogWrite(enable1Pin, 0);
                digitalWrite(motor1Pin1, LOW);
                digitalWrite(motor1Pin2, LOW);
                noconditionmotor = 0;
            }

            if ((status_motor_timer2 != 0 || status_motor_timer2 == 0) && (dayofweek == status_motor_timer_daystart2 && hourminute == status_motor_timer_dayhourstart2))
            {
                if (status_motor_timer2 != 0 && status_motor_direction_timer2 == "forward")
                {
                    analogWrite(enable1Pin, status_motor_timer2);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, HIGH);
                    noconditionmotor = 1;
                }
                if (status_motor_timer2 != 0 && status_motor_direction_timer2 == "backward")
                {
                    analogWrite(enable1Pin, status_motor_timer);
                    digitalWrite(motor1Pin1, HIGH);
                    digitalWrite(motor1Pin2, LOW);
                    noconditionmotor = 1;
                }
                if (status_motor_timer2 == 0)
                {
                    analogWrite(enable1Pin, status_motor_timer2);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, LOW);
                    noconditionmotor = 1;
                }
            }
            if ((status_motor_timer2 != 0 || status_motor_timer2 == 0) && hourminute > status_motor_timer_dayhourstart2 && hourminute < status_motor_timer_dayhourend2)
            {
                if (status_motor_timer2 != 0 && status_motor_direction_timer2 == "forward")
                {
                    analogWrite(enable1Pin, status_motor_timer2);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, HIGH);
                    noconditionmotor = 1;
                }
                if (status_motor_timer2 != 0 && status_motor_direction_timer2 == "backward")
                {
                    analogWrite(enable1Pin, status_motor_timer2);
                    digitalWrite(motor1Pin1, HIGH);
                    digitalWrite(motor1Pin2, LOW);
                    noconditionmotor = 1;
                }
                if (status_motor_timer2 == 0)
                {
                    analogWrite(enable1Pin, status_motor_timer2);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, LOW);
                    noconditionmotor = 1;
                }
            }
            if ((status_motor_timer2 != 0 || status_motor_timer2 == 0) && hourminute > status_motor_timer_dayhourstart2 && (status_motor_timer_dayhourstart2 == status_motor_timer_dayhourend2) && (status_motor_timer_daystart2 != status_motor_timer_dayend2))
            {
                if (status_motor_timer2 != 0 && status_motor_direction_timer2 == "forward")
                {
                    analogWrite(enable1Pin, status_motor_timer2);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, HIGH);
                    noconditionmotor = 1;
                }
                if (status_motor_timer2 != 0 && status_motor_direction_timer2 == "backward")
                {
                    analogWrite(enable1Pin, status_motor_timer2);
                    digitalWrite(motor1Pin1, HIGH);
                    digitalWrite(motor1Pin2, LOW);
                    noconditionmotor = 1;
                }
                if (status_motor_timer2 == 0)
                {
                    analogWrite(enable1Pin, status_motor_timer2);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, LOW);
                    noconditionmotor = 1;
                }
            }
            if ((dayofweek == status_motor_timer_dayend2 && hourminute == status_motor_timer_dayhourend2 && second == 0))
            {
                analogWrite(enable1Pin, 0);
                digitalWrite(motor1Pin1, LOW);
                digitalWrite(motor1Pin2, LOW);
                noconditionmotor = 0;
            }
        }

        // code ส่วน condition ของ led
        if (status_led == "null" && noconditionled == 0)
        {
            // code ส่วน condition ของ temp_led
            if (status_temp_led == "on" && condition_temp_led == "greater" && t > value_temp_led)
            {
                digitalWrite(led, HIGH);
                client.publish("led", "online");
            }
            if (status_temp_led == "on" && condition_temp_led == "lesser" && t < value_temp_led)
            {
                digitalWrite(led, HIGH);
                client.publish("led", "online");
            }
            if (status_temp_led == "off" && condition_temp_led == "greater" && t > value_temp_led)
            {
                digitalWrite(led, LOW);
                client.publish("led", "offline");
            }
            if (status_temp_led == "off" && condition_temp_led == "lesser" && t < value_temp_led)
            {
                digitalWrite(led, LOW);
                client.publish("led", "offline");
            }
            if (status_temp_led2 == "on" && condition_temp_led2 == "greater" && t > value_temp_led2)
            {
                digitalWrite(led, HIGH);
            }
            if (status_temp_led2 == "on" && condition_temp_led2 == "lesser" && t < value_temp_led2)
            {
                digitalWrite(led, HIGH);
            }
            if (status_temp_led2 == "off" && condition_temp_led2 == "greater" && t > value_temp_led2)
            {
                digitalWrite(led, LOW);
            }
            if (status_temp_led2 == "off" && condition_temp_led2 == "lesser" && t < value_temp_led2)
            {
                digitalWrite(led, LOW);
            }
            else if (status_temp_led3 == "on" && condition_temp_led3 == "greater" && t > value_temp_led3)
            {
                digitalWrite(led, HIGH);
            }
            else if (status_temp_led3 == "on" && condition_temp_led3 == "lesser" && t < value_temp_led3)
            {
                digitalWrite(led, HIGH);
            }
            else if (status_temp_led3 == "off" && condition_temp_led3 == "greater" && t > value_temp_led3)
            {
                digitalWrite(led, LOW);
            }
            else if (status_temp_led3 == "off" && condition_temp_led3 == "lesser" && t < value_temp_led3)
            {
                digitalWrite(led, LOW);
            }

            // code ส่วน condition ของ humidity_led
            if (status_humidity_led == "on" && condition_humidity_led == "greater" && h > value_humidity_led)
            {
                digitalWrite(led, HIGH);
            }
            if (status_humidity_led == "on" && condition_humidity_led == "lesser" && h < value_humidity_led)
            {
                digitalWrite(led, HIGH);
            }
            if (status_humidity_led == "off" && condition_humidity_led == "greater" && h > value_humidity_led)
            {
                digitalWrite(led, LOW);
            }
            if (status_humidity_led == "off" && condition_humidity_led == "lesser" && h < value_humidity_led)
            {
                digitalWrite(led, LOW);
            }
            if (status_humidity_led2 == "on" && condition_humidity_led2 == "greater" && h > value_humidity_led2)
            {
                digitalWrite(led, HIGH);
            }
            if (status_humidity_led2 == "on" && condition_humidity_led2 == "lesser" && h < value_humidity_led2)
            {
                digitalWrite(led, HIGH);
            }
            if (status_humidity_led2 == "off" && condition_humidity_led2 == "greater" && h > value_humidity_led2)
            {
                digitalWrite(led, LOW);
            }
            if (status_humidity_led2 == "off" && condition_humidity_led2 == "lesser" && h < value_humidity_led2)
            {
                digitalWrite(led, LOW);
            }
            else if (status_humidity_led3 == "on" && condition_humidity_led3 == "greater" && h > value_humidity_led3)
            {
                digitalWrite(led, HIGH);
            }
            else if (status_humidity_led3 == "on" && condition_humidity_led3 == "lesser" && h < value_humidity_led3)
            {
                digitalWrite(led, HIGH);
            }
            else if (status_humidity_led3 == "off" && condition_humidity_led3 == "greater" && h > value_humidity_led3)
            {
                digitalWrite(led, LOW);
            }
            else if (status_humidity_led3 == "off" && condition_humidity_led3 == "lesser" && h < value_humidity_led3)
            {
                digitalWrite(led, LOW);
            }

            // code ส่วน condition ของ irsensor_led
            if (status_irsensor_led == "on" && IR == value_irsensor_led)
            {
                digitalWrite(led, HIGH);
            }
            if (status_irsensor_led == "off" && IR == value_irsensor_led)
            {
                digitalWrite(led, LOW);
            }
            if (status_irsensor_led2 == "on" && IR == value_irsensor_led2)
            {
                digitalWrite(led, HIGH);
            }
            if (status_irsensor_led2 == "off" && IR == value_irsensor_led2)
            {
                digitalWrite(led, LOW);
            }

            // code ส่วน condition ของ level_led
            if (status_level_led == "on" && Level == value_level_led)
            {
                digitalWrite(led, HIGH);
            }
            if (status_level_led == "off" && Level == value_level_led)
            {
                digitalWrite(led, LOW);
            }
            if (status_level_led2 == "on" && Level == value_level_led2)
            {
                digitalWrite(led, HIGH);
            }
            if (status_level_led2 == "off" && Level == value_level_led2)
            {
                digitalWrite(led, LOW);
            }
            if (status_level_led3 == "on" && Level == value_level_led3)
            {
                digitalWrite(led, HIGH);
            }
            if (status_level_led3 == "off" && Level == value_level_led3)
            {
                digitalWrite(led, LOW);
            }
            if (status_level_led4 == "on" && Level == value_level_led4)
            {
                digitalWrite(led, HIGH);
            }
            if (status_level_led4 == "off" && Level == value_level_led4)
            {
                digitalWrite(led, LOW);
            }
            if (status_level_led5 == "on" && Level == value_level_led5)
            {
                digitalWrite(led, HIGH);
            }
            if (status_level_led5 == "off" && Level == value_level_led5)
            {
                digitalWrite(led, LOW);
            }

            // code ส่วน condition ของ light_led
            if (status_light_led == "on" && Light == value_light_led)
            {
                digitalWrite(led, HIGH);
            }
            if (status_light_led == "off" && Light == value_light_led)
            {
                digitalWrite(led, LOW);
            }
            if (status_light_led2 == "on" && Light == value_light_led2)
            {
                digitalWrite(led, HIGH);
            }
            if (status_light_led2 == "off" && Light == value_light_led2)
            {
                digitalWrite(led, LOW);
            }
            if (status_light_led3 == "on" && Light == value_light_led3)
            {
                digitalWrite(led, HIGH);
            }
            if (status_light_led3 == "off" && Light == value_light_led3)
            {
                digitalWrite(led, LOW);
            }
            if (status_light_led4 == "on" && Light == value_light_led4)
            {
                digitalWrite(led, HIGH);
            }
            if (status_light_led4 == "off" && Light == value_light_led4)
            {
                digitalWrite(led, LOW);
            }
        }
        // code ส่วน condition ของ servo
        if (actuator_servo == "null" && noconditionservo == 0)
        {
            // code ส่วน condition ของ temp_servo
            if ((status_temp_servo != 0 || status_temp_servo == 0) && t > value_temp_servo && condition_temp_servo == "greater")
            {
                myservo.write(status_temp_servo);
            }
            if ((status_temp_servo != 0 || status_temp_servo == 0) && t < value_temp_servo && condition_temp_servo == "lesser")
            {
                myservo.write(status_temp_servo);
            }
            if ((status_temp_servo2 != 0 || status_temp_servo2 == 0) && t > value_temp_servo2 && condition_temp_servo2 == "greater")
            {
                myservo.write(status_temp_servo2);
            }
            if ((status_temp_servo2 != 0 || status_temp_servo2 == 0) && t < value_temp_servo2 && condition_temp_servo2 == "lesser")
            {
                myservo.write(status_temp_servo2);
            }
            if ((status_temp_servo3 != 0 || status_temp_servo3 == 0) && t > value_temp_servo3 && condition_temp_servo3 == "greater")
            {
                myservo.write(status_temp_servo3);
            }
            if ((status_temp_servo3 != 0 || status_temp_servo3 == 0) && t < value_temp_servo3 && condition_temp_servo3 == "lesser")
            {
                myservo.write(status_temp_servo3);
            }

            // code ส่วน condition ของ humidity_servo
            if ((status_humidity_servo != 0 || status_humidity_servo == 0) && h > value_humidity_servo && condition_humidity_servo == "greater")
            {
                myservo.write(status_humidity_servo);
            }
            if ((status_humidity_servo != 0 || status_humidity_servo == 0) && h < value_humidity_servo && condition_humidity_servo == "lesser")
            {
                myservo.write(status_humidity_servo);
            }
            if ((status_humidity_servo2 != 0 || status_humidity_servo2 == 0) && h > value_humidity_servo2 && condition_humidity_servo2 == "greater")
            {
                myservo.write(status_humidity_servo2);
            }
            if ((status_humidity_servo2 != 0 || status_humidity_servo2 == 0) && h < value_humidity_servo2 && condition_humidity_servo2 == "lesser")
            {
                myservo.write(status_humidity_servo2);
            }
            if ((status_humidity_servo3 != 0 || status_humidity_servo3 == 0) && h > value_humidity_servo3 && condition_humidity_servo3 == "greater")
            {
                myservo.write(status_humidity_servo3);
            }
            if ((status_humidity_servo3 != 0 || status_humidity_servo3 == 0) && h < value_humidity_servo3 && condition_humidity_servo3 == "lesser")
            {
                myservo.write(status_humidity_servo3);
            }

            // code ส่วน condition ของ irsensor_servo
            if ((status_irsensor_servo != 0 || status_irsensor_servo == 0) && IR == value_irsensor_servo)
            {
                myservo.write(status_irsensor_servo);
            }
            if ((status_irsensor_servo2 != 0 || status_irsensor_servo2 == 0) && IR == value_irsensor_servo2)
            {
                myservo.write(status_irsensor_servo2);
            }

            // code ส่วน condition ของ level_servo
            if ((status_level_servo != 0 || status_level_servo == 0) && Level == value_level_servo)
            {
                myservo.write(status_level_servo);
            }
            if ((status_level_servo2 != 0 || status_level_servo2 == 0) && Level == value_level_servo2)
            {
                myservo.write(status_level_servo2);
            }
            if ((status_level_servo3 != 0 || status_level_servo3 == 0) && Level == value_level_servo3)
            {
                myservo.write(status_level_servo3);
            }
            if ((status_level_servo4 != 0 || status_level_servo4 == 0) && Level == value_level_servo4)
            {
                myservo.write(status_level_servo4);
            }
            if ((status_level_servo5 != 0 || status_level_servo5 == 0) && Level == value_level_servo5)
            {
                myservo.write(status_level_servo5);
            }

            // code ส่วน condition ของ light_servo
            if ((status_light_servo != 0 || status_light_servo == 0) && Light == value_light_servo)
            {
                myservo.write(status_light_servo);
            }
            if ((status_light_servo2 != 0 || status_light_servo2 == 0) && Light == value_light_servo2)
            {
                myservo.write(status_light_servo2);
            }
            if ((status_light_servo3 != 0 || status_light_servo3 == 0) && Light == value_light_servo3)
            {
                myservo.write(status_light_servo3);
            }
            if ((status_light_servo4 != 0 || status_light_servo4 == 0) && Light == value_light_servo4)
            {
                myservo.write(status_light_servo4);
            }
        }
        // code ส่วน condition ของ buzzer
        if (status_buzzer == "null" && noconditionbuzzer == 0)
        {
            // code ส่วน condition ของ temp_buzzer
            if (status_temp_buzzer == "on" && condition_temp_buzzer == "greater" && t > value_temp_buzzer)
            {
                digitalWrite(buzzer, HIGH);
            }
            if (status_temp_buzzer == "on" && condition_temp_buzzer == "lesser" && t < value_temp_buzzer)
            {
                digitalWrite(buzzer, HIGH);
            }
            if (status_temp_buzzer == "off" && condition_temp_buzzer == "greater" && t > value_temp_buzzer)
            {
                digitalWrite(buzzer, LOW);
            }
            if (status_temp_buzzer == "off" && condition_temp_buzzer == "lesser" && t < value_temp_buzzer)
            {
                digitalWrite(buzzer, LOW);
            }
            if (status_temp_buzzer2 == "on" && condition_temp_buzzer2 == "greater" && t > value_temp_buzzer2)
            {
                digitalWrite(buzzer, HIGH);
            }
            if (status_temp_buzzer2 == "on" && condition_temp_buzzer2 == "lesser" && t < value_temp_buzzer2)
            {
                digitalWrite(buzzer, HIGH);
            }
            if (status_temp_buzzer2 == "off" && condition_temp_buzzer2 == "greater" && t > value_temp_buzzer2)
            {
                digitalWrite(buzzer, LOW);
            }
            if (status_temp_buzzer2 == "off" && condition_temp_buzzer2 == "lesser" && t < value_temp_buzzer2)
            {
                digitalWrite(buzzer, LOW);
            }
            else if (status_temp_buzzer3 == "on" && condition_temp_buzzer3 == "greater" && t > value_temp_buzzer3)
            {
                digitalWrite(buzzer, HIGH);
            }
            else if (status_temp_buzzer3 == "on" && condition_temp_buzzer3 == "lesser" && t < value_temp_buzzer3)
            {
                digitalWrite(buzzer, HIGH);
            }
            else if (status_temp_buzzer3 == "off" && condition_temp_buzzer3 == "greater" && t > value_temp_buzzer3)
            {
                digitalWrite(buzzer, LOW);
            }
            else if (status_temp_buzzer3 == "off" && condition_temp_buzzer3 == "lesser" && t < value_temp_buzzer3)
            {
                digitalWrite(buzzer, LOW);
            }

            // code ส่วน condition ของ humidity_buzzer
            if (status_humidity_buzzer == "on" && condition_humidity_buzzer == "greater" && h > value_humidity_buzzer)
            {
                digitalWrite(buzzer, HIGH);
            }
            if (status_humidity_buzzer == "on" && condition_humidity_buzzer == "lesser" && h < value_humidity_buzzer)
            {
                digitalWrite(buzzer, HIGH);
            }
            if (status_humidity_buzzer == "off" && condition_humidity_buzzer == "greater" && h > value_humidity_buzzer)
            {
                digitalWrite(buzzer, LOW);
            }
            if (status_humidity_buzzer == "off" && condition_humidity_buzzer == "lesser" && h < value_humidity_buzzer)
            {
                digitalWrite(buzzer, LOW);
            }
            if (status_humidity_buzzer2 == "on" && condition_humidity_buzzer2 == "greater" && h > value_humidity_buzzer2)
            {
                digitalWrite(buzzer, HIGH);
            }
            if (status_humidity_buzzer2 == "on" && condition_humidity_buzzer2 == "lesser" && h < value_humidity_buzzer2)
            {
                digitalWrite(buzzer, HIGH);
            }
            if (status_humidity_buzzer2 == "off" && condition_humidity_buzzer2 == "greater" && h > value_humidity_buzzer2)
            {
                digitalWrite(buzzer, LOW);
            }
            if (status_humidity_buzzer2 == "off" && condition_humidity_buzzer2 == "lesser" && h < value_humidity_buzzer2)
            {
                digitalWrite(buzzer, LOW);
            }
            else if (status_humidity_buzzer3 == "on" && condition_humidity_buzzer3 == "greater" && h > value_humidity_buzzer3)
            {
                digitalWrite(buzzer, HIGH);
            }
            else if (status_humidity_buzzer3 == "on" && condition_humidity_buzzer3 == "lesser" && h < value_humidity_buzzer3)
            {
                digitalWrite(buzzer, HIGH);
            }
            else if (status_humidity_buzzer3 == "off" && condition_humidity_buzzer3 == "greater" && h > value_humidity_buzzer3)
            {
                digitalWrite(buzzer, LOW);
            }
            else if (status_humidity_buzzer3 == "off" && condition_humidity_buzzer3 == "lesser" && h < value_humidity_buzzer3)
            {
                digitalWrite(buzzer, LOW);
            }

            // code ส่วน condition ของ irsensor_buzzer
            if (status_irsensor_buzzer == "on" && IR == value_irsensor_buzzer)
            {
                digitalWrite(buzzer, HIGH);
            }
            if (status_irsensor_buzzer == "off" && IR == value_irsensor_buzzer)
            {
                digitalWrite(buzzer, LOW);
            }
            if (status_irsensor_buzzer2 == "on" && IR == value_irsensor_buzzer2)
            {
                digitalWrite(buzzer, HIGH);
            }
            if (status_irsensor_buzzer2 == "off" && IR == value_irsensor_buzzer2)
            {
                digitalWrite(buzzer, LOW);
            }

            // code ส่วน condition ของ level_buzzer
            if (status_level_buzzer == "on" && Level == value_level_buzzer)
            {
                digitalWrite(buzzer, HIGH);
            }
            if (status_level_buzzer == "off" && Level == value_level_buzzer)
            {
                digitalWrite(buzzer, LOW);
            }
            if (status_level_buzzer2 == "on" && Level == value_level_buzzer2)
            {
                digitalWrite(buzzer, HIGH);
            }
            if (status_level_buzzer2 == "off" && Level == value_level_buzzer2)
            {
                digitalWrite(buzzer, LOW);
            }
            if (status_level_buzzer3 == "on" && Level == value_level_buzzer3)
            {
                digitalWrite(buzzer, HIGH);
            }
            if (status_level_buzzer3 == "off" && Level == value_level_buzzer3)
            {
                digitalWrite(buzzer, LOW);
            }
            if (status_level_buzzer4 == "on" && Level == value_level_buzzer4)
            {
                digitalWrite(buzzer, HIGH);
            }
            if (status_level_buzzer4 == "off" && Level == value_level_buzzer4)
            {
                digitalWrite(buzzer, LOW);
            }
            if (status_level_buzzer5 == "on" && Level == value_level_buzzer5)
            {
                digitalWrite(buzzer, HIGH);
            }
            if (status_level_buzzer5 == "off" && Level == value_level_buzzer5)
            {
                digitalWrite(buzzer, LOW);
            }

            // code ส่วน condition ของ light_buzzer
            if (status_light_buzzer == "on" && Light == value_light_buzzer)
            {
                digitalWrite(buzzer, HIGH);
            }
            if (status_light_buzzer == "off" && Light == value_light_buzzer)
            {
                digitalWrite(buzzer, LOW);
            }
            if (status_light_buzzer2 == "on" && Light == value_light_buzzer2)
            {
                digitalWrite(buzzer, HIGH);
            }
            if (status_light_buzzer2 == "off" && Light == value_light_buzzer2)
            {
                digitalWrite(buzzer, LOW);
            }
            if (status_light_buzzer3 == "on" && Light == value_light_buzzer3)
            {
                digitalWrite(buzzer, HIGH);
            }
            if (status_light_buzzer3 == "off" && Light == value_light_buzzer3)
            {
                digitalWrite(buzzer, LOW);
            }
            if (status_light_buzzer4 == "on" && Light == value_light_buzzer4)
            {
                digitalWrite(buzzer, HIGH);
            }
            if (status_light_buzzer4 == "off" && Light == value_light_buzzer4)
            {
                digitalWrite(buzzer, LOW);
            }
        }
        // code ส่วน condition ของ solenoid
        if (status_solenoid == "null" && noconditionsolenoid == 0)
        {
            // code ส่วน condition ของ temp_solenoid
            if (status_temp_solenoid == "on" && condition_temp_solenoid == "greater" && t > value_temp_solenoid)
            {
                digitalWrite(Relay1, HIGH);
            }
            if (status_temp_solenoid == "on" && condition_temp_solenoid == "lesser" && t < value_temp_solenoid)
            {
                digitalWrite(Relay1, HIGH);
            }
            if (status_temp_solenoid == "off" && condition_temp_solenoid == "greater" && t > value_temp_solenoid)
            {
                digitalWrite(Relay1, LOW);
            }
            if (status_temp_solenoid == "off" && condition_temp_solenoid == "lesser" && t < value_temp_solenoid)
            {
                digitalWrite(Relay1, LOW);
            }
            if (status_temp_solenoid2 == "on" && condition_temp_solenoid2 == "greater" && t > value_temp_solenoid2)
            {
                digitalWrite(Relay1, HIGH);
            }
            if (status_temp_solenoid2 == "on" && condition_temp_solenoid2 == "lesser" && t < value_temp_solenoid2)
            {
                digitalWrite(Relay1, HIGH);
            }
            if (status_temp_solenoid2 == "off" && condition_temp_solenoid2 == "greater" && t > value_temp_solenoid2)
            {
                digitalWrite(Relay1, LOW);
            }
            if (status_temp_solenoid2 == "off" && condition_temp_solenoid2 == "lesser" && t < value_temp_solenoid2)
            {
                digitalWrite(Relay1, LOW);
            }
            else if (status_temp_solenoid3 == "on" && condition_temp_solenoid3 == "greater" && t > value_temp_solenoid3)
            {
                digitalWrite(Relay1, HIGH);
            }
            else if (status_temp_solenoid3 == "on" && condition_temp_solenoid3 == "lesser" && t < value_temp_solenoid3)
            {
                digitalWrite(Relay1, HIGH);
            }
            else if (status_temp_solenoid3 == "off" && condition_temp_solenoid3 == "greater" && t > value_temp_solenoid3)
            {
                digitalWrite(Relay1, LOW);
            }
            else if (status_temp_solenoid3 == "off" && condition_temp_solenoid3 == "lesser" && t < value_temp_solenoid3)
            {
                digitalWrite(Relay1, LOW);
            }

            // code ส่วน condition ของ humidity_solenoid
            if (status_humidity_solenoid == "on" && condition_humidity_solenoid == "greater" && h > value_humidity_solenoid)
            {
                digitalWrite(Relay1, HIGH);
            }
            if (status_humidity_solenoid == "on" && condition_humidity_solenoid == "lesser" && h < value_humidity_solenoid)
            {
                digitalWrite(Relay1, HIGH);
            }
            if (status_humidity_solenoid == "off" && condition_humidity_solenoid == "greater" && h > value_humidity_solenoid)
            {
                digitalWrite(Relay1, LOW);
            }
            if (status_humidity_solenoid == "off" && condition_humidity_solenoid == "lesser" && h < value_humidity_solenoid)
            {
                digitalWrite(Relay1, LOW);
            }
            if (status_humidity_solenoid2 == "on" && condition_humidity_solenoid2 == "greater" && h > value_humidity_solenoid2)
            {
                digitalWrite(Relay1, HIGH);
            }
            if (status_humidity_solenoid2 == "on" && condition_humidity_solenoid2 == "lesser" && h < value_humidity_solenoid2)
            {
                digitalWrite(Relay1, HIGH);
            }
            if (status_humidity_solenoid2 == "off" && condition_humidity_solenoid2 == "greater" && h > value_humidity_solenoid2)
            {
                digitalWrite(Relay1, LOW);
            }
            if (status_humidity_solenoid2 == "off" && condition_humidity_solenoid2 == "lesser" && h < value_humidity_solenoid2)
            {
                digitalWrite(Relay1, LOW);
            }
            if (status_humidity_solenoid3 == "on" && condition_humidity_solenoid3 == "greater" && h > value_humidity_solenoid3)
            {
                digitalWrite(Relay1, HIGH);
            }
            if (status_humidity_solenoid3 == "on" && condition_humidity_solenoid3 == "lesser" && h < value_humidity_solenoid3)
            {
                digitalWrite(Relay1, HIGH);
            }
            if (status_humidity_solenoid3 == "off" && condition_humidity_solenoid3 == "greater" && h > value_humidity_solenoid3)
            {
                digitalWrite(Relay1, LOW);
            }
            if (status_humidity_solenoid3 == "off" && condition_humidity_solenoid3 == "lesser" && h < value_humidity_solenoid3)
            {
                digitalWrite(Relay1, LOW);
            }

            // code ส่วน condition ของ irsensor_solenoid
            if (status_irsensor_solenoid == "on" && IR == value_irsensor_solenoid)
            {
                digitalWrite(Relay1, HIGH);
            }
            if (status_irsensor_solenoid == "off" && IR == value_irsensor_solenoid)
            {
                digitalWrite(Relay1, LOW);
            }
            if (status_irsensor_solenoid2 == "on" && IR == value_irsensor_solenoid2)
            {
                digitalWrite(Relay1, HIGH);
            }
            if (status_irsensor_solenoid2 == "off" && IR == value_irsensor_solenoid2)
            {
                digitalWrite(Relay1, LOW);
            }

            // code ส่วน condition ของ level_solenoid
            if (status_level_solenoid == "on" && Level == value_level_solenoid)
            {
                digitalWrite(Relay1, HIGH);
            }
            if (status_level_solenoid == "off" && Level == value_level_solenoid)
            {
                digitalWrite(Relay1, LOW);
            }
            if (status_level_solenoid2 == "on" && Level == value_level_solenoid2)
            {
                digitalWrite(Relay1, HIGH);
            }
            if (status_level_solenoid2 == "off" && Level == value_level_solenoid2)
            {
                digitalWrite(Relay1, LOW);
            }
            if (status_level_solenoid3 == "on" && Level == value_level_solenoid3)
            {
                digitalWrite(Relay1, HIGH);
            }
            if (status_level_solenoid3 == "off" && Level == value_level_solenoid3)
            {
                digitalWrite(Relay1, LOW);
            }
            if (status_level_solenoid4 == "on" && Level == value_level_solenoid4)
            {
                digitalWrite(Relay1, HIGH);
            }
            if (status_level_solenoid4 == "off" && Level == value_level_solenoid4)
            {
                digitalWrite(Relay1, LOW);
            }
            if (status_level_solenoid5 == "on" && Level == value_level_solenoid5)
            {
                digitalWrite(Relay1, HIGH);
            }
            if (status_level_solenoid5 == "off" && Level == value_level_solenoid5)
            {
                digitalWrite(Relay1, LOW);
            }

            // code ส่วน condition ของ light_solenoid
            if (status_light_solenoid == "on" && Light == value_light_solenoid)
            {
                digitalWrite(Relay1, HIGH);
            }
            if (status_light_solenoid == "off" && Light == value_light_solenoid)
            {
                digitalWrite(Relay1, LOW);
            }
            if (status_light_solenoid2 == "on" && Light == value_light_solenoid2)
            {
                digitalWrite(Relay1, HIGH);
            }
            if (status_light_solenoid2 == "off" && Light == value_light_solenoid2)
            {
                digitalWrite(Relay1, LOW);
            }
            if (status_light_solenoid3 == "on" && Light == value_light_solenoid3)
            {
                digitalWrite(Relay1, HIGH);
            }
            if (status_light_solenoid3 == "off" && Light == value_light_solenoid3)
            {
                digitalWrite(Relay1, LOW);
            }
            if (status_light_solenoid4 == "on" && Light == value_light_solenoid4)
            {
                digitalWrite(Relay1, HIGH);
            }
            if (status_light_solenoid4 == "off" && Light == value_light_solenoid4)
            {
                digitalWrite(Relay1, LOW);
            }
        }
        // code ส่วน condition ของ motor
        if (actuator_motor == "null" && noconditionmotor == 0)
        {
            // code ส่วน condition ของ temp_motor
            if ((status_temp_motor != 0 || status_temp_motor == 0) && t > value_temp_motor && condition_temp_motor == "greater")
            {
                if (status_temp_motor != 0 && direction_temp_motor == "forward")
                {
                    analogWrite(enable1Pin, status_temp_motor);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, HIGH);
                }
                if (status_temp_motor != 0 && direction_temp_motor == "backward")
                {
                    analogWrite(enable1Pin, status_temp_motor);
                    digitalWrite(motor1Pin1, HIGH);
                    digitalWrite(motor1Pin2, LOW);
                }
                if (status_temp_motor == 0)
                {
                    analogWrite(enable1Pin, status_temp_motor);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, LOW);
                }
            }
            if ((status_temp_motor != 0 || status_temp_motor == 0) && t < value_temp_motor && condition_temp_motor == "lesser")
            {
                if (status_temp_motor != 0 && direction_temp_motor == "forward")
                {
                    analogWrite(enable1Pin, status_temp_motor);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, HIGH);
                }
                if (status_temp_motor != 0 && direction_temp_motor == "backward")
                {
                    analogWrite(enable1Pin, status_temp_motor);
                    digitalWrite(motor1Pin1, HIGH);
                    digitalWrite(motor1Pin2, LOW);
                }
                if (status_temp_motor == 0)
                {
                    analogWrite(enable1Pin, status_temp_motor);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, LOW);
                }
            }
            if ((status_temp_motor2 != 0 || status_temp_motor2 == 0) && t > value_temp_motor2 && condition_temp_motor2 == "greater")
            {
                if (status_temp_motor2 != 0 && direction_temp_motor2 == "forward")
                {
                    analogWrite(enable1Pin, status_temp_motor2);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, HIGH);
                }
                if (status_temp_motor2 != 0 && direction_temp_motor2 == "backward")
                {
                    analogWrite(enable1Pin, status_temp_motor2);
                    digitalWrite(motor1Pin1, HIGH);
                    digitalWrite(motor1Pin2, LOW);
                }
                if (status_temp_motor2 == 0)
                {
                    analogWrite(enable1Pin, status_temp_motor2);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, LOW);
                }
            }
            if ((status_temp_motor2 != 0 || status_temp_motor2 == 0) && t < value_temp_motor2 && condition_temp_motor2 == "lesser")
            {
                if (status_temp_motor2 != 0 && direction_temp_motor2 == "forward")
                {
                    analogWrite(enable1Pin, status_temp_motor2);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, HIGH);
                }
                if (status_temp_motor2 != 0 && direction_temp_motor2 == "backward")
                {
                    analogWrite(enable1Pin, status_temp_motor2);
                    digitalWrite(motor1Pin1, HIGH);
                    digitalWrite(motor1Pin2, LOW);
                }
                if (status_temp_motor2 == 0)
                {
                    analogWrite(enable1Pin, status_temp_motor2);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, LOW);
                }
            }
            if ((status_temp_motor3 != 0 || status_temp_motor3 == 0) && t > value_temp_motor3 && condition_temp_motor3 == "greater")
            {
                if (status_temp_motor3 != 0 && direction_temp_motor3 == "forward")
                {
                    analogWrite(enable1Pin, status_temp_motor3);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, HIGH);
                }
                if (status_temp_motor3 != 0 && direction_temp_motor3 == "backward")
                {
                    analogWrite(enable1Pin, status_temp_motor3);
                    digitalWrite(motor1Pin1, HIGH);
                    digitalWrite(motor1Pin2, LOW);
                }
                if (status_temp_motor3 == 0)
                {
                    analogWrite(enable1Pin, status_temp_motor3);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, LOW);
                }
            }
            if ((status_temp_motor3 != 0 || status_temp_motor3 == 0) && t < value_temp_motor3 && condition_temp_motor3 == "lesser")
            {
                if (status_temp_motor3 != 0 && direction_temp_motor3 == "forward")
                {
                    analogWrite(enable1Pin, status_temp_motor3);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, HIGH);
                }
                if (status_temp_motor3 != 0 && direction_temp_motor3 == "backward")
                {
                    analogWrite(enable1Pin, status_temp_motor3);
                    digitalWrite(motor1Pin1, HIGH);
                    digitalWrite(motor1Pin2, LOW);
                }
                if (status_temp_motor3 == 0)
                {
                    analogWrite(enable1Pin, status_temp_motor3);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, LOW);
                }
            }

            // code ส่วน condition ของ humidity_motor
            if ((status_humidity_motor != 0 || status_humidity_motor == 0) && h > value_humidity_motor && condition_humidity_motor == "greater")
            {
                if (status_humidity_motor != 0 && direction_humidity_motor == "forward")
                {
                    analogWrite(enable1Pin, status_humidity_motor);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, HIGH);
                }
                if (status_humidity_motor != 0 && direction_humidity_motor == "backward")
                {
                    analogWrite(enable1Pin, status_humidity_motor);
                    digitalWrite(motor1Pin1, HIGH);
                    digitalWrite(motor1Pin2, LOW);
                }
                if (status_humidity_motor == 0)
                {
                    analogWrite(enable1Pin, status_humidity_motor);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, LOW);
                }
            }
            if ((status_humidity_motor != 0 || status_humidity_motor == 0) && h < value_humidity_motor && condition_humidity_motor == "lesser")
            {
                if (status_humidity_motor != 0 && direction_humidity_motor == "forward")
                {
                    analogWrite(enable1Pin, status_humidity_motor);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, HIGH);
                }
                if (status_humidity_motor != 0 && direction_humidity_motor == "backward")
                {
                    analogWrite(enable1Pin, status_humidity_motor);
                    digitalWrite(motor1Pin1, HIGH);
                    digitalWrite(motor1Pin2, LOW);
                }
                if (status_humidity_motor == 0)
                {
                    analogWrite(enable1Pin, status_humidity_motor);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, LOW);
                }
            }
            if ((status_humidity_motor2 != 0 || status_humidity_motor2 == 0) && h > value_humidity_motor2 && condition_humidity_motor2 == "greater")
            {
                if (status_humidity_motor2 != 0 && direction_humidity_motor2 == "forward")
                {
                    analogWrite(enable1Pin, status_humidity_motor2);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, HIGH);
                }
                if (status_humidity_motor2 != 0 && direction_humidity_motor2 == "backward")
                {
                    analogWrite(enable1Pin, status_humidity_motor2);
                    digitalWrite(motor1Pin1, HIGH);
                    digitalWrite(motor1Pin2, LOW);
                }
                if (status_humidity_motor2 == 0)
                {
                    analogWrite(enable1Pin, status_humidity_motor2);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, LOW);
                }
            }
            if ((status_humidity_motor2 != 0 || status_humidity_motor2 == 0) && h < value_humidity_motor2 && condition_humidity_motor2 == "lesser")
            {
                if (status_humidity_motor2 != 0 && direction_humidity_motor2 == "forward")
                {
                    analogWrite(enable1Pin, status_humidity_motor2);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, HIGH);
                }
                if (status_humidity_motor2 != 0 && direction_humidity_motor2 == "backward")
                {
                    analogWrite(enable1Pin, status_humidity_motor2);
                    digitalWrite(motor1Pin1, HIGH);
                    digitalWrite(motor1Pin2, LOW);
                }
                if (status_humidity_motor2 == 0)
                {
                    analogWrite(enable1Pin, status_humidity_motor2);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, LOW);
                }
            }
            if ((status_humidity_motor3 != 0 || status_humidity_motor3 == 0) && h > value_humidity_motor3 && condition_humidity_motor3 == "greater")
            {
                if (status_humidity_motor3 != 0 && direction_humidity_motor3 == "forward")
                {
                    analogWrite(enable1Pin, status_humidity_motor3);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, HIGH);
                }
                if (status_humidity_motor3 != 0 && direction_humidity_motor3 == "backward")
                {
                    analogWrite(enable1Pin, status_humidity_motor3);
                    digitalWrite(motor1Pin1, HIGH);
                    digitalWrite(motor1Pin2, LOW);
                }
                if (status_humidity_motor3 == 0)
                {
                    analogWrite(enable1Pin, status_humidity_motor3);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, LOW);
                }
            }
            if ((status_humidity_motor3 != 0 || status_humidity_motor3 == 0) && h < value_humidity_motor3 && condition_humidity_motor3 == "lesser")
            {
                if (status_humidity_motor3 != 0 && direction_humidity_motor3 == "forward")
                {
                    analogWrite(enable1Pin, status_humidity_motor3);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, HIGH);
                }
                if (status_humidity_motor3 != 0 && direction_humidity_motor3 == "backward")
                {
                    analogWrite(enable1Pin, status_humidity_motor3);
                    digitalWrite(motor1Pin1, HIGH);
                    digitalWrite(motor1Pin2, LOW);
                }
                if (status_humidity_motor3 == 0)
                {
                    analogWrite(enable1Pin, status_humidity_motor3);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, LOW);
                }
            }

            // code ส่วน condition ของ irsensor_motor
            if ((status_irsensor_motor != 0 || status_irsensor_motor == 0) && IR == value_irsensor_motor)
            {
                if (status_irsensor_motor != 0 && direction_irsensor_motor == "forward")
                {
                    analogWrite(enable1Pin, status_irsensor_motor);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, HIGH);
                }
                if (status_irsensor_motor != 0 && direction_irsensor_motor == "backward")
                {
                    analogWrite(enable1Pin, status_irsensor_motor);
                    digitalWrite(motor1Pin1, HIGH);
                    digitalWrite(motor1Pin2, LOW);
                }
                if (status_irsensor_motor == 0)
                {
                    analogWrite(enable1Pin, status_irsensor_motor);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, LOW);
                }
            }
            if ((status_irsensor_motor2 != 0 || status_irsensor_motor2 == 0) && IR == value_irsensor_motor2)
            {
                if (status_irsensor_motor2 != 0 && direction_irsensor_motor2 == "forward")
                {
                    analogWrite(enable1Pin, status_irsensor_motor2);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, HIGH);
                }
                if (status_irsensor_motor2 != 0 && direction_irsensor_motor2 == "backward")
                {
                    analogWrite(enable1Pin, status_irsensor_motor2);
                    digitalWrite(motor1Pin1, HIGH);
                    digitalWrite(motor1Pin2, LOW);
                }
                if (status_irsensor_motor2 == 0)
                {
                    analogWrite(enable1Pin, status_irsensor_motor2);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, LOW);
                }
            }

            // code ส่วน condition ของ level_motor
            if ((status_level_motor != 0 || status_level_motor == 0) && Level == value_level_motor)
            {
                if (status_level_motor != 0 && direction_level_motor == "forward")
                {
                    analogWrite(enable1Pin, status_level_motor);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, HIGH);
                }
                if (status_level_motor != 0 && direction_level_motor == "backward")
                {
                    analogWrite(enable1Pin, status_level_motor);
                    digitalWrite(motor1Pin1, HIGH);
                    digitalWrite(motor1Pin2, LOW);
                }
                if (status_level_motor == 0)
                {
                    analogWrite(enable1Pin, status_level_motor);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, LOW);
                }
            }
            if ((status_level_motor2 != 0 || status_level_motor2 == 0) && Level == value_level_motor2)
            {
                if (status_level_motor2 != 0 && direction_level_motor2 == "forward")
                {
                    analogWrite(enable1Pin, status_level_motor2);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, HIGH);
                }
                if (status_level_motor2 != 0 && direction_level_motor2 == "backward")
                {
                    analogWrite(enable1Pin, status_level_motor2);
                    digitalWrite(motor1Pin1, HIGH);
                    digitalWrite(motor1Pin2, LOW);
                }
                if (status_level_motor2 == 0)
                {
                    analogWrite(enable1Pin, status_level_motor2);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, LOW);
                }
            }
            if ((status_level_motor3 != 0 || status_level_motor3 == 0) && Level == value_level_motor3)
            {
                if (status_level_motor3 != 0 && direction_level_motor3 == "forward")
                {
                    analogWrite(enable1Pin, status_level_motor3);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, HIGH);
                }
                if (status_level_motor3 != 0 && direction_level_motor3 == "backward")
                {
                    analogWrite(enable1Pin, status_level_motor3);
                    digitalWrite(motor1Pin1, HIGH);
                    digitalWrite(motor1Pin2, LOW);
                }
                if (status_level_motor3 == 0)
                {
                    analogWrite(enable1Pin, status_level_motor3);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, LOW);
                }
            }
            if ((status_level_motor4 != 0 || status_level_motor4 == 0) && Level == value_level_motor4)
            {
                if (status_level_motor4 != 0 && direction_level_motor4 == "forward")
                {
                    analogWrite(enable1Pin, status_level_motor4);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, HIGH);
                }
                if (status_level_motor4 != 0 && direction_level_motor4 == "backward")
                {
                    analogWrite(enable1Pin, status_level_motor4);
                    digitalWrite(motor1Pin1, HIGH);
                    digitalWrite(motor1Pin2, LOW);
                }
                if (status_level_motor4 == 0)
                {
                    analogWrite(enable1Pin, status_level_motor4);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, LOW);
                }
            }
            if ((status_level_motor5 != 0 || status_level_motor5 == 0) && Level == value_level_motor5)
            {
                if (status_level_motor5 != 0 && direction_level_motor5 == "forward")
                {
                    analogWrite(enable1Pin, status_level_motor5);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, HIGH);
                }
                if (status_level_motor5 != 0 && direction_level_motor5 == "backward")
                {
                    analogWrite(enable1Pin, status_level_motor5);
                    digitalWrite(motor1Pin1, HIGH);
                    digitalWrite(motor1Pin2, LOW);
                }
                if (status_level_motor5 == 0)
                {
                    analogWrite(enable1Pin, status_level_motor5);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, LOW);
                }
            }

            // code ส่วน condition ของ light_motor
            if ((status_light_motor != 0 || status_light_motor == 0) && Light == value_light_motor)
            {
                if (status_light_motor != 0 && direction_light_motor == "forward")
                {
                    analogWrite(enable1Pin, status_light_motor);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, HIGH);
                }
                if (status_light_motor != 0 && direction_light_motor == "backward")
                {
                    analogWrite(enable1Pin, status_light_motor);
                    digitalWrite(motor1Pin1, HIGH);
                    digitalWrite(motor1Pin2, LOW);
                }
                if (status_light_motor == 0)
                {
                    analogWrite(enable1Pin, status_light_motor);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, LOW);
                }
            }
            if ((status_light_motor2 != 0 || status_light_motor2 == 0) && Light == value_light_motor2)
            {
                if (status_light_motor2 != 0 && direction_light_motor2 == "forward")
                {
                    analogWrite(enable1Pin, status_light_motor2);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, HIGH);
                }
                if (status_light_motor2 != 0 && direction_light_motor2 == "backward")
                {
                    analogWrite(enable1Pin, status_light_motor2);
                    digitalWrite(motor1Pin1, HIGH);
                    digitalWrite(motor1Pin2, LOW);
                }
                if (status_light_motor2 == 0)
                {
                    analogWrite(enable1Pin, status_light_motor2);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, LOW);
                }
            }
            if ((status_light_motor3 != 0 || status_light_motor3 == 0) && Light == value_light_motor3)
            {
                if (status_light_motor3 != 0 && direction_light_motor3 == "forward")
                {
                    analogWrite(enable1Pin, status_light_motor3);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, HIGH);
                }
                if (status_light_motor3 != 0 && direction_light_motor3 == "backward")
                {
                    analogWrite(enable1Pin, status_light_motor3);
                    digitalWrite(motor1Pin1, HIGH);
                    digitalWrite(motor1Pin2, LOW);
                }
                if (status_light_motor3 == 0)
                {
                    analogWrite(enable1Pin, status_light_motor3);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, LOW);
                }
            }
            if ((status_light_motor4 != 0 || status_light_motor4 == 0) && Light == value_light_motor4)
            {
                if (status_light_motor4 != 0 && direction_light_motor4 == "forward")
                {
                    analogWrite(enable1Pin, status_light_motor4);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, HIGH);
                }
                if (status_light_motor4 != 0 && direction_light_motor4 == "backward")
                {
                    analogWrite(enable1Pin, status_light_motor4);
                    digitalWrite(motor1Pin1, HIGH);
                    digitalWrite(motor1Pin2, LOW);
                }
                if (status_light_motor4 == 0)
                {
                    analogWrite(enable1Pin, status_light_motor4);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, LOW);
                }
            }
        }

        // code ส่วน manual
        if (status_led == "on")
        {
            digitalWrite(led, HIGH);
            client.publish("led", "online");
        }
        if (status_led == "off")
        {
            digitalWrite(led, LOW);
            client.publish("led", "offline");
        }
        if (status_buzzer == "on")
        {
            digitalWrite(buzzer, HIGH);
        }
        if (status_buzzer == "off")
        {
            digitalWrite(buzzer, LOW);
        }
        if (actuator_servo == "Servo Motor")
        {
            if (status_servo != 0)
            {
                myservo.write(status_servo);
            }
            if (status_servo == 0)
            {
                myservo.write(status_servo);
            }
        }
        if (actuator_motor == "DC Motor")
        {
            if (status_motor != 0)
            {
                if (status_motor_direction == "forward")
                {
                    analogWrite(enable1Pin, status_motor);
                    digitalWrite(motor1Pin1, LOW);
                    digitalWrite(motor1Pin2, HIGH);
                }
                if (status_motor_direction == "backward")
                {
                    analogWrite(enable1Pin, status_motor);
                    digitalWrite(motor1Pin1, HIGH);
                    digitalWrite(motor1Pin2, LOW);
                }
            }
            if (status_motor == 0)
            {
                analogWrite(enable1Pin, status_motor);
                digitalWrite(motor1Pin1, LOW);
                digitalWrite(motor1Pin2, LOW);
            }
        }
        if (status_solenoid == "on")
        {
            digitalWrite(Relay1, HIGH);
        }
        if (status_solenoid == "off")
        {
            digitalWrite(Relay1, LOW);
        }

        // code แสดงสถานะ
        if (status_led == "null" && actuator_led_timer == "null" && actuator_temp_led == "null" && actuator_humidity_led == "null" && actuator_irsensor_led == "null" && actuator_light_led == "null" && actuator_level_led == "null")
        {
            client.publish("led", "offline");
        }
        if (status_led == "on" || actuator_led_timer == "LED" || (actuator_temp_led == "LED" || actuator_humidity_led == "LED" || actuator_irsensor_led == "LED" || actuator_light_led == "LED" || actuator_level_led == "LED"))
        {
            client.publish("led", "online");
        }
        if (status_buzzer == "null" && actuator_buzzer_timer == "null" && actuator_temp_buzzer == "null" && actuator_humidity_buzzer == "null" && actuator_irsensor_buzzer == "null" && actuator_light_buzzer == "null" && actuator_level_buzzer == "null")
        {
            client.publish("buzzer", "offline");
        }
        if (status_buzzer == "on" || actuator_buzzer_timer == "Buzzer" || (actuator_temp_buzzer == "Buzzer" || actuator_humidity_buzzer == "Buzzer" || actuator_irsensor_buzzer == "Buzzer" || actuator_light_buzzer == "Buzzer" || actuator_level_buzzer == "Buzzer"))
        {
            client.publish("buzzer", "online");
        }
        if (status_solenoid == "null" && actuator_solenoid_timer == "null" && actuator_temp_solenoid == "null" && actuator_humidity_solenoid == "null" && actuator_irsensor_solenoid == "null" && actuator_light_solenoid == "null" && actuator_level_solenoid == "null")
        {
            client.publish("solenoid", "offline");
        }
        if (status_solenoid == "on" || actuator_solenoid_timer == "Solenoid" || (actuator_temp_solenoid == "Solenoid" || actuator_humidity_solenoid == "Solenoid" || actuator_irsensor_solenoid == "Solenoid" || actuator_light_solenoid == "Solenoid" || actuator_level_solenoid == "Solenoid"))
        {
            client.publish("solenoid", "online");
        }
        if (actuator_servo == "null" && actuator_servo_timer == "null" && actuator_temp_servo == "null" && actuator_humidity_servo == "null" && actuator_irsensor_servo == "null" && actuator_light_servo == "null" && actuator_level_servo == "null")
        {
            client.publish("servo", "offline");
        }
        if (actuator_servo == "Servo Motor" || actuator_servo_timer == "Servo Motor" || (actuator_temp_servo == "Servo Motor" || actuator_humidity_servo == "Servo Motor" || actuator_irsensor_solenoid == "Servo Motor" || actuator_light_solenoid == "Servo Motor" || actuator_level_solenoid == "Servo Motor"))
        {
            client.publish("servo", "online");
        }
        if (actuator_motor == "null" && actuator_motor_timer == "null" && actuator_temp_motor == "null" && actuator_humidity_motor == "null" && actuator_irsensor_motor == "null" && actuator_light_motor == "null" && actuator_level_motor == "null")
        {
            client.publish("motor", "offline");
        }
        if (actuator_motor == "DC Motor" || actuator_motor_timer == "DC Motor" || (actuator_temp_motor == "DC Motor" || actuator_humidity_motor == "DC Motor" || actuator_irsensor_motor == "DC Motor" || actuator_light_motor == "DC Motor" || actuator_level_motor == "DC Motor"))
        {
            client.publish("motor", "online");
        }
    }
    else
    {
        Serial.println("Error on HTTP request");
        digitalWrite(Relay1, LOW);
        digitalWrite(led, LOW);
        digitalWrite(buzzer, LOW);
        myservo.write(0);
        analogWrite(enable1Pin, 0);
        digitalWrite(motor1Pin1, LOW);
        digitalWrite(motor1Pin2, LOW);
    }
}

void setup()
{
    Serial.begin(115200);
    setupWiFi();
    pinMode(buzzer, OUTPUT);
    pinMode(led, OUTPUT);
    pinMode(motor1Pin1, OUTPUT);
    pinMode(motor1Pin2, OUTPUT);
    pinMode(enable1Pin, OUTPUT);
    pinMode(Relay1, OUTPUT);
    myservo.attach(25);
    myservo.write(0);
    analogWrite(enable1Pin, 0);
    digitalWrite(motor1Pin1, LOW);
    digitalWrite(motor1Pin2, LOW);
    digitalWrite(Relay1, LOW);
    client.setServer(mqtt_server, 1883);
    setupbme280();
    setuptimer();
}

void loop()
{
    if (!client.connected())
    {
        reconnect();
    }
    Actuator_Sensor();
    client.loop();
    if (millis() - last_time > period)
    {
        last_time = millis(); // เซฟเวลาปัจจุบันไว้เพื่อรอจนกว่า millis() จะมากกว่าตัวมันเท่า period
    }
}