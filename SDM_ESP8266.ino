#define USE_HARDWARESERIAL
#define READSDMEVERY  2000                                                       //read sdm every 2000ms
#define NBREG   10                                                               //number of sdm registers to read
//#define USE_STATIC_IP

/*  WEMOS D1 Mini                            
                     ______________________________                
                    |   L T L T L T L T L T L T    |
                    |                              |
                 RST|                             1|TX HSer
                  A0|                             3|RX HSer
                  D0|16                           5|D1
                  D5|14                           4|D2
                  D6|12                    10kPUP_0|D3
RX SSer/HSer swap D7|13                LED_10kPUP_2|D4
TX SSer/HSer swap D8|15                            |GND
                 3V3|__                            |5V
                       |                           |
                       |___________________________|
*/

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <ESPAsyncTCP.h>                                                        // https://github.com/me-no-dev/ESPAsyncTCP
#include <ESPAsyncWebServer.h>                                                  // https://github.com/me-no-dev/ESPAsyncWebServer
#include <SDM.h>                                                                // https://github.com/reaper7/SDM_Energy_Meter
#include "config.h"

#include "index_page.h"

extern const char* ssid;
extern const char* password;
extern const char* ota_password;
extern const char* ota_hostname;
//extern const int ota_port;
//extern const int serial_speed;

//------------------------------------------------------------------------------
AsyncWebServer server(80);

#if !defined ( USE_HARDWARESERIAL )                                             // SOFTWARE SERIAL
  SDM<2400, 13, 15, NOT_A_PIN> sdm;                                               // baud, rx_pin, tx_pin, de/re_pin(not used in this example)
#else                                                                           // HARDWARE SERIAL
  SDM<2400, NOT_A_PIN, false> sdm;                                                // baud, de/re_pin(not used in this example), uart0 pins 3/1(false) or 13/15(true)
#endif
//------------------------------------------------------------------------------

#if defined ( USE_STATIC_IP )
  IPAddress ip(123, 123, 123, 123);
  IPAddress gateway(123, 123, 123, 123);
  IPAddress subnet(255, 255, 255, 0);
#endif

String lastresetreason = "";

unsigned long readtime;
//------------------------------------------------------------------------------
typedef volatile struct {
  volatile float regvalarr;
  const uint16_t regarr;
} sdm_struct;

volatile sdm_struct sdmarr[NBREG] = {
  {0.00, SDM120C_VOLTAGE},                        //V
  {0.00, SDM120C_CURRENT},                        //A
  {0.00, SDM120C_POWER},                          //W
  {0.00, SDM120C_POWER_FACTOR},                   //
  {0.00, SDM120C_FREQUENCY},                      //Hz
  {0.00, SDM120C_ACTIVE_APPARENT_POWER},          //VA
  {0.00, SDM120C_REACTIVE_APPARENT_POWER},        //VAR
  {0.00, SDM120C_IMPORT_ACTIVE_ENERGY},           //Wh
  {0.00, SDM120C_EXPORT_ACTIVE_ENERGY},           //Wh
  {0.00, SDM120C_TOTAL_ACTIVE_ENERGY},            //Wh
};
//------------------------------------------------------------------------------
void xmlrequest(AsyncWebServerRequest *request) {
  String XML = F("<?xml version='1.0'?><xml>");
  for (int i = 0; i < NBREG; i++) { 
    XML += "<response" + (String)i + ">";
    XML += String(sdmarr[i].regvalarr,2);
    XML += "</response" + (String)i + ">";
  }
  XML += F("<freeh>");
  XML += String(ESP.getFreeHeap());
  XML += F("</freeh>");
  XML += F("<rst>");
  XML += lastresetreason;
  XML += F("</rst>");
  XML += F("</xml>");
  request->send(200, "text/xml", XML);
}
//------------------------------------------------------------------------------
void indexrequest(AsyncWebServerRequest *request) {
  request->send_P(200, "text/html", index_page); 
}
//------------------------------------------------------------------------------
void ledOn() {
  digitalWrite(13, HIGH);
}
//------------------------------------------------------------------------------
void ledOff() {
  digitalWrite(13, LOW);
}
//------------------------------------------------------------------------------
void ledSwap() {
  digitalWrite(13, !digitalRead(13));
}
//------------------------------------------------------------------------------
void otaInit() {
  //ArduinoOTA.setPort(ota_port);
  ArduinoOTA.setHostname(ota_hostname);
  ArduinoOTA.setPassword(ota_password);

  ArduinoOTA.onStart([]() {
    ledOn();
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    ledSwap();
  });
  ArduinoOTA.onEnd([]() {
    ledOff();
  });
  ArduinoOTA.onError([](ota_error_t error) {
    ledOff();
  });
  ArduinoOTA.begin();
}
//------------------------------------------------------------------------------
void serverInit() {
  server.on("/", HTTP_GET, indexrequest);
  server.on("/xml", HTTP_PUT, xmlrequest);
  server.onNotFound([](AsyncWebServerRequest *request){
    request->send(404);
  });
  server.begin();
}
//------------------------------------------------------------------------------
static void wifiInit() {
  WiFi.persistent(false);                                                       // Do not write new connections to FLASH
  WiFi.mode(WIFI_STA);
#if defined ( USE_STATIC_IP )
  WiFi.config(ip, gateway, subnet);                                             // Set fixed IP Address
#endif
  WiFi.begin(ssid, password);

  while( WiFi.status() != WL_CONNECTED ) {                                      //  Wait for WiFi connection
    ledSwap();
    delay(100);
  }
}
//------------------------------------------------------------------------------
void sdmRead() {
  float tmpval = NAN;

  for (uint8_t i = 0; i < NBREG; i++) {
    tmpval = sdm.readVal(sdmarr[i].regarr);

    if (isnan(tmpval))
      sdmarr[i].regvalarr = 0.00;
    else
      sdmarr[i].regvalarr = tmpval;

    yield();
  }
}
//------------------------------------------------------------------------------
void setup() {
  pinMode(13, OUTPUT);
  ledOn();

  lastresetreason = ESP.getResetReason();

  wifiInit();
  otaInit();
  serverInit();
  sdm.begin();

  readtime = millis();
  ledOff();
}
//------------------------------------------------------------------------------
void loop() {
  ArduinoOTA.handle();
  if (millis() - readtime >= READSDMEVERY) {
    sdmRead();
    readtime = millis();
  }
  yield();
}
