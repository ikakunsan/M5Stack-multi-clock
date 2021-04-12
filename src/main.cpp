/*******************************************************************
M5 Stack Multi Clock
 - NTP based clock
 - Display other info (env data etc) (To be implemented)
 - Send MQTT messages with front buttons (Work status)
s
History
    2020/09/08  Initial
    2021/03/07  Update fonts (preset to TTF)
*******************************************************************/

#include <M5Stack.h>
#include <WiFi.h>
#include "time.h"
#include <PubSubClient.h>
#include <Arduino_JSON.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ESPmDNS.h>
#include "M5FontRender.h"
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

// MQTT messages
const char* mqttMsg1 = "{\"FGColor\":\"WHITE\", \"BGColor\":\"RED\", \"textMain\":\"会議中です\", \"textSub\":\"食事は会議が終わってから\"}";
const char* mqttMsg2 = "{\"FGColor\":\"BLACK\", \"BGColor\":\"YELLOW\", \"textMain\":\"仕事中\", \"textSub\":\"呼ばれたらきりのいいとこで降りていきます\"}";
const char* mqttMsg3 = "{\"FGColor\":\"BLACK\", \"BGColor\":\"GREEN\", \"textMain\":\"今日は仕事終わり\", \"textSub\":\"呼ばれれば降りていきます\"}";

// Font definitions
const char* fontClock       = "/MicrogrammaD-Medium.ttf";
const int   fontClockSizeHM = 64;
const int   fontClockSizeSS = 38;
const char* fontTextEnv     = "/MicrogrammaD-Medium.ttf";
//const char* fontTextEnv     = "/Eurostile.ttf";
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

//
uint8_t hh;
uint8_t mm;
uint8_t ss;
byte omm = 60;
byte oss = 60;
byte xcolon;
byte xsecs;
struct tm timeInfo;
unsigned long now5min;

M5FontRender render;
WiFiClient espClient;
PubSubClient client(espClient);
//const char* rootCACertificate = \
//"-----END CERTIFICATE-----\n";
//const char* rootCACertificate = "-----END CERTIFICATE-----\n";

// Decralation for functions
int isTFTColor(const char* colorName);

// Functions
void callback(char* topic, byte* payload, unsigned int length)
{
    char msgText[256];
    int  msgBGColor;

    for (int i = 0; i < length; i++) {
        msgText[i] = (char)payload[i];
    }
    JSONVar myObject = JSON.parse(msgText);
    msgBGColor = isTFTColor((const char*) myObject["BGColor"]);

//    M5.Lcd.fillRect(0, 210, 320, 10, msgBGColor);
    M5.Lcd.fillRect(0, dispYStatus, 320, dispBtnHeight, msgBGColor);

}

void reconnect()
{
  // Loop until we're reconnected to MQTT broker
    M5.Lcd.setTextFont(1);
    while (!client.connected()) {
        M5.Lcd.fillRect(0, dispYError, 320, 16, TFT_MAGENTA);
        M5.Lcd.setCursor(0, dispYError);
        M5.Lcd.setTextColor(TFT_WHITE);
        M5.Lcd.print("Attempting MQTT connection...");
        // Create a random client ID
        String clientId = "M5Stack-";
        clientId += String(random(0xffff), HEX);
        //// Attempt to connect
        if (client.connect(clientId.c_str())) {
            M5.Lcd.println("connected");
            delay(1000);
            M5.Lcd.fillRect(0, dispYError, 320, 16, TFT_BLACK);
            client.subscribe(thisMqttTopic);
        } else {
            M5.Lcd.fillRect(0, dispYError, 320, 16, TFT_RED);
            M5.Lcd.setCursor(0, dispYError);
            M5.Lcd.setTextColor(TFT_WHITE);
            M5.Lcd.print("failed, rc=");
            M5.Lcd.print(client.state());
            M5.Lcd.println(" try again in 5 seconds");
            //// Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

void setup(void) {
    int secondPassed = 0;

    Serial.begin(115200);
    M5.begin();
  //M5.Power.begin();
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
    MDNS.begin("m5stack01");
    randomSeed(micros());

    M5.Lcd.fillRect(0, dispYError, 320, 16, TFT_CYAN);
    M5.Lcd.setTextColor(TFT_BLACK);
    M5.Lcd.setCursor(0, dispYError);
    M5.Lcd.print("Connected. IP address: ");
    M5.Lcd.println(WiFi.localIP());
    delay(2000);
    M5.Lcd.fillScreen(TFT_BLACK); //Black background

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2, ntpServer3);

    client.setServer(mqttServer, 1883);
    client.setCallback(callback);

    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.setBrightness(127);
    // draw button marks
    M5.Lcd.fillRect(dispXBtn1, dispYButton, 60, dispBtnHeight, TFT_RED);
    M5.Lcd.fillRect(dispXBtn2, dispYButton, 60, dispBtnHeight, TFT_YELLOW);
    M5.Lcd.fillRect(dispXBtn3, dispYButton, 60, dispBtnHeight, TFT_GREEN);
    M5.Lcd.fillRect(0, dispYStatus, 320, dispBtnHeight, TFT_WHITE);

//    M5.Lcd.setCursor(0, dispYEnv);
//    M5.Lcd.setTextFont(4);
//    M5.Lcd.print("22.2 \u00b0C   55.5%   THI 32");
    render.setCursor(0, dispYEnv);
    render.loadFont(fontTextEnv);
    render.setTextSize(fontTextEnvSize);
    render.setTextColor(TFT_WHITE);
    render.printf("22.2 \u00b0C   55.5%%   THI 32");
    render.unloadFont();

    now5min = millis() - 1000*60*6;

    ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) {
            type = "sketch";
        } else { // U_FS
            type = "filesystem";
        }
    });
    ArduinoOTA.begin();
}

void loop() {
    ArduinoOTA.handle();

    getLocalTime(&timeInfo);
    hh = timeInfo.tm_hour;
    mm = timeInfo.tm_min;
    ss = timeInfo.tm_sec;
    HTTPClient http;

    if (oss != ss) {
//        int xpos = 0;
        int ypos = dispYClock;
        int ysecOffset = fontClockSizeHM - fontClockSizeSS;
        // Set font
//        M5.Lcd.setTextFont(4);
        render.loadFont(fontClock);
        render.setTextSize(fontClockSizeHM);
        render.setTextColor(TFT_WHITE, TFT_BLACK);
        render.setCursor(0, dispYClock);
        if (omm != mm) {
            omm = mm;
            M5.lcd.fillRect(0, ypos + 20, 320, fontClockSizeHM, TFT_BLACK);

//            if (hh < 10) render.printf("0");
            render.printf("%02d", hh);
            render.seekCursor(7, -6);
            render.printf(":");
            render.seekCursor(7, 6);
//            if (mm < 10) M5.Lcd.print("0");
            render.printf("%02d", mm);
            xsecs = render.getCursorX();
            render.setTextSize(fontClockSizeSS);
            render.seekCursor(12, ysecOffset);
            xsecs += 12;
//            xsecs = render.getCursorX();
//            render.printf(" ");
//            if (mm < 10) M5.Lcd.print("0");
            render.printf("%02d", ss);
            /*
            if (hh < 10) xpos += M5.Lcd.drawChar('0', xpos, ypos, 8);
                xpos += M5.Lcd.drawNumber(hh, xpos, ypos, 8);
                xcolon = xpos;
                xpos += M5.Lcd.drawChar(':', xpos, ypos - 8, 8);
            if (mm < 10) xpos += M5.Lcd.drawChar('0', xpos, ypos, 8);
                xpos += M5.Lcd.drawNumber(mm, xpos, ypos, 8);
                xsecs = xpos;
            */
        }
        if (oss != ss) {
            oss = ss;
            M5.lcd.fillRect(xsecs, ypos + ysecOffset + 10, 76, fontClockSizeSS, TFT_BLACK);
            render.setTextSize(fontClockSizeSS);
            render.setCursor(xsecs, ypos + ysecOffset);
            render.printf("%02d", ss);
            /*
            xpos = xsecs;
            xpos += M5.Lcd.drawChar(' ', xsecs, ysecs, 6);
            if (ss < 10) xpos += M5.Lcd.drawChar('0', xpos, ysecs, 6);
            M5.Lcd.drawNumber(ss, xpos, ysecs, 6);
            */
        }
        render.unloadFont();
    }
    M5.update();

/*
    if ((millis() - now5min) > 1000*60*5) {
        // access site per 5 mins
        now5min = millis();
        http.begin(currencyRateUrl);
        int httpCode = http.GET();
        if (httpCode > 0) {
//          String payload = http.getString();
//          M5.Lcd.print(payload);
        M5.Lcd.print(httpCode);
    }
    http.end();
    }
*/

    if (!client.connected()) {
        reconnect();
    }
    client.loop();

    if (M5.BtnA.wasReleased()) {
        M5.Lcd.fillRect(0, dispYStatus, 320, dispBtnHeight, TFT_RED);
        client.publish(thisMqttTopic, mqttMsg1);
    } else if (M5.BtnB.wasReleased()) {
        M5.Lcd.fillRect(0, dispYStatus, 320, dispBtnHeight, TFT_YELLOW);
        client.publish(thisMqttTopic, mqttMsg2);
    } else if (M5.BtnC.wasReleased()) {
        M5.Lcd.fillRect(0, dispYStatus, 320, dispBtnHeight, TFT_GREEN);
        client.publish(thisMqttTopic, mqttMsg3);
    }
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
