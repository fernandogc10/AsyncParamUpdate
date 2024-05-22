#ifndef LORATOMQTTGATEWAY_H
#define LORATOMQTTGATEWAY_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncMqttClient.h>
#include <WiFi.h>
#include <string>
#include <LoRa.h>

#define SCK 5   // GPIO5  -- SX1276's SCK
#define MISO 19 // GPIO19 -- SX1276's MISO
#define MOSI 27 // GPIO27 -- SX1276's MOSI
#define SS 18   // GPIO18 -- SX1276's CS
#define RST 14  // GPIO14 -- SX1276's RESET
#define DI0 26  // GPIO26 -- SX1276's IRQ(Interrupt Request)
#define BAND 915E6
#define DELAY_MS 5000
#define REGISTRY_TOPIC "boards/registry"
#define MQTT_QOS_LEVEL 2
#define WIFI_EVENT_CONNECTED SYSTEM_EVENT_STA_GOT_IP
#define WIFI_EVENT_DISCONNECTED SYSTEM_EVENT_STA_DISCONNECTED

const char *wifiSSID;
const char *wifiPassword;

AsyncMqttClient mqttClient;
QueueHandle_t loraQueue;

TaskHandle_t wifiGatewayConnectionTask;
TaskHandle_t mqttGatewayConnectionTask;

class LoRaMqttGateway
{
public:
    static void setGateway(const char *wifiID, const char *wifiPass, const char *mqttHost, uint16_t mqttPort, const char *mqttUser, const char *mqttPassword)
    {
        wifiSSID = wifiID;
        wifiPassword = wifiPass;
        configMqttConnection(mqttHost, mqttPort, mqttUser, mqttPassword);
        WiFi.onEvent(WiFiEvent);

        loraQueue = xQueueCreate(10, sizeof(char) * 256);
        if (loraQueue == NULL)
        {
            Serial.println("Error creating the queue");
            while (1)
                ;
        }

        xTaskCreate(reconnectGatewayWifi, "WiFiReconnect", 4096, NULL, 1, &wifiGatewayConnectionTask);
        xTaskCreate(reconnectGatewayMqtt, "MqttReconnect", 4096, NULL, 1, &mqttGatewayConnectionTask);
        xTaskCreate(loraTask, "LoRaTask", 4096, NULL, 1, NULL);
        vTaskSuspend(mqttGatewayConnectionTask);
        initializeLoRaMqttGateway();
    }

    static void publishToMQTT(String message)
    {
        mqttClient.publish(REGISTRY_TOPIC, MQTT_QOS_LEVEL, false, message.c_str());
        Serial.println("Published to MQTT: ");
        Serial.println(message);
    }

    static void onLoRaReceived(int packetSize)
    {

        String message = "";

        while (LoRa.available())
        {
            message += (char)LoRa.read();
        }
        Serial.print("Received packet: ");
        Serial.println(message);

        char *msg = strdup(message.c_str());
        if (xQueueSendFromISR(loraQueue, &msg, NULL) != pdPASS)
        {
            Serial.println("Failed to send to queue");
            free(msg);
        }
    }

    static void publishToLoRa(String message)
    {
        LoRa.beginPacket();
        LoRa.print(message);
        LoRa.endPacket();
        LoRa.receive();
    }

private:
    static void initializeLoRaMqttGateway()
    {
        Serial.println("Initializing SPI...");
        SPI.begin(SCK, MISO, MOSI, SS);

        Serial.println("Setting LoRa pins...");
        LoRa.setPins(SS, RST, DI0);

        Serial.println("Starting LoRa...");
        if (!LoRa.begin(BAND))
        {
            Serial.println("Starting LoRa failed!");
            while (1)
                ;
        }

        Serial.println("Starting LoRa success!");

        Serial.println("Setting LoRa receive callback...");
        LoRa.onReceive(onLoRaReceived);

        Serial.println("LoRa Receiving");
        LoRa.receive();
    }

    static void loraTask(void *pvParameters)
    {
        char *packet;
        while (true)
        {
            if (xQueueReceive(loraQueue, &packet, portMAX_DELAY))
            {

                Serial.println("Processing packet: " + String(packet));
                JsonDocument doc;
                DeserializationError error = deserializeJson(doc, packet);
                if (error)
                {
                    Serial.print("deserializeJson() failed: ");
                    Serial.println(error.c_str());
                    continue;
                }

                if (!doc.containsKey("Device"))
                {
                    Serial.println("Received packet does not contain 'Device' key");
                    continue;
                }

                String deviceName = doc["Device"].as<String>();
                String topic = "boards/" + deviceName;
                mqttClient.subscribe(topic.c_str(), MQTT_QOS_LEVEL);
                publishToMQTT(packet);
                free(packet);
            }
        }
    }

    static void OnGatewayMqttConnect(bool sessionPresent)
    {
        vTaskSuspend(mqttGatewayConnectionTask);
        Serial.println("Connected to Mqtt");
    }

    static void OnGatewayMqttDisconnect(AsyncMqttClientDisconnectReason reason)
    {
        Serial.println("Disconnected from MQTT.");
        if (WiFi.isConnected())
        {
            vTaskResume(mqttGatewayConnectionTask);
        }
    }

    static String extractDeviceName(String topic)
    {
        int pos = topic.indexOf('/');
        return topic.substring(pos + 1);
    }

    static void addDeviceNameToJson(String &jsonMessage, String deviceName)
    {
        JsonDocument doc;
        deserializeJson(doc, jsonMessage);
        doc["Device"] = deviceName;
        serializeJson(doc, jsonMessage);
    }

    static void configMqttConnection(const char *mqttHost, uint16_t mqttPort, const char *mqttUser, const char *mqttPassword)
    {
        mqttClient.onConnect(OnGatewayMqttConnect);
        mqttClient.onDisconnect(OnGatewayMqttDisconnect);
        mqttClient.setServer(mqttHost, mqttPort);
        mqttClient.setCredentials(mqttUser, mqttPassword);
        mqttClient.setClientId("LoRaGatewayDevice");
    }

    static void reconnectGatewayWifi(void *parameters)
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
                WiFi.begin(wifiSSID, wifiPassword);
                vTaskDelay(pdMS_TO_TICKS(DELAY_MS));
            }
        }
    }

    static void reconnectGatewayMqtt(void *parameters)
    {
        for (;;)
        {
            if (!mqttClient.connected())
            {
                mqttClient.connect();
            }

            vTaskDelay(pdMS_TO_TICKS(DELAY_MS));
        }
    }

    static void WiFiEvent(WiFiEvent_t event)
    {
        switch (event)
        {
        case WIFI_EVENT_CONNECTED:
            Serial.println("WiFi connected");
            Serial.print("IP address: ");
            Serial.println(WiFi.localIP().toString());
            vTaskSuspend(wifiGatewayConnectionTask);
            vTaskResume(mqttGatewayConnectionTask);
            break;
        case WIFI_EVENT_DISCONNECTED:
            vTaskSuspend(mqttGatewayConnectionTask);
            vTaskResume(wifiGatewayConnectionTask);
            break;
        }
    }
};

#endif
