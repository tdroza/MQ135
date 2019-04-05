//Normal Mode
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include "MQ135.h"
#include "HttpUtils.h"

//Config. Mode
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <FS.h>

#define ANALOG_PIN A0
MQ135 gasSensor = MQ135(A0);
String ssid;
String password;
String host;
String url;
int wc_p;                      // max. time in seconds to connect to wifi, before giving up
int gr_p;                      // max. times of attemps to perform GET request, before giving up
int reporting_interval_mins;   // number of minutes between reporting (aka, duration of deep sleep)
bool s_vcc;                    // whether to send VCC voltage as a parameter in the url request
bool is_ip;                    // whether host adress is IP
String vcc_parm;               // parameter to pass VCC voltage by
bool s_lowbattery;             // whether to send low battery notifications
uint32_t lowbattery_threshold; // notifications will be sent when the battery level falls below this threshold
String lowbattery_uri;         // the uri to GET when battery level falls below the threshold

ESP8266WebServer server(80);
File fsUploadFile;

#include "FileFunctions.h"
#include "LEDControl.h"
// Normal Mode
int failCount = 0;
//ADC_MODE(ADC_VCC);
bool su_mode = true;
// Config. Mode
#define CONFIG_PIN 3

const char *APssid = "ESP_Sensor";
const char *APpass = "wifisensor";

void gotoSleep() {
  ESP.deepSleep(reporting_interval_mins * 60 * 1000000);
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  long startMillis = millis();
  pinMode(CONFIG_PIN, INPUT_PULLUP);
  Serial.println("");

  SPIFFS.begin();
  Serial.println();
  Serial.println("ESP Booting...");
  Serial.println("SPIFFS Content: ");
  {
    Dir dir = SPIFFS.openDir("/");
    while (dir.next())
    {
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      Serial.printf("FS File: %s, size: %s\n", fileName.c_str(), formatBytes(fileSize).c_str());
    }
    Serial.printf("\n");
  }

  //read and parse config.json from SPIFFS
  readConfig();

  //read Config. Pin
  su_mode = !digitalRead(CONFIG_PIN);
  if (su_mode)
  {

    //start Config. Mode
    Serial.println("Entering Config. Mode!");

    //start WiFi Access Point
    Serial.println("Configuring access point...");
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(APssid, APpass);

    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);

    //start HTTP server
    server.on("/list", HTTP_GET, handleFileList);
    server.on("/edit", HTTP_GET, []() {
      if (!handleFileRead("/edit.htm"))
        server.send(404, "text/plain", "FileNotFound");
    });
    server.on("/edit", HTTP_PUT, handleFileCreate);
    server.on("/edit", HTTP_DELETE, handleFileDelete);
    server.on("/edit", HTTP_POST, []() {
      server.send(200, "text/plain", "");
    }, handleFileUpload);

    //pages from SPIFFS
    server.onNotFound([]() {
      if (!handleFileRead(server.uri()))
      {
        server.send(404, "text/plain", "FileNotFound");
      }
    });

    server.begin();
    Serial.println("HTTP server started");
  } else {
    //start Normal Mode

    //connect to WiFi
    Serial.println();
    Serial.print("Connecting to WiFi: ");
    Serial.println(ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
      //if we are taking too long to connect to WiFi give up.
      failCount++;
      if (failCount == wc_p * 2) {
        Serial.println("Session Terminated. Giving up after 21 tries connecting to WiFi.");
        Serial.println("entering deep sleep");
        delay(20);
        fail();
        gotoSleep();
      }
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    failCount = 0;
    Serial.println("");
    Serial.println("Preheating sensor...");
    while(millis() - startMillis < 10000) {
      Serial.print(".");
      delay(500);
    }
    Serial.println("");
  }
}

void loop() {
  
  Serial.println("/--- RZero ---/--- Raw ---/--- Resist ---/--- PPM ---/");
  float rzero = gasSensor.getRZero();
  float ppm = gasSensor.getPPM();
  int analogRaw = analogRead(A0);
  float resistance = gasSensor.getResistance();
  Serial.print("/   ");
  Serial.print(rzero);
  Serial.print("    /    ");
  Serial.print(analogRaw);
  Serial.print("     /    ");
  Serial.print(resistance);
  Serial.print("    /   ");
  Serial.print(ppm);
  Serial.println("  /");

  if (su_mode)
  {
    server.handleClient();
  } else {
    //if we have tried too many times to make a GET Request give up.
    ++failCount;
    if (failCount == gr_p + 1)
    {
      Serial.println("Session Terminated. Giving up after 10 tries doing GET Request.");
      Serial.println("entering deep sleep");
      delay(20);
      fail();
      gotoSleep();
    }

    Serial.print("Try: ");
    Serial.println(failCount);
  
    //try to connect to the host with TCP
    WiFiClient client;
    const int httpPort = 80;
    
    Serial.print("connecting to ");
    Serial.println(host);
    if (is_ip)
    {
      IPAddress addr;
      if (addr.fromString(host))
      {
        if (!client.connect(addr, httpPort))
        {
          //try again if the connection fails.
          Serial.println("connection to IP failed");
          delay(10);
          return;
        }
      }
      else
      {
        Serial.println("failed to convert IP String to IP Address.");
        while (1)
          ;
      }
    } else {
      if (!client.connect(host.c_str(), httpPort))
      {
        //try again if the connection fails.
        Serial.println("connection failed");
        delay(10);
        return;
      }
    }
    
    Serial.print("\n PPM: ");
    Serial.println(ppm);
    delay(500);
    addQueryParam(url, "field1", (String)ppm);
    
    //request url to server
    Serial.print("Requesting URL: ");
    Serial.println(url);

    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + host.c_str() + "\r\n" +
                 "Connection: close\r\n\r\n");

    unsigned long timeout = millis();
    while (client.available() == 0)
    {
      if (millis() - timeout > 60000)
      {
        //give up if the server takes too long to reply.
        Serial.println(">>> Client Timeout !");
        client.stop();
        //return;
        Serial.println("entering deep sleep");
        gotoSleep();
      }
    }

    //print response to serial
    while (client.available())
    {
      String line = client.readStringUntil('\r');
      Serial.print(line);
    }
    //finish request and close connections
    client.stop();
    Serial.println();
    Serial.println("closing connection");

    //enter Deep Sleep
    Serial.println("entering deep sleep");
    delay(100);
    yay();
    gotoSleep();
  }
}
