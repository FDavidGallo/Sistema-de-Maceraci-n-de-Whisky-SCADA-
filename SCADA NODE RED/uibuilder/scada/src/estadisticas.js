'use strict'
let hist = [], ciclos = 0, litros = 0

function grafico(cv, d, series) {
  const dpr = window.devicePixelRatio || 1, w = cv.clientWidth, h = cv.clientHeight
  cv.width = w * dpr; cv.height = h * dpr
  const g = cv.getContext('2d'); g.scale(dpr, dpr); g.clearRect(0, 0, w, h)
  const L = 44, R = 10, Tp = 10, B = 22
  g.font = '11px system-ui'; g.fillStyle = '#8b949e'; g.strokeStyle = '#2a323d'; g.lineWidth = 1
  if (d.length < 2) { g.fillText('Juntando datos… (1 muestra cada 5 s)', L + 10, h / 2); return }
  let mn = Infinity, mx = -Infinity
  for (const s of series) for (const p of d) { const v = p[s.k]; if (v != null && isFinite(v)) { mn = Math.min(mn, v); mx = Math.max(mx, v) } }
  if (!isFinite(mn)) return
  if (mx - mn < 1) { mx += 0.5; mn -= 0.5 }
  const pad10 = (mx - mn) * 0.1; mn -= pad10; mx += pad10
  const t0 = d[0].t, t1 = d[d.length - 1].t
  const X = t => L + (t - t0) / (t1 - t0 || 1) * (w - L - R), Y = v => Tp + (1 - (v - mn) / (mx - mn)) * (h - Tp - B)
  for (let i = 0; i <= 4; i++) { const v = mn + (mx - mn) * i / 4, y = Y(v); g.beginPath(); g.moveTo(L, y); g.lineTo(w - R, y); g.stroke(); g.fillText(v.toFixed(1), 4, y + 4) }
  g.fillText(fechaHora(t0), L, h - 6); g.fillText(fechaHora(t1), w - R - 80, h - 6)
  for (const s of series) {
    g.strokeStyle = s.c; g.lineWidth = 2; g.beginPath(); let en = false
    for (const p of d) { const v = p[s.k]; if (v == null || !isFinite(v)) { en = false; continue } const x = X(p.t), y = Y(v); en ? g.lineTo(x, y) : g.moveTo(x, y); en = true }
    g.stroke()
  }
}

function render() {
  const horas = Number($('rango').value), desde = Date.now() - horas * 3600e3
  const d = hist.filter(p => p.t >= desde)
  const paso = Math.max(1, Math.ceil(d.length / 600)), dd = d.filter((_, i) => i % paso === 0)   // máx. ~600 puntos por gráfico
  grafico($('g-temp'), dd, [{ k: 'ft', c: '#58a6ff' }, { k: 'dt', c: '#f0883e' }])
  grafico($('g-vol'), dd, [{ k: 'fv', c: '#58a6ff' }, { k: 'dv', c: '#f0883e' }, { k: 'rv', c: '#3fb950' }, { k: 'cv', c: '#d2a8ff' }])
  const max = k => { const v = d.map(p => p[k]).filter(x => x != null && isFinite(x)); return v.length ? Math.max(...v).toFixed(1) : '--' }
  $('s-ciclos').textContent = ciclos; $('s-litros').textContent = Number(litros).toFixed(1)
  $('s-maxd').textContent = max('dt'); $('s-maxf').textContent = max('ft')
}

function pedir() { enviar('get_hist', { since: hist.length ? hist[hist.length - 1].t : 0 }) }
function alRecibir(msg) {
  if (msg.topic === 'hist') {
    const p = msg.payload || {}
    hist = p.full ? (p.hist || []) : hist.concat(p.hist || [])
    ciclos = p.ciclos || 0; litros = p.litros || 0
    render()
  } else if (msg.topic === 'hist_cleared') { hist = []; ciclos = 0; litros = 0; render() }
}
window.addEventListener('load', () => {
  $('rango').onchange = render
  $('btn-borrar').onclick = () => { if (confirm('¿Borrar todo el historial (muestras y eventos) del servidor?')) enviar('clear_hist') }
  window.addEventListener('resize', render)
  setInterval(pedir, 10000)
  render()
  conectar(alRecibir, pedir)
})
