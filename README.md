# ESP OTA GitHub Library

Automatically update your ESP from exported compiled binaries attached to GitHub releases of your project.  Supports upgrade and downgrade of sketch.  SPIFFS updating not yet supported.

In short if you have the code for your project in GitHub then by adding exported compiled binaries to releases you can, with this library, update devices remotely.  Using naming conventions for your compiled binaries you can add different binaries for different platforms and the library will upgrade to the correct one.  Deleting a release will cause the device to automatically downgrade to the previous one.

## Copyright and Acknowledgements

This library depends on the `BearSSL::WiFiClientSecure`, `ESP8266HTTPClient`, `ESP8266httpUpdate` and `ArduinoJSON` libraries.  I claim no credit for their work, please see their respective readme files for their details.

The work in this library is entirely my own work and released into the public domain on the understanding that anyone who alters, modifies or improves it in any way releases those modification, alterations or improvements for the benefit of everyone.

## Compatability

The library should work with all boards that can use a `BearSSL::WiFiClientSecure` client.

The library requires `ArduinoJSON` library prior to v6.  v6 compatibility is likely to follow some time in the near future.

## Certificates

As with all uses of https with the `BearSSL:WiFiClientSecure` client, this library requires a certificate archive to be generated.  Official instructions, such as they are, can be found in the notes at the top of [this example](https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/examples/BearSSL_CertStore/BearSSL_CertStore.ino) but the incuded example contains such relevant data and python script to generate that data as was current at the time it was published.

To regenerate the certificates (as will be needed from time to time) run the python script at ESP_OTA_GitHub/examples/GitHub_Upgrade/certs-from-mozilla.py (the definitive version of this can be found in the esp8266/Arduino repository listed above), create a folder within your sketch folder called "data", copy the generated `certs.ar` file to that folder and use the [ESP SPIFFS Upload Tool](https://github.com/esp8266/arduino-esp8266fs-plugin/) to put that file on the ESP8266 that you wish to use. Thereafter ensure that future uploades are to program space only and do not overwrite this data.  You may use SPIFFS for other purposes too so long as you do not alter the `certs.ar` file in any way.

## Hardware Wiring

There are no specific hardware or wiring requirements.

## Features

Update your ESP866 based device directly from adding a binary to a GitHub release.

## Usage

### Settings

`#define GHOTA_USER "github_user_name"`
The GitHub username that owns the repository that the updates will be stored in

`#define GHOTA_REPO "github_respository_name"`
The name of the GitHub repository that the updates will be stored in

`#define GHOTA_CURRENT_TAG "0.0.0"`
The release tag of THIS version of the code.  An update will happen when the latest release available DOESN'T match this tag.

`#define GHOTA_BIN_FILE "temp.ino.d1_mini.bin"`
The name of the file that the upgrade code should look for for this architecture.  This should be kept the same across all releases as this is the filename that the code will look for in the NEXT release.

`#define GHOTA_ACCEPT_PRERELEASE 0`
Boolean that determines whether the upgrade code should process releases tagged as "pre-release" or just full releases.

### Include

*#include <ESP_OTA_GitHub.h>*

### Constructor

*ESP_OTA_GitHub ESP_OTA_GitHub(BearSSL::CertStore* certStore, const char* user, const char* repo, const char* currentTag, const char* binFile, bool preRelease);*

In the example code this is called using the `#define` directives set in the Settings section like this:

*ESPOTAGitHub ESPOTAGitHub(&certStore, GHOTA_USER, GHOTA_REPO, GHOTA_CURRENT_TAG, GHOTA_BIN_FILE, GHOTA_ACCEPT_PRERELEASE);*

`certStore` needs to be created first as in the example.  This is common whenever using the `BearSSL::WiFiClientSecure` library.  More details can be found in their documentation.

### checkUpgrade()

*bool checkUpgrade();*

This has been separated from `doUpgrade()` so that it is possible to create a user confirmation prior to upgrading, or similar.  Returns `true` if an upgrade is available, or `false` if one was not found.  Check `getLastError()` immediately afterwards to determine if no upgrade was found or if the check failed.

After running this method it is possible to call `getUpgradeURL()` to display the URL of the upgrade that is to be performed.

If this function returns true then either `doUpgrade()` can be called immediately or some kind of prompt issued to the user for them to initiate or ignore the update.

### doUpgrade()

*bool doUpgrade();*

Returns `true` once an upgrade is successfully completed (though this is never seen as a successful upgrade triggers a reboot).  Returns `false` if something went wrong.  Check `getLastError()` immediately aftewards to determine the cause of the failure.

Must be run AFTER `checkUpgrade()` otherwise it will error.

## Ongoing Usage

This is slightly more complicated than using ArduinoOTA, but there is one big advantage - it works for devices anywhere on the internet, not just locally and it works whenever they next check for updates, not just when you press the button, so can be used for devices anywhere "in the wild".

Create a repository (must be public) on GitHub for your code.  Each time you wish to distribute a new version follow these instructions:

1. Create new versions of your code for each device type that you wish to update.
2. In the Arduino IDE click `Sketch` and `Export Compiled Binary` to create new `.bin` files (these must be named as per the `#define GHOTA_BIN_FILE` directives in the previous versions of the code).
3. Commit your changes to GitHub and create a new release with the same tag as you placed in the `#define GHOTA_CURRENT_TAG` directive in the new code.
4. Add your binary file(s) to the relase.

The next time your device(s) check for updates they will find that the latest release tag no longer matches that in their current firmware when `checkUpgrade()` is called and will update when `checkUpgrade()` is called.

## Examples

An exmaple is provided which shows how to use the code on the device.  It is left to the user to set up their repository.
