/*
  ESP8266 Blink by Simon Peter
  Blink the blue LED on the ESP-01 module
  This example code is in the public domain

  The blue LED on the ESP-01 module is connected to GPIO1
  (which is also the TXD pin; so we cannot use Serial.print() at the same time)

  Note that this sketch uses LED_BUILTIN to find the pin with the internal LED
*/

#include <Ticker.h>

Ticker flipper;

enum State {
  LO,
  HI
};

#define TICK_READ_STEP 0.1 // seconds
#define TICK_READ_TIME 10 // seconds
#define READS_TILL_SAVE (TICK_READ_TIME/TICK_READ_STEP)

#define SEND_DELAY (30) //seconds
#define READS_TO_SEND (SEND_DELAY / TICK_READ_TIME)
#define READS_SIZE (READS_TO_SEND * 2) // double buffer

#define HI_THRESHOLD 250
#define LO_THRESHOLD 150

volatile State currentState = LO;
volatile int steps = 0;
volatile int idx = 0;
int saved_reads[READS_SIZE];

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

void setup() {
  Serial.begin(115200);
  pinMode(D7, OUTPUT);
  //pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  pinMode(A0, INPUT);

  flipper.attach(TICK_READ_STEP, readA0);
}

int start_idx = 0;

void loop() {
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
  int count = ( start_idx <= end_idx ) ? ((end_idx - start_idx) + 1) : ( ( ( READS_SIZE + end_idx ) - start_idx ) + 1 );
  Serial.print( count );
  Serial.print( ")" );
  int i = start_idx;
  for( ;i < start_idx + count; i++ ) {
    Serial.print( saved_reads[i % READS_SIZE] );
    Serial.print( "," );
  }
  Serial.println("]");
  
  // nastepnym razem wyslij kolejne liczniki
  start_idx = i % READS_SIZE;
  delay(SEND_DELAY * 1000);
}
