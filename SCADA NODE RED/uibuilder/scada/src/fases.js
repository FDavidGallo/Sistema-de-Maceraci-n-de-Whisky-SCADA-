'use strict'
const ALMS = { alm_sobrecalentado: 'Fermentador sobrecalentado', alm_ferm_lleno: 'Fermentador lleno', alm_demasiado_wash: 'Demasiado wash en el destilador',
  alm_aviso_metanol: 'Aviso de descarte de metanol', alerta_agua_baja: 'Agua baja en el destilador', alm_falla_uss: 'Falla USS' }
const FASES = [['fer_llenando', 'Llenado del fermentador'], ['fase_ferm_terminada', 'Fermentación terminada'], ['fase_limpieza_ini', 'Limpieza del fermentador'],
  ['fase_inicio_destilado', 'Pesaje previo al clarificado'], ['fase_inicio_clarificado', 'Clarificado (agregado de agua)'], ['fase_descarte_metanol', 'Descarte de metanol'],
  ['fase_descarte_exitoso', 'Descarte exitoso'], ['fase_inicio_recirculado', 'Recirculado'], ['fase_fin_recirculado', 'Fin de recirculado'], ['fase_whisky_terminado', 'Whisky terminado']]
let eventos = []

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
function render() {
  $('lista-fases').innerHTML = FASES.map(([k, n]) => {
    let cls = ''
    if (k === 'fer_llenando') cls = S.fer_llenando ? 'act' : (S.fase_ferm_terminada ? 'ok' : '')
    else if (k === 'fase_limpieza_ini') cls = S.fase_limpieza_fin ? 'ok' : S.fase_limpieza_ini ? 'act' : ''
    else cls = S[k] ? 'ok' : ''
    return `<div class="fi ${cls}"><i></i>${n}</div>`
  }).join('') + `<p class="nota">Fase actual: <b>${faseTxt()}</b></p>`
  $('lista-alarmas').innerHTML = Object.entries(ALMS).map(([k, n]) => `<div class="fi ${S[k] ? 'alm' : ''}"><i></i>${n}${S[k] ? ' — ACTIVA' : ''}</div>`).join('')
  $('lista-eventos').innerHTML = eventos.length ? eventos.slice(0, 100).map(e =>
    `<div class="ev"><span>${fechaHora(e.t)}</span><span>${e.txt}</span><span class="${e.alm ? (e.on ? 'r' : 'v') : ''}">${e.alm ? (e.on ? 'Alarma ON' : 'Alarma OFF') : (e.on ? 'Fase iniciada' : 'Fase terminada')}</span><span>${e.modo}</span></div>`).join('')
    : '<p class="nota">Todavía no hay eventos.</p>'
}
const pedir = () => enviar('get_hist', { since: Date.now() })      // since reciente: solo interesan los eventos, no las muestras
function alRecibir(msg) {
  if (msg.topic === 'hist') eventos = (msg.payload && msg.payload.ev) || eventos
  else if (msg.topic === 'hist_cleared') eventos = []
  render()
}
window.addEventListener('load', () => {
  const chk = $('chk-popup'); chk.checked = Alarmas.habilitado(); chk.onchange = () => { Alarmas.setHabilitado(chk.checked); if (chk.checked) Alarmas.revisar(S) }
  $('btn-borrar').onclick = () => { if (confirm('¿Borrar el registro de eventos (y el historial de muestras) del servidor?')) enviar('clear_hist') }
  setInterval(pedir, 5000)
  render()
  conectar(alRecibir, pedir)
})
