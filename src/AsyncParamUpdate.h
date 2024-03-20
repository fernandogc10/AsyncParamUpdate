#ifndef AsyncParamUpdate_h
#define AsyncParamUpdate_h

#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncMqttClient.h>
#include "logger.h"
#include <WiFi.h>
#include <set>
#include <string>
#include <typeinfo>
#include <unordered_map>
#include <queue>
#include <Preferences.h>

#define DELAY_MS 5000
#define BOARDS_PREFIX "boards/"
#define REGISTRY_TOPIC "boards/registry"
#define JSON_BUFFER_SIZE 1024
#define MQTT_QOS_LEVEL 2
#define LOG_SUFFIX "/log"
#define WIFI_EVENT_CONNECTED SYSTEM_EVENT_STA_GOT_IP
#define WIFI_EVENT_DISCONNECTED SYSTEM_EVENT_STA_DISCONNECTED

class AsyncParamUpdate
{
public:
    struct ParamInfo
    {
        void *param;
        const char *typeName;
        std::string paramName;

        ParamInfo() : param(nullptr), typeName(nullptr) {}
        ParamInfo(void *param, const char *typeName, const std::string &paramName) : param(param), typeName(typeName), paramName(paramName) {}
    };

    AsyncParamUpdate(const char *wifiSSID, const char *wifiPassword, const char *mqttHost, uint16_t mqttPort, const char *mqttUser, const char *mqttPassword, const char *deviceName, bool mqttLog);

    template <typename T>
    void addParameter(const std::string &paramName, T &param)
    {
        if (preferences.isKey(paramName.c_str()))
        {
            getParameter(paramName, param);
        }
        else
        {
            saveParameter(paramName, param);
        }

        params[paramName] = ParamInfo(&param, typeid(T).name(), paramName);
        publishParametersList(paramName);
    }

    void getParameter(const std::string &paramName, int &outValue)
    {
        outValue = preferences.getInt(paramName.c_str(), 0);
    }

    void getParameter(const std::string &paramName, float &outValue)
    {
        outValue = preferences.getFloat(paramName.c_str(), 0.0f);
    }

    void getParameter(const std::string &paramName, bool &outValue)
    {
        outValue = preferences.getBool(paramName.c_str(), false);
    }

    void getParameter(const std::string &paramName, String &outValue)
    {
        outValue = preferences.getString(paramName.c_str(), "");
    }

    void begin()
    {
        preferences.begin("app", false);
    }

private:
    static AsyncParamUpdate *instance;

    const char *wifiSSID;
    const char *wifiPassword;
    String deviceName;
    String updateTopic;
    String logTopic;
    const char *mqttHost;
    uint16_t mqttPort;
    const char *mqttUser;
    const char *mqttPassword;
    bool mqttLog;
    logging::Logger logger;

    AsyncMqttClient mqttClient;
    Preferences preferences;

    std::unordered_map<std::string, ParamInfo> params;
    std::queue<String> pendingMessages;

    TaskHandle_t wifiConnectionTask;
    TaskHandle_t mqttConnectionTask;

    static void reconnectWifi(void *parameters);
    static void reconnectMqtt(void *parameters);
    static void WiFiEvent(WiFiEvent_t event);
    static void OnMqttConnect(bool sessionPresent);
    static void OnMqttDisconnect(AsyncMqttClientDisconnectReason reason);
    static void OnMqttSubscribe(uint16_t packetId, uint8_t qos);
    static void OnMqttUnsubscribe(uint16_t packetId);
    static void OnMqttPublish(uint16_t packetId);
    static void OnMqttReceived(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
    void InitMqtt();
    void publishParametersList(std::string paramName);
    void updateParameter(const ParamInfo &paramInfo, JsonVariant value);
    void saveParameter(const std::string &key, int value);
    void saveParameter(const std::string &key, float value);
    void saveParameter(const std::string &key, bool value);
    void saveParameter(const std::string &key, const char *value);
    void saveParameter(const std::string &key, const String &value);
    void saveParameter(const std::string &key, double value);
    void logMessage(const String &message);
};

#endif
