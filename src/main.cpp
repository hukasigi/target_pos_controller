#include "SerialConsole.h"
#include "cstring"
#include <Arduino.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <peer_link.h>
#include <strings.h>
#include <vector>

struct Position {
        double x;
        double y;
        double deg;

        Position() : x(0.0), y(0.0), deg(0.0) {}
        Position(double x_, double y_, double yaw_) : x(x_), y(y_), deg(yaw_) {}
};

const uint8_t   WIFI_CHANNEL = 14;
const peer_id_t FROM_PEER_ID = 0x12;
const peer_id_t TO_PEER_ID   = 0x11;

volatile Position now_pos;
volatile bool     target_received = false;

const uint8_t POSITION_MESSAGE_TYPE = 0x01;

int     target_x   = 0;
int     target_y   = 0;
int16_t target_deg = 0;

void processCommand(const char* input);

SerialConsole<32> console(processCommand);

void appendInt16(std::vector<uint8_t>& data, int16_t value) {
    const uint16_t v = static_cast<uint16_t>(value);

    data.push_back(static_cast<uint8_t>(v & 0xFF));
    data.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

void processCommand(const char* input) {
    if (input == nullptr || input[0] == '\0') {
        return;
    }

    if (strcasecmp(input, "stop") == 0) {
        target_x   = 0;
        target_y   = 0;
        target_deg = 0;

        Serial.println("target reset");
        return;
    }

    if (input[0] == 'm' || input[0] == 'M') {
        // m<target_x>,<target_y>,<target_deg>
        // 例: m100,-50,30
        double x   = 0.0;
        double y   = 0.0;
        double deg = 0.0;

        if (sscanf(input + 1, " %lf%*[ ,]%lf%*[ ,]%lf", &x, &y, &deg) != 3) {
            Serial.println("usage: m<target_x>,<target_y>,<target_deg>");
            return;
        }

        target_x   = static_cast<int16_t>(lround(x));
        target_y   = static_cast<int16_t>(lround(y));
        target_deg = static_cast<int16_t>(lround(deg));

        Serial.printf("target_x=%d target_y=%d target_deg=%d\n", target_x, target_y, target_deg);

        return;
    }

    Serial.println("usage: m100,-50,30 or stop");
}

void peer_link_recv_cb(const peer_id_t peer_id, const std::vector<Message>& messages) {

    for (const Message& message : messages) {
        if (message.type != POSITION_MESSAGE_TYPE || message.data.size() != sizeof(Position)) {
            continue;
        }

        Position received_position;

        const void* raw_data = static_cast<const void*>(message.data.data());

        std::memcpy(&received_position, raw_data, sizeof(Position));

        now_pos.x       = received_position.x;
        now_pos.y       = received_position.y;
        now_pos.deg     = received_position.deg;
        target_received = true;

        Serial.printf("from 0x%02X: x=%.3f y=%.3f deg=%.3f\n", peer_id, now_pos.x, now_pos.y, now_pos.deg);
    }
}

// 送信先の情報を保持する構造体
// ブロードキャスト宛先を入れるために使っている

esp_now_peer_info_t slave;

void setup() {
    Serial.begin(115200);
    peer_link_task_init(WIFI_CHANNEL, FROM_PEER_ID);
}

void loop() {
    // SerialConsole を使用
    console.handleInput();

    if (peer_link_is_peer_exist(TO_PEER_ID)) {
        std::vector<uint8_t> payload;
        payload.reserve(6);

        appendInt16(payload, target_x);
        appendInt16(payload, target_y);
        appendInt16(payload, target_deg);

        Message message;
        message.type = POSITION_MESSAGE_TYPE;
        message.data = payload;

        std::vector<Message> messages;
        messages.push_back(message);

        const esp_err_t result = peer_link_send(TO_PEER_ID, messages);

        if (result != ESP_OK) {
            Serial.printf("send error: %d\n", result);
        }
    } else {
        Serial.println("target peer not found");
    }
    delay(1000);
}
