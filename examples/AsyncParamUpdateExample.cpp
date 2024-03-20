#include "AsyncParamUpdate.h"

// WiFi and MQTT credentials
const char *ssid = "YOUR_SSID";
const char *password = "YOUR_WIFI_PASSWORD";
const char *mqttHost = "MQTT_HOST_IP_OR_HOSTNAME";
const uint16_t mqttPort = 1883; // Change according to your setup
const char *mqttUser = "YOUR_MQTT_USER";
const char *mqttPassword = "YOUR_MQTT_PASSWORD";
const char *deviceName = "DEVICE_NAME";
bool mqttLog = true; // Change according to your setup

// Create a global instance of the AsyncParamUpdate class
AsyncParamUpdate asyncParamUpdater(ssid, password, mqttHost, mqttPort, mqttUser, mqttPassword, deviceName, mqttLog);

void setup()
{
    // Initialize the instance
    asyncParamUpdater.begin();

    // Define some parameters to add
    int someIntParameter = 42;
    float someFloatParameter = 3.14;
    bool someBoolParameter = true;
    String someStringParameter = "Hello World";

    // Use addParameter to add the defined parameters
    asyncParamUpdater.addParameter("someIntParameter", someIntParameter);
    asyncParamUpdater.addParameter("someFloatParameter", someFloatParameter);
    asyncParamUpdater.addParameter("someBoolParameter", someBoolParameter);
    asyncParamUpdater.addParameter("someStringParameter", someStringParameter);
}

void loop()
{
    // Code that makes use of the parameter values goes here
}
