/*
  This ESP8266 Web Socket Remote based on:
  https://github.com/Links2004/arduinoWebSockets
  https://gist.github.com/rjrodger/1011032
*/

#define AP_SSID "YOUR MOBILE HOTSPOT SSID"
#define AP_PASSWORD "YOUR MOBILE HOTSPOT PASSWORD"
#define MDNS_NAME "espwsremote"

#define LEFT_A  D2
#define LEFT_B  D3
#define RIGHT_A D5
#define RIGHT_B D6

#define USE_SERIAL Serial

#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WebSocketsServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Hash.h>

ESP8266WiFiMulti WiFiMulti;

ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

void motor(int left, int right) {
  digitalWrite(LEFT_A, (left < 0) ? LOW : HIGH);
  digitalWrite(LEFT_B, (left > 0) ? LOW : HIGH);
  digitalWrite(RIGHT_A, (right < 0) ? LOW : HIGH);
  digitalWrite(RIGHT_B, (right > 0) ? LOW : HIGH);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

  switch (type) {
    case WStype_DISCONNECTED:
      USE_SERIAL.printf("[%u] Disconnected!\n", num);
      motor(0, 0);
      break;
    case WStype_CONNECTED: {
        IPAddress ip = webSocket.remoteIP(num);
        USE_SERIAL.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

        // send message to client
        webSocket.sendTXT(num, "Connected");
      }
      break;
    case WStype_TEXT:
      USE_SERIAL.printf("[%u] get Text: %s\n", num, payload);

      if (payload[0] == '#') {
        // we get RGB data

        // decode rgb data
        uint32_t data = (uint32_t) strtol((const char *) &payload[1], NULL, 16);
        uint16_t x = (data >> 8) & 0xFF;
        uint16_t y = data & 0xFF;

        USE_SERIAL.printf("x: %d, y: %d\n", x, y);

        if (x < 80) {
          if (y < 80) {
            // left forward
            motor(0, 1);
          } else if (y > 176) {
            // left backward
            motor(0, -1);
          } else {
            // turn anti-clockwise
            motor(-1, 1);
          }
        } else if (x > 176) {
          if (y < 80) {
            // right forward
            motor(1, 0);
          } else if (y > 176) {
            // right backward
            motor(-1, 0);
          } else {
            // turn anti-clockwise
            motor(1, -1);
          }
        } else {
          if (y < 80) {
            // forward
            motor(1, 1);
          } else if (y > 176) {
            // backward
            motor(-1, -1);
          } else {
            // stop
            motor(0, 0);
          }
        }
      }

      break;
  }

}

void setup() {
  USE_SERIAL.begin(115200);

  //USE_SERIAL.setDebugOutput(true);

  for (uint8_t t = 4; t > 0; t--) {
    USE_SERIAL.printf("[SETUP] BOOT WAIT %d...\n", t);
    USE_SERIAL.flush();
    delay(1000);
  }

  pinMode(LEFT_A, OUTPUT);
  pinMode(LEFT_B, OUTPUT);
  pinMode(RIGHT_A, OUTPUT);
  pinMode(RIGHT_B, OUTPUT);

  motor(0, 0);

  WiFiMulti.addAP(AP_SSID, AP_PASSWORD);

  while (WiFiMulti.run() != WL_CONNECTED) {
    delay(100);
  }

  // start webSocket server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  if (MDNS.begin(MDNS_NAME)) {
    USE_SERIAL.println("MDNS responder started");
  }

  // handle index
  server.on("/", []() {
    // send index.html
    server.send(200, "text/html", "<!DOCTYPE html><html><head><meta name='viewport' content='user-scalable=no,initial-scale=1.0,maximum-scale=1.0' /><style>body{padding:0 24px 0 24px;background-color:#ccc;}#main{margin:0 auto 0 auto;}</style><script>function nw(){return new WebSocket('ws://'+location.hostname+':81/',['arduino']);}var ws=nw();window.onload=function(){document.ontouchmove=function(e){e.preventDefault();};var cv=document.getElementById('main');var ctop=cv.offsetTop;var c=cv.getContext('2d');function clr(){c.fillStyle='#fff';c.rect(0,0,255,255);c.fill();}function t(e){e.preventDefault();var x,y,u=e.touches[0];if(u){x=u.clientX;y=u.clientY-ctop;c.beginPath();c.fillStyle='rgba(96,96,208,0.5)';c.arc(x,y,5,0,Math.PI*2,true);c.fill();c.closePath();}else{x=128;y=128;}x=x.toString(16);y=y.toString(16);if(x.length<2){x='0'+x;}if(y.length<2){y='0'+y;}if(ws.readyState===ws.CLOSED){ws=nw();}ws.send('#'+x+y);}cv.ontouchstart=function(e){t(e);clr();};cv.ontouchmove=t;cv.ontouchend=t;clr();}</script></head><body><h2>ESP TOUCH REMOTE</h2><canvas id='main' width='255' height='255'></canvas></body></html>");
  });

  server.begin();

  // Add service to MDNS
  MDNS.addService("http", "tcp", 80);
  MDNS.addService("ws", "tcp", 81);
}

unsigned long last_10sec = 0;
unsigned int counter = 0;

void loop() {
  unsigned long t = millis();
  webSocket.loop();
  server.handleClient();

  if ((t - last_10sec) > 10 * 1000) {
    counter++;
    bool ping = (counter % 2);
    int i = webSocket.connectedClients(ping);
    USE_SERIAL.printf("%d Connected websocket clients ping: %d\n", i, ping);
    last_10sec = millis();
  }
}
