// Exercise the actual embedded chart code without browser or third-party packages.
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const page=fs.readFileSync(process.argv[2]||'src/web/web_page.h','utf8');
// Parse the whole program as well as exercising the chart functions below.
new vm.Script(page.match(/<script>([\s\S]*?)<\/script>/)[1],{filename:'embedded-dashboard.js'});
const source=page.slice(page.indexOf('let historyData='),page.indexOf('function age(s)'));
const elements={};
for(const id of ['trend','trendNote','historyLegend','historyMetric'])elements[id]={innerHTML:'',textContent:'',style:{},setAttribute(){},getBoundingClientRect(){return {width:640}},value:'power',options:[{}],add(o){this.options.push(o)}};
const fields=Array.from({length:13},(_,i)=>({name:'Metric '+i,unit:i===4?'%':i<7?'W':i<9?'kWh':i===9||i===11?'C':'code'}));
const end=1793836800;
const sample=(i,gap=false)=>({slot:end-900+i*300,t:gap?null:end-900+i*300,sourceTimeKnown:0,v:fields.map((_,k)=>gap?null:k===2?-1000:k===3?2000:k===4?60:1000)});
const data={schema:1,end,start:end-86400,clockKnown:true,intervalSeconds:300,fields,samples:[sample(0),sample(1,true),sample(2),sample(3)],storageHealthy:true,demoExcluded:false};
let calls=0;
const context=vm.createContext({$:id=>elements[id],busy:false,Date,Math,Number,Array,Option:function(text,value){this.text=text;this.value=value},window:{addEventListener(){}},esc:s=>String(s).replace(/[<>&"]/g,'_'),api:async path=>{assert.equal(path,'/api/history?hours=24');calls++;return data}});
vm.runInContext(source,context);
(async()=>{
    await vm.runInContext('refreshHistory(true)',context);
    assert.equal(elements.historyMetric.options.length,9); // Forecast and weather fields are not offered.
    assert.equal(calls,1);
    await vm.runInContext('refreshHistory()',context);
    assert.equal(calls,1); // Do not fetch the entire history on every status refresh.
    assert.match(elements.trendNote.textContent,/5-minute snapshots/);
    assert.match(elements.trendNote.textContent,/unknown upstream age/);
    const paths=[...elements.trend.innerHTML.matchAll(/<path d="([^"]+)"/g)].map(m=>m[1]);
    assert.equal(paths.length,4);
    for(const path of paths){assert.equal((path.match(/M/g)||[]).length,2);assert.equal((path.match(/L/g)||[]).length,1)}
    assert.match(elements.trend.innerHTML,/-1<\/text>/); // Signed grid axis.
    elements.historyMetric.value='4';vm.runInContext('trend()',context);
    assert.equal(elements.historyLegend.style.display,'none');
    assert.match(elements.trend.innerHTML,/>%<\/text>/);
    assert.match(elements.trendNote.textContent,/60.00 \/ 60.00 \/ 60.00 %/);
    elements.historyMetric.value='7';vm.runInContext('trend()',context);
    assert.match(elements.trend.innerHTML,/>kWh<\/text>/);
    elements.historyMetric.value='9';vm.runInContext('trend()',context);
    assert.match(elements.trend.innerHTML,/>C<\/text>/);
    elements.historyMetric.value='10';vm.runInContext('trend()',context);
    assert.doesNotMatch(elements.trendNote.textContent,/Sampled min/);
    assert.doesNotMatch([...elements.trend.innerHTML.matchAll(/<path d="([^"]+)"/g)].map(m=>m[1]).join(''),/L/);
    vm.runInContext('historyData={...historyData,storageHealthy:false,demoExcluded:true};trend()',context);
    assert.match(elements.trendNote.textContent,/RAM history only/);
    assert.match(elements.trendNote.textContent,/Demo excluded/);
    vm.runInContext('historyData={...historyData,samples:[],clockKnown:false};trend()',context);
    assert.match(elements.trend.innerHTML,/Waiting for device clock sync/);
    context.api=async()=>{throw Error('offline')};
    await vm.runInContext('refreshHistory(true)',context);
    assert.match(elements.trendNote.textContent,/History unavailable: offline/);
    console.log('Web history: shared API, gap paths, signed axes, unit selection, refresh rate, clock/storage/demo/error states passed.');
})().catch(e=>{console.error(e);process.exitCode=1});
