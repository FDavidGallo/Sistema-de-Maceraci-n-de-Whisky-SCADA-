/* SCADA destilería v2 – front-end uibuilder.
   Recibe:  'plc' (payload {variable:valor}, nombres de tabla_de_direcciones.md), 'micro' ({src}), 'status' ({modo}).
   Envía:   'hello', 'modo', 'manual', 'btn', 'set_sp', 'test_set' (todo con el token de sesión, ver auth.js). */
'use strict'

const COLOR = ['gris', 'verde', 'rojo']
const EQ = ['apagado', 'encendido', 'alarma']
const PIEZAS = [
  { id: 'img1', dir: '1_tanque_serpentin', eq: 'eq1', comp: ['xv102', 'xv101', 'p101'] },
  { id: 'img2', dir: '2_alambique',        eq: 'eq2', comp: ['xv203', 'xv204', 'xv202'] },
  { id: 'img3', dir: '3_condensador',      eq: 'eq3', comp: ['xv201', 'p201'], fijo: true },   // el agua se dibuja aparte (AGUA)
  { id: 'img4', dir: '4_tanque_colector',  eq: 'eq4', comp: ['xv302', 'xv301'] },
  { id: 'img5', dir: '5_tanque_chico',     eq: 'eq5', comp: ['p302'] },
]
// componente del dibujo -> marca real del PLC que lo gobierna
const RAW = {
  xv102: 'act_refrig', xv101: 'act_purgaferm', p101: 'act_bombawash', xv203: 'act_bombawash', xv204: 'act_valvrecirc',
  xv202: 'act_purgadest', xv201: 'act_aguadest', p201: 'act_aguadest', xv302: 'act_bombarecirc', xv301: 'act_vaciado',
  p302: 'act_bombarecirc', mosto: 'act_valvmosto', agua: 'act_aguadest',
}
// componente -> bit de DB2 "ControlManual" (modo Manual)
const MAN = {
  xv102: 'man_refrig', xv101: 'man_purgaferm', p101: 'man_wash', xv203: 'man_wash', xv204: 'man_valvrecirc',
  xv202: 'man_purgadest', xv201: 'man_aguadest', p201: 'man_aguadest', xv302: 'man_bombarecirc', xv301: 'man_vaciado',
  p302: 'man_bombarecirc', mosto: 'man_mosto', agua: 'man_aguadest',
}
const POS = {   // [imagen, x, y, texto] posiciones dentro de cada imagen (px nativos, x2 en pantalla)
  xv102: ['img1', 8, 71, 'XV102 refrig.'], xv101: ['img1', 43, 162, 'XV101 purga'], p101: ['img1', 81, 150, 'P101 wash'],
  xv203: ['img2', 12, 71, 'XV203 ent. wash'], xv204: ['img2', 80, 71, 'XV204 ent. recirc.'], xv202: ['img2', 47, 122, 'XV202 purga'],
  xv302: ['img4', 6, 54, 'XV302 recirc.'], xv301: ['img4', 45, 54, 'XV301 extracción'], p302: ['img5', 18, 30, 'P302 recirc.'],
}
const ORIGEN = { img1: [20, 80], img2: [320, 80], img3: [600, 80], img4: [600, 350], img5: [320, 440] }
const AGUA_XY = [392, 94]            // válvula XV201 sobre la entrada de agua de la torre
const MOSTO_XY = [192, 289]          // chip de ingreso de mosto
const TUBERIAS = [
  { id: 'p1', pts: '220,393 326,393 326,232 332,232', act: () => S.act_bombawash },
  { id: 'p2', pts: '502,87 560,87 560,162 600,162', act: () => S.act_resistencia },
  { id: 'p3', pts: '784,166 840,166 840,320 646,320 646,350', act: () => S.act_resistencia },
  { id: 'p4', pts: '600,466 566,466 566,448 506,448', act: () => S.act_bombarecirc },
  { id: 'p5', pts: '320,513 296,513 296,425 548,425 548,232 520,232', act: () => S.act_bombarecirc && S.act_valvrecirc },
  { id: 'p6', pts: '790,466 880,466', act: () => S.act_vaciado },
  { id: 'p7', pts: '360,94 414,94', act: () => S.act_aguadest },
]

// ---- Estado ------------------------------------------------------------------------------------
const NUM = ['fer_temp', 'des_temp', 'fer_vol', 'des_vol', 'rec_vol', 'recir_vol', 'rec_sp_fer', 'rec_hist_fer', 'rec_sp_des', 'rec_hist_des']
const BOOL = ['act_bombawash', 'act_aguadest', 'act_purgadest', 'act_valvrecirc', 'act_resistencia', 'act_valvmosto', 'act_purgaferm',
  'act_refrig', 'act_vaciado', 'act_bombarecirc', 'fer_llenando', 'fer_completa', 'cond_clarificar', 'alerta_agua_baja', 'modo_manual',
  'alm_falla_uss', 'alm_sobrecalentado', 'alm_aviso_metanol', 'alm_ferm_lleno', 'alm_demasiado_wash', 'fase_descarte_metanol',
  'fase_ferm_terminada', 'fase_limpieza_ini', 'fase_limpieza_fin', 'fase_inicio_destilado', 'fase_inicio_clarificado',
  'fase_descarte_exitoso', 'fase_whisky_terminado', 'fase_inicio_recirculado', 'fase_fin_recirculado', 'fase_mezcla',
  'man_wash', 'man_aguadest', 'man_purgadest', 'man_valvrecirc', 'man_mosto', 'man_purgaferm', 'man_refrig', 'man_vaciado', 'man_bombarecirc']
const S = { modo: 'default', rec_nombre: '', fase_txt: '' }
for (const k of NUM) S[k] = null
for (const k of BOOL) S[k] = false
const F = {}                                      // fallas locales (solo modo Pruebas)
const PEND = {}                                   // cambios manuales esperando confirmación del PLC: clave -> {v, hasta}
function aplicarPlc(p) {
  const ahora = Date.now()
  for (const k in p) {
    const pe = PEND[k]
    if (pe) { if (p[k] === pe.v) delete PEND[k]; else if (ahora < pe.hasta) continue; else delete PEND[k] }
    S[k] = p[k]
  }
}
const $ = id => document.getElementById(id)
const compColor = c => (F[c] ? 2 : S[RAW[c]] ? 1 : 0)
let hayLiquido = false
const eqEstado = p => {
  let alarma = false, on = false
  switch (p.eq) {
    case 'eq1': alarma = S.alm_sobrecalentado || S.alm_ferm_lleno; on = S.act_refrig || S.fer_llenando || S.act_bombawash; break
    case 'eq2': alarma = S.alm_demasiado_wash || S.alm_aviso_metanol || S.alerta_agua_baja
                on = S.act_resistencia || S.act_aguadest || S.act_valvrecirc || S.act_purgadest || S.act_bombawash; break
    case 'eq3': on = S.act_resistencia; break
    case 'eq4': {   // histéresis: la celda de carga oscila cerca de 0 y con un solo umbral el tanque titilaba
      const v = S.rec_vol || 0
      if (v > 0.25) hayLiquido = true; else if (v < 0.10) hayLiquido = false
      on = hayLiquido || S.act_vaciado; break
    }
    case 'eq5': on = S.act_bombarecirc; break
  }
  return alarma ? 2 : on ? 1 : 0
}
const f = (v, d) => (v === null || v === undefined || isNaN(v)) ? '--' : Number(v).toFixed(d)

const NOMBRES = { irlandes: 'Irlandés', escoces: 'Escocés', americano: 'Americano', bourbon: 'Bourbon', canadiense: 'Canadiense', japones: 'Japonés' }
function nombreWhisky() {
  const n = String(S.rec_nombre || '').replace(/[\u0000-\u001f]/g, '').trim()
  if (!n) return '--'
  if (/^whisk/i.test(n)) return n
  const k = n.toLowerCase().normalize('NFD').replace(/[\u0300-\u036f]/g, '')
  return 'Whisky ' + (NOMBRES[k] || n.charAt(0).toUpperCase() + n.slice(1))
}
function faseTxt() {
  if (S.fase_txt) return S.fase_txt
  if (S.fase_whisky_terminado) return 'Whisky terminado'
  if (S.fase_fin_recirculado) return 'Fin de recirculado'
  if (S.fase_inicio_recirculado) return 'Recirculando'
  if (S.fase_descarte_exitoso) return 'Descarte de metanol exitoso'
  if (S.fase_descarte_metanol) return 'Descarte de metanol'
  if (S.fase_inicio_clarificado) return 'Clarificado (agregando agua)'
  if (S.fase_inicio_destilado) return 'Pesaje previo al clarificado'
  if (S.fase_limpieza_ini && !S.fase_limpieza_fin) return 'Limpieza del fermentador'
  if (S.fase_ferm_terminada) return 'Fermentación terminada'
  if (S.fer_llenando) return 'Llenando fermentador'
  if (S.act_resistencia) return 'Destilando'
  return 'En espera'
}

// ---- Dibujo ------------------------------------------------------------------------------------
function dibujarTuberias() {
  const [ax, ay] = AGUA_XY
  $('tuberias').innerHTML = TUBERIAS.map(t => `<polyline id="${t.id}" points="${t.pts}"/>`).join('') 
  $('tags').innerHTML = Object.entries(POS).filter(([c]) => c === 'xv301').map(([c, [img, x, y, txt]]) => {
    const [ox, oy] = ORIGEN[img]
    return `<div class="tag" style="left:${ox + x * 2 - 6}px;top:${oy + y * 2 - 14}px">${txt}</div>`
  }).join('')
  const hs = Object.entries(POS).map(([c, [img, x, y]]) => {
    const [ox, oy] = ORIGEN[img]; return [c, ox + x * 2, oy + y * 2]
  })
  hs.push(['agua', ax, ay])
  $('hotspots').innerHTML = hs.map(([c, x, y]) => `<div class="hs" data-c="${c}" style="left:${x - 15}px;top:${y - 15}px"></div>`).join('')
  const [mx, my] = MOSTO_XY; const ch = $('chip-mosto'); ch.dataset.c = 'mosto'; ch.style.left = mx + 'px'; ch.style.top = my + 'px'
}
const FILL = ['#6e7681', '#3fb950', '#f85149']

function render() {
  for (const p of PIEZAS) {
    if (p.fijo) { $(p.id).src = `./img/${p.dir}/limpio_eq-${EQ[eqEstado(p)]}.png`; continue }   // condensador sin embudo/válvula/bomba
    const partes = p.comp.map(c => `${c}-${COLOR[compColor(c)]}`).join('_')
    $(p.id).src = `./img/${p.dir}/${p.dir}_eq-${EQ[eqEstado(p)]}_${partes}.png`
  }
  document.querySelectorAll('#tuberias polyline').forEach(e => { const t = TUBERIAS.find(x => x.id === e.id); e.classList.toggle('act', !!(t && t.act())) })
  $('v-agua').src = `./img/valvula_${COLOR[compColor('agua')]}.png`
  const cm = $('chip-mosto'); cm.classList.toggle('on', !!S.act_valvmosto); cm.classList.toggle('fault', !!F.mosto)

  $('i-fti').textContent = f(S.fer_temp, 1); $('i-fsp').textContent = f(S.rec_sp_fer, 1); $('i-fh').textContent = f(S.rec_hist_fer, 1); $('i-fwi').textContent = f(S.fer_vol, 2)
  $('i-dti').textContent = f(S.des_temp, 1); $('i-dsp').textContent = f(S.rec_sp_des, 1); $('i-dh').textContent = f(S.rec_hist_des, 1); $('i-dwi').textContent = f(S.des_vol, 2)
  $('i-rwi').textContent = f(S.rec_vol, 2); $('i-cwi').textContent = f(S.recir_vol, 2)
  $('v-ft').textContent = f(S.fer_temp, 1); $('v-dt').textContent = f(S.des_temp, 1)
  $('v-fv').textContent = f(S.fer_vol, 2); $('v-dv').textContent = f(S.des_vol, 2); $('v-rv').textContent = f(S.rec_vol, 2); $('v-cv').textContent = f(S.recir_vol, 2)
  $('receta-nombre').textContent = nombreWhisky()
  $('fase').textContent = 'Fase: ' + faseTxt()

  document.body.className = 'm-' + S.modo
  document.querySelectorAll('#modos button').forEach(b => b.classList.toggle('act', b.dataset.modo === S.modo))
  const cmd = S.modo === 'default' || S.modo === 'manual'
  $('btn-inicio').disabled = !cmd; $('btn-extraer').disabled = !cmd
  for (const k of ['rec_sp_fer', 'rec_hist_fer', 'rec_sp_des', 'rec_hist_des']) {
    const i = $('in-' + k); if (i !== document.activeElement && S[k] !== null) i.value = Number(S[k]).toFixed(2).replace(/\.?0+$/, '')
  }
  document.querySelectorAll('[data-alm]').forEach(c => { c.checked = !!S[c.dataset.alm] })
  Alarmas.revisar(S)                                         // ventana emergente de alarmas de error
}

// ---- Comunicación ------------------------------------------------------------------------------
let enviar = (topic, payload) => console.log('[demo] enviar', topic, payload)
const T = { arduino: 0, esp32: 0, esp8266: 0, plc: 0 }
const badge = (id, cls, txt) => { const e = $(id); e.className = 'badge ' + cls; e.textContent = txt }

function alRecibir(msg) {
  if (!msg) return
  if (Auth.onMsg(msg)) return                                  // mensajes de login/sesión
  if (msg.topic === 'plc') { aplicarPlc(msg.payload); T.plc = Date.now() }
  else if (msg.topic === 'micro') { if (msg.payload && T[msg.payload.src] !== undefined) T[msg.payload.src] = Date.now() }
  else if (msg.topic === 'status') Object.assign(S, msg.payload)
  render()
}
function conectar() {
  if (typeof uibuilder === 'undefined') { demo(); return }
  uibuilder.onChange('msg', alRecibir)                       // uibuilder v7 arranca solo (no llamar start())
  enviar = (topic, payload) => uibuilder.send(Auth.wrap({ topic, payload }))
  uibuilder.onChange('ioConnected', c => { if (c) enviar('hello') })
  if (uibuilder.ioConnected) enviar('hello')
}
setInterval(() => {
  const ahora = Date.now(), ok = k => ahora - T[k] < 5000
  const real = S.modo === 'default' || S.modo === 'manual'
  for (const [id, k, n] of [['b-arduino', 'arduino', 'Arduino'], ['b-esp32', 'esp32', 'ESP32'], ['b-esp8266', 'esp8266', 'ESP8266']]) {
    if (S.modo === 'simulacion') badge(id, 'sim', n + ' simulado')
    else if (S.modo === 'pruebas') badge(id, 'warn', n + ' desconectado')
    else badge(id, ok(k) ? 'on' : 'off', n + (ok(k) ? ' OK' : ' sin datos'))
  }
  if (S.modo === 'simulacion') badge('b-plc', 'sim', 'PLC simulado')
  else if (S.modo === 'pruebas') badge('b-plc', 'warn', 'PLC con datos de prueba')
  else badge('b-plc', ok('plc') ? 'on' : 'off', 'PLC ' + (ok('plc') ? 'OK' : 'sin datos'))
  if (S.modo === 'manual' && ok('plc') && S.modo_manual === false) badge('b-plc', 'warn', 'PLC: Manual sin confirmar')
}, 1000)

// ---- Modo demo (index.html abierto sin Node-RED) ------------------------------------------------
function demo() {
  let t = 0
  S.modo = 'simulacion'; S.rec_nombre = 'Irlandes'
  setInterval(() => {
    t++
    const ph = t % 16
    Object.assign(S, { fer_temp: 20 + Math.sin(t / 5), des_temp: 40 + 40 * Math.abs(Math.sin(t / 20)), fer_vol: 30 + 20 * Math.sin(t / 15), des_vol: 25, rec_vol: 10, recir_vol: 3,
      rec_sp_fer: 20, rec_hist_fer: .5, rec_sp_des: 88, rec_hist_des: 1, act_bombawash: ph < 5, act_resistencia: ph > 5, act_aguadest: ph === 3, act_bombarecirc: ph > 11, act_valvrecirc: ph > 11, act_vaciado: ph > 13 })
    render()
  }, 1000)
}

// ---- UI ------------------------------------------------------------------------------------------
function clicComp(c) {
  if (S.modo === 'manual') {
    const b = MAN[c]; if (!b) return
    S[b] = !S[b]; S[RAW[c]] = S[b]                         // optimista; el PLC lo confirma en la próxima lectura
    const hasta = Date.now() + 2500                        // mientras tanto, lecturas viejas del PLC no pisan el cambio (si no, titila)
    PEND[b] = { v: S[b], hasta }; PEND[RAW[c]] = { v: S[b], hasta }
    enviar('manual', { name: b, value: S[b] })
  } else if (S.modo === 'pruebas') {
    const n = (compColor(c) + 1) % 3
    S[RAW[c]] = n === 1; F[c] = n === 2
    if (c === 'xv201' || c === 'p201' || c === 'agua') { F.xv201 = F.p201 = F.agua = n === 2 }
  } else return
  render()
}
function ligarUI() {
  document.querySelectorAll('#modos button').forEach(b => b.onclick = () => { S.modo = b.dataset.modo; enviar('modo', S.modo); render() })
  $('btn-inicio').onclick = () => enviar('btn', 'iniciar')
  $('btn-extraer').onclick = () => enviar('btn', 'extraer')
  document.querySelectorAll('button[data-sp]').forEach(b => b.onclick = () => {
    if (S.modo !== 'default') return
    const k = b.dataset.sp, v = parseFloat($('in-' + k).value); if (isFinite(v)) enviar('set_sp', { name: k, value: v })
  })
  document.querySelectorAll('button[data-test]').forEach(b => b.onclick = () => {
    if (S.modo !== 'pruebas') return
    const k = b.dataset.test, el = $('t-' + k), v = k === 'rec_nombre' ? el.value : parseFloat(el.value)
    if (v === '' || (k !== 'rec_nombre' && !isFinite(v))) return
    S[k] = v; enviar('test_set', { name: k, value: v }); render()
  })
  document.querySelectorAll('[data-alm]').forEach(c => c.onchange = () => { if (S.modo === 'pruebas') { S[c.dataset.alm] = c.checked; enviar('test_set', { name: c.dataset.alm, value: c.checked }); render() } })
  $('lienzo').addEventListener('click', e => { const h = e.target.closest('[data-c]'); if (h) clicComp(h.dataset.c) })
}
window.addEventListener('load', () => { dibujarTuberias(); ligarUI(); render(); conectar() })
