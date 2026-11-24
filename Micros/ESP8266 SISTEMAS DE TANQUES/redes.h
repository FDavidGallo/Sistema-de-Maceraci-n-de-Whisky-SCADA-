#pragma once
// ---------------------------------------------------------------
//  redes.h  -  Redes WiFi conocidas del fermentador
//
//  Matriz 4x4: las filas son los SSID y las columnas las contraseñas.
//  Los pares válidos son la diagonal:
//     (0,0) = "SSID:contraseña"   (primera red)
//     (1,1) = "SSID:contraseña"   (segunda red)
//     (2,2), (3,3) = idem
//  El resto de las celdas no se usa. Se separa en el PRIMER ':' (la contraseña
//  puede llevar ':'). Diagonal vacía ("") = se saltea. "SSID:" = red abierta.
// ---------------------------------------------------------------
#define N_REDES 4
const char* const REDES[N_REDES][N_REDES] = {
  { "Alumnos2.4GHz:alumnosfcyt", "", "", "" },
  { "", "Celu:CeluDeGallo", "", "" },
  { "", "", "Taller:clave_taller", "" },
  { "", "", "", "Hotspot-Celu:clave_celu" },
};