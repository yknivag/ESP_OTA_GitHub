/*
  ESP_OTA_GitHub.h - ESP library for auto updating code from GitHub releases.
  Created by Gavin Smalley, November 13th 2019.
  Released under the LGPL v2.1.
  It is the author's intention that this work may be used freely for private
  and commercial use so long as any changes/improvements are freely shared with
  the community under the same terms.
*/

#ifndef ESP_OTA_GitHub_h
#define ESP_OTA_GitHub_h

#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <ArduinoJson.h>
#include <time.h>

constexpr static const char *GHOTA_HOST = "api.github.com";
constexpr static const uint16_t GHOTA_PORT = 443;
constexpr static const uint16_t GHOTA_TIMEOUT = 1500;
constexpr static const char *GHOTA_CONTENT_TYPE = "application/octet-stream";
//adding alternatively allowed MIME type for binaries that were compiled on Arduino IDE on MacOS:
constexpr static const char *GHOTA_CONTENT_TYPE_MAC = "application/macbinary";

constexpr static const char *GHOTA_NTP1 = "pool.ntp.org";
constexpr static const char *GHOTA_NTP2 = "time.nist.gov";

struct urlDetails_t
{
    String proto;
    String host;
    int port;
    String path;
};

class ESPOTAGitHub
{
public:
    ESPOTAGitHub(BearSSL::CertStore *certStore, const char *user, const char *repo, const char *currentTag, const char *binFile, const bool preRelease);
    bool checkUpgrade();
    bool doUpgrade();
    const String &getLastError();
    const String &getUpgradeURL();

private:
    /// @brief Set time via NTP, as required for x.509 validation
    void _setClock();
    /// @brief Separates a URL into protocol, host and path into a custom struct
    ///
    /// @param url URL to separate
    /// @return urlDetails_t Separated URL
    urlDetails_t _urlDetails(const String &url);
    /// @brief Follows re-direct sequences until a "real" url is found.
    ///
    /// @return true URL resolved
    /// @return false URL not resolved
    bool _resolveRedirects();

private:
    BearSSL::CertStore *_certStore;
    String _lastError = "";  /// Holds the last error generated
    String _upgradeURL = ""; /// Holds the upgrade URL (changes when _resolveRedirects() is run).
    const char *_user;
    const char *_repo;
    const char *_currentTag;
    const char *_binFile;
    const bool _preRelease;
};

#endif
