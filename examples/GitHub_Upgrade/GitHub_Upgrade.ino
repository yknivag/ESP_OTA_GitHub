#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <time.h>
#include <FS.h>

#ifndef APSSID
#define APSSID "ssid"
#define APPSK  "password"
#endif



ESP8266WiFiMulti WiFiMulti;

// A single, global CertStore which can be used by all
// connections.  Needs to stay live the entire time any of
// the WiFiClientBearSSLs are present.
#include <CertStoreBearSSL.h>
BearSSL::CertStore certStore;

// Set time via NTP, as required for x.509 validation
void setClock() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");  // UTC

  Serial.print(F("Waiting for NTP time sync: "));
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    yield();
    delay(500);
    Serial.print(F("."));
    now = time(nullptr);
  }

  Serial.println(F(""));
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print(F("Current time: "));
  Serial.print(asctime(&timeinfo));
}

/* Set up values for your repository and binary names */
#define GHOTA_USER "GitHub_username"
#define GHOTA_REPO "GitHub_repository name"
#define GHOTA_CURRENT_TAG "0.0.0"
#define GHOTA_BIN_FILE "temp.ino.d1_mini.bin"
#define GHOTA_ACCEPT_PRERELEASE 0

#include <ESP_OTA_GitHub.h>
// Initialise Update Code
ESPOTAGitHub ESPOTAGitHub(&certStore, GHOTA_USER, GHOTA_REPO, GHOTA_CURRENT_TAG, GHOTA_BIN_FILE, GHOTA_ACCEPT_PRERELEASE);

void setup() {

  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(APSSID, APPSK);

  SPIFFS.begin();

  int numCerts = certStore.initCertStore(SPIFFS, PSTR("/certs.idx"), PSTR("/certs.ar"));
  Serial.print(F("Number of CA certs read: "));
  Serial.println(numCerts);
  if (numCerts == 0) {
    Serial.println(F("No certs found. Did you run certs-from-mozill.py and upload the SPIFFS directory before running?"));
    return; // Can't connect to anything w/o certs!
  }

  if ((WiFiMulti.run() == WL_CONNECTED)) {
    setClock(); // Clock needs to be set to perform certificate checks

    /* This is the actual code to check and upgrade */
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
  }
  
  // Your setup code goes here

}

void loop () {
  // Your loop code goes here

}
