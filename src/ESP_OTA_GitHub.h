/*
  ESP_OTA_GitHub.h - ESP library for auto updating code from GitHub releases.
  Created by Gavin Smalley, November 13th 2019.
  Released into the public domain.
*/

#ifndef ESP_OTA_GitHub_h
#define ESP_OTA_GitHub_h

#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <ArduinoJson.h>

#define GHOTA_HOST "api.github.com"
#define GHOTA_PORT 443
#define GHOTA_TIMEOUT 1500
#define GHOTA_CONTENT_TYPE "application/octet-stream"

typedef struct urlDetails_t {
      String proto;
      String host;
      String path;
      String url;
};

class ESPOTAGitHub {
  public:
      ESPOTAGitHub(BearSSL::CertStore* certStore, const char* user, const char* repo, const char* currentTag, const char* binFile, bool preRelease);
      bool checkUpgrade();
      bool doUpgrade();
      String getLastError();
      String getUpgradeURL();
  private:
      urlDetails_t _urlDetails(String url); // Separates a URL into protocol, host and path into a custom struct
      bool _resolveRedirects(); // Follows re-direct sequences until a "real" url is found.
      BearSSL::CertStore* _certStore;
      String _lastError; // Holds the last error generated
      String _upgradeURL; // Holds the upgrade URL (changes when getFinalURL() is run).
      const char* _user;
      const char* _repo;
      const char* _currentTag;
      const char* _binFile;
      bool _preRelease;
};

#endif
