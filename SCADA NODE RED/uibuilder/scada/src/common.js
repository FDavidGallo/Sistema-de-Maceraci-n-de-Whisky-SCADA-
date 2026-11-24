/* Código común de las páginas Estadísticas y Fases/alarmas (uibuilder).
   Recibe: 'plc' (estado), 'status' ({modo}), 'hist' (historial del servidor), 'hist_cleared'. Envía: 'hello', 'get_hist', 'clear_hist' (todo con el token de sesión, ver auth.js). */
'use strict'
const $ = id => document.getElementById(id)
const S = { modo: 'default' }
let enviar = (topic, payload) => console.log('[demo] enviar', topic, payload)
let T_PLC = 0
const pad = n => String(n).padStart(2, '0')
const hora = t => { const d = new Date(t); return pad(d.getHours()) + ':' + pad(d.getMinutes()) + ':' + pad(d.getSeconds()) }
const fechaHora = t => { const d = new Date(t); return pad(d.getDate()) + '/' + pad(d.getMonth() + 1) + ' ' + hora(t) }

function conectar(alRecibir, alAbrir) {
  if (typeof uibuilder === 'undefined') { console.warn('uibuilder no está disponible: abrí la página desde Node-RED (/scada/)'); return }
  uibuilder.onChange('msg', msg => {
    if (!msg) return
    if (Auth.onMsg(msg)) return                       // mensajes de login/sesión
    if (msg.topic === 'plc') { Object.assign(S, msg.payload); T_PLC = Date.now() }
    else if (msg.topic === 'status') Object.assign(S, msg.payload)
    if (msg.topic === 'plc') Alarmas.revisar(S)
    alRecibir(msg)
  })
  enviar = (topic, payload) => uibuilder.send(Auth.wrap({ topic, payload }))
  uibuilder.onChange('ioConnected', c => { if (c) { enviar('hello'); alAbrir() } })
  if (uibuilder.ioConnected) { enviar('hello'); alAbrir() }
}

const MODOS = { default: 'Default', manual: 'Manual', simulacion: 'Simulación', pruebas: 'Pruebas' }
setInterval(() => {
  const m = $('b-modo'); if (m) { m.textContent = 'Modo: ' + (MODOS[S.modo] || S.modo); m.className = 'badge ' + (S.modo === 'pruebas' ? 'warn' : S.modo === 'simulacion' ? 'sim' : 'on') }
  const p = $('b-plc'); if (!p) return
  if (S.modo === 'simulacion') { p.className = 'badge sim'; p.textContent = 'PLC simulado' }
  else if (S.modo === 'pruebas') { p.className = 'badge warn'; p.textContent = 'PLC con datos de prueba' }
  else { const ok = Date.now() - T_PLC < 5000; p.className = 'badge ' + (ok ? 'on' : 'off'); p.textContent = 'PLC ' + (ok ? 'OK' : 'sin datos') }
}, 1000)
