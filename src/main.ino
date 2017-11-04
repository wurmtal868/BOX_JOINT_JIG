#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>
#include <Hash.h>
#include "helper.h"

// Includes für display

//#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>

#include <Adafruit_SSD1306.h>

#define OLED_RESET 2 // Define dummy reset for the OLED display, use JigEnable because it will only disable the motor
Adafruit_SSD1306 display(OLED_RESET); // Create an instance of the OLED display

// #define DEBUG


WebSocketsServer webSocket = WebSocketsServer(81);

// #include "credentials.h"

// Add lines below into "credentials.h" with your WiFi credentials
/*
const char* ssid1     = "ssid1";
const char* password1 = "*****";
const char* ssid     = "ssid";
const char* password = "*****";
*/

// Jig GPIOs on ESP8266

#define OLED_SDA 12
#define OLED_SCL 13
#define BACK_PIN 2     // Backward bridge of L298 and END positions switch
#define FWD_PIN 4      // Foward bridge of L298 and HOME positions switch
#define sensor_clk 14   // Feedback sensor_clk
#define sensor_dir 16   // Feedback sensor_dir
#define blueKey 0     // Blue PushButton(active LOW)
#define redKey 15      // Red PushButton (active HIGH)
#define JigEnable 5   // Enable of L298


String last_counter="";
String lastStatus="";
String lastLine1="";
String lastLine2="";
volatile bool HOME = false ;
volatile int warte=0;
volatile int counter = 0;
String task="";


FIFO taskList;

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
  // pinMode(FWD_PIN, OUTPUT);
  // digitalWrite(FWD_PIN,LOW);      // set FWD_PIN LOW to debounce the "HOME" microswtich
  detachInterrupt(FWD_PIN);
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
  if (digitalRead(sensor_dir) == LOW) {
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
  if (digitalRead(sensor_dir) == HIGH ) {
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

/*
Reset Cut list when Red Button is pushed for longer then 1 second
*/

void resetCutList()
{
    while (taskList.isNotEmpty())
    {
      taskList.getText();
    }
    display_text("Reset", "Cut List") ;
}



// Websocket Event Handler
//

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {

    switch(type)
    {
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


            task = String((char *) &payload[0]);
            // taskList.addText(task);
            // display_status(task);
            webSocket.sendTXT(num, payload, lenght);
            break;

        case WStype_BIN:

            hexdump(payload, lenght);

            // echo data back to browser
            webSocket.sendBIN(num, payload, lenght);
            break;
    }

}


void actOnTask(String text)
{
  display_status(text);
  if(text=="RESET")
  {
    resetJig();
    String actual = "Position:" + String(counter);
    }

  if(text.startsWith("go"))
  {
    taskList.addText(text);
  }

  if(text=="Home")
  {
    gopos(-100);
    delay(100);
    gopos(0);
    display_status("HOME done!");
    // display_text("Waiting","for Setup!");
  }

  if(text=="DONE")
  {
    display_status("CutList complete");
    // attachInterrupt(blueKey,resetCutList,RISING);

    display_text("Push Blue", "To Start!");
    waitForBlueButton();
    if (counter !=0)
      gopos(-100);
      gopos(0);
    while (taskList.isNotEmpty())
      {
        text = taskList.getText();
        display_status(text);
        String goVal=(text.substring(text.indexOf("g")+2,text.length()));
        int pos = goVal.toInt();

        Serial.print("Go");
        Serial.println(goVal);
        gopos(pos);
        display_text("","Cut!");
        Serial.println("Cut now!");
        waitForJigEnable();
       /* code */
      }
    // detachInterrupt(blueKey);
    display_text("Done","Cutting");
    display_status("");
    delay(1000);
  }
}

void gopos(int pos)
{
  pinMode(JigEnable,INPUT);
  while (digitalRead(JigEnable)==LOW)
  {
    display_text("Move Jig","Back!");
    delay(100);
  }
  pinMode(JigEnable,OUTPUT);
  analogWrite(JigEnable,1023);
  display_text("WAIT!","");
  yield();
  if ((pos-counter)<-10)
  {
    pinMode(BACK_PIN, OUTPUT);
    digitalWrite(BACK_PIN,LOW);
    // analogWrite(JigEnable,1023);

    while (pos < (counter-300))
    {
    // delay (1000);
    yield();
    }

    while (pos < (counter-150)){
    delay (500);
    // yield();
    }
    yield();
    while (pos < (counter-3))
    {
    analogWrite(JigEnable,(420+(counter-pos)*4));
    // delay (10);
    // yield();
    }

    digitalWrite(BACK_PIN,HIGH);
    pinMode(BACK_PIN, INPUT);
  }
  if ((pos-counter)>10)
  {
    pinMode(FWD_PIN, OUTPUT);
    digitalWrite(FWD_PIN,LOW);
    // analogWrite(JigEnable,1023);

    while (pos > (counter+300))
    {
      // delay (1000);
      yield();
    }

    while (pos > (counter+150))
    {
      delay (500);
      // yield();
    }
    yield();
    while (pos > (counter+3))
    {
    analogWrite(JigEnable,(420+(pos-counter)*4));
    // delay (10);
    // yield();
    }

    digitalWrite(FWD_PIN,HIGH);
    pinMode(FWD_PIN, INPUT);
  }
  analogWrite(JigEnable,1023);
  // pinMode(JigEnable,INPUT);
  delay (300);
  display_counter(String(counter));

}   // end gopos()

void resetJig()
{
  HOME=false;
  display_text("WAIT!","");
  // attachInterrupt(sensor_clk, falling_edge, FALLING);
  // gopos(500);
  pinMode(FWD_PIN, INPUT);
  attachInterrupt(FWD_PIN,counter_reset, FALLING);
  pinMode(BACK_PIN, OUTPUT);
  digitalWrite(BACK_PIN,LOW);
  while (digitalRead(FWD_PIN) == HIGH) {
  delay(300);
  // yield();
  }
  // pinMode(FWD_PIN, INPUT);
  // detachInterrupt(FWD_PIN);
  digitalWrite(BACK_PIN,HIGH);
  pinMode(BACK_PIN, INPUT);

  while (digitalRead(FWD_PIN) == LOW)
  {
    delay (100);  //warte
  }

  //counter=0;
  attachInterrupt(sensor_clk, falling_edge, FALLING);
  gopos(150);
  delay(100);
  counter=0;
  // counter_reset(); // Test
  display_counter(String(counter));
  HOME=true;
  display_text("Waiting","for Setup!");
}

//main loop

// void setupWifi()
// {
//   WiFi.mode(WIFI_STA);
//   WiFi.begin(ssid1, password1);
//
//   while(WiFi.status() != WL_CONNECTED)
//   {
//       delay(100);
//       if(counter==80) {break;} ;
//       counter +=1;
//   }
//   if (WiFi.status() != WL_CONNECTED)
//   {
//     Serial.println("Versuche alternatives Netzwerk");
//
//     WiFi.begin(ssid, password);
//
//     while(WiFi.status() != WL_CONNECTED)
//       {
//         delay(100);
//         if(counter==1000) {break;} ;
//         counter +=1;
//       }
//   }
//   counter = 0;
//   Serial.println("");
//   Serial.print("My IP address:");
//   Serial.println(WiFi.localIP());
//   String Message= "IP: " + String (WiFi.localIP()[0]) + "." + String (WiFi.localIP()[1])+ "." + String (WiFi.localIP()[2])+ "." + String( WiFi.localIP()[3]);
//   display_status(Message);
// }
//
void setupAP()
{
  WiFi.mode(WIFI_AP);
  WiFi.softAP("Box-Joints","");
  Serial.println("");
  Serial.print("My IP address:");
  Serial.println(WiFi.softAPIP());
  String Message= "IP: " + String (WiFi.softAPIP()[0]) + "." + String (WiFi.softAPIP()[1])+ "." + String (WiFi.softAPIP()[2])+ "." + String( WiFi.softAPIP()[3]);
  display_status(Message);
}

void waitForJigEnable()
{
  if (digitalRead(redKey)==HIGH)
  {
    resetCutList();
    return;
  }
  pinMode(JigEnable,INPUT);
  while (digitalRead(JigEnable)==HIGH)
  {
    delay(100);
  }
  display_status("Move Jig Back!");
  while (digitalRead(JigEnable)==LOW)
  {
    delay(100);
  }


}

void waitForBlueButton()
{
  display_status("Push Blue when done!");
  while(digitalRead(blueKey)==HIGH)
  // while(digitalRead(JigEnable)==HIGH)
  {
    delay(100);
    // yield();
  }
  display_status("Release!");
  while(digitalRead(blueKey)==LOW)
  // while(digitalRead(JigEnable)==LOW)
  {
    delay(300);
  }
  display_status("");
}

void setup()
{
  WiFi.mode(WIFI_OFF);
    Serial.begin(115200);
    pinMode(blueKey,INPUT_PULLUP);
    pinMode(redKey,INPUT);
    // pinMode(JigEnable,INPUT);
    pinMode(JigEnable,OUTPUT);
// Setup display

  Wire.begin(OLED_SDA,OLED_SCL); // Set I2C pins for OLED display
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3D (for the 128x64)
  // display.display();
  //
  // init done

  Serial.println("Logo shown");
  display.clearDisplay();
  display.display();

  display_text("Box-Joints","v 0.1");
//Setup PWM

    analogWrite(JigEnable,1023);
    counter=0;
  Serial.println("\nSW ver:\njig_with_socket_0.1");
  Serial.println("Neustart");

// Jjig setup
pinMode(sensor_dir, INPUT);
pinMode(sensor_clk, INPUT);
  pinMode(BACK_PIN, INPUT);
  pinMode(FWD_PIN, INPUT);
  Serial.println(" ");
  Serial.println("Fahre Schlitten zurück. Bitte Warten!");
  counter=0;
  attachInterrupt(sensor_clk, falling_edge, FALLING);
  resetJig();
  //gopos(50);
  Serial.print("c:");
  Serial.println(counter);
  // display_counter(String(counter));
  display_text("Waiting","for Network!");

  setupAP();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  display_text("Waiting","for Setup!");


}

//

void loop()
{
    webSocket.loop();
    if (warte!=5000){
      warte ++;
    }
    else{
      display_counter(String (counter));
      display_text("Waiting","for Setup!");
      // display_counter(String (counter)+" = "+String(counter/4)+"mm/10");
      // display_counter(String(counter/4)+"mm/10");
     warte=0;
    }
    // if (task!="")
    // {
    //   actOnTask(task);
    //   task="";
    //   // delay(500);
    //   // display_status(task);
    // }
    if (task!="")
    {
      actOnTask(task);
      task="";
    }

    yield();
    if (digitalRead(redKey)==HIGH)
    {
      display_counter("Red Button pushed");
    }
    yield();
    pinMode(JigEnable,INPUT);
    if (digitalRead(blueKey)==LOW) display_counter("Blue Button pushed");
    if (digitalRead(FWD_PIN)==LOW) display_counter("HOME Button pushed");
    if (digitalRead(BACK_PIN)==LOW) display_counter("END Button pushed");
    if (digitalRead(JigEnable)==LOW) display_text("Move Jig","Back!");
  }
