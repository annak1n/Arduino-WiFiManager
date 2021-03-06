/* Written by JelleWho https://github.com/jellewie
   https://github.com/jellewie/Arduino-WiFiManager
    
   1. Load hardcoded data
   2. Load EEPROM data and save data
   3. while(no data) Set up AP mode and wait for user data
   4. try connecting, if (not) {GOTO3}

   DO NOT USE:
   Prefix 'WiFiManager' for anything
   Global variables of 'i' 'j' 'k'

   You can use "strip_ip, gateway_ip, subnet_mask" to set a static connection

   HOW TO ADD CUSTOM CALUES
   -"WiFiManager_VariableNames" Add the 'AP settings portal' name
   -"WiFiManager_EEPROM_SIZE"   [optional] Make sure it is big enough; if it's 10 and you have 2 settings (SSID, Password) than both get 10/2=5 bytes of storage, probably not enough (1 byte = 1 character)
   -"WiFiManager_Set_Value"     Set the action on what to do on startup with this value
   -"WiFiManager_Get_Value"     [optional] Set the action on what to fill in in the boxes in the 'AP settings portal'
*/

#define WiFiManager_ConnectionTimeOutMS 10000
#define WiFiManager_APSSID "ESP32"
#define WiFiManager_EEPROM_SIZE 64            //Max Amount of chars of 'SSID + PASSWORD' (+1) (+extra custom vars)
#define WiFiManager_EEPROM_Seperator char(9)  //use 'TAB' as a seperator 
#define WiFiManager_LED LED_BUILTIN           //The LED to give feedback on (like blink on error)

#define WiFiManager_SerialEnabled             //Disable to not send Serial debug feedback

const String WiFiManager_VariableNames[] {"SSID", "Password"};
const static byte WiFiManager_Settings = sizeof(WiFiManager_VariableNames) / sizeof(WiFiManager_VariableNames[0]); //Why filling this in if we can automate that? :)
const static byte WiFiManager_EEPROM_SIZE_EACH = WiFiManager_EEPROM_SIZE / WiFiManager_Settings;

bool WiFiManager_WaitOnAPMode = true;       //This holds the flag if we should wait in Apmode for data
bool WiFiManager_SettingsEnabled = false;   //This holds the flag to enable settings, else it would not responce to settings commands
//#define strip_ip, gateway_ip, subnet_mask to use static IP

#ifndef ssid
char ssid[WiFiManager_EEPROM_SIZE_EACH] = "";
#endif //ssid

#ifndef password
char password[WiFiManager_EEPROM_SIZE_EACH] = "";
#endif //password

#include <EEPROM.h>

byte WiFiManager_Start() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(WiFiManager_LED, HIGH);
  //starts wifi stuff, only returns when connected. will create Acces Point when needed
  /* <Return> <meaning>
     2 Can't begin EEPROM
     3 Can't write [all] data to EEPROM
  */
  if (!EEPROM.begin(WiFiManager_EEPROM_SIZE))
    return 2;
  String WiFiManager_a = WiFiManager_LoadEEPROM();
#ifdef WiFiManager_SerialEnabled
  Serial.println("WM: EEPROM data=" + WiFiManager_a);
#endif //WiFiManager_SerialEnabled
  if (WiFiManager_a != String(WiFiManager_EEPROM_Seperator)) {    //If there is data in EEPROM
    for (byte i = 1; i < WiFiManager_Settings + 1; i++) {
      byte j = WiFiManager_a.indexOf(char(WiFiManager_EEPROM_Seperator));
      if (j == 255)
        j = WiFiManager_a.length();
      String WiFiManager_TEMP_Value = WiFiManager_a.substring(0, j);
      if (WiFiManager_TEMP_Value != "")                     //If there is a value
        WiFiManager_Set_Value(i, WiFiManager_TEMP_Value);   //set the value in memory (and thus overwrite the Hardcoded stuff)
      WiFiManager_a = WiFiManager_a.substring(j + 1);
    }
  }
  bool WiFiManager_Connected = false;
  while (!WiFiManager_Connected) {
    if ((strlen(ssid) == 0 or strlen(password) == 0))
      WiFiManager_APMode();                 //No good ssid or password, entering APmode
    else {
      if (WiFiManager_Connect(WiFiManager_ConnectionTimeOutMS)) //try to connected to ssid password
        WiFiManager_Connected = true;
      else
        password[0] = (char)0;              //Clear this so we will enter AP mode (*just clearing first bit)
    }
  }
  digitalWrite(WiFiManager_LED, LOW);
#ifdef WiFiManager_SerialEnabled
  Serial.print("WM: ");
  Serial.println(WiFi.localIP()); //Just send it's IP on boot to let you know
#endif //WiFiManager_SerialEnabled
  return 1;
}
String WiFiManager_LoadEEPROM() {
  String WiFiManager_Value;
#ifdef WiFiManager_SerialEnabled
  Serial.print("WM: EEPROM LOAD");
#endif //WiFiManager_SerialEnabled
  for (int i = 0; i < WiFiManager_EEPROM_SIZE; i++) {
    byte WiFiManager_Input = EEPROM.read(i);
    if (WiFiManager_Input == 255)               //If at the end of data
      return WiFiManager_Value;                 //Stop and return all data stored
    if (WiFiManager_Input == 0)                 //If no data found (NULL)
      return String(WiFiManager_EEPROM_Seperator);
    WiFiManager_Value += char(WiFiManager_Input);
#ifdef WiFiManager_SerialEnabled
    Serial.print("_" + String(char(WiFiManager_Input)) + "_");
#endif //WiFiManager_SerialEnabled
  }
  return String(WiFiManager_EEPROM_Seperator);  //ERROR; [maybe] not enough space
}
bool WiFiManager_WriteEEPROM() {
  String WiFiManager_StringToWrite;                                   //Save to mem: <SSID>
  for (byte i = 0; i < WiFiManager_Settings; i++) {
    WiFiManager_StringToWrite += WiFiManager_Get_Value(i + 1, true);  //^            <Seperator>
    if (WiFiManager_Settings - i > 1)
      WiFiManager_StringToWrite += WiFiManager_EEPROM_Seperator;      //^            <Value>  (only if there more values)
  }
  WiFiManager_StringToWrite += char(255);                             //^            <emthy bit> (we use a emthy bit to mark the end)
#ifdef WiFiManager_SerialEnabled
  Serial.print("WM: EEPROM WRITE; '" + WiFiManager_StringToWrite + "'");
#endif //WiFiManager_SerialEnabled
  if (WiFiManager_StringToWrite.length() > WiFiManager_EEPROM_SIZE)   //If not enough room in the EEPROM
    return false;                                         //Return false; not all data is stored
  for (int i = 0; i < WiFiManager_StringToWrite.length(); i++)        //For each character to save
    EEPROM.write(i, (int)WiFiManager_StringToWrite.charAt(i));        //Write it to the EEPROM
  EEPROM.commit();
  Serial.println();
  return true;
}
void WiFiManager_handle_Connect() {
  if (!WiFiManager_SettingsEnabled)   //If settingscommand are disabled
    return;                           //Stop right away, and do noting

  String WiFiManager_Temp_HTML = "<strong>" + String(WiFiManager_APSSID) + " settings</strong><br><br><form action=\"/setup?\" method=\"get\">";
  for (byte i = 1; i < WiFiManager_Settings + 1; i++)
    WiFiManager_Temp_HTML += "<div><label>" + WiFiManager_VariableNames[i - 1] + "</label><input type=\"text\" name=\"" + i + "\" value=\"" + WiFiManager_Get_Value(i, false) + "\"></div>";
  WiFiManager_Temp_HTML += "<button>Send</button></form>";
  server.send(200, "text/html", WiFiManager_Temp_HTML);
}
void WiFiManager_handle_Settings() {
  if (!WiFiManager_SettingsEnabled)   //If settingscommand are disabled
    return;                           //Stop right away, and do noting
  String WiFiManager_MSG = "";
  int   WiFiManager_Code = 200;
  for (int i = 0; i < server.args(); i++) {
    String WiFiManager_ArguName = server.argName(i);
    String WiFiManager_ArgValue = server.arg(i);
    WiFiManager_ArgValue.trim();
    int j = WiFiManager_ArguName.toInt();
    if (j > 0 and j < 255 and WiFiManager_ArgValue != "") {
      WiFiManager_Set_Value(j, WiFiManager_ArgValue);
      WiFiManager_MSG += "Succesfull '" + WiFiManager_ArguName + "' = '" + WiFiManager_ArgValue + "'" + char(13);
    } else {
      WiFiManager_Code = 422;   //Flag we had a error
      WiFiManager_MSG += "ERROR; '" + WiFiManager_ArguName + "'='" + WiFiManager_ArgValue + "'" + char(13);
    }
  }
  server.send(WiFiManager_Code, "text/plain", WiFiManager_MSG);
  delay(10);
  WiFiManager_WaitOnAPMode = false;     //Flag we have input data, and we can stop waiting in APmode on data
  WiFiManager_WriteEEPROM();
}
void WiFiManager_StartServer() {
  static bool ServerStarted = false;
  if (!ServerStarted) {             //If the server hasn't started yet
    ServerStarted = true;
    server.begin();                 //Begin server
  }
}
void WiFiManager_EnableSetup(bool WiFiManager_TEMP_State) {
  WiFiManager_SettingsEnabled = WiFiManager_TEMP_State;
}
byte WiFiManager_APMode() {
  //IP of AP = 192.168.4.1
  /* <Return> <meaning>
    2 soft-AP setup Failed
  */
  if (!WiFi.softAP(WiFiManager_APSSID))
    return 2;
  WiFiManager_SettingsEnabled = true; //Flag we need to responce to settings commands
  WiFiManager_StartServer();          //start server (if we havn't already)
#ifdef WiFiManager_SerialEnabled
  Serial.print("WM: APMode on ip ");
  Serial.println(WiFi.softAPIP());
#endif //WiFiManager_SerialEnabled

  while (WiFiManager_WaitOnAPMode) {
    WiFiManager_Blink(100);  //Let the LED blink to show we are not connected
    server.handleClient();
  }
  WiFiManager_WaitOnAPMode = true;      //reset flag for next time
  WiFiManager_SettingsEnabled = false;  //Flag to stop responce to settings commands
  return 1;
}
bool WiFiManager_Connect(int WiFiManager_TimeOutMS) {
#ifdef WiFiManager_SerialEnabled
  Serial.println("WM: Connect to ssid='" + String(ssid) + "' password='" + String(password) + "'");
#endif //WiFiManager_SerialEnabled
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
#if defined(strip_ip) && defined(gateway_ip) && defined(subnet_mask)
  WiFi.config(strip_ip, gateway_ip, subnet_mask);
#endif
  unsigned long WiFiManager_StopTime = millis() + WiFiManager_TimeOutMS;
  while (WiFi.status() != WL_CONNECTED) {
    if (WiFiManager_StopTime < millis())     //If we are in overtime
      return false;
    WiFiManager_Blink(500);  //Let the LED blink to show we are not connected
  }
  return true;
}
void WiFiManager_Blink(int WiFiManager_Temp_Delay) {
  static unsigned long LastTime = millis();
  if (millis() > LastTime + WiFiManager_Temp_Delay) {
    LastTime = millis();
    digitalWrite(WiFiManager_LED, !digitalRead(WiFiManager_LED));
  }
}
void WiFiManager_Set_Value(byte WiFiManager_ValueID, String WiFiManager_Temp) {
#ifdef WiFiManager_SerialEnabled
  Serial.println("WM: Set value " + String(WiFiManager_ValueID) + " = " + WiFiManager_Temp);
#endif //WiFiManager_SerialEnabled
  WiFiManager_Temp.trim();                  //remove leading and trailing whitespace
  switch (WiFiManager_ValueID) {
    case 1:
      WiFiManager_Temp.toCharArray(ssid, WiFiManager_Temp.length() + 1);
      break;
    case 2:
      WiFiManager_Temp.toCharArray(password, WiFiManager_Temp.length() + 1);
      break;
  }
}
String WiFiManager_Get_Value(byte WiFiManager_ValueID, bool WiFiManager_Safe) {
#ifdef WiFiManager_SerialEnabled
  Serial.print("WM: Get value " + String(WiFiManager_ValueID));
#endif //WiFiManager_SerialEnabled
  String WiFiManager_Temp_Return = "";                //Make sure to return something, if we return bad data of NULL, the HTML page will break
  switch (WiFiManager_ValueID) {
    case 1:
      WiFiManager_Temp_Return += String(ssid);
      break;
    case 2:
      if (WiFiManager_Safe)                           //If's it's safe to return password.
        WiFiManager_Temp_Return += String(password);
      break;
  }
  Serial.println(" = " + WiFiManager_Temp_Return);
  return String(WiFiManager_Temp_Return);
}
//Some debug functions
//void WiFiManager_ClearEEPROM() {
//  EEPROM.write(0, 0);   //We just need to clear the first one, this will spare the EEPROM write cycles and would work fine
//  EEPROM.commit();
//}
//void WiFiManager_ClearMEM() {
//  ssid[0] = (char)0;                  //Clear these so we will enter AP mode (*just clearing first bit)
//  password[0] = (char)0;              //Clear these so we will enter AP mode
//}
//String WiFiManager_return_ssid() {
//  return ssid;
//}
//String WiFiManager_return_password() {
//  return password;
//}
