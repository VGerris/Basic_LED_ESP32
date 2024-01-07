
//  __________________________________________________
// |                                                  |
// |                    Libraries                     |
// |__________________________________________________|
#include <EEPROM.h>
#include <GyverPortal.h>  //need V1.7
#include <ESP8266mDNS.h>             //libraries for OTA
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
//definitions
//#define eepromfix
#define debugmode
#define addressableLEDs
#define prefferssid1
#define calladdress "cegledi_wemos"
#ifdef addressableLEDs
  //settings for addressableLEDs
  #define NUM_LEDS 900
  #define DATA_PIN D1  //only use D1 D2 D5 D6 D7
  #define BRIGHTNESS 30  //value 0-100
#else
  //these pins are used for standard PWM LEDs (connected to driver)
  #define R_pin D7
  #define G_pin D6
  #define B_pin D5
  #define E_pin D8  //enable line controlling the common side of the LEDs
#endif
#ifdef addressableLEDs
  //#define FASTLED_ALLOW_INTERRUPTS 0
  //#define FASTLED_INTERRUPT_RETRY_COUNT 1
  #include <FastLED.h>
  // Define the array of leds
  CRGB leds[NUM_LEDS];
#endif
#define LEDpin D4
struct memory {
  byte bootcode;          //0=normal boot, 1=config portal, 2=OTA after boot, 3=factory reset
  byte AutoRun=0;
  byte Length=6;
  byte FlowDelay=10;
  byte DelayValue=200;
  byte FadeSteps=1;
  byte Red[20];
  byte Green[20];
  byte Blue[20];
  byte colouratstart=0;   //0=none or preset number for startup colour
  char ssid[20];
  char pass[20];
  char ssid2[20];
  char pass2[20];
};
//create a short alias for memory
memory mem;
//  __________________________________________________
// |                                                  |
// |                     Globals                      |
// |__________________________________________________|
byte ColourValue[3];
byte ColourState[3]={1,0,0};       //there must be a small difference in one of the colours at startup otherwise the LEDs stay on whatever colour was set last

unsigned long rowupdatetimestamp=0;   //timestamp of row update
unsigned long LEDupdatetimestamp=0;   //timestamp of led update
//byte immediateupdate=0;
byte presetcounter=0;                 //autorun colour preset (zero indexed, 0=1st preset)
byte updateLEDS=0;                    //status to update LED 0 when colourstate has been set to all LEDs 1 when still rotating and setting colour
//char dropdownoptions[60];             //1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,none + terminate char '\0'
char colouroptions[60];
int currentLED=0;                     //current led ID to update
GPcolor valCol;
byte onoffstate=0;                    //0 means off 1 means at least 1 colour is on
byte AutoRun=0;                       //current setting of AutoRun (not EEPROM)
byte wifistatus=0; //0=boot, 1=connected, 2=lost connection
GyverPortal portal;
char dropdownoptions[500];
byte actionID=0;
byte actionPayload=0;
//     _______________________________ 
//   _|_                             _|_
//  | |_|                           |_| |
//  |                                   |
//  |               Setup               |
//  |  _                             _  |
//  |_|_|                           |_|_|
//    |_______________________________|
void setup() {
  pinMode(D4, OUTPUT);
  digitalWrite(D4, HIGH);
  Serial.begin(9600);
  EEPROM.begin(500);
  Serial.println ();
  //if bootcode is factory reset, then wipe memory
  byte checkbootcode;
  EEPROM.get(1, checkbootcode);
  Serial.print ("checkbootcode: ");
  Serial.println (checkbootcode);
  if (checkbootcode==3){
    EEPROM.put(1, mem);
    EEPROM.commit();
  }
  //----------------------
  #ifndef eepromfix
    EEPROM.get(1, mem);
  #endif
  #ifndef addressableLEDs
    pinMode(R_pin, OUTPUT);
    pinMode(G_pin, OUTPUT);
    pinMode(B_pin, OUTPUT);
    pinMode(E_pin, OUTPUT);
  #else
    FastLED.addLeds<WS2811, DATA_PIN, BRG>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );  // GRB ordering is assumed
    // set master brightness control
    FastLED.setBrightness(BRIGHTNESS);
  #endif
  AutoRun = mem.AutoRun;
  //set colour
  if (mem.colouratstart > 0){
    ColourValue[0]   = mem.Red   [mem.colouratstart-1];
    ColourValue[1] = mem.Green [mem.colouratstart-1];
    ColourValue[2]  = mem.Blue  [mem.colouratstart-1];
  }
  //LEDupdater(1);
  #ifdef debugmode
  memdebug();
  #endif
  if (mem.bootcode == 1){
    loginPortal();
  }
  wificonnect();
  // Print the IP address
  Serial.print("Use this URL : ");
  Serial.print("http://");
  Serial.print(WiFi.localIP());
  Serial.println("/"); 
  //  __________________________________________________
  // |                                                  |
  // |                        OTA                       |
  // |__________________________________________________|
  if (mem.bootcode == 2){
    mem.bootcode=0;
    EEPROM.put(1, mem);
    EEPROM.commit();

    // Port defaults to 8266
    // ArduinoOTA.setPort(8266);

    // Hostname defaults to esp8266-[ChipID]
     ArduinoOTA.setHostname(calladdress);

    // No authentication by default
    // ArduinoOTA.setPassword((const char *)"123");

    ArduinoOTA.onStart([]() {
      Serial.println("Start");
    });
    ArduinoOTA.onEnd([]() {
      Serial.println("\nEnd");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
    ArduinoOTA.begin();
    unsigned long timestamp=millis();
    while (millis()<timestamp+300000){  //stay in OTA mode for 5min
      ArduinoOTA.handle();
      delay(100);
      digitalWrite(D4, LOW);
      delay(100);
      digitalWrite(D4, HIGH);
    }
    ESP.restart();
  }
  //start serving the config page
  portal.attachBuild(buildmainpage);
  portal.start();
}
//     _______________________________ 
//   _|_                             _|_
//  | |_|                           |_| |
//  |                                   |
//  |               Loop                |
//  |  _                             _  |
//  |_|_|                           |_|_|
//    |_______________________________|
void loop(){
  colouradjust();
  autorotate();
  if (WiFi.status() != WL_CONNECTED) {
    wifistatus=2;
    wificonnect();
  }
  configPortal();
}
//     _______________________________ 
//   _|_                             _|_
//  | |_|                           |_| |
//  |                                   |
//  |             Functions             |
//  |  _                             _  |
//  |_|_|                           |_|_|
//    |_______________________________|
void getUIlists(){
  char *c = colouroptions;
  c += sprintf(c, "off");         //print "off" to the memory address of colouroptions and increment the pointer
  for (int i = 1; i <= mem.Length; i++) {
      c += sprintf(c, ",%d", i);
  }
}

#ifdef debugmode
void memdebug() {
  Serial.print (" FlowDelay: ");
  Serial.println (mem.FlowDelay);
  Serial.print (" DelayValue: ");
  Serial.println (mem.DelayValue);
  Serial.print (" Steps: ");
  Serial.println (mem.FadeSteps);
  Serial.print (" R: | ");
  //Serial.println (RedValue);
  for (int i=0; i<19; i++){
    Serial.print (mem.Red[i]);
    if (mem.Red[i]<100){
      Serial.print (" ");
      if (mem.Red[i]<10){
        Serial.print (" ");
      }
    }
    Serial.print (" | ");
  }
  Serial.println ();
  Serial.print (" G: | ");
  //Serial.println (GreenValue);
  for (int i=0; i<19; i++){
    Serial.print (mem.Green[i]);
    if (mem.Green[i]<100){
      Serial.print (" ");
      if (mem.Green[i]<10){
        Serial.print (" ");
      }
    }
    Serial.print (" | ");
  }
  Serial.println ();
  Serial.print (" B: | ");
  //Serial.println (BlueValue);
  for (int i=0; i<19; i++){
    Serial.print (mem.Blue[i]);
    if (mem.Blue[i]<100){
      Serial.print (" ");
      if (mem.Blue[i]<10){
        Serial.print (" ");
      }
    }
    Serial.print (" | ");
  }
  Serial.println ();
  Serial.print (" preset Length: ");
  Serial.println (mem.Length);
  Serial.print (" AutoRun@start: ");
  Serial.println (mem.AutoRun);
  Serial.print (" presetCounter: ");
  Serial.println (presetcounter);
  Serial.print (" bootcode: ");
  Serial.println (mem.bootcode);
}
#endif

void colouradjust(){  //adjust colour state towards target value
  if (updateLEDS==0){
    for (int i=0; i<3; i++){
      if (ColourState[i]!=ColourValue[i]){              // if the colour needs to be adjusted because it is not at the desired levels
        updateLEDS=1;
        int difference=ColourValue[i]-ColourState[i];
        if (abs(difference)<mem.FadeSteps){
          ColourState[i]+=difference;
        }else{
          if (difference>0){
            ColourState[i]+=mem.FadeSteps;
          }else{
            ColourState[i]-=mem.FadeSteps;
          }
        }
      }
    }
    #ifdef debugmode
    if (updateLEDS == 1){
    Serial.println ("Adjust colour ");
    Serial.print (ColourValue[0]);
    Serial.print (" | ");
    Serial.print (ColourValue[1]);
    Serial.print (" | ");
    Serial.println (ColourValue[2]);
    Serial.print (ColourState[0]);
    Serial.print (" | ");
    Serial.print (ColourState[1]);
    Serial.print (" | ");
    Serial.println (ColourState[2]);
    }
    #endif
  }
}

void autorotate(){
  if (updateLEDS==1){
    if (millis()-rowupdatetimestamp>mem.DelayValue){
      LEDupdater(0);
    }
  }else{
    //check if there is something to do for AutoRun to set next colour
    if ((AutoRun==1) && (mem.Length!=0) && (millis()-rowupdatetimestamp>mem.DelayValue)){
      rowupdatetimestamp=millis();
      presetcounter+=1;
      if (presetcounter>mem.Length-1){
        presetcounter=0;
      }
      ColourValue[0] = mem.Red   [presetcounter];
      ColourValue[1] = mem.Green [presetcounter];
      ColourValue[2] = mem.Blue  [presetcounter];
      #ifdef debugmode
      Serial.println ("AutoSet next colour");
      Serial.print (" R: ");
      Serial.print (ColourValue[0]);
      Serial.print (" G: ");
      Serial.print (ColourValue[1]);
      Serial.print (" B: ");
      Serial.println (ColourValue[2]);
      #endif
    }
  }
}

void LEDupdater(byte delaytrue){
  #ifdef addressableLEDs
    if (delaytrue==1){
      for (int i=0; i<NUM_LEDS; i++){
        // set the current colour state for each LED
        leds[i] = CRGB(ColourState[0],ColourState[1],ColourState[2]);
        FastLED.show();
        delay(mem.FlowDelay);
      }
    }else{
      if (millis()-LEDupdatetimestamp>mem.FlowDelay){
        //#ifdef debugmode
        //Serial.println("LEDupdate");
        //#endif
        LEDupdatetimestamp=millis();
        for (int i=0; i<NUM_LEDS/200+1; i++){
          leds[currentLED] = CRGB(ColourState[0],ColourState[1],ColourState[2]);
          if (currentLED<NUM_LEDS){
            currentLED++;
          }else{
            currentLED=0;
            rowupdatetimestamp=millis();
            #ifdef debugmode
            Serial.println("row complete");
            Serial.print (ColourState[0]);
            Serial.print (" | ");
            Serial.print (ColourState[1]);
            Serial.print (" | ");
            Serial.println (ColourState[2]);
            #endif
            updateLEDS=0;
            break;
          }
        }
        FastLED.show();
      }
    }
  #else
    LEDupdatetimestamp=millis();
    byte adjustedRed=ColourState[0]*BRIGHTNESS/100;
    byte adjustedGreen=ColourState[1]*BRIGHTNESS/100;
    byte adjustedBlue=ColourState[2]*BRIGHTNESS/100;
    analogWrite(R_pin, 255-adjustedRed);
    analogWrite(G_pin, 255-adjustedGreen);
    analogWrite(B_pin, 255-adjustedBlue);
    rowupdatetimestamp=millis();
    updateLEDS=0;
    digitalWrite(E_pin,HIGH);
  #endif
}
void wificonnect() {
  //  __________________________________________________
  // |                                                  |
  // |                   WIFI SETUP                     |
  // |__________________________________________________|
  WiFi.mode(WIFI_STA);
  WiFi.hostname(calladdress);
  int n = WiFi.scanNetworks();
  byte connecting=0;
  for (int i = 0; i < n; ++i) {
    if (WiFi.SSID(i)== mem.ssid ) {
      Serial.print("Connect to: ");
      Serial.println(mem.ssid);
      WiFi.begin(mem.ssid,mem.pass);
      connecting=1;
      break;
    }
    #ifndef prefferssid1
    if (WiFi.SSID(i)== mem.ssid2) {
      Serial.print("Connect to: ");
      Serial.println(mem.ssid2);
      WiFi.begin(mem.ssid2,mem.pass2);
      break;
    }
    #endif
  }
  #ifdef prefferssid1
  if (connecting==0){
    for (int i = 0; i < n; ++i) {
      if (WiFi.SSID(i)== mem.ssid2) {
        Serial.print("Connect to: ");
        Serial.println(mem.ssid2);
        WiFi.begin(mem.ssid2,mem.pass2);
        break;
      }
    }
  }
  #endif
  byte notfoundcounter = 0;
  unsigned long timestamp=millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    Serial.print(WiFi.status());
    if ((WiFi.status() == 1) || (millis()-timestamp > 15000)){ //network not found
      notfoundcounter += 1;
      if (notfoundcounter == 3){
        Serial.println();
        if (wifistatus==0) {
          loginPortal();      //if unable to connect open the config portal
        }
        break;
        //timestamp=millis(); //reset network search timestamp for new connection attempt
        //notfoundcounter = 0;
        //Serial.print("Connect to: ");
        //Serial.println(mem.ssid);
        //WiFi.mode(WIFI_STA);
        //WiFi.begin(mem.ssid, mem.pass);
      }
    }
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected!");
    wifistatus=1;
  }
}
//  __________________________________________________
// |                                                  |
// |                      LOGIN                       |
// |__________________________________________________|
void loginPortal() {
  Serial.println("Portal start");

  WiFi.mode(WIFI_AP);
  WiFi.softAP("WiFi Config");

  int n = WiFi.scanNetworks();
  char *p = dropdownoptions;  // current write position
  p += sprintf(p, "%s", "SSID");
  for (int i = 0; i < n; ++i) {
    p += sprintf(p, ",%s", WiFi.SSID(i).c_str());
    //Serial.println(WiFi.SSID(i));
  }
  Serial.println(dropdownoptions);
  GyverPortal portal;
  portal.attachBuild(build);
  portal.start(WIFI_AP);

  while (portal.tick()) {
    if (portal.form("/login")) {
      byte selssid1 = portal.getSelected("selectssid1", dropdownoptions);
      byte selssid2 = portal.getSelected("selectssid2", dropdownoptions);
      if (selssid1==0){
        //portal.copyStr("lgin", mem.ssid);
      }else{
        char *pointer=mem.ssid;
        sprintf(pointer, "%s", WiFi.SSID(selssid1-1));
      }
      portal.copyStr("pss", mem.pass);
      if (selssid2==0){
        //portal.copyStr("lgin2", mem.ssid2);
      }else{
        char *pointer=mem.ssid2;
        sprintf(pointer, "%s", WiFi.SSID(selssid2-1));
      }
      portal.copyStr("pss2", mem.pass2);
      mem.bootcode=0;
      EEPROM.put(1, mem);
      EEPROM.commit();
      WiFi.softAPdisconnect();
      ESP.restart();
      break;
    }
  }
}

void build() {
  String s;
  BUILD_BEGIN(s);
  add.THEME(GP_DARK);

  add.FORM_BEGIN("/login");
  add.TITLE("WiFi settings");
  add.SELECT("selectssid1",dropdownoptions,0);
  add.BREAK();
  //add.LABEL("Other SSID: ");
  //add.TEXT("lgin", "SSID", mem.ssid);
  //add.BREAK();
  add.TEXT("pss", "Password", "");
  add.BREAK();
  add.SELECT("selectssid2",dropdownoptions,0);
  add.BREAK();
  //add.LABEL("Other SSID: ");
  //add.TEXT("lgin2", "SSID2", mem.ssid2);
  //add.BREAK();
  add.TEXT("pss2", "Password2", "");
  add.BREAK();
  add.SUBMIT("Submit");
  add.FORM_END();

  BUILD_END();
}

//  __________________________________________________
// |                                                  |
// |                 CONFIG PORTAL                    |
// |__________________________________________________|


void configPortal() {
  portal.tick();
  //handling the button clicks
  if (portal.click()){
    for (int i=0; i<mem.Length; i++){
      char colournamebuffer[50];
      sprintf(colournamebuffer, "%s%d", "cl", i);
      if (portal.click(colournamebuffer)){
        valCol = portal.getColor(colournamebuffer);
        actionID=7;
        actionPayload=i;
      }
    }
    if (portal.click("sldfade")){
      actionID=4;
      actionPayload = portal.getInt("sldfade");
      actionPayload = actionPayload;
    }
    if (portal.click("autorun")){
      actionID=8;
      actionPayload=0;
    }
    if (portal.click("autorunatstart")){
      actionID=8;
      actionPayload=1;
    }
    if (portal.click("colouratstart")){
      actionID=13;
      actionPayload = portal.getSelected("colouratstart", colouroptions);
    }
    actions();
  }
  //updating the page content in real time
  if (portal.update()) {
    /*for (int i=0; i<numOfPins; i++){
      char swbuffer[50];
      sprintf(swbuffer, "%s%s", "sw", availablePinNames[i]);
      if (portal.update(swbuffer)){
        portal.answer(pinstate[i]);
      }
    }*/
  }
  if (portal.form()){
    if (portal.form("/remove")) {
      actionID=9;
    }
    if (portal.form("/add")) {
      actionID=10;
    }
    if (portal.form("/save")) {
      actionID=14;
    }
    if (portal.form("/reboot")) {
      actionID=255;
    }
    if (portal.form("/reboot_to_config")) {
      actionID=254;
    }
    if (portal.form("/reboot_to_OTA")) {
      actionID=253;
    }
    if (portal.form("/reset")) {
      actionID=252;
    }
    actions();
  }
}

void buildmainpage() {
  String s;
  BUILD_BEGIN(s);
  add.THEME(GP_DARK);
  //this line is important for real time update of content
  /*char updatebuffer[100];
  char *p = updatebuffer;  // current write position
  p += sprintf(p, "sw");
  p += sprintf(p, "%s", availablePinNames[0]);
  for (int i=1; i<numOfPins; i++){
    p += sprintf(p, ",sw");
    p += sprintf(p, "%s", availablePinNames[i]);
  }
  add.UPDATE(updatebuffer,1000);*/
  add.TITLE("ESP LED Controller");
  add.HR();
  for (int i = 0; i < mem.Length; i++) {
    //add.LABEL(String(i+1));
    char colournamebuffer[50];
    sprintf(colournamebuffer, "%s%d", "cl", i);
    GPcolor colour1(mem.Red[i], mem.Green[i], mem.Blue[i]);
    add.COLOR(colournamebuffer, colour1);
    add.BREAK();
  }
  if (mem.Length>1){
    add.FORM_BEGIN("/remove");
    add.SUBMIT("   -   ");
    add.FORM_END();
  }else{
    add.LABEL("   ");
  }
  if (mem.Length<20){
    add.FORM_BEGIN("/add");
    add.SUBMIT("   +   ");
    add.FORM_END();
  }else{
    add.LABEL("   ");
  }
  add.FORM_BEGIN("/save");
  add.TITLE("Transition Speed");
  add.SLIDER("sldfade", mem.FadeSteps, 1, 255);
  add.BREAK();
  add.SUBMIT("Save Colours");
  add.FORM_END();
  add.BREAK();
  add.LABEL("AutoRun: ");
  add.SWITCH("autorun", AutoRun);
  add.BREAK();
  add.LABEL("AutoRun@start: ");
  add.SWITCH("autorunatstart", mem.AutoRun);
  add.BREAK();
  getUIlists();
  add.LABEL("Colour@start: ");
  add.SELECT("colouratstart",colouroptions,mem.colouratstart);
  BUILD_END();
}

void actions(){
  //  __________________________________________________
  // |                                                  |
  // |                     ACTIONS                      |
  // |__________________________________________________|
  //#ifdef debugmode
  Serial.print(" actionID: ");
  Serial.print(actionID);
  Serial.print(" actionPayload: ");
  Serial.println(actionPayload);
  //#endif
  switch (actionID) {
    case 0:                     //no action
      break;
    case 4:                     //fadesteps
      mem.FadeSteps = actionPayload;
      #ifdef debugmode
      Serial.print("Fade Slider ");
      Serial.println(mem.FadeSteps);
      #endif
      break;
    /*case 5:                     //flowdelay
      mem.FlowDelay=actionPayload;
      #ifdef debugmode
      Serial.print ("setting FlowDelay to ");
      Serial.println (mem.FlowDelay);
      #endif
      break;
    case 6:                     //delay
      mem.DelayValue=actionPayload;
      #ifdef debugmode
      Serial.print ("setting Delay to ");
      Serial.println (mem.DelayValue);
      #endif
      break;*/
    case 7:                     //colourdisplay
      ColourValue[0] = valCol.r;
      ColourValue[1] = valCol.g;
      ColourValue[2] = valCol.b;
      mem.Red[actionPayload]=ColourValue[0];
      mem.Green[actionPayload]=ColourValue[1];
      mem.Blue[actionPayload]=ColourValue[2];
      #ifdef debugmode
      Serial.print ("Colour selector: ");
      Serial.print (actionPayload);
      Serial.print (" R: ");
      Serial.print (ColourValue[0]);
      Serial.print (" G: ");
      Serial.print (ColourValue[1]);
      Serial.print (" B: ");
      Serial.println (ColourValue[2]);
      #endif
      break;
    case 8:                     //autorun
      if (actionPayload == 1){
        mem.AutoRun+=1-mem.AutoRun*2;
        EEPROM.put(1, mem);
        EEPROM.commit();
      }else{
        AutoRun+=1-AutoRun*2;
      }
      #ifdef debugmode
      Serial.print ("setting AutoRun to ");
      Serial.println (AutoRun);
      Serial.print ("setting AutoRun@start to ");
      Serial.println (mem.AutoRun);
      #endif
      break;
    case 9:                     //remove preset
      if (mem.Length>1){
        mem.Length-=1;
        if (mem.colouratstart>mem.Length){
          mem.colouratstart=1;
          EEPROM.put(1, mem);
          EEPROM.commit();
        }
      }
      break;
    case 10:                     //add preset
      if (mem.Length<20){
        mem.Length+=1;
      }
      break;
    case 11:                     //off
      ColourValue[0] = 0;
      ColourValue[1] = 0;
      ColourValue[2] = 0;
      AutoRun = 0;
      onoffstate=0;
      break;
    case 12:                     //on
      AutoRun = mem.AutoRun;
      //set colour
      if (mem.colouratstart > 0){
        ColourValue[0] = mem.Red   [mem.colouratstart-1];
        ColourValue[1] = mem.Green [mem.colouratstart-1];
        ColourValue[2] = mem.Blue  [mem.colouratstart-1];
      }
      onoffstate=1;
      break;
    case 13:                     //set static colour
      Serial.print ("saving colour@start ");
      Serial.println (actionPayload);
      mem.colouratstart=actionPayload;
      EEPROM.put(1, mem);
      EEPROM.commit();
      break;
    case 14:                     //set static colour
      Serial.println ("saving");
      #ifdef debugmode
      memdebug();
      #endif
      EEPROM.put(1, mem);
      EEPROM.commit();
      break;
    case 252:                   //factory reset
      if (millis()>10000){      //do not allow reboot in first 10s
        mem.bootcode=3;
        EEPROM.put(1, mem);
        EEPROM.commit();
        ESP.restart();
      }
      break;
    case 253:                   //reboot to OTA
      if (millis()>10000){      //do not allow reboot in first 10s
        mem.bootcode=2;
        EEPROM.put(1, mem);
        EEPROM.commit();
        ESP.restart();
      }
      break;
    case 254:                   //reboot to config
      if (millis()>10000){      //do not allow reboot in first 10s
        mem.bootcode=1;
        EEPROM.put(1, mem);
        EEPROM.commit();
        ESP.restart();
      }
      break;
    case 255:                   //reboot
      if (millis()>10000){      //do not allow reboot in first 10s
        ESP.restart();
      }
      break;
    default:
      Serial.println("actionID ERROR");
      break;
  }
  actionID=0;
  actionPayload=0;
}
//END 
