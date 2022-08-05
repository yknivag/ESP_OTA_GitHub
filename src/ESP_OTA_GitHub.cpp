/*
  ESP_OTA_GitHub.cpp - ESP library for auto updating code from GitHub releases.
  Created by Gavin Smalley, November 13th 2019.
  Released under the LGPL v2.1.
  It is the author's intention that this work may be used freely for private
  and commercial use so long as any changes/improvements are freely shared with
  the community under the same terms.
*/

#include "ESP_OTA_GitHub.h"

/* Public methods */

ESPOTAGitHub::ESPOTAGitHub(BearSSL::CertStore *certStore, const char *user,
                           const char *repo, const char *currentTag,
                           const char *binFile, const bool preRelease)
    : _certStore(certStore), _user(user), _repo(repo), _currentTag(currentTag),
      _binFile(binFile), _preRelease(preRelease)
{
}

bool ESPOTAGitHub::checkUpgrade()
{
    _setClock(); // Clock needs to be set to perform certificate checks

    WiFiClientSecure client;
    client.setCertStore(_certStore);

    if (!client.connect(GHOTA_HOST, GHOTA_PORT))
    {
        client.stop();
        _lastError = "Connection failed";
        return false;
    }

    client.printf_P(PSTR("GET /repos/%s/%s/releases/latest HTTP/1.1\r\n"
                         "Host: %s\r\n"
                         "User-Agent: ESP_OTA_GitHubArduinoLibrary\r\n"
                         "Connection: close\r\n\r\n"),
                    _user, _repo, GHOTA_HOST);

    // Skip header
    client.find("\r\n\r\n");

    // Filter to reduce size of resulting doc
    StaticJsonDocument<32> filter;
    filter["tag_name"] = true;
    filter["prerelease"] = true;
    StaticJsonDocument<256> doc;
    const DeserializationError error = deserializeJson(doc, client, DeserializationOption::Filter(filter));

    client.stop();

    if (error)
    {
        _lastError = "Failed to parse JSON: ";
        _lastError += error.c_str();
        return false;
    }

    if (!doc.containsKey("tag_name"))
    {
        _lastError = "JSON didn't match expected structure. 'tag_name' missing.";
        return false;
    }

    const char *release_tag = doc["tag_name"];
    // const char *release_name = doc["name"];
    if (strcmp(release_tag, _currentTag) == 0)
    {
        _lastError = "Already running latest release.";
        return false;
    }

    if (!_preRelease && doc["prerelease"])
    {
        _lastError = "Latest release is a pre-release and GHOTA_ACCEPT_PRERELEASE is set to false.";
        return false;
    }

    JsonArray assets = doc["assets"];
    bool valid_asset = false;
    for (auto asset : assets)
    {
        const char *asset_type = asset["content_type"];
        const char *asset_name = asset["name"];
        if (strcmp(asset_type, GHOTA_CONTENT_TYPE) == 0 &&
            strcmp(asset_name, _binFile) == 0)
        {
            _upgradeURL = asset["browser_download_url"].as<String>();
            valid_asset = true;
            break;
        }
    }

    if (!valid_asset)
    {
        _lastError = "No valid binary found for latest release.";
        return false;
    }

    return true;
}

bool ESPOTAGitHub::doUpgrade()
{
    if (_upgradeURL == "")
    {
        //_lastError = "No upgrade URL set, run checkUpgrade() first.";
        // return false;

        if (!checkUpgrade())
        {
            return false;
        }
    }
    else
    {
        // Clock needs to be set to perform certificate checks
        // Don't need to do this if running check upgrade first, as it will have just been done there.
        _setClock();
    }

    _resolveRedirects();

    urlDetails_t splitURL = _urlDetails(_upgradeURL);

    WiFiClientSecure client;
    bool mfln = client.probeMaxFragmentLength(splitURL.host, splitURL.port, 1024);
    if (mfln)
    {
        client.setBufferSizes(1024, 1024);
    }
    client.setCertStore(_certStore);

    ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);
    t_httpUpdate_return ret = ESPhttpUpdate.update(client, _upgradeURL);

    switch (ret)
    {
    case HTTP_UPDATE_FAILED:
        _lastError = ESPhttpUpdate.getLastErrorString();
        return false;

    case HTTP_UPDATE_NO_UPDATES:
        _lastError = "HTTP_UPDATE_NO_UPDATES";
        return false;

    case HTTP_UPDATE_OK:
        _lastError = "HTTP_UPDATE_OK";
        return true;
    }

    return false;
}

const String &ESPOTAGitHub::getLastError() { return _lastError; }

const String &ESPOTAGitHub::getUpgradeURL() { return _upgradeURL; }

/* Private methods */

void ESPOTAGitHub::_setClock()
{
    configTime(0, 0, GHOTA_NTP1, GHOTA_NTP2); // UTC

    time_t now = time(nullptr);
    while (now < 8 * 3600 * 2)
    {
        yield();
        delay(500);
        now = time(nullptr);
    }

    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
}

urlDetails_t ESPOTAGitHub::_urlDetails(const String &url)
{
    urlDetails_t urlDetails;
    if (url.startsWith("http://"))
    {
        urlDetails.proto = "http://";
        urlDetails.port = 80;
    }
    else
    {
        urlDetails.proto = "https://";
        urlDetails.port = 443;
    }
    const unsigned int protoLen = urlDetails.proto.length();
    const int firstSlash = url.indexOf('/', protoLen);
    urlDetails.host = url.substring(protoLen, firstSlash);
    urlDetails.path = url.substring(firstSlash);
    return urlDetails;
}

bool ESPOTAGitHub::_resolveRedirects()
{
    urlDetails_t splitURL = _urlDetails(_upgradeURL);
    bool isFinalURL = false;

    WiFiClientSecure client;
    client.setCertStore(_certStore);

    while (!isFinalURL)
    {
        if (!client.connect(splitURL.host.c_str(), splitURL.port))
        {
            _lastError = "Connection Failed.";
            return false;
        }

        client.printf_P(PSTR("GET %s HTTP/1.1\r\n"
                             "Host: %s\r\n"
                             "User-Agent: ESP_OTA_GitHubArduinoLibrary\r\n"
                             "Connection: close\r\n\r\n"),
                        splitURL.path, splitURL.host);

        while (client.connected())
        {
            const String response = client.readStringUntil('\n');
            const bool startsWithLowerL = response.startsWith("location: ");
            if (startsWithLowerL || response.startsWith("Location: "))
            {
                isFinalURL = false;
                String location = response;
                if (startsWithLowerL)
                {
                    location.replace("location: ", "");
                }
                else
                {
                    location.replace("Location: ", "");
                }
                location.remove(location.length() - 1);

                if (location.startsWith("http://") || location.startsWith("https://"))
                {
                    // absolute URL - separate host from path
                    splitURL = _urlDetails(location);
                }
                else
                {
                    // relative URL - host is the same as before, location represents the new path.
                    splitURL.path = location;
                }
                // leave the while loop so we don't set isFinalURL on the next line of the header.
                break;
            }
            else
            {
                // location header not present - this must not be a redirect. Treat this as the final address.
                isFinalURL = true;
            }
            if (response == "\r")
            {
                break;
            }
        }
    }

    if (isFinalURL)
    {
        _upgradeURL = splitURL.proto + splitURL.host + splitURL.path;
    }
    else
    {
        _lastError = "CONNECTION FAILED";
    }
    return isFinalURL;
}