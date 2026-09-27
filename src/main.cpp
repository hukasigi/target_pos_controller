#include "SerialConsole.h"
#include "message.h"
#include <Arduino.h>
#include <cstdint>
#include <cstring>
#include <peer_link.h>

const uint8_t   WIFI_CHANNEL = 14;
const peer_id_t FROM_PEER_ID = 0x12;
const peer_id_t TO_PEER_ID   = 0x11;

int16_t target_x      = 0;
int16_t target_y      = 0;
int16_t target_deg    = 0;
bool    using_gamepad = 0;

TabletData now_status;

void processCommand(const char* input);

SerialConsole<32> console(processCommand);

void processCommand(const char* input) {
    if (input == nullptr || input[0] == '\0') {
        return;
    }

    if (strcasecmp(input, "stop") == 0) {
        target_x      = 0;
        target_y      = 0;
        target_deg    = 0;
        using_gamepad = false;

        Serial.println("target reset");
        return;
    }

    if (input[0] == 'm' || input[0] == 'M') {
        // m<target_x>,<target_y>,<target_deg>
        // 例: m100,-50,30

        int tmp_x       = 0;
        int tmp_y       = 0;
        int tmp_deg     = 0;
        int tmp_gamepad = 0;
        if (sscanf(input + 1, " %d%*[ ,]%d%*[ ,]%d%*[ ,]%d", &tmp_x, &tmp_y, &tmp_deg, &tmp_gamepad) != 4) {
            Serial.println("usage: m<target_x>,<target_y>,<target_deg>,<gamepad_status>");
            return;
        }
        target_x      = tmp_x;
        target_y      = tmp_y;
        target_deg    = tmp_deg;
        using_gamepad = tmp_gamepad != 0;

        Serial.printf("target_x=%d target_y=%d target_deg=%d gamepad=%d\n", target_x, target_y, target_deg, using_gamepad);

        return;
    }

    Serial.println("usage: m100,-50,30,1 or stop");
}

void peer_link_recv_cb(const peer_id_t peer_id, const std::vector<Message>& messages) {
    for (const Message& message : messages) {

        if (message.type != static_cast<uint8_t>(MessageType::RobotState)) {
            continue;
        }

        if (message.data.size() != sizeof(TabletData)) {
            Serial.println("invalid position data");
            continue;
        }

        memcpy(&now_status, message.data.data(), sizeof(TabletData));
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
        TabletData target_status;

        target_status.x           = target_x;
        target_status.y           = target_y;
        target_status.deg         = target_deg;
        target_status.gamepad_use = using_gamepad;

        // 構造体をバイト列としてコピー
        std::vector<uint8_t> payload(sizeof(TabletData));

        memcpy(payload.data(), &target_status, sizeof(TabletData));

        Message message;
        message.type = static_cast<uint8_t>(MessageType::RobotState);
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
    Serial.printf("received: x=%d y=%d deg=%d gamepad_use=%d\n", now_status.x, now_status.y, now_status.deg,
                  now_status.gamepad_use);
    delay(10);
}