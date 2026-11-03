#pragma once
#include <Arduino.h>

// Device web UI, styled after the owner's SpendAnalysis app: warm "paper"
// palette, one amber accent for chrome, green/red reserved for meaning, mono
// uppercase micro-labels, cream cards, grouped sidebar and a CRT status panel.
// Single page; the sidebar switches sections by URL hash. All element IDs and
// API calls are part of the device contract (see monitor_web.cpp).
const char WEB_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Solar Monitor</title><link rel="icon" type="image/svg+xml" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'%3E%3Cg stroke='%23E0A23F' stroke-width='3' stroke-linecap='round'%3E%3Ccircle cx='16' cy='16' r='6.5' fill='%23F2B84B'/%3E%3Cpath d='M16 2v4M16 26v4M2 16h4M26 16h4M6.1 6.1l2.8 2.8M23.1 23.1l2.8 2.8M6.1 25.9l2.8-2.8M23.1 8.9l2.8-2.8'/%3E%3C/g%3E%3C/svg%3E"><style>
:root{--bg:#F4F0E7;--side:#F1ECE0;--card:#FCFAF4;--tile:#F6F1E6;--line:#E4DDCC;--ink:#24221C;--muted:#6C675B;--label:#8A8472;
--accent:#B97512;--accent-ink:#97600D;--sel:#E6DCC4;--good:#1F8A5B;--good-bg:#DDEFE3;--bad:#C0392B;--bad-bg:#F6DEDA;--field:#FFFFFF;--on-accent:#FFFFFF;
--solar:#C0842C;--grid:#4A78A8;--battery:#2F7D62;--house:#6F7F95;--idle:#B9B2A0;color-scheme:light}
@media(prefers-color-scheme:dark){:root{--bg:#161512;--side:#131210;--card:#201E18;--tile:#1A1813;--line:#2C2920;--ink:#ECE7D8;--muted:#A9A290;--label:#847C66;
--accent:#E0A23F;--accent-ink:#F0B556;--sel:#2C281D;--good:#36B478;--good-bg:#173526;--bad:#E0644F;--bad-bg:#3A1D18;--field:#14130E;--on-accent:#161512;
--solar:#D09A45;--grid:#6690C0;--battery:#47946E;--house:#8B98AC;--idle:#4A4537;color-scheme:dark}}
*{box-sizing:border-box}html,body{margin:0;background:var(--bg);color:var(--ink);font:15px/1.5 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
.app{display:grid;grid-template-columns:260px minmax(0,1fr);min-height:100vh}
aside{background:var(--side);border-right:1px solid var(--line);padding:20px 18px;position:sticky;top:0;height:100vh;overflow:auto}
.logo{display:flex;align-items:center;gap:10px;font-size:19px;font-weight:600;letter-spacing:-.3px;color:var(--muted);margin:2px 0 22px 2px}.logo b{color:var(--accent);font-weight:600}
.mono,nav h4,.k{font-family:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;text-transform:uppercase;letter-spacing:.07em}
nav h4{font-size:11px;font-weight:400;color:var(--label);margin:18px 0 6px 8px}
nav a{display:block;color:var(--ink);text-decoration:none;padding:6px 10px;border-radius:9px;font-size:14.5px}
nav a:hover{background:var(--tile)}nav a.on{background:var(--sel);font-weight:600}
.crt-h{font:10px ui-monospace,Menlo,monospace;letter-spacing:.1em;text-transform:uppercase;color:#A39C89;margin:26px 0 8px 2px}
.crt{font-family:ui-monospace,SFMono-Regular,Menlo,monospace;background:#131E18;border:1px solid #0C140F;border-radius:9px;padding:12px 13px;box-shadow:inset 0 0 24px rgba(0,0,0,.55);color:#84F3AD}
.crt .t{display:flex;align-items:center;gap:8px;margin-bottom:9px;font-size:10.5px;letter-spacing:.05em;text-transform:uppercase;color:#4D8A67}
.led{width:8px;height:8px;border-radius:99px;background:#84F3AD;box-shadow:0 0 7px #84F3AD}
.crt .r{display:grid;grid-template-columns:58px 1fr;gap:8px;font-size:11.5px;line-height:1.75}.crt .r span:first-child{color:#4D8A67}.crt .r span:last-child{overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
main{padding:30px 44px 70px;max-width:1180px;min-width:0}
.page{display:none}.page.on{display:block}
h1{font-size:40px;line-height:1.15;font-weight:600;letter-spacing:-.5px;margin:4px 0 6px}
h2{font-size:24px;font-weight:600;margin:0 0 14px}.cap{color:var(--muted);margin:0 0 22px}
.head{display:flex;justify-content:space-between;align-items:flex-start;gap:16px}
.pill{display:inline-flex;align-items:center;gap:5px;border-radius:99px;padding:1px 9px;font-size:13px;background:var(--tile);color:var(--muted);white-space:nowrap}
.pill.good{background:var(--good-bg);color:var(--good)}.pill.bad{background:var(--bad-bg);color:var(--bad)}.pill.warn{background:var(--sel);color:var(--accent-ink)}
.tiles{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:14px;margin:0 0 18px}
.tile{background:var(--tile);border:1px solid var(--line);border-radius:9px;padding:14px 16px;min-height:112px}
.k{font-size:11px;color:var(--label);display:block}.v{font-size:30px;font-weight:600;letter-spacing:-.3px;font-variant-numeric:tabular-nums;margin:6px 0 4px;line-height:1.15}
.v small{font-size:15px;color:var(--muted);font-weight:500;margin-left:3px}.tile.stale .v{color:var(--idle)}
.bar{height:6px;border-radius:9px;background:var(--line);overflow:hidden;margin-top:9px}.bar i{display:block;height:100%;background:var(--battery);border-radius:9px;transition:width .6s}
.card{background:var(--card);border:1px solid var(--line);border-radius:9px;padding:22px 24px;margin:0 0 18px}
.card p.note{color:var(--muted);font-size:13.5px;margin:10px 0 0}
svg text{font-family:system-ui,-apple-system,"Segoe UI",sans-serif}
#flow{width:100%;height:auto;display:block;max-height:440px}
.node rect.box{fill:var(--tile);stroke:var(--line)}.node .nk{font:11px ui-monospace,Menlo,monospace;letter-spacing:.07em;fill:var(--label)}
.node .nv{font-size:24px;font-weight:600;fill:var(--ink)}.node .ns{font-size:12.5px;fill:var(--muted)}.node.stale .nv{fill:var(--idle)}
.track{stroke:var(--line);stroke-width:8;stroke-linecap:round}
.band{fill:none;stroke-linecap:round;stroke-opacity:.2;transition:stroke-width .8s}.spine{fill:none;stroke-width:1.2;stroke-linecap:round;stroke-opacity:.35}
.comet .hot{fill:#FFF8E8;opacity:.85}@media(prefers-color-scheme:dark){.band{stroke-opacity:.24}.comet .hot{fill:#FFF3D6;opacity:.7}}
.legend{display:flex;flex-wrap:wrap;gap:16px;font-size:13px;color:var(--muted);margin:8px 0 0}.legend i{display:inline-block;width:12px;height:3px;border-radius:2px;margin-right:6px;vertical-align:middle}
#trend{width:100%;height:250px;display:block}
label{display:block;font-size:14px;margin:14px 0 6px}
input:not([type=checkbox]),select,textarea{font:inherit;width:100%;border:1px solid var(--line);background:var(--field);color:var(--ink);border-radius:9px;padding:9px 11px}
input:focus,select:focus,textarea:focus{outline:2px solid var(--accent);outline-offset:0;border-color:var(--accent)}
textarea{min-height:110px;font:12.5px ui-monospace,Menlo,monospace}input[type=checkbox]{accent-color:var(--accent);margin-right:8px}
.check{display:flex;align-items:center;margin:14px 0 0}.hint{display:block;color:var(--muted);font-size:13px;margin-top:5px}
.row{display:grid;grid-template-columns:1fr 1fr;gap:18px}
button{font:inherit;font-weight:500;border-radius:9px;padding:9px 16px;cursor:pointer;border:1px solid var(--line);background:var(--card);color:var(--ink)}
button:hover{border-color:var(--accent)}button.primary{background:var(--accent);border-color:var(--accent);color:var(--on-accent)}button.danger{color:var(--bad)}
.actions{display:flex;gap:10px;flex-wrap:wrap;margin-top:20px}
table{border-collapse:collapse;width:100%;font-size:14px}th,td{text-align:left;padding:8px 10px;border-bottom:1px solid var(--line)}
th{font:11px ui-monospace,Menlo,monospace;letter-spacing:.07em;text-transform:uppercase;color:var(--label);background:var(--tile);font-weight:400}
td.num{font-variant-numeric:tabular-nums}.ok{color:var(--good);font-weight:600}.no{color:var(--bad);font-weight:600}
code{font:12.5px ui-monospace,Menlo,monospace;background:var(--tile);padding:1px 5px;border-radius:5px}
details summary{cursor:pointer;color:var(--accent-ink);margin-top:14px}pre{font:12px ui-monospace,Menlo,monospace;white-space:pre-wrap;overflow-wrap:anywhere;color:var(--muted)}
#message{position:fixed;right:22px;bottom:22px;background:var(--ink);color:var(--bg);padding:11px 16px;border-radius:9px;display:none;max-width:420px;z-index:5;box-shadow:0 6px 24px rgba(0,0,0,.18)}
.hidden{display:none}
.foot{display:flex;justify-content:space-between;align-items:center;gap:12px;flex-wrap:wrap;margin:28px 0 0;padding:14px 18px;border:1px solid var(--line);border-radius:9px;background:var(--tile);color:var(--muted);font-size:13.5px}
.foot a{color:var(--accent-ink);font-weight:600;text-decoration:none;display:inline-flex;align-items:center;gap:7px}.foot a:hover{text-decoration:underline}
@media(max-width:980px){.tiles{grid-template-columns:repeat(2,minmax(0,1fr))}}
@media(max-width:760px){.app{display:block}aside{position:static;height:auto;border-right:0;border-bottom:1px solid var(--line);padding:14px 16px}
nav{display:flex;flex-wrap:wrap;gap:4px}nav h4{display:none}.crt-h,.crt{display:none}.logo{margin-bottom:10px}
main{padding:20px 16px 60px}h1{font-size:30px}.row{grid-template-columns:1fr}.tiles{gap:10px}.v{font-size:24px}.card{padding:16px}}
</style></head><body><div class="app">
<aside>
<div class="logo"><svg width="30" height="30" viewBox="0 0 30 30" aria-hidden="true"><g stroke="var(--accent)" stroke-width="2.6" stroke-linecap="round" fill="none"><circle cx="15" cy="15" r="5.5"/><path d="M15 2.5v3.5M15 24v3.5M2.5 15H6M24 15h3.5M6.2 6.2l2.4 2.4M21.4 21.4l2.4 2.4M6.2 23.8l2.4-2.4M21.4 8.6l2.4-2.4"/></g></svg><span>Solar<b>Monitor</b></span></div>
<nav aria-label="Sections">
<h4>Live</h4><a href="#dashboard">Dashboard</a>
<h4>Settings</h4><a href="#connection">Home Assistant</a><a href="#display">Display</a><a href="#location">Location &amp; weather</a><a href="#entities">Entities</a>
<h4>Device</h4><a href="#device">Device &amp; firmware</a><a href="#diagnostics">Diagnostics</a>
<h4>Help</h4><a href="/guide/" target="_blank" rel="noopener">Handbook &#8599;</a>
</nav>
<div class="crt-h">Device status</div>
<div class="crt"><div class="t"><span class="led" id="led"></span><span id="crtTitle">Connecting</span></div>
<div class="r"><span>host</span><span>solar.local</span></div><div class="r"><span>ip</span><span id="crtIp">–</span></div>
<div class="r"><span>fw</span><span id="crtFw">–</span></div><div class="r"><span>uptime</span><span id="crtUp">–</span></div>
<div class="r"><span>heap</span><span id="crtHeap">–</span></div><div class="r"><span>source</span><span id="crtSrc">–</span></div></div>
</aside>
<main>
<section class="page" id="p-dashboard">
<div class="head"><div><h1 id="title">Home energy</h1><p class="cap" id="caption">SolarEdge &middot; LG battery &middot; waiting for the device&hellip;</p></div><span class="pill" id="badge">Connecting</span></div>
<h2>At a glance</h2>
<div class="tiles">
<div class="tile" id="tSolar"><span class="k">Solar</span><div class="v" id="vSolar">&ndash;</div><span class="pill" id="sSolar">from DC + battery</span></div>
<div class="tile" id="tHouse"><span class="k">House</span><div class="v" id="vHouse">&ndash;</div><span class="pill" id="sHouse">demand</span></div>
<div class="tile" id="tGrid"><span class="k">Grid</span><div class="v" id="vGrid">&ndash;</div><span class="pill" id="sGrid">&ndash;</span></div>
<div class="tile" id="tBatt"><span class="k">Battery</span><div class="v" id="vBatt">&ndash;</div><span class="pill" id="sBatt">&ndash;</span></div>
<div class="tile" id="tSoc"><span class="k" id="kSoc">Battery level</span><div class="v" id="vSoc">&ndash;</div><div class="bar"><i id="socBar" style="width:0"></i></div></div>
<div class="tile" id="tToday"><span class="k">Solar today</span><div class="v" id="vToday">&ndash;</div><span class="pill" id="sToday">not configured</span></div>
<div class="tile" id="tTomorrow"><span class="k">Tomorrow</span><div class="v" id="vTomorrow">&ndash;</div><span class="pill" id="sTomorrow">&ndash;</span></div>
<div class="tile" id="tWeather"><span class="k" id="kWeather">Weather</span><div class="v" id="vWeather">&ndash;</div><span class="pill" id="sWeather">not configured</span></div>
</div>
<div class="card"><h2>Energy flow</h2>
<svg id="flow" viewBox="0 0 640 400" role="img" aria-label="Energy flow between solar, grid, battery and house">
<line class="track" x1="320" y1="98" x2="320" y2="172"/><line class="track" x1="168" y1="200" x2="292" y2="200"/>
<line class="track" x1="348" y1="200" x2="472" y2="200"/><line class="track" x1="320" y1="228" x2="320" y2="302"/>
<g id="flows"></g>
<circle cx="320" cy="200" r="26" fill="var(--card)" stroke="var(--line)" stroke-width="2"/>
<path d="M305 200q3.75-9 7.5 0t7.5 0t7.5 0t7.5 0" fill="none" stroke="var(--label)" stroke-width="2" stroke-linecap="round"><title>Inverter</title></path>
<g class="node" id="nSolar" transform="translate(240,12)"><rect class="box" width="160" height="86" rx="9"/>
<g transform="translate(20,30)" stroke="var(--solar)" stroke-width="2.4" stroke-linecap="round" fill="none"><circle r="7"/><path d="M0-14v3M0 11v3M-14 0h3M11 0h3M-10-10l2 2M8 8l2 2M-10 10l2-2M8-8l2-2"/></g>
<text class="nk" x="44" y="26">SOLAR</text><text class="nv" x="44" y="54" id="nvSolar">&ndash;</text><text class="ns" x="44" y="74" id="nsSolar"></text></g>
<g class="node" id="nGrid" transform="translate(8,157)"><rect class="box" width="160" height="86" rx="9"/>
<g transform="translate(20,30)" stroke="var(--grid)" stroke-width="2.2" stroke-linecap="round" fill="none"><path d="M-7 15L0-14L7 15M-9-7h18M-6 3h12M-4-7L4 3M4-7L-4 3"/></g>
<text class="nk" x="44" y="26">GRID</text><text class="nv" x="44" y="54" id="nvGrid">&ndash;</text><text class="ns" x="44" y="74" id="nsGrid"></text></g>
<g class="node" id="nBatt" transform="translate(472,157)"><rect class="box" width="160" height="86" rx="9"/>
<g transform="translate(10,20)"><rect width="20" height="30" rx="3" fill="none" stroke="var(--battery)" stroke-width="2.2"/><rect x="6" y="-4" width="8" height="4" rx="1" fill="var(--battery)"/><rect id="battFill" x="3" y="27" width="14" height="0" rx="1" fill="var(--battery)"/></g>
<text class="nk" x="44" y="26">BATTERY</text><text class="nv" x="44" y="54" id="nvBatt">&ndash;</text><text class="ns" x="44" y="74" id="nsBatt"></text></g>
<g class="node" id="nHouse" transform="translate(240,302)"><rect class="box" width="160" height="86" rx="9"/>
<g transform="translate(20,30)" stroke="var(--house)" stroke-width="2.2" stroke-linejoin="round" fill="none"><path d="M-12 0L0-11L12 0M-8-3v15h16V-3M-3 12V5h6v7"/></g>
<text class="nk" x="44" y="26">HOUSE</text><text class="nv" x="44" y="54" id="nvHouse">&ndash;</text><text class="ns" x="44" y="74" id="nsHouse"></text></g>
</svg>
<p class="note">Comets travel the way power is flowing. A thicker arm and faster comets mean more power; a plain grey arm is idle, missing or stale.</p></div>
<div class="card"><h2>Last 30 minutes</h2>
<svg id="trend" viewBox="0 0 640 250" role="img" aria-label="Power over the last 30 minutes"></svg>
<div class="legend"><span><i style="background:var(--solar)"></i>Solar</span><span><i style="background:var(--house)"></i>House</span><span><i style="background:var(--grid)"></i>Grid (+ import / &minus; export)</span><span><i style="background:var(--battery)"></i>Battery (+ supplying / &minus; charging)</span></div>
<p class="note" id="trendNote">Collected by this browser while the page is open; the device keeps no history.</p></div>
<p class="cap" id="sourceStatus">Waiting for device status&hellip;</p>
<div class="foot"><span>CYD Home Solar Monitor &middot; SolarEdge + Home Assistant &middot; <a href="/guide/" target="_blank" rel="noopener">Handbook</a></span>
<a href="https://github.com/anthonyjclarke/CYD-Home_Solar_Monitor" target="_blank" rel="noopener"><svg width="16" height="16" viewBox="0 0 16 16" aria-hidden="true" fill="currentColor"><path d="M8 0C3.58 0 0 3.58 0 8c0 3.54 2.29 6.53 5.47 7.59.4.07.55-.17.55-.38v-1.33c-2.23.48-2.7-1.07-2.7-1.07-.36-.92-.89-1.17-.89-1.17-.73-.5.06-.49.06-.49.8.06 1.23.83 1.23.83.72 1.22 1.87.87 2.33.66.07-.52.28-.87.5-1.07-1.78-.2-3.65-.89-3.65-3.95 0-.87.31-1.59.82-2.15-.08-.2-.36-1.02.08-2.12 0 0 .67-.21 2.2.82A7.6 7.6 0 0 1 8 3.9c.68 0 1.36.09 2 .27 1.53-1.04 2.2-.82 2.2-.82.44 1.1.16 1.92.08 2.12.51.56.82 1.28.82 2.15 0 3.07-1.87 3.75-3.66 3.95.29.25.54.73.54 1.48v2.2c0 .21.15.46.55.38A8 8 0 0 0 16 8c0-4.42-3.58-8-8-8z"/></svg>github.com/anthonyjclarke/CYD-Home_Solar_Monitor</a></div>
</section>

<form id="settings">
<section class="page" id="p-connection"><h1>Home Assistant</h1><p class="cap">Where readings come from, and how often.</p>
<div class="card"><h2>Connection</h2>
<label for="url">Home Assistant base URL</label><input id="url" maxlength="180" required placeholder="http://192.168.1.254:8123">
<label for="token">Access token</label><input id="token" type="password" autocomplete="new-password" maxlength="2048" placeholder="Leave blank to keep the saved token"><span class="hint" id="tokenState">The saved token is never returned to this page.</span>
<label class="check"><input id="clearToken" type="checkbox">Clear saved Home Assistant token</label>
<div class="row"><div><label for="poll">Read interval (seconds)</label><input id="poll" type="number" min="2" max="300" required></div><div><label for="stale">Stale after (seconds)</label><input id="stale" type="number" min="15" max="3600" required><span class="hint">At least 3&times; the read interval.</span></div></div>
<div class="actions"><button class="primary" type="submit">Save settings</button></div></div></section>

<section class="page" id="p-display"><h1>Display</h1><p class="cap">How the CYD panel looks and behaves.</p>
<div class="card"><h2>Panel</h2>
<div class="row"><div><label for="layout">Layout</label><select id="layout"><option value="1">Centre flow (light)</option><option value="0">Cards (dark)</option></select></div>
<div><label for="battery">Battery label (up to 14 characters)</label><input id="battery" maxlength="14" placeholder="LG 9.8 kWh"></div></div>
<div class="row"><div><label for="brightness">Brightness (10&ndash;255)</label><input id="brightness" type="number" min="10" max="255" required></div>
<div><label class="check" style="margin-top:40px"><input id="demo" type="checkbox">Demo mode &ndash; synthetic data</label></div></div></div>
<div class="card"><h2>Night</h2>
<label class="check"><input id="nightDim" type="checkbox">Dim overnight</label>
<div class="row"><div><label for="nightStart">Night starts (hour 0&ndash;23)</label><input id="nightStart" type="number" min="0" max="23" required></div><div><label for="nightEnd">Night ends (hour 0&ndash;23)</label><input id="nightEnd" type="number" min="0" max="23" required></div></div>
<span class="hint">Uses the configured timezone once the clock is set. After a minute without touch the panel dims to a fifth; a tap restores it. Equal hours turn this off.</span>
<div class="actions"><button class="primary" type="submit">Save settings</button></div></div></section>

<section class="page" id="p-location"><h1>Location &amp; weather</h1><p class="cap">Sets the clock's timezone and the weather shown on the panel.</p>
<div class="card"><h2>Find your location</h2>
<div class="row"><input id="placeSearch" placeholder="Town or suburb, e.g. Parramatta" aria-label="Town or suburb"><div style="display:flex;gap:10px"><button type="button" id="findPlace">Search</button><button type="button" id="clearPlace">Clear location</button></div></div>
<select id="placeResults" class="hidden" aria-label="Search results" style="margin-top:12px"></select>
<span class="hint">Search runs from this browser using Open-Meteo geocoding. The device fetches weather every 30 minutes over verified HTTPS, sending only the coordinates.</span>
<div class="row"><div><label for="lat">Latitude</label><input id="lat" type="number" step="0.0001" min="-90" max="90"></div><div><label for="lon">Longitude</label><input id="lon" type="number" step="0.0001" min="-180" max="180"></div></div>
<label for="place">Place name</label><input id="place" maxlength="48"></div>
<div class="card"><h2>Timezone</h2>
<div class="row"><div><label for="tzName">Timezone</label><input id="tzName" maxlength="40" placeholder="Australia/Sydney"></div><div><label for="tz">POSIX TZ rule</label><input id="tz" maxlength="60" required placeholder="AEST-10AEDT,M10.1.0,M4.1.0/3"></div></div>
<span class="hint" id="tzHint">The device uses the POSIX rule; choosing a known timezone fills it in. Leave latitude and longitude blank to turn weather off.</span>
<div class="actions"><button class="primary" type="submit">Save settings</button></div></div></section>

<section class="page" id="p-entities"><h1>Entities</h1><p class="cap">Home Assistant sensors the device reads. Defaults match the SolarEdge Modbus Multi integration.</p>
<div class="card"><h2>Live power</h2><div id="entities"></div></div>
<div class="card"><h2>Daily totals (optional)</h2>
<label for="today">Solar energy today (kWh)</label><input id="today" maxlength="100" placeholder="sensor.solar_energy_today"><span class="hint">A daily Utility Meter on solar energy built from DC + battery power, not inverter AC energy. See the handbook.</span>
<label for="forecast">Solar forecast tomorrow (kWh)</label><input id="forecast" maxlength="100" placeholder="sensor.energy_production_tomorrow"><span class="hint">From Forecast.Solar or Solcast. Leave blank to show tomorrow's temperature instead.</span></div>
<div class="card"><h2>HTTPS certificate</h2>
<label for="ca">Trusted CA certificate (PEM, only for an https:// URL)</label><textarea id="ca" maxlength="6000" placeholder="-----BEGIN CERTIFICATE-----"></textarea>
<div class="actions"><button class="primary" type="submit">Save settings</button></div></div></section>
</form>

<section class="page" id="p-device"><h1>Device &amp; firmware</h1><p class="cap">This CYD, at <code>http://solar.local/</code>.</p>
<div class="card"><h2>Device</h2><table><tbody id="deviceRows"></tbody></table>
<div class="actions"><button type="button" id="reboot">Restart device</button><button type="button" class="danger" id="wifiReset">Reset Wi-Fi</button></div></div>
<div class="card"><h2>Firmware update</h2><p class="cap" style="margin:0 0 12px">Upload the PlatformIO <code>firmware.bin</code> app image; settings are kept. Keep power connected until the device restarts. The merged image is for a clean USB install only.</p>
<form id="updateForm"><input type="file" id="firmware" accept=".bin" required><div class="actions"><button class="primary" type="submit">Upload firmware</button></div></form><p class="cap" id="uploadStatus" style="margin-top:12px"></p></div>
</section>

<section class="page" id="p-diagnostics"><h1>Diagnostics</h1><p class="cap">Every reading as the device judged it on its last cycle.</p>
<div class="card"><h2>Readings</h2><div style="overflow-x:auto"><table><thead><tr><th>Reading</th><th>Entity</th><th>Value</th><th>Valid</th><th>Fresh</th><th>Received</th><th>Reported</th></tr></thead><tbody id="diagRows"></tbody></table></div>
<details><summary>Raw status JSON</summary><pre id="diagnostics"></pre></details></div>
</section>
</main></div>
<div id="message" role="status" aria-live="polite"></div>
<script>
'use strict';
const $=id=>document.getElementById(id);let busy=false,last=0;
const labels=['Inverter AC power (W/kW)','Household consumption (W/kW)','Raw grid power (+ export, - import)','Battery DC power (+ charge, - discharge)','Battery state of energy (%)','Inverter DC power (W/kW)'];
labels.forEach((name,i)=>{const label=document.createElement('label');label.htmlFor='entity'+i;label.textContent=name;const input=document.createElement('input');input.id='entity'+i;input.required=true;input.maxLength=100;$('entities').append(label,input)});
// Olson name to POSIX TZ rule. The device has no tz database; anything not
// listed can be typed into the POSIX field directly.
const POSIX={'Australia/Sydney':'AEST-10AEDT,M10.1.0,M4.1.0/3','Australia/Melbourne':'AEST-10AEDT,M10.1.0,M4.1.0/3','Australia/Canberra':'AEST-10AEDT,M10.1.0,M4.1.0/3','Australia/Hobart':'AEST-10AEDT,M10.1.0,M4.1.0/3','Australia/Brisbane':'AEST-10','Australia/Lindeman':'AEST-10','Australia/Adelaide':'ACST-9:30ACDT,M10.1.0,M4.1.0/3','Australia/Broken_Hill':'ACST-9:30ACDT,M10.1.0,M4.1.0/3','Australia/Darwin':'ACST-9:30','Australia/Perth':'AWST-8','Australia/Lord_Howe':'<+1030>-10:30<+11>-11,M10.1.0,M4.1.0','Pacific/Auckland':'NZST-12NZDT,M9.5.0,M4.1.0/3','Asia/Singapore':'<+08>-8','Asia/Tokyo':'JST-9','Asia/Hong_Kong':'HKT-8','Asia/Shanghai':'CST-8','Asia/Kolkata':'IST-5:30','Asia/Dubai':'<+04>-4','Europe/London':'GMT0BST,M3.5.0/1,M10.5.0','Europe/Dublin':'IST-1GMT0,M10.5.0,M3.5.0/1','Europe/Paris':'CET-1CEST,M3.5.0,M10.5.0/3','Europe/Berlin':'CET-1CEST,M3.5.0,M10.5.0/3','Europe/Amsterdam':'CET-1CEST,M3.5.0,M10.5.0/3','Europe/Madrid':'CET-1CEST,M3.5.0,M10.5.0/3','Europe/Rome':'CET-1CEST,M3.5.0,M10.5.0/3','Europe/Athens':'EET-2EEST,M3.5.0/3,M10.5.0/4','Europe/Helsinki':'EET-2EEST,M3.5.0/3,M10.5.0/4','America/New_York':'EST5EDT,M3.2.0,M11.1.0','America/Toronto':'EST5EDT,M3.2.0,M11.1.0','America/Chicago':'CST6CDT,M3.2.0,M11.1.0','America/Denver':'MST7MDT,M3.2.0,M11.1.0','America/Phoenix':'MST7','America/Los_Angeles':'PST8PDT,M3.2.0,M11.1.0','America/Vancouver':'PST8PDT,M3.2.0,M11.1.0','America/Anchorage':'AKST9AKDT,M3.2.0,M11.1.0','Pacific/Honolulu':'HST10','America/Sao_Paulo':'<-03>3','Africa/Johannesburg':'SAST-2','UTC':'UTC0'};
const SKY=c=>c===0?'Clear':c<=2?'Partly cloudy':c===3?'Cloudy':c<=48?'Fog':c<=57?'Drizzle':c<=67||(c>=80&&c<=82)?'Rain':c<=86?'Snow':c>=95?'Storm':'';
let places=[];
function setZone(name){$('tzName').value=name;if(POSIX[name]){$('tz').value=POSIX[name];$('tzHint').textContent='POSIX rule filled in for '+name+'.'}else $('tzHint').textContent=name+' is not in the built-in list; enter its POSIX TZ rule manually.'}
$('tzName').addEventListener('change',()=>setZone($('tzName').value.trim()));
$('findPlace').onclick=async()=>{const q=$('placeSearch').value.trim();if(!q)return;try{const r=await fetch('https://geocoding-api.open-meteo.com/v1/search?count=8&language=en&format=json&name='+encodeURIComponent(q));const d=await r.json();places=d.results||[];const sel=$('placeResults');sel.replaceChildren(new Option(places.length?'Choose a result...':'No places found',''));places.forEach((p,i)=>sel.add(new Option([p.name,p.admin1,p.country].filter(Boolean).join(', '),i)));sel.classList.remove('hidden')}catch(e){message('Place search failed: '+e.message)}};
$('placeResults').onchange=()=>{const p=places[$('placeResults').value];if(!p)return;$('lat').value=p.latitude.toFixed(4);$('lon').value=p.longitude.toFixed(4);$('place').value=[p.name,p.admin1].filter(Boolean).join(', ');if(p.timezone)setZone(p.timezone)};
$('clearPlace').onclick=()=>{['lat','lon','place'].forEach(k=>$(k).value='');$('placeResults').classList.add('hidden')};
// Sections are switched by URL hash so the browser back button works.
function route(){const id=(location.hash||'#dashboard').slice(1);const page=$('p-'+id)?id:'dashboard';document.querySelectorAll('.page').forEach(p=>p.classList.toggle('on',p.id==='p-'+page));document.querySelectorAll('nav a').forEach(a=>a.classList.toggle('on',a.hash==='#'+page))}
window.addEventListener('hashchange',route);route();
function fmt(w){w=Math.round(Math.abs(w));return w<1000?[w,'W']:[(w/1000).toFixed(2),'kW']}
function power(w){const f=fmt(w);return f[0]+' '+f[1]}
function setV(id,r){const f=fmt(r.value);$(id).innerHTML=r.hasValue?f[0]+'<small>'+f[1]+'</small>':'&ndash;'}
function pill(id,text,kind){const e=$(id);e.textContent=text;e.className='pill'+(kind?' '+kind:'')}
let toastTimer;function message(text){const m=$('message');m.textContent=text;m.style.display='block';clearTimeout(toastTimer);toastTimer=setTimeout(()=>m.style.display='none',5000)}
async function api(path,body){const response=await fetch(path,{method:body===undefined?'GET':'POST',headers:body===undefined?{}:{'Content-Type':'application/json','X-CYD-Request':'1'},body:body===undefined?undefined:JSON.stringify(body),cache:'no-store'});const data=await response.json();if(!response.ok)throw Error(data.error||'Request failed');return data}
async function load(){const c=await api('/api/config');['url','poll','stale','brightness','nightStart','nightEnd','ca','battery','today','forecast','place','tzName','tz'].forEach(k=>$(k).value=c[k]);$('lat').value=c.lat??'';$('lon').value=c.lon??'';['demo','nightDim'].forEach(k=>$(k).checked=c[k]);$('layout').value=String(c.layout);c.entities.forEach((id,i)=>$('entity'+i).value=id);$('tokenState').textContent=c.tokenSaved?'A token is saved. Leave blank to keep it.':'No Home Assistant token saved yet.'}
// Flow diagram: comets travel from source to destination, the way power
// flows. Each arm is [outer end, hub end];
// grid +import and battery +supplying flow into the hub (320,200).
const ARMS={Solar:[[320,100],[320,170]],Grid:[[170,200],[290,200]],Batt:[[470,200],[350,200]],House:[[320,300],[320,230]]};
// Comets: a pale core, coloured head and fading tail, eased along each arm.
// The faint band under them widens with power (capped at 5 kW).
const FL={},NS='http://www.w3.org/2000/svg',COL={Solar:'--solar',Grid:'--grid',Batt:'--battery',House:'--house'};
const RM=matchMedia('(prefers-reduced-motion: reduce)').matches;
function svgEl(t,a,p){const e=document.createElementNS(NS,t);for(const k in a)e.setAttribute(k,a[k]);p.appendChild(e);return e}
Object.keys(ARMS).forEach(id=>{const g=svgEl('g',{style:'display:none'},$('flows')),[a,b]=ARMS[id],d='M'+a[0]+' '+a[1]+'L'+b[0]+' '+b[1],c='var('+COL[id]+')';
const band=svgEl('path',{class:'band',d,style:'stroke:'+c},g);svgEl('path',{class:'spine',d,style:'stroke:'+c},g);const comets=[];
for(let i=0;i<3;i++){const k=svgEl('g',{class:'comet'},g);[[0,1,1],[-.9,.8,.7],[-1.9,.64,.45],[-3,.5,.28],[-4.2,.38,.15]].forEach(([x,r,o])=>svgEl('circle',{cx:x,r,style:'fill:'+c,opacity:o},k));svgEl('circle',{class:'hot',r:.42},k);comets.push(k)}
FL[id]={g,band,comets,track:document.querySelectorAll('#flow .track')[Object.keys(ARMS).indexOf(id)],on:false,kw:0,into:true,ph:Math.random()}});
function arm(id,live,watts,intoHub){const f=FL[id],kw=Math.abs(watts)/1000;f.on=live&&Math.abs(watts)>30;f.kw=kw;f.into=intoHub;
f.g.style.display=f.on?'':'none';f.track.style.opacity=f.on?.35:1;f.band.setAttribute('stroke-width',(3+Math.min(kw,5)*2.2).toFixed(1));if(RM)frame(0)}
const ease=p=>p<.5?2*p*p:1-2*(1-p)*(1-p);
let lastFrame=0;function frame(now){const dt=Math.min(.1,(now-lastFrame)/1000||0);lastFrame=now;
for(const id in FL){const f=FL[id];if(!f.on)continue;const [a,b]=ARMS[id],s=f.into?a:b,e=f.into?b:a,ang=Math.atan2(e[1]-s[1],e[0]-s[0])*57.3;
const n=f.kw>2.5?3:2,hr=3.4+Math.min(f.kw,5)*.9;f.ph=(f.ph+dt/Math.max(.8,2.8-f.kw*.42))%1;
f.comets.forEach((k,i)=>{if(i>=n){k.style.opacity=0;return}const p=RM?.72:(f.ph+i/n)%1,q=ease(p);
k.setAttribute('transform','translate('+(s[0]+(e[0]-s[0])*q).toFixed(1)+' '+(s[1]+(e[1]-s[1])*q).toFixed(1)+') rotate('+ang+') scale('+hr.toFixed(2)+')');
k.style.opacity=RM?1:Math.min(1,p/.18,(1-p)/.18).toFixed(2)})}
if(!RM)requestAnimationFrame(frame)}
requestAnimationFrame(frame);
function node(id,r,sub){$('n'+id).classList.toggle('stale',!r.fresh);$('nv'+id).textContent=r.hasValue?power(r.value):'–';$('ns'+id).textContent=sub}
const hist=[],HMAX=600;
// Fixed 30-minute window ending now, with a kW Y axis on "nice" steps and an
// X axis ticked every 5 minutes in local time. The viewBox follows the element's
// real width so axis text is never stretched.
function niceStep(range){const raw=range/4,p=Math.pow(10,Math.floor(Math.log10(raw)));for(const m of [1,2,2.5,5,10])if(m*p>=raw)return m*p;return 10*p}
function trend(){const svg=$('trend'),W=Math.max(320,Math.round(svg.getBoundingClientRect().width)||640),H=250;svg.setAttribute('viewBox','0 0 '+W+' '+H);
const L=50,R=14,T=24,B=30,pw=W-L-R,ph=H-T-B,now=Date.now(),t0=now-1800000;
const keys=[['s','--solar'],['h','--house'],['g','--grid'],['b','--battery']];let lo=0,hi=500;
hist.forEach(p=>keys.forEach(([k])=>{if(p[k]!=null){lo=Math.min(lo,p[k]);hi=Math.max(hi,p[k])}}));
const step=niceStep(hi-lo);lo=Math.floor(lo/step)*step;hi=Math.ceil(hi/step)*step;if(hi===lo)hi=lo+step;
const X=t=>L+(t-t0)/1800000*pw,Y=v=>T+(hi-v)/(hi-lo)*ph;
let out='<text x="'+(L-8)+'" y="'+(T-10)+'" text-anchor="end" font-size="11" fill="var(--label)" font-family="ui-monospace,Menlo,monospace">kW</text>';
for(let v=lo;v<=hi+1e-6;v+=step){const y=Y(v).toFixed(1);
out+='<line x1="'+L+'" x2="'+(L+pw)+'" y1="'+y+'" y2="'+y+'" stroke="var(--line)" stroke-width="1"'+(v===0?'':' stroke-dasharray="2 4"')+'/>';
out+='<text x="'+(L-8)+'" y="'+(+y+4)+'" text-anchor="end" font-size="11" fill="var(--label)">'+(v/1000).toFixed(step%1000?1:0)+'</text>'}
for(let m=30;m>=0;m-=5){const t=now-m*60000,x=X(t).toFixed(1),d=new Date(t);
out+='<line x1="'+x+'" x2="'+x+'" y1="'+(T+ph)+'" y2="'+(T+ph+5)+'" stroke="var(--label)"/>';
out+='<text x="'+x+'" y="'+(T+ph+19)+'" text-anchor="'+(m===0?'end':m===30?'start':'middle')+'" font-size="11" fill="var(--label)">'+(m===0?'now':String(d.getHours()).padStart(2,'0')+':'+String(d.getMinutes()).padStart(2,'0'))+'</text>'}
out+='<line x1="'+L+'" x2="'+L+'" y1="'+T+'" y2="'+(T+ph)+'" stroke="var(--label)"/><line x1="'+L+'" x2="'+(L+pw)+'" y1="'+(T+ph)+'" y2="'+(T+ph)+'" stroke="var(--label)"/>';
if(lo<0)out+='<line x1="'+L+'" x2="'+(L+pw)+'" y1="'+Y(0).toFixed(1)+'" y2="'+Y(0).toFixed(1)+'" stroke="var(--label)" stroke-width="1"/>';
keys.forEach(([k,c])=>{let d='',pen=false;hist.forEach(p=>{if(p[k]==null||p.t<t0){pen=false;return}d+=(pen?'L':'M')+X(p.t).toFixed(1)+' '+Y(p[k]).toFixed(1);pen=true});
if(d)out+='<path d="'+d+'" fill="none" stroke="var('+c+')" stroke-width="2.2" stroke-linejoin="round" stroke-linecap="round"/>'});
if(hist.length<2)out+='<text x="'+(L+pw/2)+'" y="'+(T+ph/2)+'" text-anchor="middle" font-size="13" fill="var(--muted)">Collecting readings&hellip;</text>';
svg.innerHTML=out;const span=hist.length?Math.round((now-hist[0].t)/60000):0;
$('trendNote').textContent='Collected by this browser while the page is open ('+span+' min so far); the device keeps no history. Above zero: grid importing, battery supplying.'}
window.addEventListener('resize',trend);
function age(s){return s==null?'–':s<90?s+' s':Math.round(s/60)+' min'}
function rows(id,pairs){$(id).innerHTML=pairs.map(([k,v])=>'<tr><th style="width:34%">'+k+'</th><td>'+v+'</td></tr>').join('')}
function esc(t){return String(t).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]))}
async function refresh(){if(busy)return;try{const s=await api('/api/status');last=Date.now();
const R=s.readings,solar=R[6],house=R[1],grid=R[2],batt=R[3],soc=R[4],live=s.complete;
pill('badge',s.demo?'Demo data':live?'Live':'Waiting',s.demo?'warn':live?'good':'warn');
const led=s.demo||!live?'#FFCF7A':'#84F3AD';$('led').style.background=led;$('led').style.boxShadow='0 0 7px '+led;
$('crtTitle').textContent=s.demo?'Demo mode':live?'Live':'Waiting';$('crtIp').textContent=s.ip;$('crtFw').textContent=s.version;$('crtUp').textContent=age(s.uptimeSeconds);$('crtHeap').textContent=Math.round(s.freeHeap/1024)+' KB';$('crtSrc').textContent=s.message;
const tm=(s.localTime||'').split(' ')[1];$('title').textContent='Home energy'+(tm?' — '+tm:'');
$('caption').textContent='SolarEdge · LG battery · '+(s.demo?'synthetic demo data':'live from Home Assistant')+(s.localTime?' · '+s.localTime:'');
setV('vSolar',solar);setV('vHouse',house);setV('vGrid',grid);setV('vBatt',batt);
[['tSolar',solar],['tHouse',house],['tGrid',grid],['tBatt',batt],['tSoc',soc]].forEach(([t,r])=>$(t).classList.toggle('stale',!r.fresh));
const gs=!grid.fresh?['No data','']:grid.value>30?['↓ Importing','bad']:grid.value<-30?['↑ Exporting','good']:['Idle',''];pill('sGrid',...gs);
const bs=!batt.fresh?['No data','']:batt.value<-30?['↑ Charging','good']:batt.value>30?['↓ Supplying house','']:['Idle',''];pill('sBatt',...bs);
pill('sSolar',solar.fresh?(solar.value>30?'Generating':'No generation'):'Unavailable',solar.fresh&&solar.value>30?'good':'');
pill('sHouse',house.fresh?'Demand':'Unavailable','');
$('kSoc').textContent=s.battery?'Battery · '+s.battery:'Battery level';$('vSoc').innerHTML=soc.hasValue?soc.value.toFixed(0)+'<small>%</small>':'&ndash;';
$('socBar').style.width=(soc.hasValue?Math.max(0,Math.min(100,soc.value)):0)+'%';$('socBar').style.background=soc.value>=50?'var(--battery)':soc.value>=20?'var(--solar)':'var(--bad)';
const t=s.totals||{},w=s.weather||{};
if(t.today&&t.today.entity){$('vToday').innerHTML=t.today.fresh?t.today.value.toFixed(2)+'<small>kWh</small>':'&ndash;';pill('sToday',t.today.fresh?'since midnight':'unavailable','')}
if(t.tomorrow&&t.tomorrow.entity&&t.tomorrow.fresh){$('vTomorrow').innerHTML='~'+Math.round(t.tomorrow.value)+'<small>kWh</small>';pill('sTomorrow',w.fresh?SKY(w.tomorrowCode)+' · '+w.tomorrowMaxC.toFixed(0)+'°':'solar forecast','')}
else if(w.fresh){$('vTomorrow').innerHTML=w.tomorrowMaxC.toFixed(0)+'<small>°C max</small>';pill('sTomorrow',SKY(w.tomorrowCode),'')}
if(w.configured){$('kWeather').textContent=w.place?'Now · '+w.place.split(',')[0]:'Weather now';$('vWeather').innerHTML=w.fresh?w.nowC.toFixed(0)+'<small>°C</small>':'&ndash;';pill('sWeather',w.fresh?SKY(w.nowCode):'unavailable','')}
arm('Solar',solar.fresh,solar.value,true);arm('Grid',grid.fresh,grid.value,grid.value>0);arm('Batt',batt.fresh,batt.value,batt.value>0);arm('House',house.fresh,house.value,false);
node('Solar',solar,solar.fresh?(solar.value>30?'generating':'idle'):'unavailable');node('House',house,house.fresh?'demand':'unavailable');
node('Grid',grid,!grid.fresh?'unavailable':grid.value>30?'importing':grid.value<-30?'exporting':'idle');
node('Batt',batt,(!batt.fresh?'unavailable':batt.value<-30?'charging':batt.value>30?'supplying':'idle')+(soc.hasValue?' · '+soc.value.toFixed(0)+'%':''));
const fh=soc.hasValue?Math.round(24*Math.max(0,Math.min(100,soc.value))/100):0;$('battFill').setAttribute('height',fh);$('battFill').setAttribute('y',27-fh);
const now=Date.now(),val=r=>r.fresh?r.value:null;hist.push({t:now,s:val(solar),h:val(house),g:val(grid),b:val(batt)});while(hist.length>HMAX||(hist.length&&now-hist[0].t>1800000))hist.shift();trend();
$('sourceStatus').textContent=s.message+' · '+(s.reportTimeKnown?'source reporting time verified':'source age unknown');
rows('deviceRows',[['Firmware',esc(s.project+' '+s.version)],['Address','<code>http://solar.local/</code> · '+esc(s.ip)],['Local time',esc(s.localTime||'clock not set')],['Uptime',age(s.uptimeSeconds)],['Free memory',Math.round(s.freeHeap/1024)+' KB']]);
$('diagRows').innerHTML=R.map(r=>'<tr><td>'+esc(r.name)+'</td><td><code>'+esc(r.entity||'derived')+'</code></td><td class="num">'+(r.hasValue?(r.unit==='%'?r.value.toFixed(1)+' %':Math.round(r.value)+' W'):'–')+'</td><td class="'+(r.valid?'ok':'no')+'">'+(r.valid?'yes':'no')+'</td><td class="'+(r.fresh?'ok':'no')+'">'+(r.fresh?'yes':'no')+'</td><td class="num">'+age(r.receivedAgeSeconds)+'</td><td class="num">'+age(r.reportedAgeSeconds)+'</td></tr>').join('');
$('diagnostics').textContent=JSON.stringify(s,null,2)}
catch(e){pill('badge','Disconnected','bad');$('crtTitle').textContent='Disconnected';$('led').style.background='#E0644F';$('led').style.boxShadow='0 0 7px #E0644F';$('sourceStatus').textContent=e.message;document.querySelectorAll('.tile,.node').forEach(el=>el.classList.add('stale'))}}
$('settings').addEventListener('submit',async e=>{e.preventDefault();const c={};['url','ca','battery','today','forecast','place','tzName','tz'].forEach(k=>c[k]=$(k).value.trim());const lat=$('lat').value.trim(),lon=$('lon').value.trim();c.lat=lat===''?null:Number(lat);c.lon=lon===''?null:Number(lon);['poll','stale','brightness','nightStart','nightEnd','layout'].forEach(k=>c[k]=Number($(k).value));['demo','nightDim','clearToken'].forEach(k=>c[k]=$(k).checked);c.entities=labels.map((_,i)=>$('entity'+i).value.trim());c.token=$('token').value;try{await api('/api/config',c);$('token').value='';$('clearToken').checked=false;message('Settings saved');await load();await refresh()}catch(err){message(err.message)}});
// Settings span several sections; a field that fails browser validation may be
// on a hidden one, so show that section before the browser reports it.
document.querySelectorAll('#settings input,#settings select,#settings textarea').forEach(el=>el.addEventListener('invalid',()=>{const p=el.closest('.page');if(p&&!p.classList.contains('on'))location.hash=p.id.slice(2)}));
$('reboot').onclick=async()=>{if(!confirm('Restart this CYD?'))return;try{await api('/api/reboot',{});message('Restarting. Reload this page shortly.')}catch(e){message(e.message)}};
$('wifiReset').onclick=async()=>{if(!confirm('Clear saved Wi-Fi and restart? Home Assistant settings remain saved.'))return;try{await api('/api/wifi-reset',{});message('Wi-Fi cleared. Join the setup hotspot shown on the display.')}catch(e){message(e.message)}};
$('updateForm').onsubmit=async e=>{e.preventDefault();if(!confirm('Install this app firmware and restart the CYD?'))return;const file=$('firmware').files[0];if(!file)return;busy=true;$('uploadStatus').textContent='Uploading. Keep power connected...';const body=new FormData();body.append('firmware',file);try{const r=await fetch('/update',{method:'POST',headers:{'X-CYD-Request':'1'},body});const d=await r.json();if(!r.ok)throw Error(d.error||'Update failed');$('uploadStatus').textContent='Installed. Device is restarting. Reload shortly.'}catch(err){$('uploadStatus').textContent=err.message;busy=false}};
load().then(refresh).catch(e=>message(e.message));setInterval(refresh,3000);
</script></body></html>)HTML";
