/* Login del SCADA (front). Se carga ANTES de common.js / index.js en todas las páginas.
   - El token de sesión vive en sessionStorage (se pierde al cerrar la pestaña).
   - Cada mensaje al servidor lleva msg.authToken; el servidor (nodo "Autenticación") descarta lo que no esté autorizado.
   Recibe: 'login_ok' {token, usuario, defecto}, 'login_fail' "texto", 'auth_required', 'auth_ok' {usuario, defecto}, 'logout_ok'.
   Envía:  'login' {usuario, clave}, 'ping', 'logout'. */
'use strict'
const Auth = (() => {
  const K = 'scada_token', KU = 'scada_user', KD = 'scada_defecto'
  const ss = {
    get: k => { try { return sessionStorage.getItem(k) } catch (e) { return null } },
    set: (k, v) => { try { sessionStorage.setItem(k, v) } catch (e) { /* sin storage: no hay sesión persistente */ } },
    del: k => { try { sessionStorage.removeItem(k) } catch (e) { /* nada */ } },
  }
  const el = id => document.getElementById(id)
  const token = () => ss.get(K)
  const usuario = () => ss.get(KU)
  const hayUib = () => typeof uibuilder !== 'undefined'
  const wrap = m => { const t = token(); if (t) m.authToken = t; return m }      // agrega el token a un mensaje
  const enviar = (topic, payload) => { if (hayUib()) uibuilder.send(wrap({ topic, payload })) }
  const limpiar = () => { ss.del(K); ss.del(KU); ss.del(KD) }

  function mostrarLogin(aviso) {
    let ov = el('auth-overlay')
    if (!ov) {
      ov = document.createElement('div'); ov.id = 'auth-overlay'
      ov.innerHTML = '<img class="auth-logo" src="./img/logo_barril.webp" alt="Whiskey">' +
        '<form id="auth-form" class="auth-card" autocomplete="on">' +
        '<h1>Destilería · SCADA</h1><p>Iniciá sesión para continuar</p>' +
        '<input id="auth-user" type="text" placeholder="Usuario" autocomplete="username" autocapitalize="off" spellcheck="false">' +
        '<input id="auth-pass" type="password" placeholder="Contraseña" autocomplete="current-password">' +
        '<button id="auth-entrar" type="submit">Entrar</button><div id="auth-err" class="auth-err"></div></form>'
      document.body.appendChild(ov)
      el('auth-form').addEventListener('submit', e => {
        e.preventDefault()
        const u = el('auth-user').value.trim(), c = el('auth-pass').value
        if (!u || !c) { error('Completá usuario y contraseña'); return }
        error(''); el('auth-entrar').disabled = true
        enviar('login', { usuario: u, clave: c })
        setTimeout(() => { const b = el('auth-entrar'); if (b) b.disabled = false }, 3000)
      })
    }
    ov.style.display = 'flex'
    document.body.classList.add('auth-bloqueado')
    error(aviso || '')
    setTimeout(() => { const i = el('auth-user'); if (i) i.focus() }, 50)
  }
  function error(t) { const e = el('auth-err'); if (e) e.textContent = t || ''; const b = el('auth-entrar'); if (b && t) b.disabled = false }

  function chip() {                                           // "👤 admin · Salir" al final del header
    const h = document.querySelector('header'); if (!h || el('auth-chip') || !token()) return
    const d = document.createElement('div'); d.id = 'auth-chip'; d.className = 'auth-chip'
    d.innerHTML = '<a id="auth-defecto" href="./usuarios.html" title="Cambiá la contraseña" style="display:none">⚠ contraseña por defecto</a>' +
      '<span>👤 <b id="auth-nombre"></b></span><button id="auth-salir" class="mini" type="button">Salir</button>'
    h.appendChild(d)
    el('auth-nombre').textContent = usuario() || ''
    el('auth-salir').onclick = () => enviar('logout')
    defecto(ss.get(KD) === '1')
  }
  function defecto(v) { ss.set(KD, v ? '1' : ''); const a = el('auth-defecto'); if (a) a.style.display = v ? 'inline' : 'none' }

  // Devuelve true si el mensaje era de autenticación (la página no tiene que procesarlo)
  function onMsg(m) {
    if (!m) return false
    switch (m.topic) {
      case 'login_ok': ss.set(K, m.payload.token); ss.set(KU, m.payload.usuario); ss.set(KD, m.payload.defecto ? '1' : ''); location.reload(); return true
      case 'login_fail': error(typeof m.payload === 'string' ? m.payload : 'No se pudo iniciar sesión'); return true
      case 'auth_required': { const tenia = !!token(); limpiar(); const c = el('auth-chip'); if (c) c.remove(); mostrarLogin(tenia ? 'La sesión venció. Volvé a ingresar.' : ''); return true }
      case 'auth_ok': if (m.payload) { if (m.payload.usuario) ss.set(KU, m.payload.usuario); defecto(!!m.payload.defecto); const n = el('auth-nombre'); if (n) n.textContent = usuario() || '' } return true
      case 'logout_ok': limpiar(); location.reload(); return true
    }
    return false
  }

  function init() {
    if (!hayUib()) return                                      // página abierta sin Node-RED (demo): sin login
    if (!token()) mostrarLogin(); else chip()
    setInterval(() => { if (token()) enviar('ping') }, 30000)   // mantiene la sesión y detecta si venció
    document.addEventListener('click', e => {                   // descargas CSV: el servidor también pide el token
      const a = e.target.closest && e.target.closest('a[data-csv]'); if (a) a.href = a.dataset.csv + '?t=' + encodeURIComponent(token() || '')
    })
  }
  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', init); else init()
  return { token, usuario, wrap, enviar, onMsg, defecto }
})()
