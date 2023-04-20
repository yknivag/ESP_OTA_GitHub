#!/bin/bash

# This script assumes that moquitto MQTT client is installed on your system. You can use any other as well.
# It will ask all devices connected to the mqtt broker to check for an OTA 
# update of their firmware. If a firmware binary that has a matching file name and MIME type 
# has been uploaded to a release of the repository AS AN ASSET OF THE RELEASE (not just in 
# the repository itself), and if that release has a different version tag (e.g. 0.1.4) than 
# the firmware currently running on ESP8266 (e.g. 0.1.3), then the device will do an OTA update. 
# the following command can also be issued manually.

#NOTE that you need to adapt the topic (MyDevice/update/ in this example) to the topic that you are actually using.
# The code in the examples uses MyDevice as a prefix, but you wand an individual one so that only your devices are asked to do an OTA update and 
# only when you want it.
# To achieve that, and to prevent that you need to put your personal MQTT topic in the sketch that you may upload to your (public) GitHub 
# repository, the concept is to upload configurations files to the SPIFFS memory of your ESP8266, that contain MQTT config and WiFi credentials.
# By doing so, your private data is kept on the device locally, while your general code can be published publicly (which is necessary for the OTA 
# update to work anyhow).

# when you have uploaded the mqtt-topics.txt to SPIFFS memory of your ESP8266 (see example file in GitHub repository), you need to replace "MyDevice" in the 
# command below accordingly, as this will be the MQTT topic the device listens to. If you leave the standard "MyDevice", your device(s) and the devices of everyone 
# else would look for an update each time someone sends the message "update" to this topic, which could happen (too) often.




mosquitto_pub -h broker.hivemq.com -p 1883 -t 'MyDevice/update/' -m 'update'
