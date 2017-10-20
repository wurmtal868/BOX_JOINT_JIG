#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>
#include <Hash.h>

// Includes für display

//#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>

#include <Adafruit_SSD1306.h>

#define OLED_RESET 13 // Define dummy reset for the OLED display, use JigEnable because it will only disable the motor
Adafruit_SSD1306 display(OLED_RESET); // Create an instance of the OLED display

// #define DEBUG


WebSocketsServer webSocket = WebSocketsServer(81);

#include "credentials.h"

// Add lines below into "credentials.h" with your WiFi credentials
/*
const char* ssid1     = "ssid1";
const char* password1 = "*****";
const char* ssid     = "ssid";
const char* password = "*****";
*/

// Jig GPIOs on ESP8266

unsigned char OLED_SDA = 14;
unsigned char OLED_SCL = 12;
unsigned char BACK_PIN = 0;     // Backward bridge of L298 and END positions switch
unsigned char FWD_PIN = 2;      // Foward bridge of L298 and HOME positions switch
unsigned char sensor_clk = 4;   // Feedback sensor_clk
unsigned char sensor_dir = 5;   // Feedback sensor_dir
unsigned char blueKey = 16;     // Blue PushButton(active LOW)
unsigned char redKey = 15;      // Red PushButton (active HIGH)
unsigned char JigEnable = 13;   // Enable of L298


String last_counter="";
String lastStatus="";
String lastLine1="";
String lastLine2="";
volatile bool HOME = false ;
volatile int warte=0;
volatile int counter = 0;

/*
Show a line of text (t_new) on postion t_x,t_y with textsize t_size on the OLED display
*/

void display_line(uint8_t t_size,uint8_t t_x, uint8_t t_y, String t_old,String t_new){
  display.setTextSize(t_size);
  display.setTextColor(BLACK);
  display.setCursor(t_x,t_y);
  display.println(t_old);
  display.setTextSize(t_size);
  display.setTextColor(WHITE);
  display.setCursor(t_x,t_y);
  display.println(t_new);
  display.display();

}

// Display a Message on position 0,56 with 8pt

void display_status(String statusMSG){
  display_line(1,0,56,lastStatus,statusMSG);
  lastStatus=statusMSG;
}
// Display a line of text on position 0,0 with 16 pt in YelloW part,
// and on position 0,16 with 16pt start of Blue part of the OLED display.

void display_text(String line1,String line2){
  display_line(2,0,0,lastLine1,line1);
  display_line(2,0,16,lastLine2,line2);
  lastLine1=line1;
  lastLine2=line2;
}


void counter_reset()
{
  counter=0;
  attachInterrupt(sensor_clk, falling_edge, FALLING);
}


void display_counter(String statusMSG){
  display_line(1,0,48,last_counter,statusMSG);
  last_counter=statusMSG;
  display.display();

}


// Conuter Interupt Routines
// There are routines for falling and rising edge to avoid any error if clock sensor is bouncing while the dirction sensor is stable

void falling_edge()
{
  int other = digitalRead(sensor_dir);
  if (other == LOW) {
    counter --;
  }
  else {
    counter ++;
  }
  #ifdef DEBUG
    Serial.print("c:");
    Serial.print(counter);
    Serial.print("\r\n");

  #endif

  attachInterrupt(sensor_clk, rising_edge, RISING ) ; // Change Interupt routine to rising edge to avoid false counting

}

void rising_edge()
{
  int other = digitalRead(sensor_dir);
  if (other == HIGH ) {
    counter --;
  }
  else {
    counter ++;
  }
  #ifdef DEBUG
    Serial.print("c:");
    Serial.print(counter);
    Serial.print("\r\n");

  #endif
  attachInterrupt(sensor_clk, falling_edge, FALLING ) ; // Change Interupt routine to falling edge to avoid false counting
}

// Websocket Event Handler
//

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {

    switch(type) {
        case WStype_DISCONNECTED:

            break;
        case WStype_CONNECTED:
         {
                IPAddress ip = webSocket.remoteIP(num);
                Serial.print("Connection:");
                Serial.println(num);
                Serial.print("Remote IP:");
                Serial.println(ip);

            }
            break;
        case WStype_TEXT:
        {

            String text = String((char *) &payload[0]);
            display_status(text);
          if(text=="Home"){
            goHome();
            String actual = "Position:" + String(counter);
            char C_actual[16];
            actual.toCharArray(C_actual,16);
            webSocket.sendTXT(num, C_actual);
            }
          if(text.startsWith("go")){

            String goVal=(text.substring(text.indexOf("g")+2,text.length()));
            int pos = goVal.toInt();

            Serial.print("Go");
            Serial.println(goVal);
            // if ( abs(pos-counter)<=10 and !HOME){
            //   gopos(pos-25);
            // }
            gopos(pos);
            //delay(200);
            String actual = "Position:" + String(counter);
            char C_actual[16];
            actual.toCharArray(C_actual,16);
            webSocket.sendTXT(num, C_actual);
            display_text("","Cut!");
           }

           if(text=="RESET"){
            gopos(-100);
            gopos(0);
            }

           if(text=="DONE"){
            display_text("Done","Cutting");
            }



                   }


           webSocket.sendTXT(num, payload, lenght);
          //  webSocket.broadcastTXT(payload, lenght);
            break;

        case WStype_BIN:

            hexdump(payload, lenght);

            // echo data back to browser
            webSocket.sendBIN(num, payload, lenght);
            break;
    }

}


void gopos(int pos)
{
  display_text("WAIT!","");
  if (pos==-200){
    if (!HOME and (counter!=0)) {
      goHome();
    }
  }
  else {
 //   HOME=false;
    while (abs(pos-counter)>10){
      while (pos < (counter-150)){
      pinMode(BACK_PIN, OUTPUT);
      digitalWrite(BACK_PIN,LOW);
      analogWrite(JigEnable,1023);
      delay (100);
      }
      /*
      while (pos < (counter-30)){
      pinMode(BACK_PIN, OUTPUT);
      digitalWrite(BACK_PIN,LOW);
      analogWrite(JigEnable,750);
      delay (100);
      } */
      while (pos < (counter-3)){
      pinMode(BACK_PIN, OUTPUT);
      digitalWrite(BACK_PIN,LOW);
      analogWrite(JigEnable,(420+(counter-pos)*4));
      //delay (50);
      }
      digitalWrite(BACK_PIN,HIGH);
      pinMode(BACK_PIN, INPUT);

      while (pos > (counter+150)){
      pinMode(FWD_PIN, OUTPUT);
      digitalWrite(FWD_PIN,LOW);
      analogWrite(JigEnable,1023);
      delay (100);
      }  /*
      while (pos > (counter+30)){
      pinMode(FWD_PIN, OUTPUT);
      digitalWrite(FWD_PIN,LOW);
      analogWrite(JigEnable,750);
      delay (100);
      }  */
      while (pos > (counter+3)){
      pinMode(FWD_PIN, OUTPUT);
      digitalWrite(FWD_PIN,LOW);
      analogWrite(JigEnable,(420+(pos-counter)*4));
      //delay (50);

      }
      digitalWrite(FWD_PIN,HIGH);
      pinMode(FWD_PIN, INPUT);
    }
  }
  analogWrite(JigEnable,1023);
  delay (200);
  display_counter(String(counter));

}   // end gopos()

void goHome()
{
  HOME=false;
  display_text("WAIT!","");
  attachInterrupt(FWD_PIN,counter_reset, FALLING);
  pinMode(BACK_PIN, OUTPUT);
  digitalWrite(BACK_PIN,LOW);
  while (digitalRead(FWD_PIN) == HIGH) {
  delay(100);
  }
  detachInterrupt(FWD_PIN);
  digitalWrite(BACK_PIN,HIGH);
  pinMode(BACK_PIN, INPUT);

  while (digitalRead(FWD_PIN) == LOW) {
  delay (200);  //warte

  }

  //counter=0;
  //attachInterrupt(sensor_clk, falling_edge, FALLING);
  /*gopos(100);
  gopos(0);*/
  gopos(150);
  delay(200);
  counter=0;
  display_counter(String(counter));
  HOME=true;
  display_text("Waiting","for Setup!");
}

//main loop



void setup()
{
    Serial.begin(115200);
    pinMode(blueKey,INPUT);
    pinMode(redKey,INPUT);
    pinMode(JigEnable,OUTPUT);
// Setup display

  Wire.begin(OLED_SDA,OLED_SCL); // Set I2C pins for OLED display

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3D (for the 128x64)

  Serial.println("Logo shown");
  display.clearDisplay();
  display.display();

  display_text("Box-Joints","v 0.1");
//Setup PWM

    analogWrite(JigEnable,1023);
    counter=0;
  Serial.println("\nSW ver:\njig_with_socket_0.1");
  Serial.println("Neustart");
    WiFi.begin(ssid1, password1);

    while(WiFi.status() != WL_CONNECTED) {
        delay(100);
        if(counter==200) {break;} ;
        counter +=1;
    }
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Versuche alternatives Netzwerk");

      WiFi.begin(ssid, password);

      while(WiFi.status() != WL_CONNECTED) {
          delay(100);
          if(counter==1000) {break;} ;
          counter +=1;
    }}
    Serial.println("");
    Serial.print("My IP address:");
    Serial.println(WiFi.localIP());
    String Message= "IP: " + String (WiFi.localIP()[0]) + "." + String (WiFi.localIP()[1])+ "." + String (WiFi.localIP()[2])+ "." + String( WiFi.localIP()[3]);
    display_status(Message);
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);

// Jjig setup
  pinMode(sensor_dir, INPUT);
  pinMode(BACK_PIN, INPUT);
  pinMode(FWD_PIN, INPUT);
  Serial.println(" ");
  Serial.println("Fahre Schlitten zurück. Bitte Warten!");
  //counter=0;
  goHome();
  //gopos(50);
  Serial.print("c:");
  Serial.println(counter);
  display_counter(String(counter));

}

//

void loop() {
    webSocket.loop();
    if (warte!=1000){
      warte ++;
    }
    else{
    display_counter(String (counter));
     warte=0;
    }
    if (digitalRead(redKey)==HIGH) display_counter("Red Button pushed");
    if (digitalRead(blueKey)==LOW) display_counter("Blue Button pushed");
}
