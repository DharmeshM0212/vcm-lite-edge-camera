#include <Arduino_RouterBridge.h>

enum HealthState {
  HEALTH_UNKNOWN = 0,
  HEALTH_GREEN = 1,
  HEALTH_YELLOW = 2,
  HEALTH_RED = 3
};

struct TelemetryState {
  unsigned long frame_id;
  float fps;
  float latency_ms;
  float bitrate_kbps;
  float ai_loss;
  float detector_loss;
  float cpu_percent;
  float ram_mb;
  int queue_depth;
  int dropped_frames;
  HealthState health;
  unsigned long packets_received;
  unsigned long parse_errors;
  unsigned long last_update_ms;
};

TelemetryState telemetry;
String input_line;

float extractFloat(const String& line, const String& key, float fallback) {
  int key_pos = line.indexOf(key);

  if (key_pos < 0) {
    return fallback;
  }

  int colon = line.indexOf(':', key_pos);

  if (colon < 0) {
    return fallback;
  }

  int start = colon + 1;

  while (start < line.length() && line[start] == ' ') {
    start++;
  }

  int end = start;

  while (end < line.length()) {
    char c = line[end];

    if ((c >= '0' && c <= '9') || c == '.' || c == '-') {
      end++;
    } else {
      break;
    }
  }

  if (end <= start) {
    return fallback;
  }

  return line.substring(start, end).toFloat();
}

unsigned long extractUInt(const String& line, const String& key, unsigned long fallback) {
  float value = extractFloat(line, key, (float)fallback);

  if (value < 0.0f) {
    value = 0.0f;
  }

  return (unsigned long)value;
}

HealthState extractHealth(const String& line) {
  if (line.indexOf("\"health\":\"GREEN\"") >= 0) {
    return HEALTH_GREEN;
  }

  if (line.indexOf("\"health\":\"YELLOW\"") >= 0) {
    return HEALTH_YELLOW;
  }

  if (line.indexOf("\"health\":\"RED\"") >= 0) {
    return HEALTH_RED;
  }

  return HEALTH_UNKNOWN;
}

const char* healthName(HealthState health) {
  if (health == HEALTH_GREEN) {
    return "GREEN";
  }

  if (health == HEALTH_YELLOW) {
    return "YELLOW";
  }

  if (health == HEALTH_RED) {
    return "RED";
  }

  return "UNKNOWN";
}

void parseTelemetryLine(const String& line) {
  if (line.indexOf("\"type\":\"vcm_telemetry\"") < 0) {
    telemetry.parse_errors++;
    return;
  }

  HealthState health = extractHealth(line);

  if (health == HEALTH_UNKNOWN) {
    telemetry.parse_errors++;
    return;
  }

  telemetry.frame_id = extractUInt(line, "\"frame_id\"", telemetry.frame_id);
  telemetry.fps = extractFloat(line, "\"fps\"", telemetry.fps);
  telemetry.latency_ms = extractFloat(line, "\"latency_ms\"", telemetry.latency_ms);
  telemetry.bitrate_kbps = extractFloat(line, "\"bitrate_kbps\"", telemetry.bitrate_kbps);
  telemetry.ai_loss = extractFloat(line, "\"ai_loss\"", telemetry.ai_loss);
  telemetry.detector_loss = extractFloat(line, "\"detector_loss\"", telemetry.detector_loss);
  telemetry.queue_depth = (int)extractUInt(line, "\"queue_depth\"", telemetry.queue_depth);
  telemetry.dropped_frames = (int)extractUInt(line, "\"dropped_frames\"", telemetry.dropped_frames);
  telemetry.cpu_percent = extractFloat(line, "\"cpu_percent\"", telemetry.cpu_percent);
  telemetry.ram_mb = extractFloat(line, "\"ram_mb\"", telemetry.ram_mb);
  telemetry.health = health;
  telemetry.packets_received++;
  telemetry.last_update_ms = millis();

  Monitor.print("ACK frame=");
  Monitor.print(telemetry.frame_id);
  Monitor.print(" health=");
  Monitor.print(healthName(telemetry.health));
  Monitor.print(" fps=");
  Monitor.print(telemetry.fps);
  Monitor.print(" latency=");
  Monitor.println(telemetry.latency_ms);
}

void updateLed() {
  unsigned long now = millis();
  unsigned long age = now - telemetry.last_update_ms;

  if (age > 3000 || telemetry.health == HEALTH_UNKNOWN) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
    delay(900);
    return;
  }

  if (telemetry.health == HEALTH_GREEN) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(900);
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
    return;
  }

  if (telemetry.health == HEALTH_YELLOW) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(250);
    digitalWrite(LED_BUILTIN, LOW);
    delay(250);
    return;
  }

  if (telemetry.health == HEALTH_RED) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(80);
    digitalWrite(LED_BUILTIN, LOW);
    delay(80);
    return;
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(LED_BUILTIN, OUTPUT);

  telemetry.frame_id = 0;
  telemetry.fps = 0.0f;
  telemetry.latency_ms = 0.0f;
  telemetry.bitrate_kbps = 0.0f;
  telemetry.ai_loss = 0.0f;
  telemetry.detector_loss = 0.0f;
  telemetry.cpu_percent = 0.0f;
  telemetry.ram_mb = 0.0f;
  telemetry.queue_depth = 0;
  telemetry.dropped_frames = 0;
  telemetry.health = HEALTH_UNKNOWN;
  telemetry.packets_received = 0;
  telemetry.parse_errors = 0;
  telemetry.last_update_ms = 0;

  if (!Bridge.begin()) {
    Serial.println("cannot setup Bridge");
  }

  if (!Monitor.begin()) {
    Serial.println("cannot setup Monitor");
  }

  Monitor.println("VCM telemetry MCU monitor ready");
}

void loop() {
  while (Monitor.available()) {
    char c = (char)Monitor.read();

    if (c == '\n') {
      input_line.trim();

      if (input_line.length() > 0) {
        parseTelemetryLine(input_line);
      }

      input_line = "";
    } else if (c != '\r') {
      if (input_line.length() < 512) {
        input_line += c;
      } else {
        input_line = "";
        telemetry.parse_errors++;
      }
    }
  }

  updateLed();
}