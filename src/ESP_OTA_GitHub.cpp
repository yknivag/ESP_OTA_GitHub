/*
  ESP_OTA_GitHub.cpp - ESP library for auto updating code from GitHub releases.
  Created by Gavin Smalley, November 13th 2019.
  Released under the LGPL v2.1.
  It is the author's intention that this work may be used freely for private
  and commercial use so long as any changes/improvements are freely shared with
  the community under the same terms.
*/

#include "ESP_OTA_GitHub.h"

ESPOTAGitHub::ESPOTAGitHub(BearSSL::CertStore* certStore, const char* user, const char* repo, const char* currentTag, const char* binFile, bool preRelease) {
    _certStore = certStore;
    _user = user;
    _repo = repo;
    _currentTag = currentTag;
    _binFile = binFile;
    _preRelease = preRelease;
    _lastError = "";
    _upgradeURL = "";
}

/* Private methods */

urlDetails_t ESPOTAGitHub::_urlDetails(String url) {
    String proto = "";
    int port = 80;
    if (url.startsWith("http://")) {
        proto = "http://";
        url.replace("http://", "");
    } else {
        proto = "https://";
        port = 443;
        url.replace("https://", "");
    }
    int firstSlash = url.indexOf('/');
    String host = url.substring(0, firstSlash);
    String path = url.substring(firstSlash);

    urlDetails_t urlDetail;

    urlDetail.proto = proto;
    urlDetail.host = host;
    urlDetail.port = port;
    urlDetail.path = path;

    return urlDetail;
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
        return true;
    } else {
        _lastError = "CONNECTION FAILED";
        return false;
    }
}

// Set time via NTP, as required for x.509 validation
void ESPOTAGitHub::_setClock() {
	configTime(0, 0, GHOTA_NTP1, GHOTA_NTP2);  // UTC

	time_t now = time(nullptr);
	while (now < 8 * 3600 * 2) {
		yield();
		delay(500);
		now = time(nullptr);
	}

	struct tm timeinfo;
	gmtime_r(&now, &timeinfo);
}

/* Public methods */

bool ESPOTAGitHub::checkUpgrade() {
	
	_setClock(); // Clock needs to be set to perform certificate checks
	
    BearSSL::WiFiClientSecure client;
    client.setCertStore(_certStore);
    if (!client.connect(GHOTA_HOST, GHOTA_PORT)) {
        _lastError = "Connection failed";
        return false;
    }
	  
    String url = "/repos/";
    url += _user;
    url += "/";
    url += _repo;
    url += "/releases/latest";
	  
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
        "Host: " + GHOTA_HOST + "\r\n" +
        "User-Agent: ESP_OTA_GitHubArduinoLibrary\r\n" +
        "Connection: close\r\n\r\n");

    while (client.connected()) {
        String response = client.readStringUntil('\n');
        if (response == "\r") {
			break;
        }
    }
    
	String response = client.readStringUntil('\n');
    //client->stop();
	  
	// --- ArduinoJSON v6 --- //
	 
	// Get from https://arduinojson.org/v6/assistant/
	const size_t capacity = JSON_ARRAY_SIZE(3) + 3*JSON_OBJECT_SIZE(13) + 5*JSON_OBJECT_SIZE(18) + 5560;

	DynamicJsonDocument doc(capacity);
	  
	DeserializationError error = deserializeJson(doc, response);
	  
	if (!error) {
		if (doc.containsKey("tag_name")) {
			const char* release_tag = doc["tag_name"];
            const char* release_name = doc["name"];
            bool release_prerelease = doc["prerelease"];
			if (strcmp(release_tag, _currentTag) != 0) {
				if (!_preRelease) {
					if (release_prerelease) {
						_lastError = "Latest release is a pre-release and GHOTA_ACCEPT_PRERELEASE is set to false.";
						return false;
					}
				}
				JsonArray assets = doc["assets"];
				bool valid_asset = false;
				for (auto asset : assets) {
					const char* asset_type = asset["content_type"];
					const char* asset_name = asset["name"];
					const char* asset_url = asset["browser_download_url"];
					  
					if (strcmp(asset_type, GHOTA_CONTENT_TYPE) == 0 && strcmp(asset_name, _binFile) == 0) {
						_upgradeURL = asset_url;
						valid_asset = true;
					} else {
						valid_asset = false;
					}
				}
				if (valid_asset) {
					return true;
				} else {
					_lastError = "No valid binary found for latest release.";
					return false;
				}
			} else {
				_lastError = "Already running latest release.";
                return false;
			}
		} else {
			_lastError = "JSON didn't match expected structure. 'tag_name' missing.";
            return false;
		}
	} else {
		_lastError = "Failed to parse JSON."; // Error was: " + error.c_str();
        return false;
	}
	// --- END ArduinoJSON v6 --- //
}

bool ESPOTAGitHub::doUpgrade() {
    if (_upgradeURL == "") {
        //_lastError = "No upgrade URL set, run checkUpgrade() first.";
        //return false;
		
		if(!checkUpgrade()) {
			return false;
		}
    } else {
		_setClock(); // Clock needs to be set to perform certificate checks
		// Don't need to do this if running check upgrade first, as it will have just been done there.
	}

	_resolveRedirects();
	
    urlDetails_t splitURL = _urlDetails(_upgradeURL);
	
    BearSSL::WiFiClientSecure client;
    bool mfln = client.probeMaxFragmentLength(splitURL.host, splitURL.port, 1024);
    if (mfln) {
        client.setBufferSizes(1024, 1024);
    }
    client.setCertStore(_certStore);

    ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);
    t_httpUpdate_return ret = ESPhttpUpdate.update(client, _upgradeURL);

    switch (ret) {
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
}

String ESPOTAGitHub::getLastError() {
    return _lastError;
}

String ESPOTAGitHub::getUpgradeURL() {
    return _upgradeURL;
}
