/*
M5Stack MQTT message board

Display MQTT payload to M5Stack LCD

topic: mqttmsgboard1
message: JSON
    {
        "FGcolor"  : color,
        "BGcolor"  : color,
        "textEng"  : "Primary text (English)",
        "textMain" : "Primary text",
        "textSub"  : "Sub text"
    }

2020/09/06  Initial. wio Terminalから移植（BME280関係は削除）

*/

#include <M5Stack.h>
#include <WiFi.h>
#include "time.h"
#include <PubSubClient.h>
#include <Arduino_JSON.h>
#include "M5FontRender.h"
#include <LovyanGFX.hpp>
#include <ArduinoOTA.h>
#include "../../include/wifiinfo.h"

// WiFi and MQTT related
const char* ssid          = MY_WIFI_SSID;
const char* wifipassword  = MY_WIFI_PASSWORD;
const char* mqttServer    = MY_MQTT_BROKER;
const char* thisMqttTopic = "mqttmsgboard1";
//NTP related
const char* ntpServer1 = MY_NTP_SERVER1;
const char* ntpServer2 = MY_NTP_SERVER2;
const char* ntpServer3 = MY_NTP_SERVER3;
const long  gmtOffset_sec = 3600 * 9;
const int   daylightOffset_sec = 0; // 3600;

const int LCDmaxX = 319; // 320 x 240
const int LCDmaxY = 239;
int LCDstatusY = 0;
int LCDtopicY = 60;
int LCDsubtopicY = 200;
int msgFGColor = TFT_BLACK;
int msgBGColor = TFT_GREEN;
int prevUpdate = 0;

// MQTT messages
const char* mqttMsg1 = "{\"FGColor\":\"WHITE\", \"BGColor\":\"RED\", \"textMain\":\"会議中です\", \"textSub\":\"食事は会議が終わってから\"}";
const char* mqttMsg2 = "{\"FGColor\":\"BLACK\", \"BGColor\":\"YELLOW\", \"textMain\":\"仕事中\", \"textSub\":\"呼ばれたらきりのいいとこで降りていきます\"}";
const char* mqttMsg3 = "{\"FGColor\":\"BLACK\", \"BGColor\":\"GREEN\", \"textMain\":\"今日は仕事終わり\", \"textSub\":\"呼ばれれば降りていきます\"}";

// Font definitions
const char* fontClock       = "/MicrogrammaD-Medium.ttf";
const int   fontClockSizeHM = 64;
const int   fontClockSizeSS = 38;
const char* fontTextEnv     = "/MicrogrammaD-Medium.ttf";
const int   fontTextEnvSize = 24;

// Display Positions
// Buttons
const int dispXBtn1  = 35;   // width 60
const int dispXBtn2  = 130;  // width 60
const int dispXBtn3  = 225;  // width 60
const int dispBtnHeight = 5;    // height
// Y positions
const int dispYEnv    = 0;
const int dispYClock  = 80;
const int dispYError  = 200;
const int dispYStatus = 220;
const int dispYButton = 235;
const int ysecOffset = fontClockSizeHM - fontClockSizeSS;

M5FontRender render;
WiFiClient espClient;
PubSubClient client(espClient);

int isTFTColor(const char* colorName);

void callback(char* topic, byte* payload, unsigned int length) {
    // Screen Y assignment (0 - 239)
    //   0 -  27: Temp, Humidity (font28 x1)
    //  28 -  37: Space
    //  38 -  57: Color band
    //  58 - 137: Color band & message (font40 x2)
    // 138 - 157: Color band
    // 158 - 205: Additional text (font24 x2)
    // 206 - 215: Space
    // 216 - 239: Status (font12 x2)
    
    char msgText[256];
//    char textMain[40], textSub[80];
    String submsg;

    for (int i = 0; i < (int)length; i++) {
        msgText[i] = (char)payload[i];
    }

    JSONVar myObject = JSON.parse(msgText);

  // JSON.typeof(jsonVar) can be used to get the type of the var
    if (JSON.typeof(myObject) == "undefined") {
        M5.Lcd.setCursor(0, 20);
        M5.Lcd.setTextColor(TFT_RED);
        M5.Lcd.setTextFont(1);
        M5.Lcd.setTextSize(1);
        M5.Lcd.println("Parsing input failed!");
        M5.Lcd.println(msgText);
        return;
    }

    msgFGColor = isTFTColor((const char*) myObject["FGColor"]);
    msgBGColor = isTFTColor((const char*) myObject["BGColor"]);

    if (strcmp((const char*) myObject["textMain"], "") != 0) {
//        M5.Lcd.fillRect(0,110, 319, 50, msgBGColor);
//        M5.Lcd.setCursor(0, 115);
        M5.Lcd.fillRect(0, 50, 319, 80, msgBGColor);
        M5.Lcd.setCursor(0, 65);
        M5.Lcd.loadFont("Gothic-48", SD);
        M5.Lcd.setTextColor(msgFGColor, msgBGColor);
        M5.Lcd.print((const char*) myObject["textMain"]);
        M5.Lcd.unloadFont();
    }

    if (strcmp((const char*) myObject["textSub"], "") != 0) {
//        submsg = (const char*) myObject["textSub"];
        M5.Lcd.fillRect(0, 146, 320, 84, TFT_BLACK);
        M5.Lcd.loadFont("Gothic-24", SD);
        M5.Lcd.setCursor(0, 150);
        M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Lcd.print((const char*) myObject["textSub"]);
//        M5.Lcd.print(submsg.substring(0,10));
        M5.Lcd.unloadFont();
    }
}

void reconnect() {
    // Loop until we're reconnected
    while (!client.connected()) {
        M5.Lcd.setCursor(0, 230);
        M5.Lcd.setTextFont(1);
        M5.Lcd.print("Attempting MQTT connection...");
        // Create a random client ID
        String clientId = "M5Stack-";
        clientId += String(random(0xffff), HEX);
        //// Attempt to connect
        if (client.connect(clientId.c_str())) {
            // M5.Lcd.println("connected");
            //// ... and subscribe to topic
            client.subscribe(thisMqttTopic);
            M5.Lcd.fillRect(0, 230, 319, 10, TFT_BLACK);
            M5.Lcd.setCursor(0, 230);
            M5.Lcd.setTextColor(TFT_GREEN, TFT_BLACK);
            M5.Lcd.print("Connected");
            delay(3000);
            //M5.Lcd.fillRect(0, LCDmaxY-20, LCDmaxX, 20, TFT_BLACK);
            M5.Lcd.fillRect(0, dispYError, 320, 40, TFT_BLACK);
        } else {
            M5.Lcd.fillRect(0, 230, 319, 10, TFT_RED);
            delay(500);
            M5.Lcd.setCursor(0, 230);
            M5.Lcd.setTextColor(TFT_WHITE, TFT_RED);
            M5.Lcd.print("failed, rc=");
            M5.Lcd.print(client.state());
            M5.Lcd.print(" try again in 5 seconds");
            //// Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

void setup(void) {
    int secondPassed = 0;

    Serial.begin(115200);
    M5.begin();
    M5.Lcd.fillScreen(TFT_BLACK);

    M5.Lcd.fillRect(0, dispYError, 320, 16, TFT_BLACK);
    M5.Lcd.setTextFont(1);
    M5.Lcd.setTextColor(TFT_CYAN);
    M5.Lcd.setCursor(0, dispYError);
    M5.Lcd.print("Connecting to ");
    M5.Lcd.print(ssid);

    if(WiFi.begin(ssid, wifipassword) != WL_DISCONNECTED) {
        ESP.restart();
    }
    while (WiFi.status() != WL_CONNECTED) {
        delay(2000);
        M5.Lcd.print(".");
        secondPassed += 1;
        if (secondPassed > 20) {
            ESP.restart();
        }
    }
    randomSeed(micros());
    client.setServer(mqttServer, 1883);
    client.setCallback(callback);

    M5.Lcd.fillRect(0, dispYError, 320, 16, TFT_CYAN);
    M5.Lcd.setTextColor(TFT_BLACK);
    M5.Lcd.setCursor(0, dispYError);
    M5.Lcd.print("Connected. IP address: ");
    M5.Lcd.println(WiFi.localIP());
    delay(1000);

}

void loop() {
//    ArduinoOTA.handle();
    if (!client.connected()) {
        reconnect();
    }
    client.loop();
}

int isTFTColor(const char* colorName) {
    if (strcmp(colorName, "BLACK") == 0) {
        return TFT_BLACK;
    } else if (strcmp(colorName, "NAVY") == 0) {
        return TFT_NAVY;
    } else if (strcmp(colorName, "DARKBLUE") == 0) {
        return TFT_NAVY;
    } else if (strcmp(colorName, "DARKCYAN") == 0) {
        return TFT_DARKCYAN;
    } else if (strcmp(colorName, "BLUE") == 0) {
        return TFT_BLUE;
    } else if (strcmp(colorName, "GREEN") == 0) {
        return TFT_GREEN;
    } else if (strcmp(colorName, "RED") == 0) {
        return TFT_RED;
    } else if (strcmp(colorName, "CYAN") == 0) {
        return TFT_CYAN;
    } else if (strcmp(colorName, "MAGENTA") == 0) {
        return TFT_MAGENTA;
    } else if (strcmp(colorName, "YELLOW") == 0) {
        return TFT_YELLOW;
    } else if (strcmp(colorName, "WHITE") == 0) {
        return TFT_WHITE;
    } else {
        return TFT_PURPLE;
    }
}