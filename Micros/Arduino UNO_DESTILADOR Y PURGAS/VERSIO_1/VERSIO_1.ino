#include "Headers.h"

// ==================== DEFINICIÓN DE PINES (ARDUINO UNO R4) ====================
#define PIN_DHT11     2
#define PIN_RELE     12
#define PIN_POT      A1   // Pin central del potenciómetro (peso del alambique)
#define PIN_POT_VCC   7   // Pata (+) del potenciómetro, alimentada desde un pin digital
#define PIN_POT_GND   6   // Pata (-) del potenciómetro

// Salidas ON/OFF: un solo pin por actuador (HIGH = activado, LOW = apagado). El otro borne va a GND.
#define PIN_PURGA_DEST    10   // Purga del destilador (XV202)
#define PIN_PURGA_FERM     4   // Purga del fermentador (XV101)
#define PIN_VACIADO       11   // Válvula de vaciado / extracción (XV301)

// Bomba de agua del destilador (P201 + XV201). Ojo: el pin 13 es además el LED interno de la placa
#define PIN_BOMBA_AGUA  13

// ==================== SIMULACIÓN DEL DESTILADOR ====================
const float OFFSET_TEMP_SIM = 20.0;   // Se suma a la medición del DHT11 para simular más temperatura
const float PESO_MAX_L      = 75.0;   // Fondo de escala del "peso" del alambique (litros)

// ADC de la R4: hasta 14 bits (0..16383). Más sobremuestreo = menos ruido.
const uint8_t  ADC_BITS      = 14;
const uint16_t N_MUESTRAS    = 64;    // Promedio de lecturas por cada medición de peso
float adcMin = 0.0;                   // Lectura con el pote al mínimo (calibrable con el comando calmin)
float adcMax = 1502;               // Lectura con el pote al máximo (calibrable con el comando calmax)
float adcPromedio = 0.0;              // Último promedio crudo (sirve para calibrar)

// ==================== PARÁMETROS Y VARIABLES GLOBALES ====================
float setpoint = 30.0;
float KP = 2.0;
float KI = 0.0;
float KD = 1.0;
double TS = 1.0;          // Tiempo de muestreo por defecto en segundos
double HISTERESIS = 2.0;  // Ancho de banda para el control On/Off

int BTRELE = 100;         // Base de tiempo del relé en ms
float temperaturaActual = 0.0;   // Ya incluye el offset de simulación
float pesoLitros = 0.0;
bool sensorOk = false;
unsigned long tiempoUltimoControl = 0;
unsigned long tiempoUltimaTelemetria = 0;
const unsigned long PERIODO_TELEMETRIA_MS = 500;
int porcentajeActuador = 0;

// Estado actual de las purgas (true = activada). Arrancan apagadas.
bool purgaDestilador   = false;
bool purgaFermentador  = false;

// Estado de la válvula de vaciado (true = abierta). Arranca cerrada.
bool valvulaVaciado    = false;

// Estado de la bomba de agua (true = encendida). Arranca apagada.
bool bombaAgua = false;

// ==================== OBJETOS GLOBALES ====================
SensorDeTemperatura SensorDht11(PIN_DHT11);
Rele ReleSsr(PIN_RELE, BTRELE, "SSR");
// Arrancamos con PID habilitado (true) y On/Off deshabilitado (false)
Control sistemaControl(KP, KI, KD, TS, HISTERESIS, true, false);

// ==================== PROTOTIPOS ====================
void procesarComando(String cmd);
void enviarTelemetria();
float leerPesoLitros();
void reconstruirControl();
void aplicarSalida(uint8_t pin, bool activar);
bool interpretarEstado(String v, bool &resultado);

// ==================== SETUP ====================
void setup() {
    Serial.begin(115200);   // En la R4 el USB es nativo, pero Node-RED debe usar el mismo valor
    delay(200);

    // Salidas ON/OFF: pines como salida y arrancan apagadas (LOW)
    pinMode(PIN_PURGA_DEST, OUTPUT);
    pinMode(PIN_PURGA_FERM, OUTPUT);
    pinMode(PIN_VACIADO,    OUTPUT);
    aplicarSalida(PIN_PURGA_DEST, false);
    aplicarSalida(PIN_PURGA_FERM, false);
    aplicarSalida(PIN_VACIADO,    false);

    // Bomba de agua: arranca apagada
    pinMode(PIN_BOMBA_AGUA, OUTPUT);
    digitalWrite(PIN_BOMBA_AGUA, LOW);

    // Alimentación del potenciómetro desde pines digitales
    pinMode(PIN_POT_VCC, OUTPUT);
    pinMode(PIN_POT_GND, OUTPUT);
    digitalWrite(PIN_POT_VCC, HIGH);
    digitalWrite(PIN_POT_GND, LOW);
    //analogReadResolution(ADC_BITS);   // 14 bits en la R4
    delay(50);                        // Que se estabilice la tensión del pote

    SensorDht11.inicializar();
    // Apagar el actuador al inicio de forma segura (0% de potencia)
    ReleSsr.Aplicar(0);

    tiempoUltimoControl = millis();
    tiempoUltimaTelemetria = millis();
}

// ==================== LOOP PRINCIPAL ====================
void loop() {
    // 1. Gestión de comandos por puerto serie
    //    (while en vez de if: se vacía todo lo que haya llegado, así los comandos de purga
    //     no quedan esperando detrás de otros comandos mientras el relé bloquea el loop)
    while (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd.length() > 0) {
            procesarComando(cmd);
        }
    }

    // 2. Control temporizado por Tiempo de Muestreo (TS)
    unsigned long intervaloMs = (unsigned long)(TS * 1000.0);
    if (millis() - tiempoUltimoControl >= intervaloMs) {
        tiempoUltimoControl = millis();

        float lectura = SensorDht11.leerTemperatura();
        if (lectura != -999.0) {
            sensorOk = true;
            // Simulación: sumamos 20 °C a la medición real
            temperaturaActual = lectura + OFFSET_TEMP_SIM;

            double accionControl = sistemaControl.CalcularError(setpoint, temperaturaActual);
            porcentajeActuador = (int)accionControl;
            if (porcentajeActuador < 1)   porcentajeActuador = 0;
            if (porcentajeActuador > 100) porcentajeActuador = 100;
            ReleSsr.Aplicar(porcentajeActuador);   // Ojo: bloquea ~800 ms (delay interno de Rele)
        } else {
            // Sensor caído: Rele::Aplicar deja el relé en el último estado, así que
            // forzamos 0% para no dejar el calefactor prendido sin medición.
            sensorOk = false;
            porcentajeActuador = 0;
            ReleSsr.Aplicar(0);
            delay(200);
        }
    }

    // 3. Telemetría periódica (peso incluido), independiente del control
    if (millis() - tiempoUltimaTelemetria >= PERIODO_TELEMETRIA_MS) {
        tiempoUltimaTelemetria = millis();
        pesoLitros = leerPesoLitros();
        enviarTelemetria();
    }

    delay(10);
}

// ==================== LECTURA DEL "PESO" (POTENCIÓMETRO) ====================
// Sobremuestreo: promedia N_MUESTRAS lecturas de 14 bits (resolución ~4.6 mL por cuenta,
// el promedio baja el ruido a ~1 mL). Se informa con 2 decimales (0.01 L).
float leerPesoLitros() {
    uint32_t suma = 0;
    for (uint16_t i = 0; i < N_MUESTRAS; i++) {
        suma += analogRead(PIN_POT);
    }
    adcPromedio = (float)suma / (float)N_MUESTRAS;

    float rango = adcMax - adcMin;
    if (rango < 1.0) rango = 1.0;
    float litros = (adcPromedio - adcMin) * PESO_MAX_L / rango;
    if (litros < 0.0) litros = 0.0;
    if (litros > PESO_MAX_L) litros = PESO_MAX_L;
    return litros;
}

// ==================== AUXILIAR ====================
void reconstruirControl() {
    sistemaControl = Control(KP, KI, KD, TS, HISTERESIS,
                             sistemaControl.GetBanderaPID(), sistemaControl.GetBanderaOnOff());
}

// Activa (HIGH) o desactiva (LOW) una salida de un solo pin.
void aplicarSalida(uint8_t pin, bool activar) {
    digitalWrite(pin, activar ? HIGH : LOW);
}

// Interpreta "1/0", "on/off", "true/false". Devuelve false si el texto no es válido.
bool interpretarEstado(String v, bool &resultado) {
    v.trim();
    if (v == "1" || v == "on"  || v == "true")  { resultado = true;  return true; }
    if (v == "0" || v == "off" || v == "false") { resultado = false; return true; }
    return false;
}

// ==================== PROCESAMIENTO DE COMANDOS ====================
void procesarComando(String cmd) {
    cmd.toLowerCase();
    if (cmd.startsWith("setpoint:")) {
        float val = cmd.substring(9).toFloat();
        if (val >= -50 && val <= 300) setpoint = val;
    }
    else if (cmd.startsWith("hist:")) {
        float val = cmd.substring(5).toFloat();
        if (val >= 0.1 && val <= 50) {
            HISTERESIS = val;
            reconstruirControl();
        }
    }
    else if (cmd.startsWith("kp:")) {
        float val = cmd.substring(3).toFloat();
        if (val >= 0 && val <= 255) { KP = val; reconstruirControl(); }
    }
    else if (cmd.startsWith("ki:")) {
        float val = cmd.substring(3).toFloat();
        if (val >= 0 && val <= 100) { KI = val; reconstruirControl(); }
    }
    else if (cmd.startsWith("kd:")) {
        float val = cmd.substring(3).toFloat();
        if (val >= 0 && val <= 100) { KD = val; reconstruirControl(); }
    }
    else if (cmd.startsWith("ts:")) {
        float val = cmd.substring(3).toFloat();
        if (val >= 1.0 && val <= 10.0) { TS = val; reconstruirControl(); }
        // (se quitó el Serial.print(TS) que ensuciaba el JSON)
    }
    else if (cmd.startsWith("onoff:on")) {
        sistemaControl.SetBanderaPID(false);
        sistemaControl.SetBanderaOnOff(true);
    }
    else if (cmd.startsWith("onoff:off")) {
        sistemaControl.SetBanderaPID(true);
        sistemaControl.SetBanderaOnOff(false);
    }
    // Purgas: "purgadest:1" / "purgadest:0" y "purgaferm:1" / "purgaferm:0" (también on/off)
    else if (cmd.startsWith("purgadest:")) {
        bool estado;
        if (interpretarEstado(cmd.substring(10), estado)) {
            purgaDestilador = estado;
            aplicarSalida(PIN_PURGA_DEST, purgaDestilador);
        }
    }
    else if (cmd.startsWith("purgaferm:")) {
        bool estado;
        if (interpretarEstado(cmd.substring(10), estado)) {
            purgaFermentador = estado;
            aplicarSalida(PIN_PURGA_FERM, purgaFermentador);
        }
    }
    // Válvula de vaciado: "vaciado:1" / "vaciado:0" (también on/off)
    else if (cmd.startsWith("vaciado:")) {
        bool estado;
        if (interpretarEstado(cmd.substring(8), estado)) {
            valvulaVaciado = estado;
            aplicarSalida(PIN_VACIADO, valvulaVaciado);
        }
    }
    // Bomba de agua: "agua:1" / "agua:0" (también on/off)
    else if (cmd.startsWith("agua:")) {
        bool estado;
        if (interpretarEstado(cmd.substring(5), estado)) {
            bombaAgua = estado;
            digitalWrite(PIN_BOMBA_AGUA, bombaAgua ? HIGH : LOW);
        }
    }
    // Calibración del pote: ponelo al mínimo y mandá "calmin", después al máximo y mandá "calmax".
    // Los valores quedan en RAM; copiá los que informa la telemetría a adcMin/adcMax para fijarlos.
    else if (cmd.startsWith("calmin")) {
        pesoLitros = leerPesoLitros();
        adcMin = adcPromedio;
    }
    else if (cmd.startsWith("calmax")) {
        pesoLitros = leerPesoLitros();
        adcMax = adcPromedio;
    }
}

// ==================== TELEMETRÍA ====================
void enviarTelemetria() {
    String tipoControl = "DESCONOCIDO";
    if (sistemaControl.GetBanderaPID()) {
        tipoControl = "PID";
    } else if (sistemaControl.GetBanderaOnOff()) {
        tipoControl = "ON_OFF";
    }

    String json = "{";
    json += "\"setpoint\":" + String(setpoint, 2) + ",";
    json += "\"temperatura_actual\":" + String(temperaturaActual, 2) + ",";
    json += "\"histeresis\":" + String((float)HISTERESIS, 2) + ",";
    json += "\"peso_litros\":" + String(pesoLitros, 2) + ",";
    json += "\"adc_prom\":" + String(adcPromedio, 1) + ",";
    json += "\"adc_min\":" + String(adcMin, 1) + ",";
    json += "\"adc_max\":" + String(adcMax, 1) + ",";
    json += "\"sensor_ok\":" + String(sensorOk ? "true" : "false") + ",";
    json += "\"purga_destilador\":" + String(purgaDestilador ? "true" : "false") + ",";
    json += "\"purga_fermentador\":" + String(purgaFermentador ? "true" : "false") + ",";
    json += "\"valvula_vaciado\":" + String(valvulaVaciado ? "true" : "false") + ",";
    json += "\"bomba_agua\":" + String(bombaAgua ? "true" : "false") + ",";
    json += "\"salida_porcentaje\":" + String((int)porcentajeActuador) + ",";
    json += "\"modo_control\":\"" + tipoControl + "\"";

    if (tipoControl == "PID") {
        json += ",\"parametros_pid\":{";
        json += "\"kp\":" + String(KP, 2) + ",";
        json += "\"ki\":" + String(KI, 2) + ",";
        json += "\"kd\":" + String(KD, 2) + ",";
        json += "\"ts\":" + String(TS, 1);
        json += "}";
    }
    json += "}";

    Serial.println(json);
}