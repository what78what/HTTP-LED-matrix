#if defined(ESP8266)
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#else
#include <WiFi.h>
#endif

#include <AsyncUDP.h>


//EEPROM to store queue
#include <EEPROM.h>


//General Arduino Libraries
#include <Arduino.h>

// LED Matrix libraries
#include <PxMatrix.h>


//needed for library
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>         //https://github.com/tzapu/WiFiManager

// **********************************************************//
//  set ESP32 GPIO pins used to control display              //
//  PxMatrix connect GPIO 14 to R0 (R1) on your LED matrix   //
//  G0 (G1), B0 (B1) are apparently not used                 //
//  A,B,C,D,E - rows selection                               //
// ********************************************************* //
#define P_A 19
#define P_B 23
#define P_C 18
#define P_D 5
#define P_E 0
#define P_LAT 22
#define P_OE 21

// Optional - Light sensor
// connect photoresistor in a series with 10kOhm resistor as a voltage divider
// photoresistor connect one side to (+Vcc 5V) and the other side to the GPIO specified below
#define lightSensor 34

// define the number of bytes I want to access in EEPROM
#define EEPROM_SIZE 2

hw_timer_t * timer_1ms = NULL;
hw_timer_t * timer_20ms = NULL;
hw_timer_t * timer_100ms = NULL;
hw_timer_t * timer_1s = NULL;

int interruptCounter1ms = 0;
int interruptCounter20ms = 0;
int interruptCounter100ms = 0;
int interruptCounter1s = 0;

String ip;

portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
  

// Pins for LED MATRIX
PxMATRIX display(64, 32, P_LAT, P_OE, P_A, P_B, P_C, P_D, P_E);


// Some standard colors
uint16_t myRED = display.color565(255, 0, 0);
uint16_t myGREEN = display.color565(0, 255, 0);
uint16_t myBLUE = display.color565(0, 0, 255);
uint16_t myWHITE = display.color565(255, 255, 255);
uint16_t myYELLOW = display.color565(255, 255, 0);
uint16_t myCYAN = display.color565(0, 255, 255);
uint16_t myMAGENTA = display.color565(255, 0, 255);
uint16_t myBLACK = display.color565(0, 0, 0);
uint16_t dspColor = myWHITE;


uint16_t myCOLORS[8] = {myRED, myGREEN, myBLUE, myWHITE, myYELLOW, myCYAN, myMAGENTA, myBLACK};

// declarations for queue
char series[]={'A','B','C','D','E','F','G','H','I','J'};
uint8_t series_n;
uint16_t counter=0;
String showDSP;
boolean callOnce_b;


// declarations for display
String p_name,p_value,text,Error;
uint8_t Xpos,Ypos,Size;
uint16_t Color;
boolean b_text;

// declarations for intervals
unsigned long previousMillis = 0;       // will store last time LED was updated
const long interval = 5000;             // interval at which to send udp boadcast (milliseconds)


#if !( defined(ESP8266) || defined(ESP32) )
  #error This code is intended to run only on the ESP8266 and ESP32 boards ! Please check your Tools->Board setting.
#endif

// text, pos_x, pos_y, size, color
void printText(String text, uint8_t x0, uint8_t y0, uint8_t size, uint16_t color) {
    display.setCursor(x0,y0);
    display.setTextSize(size);
    display.setTextColor(color);
    display.print(text);
}

void display_updater() {
   display.display(70);
 }

void IRAM_ATTR onTimer1ms () {
  portENTER_CRITICAL_ISR(&timerMux);
  interruptCounter1ms++;
  portEXIT_CRITICAL_ISR(&timerMux);
}


void IRAM_ATTR onTimer20ms() {
  portENTER_CRITICAL_ISR(&timerMux);
  interruptCounter20ms++;
  portEXIT_CRITICAL_ISR(&timerMux);
}

void IRAM_ATTR onTimer100ms() {
  portENTER_CRITICAL_ISR(&timerMux);
  interruptCounter100ms++;
  portEXIT_CRITICAL_ISR(&timerMux);
}

void IRAM_ATTR onTimer1s() {
  portENTER_CRITICAL_ISR(&timerMux);
  interruptCounter1s++;
  portEXIT_CRITICAL_ISR(&timerMux);
}


void configModeCallback (AsyncWiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
}



AsyncWebServer server(80);
DNSServer dns;
AsyncUDP udp;


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  AsyncWiFiManager wifiManager(&server,&dns);
  //reset settings - for testing
  //wifiManager.resetSettings();

  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);
  

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if(!wifiManager.autoConnect()) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.restart();
    //ESP.reset();
    delay(1000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...");

    ip = WiFi.localIP().toString();
    Serial.print("IP address: ");
    Serial.println(ip);
    
    IPAddress broadcastIp = WiFi.localIP();
    broadcastIp[3] = 255;
    
    udp.connect(broadcastIp, 19700);



    display.begin(16);
    display.setBrightness(10);
    printText(ip,0,0,1,myWHITE);

      // initialize EEPROM with predefined size
      EEPROM.begin(EEPROM_SIZE);

      // get the last counter number from EEPROM
      counter = EEPROM.read(0);
      series_n = EEPROM.read(1);

      // EEPROM initializes to 255 first time it is used
      if (counter==255) counter=0;
      if (series_n==255) series_n=0;

      server.on("/", HTTP_GET, [&](AsyncWebServerRequest *request){
        request->send(200, "text/html","<html><a href=\"/show.html\">Display Text</a> <br> <a href=\"/queue.html\">Queue management</a>");
        display.clearDisplay();
        printText(ip,0,0,0,myWHITE);
      });


    server.on("/show.html", HTTP_GET, [&](AsyncWebServerRequest *request){
      request->send(200, "text/html","<html><form action=\"display.html\" method=\"get\"> \
    <label for=\"text\">Text:</label> \
                <input type=\"text\" name=\"text\"><br><br> \
                <label for=\"size\">Size:</label> \
  <select name=\"size\"> \
	<option value=\"1\" selected>1</option> \
	<option value=\"2\">2</option> \
	<option value=\"3\">3</option> \
  </select>  \
  <br><br>  \
  \
  <label for=\"x\">Position X:</label> \
  <input type=\"text\" size=2 name=\"x\"><br><br> \
  <label for=\"y\">Position Y:</label> \
  <input type=\"text\" size=2 name=\"y\"><br><br> \
  \
  <label for=\"color\">Color:</label> \
  <select name=\"color\"> \
	<option value=\"WHITE\">White</option> \
	<option value=\"BLUE\">Blue</option> \
	<option value=\"YELLOW\">Yellow</option> \
	<option value=\"GREEN\">Green</option> \
	<option value=\"RED\" selected>Red</option> \
  </select> \
  <br><br> \
  <input type=\"submit\" value=\"Submit\"> \
  </form> \
  <a href=\"/\">GO HOME</a>\
</html>");
    });



server.on("/queue.html", HTTP_GET, [&](AsyncWebServerRequest *request){

       int paramsNr = request->params();
       Serial.print("Number of passed parameters: ");
       Serial.println(paramsNr);

      if (paramsNr == 0) {

      } else {
          for(int i=0;i<paramsNr;i++){
            AsyncWebParameter* p = request->getParam(i);
            if (p->name() == "counter") {
                if (p->value() == "previous") {
                  EEPROM.write(0, --counter);
                } else if (p->value() == "next") {
                  EEPROM.write(0, ++counter);
                }
                EEPROM.commit();
            } else if (p->name() == "set") {
              uint16_t setValue = atoi(p->value().c_str());
              if (setValue>99 || setValue<0) {
                Error = Error + "Counter out of range <0-99>. <br>";
              } else {
                counter = setValue;
                EEPROM.write(0,counter);
                EEPROM.commit();
              } 
            } else if (p->name() == "series") {
              String setValue = p->value();
              boolean valid=false;
              for (int j=0;j<10;j++){
                if (toUpperCase(setValue[0]) == series[j]) {
                  valid=true;
                  series_n = j;
                }
              }
              if (!valid) {
                Error = Error + "invalid series ID <A-J>";
              }
            } else if (p->name() == "call") {
              uint16_t callOnce = atoi(p->value().c_str());
              if (callOnce>99) callOnce = counter;
              showDSP = callOnce;
              callOnce_b = true;
            }

          }
      }
      
      if (counter>99) {
        counter=0;
        if (series_n<9) {
          EEPROM.write(1,series_n++);
          EEPROM.commit();
        } else {
          EEPROM.write(1,series_n=0);
          EEPROM.commit();
        }
      }

      if (!callOnce_b) {
        // format counter to include leading zeroes
        showDSP = counter; 
        if (counter<10) showDSP = "0" + showDSP;
      }
      callOnce_b = false;

      

      String html_main = series[series_n] + showDSP;
      request->send(200, "text/html",html_main);

      display.clearDisplay();
      Serial.print("Counter: ");
      Serial.println(series[series_n] + showDSP);
      display.setCursor(5,5);
      display.setTextColor(myRED);
      display.setTextSize(3);
      display.print(series[series_n] + showDSP);

    }); 



    server.on("/display.html", HTTP_GET, [](AsyncWebServerRequest *request){
       int paramsNr = request->params();
       Serial.print("Number of passed parameters: ");
       Serial.println(paramsNr);

      b_text = false;
      Error="";

      for(int i=0;i<paramsNr;i++){
 
        AsyncWebParameter* p = request->getParam(i);
        Serial.print("Param name: ");
        Serial.println(p->name());
        Serial.print("Param value: ");
        Serial.println(p->value());
        Serial.println("------");

        if (p->name() == "text") {
            b_text = true;
            text = p->value();
        }
        
        
        if (p->name() == "size") {
            Size = atoi(p->value().c_str());
            if (Size>3 || Size<0) {
              Size=0;
              Error = Error + "Size out of bounds <0-3>. <br>";
            }
        }

        if (p->name() == "x") {
            Xpos = atoi(p->value().c_str());
            if (Xpos>63 || Xpos<0) {
              Xpos=0;
              Error = Error + "X position out of bounds <0-63>. <br>";
            }

        }

        if (p->name() == "y") {
            Ypos = atoi(p->value().c_str());
            if (Ypos>31 || Ypos<0) {
              Ypos=0;
              Error = Error + "Y position out of bounds. <0-31> <br>";
            }
        }

        
        if (p->name() == "color") {
                if (p->value() == "RED") {
                  Color = myRED;
                } else if (p->value() == "GREEN") {
                          Color = myGREEN;
                } else if (p->value() == "BLUE") {
                          Color = myBLUE;
                } else if (p->value() == "YELLOW") {
                          Color = myYELLOW;
                } else    Color = myWHITE;
            }

      }
 
        display.clearDisplay();
        
        if (b_text) {
          printText(text,Xpos,Ypos,Size,Color);
        } else {
          printText(ip,0,0,0,myWHITE);
          Error = Error + "No text sent. Displaying IP address of the device. <br>";

        }
        
        if (Error.length()==0) {
          request->send(200, "text/html", "OK <br>\
          <a href=\"/show.html\">GO BACK</a>");
        } else {
          Error = Error + "<br><a href=\"/show.html\">GO BACK</a>";
          request->send(200, "text/html", Error);
        }

    });

    server.begin();


 // timer_1ms (1000Hz)
    timer_1ms = timerBegin(0, 80, true);
    timerAttachInterrupt(timer_1ms, &onTimer1ms, true);
    timerAlarmWrite(timer_1ms, 1000, true);
    timerAlarmEnable(timer_1ms);


   // timer_20ms (50Hz)
    timer_20ms = timerBegin(1, 80, true);
    timerAttachInterrupt(timer_20ms, &onTimer20ms, true);
    timerAlarmWrite(timer_20ms, 20000, true);
    timerAlarmEnable(timer_20ms);


  // timer_100ms (10Hz)
    timer_100ms = timerBegin(2, 80, true);
    timerAttachInterrupt(timer_100ms, &onTimer100ms, true);
    timerAlarmWrite(timer_100ms, 100000, true);
    timerAlarmEnable(timer_100ms);

// timer_1s (1Hz)
    timer_1s = timerBegin(3, 80, true);
    timerAttachInterrupt(timer_1s, &onTimer1s, true);
    timerAlarmWrite(timer_1s, 1000000, true);
    timerAlarmEnable(timer_1s);



}

void loop() {

    if (interruptCounter1ms>0) {
      interruptCounter1ms--;
      display_updater();
    }

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) 
  {
    previousMillis = currentMillis;
    udp.broadcastTo(ip.c_str(), 19700);
  }

}