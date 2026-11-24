/*
 *  Tanques (ESP8266)  ->  Node-RED por WebSocket Seguro (wss)
 *
 *  SOLO SENSORES: este micro no tiene actuadores (refrigeración, bomba de recirculado y
 *  válvula de recirculado se comandan desde el ESP32 del fermentador).
 *
 *  Mide:
 *    - Nivel del FERMENTADOR  : potenciómetro en A0, escala 0..55 L
 *    - Tanque de RECEPCIÓN    : celda de carga + HX711 en D5 (DOUT) / D6 (SCK), 65 L
 *    - Tanque de RECIRCULADO  : celda de carga + HX711 en D7 (DOUT) / D8 (SCK), 10 L
 *
 *  Envía (JSON, un mensaje por línea / frame de texto), cada 1 s:
 *    { "id":"tanques",
 *      "fermentador_litros":23.4, "fermentador_adc":436,
 *      "recepcion_litros":12.3,   "recepcion_ok":true,  "recepcion_raw":123456,
 *      "recirculado_litros":3.1,  "recirculado_ok":true, "recirculado_raw":98765,
 *      "evento":"tara recepcion OK" }          <- "evento" solo aparece ~20 s tras un comando
 *    (si una celda no responde: "xxx_litros":null y "xxx_ok":false)
 *
 *  Recibe del SCADA / Node-RED (cualquier subconjunto de claves):
 *    { "tara":"recepcion" }                       -> tanque VACÍO: guarda el cero
 *    { "tara":"recirculado" }
 *    { "cal":{"tanque":"recepcion","litros":5} }  -> con 5 L reales adentro: guarda la escala
 *    { "pote":"min" }  { "pote":"max" }           -> pote en el nivel vacío / lleno: ajusta 0 y 55 L
 *    {}                                           -> pide el estado
 *  La tara, la escala y el ajuste del pote quedan guardados en la EEPROM (sobreviven al reinicio).
 *
 *  Si no hay WiFi / no se llega a Node-RED, la misma telemetría sale por Serial
 *  (115200). Por Serial también se aceptan los mismos comandos JSON.
 *  Las líneas de log del programa empiezan con "# " para que no se confundan con JSON.
 *
 *  Librerías (Gestor de bibliotecas):
 *    - WebSockets (Markus Sattler / Links2004)  >= 2.4
 *    - ArduinoJson (Benoit Blanchon)            v7
 *    - HX711 (Bogdan Necula / bogde)
 *  Placa: NodeMCU 1.0 / Adafruit Huzzah (los pines van por número de GPIO, valen para ambas).
 *
 *  Conexiones:
 *    Pote:  extremos a 3V3 y GND, cursor a A0. En NodeMCU el divisor de A0 ya viene en la placa.
 *           En placas SIN divisor (Adafruit Huzzah, ESP-12 pelado) A0 admite solo 0-1 V: poner
 *           cursor -> 220k -> A0 y 100k de A0 a GND (así 3,3 V en el cursor = 1 V en A0).
 *    HX711 de recepción:  VCC 3V3, GND, DT -> D5, SCK -> D6.
 *    HX711 de recirculado: VCC 3V3, GND, DT -> D7, SCK -> D8.
 */
#include <ESP8266WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <HX711.h>
#include <EEPROM.h>

#include "redes.h"

// ===================== CONFIGURACIÓN =====================
#define ID_DISPOSITIVO     "tanques"

#define NODERED_HOST      "trabajofinalintegradoruader.hostlocal.app"   // dominio del servidor (el mismo en todas las redes)
#define NODERED_PUERTO     443
#define NODERED_RUTA       "/ws/tanques"      // Path del websocket-listener

#define TIMEOUT_WIFI_MS    8000    // por cada red
#define REINTENTO_WIFI_MS  30000   // cada cuánto reintenta si no hay WiFi
#define PERIODO_POTE_MS    100
#define PERIODO_ENVIO_MS   1000

// Fermentador (pote en A0)
#define CAP_FERMENTADOR    55.0f   // L a fondo de escala
#define ADC_MIN_DEF        0       // valor de A0 con el pote en vacío (se ajusta con {"pote":"min"})
#define ADC_MAX_DEF        1023    // valor de A0 con el pote en lleno  (se ajusta con {"pote":"max"})

// Celdas de carga (GPIO del ESP8266: D5=14, D6=12, D7=13, D8=15)
#define HX_REC_DOUT        14      // D5
#define HX_REC_SCK         12      // D6
#define HX_RECIR_DOUT      13      // D7
#define HX_RECIR_SCK       15      // D8

#define ESCALA_DEF         20000.0f   // cuentas por litro, valor de partida: hay que calibrar
#define N_PROM             8          // muestras del promedio móvil (HX711 a 10 muestras/s -> ~0,8 s)
#define TIMEOUT_CELDA_MS   2000       // sin muestras válidas por este tiempo -> celda "no ok"

// ===================== CALIBRACIÓN (EEPROM) =====================
struct Cal {
  uint32_t magic;
  long     offset[2];   // cuentas con el tanque vacío
  float    escala[2];   // cuentas por litro
  int      adcMin, adcMax;
};
#define CAL_MAGIC 0x54414E32
Cal cal;

void guardarCal() { EEPROM.put(0, cal); EEPROM.commit(); }

void cargarCal() {
  EEPROM.begin(64);
  EEPROM.get(0, cal);
  if (cal.magic != CAL_MAGIC) {
    cal.magic = CAL_MAGIC;
    for (int i = 0; i < 2; i++) { cal.offset[i] = 0; cal.escala[i] = ESCALA_DEF; }
    cal.adcMin = ADC_MIN_DEF; cal.adcMax = ADC_MAX_DEF;
    guardarCal();
    Serial.println("# EEPROM sin calibracion: valores por defecto");
  }
}

// ===================== CANALES (celdas de carga) =====================
enum { REC = 0, RECIR = 1, N_CANALES = 2 };
const char* NOMBRE[N_CANALES] = { "recepcion", "recirculado" };

HX711 hx[N_CANALES];

struct Canal {
  long     buf[N_PROM];
  uint8_t  n = 0, idx = 0;       // muestras cargadas / posición de escritura
  uint32_t tUltima = 0;          // millis() de la última muestra válida
  long     raw = 0;              // promedio actual (cuentas)
  float    litros = 0;
  bool     ok = false;
};
Canal ch[N_CANALES];

// (recibe el índice y no la struct: el IDE de Arduino genera los prototipos antes de las structs)
long promedio(int i) {
  const Canal& c = ch[i];
  long long s = 0;
  for (uint8_t i = 0; i < c.n; i++) s += c.buf[i];
  return c.n ? (long)(s / c.n) : 0;
}

void leerCanales() {
  uint32_t ahora = millis();
  for (int i = 0; i < N_CANALES; i++) {
    Canal& c = ch[i];
    if (hx[i].is_ready()) {
      long r = hx[i].read();
      // -1 / 0 = línea flotando o cortada; extremos = saturado
      bool valida = !(r == -1 || r == 0 || r >= 8388607L || r <= -8388608L);
      if (valida) {
        c.buf[c.idx] = r;
        c.idx = (c.idx + 1) % N_PROM;
        if (c.n < N_PROM) c.n++;
        c.tUltima = ahora;
      }
    }
    c.ok = c.n > 0 && (ahora - c.tUltima) < TIMEOUT_CELDA_MS;
    if (c.n > 0) {
      c.raw = promedio(i);
      c.litros = fabsf((float)(c.raw - cal.offset[i]) / cal.escala[i]);   // siempre positivo: si la celda se instala al revés, igual mide hacia arriba
    }
  }
}

// ===================== POTE (fermentador) =====================
float adcFiltrado = 0;
float litrosFerm  = 0;

void leerPote() {
  uint32_t s = 0;
  for (int k = 0; k < 8; k++) s += analogRead(A0);
  float adc = s / 8.0f;
  adcFiltrado += 0.3f * (adc - adcFiltrado);              // suavizado
  float span = (float)(cal.adcMax - cal.adcMin);
  float l = (span > 1) ? (adcFiltrado - cal.adcMin) / span * CAP_FERMENTADOR : 0;
  litrosFerm = constrain(l, 0.0f, CAP_FERMENTADOR);
}

// ===================== ESTADO =====================
WebSocketsClient ws;
bool   wsConectado = false;
String lineaSerial;
String evento;
uint32_t tEvento = 0, tPote = 0, tEnvio = 0, tIntentoWiFi = 0;

void setEvento(const String& txt) {
  evento = txt; tEvento = millis();
  Serial.printf("# %s\n", txt.c_str());
}

// ===================== TELEMETRÍA / COMANDOS =====================
float r2(float v) { return roundf(v * 100.0f) / 100.0f; }

void enviarTelemetria() {
  JsonDocument d;
  d["id"] = ID_DISPOSITIVO;
  d["fermentador_litros"] = r2(litrosFerm);
  d["fermentador_adc"]    = (int)(adcFiltrado + 0.5f);
  for (int i = 0; i < N_CANALES; i++) {
    String k = NOMBRE[i];
    if (ch[i].ok) d[k + "_litros"] = r2(ch[i].litros);
    else          d[k + "_litros"] = nullptr;
    d[k + "_ok"]  = ch[i].ok;
    d[k + "_raw"] = ch[i].raw;
  }
  if (evento.length() && millis() - tEvento < 20000) d["evento"] = evento;

  String out;
  serializeJson(d, out);
  if (wsConectado) ws.sendTXT(out);
  else             Serial.println(out);       // sin Node-RED -> por serial
}

int indiceCanal(const char* nombre) {
  for (int i = 0; i < N_CANALES; i++) if (strcmp(nombre, NOMBRE[i]) == 0) return i;
  return -1;
}

void procesarComando(const char* txt) {
  JsonDocument d;
  if (deserializeJson(d, txt) || !d.is<JsonObject>()) return;

  // --- tara: tanque vacío ---
  if (d["tara"].is<const char*>()) {
    int i = indiceCanal(d["tara"].as<const char*>());
    if (i < 0)                       setEvento("tara: tanque desconocido");
    else if (!ch[i].ok || ch[i].n < N_PROM) setEvento(String("tara ") + NOMBRE[i] + ": celda sin datos");
    else {
      cal.offset[i] = promedio(i);
      guardarCal();
      setEvento(String("tara ") + NOMBRE[i] + " OK");
    }
  }

  // --- calibración: litros reales conocidos dentro del tanque ---
  if (d["cal"].is<JsonObject>()) {
    int i = indiceCanal(d["cal"]["tanque"] | "");
    float litros = d["cal"]["litros"] | 0.0f;
    if (i < 0)                       setEvento("cal: tanque desconocido");
    else if (litros < 0.05f)         setEvento("cal: indicá los litros reales (> 0)");
    else if (!ch[i].ok || ch[i].n < N_PROM) setEvento(String("cal ") + NOMBRE[i] + ": celda sin datos");
    else {
      long dif = promedio(i) - cal.offset[i];
      if (labs(dif) < 1000) setEvento(String("cal ") + NOMBRE[i] + ": casi no cambió la lectura, ¿tarado y con líquido?");
      else {
        cal.escala[i] = (float)dif / litros;   // puede ser negativa si la celda está invertida: funciona igual
        guardarCal();
        setEvento(String("cal ") + NOMBRE[i] + " OK");
      }
    }
  }

  // --- pote: fijar el vacío / lleno con la posición actual ---
  if (d["pote"].is<const char*>()) {
    String p = d["pote"].as<const char*>();
    int a = (int)(adcFiltrado + 0.5f);
    if (p == "min") {
      if (cal.adcMax - a < 50) setEvento("pote min: queda muy cerca del máximo");
      else { cal.adcMin = a; guardarCal(); setEvento("pote: 0 L ajustado"); }
    } else if (p == "max") {
      if (a - cal.adcMin < 50) setEvento("pote max: queda muy cerca del mínimo");
      else { cal.adcMax = a; guardarCal(); setEvento("pote: 55 L ajustado"); }
    }
  }

  enviarTelemetria();     // confirma el estado real (también sirve {} para pedirlo)
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
  int n = WiFi.scanNetworks();

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
      if (WiFi.status() == WL_CONNECT_FAILED) break;
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
  ws.beginSSL(NODERED_HOST, NODERED_PUERTO, NODERED_RUTA);
}

// ===================== SETUP / LOOP =====================
void setup() {
  Serial.begin(115200);
  Serial.println();
  WiFi.persistent(false);

  cargarCal();

  hx[REC].begin(HX_REC_DOUT, HX_REC_SCK);
  hx[RECIR].begin(HX_RECIR_DOUT, HX_RECIR_SCK);

  adcFiltrado = analogRead(A0);
  leerPote();

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

  leerCanales();

  uint32_t ahora = millis();
  if (ahora - tPote  >= PERIODO_POTE_MS)  { tPote  = ahora; leerPote(); }
  if (ahora - tEnvio >= PERIODO_ENVIO_MS) { tEnvio = ahora; enviarTelemetria(); }
}