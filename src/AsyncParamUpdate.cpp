#include "AsyncParamUpdate.h"

AsyncParamUpdate *AsyncParamUpdate::instance = nullptr;

AsyncParamUpdate::AsyncParamUpdate(const char *wifiSSID, const char *wifiPassword, const char *mqttHost, uint16_t mqttPort, const char *mqttUser, const char *mqttPassword, const char *deviceName, bool mqttLog)
{

    instance = this;
    this->wifiSSID = wifiSSID;
    this->wifiPassword = wifiPassword;
    this->deviceName = DEVICE_PREFIX + String(deviceName);
    this->logTopic = this->deviceName + LOG_SUFFIX;
    this->parametersTopic = String(this->deviceName) + PARAMETERS_SUFFIX;
    this->mqttHost = mqttHost;
    this->mqttPort = mqttPort;
    this->mqttUser = mqttUser;
    this->mqttPassword = mqttPassword;
    this->mqttLog = mqttLog;

    InitMqtt();
    WiFi.onEvent(AsyncParamUpdate::WiFiEvent);

    xTaskCreate(reconnectWifi, "WiFiReconnect", 4096, NULL, 1, &wifiConnectionTask);
    xTaskCreate(reconnectMqtt, "MqttReconnect", 4096, NULL, 1, &mqttConnectionTask);
    vTaskSuspend(mqttConnectionTask);

    // addParameter("Wifi-ssid", this->wifiSSID);
    // addParameter("Wifi-password", this->wifiPassword);
}

void AsyncParamUpdate::reconnectWifi(void *parameters)
{

    for (;;)
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            vTaskSuspend(NULL);
        }
        else
        {
            // instance->logMessage("Wifi not connected...");
            WiFi.mode(WIFI_STA);
            WiFi.begin(instance->wifiSSID, instance->wifiPassword);
            vTaskDelay(pdMS_TO_TICKS(DELAY_MS));
        }
    }
}

void AsyncParamUpdate::WiFiEvent(WiFiEvent_t event)
{
    switch (event)
    {
    case WIFI_EVENT_CONNECTED:
        instance->logMessage("WiFi connected");
        instance->logMessage("IP address: ");
        instance->logMessage(WiFi.localIP().toString());
        vTaskSuspend(instance->wifiConnectionTask);
        vTaskResume(instance->mqttConnectionTask);
        break;
    case WIFI_EVENT_DISCONNECTED:
        vTaskSuspend(instance->mqttConnectionTask);
        vTaskResume(instance->wifiConnectionTask);
        break;
    }
}

void AsyncParamUpdate::reconnectMqtt(void *parameters)
{
    for (;;)
    {
        if (!instance->mqttClient.connected())
        {
            instance->mqttClient.connect();
        }

        vTaskDelay(pdMS_TO_TICKS(DELAY_MS));
    }
}

void AsyncParamUpdate::OnMqttConnect(bool sessionPresent)
{

    vTaskSuspend(instance->mqttConnectionTask);
    instance->mqttClient.subscribe(instance->parametersTopic.c_str(), MQTT_QOS_LEVEL);

    while (!instance->pendingMessages.empty())
    {

        String pendingMessage = instance->pendingMessages.front();
        instance->mqttClient.publish(instance->logTopic.c_str(), MQTT_QOS_LEVEL, false, pendingMessage.c_str());
        instance->pendingMessages.pop();
    }

    instance->logMessage("Connected to MQTT.");
}

void AsyncParamUpdate::OnMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{

    instance->logMessage("Disconnected from MQTT.");

    if (WiFi.isConnected())
    {
        vTaskResume(instance->mqttConnectionTask);
    }
}

void AsyncParamUpdate::OnMqttSubscribe(uint16_t packetId, uint8_t qos)
{
    // logMessage("Subscribe acknowledged.");
    // Serial.print("  packetId: ");
    // logMessage(packetId);
    // Serial.print("  qos: ");
    // logMessage(qos);
}

void AsyncParamUpdate::OnMqttUnsubscribe(uint16_t packetId)
{
    // logMessage("Unsubscribe acknowledged.");
    // Serial.print("  packetId: ");
    // logMessage(packetId);
}

void AsyncParamUpdate::OnMqttPublish(uint16_t packetId)
{
    // logMessage("Publish acknowledged.");
    // Serial.print("  packetId: ");
    // logMessage(packetId);
}

String GetPayloadContent(char *data, size_t len)
{
    String content = "";
    for (size_t i = 0; i < len; i++)
    {
        content.concat(data[i]);
    }
    return content;
}

void AsyncParamUpdate::OnMqttReceived(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
    String content = GetPayloadContent(payload, len);
    JsonDocument doc;

    DeserializationError error = deserializeJson(doc, content);
    if (error)
    {
        instance->logMessage("Message received deserializeJson() failed with code ");
        instance->logMessage(error.c_str());
        return;
    }

    for (JsonPair kv : doc.as<JsonObject>())
    {
        String key = kv.key().c_str();
        auto paramIter = instance->params.find(key.c_str());

        if (paramIter != instance->params.end())
        {
            ParamInfo &paramInfo = paramIter->second;
            instance->updateParameter(paramInfo, kv.value());
        }
    }
}

void AsyncParamUpdate::updateParameter(const ParamInfo &paramInfo, JsonVariant value)
{
    const std::string &paramName = paramInfo.paramName;

    if (strcmp(paramInfo.typeName, typeid(int).name()) == 0)
    {
        *(static_cast<int *>(paramInfo.param)) = value.as<int>();
        preferences.putInt(paramName.c_str(), value.as<int>());
    }
    else if (strcmp(paramInfo.typeName, typeid(float).name()) == 0)
    {
        *(static_cast<float *>(paramInfo.param)) = value.as<float>();
        preferences.putFloat(paramName.c_str(), value.as<float>());
    }
    else if (strcmp(paramInfo.typeName, typeid(double).name()) == 0)
    {
        *(static_cast<double *>(paramInfo.param)) = value.as<double>();
        preferences.putDouble(paramName.c_str(), value.as<double>());
    }
    else if (strcmp(paramInfo.typeName, typeid(bool).name()) == 0)
    {
        *(static_cast<bool *>(paramInfo.param)) = value.as<bool>();
        preferences.putBool(paramName.c_str(), value.as<bool>());
    }
    else if (strcmp(paramInfo.typeName, typeid(String).name()) == 0)
    {
        *(static_cast<String *>(paramInfo.param)) = value.as<String>();
        preferences.putString(paramName.c_str(), value.as<String>().c_str());
    }
}

void AsyncParamUpdate::InitMqtt()
{
    mqttClient.onConnect(AsyncParamUpdate::OnMqttConnect);
    mqttClient.onDisconnect(AsyncParamUpdate::OnMqttDisconnect);
    mqttClient.onPublish(AsyncParamUpdate::OnMqttPublish);
    // mqttClient.onSubscribe(ConfigManager::OnMqttSubscribe);
    mqttClient.onMessage(AsyncParamUpdate::OnMqttReceived);
    mqttClient.setServer(mqttHost, mqttPort);
    mqttClient.setCredentials(mqttUser, mqttPassword);
    mqttClient.setClientId(deviceName.c_str());
}

void AsyncParamUpdate::publishParametersList(std::string paramName)
{

    while (!mqttClient.connected())
    {
    };
    logMessage(parametersTopic.c_str());

    JsonDocument doc;
    JsonArray paramsArray = doc["parameters"].to<JsonArray>();

    for (const auto &p : params)
    {
        paramsArray.add(p.first.c_str());
    }

    char jsonBuffer[JSON_BUFFER_SIZE];
    serializeJson(doc, jsonBuffer);
    mqttClient.publish(parametersTopic.c_str(), MQTT_QOS_LEVEL, true, jsonBuffer);
}
void AsyncParamUpdate::saveParameter(const std::string &key, int value)
{
    preferences.putInt(key.c_str(), value);
}

void AsyncParamUpdate::saveParameter(const std::string &key, float value)
{
    preferences.putFloat(key.c_str(), value);
}

void AsyncParamUpdate::saveParameter(const std::string &key, bool value)
{
    preferences.putBool(key.c_str(), value);
}

void AsyncParamUpdate::saveParameter(const std::string &key, const char *value)
{
    preferences.putString(key.c_str(), value);
}

void AsyncParamUpdate::saveParameter(const std::string &key, const String &value)
{
    preferences.putString(key.c_str(), value.c_str());
}

void AsyncParamUpdate::saveParameter(const std::string &key, double value)
{
    preferences.putFloat(key.c_str(), static_cast<float>(value));
}
void AsyncParamUpdate::logMessage(const String &message)
{
    if (this->mqttLog)
    {
        if (this->mqttClient.connected())
        {
            this->mqttClient.publish(this->logTopic.c_str(), MQTT_QOS_LEVEL, false, message.c_str());
        }
        else
        {
            pendingMessages.push(message);
        }
    }
    else
    {
        this->logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "MAIN", message.c_str());
    }
}
