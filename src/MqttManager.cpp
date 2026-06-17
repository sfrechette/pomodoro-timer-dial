/**
 * MQTT Manager Implementation
 * Non-blocking WiFi/MQTT with Home Assistant Discovery
 */

#include "MqttManager.h"

MqttManager::MqttManager()
    : mqttClient(wifiClient),
      wifiConnecting(false),
      lastReconnectAttempt(0),
      lastPublishedState(STATE_SETTINGS),
      lastPublishedRemaining(UINT32_MAX),
      lastPublishedCompleted(UINT8_MAX) {
}

void MqttManager::init() {
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setBufferSize(512);
    connectWifi();
}

void MqttManager::update() {
    if (WiFi.status() != WL_CONNECTED) {
        uint32_t now = millis();
        if (now - lastReconnectAttempt >= MQTT_RECONNECT_INTERVAL_MS) {
            lastReconnectAttempt = now;
            connectWifi();
        }
        return;
    }

    if (!mqttClient.connected()) {
        uint32_t now = millis();
        if (now - lastReconnectAttempt >= MQTT_RECONNECT_INTERVAL_MS) {
            lastReconnectAttempt = now;
            if (connectMqtt()) {
                publishDiscovery();
                publish("status", "online");
            }
        }
    }

    mqttClient.loop();
}

bool MqttManager::isConnected() {
    return mqttClient.connected();
}

// ==================== WiFi ====================

void MqttManager::connectWifi() {
    if (WiFi.status() == WL_CONNECTED) return;

    Serial.print("WiFi: connecting to ");
    Serial.println(WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // Brief blocking wait (up to 3s) for initial connection only;
    // subsequent reconnects go through the non-blocking path in update()
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 3000) {
        delay(100);
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("WiFi: connected, IP ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("WiFi: connection pending (will retry in background)");
    }
}

// ==================== MQTT ====================

bool MqttManager::connectMqtt() {
    Serial.print("MQTT: connecting to ");
    Serial.println(MQTT_SERVER);

    char willTopic[64];
    snprintf(willTopic, sizeof(willTopic), "%s/status", MQTT_BASE_TOPIC);

    bool ok;
    if (strlen(MQTT_USER) > 0) {
        ok = mqttClient.connect(MQTT_DEVICE_ID, MQTT_USER, MQTT_PASSWORD,
                                willTopic, 1, true, "offline");
    } else {
        ok = mqttClient.connect(MQTT_DEVICE_ID,
                                willTopic, 1, true, "offline");
    }

    if (ok) {
        Serial.println("MQTT: connected");
    } else {
        Serial.print("MQTT: failed, rc=");
        Serial.println(mqttClient.state());
    }
    return ok;
}

// ==================== Publishing ====================

void MqttManager::publish(const char* subtopic, const char* payload, bool retain) {
    if (!mqttClient.connected()) return;

    char topic[96];
    snprintf(topic, sizeof(topic), "%s/%s", MQTT_BASE_TOPIC, subtopic);
    mqttClient.publish(topic, payload, retain);
}

static const char* stateToString(TimerState state) {
    switch (state) {
        case STATE_IDLE:        return "idle";
        case STATE_RUNNING:     return "working";
        case STATE_PAUSED:      return "paused";
        case STATE_SHORT_BREAK: return "short_break";
        case STATE_LONG_BREAK:  return "long_break";
        case STATE_SETTINGS:    return "settings";
        default:                return "unknown";
    }
}

void MqttManager::publishState(TimerState state) {
    if (state == lastPublishedState) return;
    lastPublishedState = state;
    publish("state", stateToString(state));
    Serial.print("MQTT pub state: ");
    Serial.println(stateToString(state));
}

void MqttManager::publishRemaining(uint32_t seconds) {
    if (seconds == lastPublishedRemaining) return;
    lastPublishedRemaining = seconds;
    char buf[12];
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)seconds);
    publish("remaining", buf, false);
}

void MqttManager::publishCompleted(uint8_t count) {
    if (count == lastPublishedCompleted) return;
    lastPublishedCompleted = count;
    char buf[6];
    snprintf(buf, sizeof(buf), "%u", count);
    publish("completed", buf);
}

void MqttManager::publishSettings(const PomodoroSettings& settings) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%u", settings.workDuration / 60);
    publish("work_duration", buf);
    snprintf(buf, sizeof(buf), "%u", settings.shortBreakDuration / 60);
    publish("short_break_duration", buf);
    snprintf(buf, sizeof(buf), "%u", settings.longBreakDuration / 60);
    publish("long_break_duration", buf);
}

// ==================== HA MQTT Discovery ====================

void MqttManager::publishSensorDiscovery(const char* objectId, const char* name,
                                          const char* stateTopic, const char* icon,
                                          const char* unitOfMeasurement,
                                          const char* deviceClass) {
    char configTopic[128];
    snprintf(configTopic, sizeof(configTopic),
             "homeassistant/sensor/%s/%s/config", MQTT_DEVICE_ID, objectId);

    char availTopic[64];
    snprintf(availTopic, sizeof(availTopic), "%s/status", MQTT_BASE_TOPIC);

    char fullStateTopic[64];
    snprintf(fullStateTopic, sizeof(fullStateTopic), "%s/%s", MQTT_BASE_TOPIC, stateTopic);

    char payload[480];
    int len = snprintf(payload, sizeof(payload),
        "{"
        "\"name\":\"%s\","
        "\"stat_t\":\"%s\","
        "\"avty_t\":\"%s\","
        "\"uniq_id\":\"%s_%s\","
        "\"ic\":\"%s\","
        "\"dev\":{"
            "\"ids\":[\"%s\"],"
            "\"name\":\"Pomodoro Timer\","
            "\"mf\":\"M5Stack\","
            "\"mdl\":\"Dial\""
        "}",
        name, fullStateTopic, availTopic, MQTT_DEVICE_ID, objectId, icon,
        MQTT_DEVICE_ID);

    if (unitOfMeasurement && strlen(unitOfMeasurement) > 0) {
        len += snprintf(payload + len, sizeof(payload) - len,
                        ",\"unit_of_meas\":\"%s\"", unitOfMeasurement);
    }
    if (deviceClass && strlen(deviceClass) > 0) {
        len += snprintf(payload + len, sizeof(payload) - len,
                        ",\"dev_cla\":\"%s\"", deviceClass);
    }
    snprintf(payload + len, sizeof(payload) - len, "}");

    mqttClient.publish(configTopic, payload, true);
    Serial.print("MQTT discovery: ");
    Serial.println(objectId);
}

void MqttManager::publishDiscovery() {
    publishSensorDiscovery("state",       "State",          "state",
                           "mdi:timer-sand");

    publishSensorDiscovery("remaining",   "Time Remaining", "remaining",
                           "mdi:clock-outline",  "s", "duration");

    publishSensorDiscovery("completed",   "Completed",      "completed",
                           "mdi:check-circle-outline");

    publishSensorDiscovery("work_dur",    "Work Duration",  "work_duration",
                           "mdi:briefcase-clock-outline", "min");

    publishSensorDiscovery("short_brk",   "Short Break",    "short_break_duration",
                           "mdi:coffee-outline", "min");

    publishSensorDiscovery("long_brk",    "Long Break",     "long_break_duration",
                           "mdi:sleep", "min");

    Serial.println("MQTT: discovery configs published");
}
