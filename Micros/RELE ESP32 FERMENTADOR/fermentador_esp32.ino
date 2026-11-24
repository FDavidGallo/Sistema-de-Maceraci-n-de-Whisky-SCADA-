/*
 *  Fermentador (ESP32)  ->  Node-RED por WebSocket Seguro (wss)
 *
 *  Envía (JSON, un mensaje por línea / frame de texto):
 *    { "id":"fermentador", "temperatura_actual":23.4, "sensor_ok":true,
 *      "setpoint":25.0, "histeresis":1.0, "auto_refrigeracion":false,
 *      "bomba_mosto":false, "refrigeracion":false,
 *      "valvula_destilador":false, "recirculado":false }
 *
 *  Recibe del SCADA / Node-RED (cualquier subconjunto de claves):
 *    { "bomba_mosto":true }   { "refrigeracion":0, "valvula_destilador":1 }   (true/false o 1/0)
 *    { "recirculado":true }   -> UNA sola salida que comanda juntas la bomba de recirculado (P302)
 *                                y la válvula de recirculado (XV204/XV302)
 *    { "setpoint":26.5 }      { "histeresis":0.5 }
 *    { "auto_refrigeracion":true }   -> la refrigeración pasa a ON-OFF con el setpoint:
 *                                       ON si T > SP + histéresis, OFF si T < SP - histéresis
 *                                       (con auto activo se ignoran las órdenes manuales a "refrigeracion")
 *    {}                       -> pide el estado
 *
 *  Si no hay WiFi / no se llega a Node-RED, la misma telemetría sale por Serial
 *  (115200). Por Serial también se aceptan los mismos comandos JSON.
 *  Las líneas de log del programa empiezan con "# " para que no se confundan con JSON.
 *
 *  Librerías (Gestor de bibliotecas):
 *    - WebSockets (Markus Sattler / Links2004)  >= 2.4
 *    - ArduinoJson (Benoit Blanchon)            v7
 *    - ModbusMaster (4-20ma)
 */
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <ModbusMaster.h>

#include "redes.h"

// ===================== CONFIGURACIÓN =====================
#define ID_DISPOSITIVO     "fermentador"

#define NODERED_HOST       "trabajofinalintegradoruader.hostlocal.app"   // dominio del servidor (el mismo en todas las redes)
#define NODERED_PUERTO     443
#define NODERED_RUTA       "/ws/fermentador"  // Path del websocket-listener

#define RELE_ACTIVO_ALTO   false              // placa ESP32 Relay X4 (LCTech): lógica INVERTIDA, LOW = relé ON (verificado con PruebaBien.ino)

#define TIMEOUT_WIFI_MS    8000    // por cada red
#define REINTENTO_WIFI_MS  30000   // cada cuánto reintenta si no hay WiFi
#define PERIODO_SENSOR_MS  2000
#define PERIODO_ENVIO_MS   1000

// Pines (relés de la placa: GPIO 23, 5, 4, 13 = relés 1, 2, 3, 4)
#define PIN_RX2        18   // RO del RS485
#define PIN_TX2        19   // DI del RS485
#define PIN_RS485_DIR  32   // DE + RE del RS485
#define SLAVE_ID       1

// ===================== SALIDAS =====================
struct Salida { const char* nombre; uint8_t pin; bool on; };

Salida salidas[] = {
  { "bomba_mosto",        5,  false },   // bomba de ingreso de mosto
  { "refrigeracion",      23, false },   // refrigeración
  { "valvula_destilador", 4,  false },   // bomba de wash (P101 + XV203): fermentador -> destilador
  { "recirculado",        13, false },   // bomba de recirculado + válvula de recirculado (mismo actuador)
};
const size_t N_SALIDAS = sizeof(salidas) / sizeof(salidas[0]);
#define IDX_REFRIG 1   // posición de "refrigeracion" en salidas[]

void escribirSalida(Salida& s, bool on) {
  s.on = on;
  digitalWrite(s.pin, (on == RELE_ACTIVO_ALTO) ? HIGH : LOW);
}

// ===================== ESTADO =====================
ModbusMaster node;
WebSocketsClient ws;

float temperatura = 0;
bool  sensorOk    = false;
float setpoint     = 25.0;   // °C
float histeresis   = 1.0;    // °C
bool  autoRefrig   = false;  // false = refrigeración manual
bool  wsConectado = false;
String lineaSerial;

uint32_t tSensor = 0, tEnvio = 0, tIntentoWiFi = 0;

// ===================== RS485 / Modbus =====================
void preTransmission()  { digitalWrite(PIN_RS485_DIR, HIGH); delayMicroseconds(200); }
void postTransmission() { Serial2.flush(); delayMicroseconds(200); digitalWrite(PIN_RS485_DIR, LOW); }

void leerSensor() {
  uint8_t r = node.readInputRegisters(0x0001, 2);   // temp y humedad (solo usamos temp)
  if (r == node.ku8MBSuccess) {
    temperatura = (int16_t)node.getResponseBuffer(0) / 10.0f;
    sensorOk = true;
  } else {
    sensorOk = false;
    Serial.printf("# Error Modbus: 0x%02X\n", r);
  }
}

// ===================== TELEMETRÍA / COMANDOS =====================
void enviarTelemetria() {
  JsonDocument d;
  d["id"] = ID_DISPOSITIVO;
  if (sensorOk) d["temperatura_actual"] = temperatura;
  else          d["temperatura_actual"] = nullptr;
  d["sensor_ok"] = sensorOk;
  d["setpoint"] = setpoint;
  d["histeresis"] = histeresis;
  d["auto_refrigeracion"] = autoRefrig;
  for (size_t i = 0; i < N_SALIDAS; i++) d[salidas[i].nombre] = salidas[i].on;

  String out;
  serializeJson(d, out);
  if (wsConectado) ws.sendTXT(out);
  else             Serial.println(out);       // sin Node-RED -> por serial
}

bool esNumero(JsonVariant v) { return v.is<float>() || v.is<int>(); }

void procesarComando(const char* txt) {
  JsonDocument d;
  if (deserializeJson(d, txt) || !d.is<JsonObject>()) return;

  JsonVariant v = d["setpoint"];
  if (esNumero(v) && v.as<float>() >= 0 && v.as<float>() <= 100) setpoint = v.as<float>();

  v = d["histeresis"];
  if (esNumero(v) && v.as<float>() >= 0 && v.as<float>() <= 20) histeresis = v.as<float>();

  v = d["auto_refrigeracion"];
  if (!v.isNull()) autoRefrig = v.as<bool>();

  for (size_t i = 0; i < N_SALIDAS; i++) {
    if (i == IDX_REFRIG && autoRefrig) continue;   // en automático manda el setpoint
    v = d[salidas[i].nombre];
    if (!v.isNull()) escribirSalida(salidas[i], v.as<bool>());
  }
  enviarTelemetria();     // confirma el estado real (también sirve {} para pedirlo)
}

// Refrigeración ON-OFF con histéresis. Devuelve true si cambió el relé.
bool controlAutomatico() {
  if (!autoRefrig || !sensorOk) return false;
  Salida& r = salidas[IDX_REFRIG];
  if (!r.on && temperatura > setpoint + histeresis) { escribirSalida(r, true);  return true; }
  if ( r.on && temperatura < setpoint - histeresis) { escribirSalida(r, false); return true; }
  return false;
}

void leerSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      lineaSerial.trim();
      if (lineaSerial.startsWith("{")) procesarComando(lineaSerial.c_str());
      lineaSerial = "";
    } else if (c != '\r' && lineaSerial.length() < 200) {
      lineaSerial += c;
    }
  }
}

// ===================== WIFI =====================
// Par i = diagonal de REDES: celda "SSID:contraseña"
bool parRed(int i, String& ssid, String& clave, bool& abierta) {
  String celda = REDES[i][i];
  if (celda.length() == 0) return false;
  int p = celda.indexOf(':');
  if (p < 0) { ssid = celda; clave = ""; }
  else       { ssid = celda.substring(0, p); clave = celda.substring(p + 1); }
  abierta = (clave.length() == 0);
  return ssid.length() > 0;
}

bool conectarWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  Serial.println("# Buscando redes...");
  int n = WiFi.scanNetworks();               // n < 0 -> falló el scan: se prueba todo igual

  for (int i = 0; i < N_REDES; i++) {
    String ssid, clave; bool abierta;
    if (!parRed(i, ssid, clave, abierta)) continue;

    bool visible = (n < 0);
    for (int k = 0; k < n && !visible; k++) if (WiFi.SSID(k) == ssid) visible = true;
    if (!visible) { Serial.printf("# %s: no está a la vista, la salteo\n", ssid.c_str()); continue; }

    Serial.printf("# Probando %s...\n", ssid.c_str());
    if (abierta) WiFi.begin(ssid.c_str());
    else         WiFi.begin(ssid.c_str(), clave.c_str());

    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < TIMEOUT_WIFI_MS) {
      delay(250);
      if (WiFi.status() == WL_CONNECT_FAILED) break;   // clave incorrecta: siguiente par
    }
    if (WiFi.status() == WL_CONNECTED) {
      WiFi.scanDelete();
      Serial.printf("# WiFi OK: %s  IP %s\n", ssid.c_str(), WiFi.localIP().toString().c_str());
      return true;
    }
    WiFi.disconnect();
  }
  WiFi.scanDelete();
  Serial.println("# Sin WiFi: los datos salen por serial");
  return false;
}

// ===================== WEBSOCKET SEGURO =====================
void eventoWS(WStype_t tipo, uint8_t* payload, size_t len) {
  switch (tipo) {
    case WStype_CONNECTED:
      wsConectado = true;
      Serial.println("# wss conectado a Node-RED");
      enviarTelemetria();
      break;
    case WStype_DISCONNECTED:
      if (wsConectado) Serial.println("# wss desconectado, datos por serial");
      wsConectado = false;
      break;
    case WStype_TEXT:
      procesarComando((const char*)payload);
      break;
    default:
      break;
  }
}

void iniciarWS() {
  Serial.printf("# wss://%s:%d%s\n", NODERED_HOST, NODERED_PUERTO, NODERED_RUTA);
  ws.disconnect();
  ws.beginSSL(NODERED_HOST, NODERED_PUERTO, NODERED_RUTA);   // TLS; certificado a cargo del usuario
}

// ===================== SETUP / LOOP =====================
void setup() {
  Serial.begin(115200);

  for (size_t i = 0; i < N_SALIDAS; i++) {   // todo apagado al arrancar
    digitalWrite(salidas[i].pin, RELE_ACTIVO_ALTO ? LOW : HIGH);   // apagado antes de ser salida (evita pulso al arrancar)
    pinMode(salidas[i].pin, OUTPUT);
    escribirSalida(salidas[i], false);
  }

  pinMode(PIN_RS485_DIR, OUTPUT);
  digitalWrite(PIN_RS485_DIR, LOW);
  Serial2.begin(9600, SERIAL_8N1, PIN_RX2, PIN_TX2);
  node.begin(SLAVE_ID, Serial2);
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);

  ws.onEvent(eventoWS);
  ws.setReconnectInterval(5000);
  ws.enableHeartbeat(15000, 4000, 2);

  tIntentoWiFi = millis();
  if (conectarWiFi()) iniciarWS();
}

void loop() {
  leerSerial();

  if (WiFi.status() == WL_CONNECTED) {
    ws.loop();
  } else {
    wsConectado = false;
    if (millis() - tIntentoWiFi >= REINTENTO_WIFI_MS) {
      tIntentoWiFi = millis();
      if (conectarWiFi()) iniciarWS();
    }
  }

  if (controlAutomatico()) enviarTelemetria();

  uint32_t ahora = millis();
  if (ahora - tSensor >= PERIODO_SENSOR_MS) { tSensor = ahora; leerSensor(); }
  if (ahora - tEnvio  >= PERIODO_ENVIO_MS)  { tEnvio  = ahora; enviarTelemetria(); }
}
