#define INVERT_DISPLAY false 
#define DIGITS_DISPLAY 6 

//#include <esp_now.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPUI.h>
#include "time.h"
#include <Rousis7segment.h>
#include <ModbusRTU.h>
#include <EEPROM.h>
#include <string>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Ds1302.h>

#define OPEN_HOT_SPOT 39
#define LED 13
#define RXD2 16
#define TXD2 17
#define DIR 5
#define REG1 10
#define REG_TIME 10
#define REG_BRIGHTNESS 20
//#define EEP_BRIGHT_ADDRESS 0X100
#define BUTTON1   36 //pin D5
#define SLAVE_ID 0
#define SAMPLES_MAX 60

//===========================================================================
//   WiFi now 
//uint8_t broadcastAddress1[] = { 0xAC, 0x0B, 0xFB, 0x24, 0x5C, 0x34 }; //AC:0B:FB:24:5C:34
//esp_err_t result;

//  ----- EEProme addresses -----
//EEPROM data ADDRESSER
#define EEP_WIFI_SSID 0
#define EEP_WIFI_PASS 32
#define EEP_USER_LOGIN 128
#define EEP_USER_PASS 160
#define EEP_SENSOR 196
#define EEP_DEFAULT_LOGIN 197
#define EEP_DEFAULT_WiFi 198
#define EEP_PRIGHTNESS 199
#define EEP_ADDRESS 200
#define EEP_WEB_UPDATE 201
#define EEP_WIFINOW_EN 202
#define EEP_TEMPO 203
#define EEP_TIME_DISP 204
#define EEP_DATE_DISP 205
#define EEP_TEMP_DISP 206
#define EEP_HUMI_DISP 207

//============================================================================
// DS1302 RTC instance
Ds1302 rtc(4, 32, 2);

// GPIO where the DS18B20 is connected to
const int oneWireBus = 32;
// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);
// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);

ModbusRTU mb;
Rousis7segment myLED(DIGITS_DISPLAY, 33, 26, 27, 14);    // Uncomment if not using OE pin

const int Open_Hotspot = OPEN_HOT_SPOT;

static unsigned long oldTime = 0;
int i = 0;
char Display_buf[15];
uint8_t Brightness = 255;
uint8_t BrighT_Update_Count = 0;
uint8_t Clock_Updated_once = 0;
uint8_t Connect_status = 0;
uint8_t Show_IP = 0;
uint8_t Last_Update=0xff;
String st;
String content;
String esid;
String epass = "";

uint8_t Sensor_on;
uint8_t Default_login;
int8_t samples_metter = -1;
uint16_t Timer_devide_photo = 0;
uint8_t IR_toggle;

uint16_t photosensor;
uint16_t button1;
uint16_t status;
uint16_t WiFiStatus;
// Potentiometer is connected to GPIO 34 (Analog ADC1_CH6) 
const int photoPin = 36;
// variable for storing the potentiometer value
int photoValue = 0;

const char* soft_ID = "Rousis Systems LTD\nWeb_LED_Clock_V2.2";
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0; // 7200;
const int   daylightOffset_sec = 0; // 3600;

//==================================================================================
// GUI settings 
//----------------------------------------------------------------------------------
#define SAMPLES_MAX 60
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);
DNSServer dnsServer;

const char* ssid = "rousis";
const char* password = "rousis074520198";
const char* hostname = "espui";

uint16_t password_text, user_text;
uint16_t wifi_ssid_text, wifi_pass_text;

String stored_user, stored_password;
String stored_ssid, stored_pass;
uint8_t photo_samples[60];

uint16_t mainTime;
uint16_t mainNumber;
//----------------------------------------------------------------------------------------
//  UI functions callback
void getTimeCallback(Control* sender, int type) {
    
    if (type == B_UP) {
        Serial.print("Time set: ID: ");
        Serial.print(sender->id);
        Serial.print(", Value: ");
        Serial.println(sender->value);
        ESPUI.updateTime(mainTime);        

    }else if (type == TM_VALUE) {
        Serial.print("Time_Value: ");
        Serial.println(sender->value);
    }
}

void numberCall(Control* sender, int type) {
    Serial.println("Instruction Nn:");
    Serial.println(sender->label);
    Serial.println("Type:");
    Serial.println(type);

}

void textCall(Control* sender, int type) {
    Serial.print("Text: ID: ");
    Serial.print(sender->id);
    Serial.print(", Value: ");
    Serial.println(sender->value);
}

void slider(Control* sender, int type) {
    Serial.print("Slider: ID: ");
    Serial.print(sender->id);
    Serial.print(", Value: ");
    Serial.println(sender->value);
    Brightness = (sender->value.toInt()) & 0xff;
    //dmd.setBrightness(Brightness);
    EEPROM.write(EEP_PRIGHTNESS, Brightness);
    EEPROM.commit();

    myLED.displayBrightness(Brightness);
    if (!mb.slave()) {
        mb.writeHreg(SLAVE_ID, REG_BRIGHTNESS, Brightness, cbWrite);
    }
}

void enterWifiDetailsCallback(Control* sender, int type) {
    if (type == B_UP) {
        if (sender->label == "WiFi")
        {
            Serial.println();
            Serial.print("Saving credentials to EPROM: ");
            Serial.print(ESPUI.getControl(wifi_ssid_text)->value);
            Serial.print(" - Size: ");
            Serial.println(sizeof(ESPUI.getControl(wifi_ssid_text)->value));
           /* Serial.println(ESPUI.getControl(wifi_pass_text)->value); */
            writeString(EEP_WIFI_SSID, ESPUI.getControl(wifi_ssid_text)->value, 32);
            writeString(EEP_WIFI_PASS, ESPUI.getControl(wifi_pass_text)->value, 64);
            //EEPROM.commit();

            ESPUI.getControl(status)->value = "Saved credentials to EEPROM<br>SSID: "
                + read_String(EEP_WIFI_SSID, 32)
                + "<br>Password: " + read_String(EEP_WIFI_PASS, 32);
            ESPUI.updateControl(status);

            Serial.println(read_String(EEP_WIFI_SSID, 32));
            Serial.println(read_String(EEP_WIFI_PASS, 32));
        }
        else if (sender->label == "User") {
            Serial.println();
            Serial.println("Saving User to EPROM: ");
            /*Serial.println(ESPUI.getControl(user_text)->value);
            Serial.println(ESPUI.getControl(password_text)->value);*/
            writeString(EEP_USER_LOGIN, ESPUI.getControl(user_text)->value, sizeof(ESPUI.getControl(user_text)->value));
            writeString(EEP_USER_PASS, ESPUI.getControl(password_text)->value, sizeof(ESPUI.getControl(password_text)->value));
            //EEPROM.commit();

            ESPUI.getControl(status)->value = "Saved login details to EEPROM<br>User name: "
                + read_String(EEP_USER_LOGIN,32)
                + "<br>Password: " + read_String(EEP_USER_PASS, 32);
            ESPUI.updateControl(status);
            Serial.println(read_String(EEP_USER_LOGIN, 32));
            Serial.println(read_String(EEP_USER_PASS, 32));
        }
        
        
        /*Serial.println("Reset..");
        ESP.restart();*/
    }
}

void writeString(char add, String data, uint8_t length)
{
    int _size = length; // data.length();
    int i;
    for (i = 0; i < _size; i++)
    {
        if (data[i] == 0) { break; }
        EEPROM.write(add + i, data[i]);
    }
    EEPROM.write(add + i, '\0');   //Add termination null character for String Data
    EEPROM.commit();
}

String read_String(char add, uint8_t length)
{
    int i;
    char data[100]; //Max 100 Bytes
    int len = 0;
    unsigned char k;
    k = EEPROM.read(add);
    while (k != '\0' && len < length)   //Read until null character
    {
        k = EEPROM.read(add + len);
        data[len] = k;
        len++;
    }
    data[len] = '\0';
    return String(data);
}

void ReadWiFiCrententials(void) {
    //uint8_t DefaultWiFi = EEPROM.read(EEP_DEFAULT_WiFi);
    if (Default_login || digitalRead(OPEN_HOT_SPOT) == 0) { ///testing to default
        Serial.println("Load default WiFi Crententials..");
        unsigned int i;
        stored_ssid = "rousis";  // "Zyxel_816E51"; // "rousis";
        stored_pass = "rousis074520198";  // "8GJ4B3GP"; // "rousis074520198";
        writeString(EEP_WIFI_SSID, stored_ssid, sizeof(stored_ssid));
        writeString(EEP_WIFI_PASS, stored_pass, sizeof(stored_pass));
        stored_user = "espboard";
        stored_password = "12345678";
        writeString(EEP_USER_LOGIN, stored_user, sizeof(stored_user));
        writeString(EEP_USER_PASS, stored_password, sizeof(stored_password));

        EEPROM.write(EEP_DEFAULT_WiFi, 0); //Erase the EEP_DEFAULT_WiFi
        EEPROM.write(EEP_DEFAULT_LOGIN, 0);
        EEPROM.commit();
    }
    else {
        stored_ssid = read_String(EEP_WIFI_SSID, 32);
        stored_pass = read_String(EEP_WIFI_PASS, 32);
    }
}

void Readuserdetails(void) {
    stored_user = read_String(EEP_USER_LOGIN, 32);
    stored_password = read_String(EEP_USER_PASS, 32);   

    Serial.println();
    Serial.print("stored_user: ");
    Serial.println(stored_user);
    Serial.print("stored_password: ");
    Serial.println(stored_password);
    /*readStringFromEEPROM(stored_user, 128, 32);
    readStringFromEEPROM(stored_password, 160, 32);*/
}

void ReadSettingEeprom() {
    Serial.println("Rad Settings EEPROM: ");
    Sensor_on = EEPROM.read(EEP_SENSOR);
    if (Sensor_on > 1) { Sensor_on = 1; }
    Serial.print("Photo sensor= ");
    Serial.println(Sensor_on, HEX);
    Default_login = EEPROM.read(EEP_DEFAULT_LOGIN);
    Serial.print("Default login= ");
    Serial.println(Default_login, HEX);
    Brightness = EEPROM.read(EEP_PRIGHTNESS);
    Serial.print("Brightness= ");
    Serial.println(Brightness);
    Serial.print("Display Items: ");
    Serial.print(EEPROM.read(EEP_TIME_DISP)); Serial.print(" ");
    Serial.print(EEPROM.read(EEP_DATE_DISP)); Serial.print(" ");
    Serial.print(EEPROM.read(EEP_TEMP_DISP)); Serial.print(" ");
    Serial.print(EEPROM.read(EEP_HUMI_DISP)); Serial.println();
    Serial.println("-------------------------------");
}

void textCallback(Control* sender, int type) {
    //This callback is needed to handle the changed values, even though it doesn't do anything itself.
}

void ResetCallback(Control* sender, int type) {
    if (type == B_UP) {
        Serial.println("Reset..");
        ESP.restart();
    }
}

void ClearCallback(Control* sender, int type) {
    switch (type) {
    case B_DOWN:
        Serial.println("Button DOWN");
        setTime(2023, 7, 19, 12, 0, 0, 0);
        break;
    case B_UP:
        Serial.println("Button UP");
        break;
    }
}

void generalCallback(Control* sender, int type) {
    Serial.print("CB: id(");
    Serial.print(sender->id);
    Serial.print(") Type(");
    Serial.print(type);
    Serial.print(") '");
    Serial.print(sender->label);
    Serial.print("' = ");
    Serial.println(sender->value);
    if (sender->label == "Set_time") {
        // Ανάλυση του ISO 8601 string
        int year, month, day, hour, minute, second;
        float milliseconds;
        sscanf(sender->value.c_str(), "%d-%d-%dT%d:%d:%d.%fZ", &year, &month, &day, &hour, &minute, &second, &milliseconds);

        struct tm tm;
        tm.tm_year = year - 1900; // Το έτος πρέπει να είναι έτος από 1900.
        tm.tm_mon = month - 1; // Οι μήνες ξεκινούν από το 0 (Ιανουάριος).
        tm.tm_mday = day;
        tm.tm_hour = hour;
        tm.tm_min = minute;
        tm.tm_sec = second;
        // Ρύθμιση του χρόνου και της ημερομηνίας
        Serial.println("Trying to fix the local time - Set the local RTC..");
        Serial.println(&tm, "%H:%M:%S");
        //setTime(2021, 3, 28, 1, 59, 50, 1);
        setTimezone("GMT0");
        setTime(year, month, day, hour, minute, second, 1);
        SetExtRTC();
        setTimezone("EET-2EEST,M3.5.0/3,M10.5.0/4");
    }
    else if (sender->label == "Tempo Seconds")
    {
        EEPROM.write(EEP_TEMPO, sender->value.toInt());
        EEPROM.commit();
    }
    else if (sender->label == "Display Info Options")
    {
        EEPROM.write(EEP_TIME_DISP, sender->value.toInt());
        EEPROM.commit();
    }
    else if (sender->label == "Display Date")
    {
        EEPROM.write(EEP_DATE_DISP, sender->value.toInt());
        EEPROM.commit();
    }
    else if (sender->label == "Display Temp")
    {
        EEPROM.write(EEP_TEMP_DISP, sender->value.toInt());
        EEPROM.commit();
    }
    else if (sender->label == "Display Humi")
    {
        EEPROM.write(EEP_HUMI_DISP, sender->value.toInt());
        EEPROM.commit();
    } 
    else if (sender->label == "Auto Sensor Brigntness")
    {
        EEPROM.write(EEP_SENSOR, sender->value.toInt());
        EEPROM.commit();
    }
}

void readStringFromEEPROM(String& buf, int baseaddress, int size) {
    buf.reserve(size);
    for (int i = baseaddress; i < baseaddress + size; i++) {
        char c = EEPROM.read(i);
        buf += c;
        if (!c) break;
    }
}
//=========================================================================
// Time settings
//=========================================================================
void setTimezone(String timezone) {
    Serial.printf("  Setting Timezone to %s\n", timezone.c_str());
    setenv("TZ", timezone.c_str(), 1);  //  Now adjust the TZ.  Clock settings are adjusted to show the new local time
    tzset();
}

bool cbWrite(Modbus::ResultCode event, uint16_t transactionId, void* data) {
  Serial.printf_P("Request result: 0x%02X, Mem: %d\n", event, ESP.getFreeHeap());

  return true;
}
//=========================================================================
//  WiFiNOW functions

//typedef struct struct_sending {
//    String time;
//    String lap;
//    String score;
//} struct_sending;
//
//struct_sending score_senting;
//struct_sending time_senting;
//esp_now_peer_info_t peerInfo;
//
//// callback when data is sent
//void OnDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
//    char macStr[18];
//    Serial.print("Packet to: ");
//    // Copies the sender mac address to a string
//    snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
//        mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
//    Serial.print(macStr);
//    Serial.print(" send status:\t");
//    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
//}

//=========================================================================
void setup(){
  EEPROM.begin(255);
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  sensors.begin();
  sensors.setResolution(9);
  rtc.init();
  mb.begin(&Serial2,DIR,true);
  mb.master();
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);
  pinMode(Open_Hotspot, INPUT_PULLUP);
  myLED.displayEnable();     // This command has no effect if you aren't using OE pin
  myLED.normalMode();

  ReadSettingEeprom();
  myLED.displayBrightness(Brightness);
  if (!mb.slave()) {
      mb.writeHreg(SLAVE_ID, REG_BRIGHTNESS, Brightness, cbWrite);
  }

  initial_leds_test();
  
  Serial.print("Hot spot switch: ");
  if (digitalRead(OPEN_HOT_SPOT))
  {
      Serial.println("OFF");
  }
  else {
      Serial.println("ON");
  }
  //-----------------------------------------------------------------------
// GUI interface setup
  WiFi.setHostname(hostname);
  ReadWiFiCrententials();

  Serial.println("Stored SSID:");
  Serial.println(stored_ssid);
  Serial.println("Stored password:");
  Serial.println(stored_pass);
  Serial.println(".....................");
  /*Serial.println("Stored User:");
  Serial.println(stored_user);
  Serial.println("Stored User pass:");
  Serial.println(stored_password);*/
  // try to connect to existing network
  WiFi.mode(WIFI_AP_STA);

  WiFi.begin(stored_ssid.c_str(), stored_pass.c_str());
  //WiFi.begin(ssid, password);
  Serial.print("\n\nTry to connect to existing network");

  uint8_t timeout = 10;

  // Wait for connection, 5s timeout
  do {
      delay(500);
      Serial.print(".");
      timeout--;
  } while (timeout && WiFi.status() != WL_CONNECTED);

  // not connected -> create hotspot
  if (WiFi.status() != WL_CONNECTED) {
      Serial.print("\n\nCreating hotspot");

      WiFi.mode(WIFI_AP);
      WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
      WiFi.softAP(ssid);
      timeout = 5;

      do {
          delay(500);
          Serial.print(".");
          timeout--;
      } while (timeout);
  }

  //------------------------------------------------------------------------
    // Init ESP-NOW
  //WiFi.mode(WIFI_MODE_APSTA);
  //Serial.println();
  //if (esp_now_init() != ESP_OK) {
  //    Serial.println("Error initializing ESP-NOW");
  //    //return;
  //}
  //else {
  //    Serial.println("initialized ESP - NOW");
  //}
  //esp_now_register_send_cb(OnDataSent);

  //// register peer
  //peerInfo.channel = 0;
  //peerInfo.encrypt = false;
  //// register first peer
  //memcpy(peerInfo.peer_addr, broadcastAddress1, 6);
  //if (esp_now_add_peer(&peerInfo) != ESP_OK) {
  //    Serial.println("Failed to add peer");
  //    //return;
  //} else {
  //    Serial.println("Succesfully added peer");
  //}

  //------------------------------------------------------------------------

  dnsServer.start(DNS_PORT, "*", apIP);

  Serial.println("\n\nWiFi parameters:");
  Serial.print("Mode: ");
  Serial.println(WiFi.getMode() == WIFI_AP ? "Station" : "Client");
  Serial.print("IP address: ");
  Serial.println(WiFi.getMode() == WIFI_AP ? WiFi.softAPIP() : WiFi.localIP());

  Serial.println("WiFi mode:");
  Serial.println(WiFi.getMode());

  IPdisplay();

  //---------------------------------------------------------------------------------------------
  //We need this CSS style rule, which will remove the label's background and ensure that it takes up the entire width of the panel
  String clearLabelStyle = "background-color: unset; width: 100%;";

  uint16_t tab1 = ESPUI.addControl(ControlType::Tab, "Main Display", "Main Display");
  uint16_t tab2 = ESPUI.addControl(ControlType::Tab, "User Settings", "User");
  uint16_t tab3 = ESPUI.addControl(ControlType::Tab, "WiFi Settings", "WiFi");

  mainTime = ESPUI.addControl(Time, "Set_time", "Set_time", None, 0, generalCallback);
  ESPUI.addControl(Button, "Synchronize with your time", "Set", Carrot, tab1, getTimeCallback);
  //ESPUI.updateTime(mainTime);

  //ESPUI.addControl(Separator, "Other Options", "", None, tab1);

  //button1 = ESPUI.addControl(ControlType::Button, "Push Button", "Clear All", ControlColor::Carrot, tab1, &ClearCallback);

  //We will now need another label style. This one sets its width to the same as a switcher (and turns off the background)
  String switcherLabelStyle = "width: 60px; margin-left: .3rem; margin-right: .3rem; background-color: unset;";
  //Some controls can even support vertical orientation, currently Switchers and Sliders
  ESPUI.addControl(Separator, "Display controls", "", None, tab1);
  uint8_t Switch_val = EEPROM.read(EEP_TIME_DISP);
  auto vertgroupswitcher = ESPUI.addControl(Switcher, "Display Info Options", String(Switch_val), Dark, tab1, generalCallback);
  ESPUI.setVertical(vertgroupswitcher);
  //On the following lines we wrap the value returned from addControl and send it straight to setVertical
  Switch_val = EEPROM.read(EEP_DATE_DISP);
  ESPUI.setVertical(ESPUI.addControl(Switcher, "Display Date", String(Switch_val), None, vertgroupswitcher, generalCallback));
  Switch_val = EEPROM.read(EEP_TEMP_DISP);
  ESPUI.setVertical(ESPUI.addControl(Switcher, "Display Temp", String(Switch_val), None, vertgroupswitcher, generalCallback));
  Switch_val = EEPROM.read(EEP_HUMI_DISP);
  ESPUI.setVertical(ESPUI.addControl(Switcher, "Display Humi", String(Switch_val), None, vertgroupswitcher, generalCallback));
  //The mechanism for labelling vertical switchers is the same as we used above for horizontal ones
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, vertgroupswitcher), clearLabelStyle);
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "Time", None, vertgroupswitcher), switcherLabelStyle);
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "Date", None, vertgroupswitcher), switcherLabelStyle);
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "Temp", None, vertgroupswitcher), switcherLabelStyle);
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "Humi", None, vertgroupswitcher), switcherLabelStyle);

  //Number inputs also accept Min and Max components, but you should still validate the values.
  uint8_t tempo = EEPROM.read(EEP_TEMPO);
  char tempoString[3];
  itoa(tempo, tempoString, 10);
  mainNumber = ESPUI.addControl(Number, "Tempo Seconds", tempoString, Emerald, tab1, generalCallback);
  ESPUI.addControl(Min, "", "1", None, mainNumber);
  ESPUI.addControl(Max, "", "9", None, mainNumber);

  photosensor = ESPUI.addControl(Switcher, "Auto Sensor Brigntness", "0", Dark, tab1, generalCallback);
  ESPUI.getControl(photosensor)->value = String(Sensor_on);

  uint16_t mainSlider = ESPUI.addControl(ControlType::Slider, "Brightness", "255", ControlColor::Alizarin, tab1, &slider);
  ESPUI.addControl(Min, "", "0", None, mainSlider);
  ESPUI.addControl(Max, "", "255", None, mainSlider);
  ESPUI.getControl(mainSlider)->value = String(Brightness);

  Readuserdetails();
  user_text = ESPUI.addControl(Text, "User name", stored_user, Alizarin, tab2, textCallback); // stored_user
  ESPUI.addControl(Max, "", "32", None, user_text);
  password_text = ESPUI.addControl(Text, "Password", stored_password, Alizarin, tab2, textCallback); //stored_password
  ESPUI.addControl(Max, "", "32", None, password_text);
  ESPUI.setInputType(password_text, "password");
  ESPUI.addControl(Button, "User", "Save", Peterriver, tab2, enterWifiDetailsCallback);

  wifi_ssid_text = ESPUI.addControl(Text, "SSID", stored_ssid, Alizarin, tab3, textCallback); // stored_ssid
  ESPUI.addControl(Max, "", "32", None, wifi_ssid_text);
  wifi_pass_text = ESPUI.addControl(Text, "Password", stored_pass, Alizarin, tab3, textCallback); //stored_pass
  ESPUI.addControl(Max, "", "64", None, wifi_pass_text);
  ESPUI.addControl(Button, "WiFi", "Save", Peterriver, tab3, enterWifiDetailsCallback);

  ESPUI.addControl(ControlType::Button, "Reset device", "Reset", ControlColor::Emerald, tab3, &ResetCallback);

  //WiFiStatus = ESPUI.addControl(Label, "WiFi Status", "Wifi Status ok", ControlColor::None, tab3);

  photoValue = analogRead(photoPin);
  status = ESPUI.addControl(Label, "Status", "System status: OK", Wetasphalt, Control::noParent);
  ESPUI.getControl(status)->value = "Photo sensor = " + String(photoValue);
  ESPUI.addControl(Separator, soft_ID, "", Peterriver, tab1);

  ESPUI.begin("- Web LED Clock", stored_user.c_str(), stored_password.c_str()); //"espboard", 
 
  digitalWrite(LED, LOW);
  //-------------------------------------------------------------
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  //configTime( 0, 0, ntpServer);
   //Europe/Athens	EET-2EEST,M3.5.0/3,M10.5.0/4
  setTimezone("EET-2EEST,M3.5.0/3,M10.5.0/4");
  printLocalTime();
  SetExtRTC();
  oldTime = millis();
}

void loop() {
    dnsServer.processNextRequest();
    uint8_t tempo = EEPROM.read(EEP_TEMPO);
    if (tempo > 9 || tempo == 0)
        tempo = 9;
    Serial.print("Tempo Sec.= ");
    Serial.println(tempo);
    digitalWrite(LED, HIGH);
    //....................................................................
    oldTime = millis();
    if (!EEPROM.read(EEP_TIME_DISP) && !EEPROM.read(EEP_DATE_DISP) && !EEPROM.read(EEP_TEMP_DISP) && !EEPROM.read(EEP_DATE_DISP))
    {
        myLED.print("------", INVERT_DISPLAY);
        delay(tempo * 3000);
    }
    printLocalTime();
    while (millis() - oldTime < tempo * 1000 && EEPROM.read(EEP_TIME_DISP))
    {
        digitalWrite(LED, LOW);
        displayocalTime(0x80);
        delay(500);
        digitalWrite(LED, HIGH);
        displayocalTime(0);
        delay(500);
    }

    if (EEPROM.read(EEP_DATE_DISP)) {
        digitalWrite(LED, LOW);
        displayocalDate();
        delay(tempo * 1000);
    }

    if (EEPROM.read(EEP_TEMP_DISP)) {
        sensors.requestTemperatures();
        float temperature = sensors.getTempCByIndex(0);
        if (temperature > -50)
        {
            char buf[10] = { 0 };
            itoa(temperature, buf, 10);

            if (DIGITS_DISPLAY == 4)
            {
                Display_buf[0] = buf[0];
                Display_buf[1] = buf[1];
                if (!buf[2]) {
                    Display_buf[2] = '*'; // 'º';
                    Display_buf[3] = 'C';
                }
                else {
                    Display_buf[2] = buf[2];
                    Display_buf[3] = '*';
                }
                myLED.print(Display_buf, INVERT_DISPLAY);
            }
            else if (DIGITS_DISPLAY == 6)
            {
                Display_buf[1] = buf[0];
                Display_buf[2] = buf[1];
                if (!buf[2]) {
                    Display_buf[3] = '*'; // 'º';
                    Display_buf[4] = 'C';
                }
                else {
                    Display_buf[3] = buf[2];
                    Display_buf[4] = '*';
                }
                myLED.print(Display_buf, INVERT_DISPLAY);
            }
        }
        else {
            Display_buf[0] = '-'; // 'º';
            Display_buf[1] = '-';
            Display_buf[2] = '*'; // 'º';
            Display_buf[3] = 'C';
            Display_buf[4] = ' ';
            Display_buf[5] = ' ';
            myLED.print(Display_buf, INVERT_DISPLAY);
        }

        

        Serial.println("Ampient Temperature is:");
        Serial.print(temperature);
        Serial.println("ºC");

        delay(tempo * 1000);
    }

    if (!mb.slave() && BrighT_Update_Count > 60) {
        mb.writeHreg(SLAVE_ID, REG_BRIGHTNESS, Brightness, cbWrite);
        BrighT_Update_Count = 0;
    }
    else {
        BrighT_Update_Count++;
    }

    /*
    while ((WiFi.status() != WL_CONNECTED))
    {
        WiFi.disconnect();
        WiFi.reconnect();
        Serial.println("No internet Try to reconnect...");
    }
    */
    // Check if the connection is lost
    // Check if the connection is lost
    if (WiFi.getMode() != WIFI_AP && WiFi.status() != WL_CONNECTED) {
        Serial.println("Connection lost. Reconnecting...");
        WiFi.reconnect();
        // delay(2000); // Προσθέστε μια μικρή καθυστέρηση
    }
}

String printTimeString(void) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return "--";
    }
    //Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    char time_str[10];
    strftime(time_str, 3, "%H:%M:%S", &timeinfo);
    return time_str;
}

void displayocalDate() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return;
    }

    char date[3];
    strftime(date, 3, "%d", &timeinfo);
    char month[3];
    strftime(month, 3, "%m", &timeinfo);
    char Displaybuf[6];
    if (DIGITS_DISPLAY == 4)
    {
        Displaybuf[0] = date[0];
        Displaybuf[1] = date[1] | 0x80;
        Displaybuf[2] = month[0];
        Displaybuf[3] = month[1];
    }
    else if (DIGITS_DISPLAY == 6)
    {
        Displaybuf[0] = ' ';
        Displaybuf[1] = date[0];
        Displaybuf[2] = date[1];
        Displaybuf[3] = '-';
        Displaybuf[4] = month[0];
        Displaybuf[5] = month[1];
    }
    myLED.print(Displaybuf, INVERT_DISPLAY);
}

void displayocalTime(uint8_t dots_on){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
      myLED.print("------", INVERT_DISPLAY);
    Serial.println("Failed to obtain time to display");
    return;
  }
  
  char timeHour[3];
  strftime(timeHour,3, "%H", &timeinfo);
  char timeMinute[3];
  strftime(timeMinute,3, "%M", &timeinfo);  
  char Displaybuf[6];
  if (DIGITS_DISPLAY == 4)
  {      
      Displaybuf[0] = timeHour[0];
      Displaybuf[1] = timeHour[1] | dots_on;
      Displaybuf[2] = timeMinute[0] | dots_on;
      Displaybuf[3] = timeMinute[1];
  }
  else if (DIGITS_DISPLAY == 6)
  {
      char timeSecond[3];
      strftime(timeSecond, 3, "%S", &timeinfo);
      Displaybuf[0] = timeHour[0];
      Displaybuf[1] = timeHour[1] | dots_on;
      Displaybuf[2] = timeMinute[0];
      Displaybuf[3] = timeMinute[1] | dots_on;
      Displaybuf[4] = timeSecond[0];
      Displaybuf[5] = timeSecond[1];
  }
  
  

  int conv_min = atoi(timeMinute);
  int send_data = atoi(timeHour) | dots_on;
  send_data = (send_data << 8) | conv_min;
  mb.task();
  //yield();
  if (!mb.slave()) {
        mb.writeHreg(SLAVE_ID, REG1, send_data,  cbWrite);
      } 
      
  myLED.print(Displaybuf, INVERT_DISPLAY);
}

void printLocalTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return;
    }

    //Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    char timeStringBuff[50]; //50 chars should be enough
    strftime(timeStringBuff, sizeof(timeStringBuff), "%a, %d %h %Y %H:%M:%S", &timeinfo);
    //print like "const char*"
    Serial.println(timeStringBuff);
    //Optional: Construct String object 
    String asString(timeStringBuff);

    ESPUI.getControl(status)->value = "Display Time: " + asString + "<br>Photo sensor = " + String(photoValue);
    //Serial.println(&timeinfo, "%H:%M:%S"); "Photo sens. = " + String(photoValue);
}

void SetExtRTC() {

    Serial.println("Setting the external RTC");
    setTimezone("GMT0");
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        get_time_externalRTC();
        return;
    }
    setTimezone("EET-2EEST,M3.5.0/3,M10.5.0/4");
    /*
        Serial.print("Time to set DS1302: ");
        Serial.print(timeinfo.tm_year); Serial.print(" ");
        Serial.print(timeinfo.tm_mon); Serial.print(" ");
        Serial.print(timeinfo.tm_mday); Serial.print(" ");
        Serial.print(timeinfo.tm_hour); Serial.print(" ");
        Serial.print(timeinfo.tm_min); Serial.print(" ");
        Serial.println(timeinfo.tm_sec);
        Serial.println("--------------------------------------");*/

    Ds1302::DateTime dt = {
           .year = timeinfo.tm_year,
           .month = timeinfo.tm_mon + 1,
           .day = timeinfo.tm_mday,
           .hour = timeinfo.tm_hour,
           .minute = timeinfo.tm_min,
           .second = timeinfo.tm_sec,
           .dow = timeinfo.tm_wday
    };
    rtc.setDateTime(&dt);

    Serial.println(&timeinfo, "%A, %d %B %Y %H:%M:%S");
    Serial.println("----------------------------------------");
}

void get_time_externalRTC() {
    // get the current time
    Ds1302::DateTime now;
    rtc.getDateTime(&now);

    Serial.println();
    Serial.println("Read and set the external Time from DS1302:");

    Serial.print("20");
    Serial.print(now.year);    // 00-99
    Serial.print('-');
    if (now.month < 10) Serial.print('0');
    Serial.print(now.month);   // 01-12
    Serial.print('-');
    if (now.day < 10) Serial.print('0');
    Serial.print(now.day);     // 01-31
    Serial.print(' ');
    Serial.print(now.dow); // 1-7
    Serial.print(' ');
    if (now.hour < 10) Serial.print('0');
    Serial.print(now.hour);    // 00-23
    Serial.print(':');
    if (now.minute < 10) Serial.print('0');
    Serial.print(now.minute);  // 00-59
    Serial.print(':');
    if (now.second < 10) Serial.print('0');
    Serial.print(now.second);  // 00-59
    Serial.println();
    Serial.println("---------------------------------------------");
        
    setTimezone("GMT0");
    setTime(now.year + 2000, now.month, now.day, now.hour, now.minute, now.second, 1);
    setTimezone("EET-2EEST,M3.5.0/3,M10.5.0/4");
}
//----------------------------------------------- Fuctions used for WiFi credentials saving and connecting to it which you do not need to change
bool testWifi(void)
{
    int c = 0;
    Serial.println("Waiting for Wifi to connect");
    while (c < 20) {
        if (WiFi.status() == WL_CONNECTED)
        {
            Serial.println("");
            return true;
        }
        delay(500);
        Serial.print("*");
        c++;
    }
    Serial.println("");
    Serial.println("Connect timed out, opening AP");
    return false;
}

void setTime(int yr, int month, int mday, int hr, int minute, int sec, int isDst) {
    struct tm tm;

    tm.tm_year = yr - 1900;   // Set date
    tm.tm_mon = month - 1;
    tm.tm_mday = mday;
    tm.tm_hour = hr;      // Set time
    tm.tm_min = minute;
    tm.tm_sec = sec;
    tm.tm_isdst = isDaylightSavingTime(&tm);  // 1 or 0
    time_t t = mktime(&tm);
    Serial.printf("Setting time: %s", asctime(&tm));
    struct timeval now = { .tv_sec = t };
    settimeofday(&now, NULL);
}

bool isDaylightSavingTime(struct tm* timeinfo) {
    // Implement your logic to check if DST is in effect based on your timezone's rules
    // For example, you might check if the current month and day fall within the DST period.

    int month = timeinfo->tm_mon + 1; // Month is 0-11, so add 1
    int day = timeinfo->tm_mday;

    // Add your DST logic here
    // For example, assuming DST is from April to October
    if ((month > 3) && (month < 11)) {
        return true;
    }

    return false;
}

void IPdisplay() {

    char Displaybuf[7];
    IPAddress localIp;
    char arrIp[17];
    const char* strLocalIp;

    if (WiFi.getMode() == WIFI_AP) {
        memcpy(Displaybuf, "AP Display:", 11);
        localIp = WiFi.softAPIP();
        strLocalIp = WiFi.softAPIP().toString().c_str();
    }
    else {
        memcpy(Displaybuf, "IP Display:", 11);
        localIp = WiFi.localIP();
        strLocalIp = WiFi.localIP().toString().c_str();
    }
    myLED.print(Displaybuf , INVERT_DISPLAY);
    delay(1000);

    myLED.print("      ", INVERT_DISPLAY);
    Displaybuf[7] = {' '};
    itoa(localIp[0], Displaybuf, 10);
    if (Displaybuf[1] == 0) { Displaybuf[1] = ' '; Displaybuf[2] = 0; }  
    myLED.print(Displaybuf, INVERT_DISPLAY);
    delay(1000);
    myLED.print(".   ", INVERT_DISPLAY);
    delay(500);
    itoa(localIp[1], Displaybuf, 10);
    if (Displaybuf[1] == 0) { Displaybuf[1] = ' '; Displaybuf[2] = 0; }
    myLED.print(Displaybuf, INVERT_DISPLAY);
    delay(1000);
    myLED.print(".   ", INVERT_DISPLAY);
    delay(500);
    itoa(localIp[2], Displaybuf, 10);
    if (Displaybuf[1] == 0) { Displaybuf[1] = ' '; Displaybuf[2] = 0; }
    myLED.print(Displaybuf, INVERT_DISPLAY);
    delay(1000);
    myLED.print(".   ", INVERT_DISPLAY);
    delay(500);
    itoa(localIp[3], Displaybuf, 10);
    if (Displaybuf[1] == 0) { Displaybuf[1] = ' '; Displaybuf[2] = 0; }
    myLED.print(Displaybuf, INVERT_DISPLAY);
    delay(1000);
    myLED.print(".   ", INVERT_DISPLAY);
    delay(500);
}

void photo_sample(void) {
    // Reading potentiometer value
    if (!Sensor_on) { return; }

    photoValue = analogRead(photoPin);
    if (photoValue < 200) { photoValue = 1; }
    photoValue = (4095 - photoValue);
    photoValue = 0.06 * photoValue; //(255 / 4095) = 0.06
    if (photoValue > 240) { photoValue = 255; }
    Brightness = photoValue;
    if (Brightness < 20) { Brightness = 20; }

    if (samples_metter == -1) {
        samples_metter = 0;
        //myLED.displayBrightness(Brightness);
        Serial.print("First Sensor value = ");
        Serial.println(Brightness);
    }
    photo_samples[samples_metter] = Brightness;
    samples_metter++;

    /*Serial.print("Sample metter nummber = ");
    Serial.println(samples_metter);
    Serial.print("Sensor value =  ");
    Serial.println(Brightness);*/

    if (samples_metter >= SAMPLES_MAX)
    {
        uint16_t A = 0;
        uint8_t i;
        for (i = 0; i < SAMPLES_MAX; i++) { A += photo_samples[i]; }
        samples_metter = 0;
        Brightness = A / SAMPLES_MAX;
        //uint8_t digits = eeprom_read_byte((uint8_t*)FAV_eep + DISPLAY_DIGITS);
        //TLC_config_byte(sram_brigt,digits);
        //myLED.displayBrightness(Brightness);
        Serial.print("Average Sensor value to brightness = ");
        Serial.println(Brightness);
    }
}

void initial_leds_test(void) {
    uint8_t n[6];
    int i;
    uint8_t val = 0b00000100;
    //for (i = 0; i < 8; i++) {        

        //val = val >> 1;
    val = 0b00000010;
    n[0] = val; n[1] = val; n[2] = val; n[3] = val; n[4] = val; n[5] = val;
    myLED.printDirect(n);
    delay(500);
    val = 0b00001000;
    n[0] = val; n[1] = val; n[2] = val; n[3] = val; n[4] = val; n[5] = val;
    myLED.printDirect(n);
    delay(500);
    val = 0b01000000;
    n[0] = val; n[1] = val; n[2] = val; n[3] = val; n[4] = val; n[5] = val;
    myLED.printDirect(n);
    delay(500);
    val = 0b00000100;
    n[0] = val; n[1] = val; n[2] = val; n[3] = val; n[4] = val; n[5] = val;
    myLED.printDirect(n);
    delay(500);
    val = 0b00000001;
    n[0] = val; n[1] = val; n[2] = val; n[3] = val; n[4] = val; n[5] = val;
    myLED.printDirect(n);
    delay(500);
    val = 0b00100000;
    n[0] = val; n[1] = val; n[2] = val; n[3] = val; n[4] = val; n[5] = val;
    myLED.printDirect(n);
    delay(500);
    val = 0b00010000;
    n[0] = val; n[1] = val; n[2] = val; n[3] = val; n[4] = val; n[5] = val;
    myLED.printDirect(n);
    delay(500);
    val = 0b10000000;
    n[0] = val; n[1] = val; n[2] = val; n[3] = val; n[4] = val; n[5] = val;
    myLED.printDirect(n);
    delay(500);
}
