#include <ESP8266WiFi.h>
#include <FS.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

#ifndef STASSID
#define STASSID "IdoNOTwantToShowMySSIDtoApublicGitHibRepository"
#define STAPSK  "mySSIDandPasswordAreStoredOnAtextfile_locally_on_ESP8266"
#endif
#define wm_accesspointname "ESP8266 OTA-GitHub-Upgrade"
const int wm_accesspointtimeout = 30;
const int wifitimeout = 6; //in SECONDS.

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

/* Set up values for your repository and binary names */
#define GHOTA_USER "lademeister"
#define GHOTA_REPO "ESP_OTA_GitHub"
#define GHOTA_CURRENT_TAG "0.0.0"
#define GHOTA_BIN_FILE "GitHub_Upgrade.ino.generic.bin"

#define GHOTA_ACCEPT_PRERELEASE 0 //if set to 1 we will also update if a release of the github repository is set as 'prereelease'. Ignore prereleases if set to 0.

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

void readKnownWifiCredentials() {
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


//moved to top for global definition
//  // Define variables to hold the credentials
//  const int MAX_WIFI_NETWORKS = 10;
//  char* ssid[MAX_WIFI_NETWORKS];
//  char* pass[MAX_WIFI_NETWORKS];
//  int numWifiNetworks = 0;

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

  if(numWifiNetworks>0){
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

void MultWiFicheck() {
  int ssid_count=0;
  int ssid_mas_size = sizeof(ssid) / sizeof(ssid[0]);
  do {
    Serial.println();
    Serial.print("Trying to connect to WiFi '" + String(ssid[ssid_count]));
    Serial.print(F("' with a timeout of "));
    Serial.print(wifitimeout);
    Serial.print(F(" seconds.  "));
    WiFi.begin(ssid[ssid_count], pass[ssid_count]);    
    int WiFi_timeout_count=0;
    while (WiFi.status() != WL_CONNECTED && WiFi_timeout_count<(wifitimeout*10)) { //waiting wifitimeout seconds (*10 is used to get more dots "." printed while trying)
      delay(100);
      Serial.print(".");
      ++WiFi_timeout_count;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println();
      Serial.println(F("Connected to WiFi!"));
      //connected_to_preset_wifi=true;
      Serial.println();
      return;
    }
    Serial.println();
    ++ssid_count; 
  }
  //while (ssid_count<ssid_mas_size);
  while (ssid_count<numWifiNetworks);
  

}


void setup() {
  // Start serial for debugging (not used by library, just this sketch).
  Serial.begin(115200);
  Serial.println();
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
  Serial.println("================================================================================");
  Serial.println();
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
  readKnownWifiCredentials();
  MultWiFicheck();
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
//readKnownWifiCredentials();
//MultWiFicheck();
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
    // code to be executed when there is a successful WiFi connection
    Serial.println("WiFi connected.");
    wifiManager.stopConfigPortal();
    now=millis();
    if(now-boottime<5000){
      Serial.println(F("fresh boot."));
      delay(2000);
      Serial.println(F("...waiting a short time to establish and stabilize WiFi connection."));
      delay(2000);
    }
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
  /* End of check and upgrade code */
  } else {
    // code to be executed when there is no successful WiFi connection
    Serial.println(F("WiFi could not be connected: \n- no success to connect to preset WiFi's. \n- WifiManager timed out, or \n- Wifi credentials were not entered correctly. \n\nSkipping check for Firmware update due to missing internet connection.\nReboot to try again. \nNow proceeding with void loop()."));
  }



  // Your setup code goes here

}


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
  // Your loop code goes here

} 
