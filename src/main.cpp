#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

using namespace std;

AsyncWebServer server(80);
const int ledPin = 2;

enum Condition { ClientConnected, Idle, Off };
volatile Condition serverCondition = Off;

list<Condition> serverConditionQueue;

void blinkWithBlocking(int duration, int count) {
    for (int i = 0; i < count; i++) {
        digitalWrite(ledPin, HIGH);
        delay(duration);
        digitalWrite(ledPin, LOW);
        delay(duration);
    }
}

void indicateClientConnect() {
    blinkWithBlocking(100, 3);
    vTaskDelay(pdMS_TO_TICKS(200));
}

void indicateIdle() {
    blinkWithBlocking(800, 1);
}

void BlinkingTask(void *pv) {
    while (true) {
        //Condition st = serverCondition;
        if (!serverConditionQueue.empty()){
            Condition st = serverConditionQueue.front();
            serverConditionQueue.pop_front();

            if (st == ClientConnected) indicateClientConnect();
        }
        else {
            indicateIdle();
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void setup() {
    Serial.begin(9600);
    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, LOW);

    if (!LittleFS.begin()) {
        Serial.println("Failed to mount LittleFS");
        return;
    }
    else {
        Serial.println("LittleFS mounted");
    }

    char wifiName[32];
    char wifiPassword[32];

    File file = LittleFS.open("/pass", "r");
    if (file) {
        size_t i = 0;

        while (file.available() && i < 32 - 1) {
            char c = file.read();
            if (c == '\n') break;
            wifiName[i++] = c;
        }
        wifiName[i] = '\0';

        i = 0;
        while (file.available() && i < 32 - 1) {
            char c = file.read();
            if (c == '\n') break;
            wifiPassword[i++] = c;
        }
        wifiPassword[i] = '\0';

        file.close();

        Serial.print(wifiName); Serial.print("["); Serial.print(strlen(wifiName)); Serial.print("], ");
        Serial.print(wifiPassword); Serial.print("["); Serial.print(strlen(wifiPassword)); Serial.println("]");
    } else {
        Serial.println("Cant open file.");
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiName, wifiPassword);
    Serial.print("Connecting WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(200);
        Serial.print(".");
    }

    Serial.println("\nConnected, IP: " + WiFi.localIP().toString());

    server.serveStatic("/static", LittleFS, "/static").setCacheControl("max-age=600");

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        Serial.println("[HTTP] / requested → ClientConnected");
        serverConditionQueue.push_front(ClientConnected);
        request->send(LittleFS, "/index.html", "text/html");
    });

    server.begin();
    Serial.println("Server started");

    xTaskCreatePinnedToCore(BlinkingTask, "BlinkTask", 2048, NULL, 1, NULL, 1);
}

void loop() {
    Serial.println("loop");
}