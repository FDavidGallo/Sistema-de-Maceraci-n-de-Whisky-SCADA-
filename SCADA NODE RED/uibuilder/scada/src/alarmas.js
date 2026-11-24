/* Ventana emergente para alarmas de error. Se carga en todas las páginas; index.js y common.js le pasan el estado con Alarmas.revisar(S).
   - Solo avisa cuando una alarma pasa de inactiva a activa (o ya estaba activa al abrir la página y todavía no la aceptaste).
   - "Aceptar" la silencia hasta que se desactive y vuelva a activarse.
   - Se activa/desactiva en fases.html (se guarda en este navegador). */
'use strict'
const Alarmas = (() => {
  const KEY_ON = 'scada_popup_alarmas', KEY_VISTAS = 'scada_alarmas_aceptadas'
  // alarmas de error que abren la ventana ("Aviso de descarte de metanol" es un paso normal del proceso y no se incluye)
  const ALM_POPUP = {
    alm_sobrecalentado: 'Fermentador sobrecalentado',
    alm_ferm_lleno: 'Fermentador lleno',
    alm_demasiado_wash: 'Demasiado wash en el destilador',
    alerta_agua_baja: 'Agua baja en el destilador',
    alm_falla_uss: 'Falla USS',
  }
  const ls = {
    get: k => { try { return localStorage.getItem(k) } catch (e) { return null } },
    set: (k, v) => { try { localStorage.setItem(k, v) } catch (e) { /* sin storage */ } },
  }
  const ss = {
    get: k => { try { return sessionStorage.getItem(k) } catch (e) { return null } },
    set: (k, v) => { try { sessionStorage.setItem(k, v) } catch (e) { /* sin storage */ } },
  }
  const habilitado = () => ls.get(KEY_ON) !== '0'                     // por defecto: activado
  const setHabilitado = v => { ls.set(KEY_ON, v ? '1' : '0'); if (!v) cerrar() }
  const aceptadas = () => { try { return JSON.parse(ss.get(KEY_VISTAS) || '[]') } catch (e) { return [] } }
  const guardarAceptadas = l => ss.set(KEY_VISTAS, JSON.stringify(l))

  let abiertas = []                                                    // claves que muestra la ventana ahora
  const el = id => document.getElementById(id)
  const cerrar = () => { const m = el('alm-modal'); if (m) m.remove(); abiertas = [] }

  function mostrar() {
    let m = el('alm-modal')
    if (!m) {
      m = document.createElement('div'); m.id = 'alm-modal'; m.setAttribute('role', 'alertdialog')
      m.innerHTML = '<div class="alm-card"><h2>⚠ Alarma</h2><ul id="alm-lista"></ul>' +
        '<div class="alm-bot"><a id="alm-ver" href="./fases.html">Ver alarmas</a><button id="alm-ok" type="button">Aceptar</button></div></div>'
      document.body.appendChild(m)
      el('alm-ok').onclick = () => { guardarAceptadas([...new Set(aceptadas().concat(abiertas))]); cerrar() }
      if (/fases\.html$/.test(location.pathname)) el('alm-ver').style.display = 'none'
    }
    el('alm-lista').innerHTML = abiertas.map(k => `<li>${ALM_POPUP[k]}</li>`).join('')
  }

  function revisar(S) {
    if (!S) return
    const activas = Object.keys(ALM_POPUP).filter(k => S[k])
    let acept = aceptadas().filter(k => S[k])                            // las que se apagaron vuelven a poder avisar
    if (acept.length !== aceptadas().length) guardarAceptadas(acept)
    abiertas = abiertas.filter(k => S[k])                                // la ventana muestra solo las que siguen activas
    if (!abiertas.length && el('alm-modal')) cerrar()
    if (!habilitado()) return
    const nuevas = activas.filter(k => !acept.includes(k) && !abiertas.includes(k))
    if (!nuevas.length) return
    abiertas = abiertas.concat(nuevas); mostrar()
  }
  return { revisar, habilitado, setHabilitado, ALM_POPUP }
})()
