/**
 * MQTT Manager Module
 * Handles WiFi connection, MQTT broker communication,
 * and Home Assistant MQTT Discovery
 */

#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"
#include "types.h"

class MqttManager {
public:
    MqttManager();

    void init();
    void update();

    void publishState(TimerState state);
    void publishRemaining(uint32_t seconds);
    void publishCompleted(uint8_t count);
    void publishSettings(const PomodoroSettings& settings);

    bool isConnected();

private:
    WiFiClient wifiClient;
    PubSubClient mqttClient;

    bool wifiConnecting;
    uint32_t lastReconnectAttempt;
    TimerState lastPublishedState;
    uint32_t lastPublishedRemaining;
    uint8_t lastPublishedCompleted;

    void connectWifi();
    bool connectMqtt();
    void publishDiscovery();
    void publishSensorDiscovery(const char* objectId, const char* name,
                                const char* stateTopic, const char* icon,
                                const char* unitOfMeasurement = nullptr,
                                const char* deviceClass = nullptr);
    void publish(const char* subtopic, const char* payload, bool retain = true);
};

#endif // MQTT_MANAGER_H
