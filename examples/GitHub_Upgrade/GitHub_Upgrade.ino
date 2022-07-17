/*
  GitHub_Upgrade.ino - Example for ESP library for auto updating code from GitHub releases.
  Created by Gavin Smalley, November 13th 2019.
  Released under the LGPL v2.1.
  It is the author's intention that this work may be used freely for private
  and commercial use so long as any changes/improvements are freely shared with
  the community under the same terms.
*/

#include <CertStoreBearSSL.h>
#include <ESP_OTA_GitHub.h>
#include <ESP8266WiFi.h>
#include <FS.h>

constexpr static const char *STASSID = "ssid";
constexpr static const char *STAPSK = "password";

// A single, global CertStore which can be used by all
// connections.  Needs to stay live the entire time any of
// the WiFiClientBearSSLs are present.
BearSSL::CertStore certStore;

/* Set up values for your repository and binary names */
constexpr static const char *GHOTA_USER = "username";
constexpr static const char *GHOTA_REPO = "repository";
constexpr static const char *GHOTA_CURRENT_TAG = "0.0.0";
constexpr static const char *GHOTA_BIN_FILE = "sketchname.ino.d1_mini.bin";
constexpr static const bool GHOTA_ACCEPT_PRERELEASE = false;

void handle_upgade()
{
    // Initialise Update Code
    // We do this locally so that the memory used is freed when the function exists.
    ESPOTAGitHub ESPOTAGitHub(&certStore, GHOTA_USER, GHOTA_REPO, GHOTA_CURRENT_TAG, GHOTA_BIN_FILE, GHOTA_ACCEPT_PRERELEASE);

    Serial.println("Checking for update...");
    if (ESPOTAGitHub.checkUpgrade())
    {
        Serial.print("Upgrade found at: ");
        Serial.println(ESPOTAGitHub.getUpgradeURL());
        if (ESPOTAGitHub.doUpgrade())
        {
            Serial.println("Upgrade complete."); // This should never be seen as the device should restart on successful upgrade.
        }
        else
        {
            Serial.print("Unable to upgrade: ");
            Serial.println(ESPOTAGitHub.getLastError());
        }
    }
    else
    {
        Serial.print("Not proceeding to upgrade: ");
        Serial.println(ESPOTAGitHub.getLastError());
    }
}

void setup()
{
    // Start serial for debugging (not used by library, just this sketch).
    Serial.begin(115200);

    // Start SPIFFS and retrieve certificates.
    SPIFFS.begin();
    int numCerts = certStore.initCertStore(SPIFFS, PSTR("/certs.idx"), PSTR("/certs.ar"));
    Serial.print(F("Number of CA certs read: "));
    Serial.println(numCerts);
    if (numCerts == 0)
    {
        Serial.println(F("No certs found. Did you run certs-from-mozill.py and upload the SPIFFS directory before running?"));
        return; // Can't connect to anything w/o certs!
    }

    // Connect to WiFi
    Serial.print("Connecting to WiFi... ");
    WiFi.mode(WIFI_STA);
    WiFi.begin(STASSID, STAPSK);
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(500);
    }
    Serial.println();

    /* This is the actual code to check and upgrade */
    handle_upgade();
    /* End of check and upgrade code */

    // Your setup code goes here
}

void loop()
{
    // Your loop code goes here
}