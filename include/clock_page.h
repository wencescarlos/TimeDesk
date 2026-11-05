/**
 * clock_page.h - Página web "Estilos de reloj" (/clock)
 *
 * Se guarda en flash (PROGMEM) y se envía en dos trozos; entre ambos el
 * servidor inyecta el estado actual: var CUR=<estilo>,START=<0|1>,BR=<brillo>;
 * Las vistas previas son capturas reales del framebuffer que genera el propio
 * dispositivo en /clock-preview.bmp?s=<estilo>.
 */
#ifndef CLOCK_PAGE_H
#define CLOCK_PAGE_H

#include <Arduino.h>

static const char CLOCK_PAGE_HEAD[] PROGMEM = R"HTML(<!DOCTYPE html><html><head><meta charset='UTF-8'>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<title>TimeDesk · Estilos de reloj</title><style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:'Segoe UI',sans-serif;background:#0f0e17;color:#fffffe;min-height:100vh}
.header{background:linear-gradient(135deg,#16213e,#1a1a2e);padding:24px;text-align:center;border-bottom:2px solid #4cc9f0}
.header h1{font-size:1.6em;color:#4cc9f0}.header p{color:#adb5bd;margin-top:6px}
.container{max-width:760px;margin:0 auto;padding:16px}
.card{background:#1a1a2e;border-radius:14px;padding:20px;margin:12px 0;box-shadow:0 4px 20px rgba(0,0,0,.3)}
.card h2{color:#e0aaff;font-size:1.1em;margin-bottom:12px;padding-bottom:8px;border-bottom:1px solid #333}
.card h3{color:#adb5bd;font-size:.85em;text-transform:uppercase;letter-spacing:.08em;margin:18px 0 10px}
.card h3:first-of-type{margin-top:4px}
.nav-links{display:flex;gap:10px;justify-content:center;margin:16px 0;flex-wrap:wrap}
.nav-links a{color:#4cc9f0;text-decoration:none;padding:8px 16px;border:1px solid #4cc9f0;border-radius:8px;font-size:.9em}
.nav-links a:hover{background:#4cc9f0;color:#000}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:12px}
.st{background:#16213e;border:2px solid transparent;border-radius:12px;padding:8px;cursor:pointer;transition:all .2s;text-align:left;color:inherit;font:inherit;position:relative}
.st:hover{transform:translateY(-2px);border-color:#334}
.st.on{border-color:#4cc9f0;box-shadow:0 0 0 3px rgba(76,201,240,.25)}
.pv{width:100%;aspect-ratio:240/320;border-radius:6px;background:#0b0b14;display:flex;align-items:center;justify-content:center;overflow:hidden}
.pv img{width:100%;height:100%;display:block;image-rendering:pixelated}
.pv span{color:#555;font-size:.8em;padding:8px;text-align:center}
.st b{display:block;margin-top:8px}
.st small{color:#adb5bd;font-size:.8em;line-height:1.3}
.st .tag{position:absolute;top:14px;right:14px;font-size:.75em;background:#4cc9f0;color:#000;padding:2px 8px;border-radius:10px;font-weight:bold;display:none}
.st.on .tag{display:inline}
.sw-row{display:flex;align-items:center;gap:10px;margin:10px 0}
.switch{position:relative;display:inline-block;width:48px;height:26px;flex:none}
.switch input{opacity:0;width:0;height:0}
.slider{position:absolute;cursor:pointer;inset:0;background:#333;border-radius:26px;transition:.3s}
.slider:before{position:absolute;content:'';height:20px;width:20px;left:3px;bottom:3px;background:#fff;border-radius:50%;transition:.3s}
input:checked+.slider{background:#4cc9f0}
input:checked+.slider:before{transform:translateX(22px)}
label{color:#adb5bd;font-size:.9em}
input[type=range]{width:100%;accent-color:#4cc9f0;margin:8px 0}
.btn{width:100%;padding:14px;border:none;border-radius:10px;font-size:1.05em;cursor:pointer;font-weight:bold;margin-top:10px;background:linear-gradient(135deg,#4cc9f0,#7209b7);color:#fff}
.btn:hover{opacity:.85}
#toast{position:fixed;left:50%;bottom:24px;transform:translateX(-50%) translateY(80px);background:#16213e;border:1px solid #4cc9f0;color:#fff;padding:10px 18px;border-radius:10px;transition:transform .3s,visibility .3s;pointer-events:none;visibility:hidden}
#toast.show{transform:translateX(-50%) translateY(0);visibility:visible}
#toast.err{border-color:#e63946}
</style></head><body>
<div class='header'><h1>&#127912; Estilos de reloj</h1><p>Vistas previas reales de tu TimeDesk · los cambios se aplican al instante</p></div>
<div class='container'>
<div class='nav-links'><a href='/'>&#127968; Inicio</a><a href='/clock'>&#127912; Reloj</a><a href='/schedule'>&#9200; Horario</a><a href='/estado'>&#128202; Estado</a><a href='/update'>&#128260; OTA</a></div>
<div class='card'><h2>&#9200; Elige un estilo</h2><div id='groups'></div></div>
<div class='card'><h2>&#9881;&#65039; Opciones de pantalla</h2>
<div class='sw-row'><label class='switch'><input type='checkbox' id='start'><span class='slider'></span></label><span>Arrancar en el reloj a pantalla completa</span></div>
<label for='br'>Brillo: <b id='brv'></b></label>
<input type='range' id='br' min='10' max='100' step='5'>
<button class='btn' id='show'>&#128250; Mostrar el reloj en la pantalla ahora</button>
</div></div>
<div id='toast'></div>
<script>
)HTML";

static const char CLOCK_PAGE_TAIL[] PROGMEM = R"HTML(
var S=[
 ['Clásico','El diseño original, limpio y legible'],
 ['Retro','LCD ámbar de 7 segmentos de los 80'],
 ['Anime','Viñeta manga, brillos y carita kawaii'],
 ['Arcade','Pixel art 8-bit con barra de experiencia'],
 ['Portátil','Pantalla verde de consola de bolsillo'],
 ['Futurista','Panel sci-fi con anillo de segundos'],
 ['Analógico','Esfera con agujas y segundero'],
 ['Escritorio 1-bit','Ventanas monocromas de los 80'],
 ['Escritorio 3D','Ventanas biseladas y barra de tareas de los 90'],
 ['Terminal','Sesión de shell en fósforo verde'],
 ['Micro 8-bit','Microordenador doméstico con BASIC'],
 ['Aventura','Fantasía 16-bit con cofre y diálogo'],
 ['Velocidad','Carreras arcade 16-bit al atardecer'],
 ['Plataformas','Bloques, nubes y una mascota que salta'],
 ['Según el tiempo','La escena y los colores cambian con el tiempo real'],
 ['Noche estrellada','Constelaciones, estrella fugaz y la fase lunar real'],
 ['Acuario','Peces, algas y la hora dentro de una burbuja'],
 ['Osciloscopio','La hora trazada por el haz, con onda y lecturas'],
 ['Nixie','Tubos de neón naranja con rejilla y zócalo'],
 ['Flip clock','Tarjetas abatibles que caen al cambiar el minuto']];
var G=[['Relojes',[0,6,19,18,1,5]],['Ambientes',[14,15,16,2]],['Informática y electrónica',[7,8,9,10,17]],['Videojuegos',[3,4,11,12,13]]];
var cur=typeof CUR=='number'?CUR:0,cards={},queue=[],busy=false;
function $(i){return document.getElementById(i)}
function toast(m,e){var t=$('toast');t.textContent=m;t.className='show'+(e?' err':'');clearTimeout(t._h);t._h=setTimeout(function(){t.className=''},1800)}
function post(q,ok){return fetch('/clock-save',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:q})
 .then(function(r){if(!r.ok)throw 0;return r.json()}).then(function(){toast(ok)}).catch(function(){toast('Error al guardar',1)})}
function mark(){for(var i in cards)cards[i].classList.toggle('on',+i==cur)}
// Las vistas previas se piden de una en una: el ESP8266 atiende peticiones de forma secuencial
function load(i,front){if(queue.indexOf(i)<0){front?queue.unshift(i):queue.push(i)}next()}
function next(){if(busy||!queue.length)return;busy=true;var i=queue.shift(),pv=cards[i].querySelector('.pv'),img=new Image();
 img.onload=function(){pv.innerHTML='';pv.appendChild(img);busy=false;next()};
 img.onerror=function(){pv.innerHTML='<span>Vista previa no disponible</span>';busy=false;next()};
 img.alt=S[i][0];img.src='/clock-preview.bmp?s='+i+'&t='+Date.now()}
G.forEach(function(g){var h=document.createElement('h3');h.textContent=g[0];$('groups').appendChild(h);
 var grid=document.createElement('div');grid.className='grid';$('groups').appendChild(grid);
 g[1].forEach(function(i){var b=document.createElement('button');b.type='button';b.className='st';
  b.innerHTML="<div class='pv'><span>Cargando…</span></div><span class='tag'>&#10004; Activo</span><b></b><small></small>";
  b.querySelector('b').textContent=S[i][0];b.querySelector('small').textContent=S[i][1];
  b.onclick=function(){cur=i;mark();post('style='+i,'Estilo "'+S[i][0]+'" aplicado')};
  grid.appendChild(b);cards[i]=b})});
mark();
load(cur);G.forEach(function(g){g[1].forEach(function(i){load(i)})});
// Refresca la vista previa del estilo activo cada 20 s (la hora avanza)
setInterval(function(){load(cur,true)},20000);
var st=$('start'),br=$('br'),bv=$('brv');
st.checked=typeof START!='undefined'&&START==1;br.value=typeof BR=='number'?BR:100;bv.textContent=br.value+'%';
st.onchange=function(){post('start_clock='+(st.checked?1:0),st.checked?'Arrancará en el reloj':'Arrancará en la pantalla principal')};
br.oninput=function(){bv.textContent=br.value+'%'};br.onchange=function(){post('brightness='+br.value,'Brillo '+br.value+'%')};
$('show').onclick=function(){post('show=1','Reloj mostrado en la pantalla')};
</script></body></html>
)HTML";

#endif
