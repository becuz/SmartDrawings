/*
  Trigger a Restmote command touching pin-attached pencil figures.
  Works on an Arduino Ethernet or a Ethernet Shield.
  
  See http://www.restmote.com/ for details.
  Credits for the uint8_t readCapacitivePin function: http://www.instructables.com/id/Turn-a-pencil-drawing-into-a-capacitive-sensor-for/ 
*/
#include <SPI.h>
#include <Ethernet.h>

const int ledPin = 9;                             // pin of connection led
const int numPins = 6;                            // num of pins to connect to your drawing, ie size of capSensePins
const int capSensePins[] = {2, 3, 4, 5, 6, 7, 8}; // pins to connect to your drawing
const char* calls[] = {                           // calls associated to each pin
  "/api/pc/keyboard/1", 
  "/api/pc/keyboard/2",
  "/api/pc/keyboard/3",
  "/api/pc/keyboard/4",
  "/api/pc/keyboard/5",
  "/api/pc/keyboard/6"
};
const int touchedCutoff = 60;                    //sensibility cutoff for d
const long pauseNetCalls = 0;                    //
const boolean debug = false;                     //determines if to print in the serial line general messages
const boolean debugNet = false;                  //determines if to print in the serial line net traffic 

//ethernet
EthernetClient client;
byte mac[] = { 0x00, 0xA2, 0xDA, 0x00, 0xF2, 0x87 };
//IPAddress gateway(192,168,0,1);                //uncomment the following three lines if no dhcp is available or preferred
//IPAddress subnet(255,255,255,0);
//IPAddress ip(192,168,0,12);

//restmote server
char serverRestmote[] = "192.168.0.11";  
int port = 9898;

long lastSentCommand = 0;
boolean lastConnected = true;

void setup(){
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }
  Serial.println("Setup..");
  pinMode(ledPin, OUTPUT);
  
  //Ethernet.begin(mac, ip, gateway, subnet);  //no dhcp
  if (Ethernet.begin(mac) == 0) {              //dhcp
    Serial.println("Failed to configure Ethernet using DHCP");
    for(;;){     // no point to continue, blink the led to signal the error
      digitalWrite(ledPin, HIGH);  
      delay(500);
      digitalWrite(ledPin, LOW);  
      delay(500);
    }
  }
  delay(2000);   // give the Ethernet shield time to initialize
  while(true){   // make a test GET
    if (client.connect(serverRestmote, port)) {
      Serial.println("connected");
      client.println("GET /api/test/ping HTTP/1.0");  
      client.println();
      digitalWrite(ledPin, HIGH);  //signal that everything is ready and working
      delay(500);
      break;
    } else {
      Serial.println("connection failed");
    }
  }
}

void loop(){  
  if (client.available()) { //data from the ethernet connection
    char c = client.read();  
    if (debugNet){
      Serial.print(c);
    }
  }

  // if there's no net connection, but there was one last time
  // through the loop, then stop the client
  if (!client.connected() && lastConnected) {
    if (debug){
      Serial.println("disconnecting.");
    }
    client.stop();
  }
  
  digitalWrite(ledPin, LOW);
  // If the capacitive sensor reads above a certain threshold turn on the LED and send the restmote command
  for(int i=0;i<numPins;i++){
    if (readCapacitivePin(capSensePins[i]) > touchedCutoff) {
      digitalWrite(ledPin, HIGH);
      if (!client.connected()){// && millis() - lastSentCommand > pauseNetCalls){
        remoskoSendCommands(i);
        lastSentCommand = millis();
      }
    }
  }
  
  if (debug && (millis() % 10) == 0){
    for(int i=0;i<numPins;i++){
      Serial.print("Capacitive Sensor on pin ");
      Serial.print(capSensePins[i]);
      Serial.print(" ");
      Serial.println(readCapacitivePin(capSensePins[i]));
    }
  }
  
  lastConnected = client.connected();
}

//send the calls[index] HTTP POST
void remoskoSendCommands(int index) {
  if (debug){
    Serial.print("remoskoSendCommands ");
    Serial.print(index);
  }
  if (client.connect(serverRestmote, port)) {
    Serial.println("connecting...");
    client.print("POST "); 
    client.print(calls[index]);
    client.println(" HTTP/1.0");
    client.println("Connection: close");
    client.println();
  }  else {
    Serial.println("connection failed, disconnecting.");
    client.stop();
  }
}


// readCapacitivePin
//  Input: Arduino pin number
//  Output: A number, from 0 to 17 expressing
//          how much capacitance is on the pin
//  When you touch the pin, or whatever you have
//  attached to it, the number will get higher
//  In order for this to work now,
// The pin should have a 1+Megaohm resistor pulling
//  it up to +5v.
uint8_t readCapacitivePin(int pinToMeasure){
  // This is how you declare a variable which
  //  will hold the PORT, PIN, and DDR registers
  //  on an AVR
  volatile uint8_t* port;
  volatile uint8_t* ddr;
  volatile uint8_t* pin;
  // Here we translate the input pin number from
  //  Arduino pin number to the AVR PORT, PIN, DDR,
  //  and which bit of those registers we care about.
  byte bitmask;
  if ((pinToMeasure >= 0) && (pinToMeasure <= 7)){
    port = &PORTD;
    ddr = &DDRD;
    bitmask = 1 << pinToMeasure;
    pin = &PIND;
  }
  if ((pinToMeasure > 7) && (pinToMeasure <= 13)){
    port = &PORTB;
    ddr = &DDRB;
    bitmask = 1 << (pinToMeasure - 8);
    pin = &PINB;
  }
  if ((pinToMeasure > 13) && (pinToMeasure <= 19)){
    port = &PORTC;
    ddr = &DDRC;
    bitmask = 1 << (pinToMeasure - 13);
    pin = &PINC;
  }
  // Discharge the pin first by setting it low and output
  *port &= ~(bitmask);
  *ddr  |= bitmask;
  delay(1);
  // Make the pin an input WITHOUT the internal pull-up on
  *ddr &= ~(bitmask);
  // Now see how long the pin to get pulled up
  int cycles = 16000;
  for(int i = 0; i < cycles; i++){
    if (*pin & bitmask){
      cycles = i;
      break;
    }
  }
  // Discharge the pin again by setting it low and output
  //  It's important to leave the pins low if you want to 
  //  be able to touch more than 1 sensor at a time - if
  //  the sensor is left pulled high, when you touch
  //  two sensors, your body will transfer the charge between
  //  sensors.
  *port &= ~(bitmask);
  *ddr  |= bitmask;
  
  return cycles;
}



