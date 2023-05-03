/*
  ESP_OTA_GitHub.cpp - ESP library for auto updating code from GitHub releases.
  Created by Gavin Smalley, November 13th 2019.
  Released under the LGPL v2.1.
  It is the author's intention that this work may be used freely for private
  and commercial use so long as any changes/improvements are freely shared with
  the community under the same terms.
*/

#include "ESP_OTA_GitHub.h"

////#################  PIN DEFINITIONS  #################
//// Define I2C pins
//#define SCL_PIN 4    // GPIO4/D2
//#define SDA_PIN 5    // GPIO5/D1

//SSD1306Wire display(0x3c, SDA_PIN, SCL_PIN);
//PCF8575 PCF_20(0x20);



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
    Serial.println(F("downloading JSON from GitHub...")); 
    client.printf_P(PSTR("GET /repos/%s/%s/releases/latest HTTP/1.1\r\n"
                         "Host: %s\r\n"
                         //"User-Agent: ESP_OTA_GitHubArduinoLibrary\r\n"
                         "User-Agent: ESP_OTA_GitHub\r\n"
                         "Connection: close\r\n\r\n"),
                    _user, _repo, GHOTA_HOST);


    // Skip header
    client.find("\r\n\r\n");

    // Filter to reduce size of resulting doc
    Serial.println(F("setting filter..."));
    //StaticJsonDocument<32> filter;
    //StaticJsonDocument<512> filter; //increased size
    //filter["assets"] = true;
    //filter["assets"][0]["name"] = true;
    //filter["assets"][0]["content_type"] = true;
    //filter["assets"][0]["browser_download_url"] = true;
    //filter["tag_name"] = true;
    //filter["prerelease"] = true;
    
    //filter["content_type"] = true;
    //StaticJsonDocument<256> doc;


    //new chatgpt filter:
    StaticJsonDocument<200> filter;
        filter["assets"][0]["browser_download_url"] = true;
        filter["assets"][0]["content_type"] = true;
        filter["assets"][0]["name"] = true;
        filter["tag_name"] = true;
        filter["prerelease"] = true;
    StaticJsonDocument<1024> doc; //increased size





   const DeserializationError error = deserializeJson(doc, client, DeserializationOption::Filter(filter));
   //CHATGPT: const DeserializationError error = deserializeJson(doc, client, DeserializationOption::Filter(JsonArrayFilter().select("assets[content_type,name,browser_download_url]")));


    client.stop();

    if (error)
    {
        _lastError = "Failed to parse JSON: ";
        _lastError += error.c_str();
        return false;
    }else {
        Serial.println(F("Parsing and filtering JSON. Filtered document:"));
        String jsonStr;
        serializeJsonPretty(doc, jsonStr);
        Serial.println(jsonStr);
    }


    if (!doc.containsKey("tag_name"))
    {
        _lastError = "JSON didn't match expected structure. 'tag_name' missing.";
        return false;
    }



    //if (!doc.containsKey("content_type"))
    //{
    //    _lastError = "JSON didn't match expected structure. 'content_type' (=MIME type) missing.";
    //    return false;
    //}

    const char *release_tag = doc["tag_name"];
    // const char *release_name = doc["name"];
    

    //Print release tag information:
    Serial.println(); 
    Serial.println(F("comparing release tag (from GitHub) with current tag (of this running software):")); 
    Serial.print(F("current tag in this running software : "));
    Serial.println(_currentTag);
    Serial.print(F("release tag of the binary on GitHub:   "));
    Serial.println(release_tag);
    //const char *mime_type = doc["content_type"];
    //Serial.print(F("MIME type of the binary on GitHub:   "));


    
    if (strcmp(release_tag, _currentTag) == 0)
    {
        _lastError = "Already running latest release.";
        return false;
    }
    else{
        Serial.println(F("release tags are not matching. We need to upgrade (or downgrade). Checking if the binary on GitHub meets the requirements for an OTA update."));
    }

    if (!_preRelease && doc["prerelease"])
    {
        _lastError = "Latest release is a pre-release and GHOTA_ACCEPT_PRERELEASE is set to false.";
        return false;
    }


    JsonArray assets = doc["assets"];

   // Serial.println("Assets:");
   // for (JsonVariant asset : assets) {
   //   Serial.println(asset.as<String>());
   // }

    bool valid_asset = false;
//    Serial.println(F("DEBUG #1 for loop of assets..."));


//Serial.println("########     DEBUG Asset check:  ###############");



    for (auto asset : assets)
    {
        const char *asset_type = asset["content_type"];
        const char *asset_name = asset["name"];
            //Serial.print("asset_type: ");
            //Serial.println(asset_type);
            //Serial.print("GHOTA_CONTENT_TYPE: ");
            //Serial.println(GHOTA_CONTENT_TYPE);
        if ((strcmp(asset_type, GHOTA_CONTENT_TYPE) == 0 || (strcmp(asset_type, GHOTA_CONTENT_TYPE_MAC) == 0))&&
            strcmp(asset_name, _binFile) == 0)
        //{
        //if (strcmp(asset_name, _binFile) == 0)
        {
            _upgradeURL = asset["browser_download_url"].as<String>();
            valid_asset = true;
            Serial.println();
            Serial.print(F("GitHub binary MIME type: "));
            Serial.print(asset_type);
            if ((strcmp(asset_type, GHOTA_CONTENT_TYPE) == 0)){
                Serial.println(F(" OK - standard compile"));
            }
            else if ((strcmp(asset_type, GHOTA_CONTENT_TYPE_MAC) == 0)){
                Serial.println(F(" OK - compiled on MacOS"));
            }
            if(strcmp(asset_name, _binFile) == 0){
               Serial.print(F("GitHub binary file name: OK - matches as specified in program code: ")); 
               Serial.println(_binFile);
            }
            Serial.println();
            Serial.println(F("#######################################################################################"));
            Serial.println(F("#                                                                                     #"));
            Serial.println(F("#                     checks successful, all requirements are met.                    #"));
            Serial.println(F("#                        --> starting OTA update from GitHub! <--                     #"));
            Serial.println(F("#                                                                                     #"));
            Serial.println(F("#######################################################################################"));
            Serial.println();
            Serial.println();
            break;
        }
        else{
            Serial.println();
            Serial.print(F("at least one of the checks regarding the binary on GitHub FAILED."));
            Serial.println();
            if ((strcmp(asset_type, GHOTA_CONTENT_TYPE) != 0) && (strcmp(asset_type, GHOTA_CONTENT_TYPE_MAC) != 0)){
                Serial.println(F("wrong MIME type detected. The MIME type of the uploaded Binary does not match the requirement. "));
                Serial.println(F("You need to upload a binary that has MIME type application/octet-stream or application/macbinary."));
                Serial.print(F("The MIME type of the binary that was uploaded as an asset to latest GitHub release is : "));
                Serial.println(asset_type);
                Serial.print(F("this must match either the mime type of GHOTA_CONTENT_TYPE, which is            : "));
                Serial.println(GHOTA_CONTENT_TYPE);
                Serial.print(F("...or it must match the mime type of GHOTA_CONTENT_TYPE_MAC, which is           : "));
                Serial.println(GHOTA_CONTENT_TYPE_MAC);
                Serial.println(F("Both is not the case, you need to correct that either on the side of the uploaded binary,"));
                Serial.println(F("or by modifying ESP_OTA_GitHub.h and ESP_OTA_GitHub.cpp files of the Library ESP-OTA_GitHub by adding your additional MIME type - if applicable."));
            }
            if ( strcmp(asset_name, _binFile) != 0){
                Serial.println(F("wrong file name. The file name of the uploaded binary does not match the requirement. "));
                Serial.println(F("Upload a binary as an asset to a release of your repository that matches the file name set in your Arduino code, "));
                Serial.print(F("or adapt the filename for the binary that has been (pre)set in your Arduino code."));
                Serial.print(F("File name of the binary that was uploaded as an asset of the latest GitHub release   : "));
                Serial.println(asset_name);
                Serial.print(F("File name requirement of the Arduino sketch, which is set with #define GHOTA_BIN_FILE: "));
                Serial.println(_binFile);
                Serial.println();
            }
        }
    
    }
   // Serial.println(F("DEBUG #3 after for loop of assets..."));


    //NEW TRY CHATGPT:
//for (auto asset : assets)
//{
//    const char *asset_type = asset["content_type"];
//    const char *asset_name = asset["name"];
//    if ((strcmp(asset_type, GHOTA_CONTENT_TYPE) == 0 || strcmp(asset_type, GHOTA_CONTENT_TYPE_MAC) == 0) &&
//        strcmp(asset_name, _binFile) == 0)
//    {
//        _upgradeURL = asset["browser_download_url"].as<String>();
//        valid_asset = true;
//        break;
//    }
//}
    //END OF NEW TRY

    if (!valid_asset)
    {
        _lastError = "No valid binary found for latest release.";
        Serial.println();
        Serial.print(F("at least one of the checks regarding the binary on GitHub FAILED."));
        Serial.println();
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
    //add percentage output during update:
    ESPhttpUpdate.onProgress([](int progress, int total) {
  //display.clear();
  //delay(10);
  ////display.setFont(ArialMT_Plain_16);
  //display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
  //display.drawProgressBar(4, 36, 120, 10, progress / (total / 100) );
    Serial.printf("OTA update from GitHub repo: %d%%\n", (progress / (total / 100)));
    });

    ESPhttpUpdate.onEnd([]() {
        Serial.println();
        Serial.println(F("#######################################################################################"));
        Serial.println(F("#                                                                                     #"));
        Serial.println(F("#                        OTA update from GitHub repo: finished                        #"));
        Serial.println(F("#                            - device will reboot now -                               #"));
        Serial.println(F("#                                                                                     #"));
        Serial.println(F("#######################################################################################"));
        Serial.println();
        Serial.println();
    Serial.printf("");
  });

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


bool ESPOTAGitHub::_resolveRedirects() {
    urlDetails_t splitURL = _urlDetails(_upgradeURL);
    String proto = splitURL.proto;
    String host = splitURL.host;
    int port = splitURL.port;
    String path = splitURL.path;
    bool isFinalURL = false;

    BearSSL::WiFiClientSecure client;
    client.setCertStore(_certStore);

    while (!isFinalURL) {
        if (!client.connect(host, port)) {
            _lastError = "Connection Failed.";
            return false;
        }

        client.print(String("GET ") + path + " HTTP/1.1\r\n" +
            "Host: " + host + "\r\n" +
            "User-Agent: ESP_OTA_GitHubArduinoLibrary\r\n" +
            "Connection: close\r\n\r\n");

        while (client.connected()) {
            String response = client.readStringUntil('\n');
            if (response.startsWith("location: ") || response.startsWith("Location: ")) {
                isFinalURL = false;
                String location = response;
                if (response.startsWith("location: ")) {
                    location.replace("location: ", "");
                } else {
                    location.replace("Location: ", "");
                }
                location.remove(location.length() - 1);

                if (location.startsWith("http://") || location.startsWith("https://")) {
                    //absolute URL - separate host from path
                    urlDetails_t url = _urlDetails(location);
                    proto = url.proto;
                    host = url.host;
                    port = url.port;
                    path = url.path;
                } else {
                    //relative URL - host is the same as before, location represents the new path.
                    path = location;
                }
                //leave the while loop so we don't set isFinalURL on the next line of the header.
                break;
            } else {
                //location header not present - this must not be a redirect. Treat this as the final address.

                isFinalURL = true;
            }
            if (response == "\r") {
                break;
            }
        }
    }

    if(isFinalURL) {
        String finalURL = proto + host + path;
        _upgradeURL = finalURL;
        Serial.print("final URL after resolving redirects: ");
        Serial.println(_upgradeURL);
        return true;
    } else {
        _lastError = "CONNECTION FAILED";
        return false;
    }
}