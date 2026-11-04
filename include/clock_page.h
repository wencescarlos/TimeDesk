/**
 * clock_page.h - Pagina web "Estilos de Reloj" (/clock)
 *
 * Se guarda en flash (PROGMEM) y se envia en dos trozos; entre ambos el
 * servidor inyecta el estado actual: var CUR=<estilo>,START=<0|1>,BR=<brillo>;
 * Las vistas previas se dibujan en <canvas> con los mismos algoritmos y
 * paletas RGB565 que usa la pantalla (ver src/clock_styles.cpp).
 */
#ifndef CLOCK_PAGE_H
#define CLOCK_PAGE_H

#include <Arduino.h>

static const char CLOCK_PAGE_HEAD[] PROGMEM = R"HTML(<!DOCTYPE html><html><head><meta charset='UTF-8'>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<title>Estilos de Reloj</title><style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:'Segoe UI',sans-serif;background:#0f0e17;color:#fffffe;min-height:100vh}
.header{background:linear-gradient(135deg,#16213e,#1a1a2e);padding:24px;text-align:center;border-bottom:2px solid #4cc9f0}
.header h1{font-size:1.6em;color:#4cc9f0}.header p{color:#adb5bd;margin-top:6px}
.container{max-width:620px;margin:0 auto;padding:16px}
.card{background:#1a1a2e;border-radius:14px;padding:20px;margin:12px 0;box-shadow:0 4px 20px rgba(0,0,0,.3)}
.card h2{color:#e0aaff;font-size:1.1em;margin-bottom:12px;padding-bottom:8px;border-bottom:1px solid #333}
.nav-links{display:flex;gap:10px;justify-content:center;margin:16px 0;flex-wrap:wrap}
.nav-links a{color:#4cc9f0;text-decoration:none;padding:8px 16px;border:1px solid #4cc9f0;border-radius:8px;font-size:.9em}
.nav-links a:hover{background:#4cc9f0;color:#000}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(170px,1fr));gap:12px}
.st{background:#16213e;border:2px solid transparent;border-radius:12px;padding:10px;cursor:pointer;transition:all .2s;text-align:left;color:inherit;font:inherit}
.st:hover{transform:translateY(-2px);border-color:#334}
.st.on{border-color:#4cc9f0;box-shadow:0 0 0 3px rgba(76,201,240,.25)}
.st canvas{width:100%;display:block;border-radius:6px;image-rendering:pixelated;aspect-ratio:240/160}
.st b{display:block;margin-top:8px}
.st small{color:#adb5bd;font-size:.8em}
.st .tag{float:right;font-size:.75em;color:#4cc9f0;display:none}
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
#toast{position:fixed;left:50%;bottom:24px;transform:translateX(-50%) translateY(80px);background:#16213e;border:1px solid #4cc9f0;color:#fff;padding:10px 18px;border-radius:10px;transition:transform .3s;pointer-events:none}
#toast.show{transform:translateX(-50%) translateY(0)}
#toast.err{border-color:#e63946}
</style></head><body>
<div class='header'><h1>&#127912; Estilos de Reloj</h1><p>Los cambios se aplican al instante, sin reiniciar</p></div>
<div class='container'>
<div class='nav-links'><a href='/'>&#127968; Inicio</a><a href='/clock'>&#127912; Reloj</a><a href='/schedule'>&#9200; Horario</a><a href='/status'>&#128202; Estado</a><a href='/update'>&#128260; OTA</a></div>
<div class='card'><h2>&#9200; Elige un estilo</h2><div class='grid' id='grid'></div></div>
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
 {n:'Clásico',d:'El diseño original, limpio y legible',p:[0x0000,0xFFFF,0xFFE0,0x7E3C]},
 {n:'Retro',d:'LCD ámbar de 7 segmentos años 80',p:[0x0000,0xFD20,0xFFB5,0x5180]},
 {n:'Anime',d:'Viñetas manga, brillos y carita kawaii',p:[0x1846,0xFFFF,0xFB56,0x9D5F]},
 {n:'Arcade',d:'Pixel art 8-bit con barra de experiencia',p:[0x0000,0xFFFF,0xFFE0,0xF8A0]},
 {n:'Portátil',d:'Pantalla verde de consola de bolsillo',p:[0x9DE1,0x09C1,0x3306,0x6463]},
 {n:'Futurista',d:'HUD sci-fi con anillo de segundos neón',p:[0x0000,0x07FF,0xF81F,0x0459]}];
var WD=['DOM','LUN','MAR','MIE','JUE','VIE','SAB'],MO=['ENE','FEB','MAR','ABR','MAY','JUN','JUL','AGO','SEP','OCT','NOV','DIC'];
var SEG=[0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F,0x40,0];
var PF={'0':'75557','1':'26227','2':'71747','3':'71717','4':'55711','5':'74717','6':'74757','7':'71122','8':'75757','9':'75717',
'A':'25755','B':'65656','C':'34443','D':'65556','E':'74647','F':'74644','G':'34553','H':'55755','I':'72227','J':'11152',
'K':'55655','L':'44447','O':'25552','P':'65644','Q':'25563','R':'65655','S':'34216','T':'72222',
'U':'55557','V':'55552','X':'55255','Y':'55222','Z':'71247',':':'04040','.':'00004','°':'66000','-':'00700',' ':'00000'};
var PW={'M':[5,17,27,21,17,17],'N':[4,9,13,11,9,9],'W':[5,17,17,21,27,17],':':[1,0,1,0,1,0],'.':[1,0,0,0,0,1]};
function gl(c){if(PW[c])return PW[c];var g=PF[c]||PF[' '];return[3].concat(g.split('').map(Number))}
var X,PAL;
function rgb(v){return 'rgb('+((v>>11)*255/31|0)+','+((v>>5&63)*255/63|0)+','+((v&31)*255/31|0)+')'}
function col(i){X.fillStyle=PAL[i]}
function R(x,y,w,h){X.fillRect(x,y,w,h)}
function sg(x,y,w,h,m){var H=w>=h,t=H?h:w,L=H?w:h,r=t/2;for(var i=0;i<t;i++){var d=i-(t-1)/2,n=m==2?Math.floor(Math.abs(d)):m==1?Math.floor(r-Math.sqrt(Math.max(r*r-d*d,0))):0;if(L-2*n>0){if(H)R(x+n,y+i,L-2*n,1);else R(x+i,y+n,1,L-2*n)}}}
function dg(x,y,w,h,th,c,on,off,m){var g=m==0?(th>>1)+1:1,xL=x+(th>>1),xR=x+w-1-(th>>1),yT=y+(th>>1),yM=y+(h>>1),yB=y+h-1-(th>>1),hh=th>>1;
 for(var p=0;p<2;p++)for(var s=0;s<7;s++){var o=SEG[c]&(1<<s);if(p==0&&o||p==1&&!o)continue;var k=o?on:off;if(k<0)continue;col(k);
  [[xL+g,yT-hh,xR-xL-2*g+1,th],[xR-hh,yT+g,th,yM-yT-2*g+1],[xR-hh,yM+g,th,yB-yM-2*g+1],[xL+g,yB-hh,xR-xL-2*g+1,th],[xL-hh,yM+g,th,yB-yM-2*g+1],[xL-hh,yT+g,th,yM-yT-2*g+1],[xL+g,yM-hh,xR-xL-2*g+1,th]].slice(s,s+1).forEach(function(a){sg(a[0],a[1],a[2],a[3],m)})}}
function ss(s,cx,y,w,h,th,sp,on,off,m,co){var cw=th+4,t=0,i;for(i=0;i<s.length;i++)t+=(s[i]==':'?cw:w)+(i?sp:0);var x=cx-(t>>1);
 for(i=0;i<s.length;i++){var c=s[i];if(c==':'){var k=co?on:off;if(k>=0){col(k);var dx=x+((cw-th)>>1);
  if(m==1){X.beginPath();X.arc(dx+th/2,y+h/3,th/2,0,7);X.arc(dx+th/2,y+2*h/3,th/2,0,7);X.fill()}else{R(dx,y+(h/3|0)-(th>>1),th,th);R(dx,y+(2*h/3|0)-(th>>1),th,th)}}x+=cw+sp}
 else{dg(x,y,w,h,th,c>='0'&&c<='9'?+c:(c=='-'?10:11),on,off,m);x+=w+sp}}return t}
function pw(s,k){var w=0;for(var i=0;i<s.length;i++)w+=(gl(s[i])[0]+1)*k;return w-k}
function pt(s,x,y,k,c,a,sh){s=s.toUpperCase();var w=pw(s,k);if(a==1)x-=w>>1;var b=k>3?k-1:k,so=k>3?(k/3|0)+1:1;
 for(var p=(sh==null?1:0);p<2;p++){col(p?c:sh);var o=p?0:so,cx=x;for(var i=0;i<s.length;i++){var g=gl(s[i]),gw=g[0];
  for(var r=0;r<5;r++)for(var q=0;q<gw;q++)if(g[r+1]&(1<<(gw-1-q)))R(cx+q*k+o,y+r*k+o,b,b);cx+=(gw+1)*k}}}
function txt(s,x,y,sz,c,a,f){col(c);X.font=(f||'bold ')+sz+'px Arial';X.textAlign=a||'center';X.textBaseline='top';X.fillText(s,x,y)}
function spk(x,y,r,c){col(c);R(x-r,y,2*r+1,1);R(x,y-r,1,2*r+1)}
function draw(cv,i,d){X=cv.getContext('2d');PAL=S[i].p.map(rgb);col(0);R(0,0,240,160);
 var h=d.getHours(),m=d.getMinutes(),s=d.getSeconds(),p2=function(n){return(n<10?'0':'')+n};
 var hm=p2(h)+':'+p2(m),hms=hm+':'+p2(s),dt=WD[d.getDay()]+' '+p2(d.getDate())+' '+MO[d.getMonth()];
 if(i==0){txt(dt+' '+d.getFullYear(),120,8,14,1);txt(hms,120,26,36,1);col(3);X.strokeStyle=PAL[3];X.strokeRect(30.5,82.5,180,6);col(2);R(31,83,178*s/59,4);txt('21.5°C',120,100,30,1);txt('cielo claro',120,136,14,2)}
 if(i==1){txt(dt+' '+d.getFullYear(),8,6,11,1,'left','');var t=ss(hms,120,24,21,34,4,4,1,3,0,1);X.strokeStyle=PAL[3];X.strokeRect(120-t/2-6.5,19.5,t+12,42);
  ss(p2(d.getDate())+'-'+p2(d.getMonth()+1),120,80,20,36,4,4,2,-1,0,1);txt('21.5°C  Madrid',120,128,15,1,0,'')}
 if(i==2){X.strokeStyle=PAL[3];for(var a=0;a<36;a++){var an=(a*10+(s%2)*5)*Math.PI/180,r0=a%3?100:85;X.beginPath();X.moveTo(120+Math.cos(an)*r0,80+Math.sin(an)*r0);X.lineTo(120+Math.cos(an)*260,80+Math.sin(an)*260);X.stroke()}
  col(2);X.beginPath();X.roundRect(26,30,188,90,14);X.fill();col(1);X.beginPath();X.roundRect(29,33,182,84,12);X.fill();
  ss(hm,120,46,32,56,9,6,0,-1,1,1);col(2);X.beginPath();X.arc(204,34,15,0,7);X.fill();txt(p2(s),204,27,13,1);txt(dt,120,8,14,2);
  spk(30,134,s%2?6:3,1);spk(206,140,s%2?3:6,2);spk(70,146,4,2)}
 if(i==3||i==4){if(i==4){X.strokeStyle=PAL[2];X.lineWidth=2;X.strokeRect(3,3,234,154);X.lineWidth=1}
  pt(dt,10,10,2,2);pt(hm,120,34,12,1,1,3);pt('SEG '+p2(s),20,104,2,2);for(var b=0;b<20;b++){X.strokeStyle=PAL[3];X.strokeRect(20.5+b*10,118.5,8,9);if(b*3<s+1){col(2);R(21+b*10,119,7,8)}}
  pt('21.5°C',120,138,3,1,1,3)}
 if(i==5){X.strokeStyle=PAL[3];var br=function(x,y,w,h,l){col(3);R(x,y,l,1);R(x,y,1,l);R(x+w-l,y,l,1);R(x+w-1,y,1,l);R(x,y+h-1,l,1);R(x,y+h-l,1,l);R(x+w-l,y+h-1,l,1);R(x+w-1,y+h-l,1,l)};
  br(2,2,236,156,12);txt('// '+WD[d.getDay()]+' '+p2(d.getDate())+'.'+p2(d.getMonth()+1)+'.'+d.getFullYear(),10,8,10,3,'left','');
  for(var k=0;k<60;k++){var an2=(k*6-90)*Math.PI/180,L=k%5?5:10;X.strokeStyle=PAL[k<=s?2:3];X.beginPath();X.moveTo(120+Math.cos(an2)*(62-L),90+Math.sin(an2)*(62-L));X.lineTo(120+Math.cos(an2)*62,90+Math.sin(an2)*62);X.stroke()}
  ss(hm,120,72,18,32,3,4,1,-1,2,s%2==0);ss(p2(s),120,110,8,14,2,2,2,-1,2,1)}}
var cur=typeof CUR=='number'?CUR:0,cvs=[];
function toast(m,e){var t=document.getElementById('toast');t.textContent=m;t.className='show'+(e?' err':'');clearTimeout(t._h);t._h=setTimeout(function(){t.className=''},1800)}
function post(q,ok){fetch('/clock-save',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:q}).then(function(r){if(!r.ok)throw 0;return r.json()}).then(function(){toast(ok)}).catch(function(){toast('Error al guardar',1)})}
function mark(){document.querySelectorAll('.st').forEach(function(e,i){e.classList.toggle('on',i==cur)})}
S.forEach(function(st,i){var b=document.createElement('button');b.type='button';b.className='st';b.innerHTML="<canvas width='240' height='160'></canvas><span class='tag'>&#10004; Activo</span><b>"+st.n+"</b><small>"+st.d+"</small>";
 b.onclick=function(){cur=i;mark();post('style='+i,'Estilo "'+st.n+'" aplicado')};document.getElementById('grid').appendChild(b);cvs.push(b.querySelector('canvas'))});
function tick(){var d=new Date();cvs.forEach(function(c,i){draw(c,i,d)})}
mark();tick();setInterval(tick,1000);
var st=document.getElementById('start'),br=document.getElementById('br'),bv=document.getElementById('brv');
st.checked=typeof START!='undefined'&&START==1;br.value=typeof BR=='number'?BR:100;bv.textContent=br.value+'%';
st.onchange=function(){post('start_clock='+(st.checked?1:0),st.checked?'Arrancará en el reloj':'Arrancará en la pantalla principal')};
br.oninput=function(){bv.textContent=br.value+'%'};br.onchange=function(){post('brightness='+br.value,'Brillo '+br.value+'%')};
document.getElementById('show').onclick=function(){post('show=1','Reloj mostrado en la pantalla')};
</script></body></html>
)HTML";

#endif
