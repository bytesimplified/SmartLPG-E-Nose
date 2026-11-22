#pragma once
// Served only on the ESP32's opt-in local HTTP listener.
// HTTP Basic authentication is NOT encrypted; use an isolated trusted LAN.
static const char DASHBOARD_PAGE[] PROGMEM = R"ENOSENODE(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>SmartLPG E-Nose research dashboard</title>
  <style>
    :root{font-family:system-ui,sans-serif;color:#182331;background:#f2f5fa}
    body{max-width:940px;margin:18px auto;padding:0 14px}
    h1{font-size:1.5rem;margin:0 0 8px}p{line-height:1.45}
    .notice{background:#fff8e7;border-left:4px solid #d28a03;padding:12px;margin:14px 0}
    .cards{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:12px}
    .panel{background:white;border:1px solid #dde3eb;border-radius:12px;padding:14px}
    .big{font-size:1.5rem;font-weight:700;margin:5px 0}
    .muted{color:#596574;font-size:.9rem}
    .state{padding:12px;border-radius:9px;background:#f1f5f9;font-weight:700}
    .state.warn{background:#fff0d8;color:#7a4200}
    .state.danger{background:#ffe1e1;color:#9d1426}
    .state.ready{background:#e9f3ff;color:#155596}
    canvas{width:100%;height:240px;display:block}
    a{color:#174c9b} .footer{margin:20px 0;color:#596574}
  </style>
</head>
<body>
  <h1>SmartLPG E-Nose — Experimental Dashboard</h1>
  <div class="notice"><b>Not a safety detector.</b> These signals are not LPG ppm or %LEL. Keep an independent certified LPG alarm. Never release LPG in a kitchen to test this device.</div>
  <div id="status" class="state">Connecting to local ESP32…</div>
  <div class="cards" style="margin-top:12px">
    <div class="panel"><div class="muted">MQ-6 estimated AO</div><div class="big" id="mq6">—</div></div>
    <div class="panel"><div class="muted">MQ-2 estimated AO</div><div class="big" id="mq2">—</div></div>
    <div class="panel"><div class="muted">Temperature</div><div class="big" id="temp">—</div></div>
    <div class="panel"><div class="muted">Relative humidity</div><div class="big" id="rh">—</div></div>
  </div>
  <section class="panel" style="margin-top:12px">
    <h2 style="font-size:1.1rem">Recent sensor trends (browser session)</h2>
    <canvas id="chart" role="img" aria-label="Recent MQ-6 and MQ-2 output voltages"></canvas>
    <div class="muted">Blue: MQ-6 • Green: MQ-2. Approximate millivolts only. History is not retained across page reloads.</div>
  </section>
  <section class="panel" style="margin-top:12px">
    <h2 style="font-size:1.1rem">Device and experiment</h2>
    <p>Fault: <b id="fault">—</b></p>
    <p>Activity annotation: <b id="label">—</b></p>
    <p>Device buffer: <b id="rows">—</b> recent readings</p>
    <p>Last update: <b id="updated">—</b></p>
    <p><a href="/csv">Download device CSV (last 300 samples)</a></p>
  </section>
  <p class="footer">The web UI requires a trusted local network. HTTP Basic authentication is not HTTPS and does not encrypt traffic; do not port-forward this device to the internet. No cloud or phone notification is sent.</p>
  <script>
    'use strict';
    const points = [];
    const MAX_POINTS = 120;
    const chart = document.querySelector('#chart');
    const ctx = chart.getContext('2d');
    const el = id => document.getElementById(id);
    const format = (n, unit, decimals) => Number.isFinite(Number(n)) && n !== null
      ? Number(n).toFixed(decimals) + ' ' + unit : 'unavailable';

    function plot() {
      const ratio = window.devicePixelRatio || 1;
      const w = Math.max(250, chart.clientWidth), h = 240;
      chart.width = Math.round(w * ratio);
      chart.height = Math.round(h * ratio);
      ctx.setTransform(ratio,0,0,ratio,0,0);
      ctx.clearRect(0,0,w,h);
      const left = 56, right = w-16, top = 16, bottom = h-35;
      ctx.strokeStyle = '#cbd5e1';ctx.lineWidth=1;
      ctx.beginPath();ctx.moveTo(left,top);ctx.lineTo(left,bottom);ctx.lineTo(right,bottom);ctx.stroke();
      if (!points.length) {ctx.fillStyle='#596574';ctx.fillText('Collecting samples…',left+18,top+50);return;}
      const values = points.flatMap(p => [p.mq6,p.mq2]).filter(Number.isFinite);
      if (!values.length) return;
      const low = Math.max(0,Math.min(...values)-75), high=Math.max(...values)+75;
      const span = Math.max(1,high-low);
      ctx.font='12px system-ui';ctx.fillStyle='#596574';
      for(let k=0;k<=4;k++){
        const y=top+(bottom-top)*k/4;
        const v=high-span*k/4;
        ctx.fillText(String(Math.round(v)),4,y+4);
        ctx.strokeStyle='#e6ecf3';ctx.beginPath();ctx.moveTo(left,y);ctx.lineTo(right,y);ctx.stroke();
      }
      function line(key,color){
        ctx.strokeStyle=color;ctx.lineWidth=2;ctx.beginPath();
        points.forEach((p,i)=>{
          const x=left+(right-left)*(points.length===1 ? 0.5 : i/(points.length-1));
          const y=bottom-(p[key]-low)/span*(bottom-top);
          if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y);
        });
        ctx.stroke();
      }
      line('mq6','#2563eb');line('mq2','#169953');
      ctx.fillStyle='#596574';ctx.fillText('Earlier',left,bottom+18);ctx.fillText('Latest',right-38,bottom+18);
    }

    async function refresh(){
      try {
        const r=await fetch('/api',{cache:'no-store'});
        if(!r.ok)throw new Error('HTTP '+r.status);
        const d=await r.json();
        el('mq6').textContent=format(d.mq6_mV,'mV',0);
        el('mq2').textContent=format(d.mq2_mV,'mV',0);
        el('temp').textContent=d.env_valid ? format(d.temperature_c,'°C',1) : 'unavailable';
        el('rh').textContent=d.env_valid ? format(d.humidity_pct,'%',1) : 'unavailable';
        el('fault').textContent=d.fault ? (d.fault_reason || 'attention') : 'none reported';
        el('label').textContent=d.activity_label || 'unlabeled';
        el('rows').textContent=d.recorded_rows ?? '0';
        el('updated').textContent=new Date().toLocaleTimeString();
        const state=el('status');
        state.textContent=d.status || 'Status unavailable';
        state.className='state '+(d.fault || d.alarm ? 'danger' :
          String(d.status).includes('MONITORING') ? 'ready' : 'warn');
        const mq6=Number(d.mq6_mV),mq2=Number(d.mq2_mV);
        const t=Number(d.uptime_ms);
        if(Number.isFinite(mq6)&&Number.isFinite(mq2)){
          if(points.length && t<points[points.length-1].time)points.length=0;
          points.push({time:t,mq6,mq2});
          if(points.length>MAX_POINTS)points.shift();
          plot();
        }
      }catch(e){
        const state=el('status');
        state.textContent='Dashboard connection unavailable — device status unknown ('+e.message+')';
        state.className='state danger';
      }
    }
    window.addEventListener('resize',plot);
    refresh();
    setInterval(refresh,2000);
  </script>
</body></html>
)ENOSENODE";
