// Exercise the actual dashboard refresh code (audit F2/F3) with a minimal fake
// DOM, deferred fetches and manually fired timers. No browser or packages.
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const page=fs.readFileSync('src/web/web_page.h','utf8');
const source=page.slice(page.indexOf('function fmt(w)'),page.indexOf("$('settings').addEventListener('submit'"));

function el(){const cls=new Set();return {innerHTML:'',textContent:'',value:'power',className:'',style:{},options:[{}],add(){},
    classList:{add:c=>cls.add(c),remove:c=>cls.delete(c),toggle:(c,on)=>on?cls.add(c):cls.delete(c),contains:c=>cls.has(c)},
    setAttribute(k,v){this[k]=v},appendChild(c){return c},getBoundingClientRect(){return {width:640}}}}
const elements={};const $=id=>elements[id]||(elements[id]=el());
const tracks=[el(),el(),el(),el()];
const staleable=()=>['tSolar','tHouse','tGrid','tBatt','tSoc','tToday','tTomorrow','tWeather','nSolar','nGrid','nBatt','nHouse'].map($);
const document={createElementNS:()=>el(),querySelectorAll:s=>s==='#flow .track'?tracks:s==='.tile,.node'?staleable():[]};

// Timers fire only when the test says so.
let timers=[],timerId=0;
const setTimeout=(fn,ms)=>{const id=++timerId;timers.push({id,fn,ms});return id};
const clearTimeout=id=>{timers=timers.filter(t=>t.id!==id)};
const fireDeadlines=()=>{const due=timers.filter(t=>t.ms>=5000);timers=timers.filter(t=>t.ms<5000);due.forEach(t=>t.fn())};

// Each status fetch is a deferred promise the test resolves, rejects or abandons.
const pending=[];
function fetch(path,opts){
    if(path!=='/api/status')return Promise.resolve({ok:true,json:async()=>({schema:0})});
    return new Promise((resolve,reject)=>{const req={resolve:body=>resolve({ok:true,json:async()=>body}),
        fail:(code,error)=>resolve({ok:false,status:code,json:async()=>({error})})};
        opts.signal?.addEventListener('abort',()=>reject(Object.assign(Error('aborted'),{name:'AbortError'})));pending.push(req)})}

const context=vm.createContext({$,document,fetch,AbortController,setTimeout,clearTimeout,Date,Math,Number,String,Array,Object,JSON,Error,
    busy:false,last:0,SKY:c=>'sky'+c,window:{addEventListener(){}},matchMedia:()=>({matches:true}),requestAnimationFrame(){},Option:function(){}});
vm.runInContext(source,context);
const flowOn=id=>vm.runInContext(`FL.${id}.on`,context);
const settle=()=>new Promise(r=>setImmediate(r));

function reading(name,value,fresh=true){return {name,value,fresh,valid:fresh,hasValue:true,unit:'W',entity:'sensor.x'}}
function status({today,tomorrow,weather,solar=2500}={}){
    const R=[reading('AC',2000),reading('Home',800),reading('Grid',-900),reading('Batt',-800),{...reading('Soc',60),unit:'%'},reading('DC',2600),reading('Solar',solar)];
    return {readings:R,complete:true,demo:false,ip:'192.0.2.1',version:'test',uptimeSeconds:1,freeHeap:90000,message:'ok',localTime:'2026-11-08 10:00 AEDT',
        battery:'Test',project:'p',reportTimeKnown:true,totals:{today:today||{entity:'',fresh:false},tomorrow:tomorrow||{entity:'',fresh:false}},
        weather:weather||{configured:false,fresh:false}};
}
const full=status({today:{entity:'sensor.today',value:12.34,fresh:true},tomorrow:{entity:'sensor.fc',value:21.4,fresh:true},
    weather:{configured:true,fresh:true,place:'Putney, NSW',nowC:25.2,nowCode:1,tomorrowMaxC:19.6,tomorrowCode:61}});
const text=id=>$(id).innerHTML+' | '+$(id).textContent;

async function poll(reply){const p=vm.runInContext('refresh()',context);await settle();const req=pending.shift();reply(req);await p;await settle()}

(async()=>{
    // F2: fully configured, then everything cleared.
    await poll(r=>r.resolve(full));
    assert.match($('vToday').innerHTML,/12\.34/);assert.match($('vTomorrow').innerHTML,/~21/);assert.match($('vWeather').innerHTML,/25/);
    assert.equal($('kWeather').textContent,'Now · Putney');
    assert.ok(flowOn('Solar'));
    await poll(r=>r.resolve(status()));
    for(const id of ['vToday','vTomorrow','vWeather'])assert.equal($(id).innerHTML,'&ndash;',id);
    for(const id of ['sToday','sTomorrow','sWeather'])assert.equal($(id).textContent,'not configured',id);
    assert.equal($('kWeather').textContent,'Weather');

    // F2: configured but invalid, then forecast lost with weather as fallback, then both lost.
    await poll(r=>r.resolve(status({today:{entity:'sensor.today',value:9,fresh:false},tomorrow:{entity:'sensor.fc',value:21,fresh:false},
        weather:{configured:true,fresh:true,place:'',nowC:20,nowCode:3,tomorrowMaxC:18.4,tomorrowCode:3}})));
    assert.equal($('vToday').innerHTML,'&ndash;');assert.equal($('sToday').textContent,'unavailable');
    assert.match($('vTomorrow').innerHTML,/18<small>°C max/);assert.equal($('kWeather').textContent,'Weather now');
    await poll(r=>r.resolve(status({tomorrow:{entity:'sensor.fc',value:21,fresh:false},weather:{configured:true,fresh:false}})));
    assert.equal($('vTomorrow').innerHTML,'&ndash;');assert.equal($('sTomorrow').textContent,'unavailable');
    assert.equal($('vWeather').innerHTML,'&ndash;');assert.equal($('sWeather').textContent,'unavailable');
    await poll(r=>r.resolve(full));
    assert.match($('vTomorrow').innerHTML,/~21/);assert.ok(!$('tTomorrow').classList.contains('stale'));

    // F3: explicit rejection stops every arm and greys tiles; recovery restores them.
    await poll(r=>r.fail(503,'Busy'));
    assert.equal($('badge').textContent,'Disconnected');assert.match($('sourceStatus').textContent,/^Busy/);
    for(const id of ['Solar','Grid','Batt','House'])assert.ok(!flowOn(id),id);
    assert.ok($('tSolar').classList.contains('stale'));assert.match($('vToday').innerHTML,/12\.34/); // Last value kept, greyed.
    await poll(r=>r.resolve(full));
    assert.ok(flowOn('Solar'));assert.equal($('badge').textContent,'Live');assert.ok(!$('tSolar').classList.contains('stale'));

    // F3: a hung request blocks overlapping timer ticks, then times out.
    const hung=vm.runInContext('refresh()',context);await settle();
    assert.equal(pending.length,1);
    vm.runInContext('refresh()',context);vm.runInContext('refresh()',context);await settle();
    assert.equal(pending.length,1); // Timer ticks did not start another poll.
    fireDeadlines();await hung;await settle();pending.length=0;
    assert.match($('sourceStatus').textContent,/No response from the device · last update/);
    assert.ok(!flowOn('Solar'));

    // F3: a late reply older than the one shown is dropped.
    const slow=vm.runInContext('refresh()',context);await settle();
    const forced=vm.runInContext('refresh(true)',context);await settle(); // e.g. after saving settings
    const [oldReq,newReq]=pending.splice(0);
    newReq.resolve(full);await forced;await settle();
    assert.equal($('badge').textContent,'Live');
    oldReq.fail(500,'old failure');await slow;await settle();
    assert.equal($('badge').textContent,'Live'); // Superseded failure ignored.
    assert.ok(flowOn('Solar'));
    // And the poll guard is released for the next tick.
    await poll(r=>r.resolve(status({solar:0})));
    assert.ok(!flowOn('Solar'));
    console.log('Web dashboard: optional tile transitions, rejection, deadline, overlap guard, superseded replies and recovery passed.');
})().catch(e=>{console.error(e);process.exit(1)});
