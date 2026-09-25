#include "SerialConsole.h"
#include <Arduino.h>
#include <cstdint>
#include <cstring>
#include <peer_link.h>

struct Position {
        int16_t x;
        int16_t y;
        int16_t deg;
};

const uint8_t   WIFI_CHANNEL = 14;
const peer_id_t FROM_PEER_ID = 0x12;
const peer_id_t TO_PEER_ID   = 0x11;

const uint8_t POSITION_MESSAGE_TYPE = 0x01;

int16_t target_x   = 0;
int16_t target_y   = 0;
int16_t target_deg = 0;

Position now_pos;

void processCommand(const char* input);

SerialConsole<32> console(processCommand);

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

        if (message.type != POSITION_MESSAGE_TYPE) {
            continue;
        }

        if (message.data.size() != sizeof(Position)) {
            Serial.println("invalid position data");
            continue;
        }

        memcpy(&now_pos, message.data.data(), sizeof(Position));
    }
}

void setup() {
    Serial.begin(115200);

    peer_link_task_init(WIFI_CHANNEL, FROM_PEER_ID);
}

void loop() {
    console.handleInput();

    if (peer_link_is_peer_exist(TO_PEER_ID)) {

        // 送信する構造体
        Position position;

        position.x   = target_x;
        position.y   = target_y;
        position.deg = target_deg;

        // 構造体をバイト列としてコピー
        std::vector<uint8_t> payload(sizeof(Position));

        memcpy(payload.data(), &position, sizeof(Position));

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

    Serial.printf("x=%d y=%d deg=%d\n", now_pos.x, now_pos.y, now_pos.deg);
    delay(1000);
}