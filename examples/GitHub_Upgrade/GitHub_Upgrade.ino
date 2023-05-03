
/* Short description: This sketch for ESP8266 updates itself if a newer version is available in a GitHub repository.
    preset wifi credentials can be put to a textfile in SPIFFS memory of ESP8266. If none of them works, WiFiManager will be started and open an acces point for setup.
    Attention: to upload files to SPIFFS, serial monitor must be closed first.
    To start the OTA update from github:
    - it's possible to enter a command like mosquitto_pub -h broker.hivemq.com -p 1883 -t 'MyDevice/update/' -m 'update' to start the update manually
    - it's possible to check for an update on device start
    - it's possible to start an update based on a time schedule, e.g. once a day (to be implemented)

    WiFi SSID and Password - if you want to upload the sketch to a public GitHub repository, you might not want to put personal data in that sketch
   --> Therefore you can use the 'known_wifis.txt'-file and put your credentials there instead (even more than one wifi).
   You then upload the file as a configuration file for your ESP8266 to its SPIFFS memory (using for example an Arduino IDE SPIFFS uploader plugin like https://github.com/esp8266/arduino-esp8266fs-plugin).
   The sketch will read the file, extract Wifi(s) and password(s) and try to connect to them. If it fails, the sketch starts WiFiManager.
   You then can select one of the available netorks and enter the password for it by connecting to the wifi acces point that the wifimanager opens on your ESP8266.
   The Wifi credentials will be saved on the device as well.
   
   A double reset of ESP8266 will reset wifi credentials. You may need to triple-reset, depending on the speed of pressing reset to detect it correctly.
*/
#include <ESP8266WiFi.h>
#include <FS.h>
//#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <DoubleResetDetector.h>
#include <ArduinoOTA.h>

//general settings:
bool check_OTA_on_boot = false; // may be changed by user before compiling. if set to true, the device will check for an update on each boot.
/* Set up values for your repository and binary names */
//assuming you use this example, www.github.com/lademeister/ESP_OTA_GitHub is the repository we are using to fetch our OTA updates from:
#define GHOTA_USER "lademeister" //the name of the GitHub user that owns the repository you want to update from
#define GHOTA_REPO "ESP_OTA_GitHub" //the name of the repository you want to update from
#define GHOTA_CURRENT_TAG "0.2.4" //THis resembles the current version of THIS sketch.
//If that version number matches the latest release on GitHub, the device will not refleash with the binary (additionally uploaded as an ASSET of the release) from Github.
// in case of mismatch, the device will upgrade/downgrade, as soon as the check for new firmware is running.

//Explanation: you need to CHANGE THAT in case you create new software. Lets say latest release on GitHub was 0.2.3 and you modify something that you want to be the latest shit.
//you then set the above line as #define GHOTA_CURRENT_TAG "0.2.4" (or 0.3.0 for major changes) for example.
//to get that latest software you just created onto your device, you could just flash it locally via USB and you are fine.
//but to make it available as OTA binary on github, you should:

//1. use Sketch/export compiled binary to create an already compiled .bin file. (-->You should keep the name the same all times unless you understand the comments below)
//2. upload the latest .ino to your github repository (thats only for reference and for others to work/improve it)
//3. draft a new release of that github repository (using web interface on github.com) and set a tag for that release that matches your new version number (0.2.4 as in the example above)
//4. AND, most important, you NEED to upload the binary for that version as an ASSET to that release (you will find a square field where you can drag&drop the .bin when using the GitHub web interface).

//COMMENTS:
//it is important to understand that you do NOT need to upload the .bin to the actual repository next to the other files in the repository (like the .ino or so).
//You could do that without harm, but while you are free to upload ten or hundred binaries to your repository, none of them will be used for the OTA update.
//Only the one binary that was uploaded separately as an asset of the latest release of the repository will be used.
//You need to create releases manually, they are NOT created by uploading something.
//The release that you create must have a correct tag (=version numbering) - you can set that manually while drafting a release.
//The release that you create must have at least the .bin file for that current software version uploaded as an asset.
//The GHOTA_CURRENT_TAG while compiling that binary must be the SAME as the tag that you set for that release.
//Note that you also need to set the release as "latest" and that the binary that you compiled fromn your latest code must have the same name that 
//the firmware which is running on the OTA devices out in the field expects. This name is stored in #define GHOTA_BIN_FILE "GitHub_Upgrade.ino.generic.bin", so the 
//binary you upload as an assed for a new release must be named GitHub_Upgrade.ino.generic.bin to successfully update.
//Of yourse, you can change that at the beginning of your project to whatever the generated bin file is named as a standard.
//Lets assume you clone this repository and do your own work and name it 'MyProject'.
//When you go to Sketch/export compiled binary, depending if you compile for Generic ESP8266 or for Wemos D1 mini or something else, the output name will be different.
//It could -for example - be named MyProject.D1mini.bin or so.
//Then you would do yourself a favour in just changing the vaerriable in the sketch to #define GHOTA_BIN_FILE "MyProject.D1mini.bin".
//When cecking for an OTA update, the code will 
//-check the version tag: it compares GHOTA_CURRENT_TAG of the currently running software with the tag that you set for the latest release on github.
//-check the binary name: it compares GHOTA_BIN_FILE with the name of the binary that was uploaded as an asset on github
//-check the MIME type: nothing you need to care about as long as it works (if you need to adapt look at ESP_OTA_GitHub.h and ESP_OTA_GitHub.cpp file in /src folder of this repository. If you need to edit for other MIME types, then you need to edit the files in the forlder where you imported this library (most likely Arduino/libraries/ESP_OTA_GitHub/src)
//-check if the release was set as prerelease and compare if accepting a prerelease is allowed in the sketch currently running on the device. #define GHOTA_ACCEPT_PRERELEASE 0 would not allow to update from a release marked as prerelease.
//only if all requirements are met, an OTA update will be started.
//So, if you have devices out in the field that expect a binary named aaa.bin and in a later version that you create you change GHOTA_BIN_FILE to bbb.bin, you are fine if you update all devices first.
//But if you upload a file named bbb.bin as an asset to a new release while the code on the devies still expects aaa.bin, the update will fail.
//As there is no issue in keeping the same name unchanged forever, you should only change it at the beginning or if needed for some special reason.
//As the version control is done with the tag number of the GitHub release and the GHOTA_CURRENT_TAG in the code, there is no need for versioning file names.

// ### ATTENTION ###: Understand how it works before you use a different name for the .bin that you upload to GitHub or before you change #define GHOTA_BIN_FILE "GitHub_Upgrade.ino.generic.bin".

//additional remark: for testing you may want to flash a device locally that will definitely be OTA updated right away. To do so, just use #define GHOTA_CURRENT_TAG "0.0.0" for the 
//sketch that you upload to your test device while the GitHub repository at least has a release of 0.0.1. 
//In this way your test device will update even if you were flashing it with the most recent code (because it thinks it is 0.0.0).
//That can be handy for testing because you do not need to create new releases each time.

#define GHOTA_BIN_FILE "GitHub_Upgrade.ino.generic.bin" //only change it if you understand the above comments
#define GHOTA_ACCEPT_PRERELEASE 0 //if set to 1 we will also update if a release of the github repository is set as 'prereelease'. Ignore prereleases if set to 0.


#define DRD_TIMEOUT 10 // Number of seconds after reset during which a subseqent reset will be considered a double reset.

// RTC Memory Address for the DoubleResetDetector to use
#define DRD_ADDRESS 0

DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS);


bool mqtt_config_hasbeenread = false; //do not change. used to know if we need to read the config data
// Publish device ID and firmware version to MQTT topic
char device_id[9]; // 8 character hex device ID + null terminator


      



//Static WiFi SSID and password definitions
#ifndef STASSID
#define STASSID "dont_place_it_here"
#define STAPSK  "put_it_in_textfile_on_SPIFFS_instead"
#endif
#define wm_accesspointname "ESP8266 OTA-GitHub-Upgrade" //SSID when device opens up an access point for configuration
const int wm_accesspointtimeout = 30;
const int wifitimeout = 6; //in SECONDS.

//initial MQTT setup: if a file named mqtt_config.txt is uploaded to spiffs that contains the information in the form
/*
  mqttClientId:MyDeviceName
  mqttDevicenameTopic:MyDeviceName/devicename/
  mqttupdateTopic:MyDeviceName/update/
  mqttOnlineTopic:MyDeviceName/status/
  mqttFirmwareTopic:MyDeviceName/firmwareversion/
  mqttWillTopic:MyDeviceName/online/


  The "mqtt_config.txt" file contains MQTT topics and variables that are used in the Arduino sketch to connect to the MQTT broker. The contents of the file specify the MQTT client ID, topics for device name, firmware version, online/offline status, and updates. These topics are used by the sketch to publish messages and subscribe to topics on the MQTT broker.
  The code reads each line of the file, trims the line, and checks if it is a comment or empty. If the line contains a variable name and topic separated by an equals sign, the sketch finds the matching MQTT variable and sets the topic to the corresponding value.
  For example, the line "mqttDevicenameTopic=MyDevice/devicename/" sets the "mqttDevicenameTopic" variable to "MyDevice/devicename/". You should change 'MyDevice' to something unique.
  The sketch uses this variable to subscribe to the "devicename" topic on the public MQTT broker broker.hivemq.com.
  in summary, the "mqtt_config.txt" file allows you to customize the MQTT topics and variables used by the sketch without having to modify the code, and to keep those adaptions privately on the device.
*/
//the following variables will be automatically changed, if you have uploaded a configuration file named mqtt_config.txt to your ESP8266's SPIFFS memory.
//You do NOT need to change them here, so that your actual topics can be kept in the mqtt_config.txt on the device, instead of uploading it to a public GitHub repository.
char* mqttClientId = "MyDevice";
char* mqttDevicenameTopic = "MyDevice/devicename/";
char* mqttupdateTopic = "MyDevice/update/";
char* mqttonlineTopic = "MyDevice/status/";
char* mqttfirmwareTopic = "MyDevice/firmwareversion/";
char* mqttWillTopic = "MyDevice/online/";
char *mqttServer = "broker.hivemq.com";
char *mqttPort_txt = "1883"; //char variable to read from configuration text file mqtt_config.txt

int mqttPort = 1883;//integer variable that will be overwritten with the content of *mqttPort_txt, if it can be read from mqtt config file.

//otherMQTT definitions (they are set in sketch as they dont contain private data. Therefore its not critical to upload this code publicly on github).

int maximum_mqtt_connection_tries = 5;
const char* mqttWillMessage = "Offline";
byte mqttwillQoS = 0;
boolean mqttwillRetain = false;
boolean mqttcleanSession = false;



// Valid variable names for MQTT topics in mqtt_config.txt config file in SPIFFS memory of ESP8266
const char* validVarNames[] = {
  "mqttClientId",
  "mqttDevicenameTopic",
  "mqttupdateTopic",
  "mqttonlineTopic",
  "mqttfirmwareTopic",
  "mqttWillTopic",
  "mqttServer",
  "mqttPort_txt"
};

// MQTT topic variables definition. Essentially, mqttVars is an array of pointers to the MQTT topics that will be read from the mqtt_config.txt file and assigned to the respective MQTT variables.
const size_t numValidVarNames = sizeof(validVarNames) / sizeof(validVarNames[0]);
char** mqttVars[numValidVarNames] = {
  &mqttClientId,
  &mqttDevicenameTopic,
  &mqttupdateTopic,
  &mqttonlineTopic,
  &mqttfirmwareTopic,
  &mqttWillTopic,
  &mqttServer,
  &mqttPort_txt
};


/*The function read_mqtt_topics_from_configfile() reads MQTT topics from a file named "mqtt_config.txt" stored in the SPIFFS memory of a device.
   The function opens the file and reads each line of the file, skipping comments and empty lines. The function then splits each line into a variable name and a topic using the equal sign (=)
   as a delimiter. The function searches for a matching variable name in an array of valid variable names and sets the corresponding MQTT variable to the topic. If the variable name is invalid,
   an error message is printed. After all topics are read and set, the function prints the set MQTT topics and information about the MQTT broker to which the device will connect.
   If the file cannot be opened, an error message is printed, instructing the user to create the file with the correct format.

*/
void read_mqtt_topics_from_configfile() {
  // Open mqtt_config.txt file for reading
  File MQTTconfigFile = SPIFFS.open("/mqtt_config.txt", "r");

  if (MQTTconfigFile) {
    Serial.println();
    Serial.println("reading mqtt_config.txt from SPIFFS memory.");
    // Read each line from the file
    while (MQTTconfigFile.available()) {
      String line = MQTTconfigFile.readStringUntil('\n');
      line.trim();

      // Skip comments and empty lines
      if (line.startsWith("#") || line.isEmpty()) {
        continue;
      }

      // Split the line into variable name and topic
      int equalsIndex = line.indexOf("=");
      if (equalsIndex == -1) {
        Serial.println("Error: Invalid format on line '" + line + "'. Skipping.");
        continue;
      }
      String varName = line.substring(0, equalsIndex);
      String topic = line.substring(equalsIndex + 1);

      // Find the matching MQTT variable and set the topic
      bool foundVar = false;
      for (int i = 0; i < numValidVarNames; i++) {
        if (varName == validVarNames[i]) {
          *mqttVars[i] = strdup(topic.c_str());
          foundVar = true;
          break;
        }
      }

      // Print an error message if the variable name is invalid
      if (!foundVar) {
        Serial.println();
        Serial.println("Error: Invalid variable name '" + varName + "' in config file. Skipping.");
        Serial.println();
      }
    }
    MQTTconfigFile.close();

    mqttPort = atoi(&*mqttPort_txt); //set the content of the char variable that was read from config file into the int variable for MQTT configuration



    Serial.println("The MQTT settings are now set to:");
    Serial.print("mqttClientId: ");
    Serial.println(mqttClientId);
    Serial.print("mqttDevicenameTopic: ");
    Serial.println(mqttDevicenameTopic);
    Serial.print("mqttupdateTopic: ");
    Serial.println(mqttupdateTopic);
    Serial.print("mqttonlineTopic: ");
    Serial.println(mqttonlineTopic);
    Serial.print("mqttfirmwareTopic: ");
    Serial.println(mqttfirmwareTopic);
    Serial.print("mqttWillTopic: ");
    Serial.println(mqttWillTopic);
    Serial.println();
    Serial.print("Client name: '");
    Serial.print(mqttClientId);
    Serial.print("', MQTT broker '");
    Serial.print(mqttServer);
    Serial.print("', at port ");
    Serial.print(mqttPort);
    Serial.println(".");
    Serial.println();
    mqtt_config_hasbeenread = true;

  } else {
    Serial.println(F("Failed to read mqtt_config.txt file from SPIFFS memory. To store individual MQTT topics for the device to connect to, create a text file named 'mqtt_config.txt', and add the MQTT topics in a form like this:"));
    Serial.println("topic1variable_in_sketch=MyTopic/subtopic1");
    Serial.println("topic2variable_in_sketch=MyTopic/subtopic2");
    Serial.println("topic3variable_in_sketch=MyTopic/subtopic3");
    Serial.println("you must not change the variable names before '=' - if you do, the serial output will display an according error message, as it checks for validity.");
    Serial.println();
  }
}


long lastReconnectAttempt = 0;

//WiFiClientSecure wifiClient; //does not work with pubsubclient and unauthorized hive.mq
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
WiFiManager wifiManager;

////variables to read a wifi SSID and wifi password from a text file (named known_wifis.txt) which is stored locally on your ESP8266, so that the publicly visible code on the GitHub repository doesn't contain your wifi credentials
//String SSID_txt = "";
//String PASS_txt = "";


// Define variables to hold the credentials
const int MAX_WIFI_NETWORKS = 10;
char* ssid[MAX_WIFI_NETWORKS];
char* pass[MAX_WIFI_NETWORKS];
int numWifiNetworks = 0;



unsigned long now = 0;
unsigned long boottime = millis();

// A single, global CertStore which can be used by all
// connections.  Needs to stay live the entire time any of
// the WiFiClientBearSSLs are present.
#include <CertStoreBearSSL.h>
BearSSL::CertStore certStore;


#include <ESP_OTA_GitHub.h>
// Initialise Update Code
ESPOTAGitHub ESPOTAGitHub(&certStore, GHOTA_USER, GHOTA_REPO, GHOTA_CURRENT_TAG, GHOTA_BIN_FILE, GHOTA_ACCEPT_PRERELEASE);


////char auth[] = "your_token";
//char* ssid[] = {""}; //list a necessary wifi networks
//char* pass[] = {""}; //list a passwords
//void readWifiCredentials(char* ssid[], char* pass[], int maxNetworks) {
//  // open the "known_wifis.txt" file from SPIFFS
//  File file = SPIFFS.open("/known_wifis.txt", "r");
//  if (!file) {
//    Serial.println("Failed to open file");
//    return;
//  }
//
//  // read each line from the file and extract SSID and password
//  int networkCount = 0;
//  while (file.available() && networkCount < maxNetworks) {
//    String line = file.readStringUntil('\n');
//    int separatorIndex = line.indexOf(':');
//    if (separatorIndex == -1) continue; // skip lines without separator
//    ssid[networkCount] = strdup(line.substring(0, separatorIndex).c_str());
//    pass[networkCount] = strdup(line.substring(separatorIndex + 1).c_str());
//    networkCount++;
//  }
//
//  // close the file
//  file.close();
//
//  // print the extracted SSIDs and passwords
//  Serial.println("Extracted Wi-Fi credentials:");
//  for (int i = 0; i < networkCount; i++) {
//    Serial.print(ssid[i]);
//    Serial.print(": ");
//    Serial.println(pass[i]);
//  }
//}



/*This function reads known Wi-Fi credentials from a text file named "known_wifis.txt" stored in the SPIFFS memory. If the file cannot be opened,
   the function prints out instructions on how to create the file and add Wi-Fi credentials. The function reads the file line by line, splits each line into an SSID and password and stores them
   in the 'ssid' and 'pass' arrays respectively. Each SSID and password string is dynamically allocated using 'new', and their pointers are stored in the arrays. The function also prints out the
   number of Wi-Fi networks found in the file and the SSID and password credentials for debugging purposes. The maximum number of Wi-Fi networks that can be stored is defined as 'MAX_WIFI_NETWORKS'.
*/

void read_known_wifi_credentials_from_configfile() {
  // Open the file 'known_wifis.txt' in SPIFFS memory for reading
  File file = SPIFFS.open("/known_wifis.txt", "r");
  if (!file) {
    Serial.println(F("Failed to open known_wifis.txt file. To store preset wifi credentials, create a text file named 'known_wifis.txt', and add the preset wifi credentials like this:"));
    Serial.println(F("ssid1:password1"));
    Serial.println(F("ssid2:password2"));
    Serial.println(F("ssid3:password3"));
    Serial.println("...up to MAX_WIFI_NETWORKS, which is currently set to a maximum number of " + MAX_WIFI_NETWORKS);
    Serial.println();
    Serial.println(F("then put this text file in the 'data' folder in the sketch directory, next to the 'certs.ar' file."));
    Serial.println(F("upload both files to ESP8266's SPIFFS memory (using some SPIFFS upload plugin for Arduino IDE or an SPIFFS upload sketch, before flashing this sketch again."));
    Serial.println(F("You then have both files in SPIFFS memory, which will be accessed by this sketch."));
    Serial.println(F("You can omit uploading the known_wifis.txt if you dont want the ESP8266 to initially try to connect to some preset wifi networks, and only use WiFiManager"));
    Serial.println(F("WiFiManager will be started if either no connection to a preset WiFi can be established, or if known_wifis.txt is not preset at all."));
    Serial.println();
    return;
  }

  // Read the file line by line
  while (file.available()) {
    String line = file.readStringUntil('\n');

    // Split the line into ssid and password
    int splitIndex = line.indexOf(':');
    if (splitIndex == -1) {
      continue;
    }
    String ssidStr = line.substring(0, splitIndex);
    String passStr = line.substring(splitIndex + 1);
    ssid[numWifiNetworks] = new char[ssidStr.length() + 1];
    strcpy(ssid[numWifiNetworks], ssidStr.c_str());
    pass[numWifiNetworks] = new char[passStr.length() + 1];
    strcpy(pass[numWifiNetworks], passStr.c_str());
    numWifiNetworks++;

    if (numWifiNetworks >= MAX_WIFI_NETWORKS) {
      break;
    }
  }

  // Close the file
  file.close();

  if (numWifiNetworks > 0) {
    Serial.print("Found ");
    Serial.print(numWifiNetworks);
    Serial.println(F(" preset WiFi networks in 'known_wifis.txt' on ESP8266 SPIFFS memory:"));
    Serial.println();
  }
  // Print the credentials for debugging purposes
  for (int i = 0; i < numWifiNetworks; i++) {
    Serial.printf("Wifi %d: %s, %s\n", i, ssid[i], pass[i]);
  }
}


/*MultiWiFiCheck that attempts to connect to multiple WiFi networks in sequence until it successfully connects to one.
   It first calculates the number of WiFi networks stored in the ssid array and then tries to connect to each network in turn.
*/
//void MultiWiFiCheck() {
//  int ssid_count=0;
//  int ssid_mas_size = sizeof(ssid) / sizeof(ssid[0]);
//  do {
//    Serial.println();
//    Serial.print("Trying to connect to WiFi '" + String(ssid[ssid_count]));
//    Serial.print(F("' with a timeout of "));
//    Serial.print(wifitimeout);
//    Serial.print(F(" seconds.  "));
//    WiFi.begin(ssid[ssid_count], pass[ssid_count]);
//    int WiFi_timeout_count=0;
//    while (WiFi.status() != WL_CONNECTED && WiFi_timeout_count<(wifitimeout*10)) { //waiting wifitimeout seconds (*10 is used to get more dots "." printed while trying)
//      delay(100);
//      Serial.print(".");
//      ++WiFi_timeout_count;
//    }
//    if (WiFi.status() == WL_CONNECTED) {
//      Serial.println();
//      Serial.println(F("Connected to WiFi!"));
//      //connected_to_preset_wifi=true;
//      Serial.println();
//      return;
//    }
//    Serial.println();
//    ++ssid_count;
//  }
//  //while (ssid_count<ssid_mas_size);
//  while (ssid_count<numWifiNetworks);
//
//
//}

void MultiWiFiCheck() {

  int numFoundNetworks = WiFi.scanNetworks();
  int bestSignalStrength = -1000;  // Start with a very weak signal strength
  int bestNetworkIndex = -1;  // Index of the network with the best signal strength

  // Find the network with the strongest signal strength
  for (int i = 0; i < numWifiNetworks; i++) {
    for (int j = 0; j < numFoundNetworks; j++) {
      if (WiFi.SSID(j) == String(ssid[i])) {
        int32_t rssi = WiFi.RSSI(j);  // Get the signal strength of the current network
        if (rssi > bestSignalStrength) {
          bestSignalStrength = rssi;
          bestNetworkIndex = j;
        }
        break;
      }
    }
    // Output the strongest network's SSID and signal strength to the serial monitor
  }

  if (bestNetworkIndex == -1) {
    // None of the preset networks were found
    Serial.println(F("None of the preset WiFi networks were found."));
    return;
  }
  else {
    if (bestNetworkIndex >= 0) {
      Serial.println();
      Serial.print("Strongest (known) network from .txt file: ");
      Serial.print(WiFi.SSID(bestNetworkIndex));
      Serial.print(", signal strength: ");
      Serial.println(bestSignalStrength);
    }
  }

  // Connect to the network with the strongest signal strength
  String selectedSSID = WiFi.SSID(bestNetworkIndex);
  String selectedPassword = pass[0];

  for (int i = 0; i < numWifiNetworks; i++) {
    if (selectedSSID == String(ssid[i])) {
      selectedPassword = pass[i];
      break;
    }
  }

  Serial.print(F("Connecting to "));
  Serial.print(selectedSSID);
  Serial.println(F("..."));
  WiFi.begin(selectedSSID.c_str(), selectedPassword.c_str());

  int WiFi_timeout_count = 0;
  while (WiFi.status() != WL_CONNECTED && WiFi_timeout_count < (wifitimeout * 10)) {
    delay(100);
    Serial.print(".");
    ++WiFi_timeout_count;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println(F("Connected to WiFi!"));
  } else {
    Serial.println();
    Serial.println(F("Failed to connect to WiFi."));
  }
}



void setup() {
  if (drd.detectDoubleReset()) {
    Serial.println("======================== Double Reset Detected ========================");
    digitalWrite(LED_BUILTIN, LOW);
    delay(1000);
      wifiManager.resetSettings();
      delay(100);
      ESP.restart();
  }
  else{
  // Start serial for debugging (not used by library, just this sketch).
  Serial.begin(115200);
  Serial.println();
  Serial.println();
  Serial.println();
  drd.loop();
  // generate device ID
  uint32_t chip_id = ESP.getChipId();
  sprintf(device_id, "%08X", chip_id);
  
  Serial.println();
  Serial.println();
  Serial.println("================================================================================");
  Serial.println("|                                                                              |");
  Serial.println("|                  Welcome to the ESP GitHub OTA Update TEST                   |");
  Serial.println("|                =============================================                 |");
  Serial.print("|    Version:    ");
  Serial.print(GHOTA_CURRENT_TAG);
  Serial.println("                                                         |");
  Serial.println("|                                                                              |");
  Serial.println("|                - double reset will reset WiFi credentials -                  |");
  Serial.println("|                     - you may need to triple reset -                         |");
  Serial.println("================================================================================");
  Serial.println();
  drd.loop();
  Serial.println();

  // Start SPIFFS and retrieve certificates.
  SPIFFS.begin();
  int numCerts = certStore.initCertStore(SPIFFS, PSTR("/certs.idx"), PSTR("/certs.ar"));
  Serial.println();
  Serial.println();
  Serial.print(F("Reading CA certificates: "));
  Serial.print(numCerts);
  Serial.println(F(" certificates read. "));
  if (numCerts == 0) {
    Serial.println();
    Serial.println();
    Serial.println("################################################################################################################################################################");
    Serial.println(F("No certificates found. Did you run certs-from-mozilla.py (contained in this repository) on your computer and upload the file 'certs.ar' to the SPIFFS memory section on ESP8266 before flashing this sketch to the device?"));
    Serial.println(F("To upload files to ESP8266's SPIFFS memory I suggest this plugin for Arduino IDE: https://github.com/esp8266/arduino-esp8266fs-plugin, "));
    Serial.println(F("which will create an entry called 'ESP8266 sketch data upload' in the tools menu. One press uploads all files in the 'data' folder of the sketch."));
    Serial.println(F("All files that you want to upload to SPIFFS memory section must be in the 'data' folder."));
    Serial.println(F("The 'data' folder can be the one in the Arduino/libraries/ESP_OTA_GitHub/examples/data folder if you are using the example code, or can be in your local code area where you store your projects if you have saved already.\n There it would most likely be found in ESP_OTA_GitHub/examples/data"));
    Serial.println(F("HINT: If sketch data upload doesn't work, make sure that serial monitor window is closed."));
    Serial.println(F("At the end, you want to upload certs.ar, mqtt_config.txt and known_wifis.txt to SPIFFS memory of your ESP8266."));
    Serial.println(F("Important: if compiling for generic ESP8266, set Tools/erase flash to 'all flash contents' to also delete SPIFFS contents. This is NOT necessary for overwriting files."));
    Serial.println(F("Usually you will set that to 'sketch only'."));
    return; // Can't connect to anything w/o certs!
  }


  //readWifiCredentials();
  if (WiFi.SSID() == "") {
    // WiFiManager was not previously connected to a network
    Serial.println(F("It seems that WiFiManager was not previously connected to a network"));
    // Perform setup for first-time use
    read_known_wifi_credentials_from_configfile();
    MultiWiFiCheck();
    // read_mqtt_topics_from_configfile();



    if (WiFi.status() != WL_CONNECTED) { //only start wifi manager if we do not yet have a wifi connection from preset wifis
      Serial.println();
      Serial.println(F("It was not possible to connect to one of the known WiFi's."));
      Serial.println();
      Serial.print(F("Starting WiFiManager now. Please connect to the access Point '"));
      Serial.print(wm_accesspointname);
      Serial.print(F("' within "));
      Serial.print(wm_accesspointtimeout);
      Serial.println(F("s timeout."));
      Serial.println();
      Serial.println();
      Serial.println();
    }
  } else {
    // WiFiManager was previously connected to a network
    // Perform setup for subsequent use
  }


  //initialize WiFiManager
  if (WiFi.status() != WL_CONNECTED) { //only start wifi manager if we do not yet have a wifi connection from preset wifis
    //WiFiManager wifiManager;
    wifiManager.setTimeout(wifitimeout);



    //
    //   //Open known_wifis.txt file for reading
    //  File configFile = SPIFFS.open("/known_wifis.txt", "r");
    //  if (!configFile) {
    //    Serial.println("Failed to open known_wifis.txt file");
    //    return;
    //  }
    //
    //  //Read wifi credentials from known_wifis.txt and add to WiFiManager
    //  while (configFile.available()) {
    //    String line = configFile.readStringUntil('\n');
    //    int separatorIndex = line.indexOf('/');
    //    if (separatorIndex != -1) {
    //      String ssid = line.substring(0, separatorIndex);
    //      String password = line.substring(separatorIndex + 1);
    //      //wifiManager.addAPNode("ssid", "password");
    //
    //    }
    //  }
    //
    //  configFile.close();





    //set minimum quality of signal to be connected to wifi network
    //default is 8%, but you can set it higher for more reliability
    //wifiManager.setMinimumSignalQuality(20);

    //set config portal timeout
    wifiManager.setConfigPortalTimeout(30);

    //set custom parameters for the configuration portal
    //wifiManager.setAPCallback(yourCallbackFunction);
    //wifiManager.setAPStaticIPConfig(yourIPConfig);
    //wifiManager.setSTAStaticIPConfig(yourSTAIPConfig);


    wifiManager.autoConnect(wm_accesspointname);

    //  testing  to add wifis - doesnt compile with this version of wifimanager but there should be versions that accept that command.
    //  Serial.println("testing  to add wifis to wifimanager");
    //  wifiManager.addAP("SSID1", "password1"); //should work but doesn't work with this verison of wifimanager, so we try to connect manually to the networks saved in textfile and cannot use this here.
    //  wifiManager.addAP("SSID2", "password2");



    wifiManager.setTimeout(wifitimeout);
    //
    //  // Connect to WiFi
    //  Serial.print("Connecting to WiFi... ");
    //  WiFi.mode(WIFI_STA);
    //  WiFi.begin(STASSID, STAPSK);
    //  if ((WiFi.status() != WL_CONNECTED)) {
    //    Serial.print("... ");
    //  }
    Serial.println();
  }

  //  Serial.println("Waiting for WiFiMAnager to be stopped.");
  //// Wait until WiFiManager is unloaded
  //  while (!wifiManager.getWiFiIsSaved()) {
  //    delay(1000);
  //    Serial.println("Waiting for WiFiManager to unload...");
  //  }

  // Now that WiFiManager is unloaded, we can continue with the rest of the program
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected.");
    // Print the name of the WiFi network
    Serial.print("Connected to WiFi '");
    Serial.print(WiFi.SSID());
    Serial.println("'.");
    wifiManager.stopConfigPortal();
    Serial.println(F("Config portal stopped."));
    now = millis();
    if (now - boottime < 7000) {
      Serial.println(F("fresh boot."));
      delay(2000);
      Serial.println(F("...waiting a short time to establish and stabilize WiFi connection."));
      delay(2000);
    }
    if (check_OTA_on_boot) { //if flag is set, check for an update when starting
      check_ota_github();
    }
    /* End of check and upgrade code */
  } else {
    // code to be executed when there is no successful WiFi connection
    Serial.println(F("WiFi could not be connected: \n- no success to connect to preset WiFi's. \n- WifiManager timed out, or \n- Wifi credentials were not entered correctly. \n\nSkipping check for Firmware update due to missing internet connection.\nReboot to try again. \nNow proceeding with void loop()."));
  }



  if (WiFi.status() == WL_CONNECTED) { //only read mqtt config and set mqtt client if we are already connected to wifi
    Serial.println("setting up MQTT.");
    // Set up MQTT client
    setup_mqtt_config();

    //mqttClient.setCredentials(mqttUsername, mqttPassword);

    // Set up MQTT LWT
    //mqttClient.setWill(mqttWillTopic, mqttWillMessage);
    // Set the LWT message
    //  mqttClient.setWillTopic(mqttWillTopic);
    //  mqttClient.setWillMessage(mqttWillMessage);

    //++++++++++++++++++++++ OFF FOR DEBUG
    //  // Connect to MQTT broker
    //  mqttClient.setServer(mqttServer, mqttPort);
    //  mqttClient.setCallback(mqttCallback);

    int mqtt_connection_tries = 0;
    while (!mqttClient.connect(mqttClientId) && mqtt_connection_tries < maximum_mqtt_connection_tries) {

      //while (!mqttClient.connect(mqttClientId, mqttUsername, mqttPassword)) {
      //while (!mqttClient.connect(mqttClientId, mqttUsername, mqttPassword, mqttWillTopic, mqttwillQoS, mqttwillRetain, mqttWillMessage, mqttcleanSession)) {



      Serial.print("MQTT: trying to connect as ");
      Serial.print(mqttClientId);
      Serial.print(" to ");

      Serial.print(mqttServer);
      Serial.print(" at Port ");
      Serial.println(mqttPort);
      mqtt_connection_tries++;
      delay(1000);

    }
    mqtt_connection_tries = 0;
    if (mqttClient.connected()) {
      // Publish online message to MQTT broker
      mqttClient.publish(mqttWillTopic, "Online", true); //needs correct setup with 'will topic' to work correctly (see OpenMQTTGateway for example)
      publish_device_info();
      mqttClient.publish(mqttfirmwareTopic, GHOTA_CURRENT_TAG, true);
      Serial.print(F("Firmware version published to '"));
      Serial.print(mqttfirmwareTopic);
      Serial.println(F("."));
      mqttClient.subscribe(mqttupdateTopic);
      Serial.println();
      Serial.print(F("Device is now subscribed to topic '"));
      Serial.print(mqttupdateTopic);
      Serial.print("' on ");
      Serial.print(mqttServer);
      Serial.println(F(". Publish the message 'update' to that topic, to manually"));
      Serial.println(F("start an OTA firmware update from GitHub repository."));
      Serial.println(F("Working example syntax for MacOS / Linux shell for this device:"));
      Serial.print(F("mosquitto_pub -h broker.hivemq.com -p 1883 -t '"));
      Serial.print(mqttupdateTopic);
      Serial.println(F("' -m 'update'"));
    }
    else {
      Serial.println(F("ERROR: MQTT connection not successful."));
    }
    Serial.println();
    //ALTERNATIVE, BETTER, USING WILLTOPIC (doesn't work with this pubsub library):
    //mqttClient.setWill(mqttWillTopic, "Offline");
    //mqttClient.publish(mqttWillTopic, "Online", true);


    //++++++++++++++++++++++
    setup_local_OTA();
  }
  else {
    Serial.println(F("skipping MQTT connection due to missing Internet connectivity."));
  }
  lastReconnectAttempt = 0;

  Serial.println("Setup finished. going to loop() now");
  }
}

void setup_mqtt_config() {
  read_mqtt_topics_from_configfile();
  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setCallback(mqttCallback);
}

void setup_local_OTA(){
     ArduinoOTA.setHostname("MyDevice");

  ArduinoOTA.begin();
    ArduinoOTA.onStart([]() {
    //ota_target_position =0;
    //old_ota_target_position = 0;
//    ausfallschritt_servo.attach(SERVO_PIN);  // attaches the servo on GIO2 to the servo object
//    ausfallschritt_servo.write(zurück);
//    delay(100);
//    ausfallschritt_servo.detach();
//    display.clear();
//    display.setFont(ArialMT_Plain_16);
//    display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
//    display.drawString(display.getWidth() / 2, display.getHeight() / 2 - 10, "OTA Update");
//    display.display();
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    //display.drawProgressBar(4, 36, 120, 10, progress / (total / 100) );
    Serial.println("Progress: "+ progress);
    Serial.println("Progress/(total/100): "+ progress / (total / 100));
    //display.display();
    //use 'stufe der weisheit' as LED progress bar for OTA update:
//    int ota_percentage_done=progress / (total / 100);
//    target_leds(ota_percentage_done);

//    
//    init_stepper();
//    //int target_position = map(ota_percentage_done, 0, 100, 0, 200);
//    int target_position = map(progress, 0, 100, 0, 200);
//    // Convert position to a number of steps based on the steps per revolution
//    
//    ota_target_position=ota_target_position-old_ota_target_position;
//    
//    stepper.step(ota_target_position);
//
//    old_ota_target_position=ota_target_position;
//  
//    free_stepper();
  });

  ArduinoOTA.onEnd([]() {
    //ota_target_position =0;
    //old_ota_target_position = 0;
//    display.clear();
//    display.setFont(ArialMT_Plain_16);
//    display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
//    display.drawString(display.getWidth() / 2, display.getHeight() / 2, "rebooting...");
//    display.display();
  });
}

void check_ota_github() {
  // code to be executed when there is a successful WiFi connection

  /* This is the actual code to check and upgrade */
  Serial.println(F("Checking if an OTA update is available on GitHub..."));
  if (ESPOTAGitHub.checkUpgrade()) {
    Serial.print("Upgrade found at: ");
    Serial.println(ESPOTAGitHub.getUpgradeURL());
    if (ESPOTAGitHub.doUpgrade()) {
      Serial.println("Upgrade complete."); //This should never be seen as the device should restart on successful upgrade.
    } else {
      Serial.print("Unable to upgrade: ");
      Serial.println(ESPOTAGitHub.getLastError());
    }
  } else {
    Serial.print("Not proceeding to upgrade: ");
    //Serial.println(ESPOTAGitHub.getLastError());


    String errorCode = ESPOTAGitHub.getLastError();
    if (errorCode == "Failed to parse JSON.") { //DEBUG: reboot here, as JSON parsing fails after freshly setting an AP with wifimanager.
      Serial.println(ESPOTAGitHub.getLastError());
      Serial.println("rebooting...");
      ESP.restart();
      delay(1000); //we do not arrive here because of reboot.
    } else {
      Serial.println(ESPOTAGitHub.getLastError());
    }
  }
}

//void mqttCallback(char* topic, byte* payload, unsigned int length) {
//  // Handle MQTT messages received
//}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println("MQTT Message received: " + message);

  if (strcmp(topic, mqttupdateTopic) == 0) {
    if (message == "update") {
      check_ota_github();
    }
  }
}

void setupMQTT() {
  mqttClient.setServer(mqttServer, mqttPort);
  // set the callback function
  mqttClient.setCallback(mqttCallback);
}

boolean reconnect() {
  setupMQTT();

  //++++++++++++ MQTT  DEBUG ONLY

  if (!mqttClient.connected()) {
    Serial.print("MQTT: trying to connect as ");
    Serial.print(mqttClientId);
    Serial.print(" to ");

    Serial.print(mqttServer);
    Serial.print(" at Port ");
    Serial.println(mqttPort);

    if (mqttClient.connect(mqttClientId)) {
      Serial.println("Connected.");
      mqttClient.publish(mqttDevicenameTopic, mqttClientId);
      mqttClient.publish(mqttonlineTopic, " online"); //should be set up with will message so that "offline" appears automatically set by the MQTT broker when the device is offline
      publish_device_info();
      mqttClient.publish(mqttfirmwareTopic, GHOTA_CURRENT_TAG, true); //true makes the message retained, so that you can see which firmware version the device had when it was last connected.


      mqttClient.subscribe(mqttupdateTopic);
    }

  }

  //++++++++++++++++++++++


  // // if (mqttClient.connect(mqttClientId, mqttUsername, mqttPassword)) {
  // //if (mqttClient.connect(mqttClientId, mqttUsername, mqttPassword, mqttWillTopic, mqttwillQoS, mqttwillRetain, mqttWillMessage)) {
  // if (mqttClient.connect(mqttClientId)) {
  //
  //
  //    // Once connected, publish an announcement...
  //    //mqttClient.publish("outTopic","MyDevice is back!");
  //    mqttClient.publish(mqttWillTopic, "Online", true);
  //    mqttClient.publish(mqttfirmwareTopic, GHOTA_CURRENT_TAG, true);
  //    // ... and resubscribe
  //    mqttClient.subscribe(mqttupdateTopic);
  //  }

  return mqttClient.connected();

}

void publish_device_info() {
  String message = "DeviceID ";
  message += device_id;
  message += " running firmware ";
  message += GHOTA_CURRENT_TAG;
  mqttClient.publish(mqttDevicenameTopic, message.c_str());
  Serial.print("Device info published to '");
  Serial.print(mqttDevicenameTopic);
  Serial.println("'.");
}

void loop () {


  drd.loop(); //double reset detector
  //  OFF FOR DEBUG
  if (WiFi.status() == WL_CONNECTED) { //if connected to wifi
    if (!mqttClient.connected()) {
      if (!mqtt_config_hasbeenread) { //if we have not yet read mqtt config, try to read it from config file
        setup_mqtt_config();
      }
      long now = millis();
      if (now - lastReconnectAttempt > 5000) {
        lastReconnectAttempt = millis();
        // Attempt to reconnect
        //Serial.println("DEBUG: reconnecting, should happen only each 5 seconds");
        if (reconnect()) {
          lastReconnectAttempt = 0;
        }
      }
    } else {
      // Client connected
      mqttClient.loop();
    }
  }

}
