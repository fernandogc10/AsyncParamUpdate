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
#include <map>
#include <queue>
#include <Preferences.h>
#include <LoRa.h>

#define SCK 5   // GPIO5  -- SX1276's SCK
#define MISO 19 // GPIO19 -- SX1276's MISO
#define MOSI 27 // GPIO27 -- SX1276's MOSI
#define SS 18   // GPIO18 -- SX1276's CS
#define RST 14  // GPIO14 -- SX1276's RESET
#define DI0 26  // GPIO26 -- SX1276's IRQ(Interrupt Request)
#define BAND 915E6
#define DELAY_MS 5000
#define BOARDS_PREFIX "boards/"
#define REGISTRY_TOPIC "boards/registry"
#define JSON_BUFFER_SIZE 1024
#define MQTT_QOS_LEVEL 2
#define LOG_SUFFIX "/log"
#define CONFIRMATION_SUFFIX "/confirmation"
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

    AsyncParamUpdate(const char *deviceName, bool mqttLog = false);

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
    String confirmationTopic;
    String logTopic;
    const char *mqttHost;
    uint16_t mqttPort;
    const char *mqttUser;
    const char *mqttPassword;
    bool mqttLog;
    bool useLoRa;
    logging::Logger logger;

    AsyncMqttClient mqttClient;
    Preferences preferences;

    std::unordered_map<std::string, ParamInfo> params;
    std::queue<String> pendingMessages;

    TaskHandle_t wifiConnectionTask;
    TaskHandle_t mqttConnectionTask;

    static void OnLoRaReceived(int packetSize);
    static void reconnectWifi(void *parameters);
    static void reconnectMqtt(void *parameters);
    static void sendActiveMessage(void *parameters);
    static void WiFiEvent(WiFiEvent_t event);
    static void OnMqttConnect(bool sessionPresent);
    static void OnMqttDisconnect(AsyncMqttClientDisconnectReason reason);
    static void OnMqttSubscribe(uint16_t packetId, uint8_t qos);
    static void OnMqttUnsubscribe(uint16_t packetId);
    static void OnMqttPublish(uint16_t packetId);
    static void OnMqttReceived(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
    static void onTxDone();
    void InitMqtt();
    void publishParametersList(std::string paramName);
    bool updateParameter(const ParamInfo &paramInfo, JsonVariant value);
    void saveParameter(const std::string &key, int value);
    void saveParameter(const std::string &key, float value);
    void saveParameter(const std::string &key, bool value);
    void saveParameter(const std::string &key, const char *value);
    void saveParameter(const std::string &key, const String &value);
    void saveParameter(const std::string &key, double value);
    void initializeLoRa();
    void logMessage(const String &message);
};

#endif
