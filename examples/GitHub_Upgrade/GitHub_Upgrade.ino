#include <ESP8266WiFi.h>
#include <FS.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#ifndef STASSID
#define STASSID "IdoNOTwantToShowMySSIDtoApublicGitHibRepository"
#define STAPSK  "mySSIDandPasswordAreStoredOnAtextfile_locally_on_ESP8266"
#endif

//variables to read a wifi SSID and wifi password from a text file (named known_wifis.txt) which is stored locally on your ESP8266, so that the publicly visible code on the GitHub repository doesn't contain your wifi credentials
String SSID_txt = "";
String PASS_txt = "";

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
  Serial.print(F("Number of CA certs read: "));
  Serial.println(numCerts);
  if (numCerts == 0) {
    Serial.println(F("No certs found. Did you run certs-from-mozilla.py on your computer and upload the file 'certs.ar' to the SPIFFS memory section on ESP8266 before flashing this sketch to the device?"));
    
    return; // Can't connect to anything w/o certs!
  }

  Serial.print("We use OTA update from a public GitHub repository. As we do not want to upload sketches that contain Wifi credentials, \nwe instead upload a file 'known_wifis.txt' to the SPIFFS memory section\non ESP8266 and read our credentials from there, so no one can see them in the git repositiry.");

   // Open the known wifi text file from text file on SPIFFS for reading
  File file = SPIFFS.open("/known_wifis.txt", "r");
  
  if (file) {
    // Loop through each line in the file
    while (file.available()) {
      String line = file.readStringUntil('\n');
      
//      // Extract the content before and after the first space character
//      int value = line.substring(0, line.indexOf(' ')).toInt();
//      String text = line.substring(line.indexOf(' ') + 1);

      // Extract the content before and after the first '/' character (which is used to separate SSID and password within one line in the text file
      SSID_txt = line.substring(0, line.indexOf('/'));
      PASS_txt = line.substring(line.indexOf('/') + 1);
      
      // Print the extracted values
      Serial.println("Reading WiFi credentials from textfile in SPIFFS memory on ESP8266:");
      Serial.print("SSID: ");
      Serial.println(SSID_txt);
      Serial.print("Password: ");
      Serial.println(PASS_txt);

      Serial.print("old WiFi SSID from sketch: ");
      Serial.println(STASSID);
      Serial.print("old WiFi Password from sketch: ");
      Serial.println(STAPSK);
      
      //setting the wifi credentials to the ones that we just read from the text file to use them in our sketch:
      #undef STASSID
      #define STASSID SSID_txt
      #undef STAPSK
      #define STAPSK PASS_txt

      Serial.print("new set WiFi SSID: ");
      Serial.println(STASSID);
      Serial.print("new set WiFi Password: ");
      Serial.println(STAPSK);
    }
    
    // Close the file
    file.close();
  }
  else{
    Serial.println("file 'known_wifis.txt' was not found on SPIFFS memory of ESP8266, not not using any presets for wifi credentials that are set outside this sketch. Using sketch credentials instead.");
  }


  // Connect to WiFi
  Serial.print("Connecting to WiFi... ");
  WiFi.mode(WIFI_STA);
  WiFi.begin(STASSID, STAPSK);
  if ((WiFi.status() != WL_CONNECTED)) {
    Serial.print("... ");
  }
  Serial.print("..Connected.");
  Serial.println();

    /* This is the actual code to check and upgrade */
    Serial.println("Checking if an OTA update is available on GitHub...");
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
    Serial.println(ESPOTAGitHub.getLastError());
    }
    /* End of check and upgrade code */

  // Your setup code goes here

}

void loop () {
  // Your loop code goes here

} 
