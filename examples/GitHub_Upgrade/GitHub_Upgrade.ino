
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

//general settings:
bool check_OTA_on_boot = false; // may be changed by user before compiling. if set to true, the device will check for an update on each boot.
/* Set up values for your repository and binary names */
#define GHOTA_USER "lademeister"
#define GHOTA_REPO "ESP_OTA_GitHub"
#define GHOTA_CURRENT_TAG "0.0.0" //change that to a current version number. if the version tag of your current github release is -for example- 0.2.3, 
//then your next version needs to set at least 0.2.4 here, and you need to make a new release on your GitHub repository with that versiontag (0.2.4) and add a compiled 
//binary of your new code as ASSET to that release (not, or not only to the repository itself! - the binary that will be flashed by OTA will be the one that you uploaded as asset fhile creating the new release)
#define GHOTA_BIN_FILE "GitHub_Upgrade.ino.generic.bin"
#define GHOTA_ACCEPT_PRERELEASE 0 //if set to 1 we will also update if a release of the github repository is set as 'prereelease'. Ignore prereleases if set to 0.
bool mqtt_config_hasbeenread = false; //do not change. used to know if we need to read the config data





// Number of seconds after reset during which a 
// subseqent reset will be considered a double reset.
#define DRD_TIMEOUT 10

// RTC Memory Address for the DoubleResetDetector to use
#define DRD_ADDRESS 0

DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS);


//Static WiFi SSID and password definitions
#ifndef STASSID
#define STASSID "dont_place_it_here"
#define STAPSK  "put_it_in_textfile_on_SPIFFS_instead"
#endif
#define wm_accesspointname "ESP8266 OTA-GitHub-Upgrade"
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


  the "mqtt_config.txt" file contains MQTT topics and variables that are used in the Arduino sketch to connect to the MQTT broker. The contents of the file specify the MQTT client ID, topics for device name, firmware version, online/offline status, and updates. These topics are used by the sketch to publish messages and subscribe to topics on the MQTT broker.
  The code reads each line of the file, trims the line, and checks if it is a comment or empty. If the line contains a variable name and topic separated by an equals sign, the sketch finds the matching MQTT variable and sets the topic to the corresponding value.
  For example, the line "mqttDevicenameTopic=MyDevice/devicename/" sets the "mqttDevicenameTopic" variable to "MyDevice/devicename/". You should change 'MyDEvice' to something unique.
  The sketch uses this variable to subscribe to the "devicename" topic on the public MQTT broker broker.hivemq.com.
  in summary, the "mqtt_config.txt" file allows you to customize the MQTT topics and variables used by the sketch without having to modify the code, and to keep those adaptions privately on the device.
*/
//the following variables will be automatically changed, if you have uploaded a configuration file named mqtt_config.txt to your ESP8266's SPIFFS memory.
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

// MQTT topic variables efinition. Essentially, mqttVars is an array of pointers to the MQTT topics that will be read from the mqtt_config.txt file and assigned to the respective MQTT variables.
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
  drd.loop();
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
    Serial.println(F("No certificates found. Did you run certs-from-mozilla.py on your computer and upload the file 'certs.ar' to the SPIFFS memory section on ESP8266 before flashing this sketch to the device?"));

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
      mqttClient.publish(mqttWillTopic, "Online", true);
      mqttClient.publish(mqttfirmwareTopic, GHOTA_CURRENT_TAG, true);
      mqttClient.subscribe(mqttupdateTopic);
      Serial.print("subscribed to topic '");
      Serial.print(mqttupdateTopic);
      Serial.print("' on ");
      Serial.print(mqttServer);
      Serial.println(". Publish the message 'update' to that topic, to manually");
      Serial.println("start an OTA firmware update from GitHub repository.");
      Serial.println("Example syntax for MacOS / Linux shell:");
      Serial.println("mosquitto_pub -h broker.hivemq.com -p 1883 -t 'MyDevice/update/' -m 'update'");
    }
    else {
      Serial.println(F("ERROR: MQTT connection not successful."));
    }
    Serial.println();
    //ALTERNATIVE, BETTER, USING WILLTOPIC (doesn't work with this pubsub library):
    //mqttClient.setWill(mqttWillTopic, "Offline");
    //mqttClient.publish(mqttWillTopic, "Online", true);


    //++++++++++++++++++++++
  }
  else {
    Serial.println(F("skipping MQTT connection due to missing Internet connectivity."));
  }
  lastReconnectAttempt = 0;
  //check_ota_github();
  Serial.println("Setup finished. going to loop() now");
  }
}

void setup_mqtt_config() {
  read_mqtt_topics_from_configfile();
  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setCallback(mqttCallback);
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
  Serial.println("Message received: " + message);

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

//void reconnect2() {
//  Serial.println("Connecting to MQTT Broker...");
//  setupMQTT();
//  while (!mqttClient.connected()) {
//      Serial.println("Reconnecting to MQTT Broker..");
//
//      if (mqttClient.connect(mqttClientId)) {
//        Serial.println("Connected.");
//        mqttClient.publish(mqttDevicenameTopic, mqttClientId);
//        mqttClient.publish(mqttonlineTopic, " online");
//        mqttClient.publish(mqttfirmwareTopic, "0.0.1", true);
//        mqttClient.subscribe(mqttupdateTopic);
//      }
//
//  }
//}


//void read_known_wifis1(){
//     // Open the known wifi text file from text file on SPIFFS for reading
//  File file = SPIFFS.open("/known_wifis.txt", "r");
//
//  if (file) {
//    // Loop through each line in the file
//    while (file.available()) {
//      String line = file.readStringUntil('\n');
//
////      // Extract the content before and after the first space character
////      int value = line.substring(0, line.indexOf(' ')).toInt();
////      String text = line.substring(line.indexOf(' ') + 1);
//
//      // Extract the content before and after the first '/' character (which is used to separate SSID and password within one line in the text file
//      SSID_txt = line.substring(0, line.indexOf('/'));
//      PASS_txt = line.substring(line.indexOf('/') + 1);
//
//      Serial.println("known_wifis.txt was found in SPIFFS memory on ESP8266.");
//
//      Serial.print("old WiFi SSID from sketch: ");
//      Serial.println(STASSID);
//      Serial.print("old WiFi Password from sketch: ");
//      Serial.println(STAPSK);
//
//      //setting the wifi credentials to the ones that we just read from the text file to use them in our sketch:
//      #undef STASSID
//      #define STASSID SSID_txt
//      #undef STAPSK
//      #define STAPSK PASS_txt
//      Serial.println("We will be using the credentials from the text file instead of the ones from the arduino sketch.");
//      Serial.println();
//      Serial.println("using WiFi credentials from known_wifis.txt in SPIFFS memory on ESP8266:");
//      Serial.print("(new) WiFi SSID: ");
//      Serial.println(STASSID);
//      Serial.print("(new) WiFi Password: ");
//      Serial.println(STAPSK);
//    }
//
//    // Close the file
//    file.close();
//  }
//  else{
//    Serial.println("file 'known_wifis.txt' was not found on SPIFFS memory of ESP8266, not not using any presets for wifi credentials that are set outside this sketch. Using sketch credentials instead:");
//    Serial.print("WiFi SSID from sketch: ");
//    Serial.println(STASSID);
//    Serial.print("WiFi Password from sketch: ");
//    Serial.println(STAPSK);
//  }
//}



void loop () {


  drd.loop(); //double reset detector
  //  OFF FOR DEBUG
  if (WiFi.status() == WL_CONNECTED) { //if connected to wifi
    //  if (!mqttClient.connected()) {
    //    while (!mqttClient.connect(mqttClientId)) {
    //      delay(1000);
    //    }
    //    mqttClient.publish(mqttonlineTopic, "Online", true);
    //  }
    //
    //
    //  // Process MQTT messages
    //  mqttClient.loop();
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
    //check_ota_github(); //call that upon request
  }

}
