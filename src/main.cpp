#include "SerialConsole.h"
#include "message.h"
#include <Arduino.h>
#include <cstdint>
#include <cstring>
#include <peer_link.h>

const uint8_t   WIFI_CHANNEL = 14;
const peer_id_t FROM_PEER_ID = 0x12;
const peer_id_t TO_PEER_ID   = 0x11;

TabletData_Pos target_pos;
TabletOrder    target_order;
TabletData_Pos now_pos;
StateData      now_state;

void processCommand(const char* input);

SerialConsole<32> console(processCommand);

void processCommand(const char* command) {
    String input = command;
    input.trim();

    if (input.length() == 0) {
        return;
    }

    if (input == "s") {
        target_pos.x   = 0;
        target_pos.y   = 0;
        target_pos.deg = 0;

        target_order.gamepad_use                  = false;
        target_order.belt_launch                  = false;
        target_order.roller_launch                = false;
        target_order.floor_collection_open        = false;
        target_order.bucket_collection_open       = false;
        target_order.belt_launch_deg              = 0;
        target_order.bucket_collection_height     = 0;
        target_order.bucket_collection_front_back = 0;

        Serial.println("reset");
        return;
    }

    if (input.startsWith("m")) {
        String data = input.substring(1);
        data.trim();

        // 文字列のどこから始めればいいか
        int start = 0;
        // 切り出した数値入れる
        int values[3];
        // 値が更新されたか
        bool updated[3] = {false, false, false};

        for (int i = 0; i < 3; i++) {
            int comma = data.indexOf(',', start);

            String value;

            // カンマを基準に文字列を切り出す
            if (comma >= 0) {
                value = data.substring(start, comma);
                start = comma + 1;
            } else {
                value = data.substring(start);
                start = data.length();
            }

            // 前後の空白を消して、数値に変換する
            value.trim();
            if (value.length() > 0) {
                values[i]  = value.toInt();
                updated[i] = true;
            }

            // 文字列の最後まで来たらループ抜ける
            if (start >= data.length()) {
                break;
            }
        }

        if (updated[0]) {
            target_pos.x = values[0];
        }

        if (updated[1]) {
            target_pos.y = values[1];
        }

        if (updated[2]) {
            target_pos.deg = values[2];
        }

        Serial.printf("target: x=%d y=%d deg=%d \n", target_pos.x, target_pos.y, target_pos.deg);

        return;
    }

    if (input.startsWith("o")) {
        String data = input.substring(1);
        data.trim();

        int start = 0;

        bool* bool_values[] = {&target_order.gamepad_use, &target_order.belt_launch, &target_order.roller_launch,
                               &target_order.floor_collection_open, &target_order.bucket_collection_open};

        for (int i = 0; i < 5; i++) {
            int space = data.indexOf(' ', start);

            String value;

            if (space >= 0) {
                value = data.substring(start, space);
                start = space + 1;
            } else {
                value = data.substring(start);
                start = data.length();
            }

            value.trim();

            if (value.length() > 0) {
                *bool_values[i] = (value.toInt() != 0);
            }

            if (start >= data.length()) {
                break;
            }
        }

        Serial.printf("order: gamepad=%d belt=%d roller=%d floor=%d bucket=%d\n", target_order.gamepad_use,
                      target_order.belt_launch, target_order.roller_launch, target_order.floor_collection_open,
                      target_order.bucket_collection_open);

        // 0: gamepad_use
        // 1: belt_launch
        // 2: roller_launch
        // 3: floor_collection_open
        // 4: bucket_collection_open
        return;
    }

    Serial.println("unknown command");
}

void peer_link_recv_cb(const peer_id_t peer_id, const std::vector<Message>& messages) {
    for (const Message& message : messages) {

        if (message.type == static_cast<uint8_t>(MessageType::TabletPos)) {

            if (message.data.size() != sizeof(TabletData_Pos)) {
                Serial.println("invalid position data");
                continue;
            }

            memcpy(&now_pos, message.data.data(), sizeof(TabletData_Pos));

            Serial.printf("now: x=%d y=%d deg=%d", now_pos.x, now_pos.y, now_pos.deg);

        } else if (message.type == static_cast<uint8_t>(MessageType::RobotState)) {

            if (message.data.size() != sizeof(StateData)) {
                Serial.println("invalid state data");
                continue;
            }

            memcpy(&now_state, message.data.data(), sizeof(StateData));

            Serial.printf("received: gamepad=%d belt=%d roller=%d floor=%d belt_deg=%d height=%u front_back=%u\n",
                          now_state.gamepad_used, now_state.is_launch_belt, now_state.is_launch_roller,
                          now_state.floor_rag_collection, now_state.belt_launch_deg, now_state.Bucket_collection_height,
                          now_state.Bucket_collection_front_back);
        }
    }
}

void setup() {
    Serial.begin(115200);

    memset(&target_pos, 0, sizeof(target_pos));
    memset(&target_order, 0, sizeof(target_order));
    memset(&now_pos, 0, sizeof(now_pos));
    memset(&now_state, 0, sizeof(now_state));

    peer_link_task_init(WIFI_CHANNEL, FROM_PEER_ID);
}

void loop() {
    console.handleInput();

    if (peer_link_is_peer_exist(TO_PEER_ID)) {

        std::vector<Message> messages;

        Message pos_message;
        pos_message.type = static_cast<uint8_t>(MessageType::TabletPos);
        pos_message.data.resize(sizeof(TabletData_Pos));

        memcpy(pos_message.data.data(), &target_pos, sizeof(TabletData_Pos));

        messages.push_back(pos_message);

        Message order_message;
        order_message.type = static_cast<uint8_t>(MessageType::TabletOrder);
        order_message.data.resize(sizeof(TabletOrder));

        memcpy(order_message.data.data(), &target_order, sizeof(TabletOrder));

        messages.push_back(order_message);

        const esp_err_t result = peer_link_send(TO_PEER_ID, messages);

        if (result != ESP_OK) {
            Serial.printf("send error: %d\n", result);
        }

    } else {
        Serial.println("target peer not found");
    }

    delay(10);
}