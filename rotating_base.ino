#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Stepper.h>
#include <Preferences.h>

#define STEPS 2038
#define SPEED_MIN 3
#define SPEED_MAX 20
#define SPEED_DEFAULT 7

Stepper stepper(STEPS, 3, 1, 2, 0);
Preferences preferences;

const char* ssid = "***";
const char* password = "***";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

int speedValue;
int direction;

void loadPreferences() {
    preferences.begin("stepper", false);
    speedValue = preferences.getInt("speed", SPEED_DEFAULT);
    direction = preferences.getInt("direction", 1);
    preferences.end();
}

void savePreferences() {
    preferences.begin("stepper", false);
    preferences.putInt("speed", speedValue);
    preferences.putInt("direction", direction);
    preferences.end();
}

void notifyClients() {
    String message = String(speedValue) + "," + String(direction);
    ws.textAll(message);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        String message = "";
        for (size_t i = 0; i < len; i++) {
            message += (char)data[i];
        }
        
        int commaIndex = message.indexOf(',');
        if (commaIndex != -1) {
            speedValue = constrain(message.substring(0, commaIndex).toInt(), SPEED_MIN, SPEED_MAX);
            direction = message.substring(commaIndex + 1).toInt();
            stepper.setSpeed(speedValue);
            savePreferences();
        }
    }
}

void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
                      void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        client->text(String(speedValue) + "," + String(direction));
    } else if (type == WS_EVT_DATA) {
        handleWebSocketMessage(arg, data, len);
    }
}

void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("WiFi connected.");
    Serial.println(WiFi.localIP());

    loadPreferences();
    stepper.setSpeed(speedValue);

    ws.onEvent(onWebSocketEvent);
    server.addHandler(&ws);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Rotating Base</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; text-align: center; padding: 20px; }
        input[type=range] { width: 80%; }
        label, span { font-size: 18px; }
    </style>
    <script>
        var socket;
        function init() {
            socket = new WebSocket("ws://" + window.location.host + "/ws");
            socket.onmessage = function(event) {
                var values = event.data.split(",");
                document.getElementById("speed").value = values[0];
                document.getElementById("direction").checked = (values[1] == "-1");
                document.getElementById("speedValue").innerText = values[0];
            };
        }
        function updateStepper() {
            var speed = document.getElementById("speed").value;
            var direction = document.getElementById("direction").checked ? -1 : 1;
            socket.send(speed + "," + direction);
            document.getElementById("speedValue").innerText = speed;
        }
    </script>
</head>
<body onload="init()">
    <h1>Rotating Base by Xoid</h1>
    <label>Speed: <span id="speedValue">7</span></label><br>
    <input type="range" id="speed" min="3" max="20" value="7" oninput="updateStepper()"><br><br>
    <label>Reverse</label>
    <input type="checkbox" id="direction" onchange="updateStepper()">
</body>
</html>
)rawliteral");
    });

    server.begin();
}

void loop() {
    ws.cleanupClients();
    stepper.step(direction * STEPS / 100);
}
