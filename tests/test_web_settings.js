// Exercise the actual settings load/submit code, including debug level zero.
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const page=fs.readFileSync('src/web/web_page.h','utf8');
const elements={};let submit,posted;
function $(id){if(!elements[id]){let value='';elements[id]={get value(){return value},set value(v){value=String(v)},checked:false,textContent:'',addEventListener(event,handler){assert.equal(event,'submit');submit=handler}}}return elements[id]}
let config={url:'http://fixture.invalid',poll:5,stale:180,brightness:180,nightStart:19,nightEnd:7,debugLevel:3,ca:'',battery:'Test',today:'',forecast:'',place:'',tzName:'UTC',tz:'UTC0',lat:null,lon:null,nightDim:true,demo:false,layout:1,entities:Array.from({length:6},(_,i)=>'sensor.test_'+i)};
const context=vm.createContext({$,labels:config.entities,api:async(path,body)=>{assert.equal(path,'/api/config');if(body){posted=body;config={...config,...body};return {saved:true}}return config},message(){},refresh:async()=>{},refreshHistory:async()=>{}});
vm.runInContext(page.slice(page.indexOf('async function load()'),page.indexOf('// Flow diagram:')),context);
vm.runInContext(page.slice(page.indexOf("$('settings').addEventListener('submit'"),page.indexOf('// Settings span several sections')),context);
(async()=>{
    for(const level of [3,0,4,2,1]){
        config.debugLevel=level;
        await vm.runInContext('load()',context);
        assert.equal($('debugLevel').value,String(level));
        $('debugLevel').value=String(level);$('token').value='';
        await submit({preventDefault(){}});
        assert.equal(posted.debugLevel,level);
        assert.equal(typeof posted.debugLevel,'number');
        assert.equal($('debugLevel').value,String(level));
        assert.equal(posted.entities.length,6);
    }
    for(const mode of [0,1,2]){
        Object.assign(config,{nightDim:true,nightMode:mode,nightDimAt:1155,nightOffAt:1420,nightOnAt:390});
        await vm.runInContext('load()',context);
        assert.equal($('nightDimAt').value,'19:15');assert.equal($('nightOffAt').value,'23:40');assert.equal($('nightOnAt').value,'06:30');
        assert.equal($('nightDimControl').hidden,mode===1);assert.equal($('nightOffControl').hidden,mode===0);
        assert.equal($('nightDimAt').disabled,mode===1);assert.equal($('nightOffAt').disabled,mode===0);
        await submit({preventDefault(){}});
        assert.equal(posted.nightMode,mode);assert.equal(posted.nightDimAt,1155);assert.equal(posted.nightOffAt,1420);assert.equal(posted.nightOnAt,390);
        assert.equal(posted.nightStart,mode===1?23:19);assert.equal(posted.nightEnd,6);
    }
    Object.assign(config,{nightMode:0,nightDimAt:0,nightOffAt:0,nightOnAt:60});
    await vm.runInContext('load()',context);assert.equal($('nightDimAt').value,'00:00');
    $('nightOffAt').value=''; // An unused hidden field must not block a save.
    await submit({preventDefault(){}});assert.equal(posted.nightOffAt,0);
    $('nightDim').checked=false;vm.runInContext('nightControls()',context);
    assert.equal($('nightOnAt').disabled,true);
    $('nightOnAt').value='';await submit({preventDefault(){}});
    assert.equal(posted.nightDim,false);assert.equal(posted.nightOnAt,60);
    // Legacy API responses still populate valid time controls.
    delete config.nightMode;delete config.nightDimAt;delete config.nightOffAt;delete config.nightOnAt;
    config.nightStart=21;config.nightEnd=8;
    await vm.runInContext('load()',context);
    assert.equal($('nightDimAt').value,'21:00');assert.equal($('nightOffAt').value,'23:00');assert.equal($('nightOnAt').value,'08:00');
    console.log('Web settings: debug levels, Off=0, night modes, time precision, disabled fields, midnight, legacy fallback and save/reload passed.');
})().catch(e=>{console.error(e);process.exitCode=1});
