#include "AsyncParamUpdate.h"

AsyncParamUpdate *AsyncParamUpdate::instance = nullptr;

AsyncParamUpdate::AsyncParamUpdate(const char *wifiSSID, const char *wifiPassword, const char *mqttHost, uint16_t mqttPort, const char *mqttUser, const char *mqttPassword, const char *deviceName, bool mqttLog)
{

    instance = this;
    this->wifiSSID = wifiSSID;
    this->wifiPassword = wifiPassword;
    this->deviceName = deviceName;
    this->logTopic = BOARDS_PREFIX + this->deviceName + LOG_SUFFIX;
    this->updateTopic = BOARDS_PREFIX + String(this->deviceName);
    this->confirmationTopic = this->updateTopic + CONFIRMATION_SUFFIX;
    this->mqttHost = mqttHost;
    this->mqttPort = mqttPort;
    this->mqttUser = mqttUser;
    this->mqttPassword = mqttPassword;
    this->mqttLog = mqttLog;
    this->useLoRa = false;

    InitMqtt();
    WiFi.onEvent(AsyncParamUpdate::WiFiEvent);

    xTaskCreate(reconnectWifi, "WiFiReconnect", 4096, NULL, 1, &wifiConnectionTask);
    xTaskCreate(reconnectMqtt, "MqttReconnect", 4096, NULL, 1, &mqttConnectionTask);
    vTaskSuspend(mqttConnectionTask);
    xTaskCreate(sendActiveMessage, "ActiveMessage", 4096, NULL, 1, NULL);
}

AsyncParamUpdate::AsyncParamUpdate(const char *deviceName, bool mqttLog)
{
    instance = this;

    this->deviceName = deviceName;
    this->mqttLog = mqttLog;
    this->useLoRa = true;

    this->initializeLoRa();
}

void AsyncParamUpdate::initializeLoRa()
{

    SPI.begin(SCK, MISO, MOSI, SS);
    LoRa.setPins(SS, RST, DI0);

    if (!LoRa.begin(BAND))
    {
        Serial.println("Starting LoRa failed!");
        while (1)
            ;
    }

    Serial.println("Starting LoRa success!");

    LoRa.onReceive(OnLoRaReceived);
    LoRa.receive();
    // LoRa.onTxDone(onTxDone);
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

void AsyncParamUpdate::sendActiveMessage(void *parameters)
{

    vTaskDelay(pdMS_TO_TICKS(30000));
    for (;;)
    {
        JsonDocument doc;
        doc["Device"] = instance->deviceName;
        doc["status"] = "active";

        char jsonBuffer[JSON_BUFFER_SIZE];
        serializeJson(doc, jsonBuffer);

        instance->mqttClient.publish(instance->updateTopic.c_str(), MQTT_QOS_LEVEL, true, jsonBuffer);

        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

void AsyncParamUpdate::OnMqttConnect(bool sessionPresent)
{

    vTaskSuspend(instance->mqttConnectionTask);
    instance->mqttClient.subscribe(instance->updateTopic.c_str(), MQTT_QOS_LEVEL);

    if (instance->mqttLog)
    {

        while (!instance->pendingMessages.empty())
        {

            String pendingMessage = instance->pendingMessages.front();
            instance->mqttClient.publish(instance->logTopic.c_str(), MQTT_QOS_LEVEL, false, pendingMessage.c_str());
            instance->pendingMessages.pop();
        }
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
        instance->logMessage("Message received deserializeJson() failed with code " + String(error.c_str()));
        return;
    }

    if (!doc.containsKey("parameters"))
    {
        return;
    }

    JsonObject params = doc["parameters"].as<JsonObject>();
    String messageId = doc["id"].as<String>();
    bool allParamsUpdated = true;
    std::map<String, JsonVariant> oldValues;

    for (JsonPair kv : params)
    {
        String key = kv.key().c_str();
        instance->logMessage(key.c_str());

        auto paramIter = instance->params.find(key.c_str());
        if (paramIter != instance->params.end())
        {
            ParamInfo &paramInfo = paramIter->second;
            JsonVariant oldValue = *static_cast<JsonVariant *>(paramInfo.param);
            oldValues[key] = oldValue;
            bool updateSuccess = instance->updateParameter(paramInfo, kv.value());

            if (!updateSuccess)
            {
                allParamsUpdated = false;
                *static_cast<JsonVariant *>(paramInfo.param) = oldValue;
                break;
            }
        }
    }

    JsonDocument ackDoc;
    ackDoc["id"] = messageId;
    ackDoc["Device"] = instance->deviceName;
    ackDoc["status"] = allParamsUpdated ? "updated" : "failed";

    char jsonBuffer[JSON_BUFFER_SIZE];
    serializeJson(ackDoc, jsonBuffer);
    instance->mqttClient.publish(instance->confirmationTopic.c_str(), MQTT_QOS_LEVEL, true, jsonBuffer);

    if (!allParamsUpdated)
    {
        instance->logMessage("Error updating parameters");
    }
}

bool AsyncParamUpdate::updateParameter(const ParamInfo &paramInfo, JsonVariant value)
{
    const std::string &paramName = paramInfo.paramName;
    bool success = false;

    try
    {

        if (strcmp(paramInfo.typeName, typeid(int).name()) == 0)
        {
            *(static_cast<int *>(paramInfo.param)) = value.as<int>();
            success = preferences.putInt(paramName.c_str(), value.as<int>());
        }
        else if (strcmp(paramInfo.typeName, typeid(float).name()) == 0)
        {
            *(static_cast<float *>(paramInfo.param)) = value.as<float>();
            success = preferences.putFloat(paramName.c_str(), value.as<float>());
        }
        else if (strcmp(paramInfo.typeName, typeid(double).name()) == 0)
        {
            *(static_cast<double *>(paramInfo.param)) = value.as<double>();
            success = preferences.putDouble(paramName.c_str(), value.as<double>());
        }
        else if (strcmp(paramInfo.typeName, typeid(bool).name()) == 0)
        {
            *(static_cast<bool *>(paramInfo.param)) = value.as<bool>();
            success = preferences.putBool(paramName.c_str(), value.as<bool>());
        }
        else if (strcmp(paramInfo.typeName, typeid(String).name()) == 0)
        {
            *(static_cast<String *>(paramInfo.param)) = value.as<String>();
            success = preferences.putString(paramName.c_str(), value.as<String>().c_str());
        }
        else
        {
            logMessage("Error: Type mismatch or unsupported type.");
        }
    }
    catch (const std::exception &e)
    {
        logMessage("Exception caught: " + String(e.what()));
        success = false;
    }

    return success;
}

void AsyncParamUpdate::OnLoRaReceived(int packetSize)
{
    String receivedMessage;
    while (LoRa.available())
    {
        receivedMessage += (char)LoRa.read();
    }

    // Procesar el mensaje recibido
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, receivedMessage);
    if (error)
    {
        instance->logMessage("LoRa message deserializeJson() failed with code " + String(error.c_str()));
        return;
    }

    String deviceName = doc["Device"].as<String>();
    if (deviceName != instance->deviceName)
    {
        instance->logMessage("Received message is not for this device, discarding.");
        return;
    }

    if (!doc.containsKey("parameters"))
    {
        return;
    }

    JsonObject params = doc["parameters"].as<JsonObject>();
    String messageId = doc["id"].as<String>();
    bool allParamsUpdated = true;
    std::map<String, JsonVariant> oldValues;

    for (JsonPair kv : params)
    {
        String key = kv.key().c_str();
        instance->logMessage(key.c_str());

        auto paramIter = instance->params.find(key.c_str());
        if (paramIter != instance->params.end())
        {
            ParamInfo &paramInfo = paramIter->second;
            JsonVariant oldValue = *static_cast<JsonVariant *>(paramInfo.param);
            oldValues[key] = oldValue;
            bool updateSuccess = instance->updateParameter(paramInfo, kv.value());

            if (!updateSuccess)
            {
                allParamsUpdated = false;
                *static_cast<JsonVariant *>(paramInfo.param) = oldValue;
                break;
            }
        }
    }

    JsonDocument ackDoc;
    ackDoc["id"] = messageId;
    ackDoc["Device"] = instance->deviceName;
    ackDoc["status"] = allParamsUpdated ? "updated" : "failed";

    char jsonBuffer[JSON_BUFFER_SIZE];
    serializeJson(ackDoc, jsonBuffer);

    // Envía una respuesta a través de LoRa
    LoRa.beginPacket();
    LoRa.print(jsonBuffer);
    LoRa.endPacket();

    if (!allParamsUpdated)
    {
        instance->logMessage("Error updating parameters");
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
    mqttClient.setSecure(MQTT_SECURE);
}

void AsyncParamUpdate::publishParametersList(std::string paramName)
{

    if (!useLoRa)
    {
        while (!mqttClient.connected())
        {
        }
    }

    JsonDocument doc;
    doc["Device"] = deviceName;
    doc["Ip"] = WiFi.localIP().toString();
    JsonArray paramsArray = doc["parameters"].to<JsonArray>();

    for (const auto &p : params)
    {
        JsonObject paramObj = paramsArray.add<JsonObject>();

        std::string paramValue;

        if (p.second.typeName == typeid(int).name())
        {
            paramValue = std::to_string(*static_cast<int *>(p.second.param));
        }
        else if (p.second.typeName == typeid(float).name())
        {
            paramValue = std::to_string(*static_cast<float *>(p.second.param));
        }
        else if (p.second.typeName == typeid(double).name())
        {
            paramValue = std::to_string(*static_cast<double *>(p.second.param));
        }
        else if (p.second.typeName == typeid(bool).name())
        {
            paramValue = *static_cast<bool *>(p.second.param) ? "true" : "false";
        }
        else if (p.second.typeName == typeid(std::string).name())
        {
            paramValue = *static_cast<std::string *>(p.second.param);
        }
        else
        {
            continue;
        }

        paramObj[p.first.c_str()] = paramValue;
    }

    char jsonBuffer[JSON_BUFFER_SIZE];
    serializeJson(doc, jsonBuffer);

    if (useLoRa)
    {
        // Send via LoRa
        LoRa.beginPacket();
        LoRa.print(jsonBuffer);
        LoRa.endPacket();
    }
    else
    {
        // Send via MQTT
        mqttClient.publish(REGISTRY_TOPIC, MQTT_QOS_LEVEL, true, jsonBuffer);
    }
}

void AsyncParamUpdate::onTxDone()
{

    instance->logMessage("Datos enviados");
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
