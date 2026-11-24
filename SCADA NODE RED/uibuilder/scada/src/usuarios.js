/* Página Usuarios. Envía: 'usr_list', 'usr_pass' {actual,nueva}, 'usr_add' {usuario,clave}. Recibe: 'usr_list' {usuarios,yo}, 'usr_res' {ok,accion,texto}. */
'use strict'
let usuarios = [], yo = ''
const esc = s => String(s).replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]))

function render() {
  $('lista-usuarios').innerHTML = usuarios.length
    ? usuarios.map(u => `<div class="fi ${u === yo ? 'ok' : ''}"><i></i>${esc(u)}${u === yo ? ' (vos)' : ''}</div>`).join('')
    : '<p class="nota">Sin datos todavía.</p>'
}
function msg(id, ok, texto) { const e = $(id); e.textContent = texto || ''; e.className = 'msg ' + (texto ? (ok ? 'ok' : 'err') : '') }

function alRecibir(m) {
  if (m.topic === 'usr_list') { usuarios = (m.payload && m.payload.usuarios) || []; yo = (m.payload && m.payload.yo) || ''; render() }
  else if (m.topic === 'usr_res') {
    const p = m.payload || {}
    if (p.accion === 'pass') {
      msg('msg-pass', p.ok, p.texto)
      if (p.ok) { $('p-actual').value = $('p-nueva').value = $('p-repetir').value = ''; Auth.defecto(false) }
    } else if (p.accion === 'add') {
      msg('msg-add', p.ok, p.texto)
      if (p.ok) $('u-nombre').value = $('u-clave').value = $('u-repetir').value = ''
    }
  }
}
window.addEventListener('load', () => {
  $('btn-pass').onclick = () => {
    const a = $('p-actual').value, n = $('p-nueva').value, r = $('p-repetir').value
    if (!a || !n) return msg('msg-pass', false, 'Completá todos los campos')
    if (n !== r) return msg('msg-pass', false, 'La contraseña nueva y su repetición no coinciden')
    msg('msg-pass', true, ''); enviar('usr_pass', { actual: a, nueva: n })
  }
  $('btn-add').onclick = () => {
    const u = $('u-nombre').value.trim(), c = $('u-clave').value, r = $('u-repetir').value
    if (!u || !c) return msg('msg-add', false, 'Completá todos los campos')
    if (c !== r) return msg('msg-add', false, 'La contraseña y su repetición no coinciden')
    msg('msg-add', true, ''); enviar('usr_add', { usuario: u, clave: c })
  }
  render()
  conectar(alRecibir, () => enviar('usr_list'))
})
