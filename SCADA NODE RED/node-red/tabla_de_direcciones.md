# Tabla de direcciones (PLC_1 S7-1200 ↔ Node-RED)

Todas salen de tu export de TIA Portal. Acceso PUT/GET habilitado y DB sin acceso optimizado (los offsets figuran en el export).

| Nombre en Node-RED | Dirección | Variable del PLC |
|---|---|---|
| fer_temp | MR40 | Fer_Temperatura |
| des_temp | MR134 | Des_Temperatura |
| fer_vol | MR54 | Fer_Volumen |
| des_vol | MR14 | Des_Volumen |
| rec_vol | MR82 | Rec_Volumen |
| recir_vol | MR92 | RecirVol |
| fer_llenando | M130.0 | Fer_Llenando |
| fer_completa | M72.7 | Fer_FermentacionCompleta |
| cond_clarificar | M131.1 | Des_CondicionesClarificar |
| alerta_agua_baja | M132.3 | AlertaAguaBajaDestilador |
| modo_manual | M500.0 | ModoManual |
| act_bombawash | M36.0 | Act_BombaWash |
| act_aguadest | M36.1 | Act_BombaAguaDestilador |
| act_purgadest | M36.2 | Act_PurgaDestilador |
| act_valvrecirc | M36.3 | Act_ValvRecirculadoDestilador |
| act_resistencia | M36.4 | Act_ResistenciaDestilador |
| act_valvmosto | M72.1 | Act_ValvIngresoMosto |
| act_purgaferm | M72.2 | Act_PurgaFermentador |
| act_refrig | M72.5 | Act_RefrigeracionFermentador |
| act_vaciado | M88.0 | Act_ValvVaciado |
| act_bombarecirc | M90.0 | Act_BombaRecirculado |
| alm_falla_uss | DB6,X0.0 | Alarmas.Alm_FallaUSS |
| alm_sobrecalentado | DB6,X0.1 | Alarmas.Alm_FermentadorSobrecalentado |
| alm_aviso_metanol | DB6,X0.2 | Alarmas.Alm_AvisoDescarteMetanol |
| alm_ferm_lleno | DB6,X0.3 | Alarmas.Alm_FermentadorLLeno |
| alm_demasiado_wash | DB6,X0.4 | Alarmas.Alm_DemasiadoWashDestilador |
| fase_descarte_metanol | DB6,X0.5 | Alarmas.Fase_MomentoDescarteMetanol |
| fase_ferm_terminada | DB6,X0.6 | Alarmas.Fase_FermentacionTerminada |
| fase_limpieza_ini | DB6,X0.7 | Alarmas.Fase_InicioLimpiezaFermentador |
| fase_limpieza_fin | DB6,X1.0 | Alarmas.Fase_FinLimpiezaFermentador |
| fase_inicio_destilado | DB6,X1.1 | Alarmas.Fase_InicioDestilado |
| fase_inicio_clarificado | DB6,X1.2 | Alarmas.Fase_InicioClarificado |
| fase_descarte_exitoso | DB6,X1.3 | Alarmas.Fase_DescarteExitoso |
| fase_whisky_terminado | DB6,X1.4 | Alarmas.Fase_WhiskeyTerminado |
| fase_inicio_recirculado | DB6,X1.5 | Alarmas.Fase_InicioRecirculado |
| fase_fin_recirculado | DB6,X1.6 | Alarmas.Fase_FinRecirculado |
| fase_mezcla | DB6,X1.7 | Alarmas.Fase_MezclaCompletada |
| rec_nombre | DB4,S0.254 | DatosDeRecetas.NombreReceta |
| rec_sp_fer | DB4,REAL258 | DatosDeRecetas.SetPointTempFermentador |
| rec_hist_fer | DB4,REAL262 | DatosDeRecetas.HisterisisFermentador |
| rec_sp_des | DB4,REAL266 | DatosDeRecetas.SetPointTempDestilador |
| rec_hist_des | DB4,REAL270 | DatosDeRecetas.HisterisisDestilador |
| man_wash | DB2,X0.0 | ControlManual.ManualBombaDeWash |
| man_aguadest | DB2,X0.1 | ControlManual.ManualBombaDeAguaDestilador |
| man_purgadest | DB2,X0.2 | ControlManual.ManualPurgaDestilador |
| man_valvrecirc | DB2,X0.3 | ControlManual.ManualValvulaRecirculadoDestilador |
| man_mosto | DB2,X0.4 | ControlManual.ManualValvulaIngresoDeMosto |
| man_purgaferm | DB2,X0.5 | ControlManual.ManualPurgaFermentador |
| man_refrig | DB2,X0.6 | ControlManual.ManualEstadoRefriegeracionFermentador |
| man_vaciado | DB2,X0.7 | ControlManual.ManualValvulaDeVaciado |
| man_bombarecirc | DB2,X1.0 | ControlManual.ManualBombaDeRecirculado |
| btn_iniciar | M231.0 | SCADA -> Iniciar (ver nota PLC) |
| btn_extraer | M231.1 | SCADA -> Extraer (ver nota PLC) |

## Relación dibujo ↔ PLC

| Elemento del SCADA | Marca PLC | Bit manual (DB2) |
|---|---|---|
| P101 + XV203 (wash fermentador→torre) | Act_BombaWash M36.0 | ManualBombaDeWash X0.0 |
| AGUA: XV201 + P201 | Act_BombaAguaDestilador M36.1 | ManualBombaDeAguaDestilador X0.1 |
| XV202 purga destilador | Act_PurgaDestilador M36.2 | ManualPurgaDestilador X0.2 |
| XV204 entrada recirculado | Act_ValvRecirculadoDestilador M36.3 (sale por la misma salida `recirculado` del ESP32 que la bomba) | ManualValvulaRecirculadoDestilador X0.3 |
| XV-MOSTO (chip) | Act_ValvIngresoMosto M72.1 | ManualValvulaIngresoDeMosto X0.4 |
| XV101 purga fermentador | Act_PurgaFermentador M72.2 | ManualPurgaFermentador X0.5 |
| XV102 + equipo de refrigeración | Act_RefrigeracionFermentador M72.5 | ManualEstadoRefriegeracionFermentador X0.6 |
| XV301 extracción | Act_ValvVaciado M88.0 | ManualValvulaDeVaciado X0.7 |
| P302 + XV302 | Act_BombaRecirculado M90.0 (misma salida `recirculado` del ESP32 que XV204) | ManualBombaDeRecirculado X1.0 |
| Resistencia (torre/condensador encendidos) | Act_ResistenciaDestilador M36.4 | — (no tiene bit manual) |

## Salidas físicas por micro

| Micro | Actuador (clave) | Marca(s) PLC |
|---|---|---|
| Arduino | purgadest (pin 10) | Act_PurgaDestilador M36.2 |
| Arduino | purgaferm (pin 4) | Act_PurgaFermentador M72.2 |
| Arduino | agua (pin 13) | Act_BombaAguaDestilador M36.1 |
| Arduino | vaciado (pin 11) | Act_ValvVaciado M88.0 |
| ESP32 | bomba_mosto (GPIO5, relé 2) | Act_ValvIngresoMosto M72.1 |
| ESP32 | refrigeracion (GPIO23, relé 1) | Act_RefrigeracionFermentador M72.5 |
| ESP32 | valvula_destilador (GPIO4, relé 3) | Act_BombaWash M36.0 |
| ESP32 | recirculado (GPIO13, relé 4) | Act_BombaRecirculado M90.0 OR Act_ValvRecirculadoDestilador M36.3 |
| ESP8266 | — (solo sensores) | — |
