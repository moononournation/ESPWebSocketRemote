/*
  This ESP8266 Web Socket Remote based on:
  https://github.com/tzapu/WiFiManager
  https://github.com/Links2004/arduinoWebSockets
  https://gist.github.com/rjrodger/1011032
*/

#define MDNS_NAME "espwsremote"

// Pleaee use PWM pins connect to motor drive
#define LEFT_A 14
#define LEFT_B 12
#define RIGHT_A 13
#define RIGHT_B 15

#define USE_SERIAL Serial

#define CENTER_THRESHOLD 32
#define CENTER_CONSTANT 127
#define IN_MIN 0
#define IN_MAX (127 * 127)
#define LEFT_MIN 32
#define LEFT_MAX 255
#define RIGHT_MIN 32
#define RIGHT_MAX 255

#include <Arduino.h>
#include <ESP8266WiFi.h>

#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <Hash.h>
#include <WebSocketsServer.h>
#include <WiFiManager.h>

ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

uint32_t bound_map(int v, int in_min, int in_max, int out_min, int out_max)
{
  v = max(in_min, min(in_max, v));
  v = (v - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
  return max(out_min, min(out_max, v));
}

void motor(int left, int right)
{
  uint32_t lpwm = bound_map(abs(left), IN_MIN, IN_MAX, LEFT_MIN, LEFT_MAX);
  uint32_t rpwm = bound_map(abs(right), IN_MIN, IN_MAX, LEFT_MIN, LEFT_MAX);
  USE_SERIAL.printf("LEFT: %d %d, RIGHT: %d %d\n", left, lpwm, right, rpwm);

  if (left == 0)
  {
    digitalWrite(LEFT_A, LOW);
    digitalWrite(LEFT_B, LOW);
  }
  else if (left < 0)
  {
    digitalWrite(LEFT_A, LOW);
    analogWrite(LEFT_B, lpwm);
  }
  else
  {
    analogWrite(LEFT_A, lpwm);
    digitalWrite(LEFT_B, LOW);
  }

  if (right == 0)
  {
    digitalWrite(RIGHT_A, LOW);
    digitalWrite(RIGHT_B, LOW);
  }
  else if (right < 0)
  {
    digitalWrite(RIGHT_A, LOW);
    analogWrite(RIGHT_B, rpwm);
  }
  else
  {
    analogWrite(RIGHT_A, rpwm);
    digitalWrite(RIGHT_B, LOW);
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{

  switch (type)
  {
  case WStype_DISCONNECTED:
    USE_SERIAL.printf("[%u] Disconnected!\n", num);
    motor(0, 0);
    break;
  case WStype_CONNECTED:
  {
    IPAddress ip = webSocket.remoteIP(num);
    USE_SERIAL.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

    // send message to client
    webSocket.sendTXT(num, "Connected");
  }
  break;
  case WStype_TEXT:
    USE_SERIAL.printf("[%u] get Text: %s\n", num, payload);

    if (payload[0] == '#')
    {
      long int data = strtol((const char *)&payload[1], NULL, 16);
      int x = (data >> 8) & 0xFF;
      int y = data & 0xFF;
      USE_SERIAL.printf("x: %d, y: %d\n", x, y);

      if (abs(x) < CENTER_THRESHOLD)
      {
        x = 0;
      }
      if (abs(y) < CENTER_THRESHOLD)
      {
        y = 0;
      }
      int leftA = (-y) * ((x < 0) ? (CENTER_CONSTANT + x) : CENTER_CONSTANT);
      int leftB = (x)*abs(CENTER_CONSTANT - y);
      int rightA = (-y) * ((x > 0) ? (CENTER_CONSTANT - x) : CENTER_CONSTANT);
      int rightB = (-x) * abs(CENTER_CONSTANT - y);

      USE_SERIAL.printf("%d %d, %d %d\n", leftA, leftB, rightA, rightB);
      motor(leftA + leftB, rightA + rightB);
    }

    break;
  }
}

void setup()
{
  USE_SERIAL.begin(115200);

  //USE_SERIAL.setDebugOutput(true);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  //reset saved settings
  //wifiManager.resetSettings();
  wifiManager.autoConnect(MDNS_NAME);
  //or use this for auto generated name ESP + ChipID
  //wifiManager.autoConnect();

  pinMode(LEFT_A, OUTPUT);
  pinMode(LEFT_B, OUTPUT);
  pinMode(RIGHT_A, OUTPUT);
  pinMode(RIGHT_B, OUTPUT);

  motor(0, 0);

  // start webSocket server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  if (MDNS.begin(MDNS_NAME))
  {
    USE_SERIAL.println("MDNS responder started");
  }

  // handle index
  server.on("/", []() {
    // send index.html
    server.send(200, "text/html", "<!DOCTYPE html><html><head><meta name='viewport' content='user-scalable=no,initial-scale=1.0,maximum-scale=1.0' /><style>body{padding:0 24px 0 24px;background-color:#ccc;}#main{margin:0 auto 0 auto;}</style><script>function nw(){return new WebSocket('ws://'+location.hostname+':81/',['arduino']);}var ws=nw();window.onload=function(){document.ontouchmove=function(e){e.preventDefault();};var cv=document.getElementById('main');var ctop=cv.offsetTop;var c=cv.getContext('2d');function clr(){c.fillStyle='#fff';c.rect(0,0,255,255);c.fill();}function t(e){e.preventDefault();var x,y,u=e.touches[0];if(u){x=u.clientX;y=u.clientY-ctop;c.beginPath();c.fillStyle='rgba(96,96,208,0.5)';c.arc(x,y,5,0,Math.PI*2,true);c.fill();c.closePath();}else{x=127;y=127;}x='000'+x.toString(16);y='000'+y.toString(16);if(ws.readyState===ws.CLOSED){ws=nw();}ws.send('#'+x.substr(-4)+y.substr(-4));}cv.ontouchstart=function(e){t(e);clr();};cv.ontouchmove=t;cv.ontouchend=t;clr();}</script></head><body><h2>ESP TOUCH REMOTE</h2><canvas id='main' width='255' height='255'></canvas></body></html>");
  });

  server.begin();

  // Add service to MDNS
  MDNS.addService("http", "tcp", 80);
  MDNS.addService("ws", "tcp", 81);
}

unsigned long last_10sec = 0;
unsigned int counter = 0;

void loop()
{
  unsigned long t = millis();
  webSocket.loop();
  server.handleClient();

  if ((t - last_10sec) > 10 * 1000)
  {
    counter++;
    bool ping = (counter % 2);
    int i = webSocket.connectedClients(ping);
    USE_SERIAL.printf("%d Connected websocket clients ping: %d\n", i, ping);
    last_10sec = millis();
  }
}
