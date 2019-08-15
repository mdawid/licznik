#include <WiFiManager.h>

/*
  ESP8266 Blink by Simon Peter
  Blink the blue LED on the ESP-01 module
  This example code is in the public domain

  The blue LED on the ESP-01 module is connected to GPIO1
  (which is also the TXD pin; so we cannot use Serial.print() at the same time)

  Note that this sketch uses LED_BUILTIN to find the pin with the internal LED
*/

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <DoubleResetDetect.h>
#include <CStringBuilder.h>
#include <Ticker.h>

#include "config.h"

// maximum number of seconds between resets that
// counts as a double reset
#define DRD_TIMEOUT 2.0
// address to the block in the RTC user memory
// change it if it collides with another usage
// of the address block
#define DRD_ADDRESS 0x00

#define WIFI_RETRY_DELAY_S 5*60
#define WIFI_CONNECTION_TIMEOUT_S 30
#define CONNECTION_RETRY_COUNT 3
#define DATA_READ_TIMEOUT_S 10

DoubleResetDetect drd(DRD_TIMEOUT, DRD_ADDRESS);

String ssid = "";
String password = "";

const char* host = "docs.google.com";
const int httpsPort = 443;

// Use web browser to view and copy
// SHA1 fingerprint of the certificate
const char* fingerprint = "‎c8 47 46 6b 7b bb d0 d4 a3 1c e9 7a 40 74 9e cc ba fa 5e c0";

const char* formId = FORM_TOKEN;

enum State {
  LO,
  HI
};

#define TICK_READ_STEP 0.1 // seconds
#define TICK_READ_TIME 60 // seconds
#define READS_TILL_SAVE (TICK_READ_TIME/TICK_READ_STEP)

#define SEND_DELAY (5*60) //seconds
#define READS_TO_SEND (SEND_DELAY / TICK_READ_TIME)
#define MAX_FAILED_SENDS 128  // to znaczy ze mozemy zbuforowac do 128" nieudanych prob wyslania danych
#define READS_SIZE (READS_TO_SEND * MAX_FAILED_SENDS )

#define HI_THRESHOLD 250
#define LO_THRESHOLD 150

volatile State currentState = LO;
volatile int steps = 0;
volatile int idx = 0;

int saved_reads[READS_SIZE];

Ticker flipper;

void readA0() {
  int sensorValue = analogRead(A0);
  
  steps++;
  
  State lastState = currentState;
  if ( sensorValue > HI_THRESHOLD ) {
    if ( currentState == LO ) {
      currentState = HI;
    }
  } else if ( sensorValue < LO_THRESHOLD ) {
    if ( currentState == HI ) {
      currentState = LO;
    }
  }

  if ( currentState == LO ) {
      digitalWrite(D7, LOW );
  } else {
      digitalWrite(D7, HIGH );  
  }
  
  if ( (lastState == LO) && (currentState == HI) ) {
    saved_reads[idx]++;
  }

  if ( steps >= READS_TILL_SAVE ) {
    idx = (idx + 1) % READS_SIZE;
    saved_reads[idx] = 0;
    steps = 0;
    Serial.println( "*" );
  }
}

void advance() {
  steps = READS_TILL_SAVE;
}

void connect() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println( "Already connected" );
    return;
  }

  Serial.println();
  Serial.print("Connecting to: " );
  Serial.println( ssid );

  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  while(WiFi.status() != WL_CONNECTED) {
    uint32_t wifiTo = millis() + WIFI_CONNECTION_TIMEOUT_S * 1000; 
    do {
      if (WiFi.status() == WL_CONNECTED)
        break;
      delay(500);
      Serial.print(".");
    } while(millis() < wifiTo);
    delay(WIFI_RETRY_DELAY_S);
  }
  
  Serial.println("WiFi connected");
  Serial.print("IP address: " );
  Serial.println( WiFi.localIP() );
}

int send( int rssi, int ticksReadTime, String ticksBuffer ) {

  WiFiClientSecure client;

  connect();
  
  boolean connected;
  int counter = 0;
  while (!client.connected() && counter < CONNECTION_RETRY_COUNT) {
    Serial.print("Connecting to: " );
    Serial.println(host);
    connected = client.connect(host, httpsPort);
    if (connected) { 
      break; 
    }
    
    client.stop();
    
    delay(1000);
    Serial.println("retry");
    counter++;
  }

  if ( !client.connected() )
    return -1;

  //if (client.verify(fingerprint, host)) {
  //  Serial.println("Certificate matches");
  //} else {
  //  Serial.println("Certificate doesn't match");
  //}

  char buff[1024];
  CStringBuilder sb(buff,sizeof(buff));
  sb.printf( "/forms/d/e/%s/formResponse", formId );
  sb.print( "?ifq" );

  sb.printf( "&entry.545124288=%d", rssi );
  sb.printf( "&entry.733810777=%d", ticksReadTime );
  sb.print( "&entry.1707835926=" + ticksBuffer );
  sb.print( "&fvv=1");
  
  sb.print( "&submit=Submit" );
  Serial.print( "Requesting URL: " );
  Serial.println( buff );
  client.print(String("GET ") + buff + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "Connection: close\r\n" +
               "Accept-Encoding: gzip, deflate, br\r\n\r\n");
  client.flush();
  
  Serial.println("Request sent");

  int c = '\0';
  int i = 0;
  unsigned long startTime = millis();
  unsigned long httpResponseTimeOut = DATA_READ_TIMEOUT_S * 1000; // 10 sec
  while (client.connected() && ((millis() - startTime) < httpResponseTimeOut)) {
      if (client.available()) {
          c = client.read();
      } else {
          delay(100);
      }
      if ( i > 1024 ) {
        Serial.print( '.' );
        yield();
        i = 0;
      }
      i++;
  }

  Serial.println("Closing connection...");
  client.stop();

  /* To jest odczyt ogromnego body, ktore obecnie olewamy
  uint32_t to = millis() + DATA_READ_TIMEOUT_S * 1000;
  char tmp[2048];
  while(client.available()) {
    if ( millis() > to ) {
        Serial.println("Closing connection");
        client.stop();
        break;
    }
    int rlen = client.read((uint8_t*)tmp, sizeof(tmp) - 1);
    Serial.print("<");
    if (rlen < 0) break;
    yield();
  };*/

  return 0;
}

int start_idx = 0;

void setup() {
  Serial.begin(115200);
  pinMode(D7, OUTPUT);
  //pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  pinMode(A0, INPUT);

  flipper.attach(TICK_READ_STEP, readA0);
  
  if (drd.detect()) {
    Serial.println("** Double reset boot **");
    //WiFiManager
    WiFiManager wifiManager;

    //reset settings - for testing
    //wifiManager.resetSettings();

    //sets timeout until configuration portal gets turned off
    //useful to make it all retry or go to sleep
    //in seconds
    wifiManager.setTimeout(120);
    wifiManager.setConnectTimeout(30);
    wifiManager.setBreakAfterConfig(true);

    //it starts an access point with the specified name
    //and goes into a blocking loop awaiting configuration

    //WITHOUT THIS THE AP DOES NOT SEEM TO WORK PROPERLY WITH SDK 1.5 , update to at least 1.5.1
    //WiFi.mode(WIFI_STA);
    if (!wifiManager.startConfigPortal("TermometrAP")) {
      Serial.println("Failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(5000);
    } else {
      Serial.print("***SSID: ");
      Serial.println( WiFi.SSID() );
      Serial.print("***Password: ");
      Serial.println( WiFi.psk() );
    }
  }

  //WiFi.disconnect(true);

  ssid = WiFi.SSID();
  password = WiFi.psk();

  while(1) {
    int end_idx = idx;
    // zakoncz zliczanie aktywnego licznika
    advance();

    // tutaj mozemy spokojnie zebrac dane do wyslania
    Serial.println();
    Serial.print( "[" );
    Serial.print( start_idx );
    Serial.print( ":" );
    Serial.print( end_idx );
    Serial.print( "(" );
    // bufor kolowy. indeks koncowy moze byc mniejszy niz startowy. rozwijamy wtedy bufor tak, zeby indeks koncowy byl wiekszy od startowego
    int count = ( (start_idx <= end_idx) ? (end_idx - start_idx) : ((READS_SIZE + end_idx) - start_idx) ) + 1;
    Serial.print( count );
    Serial.print( ")" );
  
    String ticksBuffer = "";
    int i = start_idx;
    for( ;i < start_idx + count; i++ ) {
      int ticks = saved_reads[i % READS_SIZE];
      if ( i != start_idx ) {
        ticksBuffer = ticksBuffer + ",";
      }
      ticksBuffer = ticksBuffer + ticks;
    }
    Serial.print( ticksBuffer );
    Serial.println("]");

    int rssi = WiFi.RSSI();
    if (send( rssi, TICK_READ_TIME, ticksBuffer ) != 0 )
      continue;

    // nastepnym razem wyslij kolejne liczniki
    start_idx = i % READS_SIZE;

    delay(SEND_DELAY * 1000);
  }
}

void loop() {}
