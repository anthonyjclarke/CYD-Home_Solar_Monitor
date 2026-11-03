#include "monitor_web.h"
#include "monitor_config.h"
#include "ha_source.h"
#include "config.h"
#include "web_page.h"
#include <ArduinoJson.h>
#include <WebServer.h>
#include <Update.h>
#include <WiFi.h>
#include <ctime>
#include <algorithm>

namespace web {
namespace {
WebServer server(80);
bool started=false, uploading=false, uploadOK=false, uploadAuth=false;
uint32_t rebootAt=0;
String updateError;

bool auth() {
    if (server.authenticate("admin",monitor::accessCode().c_str())) return true;
    server.requestAuthentication(BASIC_AUTH,"CYD Home Energy","Use the device code shown on the panel");
    return false;
}
bool mutation() {
    if (!auth()) return false;
    if (server.header("X-CYD-Request") != "1") {
        server.send(403,"application/json","{\"error\":\"Missing device request header\"}"); return false;
    }
    return true;
}
void json(const JsonDocument& doc,int code=200) {
    String body;serializeJson(doc,body);
    server.sendHeader("Cache-Control","no-store");
    server.send(code,"application/json",body);
}
void error(const char* msg,int code=400) { JsonDocument d;d["error"]=msg;json(d,code); }

bool validEntity(const String& s) {
    if (!s.startsWith("sensor.") || s.length()<8 || s.length()>100) return false;
    for (size_t i=7;i<s.length();++i) if (!(isLowerCase(s[i]) || isDigit(s[i]) || s[i]=='_')) return false;
    return true;
}
bool validUrl(const String& s) {
    if (!(s.startsWith("http://") || s.startsWith("https://")) || s.length()>180) return false;
    int hostStart=s.indexOf("://")+3;
    if (s.length()<=static_cast<unsigned>(hostStart)) return false;
    for (char ch:s) if (ch<=32 || ch==127 || ch=='@' || ch=='?' || ch=='#') return false;
    return s[hostStart]!='/' && s[hostStart]!=':';
}

void getConfig() {
    if (!auth()) return;
    const auto& c=monitor::config();JsonDocument d;
    d["url"]=c.url; d["tokenSaved"]=!c.token.isEmpty(); d["ca"]=c.caCertificate;
    d["poll"]=c.pollSeconds; d["stale"]=c.staleSeconds; d["brightness"]=c.brightness;
    d["nightStart"]=c.nightStart; d["nightEnd"]=c.nightEnd; d["nightDim"]=c.nightDim; d["demo"]=c.demo; d["battery"]=c.batteryLabel;
    for (const auto& entity:c.entities) d["entities"].add(entity);
    json(d);
}
void saveConfig() {
    if (!mutation()) return;
    if (server.arg("plain").length()>12288) { error("Settings too large");return; }
    JsonDocument d;
    if (deserializeJson(d,server.arg("plain")) || !d.is<JsonObject>()) { error("Expected a JSON object");return; }
    monitor::Config c=monitor::config();
    if (!d["url"].is<String>() || !d["entities"].is<JsonArray>() || d["entities"].size()!=energy::SourceCount) { error("URL and six entity IDs required; reload the page after updating");return; }
    c.url=d["url"].as<String>();c.url.trim();while(c.url.endsWith("/"))c.url.remove(c.url.length()-1);
    if (!validUrl(c.url)) { error("Invalid Home Assistant base URL");return; }
    for(int i=0;i<energy::SourceCount;++i) {
        c.entities[i]=d["entities"][i].as<String>();c.entities[i].trim();
        if (!validEntity(c.entities[i])) { error("Expected sensor.entity_name IDs");return; }
    }
    if (!d["token"].isNull()) {
        if (!d["token"].is<String>()) { error("Invalid token field");return; }
        String token=d["token"].as<String>();
        if (token.length()>2048 || token.indexOf('\r')>=0 || token.indexOf('\n')>=0) { error("Invalid token");return; }
        if (!token.isEmpty()) c.token=token;
    }
    if (d["clearToken"] == true) c.token="";
    if (!d["ca"].isNull()) c.caCertificate=d["ca"].as<String>();
    if (c.caCertificate.length()>6000) { error("CA certificate too large");return; }
    if (c.url.startsWith("https://") && c.caCertificate.indexOf("-----BEGIN CERTIFICATE-----")<0) { error("HTTPS needs a PEM CA certificate");return; }
    const char* numbers[]={"poll","stale","brightness","nightStart","nightEnd"};
    for (const char* key:numbers) if (!d[key].is<int>()) { error("Expected integer display/timing settings");return; }
    int poll=d["poll"],stale=d["stale"],bright=d["brightness"],start=d["nightStart"],end=d["nightEnd"];
    if (poll<2 || poll>300 || stale<15 || stale>3600 || stale<poll*3 || bright<10 || bright>255 || start<0 || start>23 || end<0 || end>23) {
        error("Check ranges; stale time must be at least 3x the read interval");return;
    }
    if (!d["demo"].is<bool>() || !d["nightDim"].is<bool>()) { error("Expected boolean display settings");return; }
    c.pollSeconds=poll;c.staleSeconds=stale;c.brightness=bright;c.nightStart=start;c.nightEnd=end;
    c.demo=d["demo"].as<bool>();c.nightDim=d["nightDim"].as<bool>();
    if (!d["battery"].isNull()) {
        String label=d["battery"].as<String>();label.trim();
        if (!d["battery"].is<String>() || label.length()>14) { error("Battery label must be 14 characters or fewer");return; }
        for (char ch:label) if (ch<32 || ch>126) { error("Battery label must be plain printable text");return; }
        c.batteryLabel=label;
    }
    if (!monitor::saveConfig(c)) { error("Could not persist settings",500);return; }
    source::configure(c);JsonDocument result;result["saved"]=true;json(result);
}

void getStatus() {
    if (!auth()) return;
    const auto s=source::snapshot();const auto& c=monitor::config();
    const uint32_t now=millis();const time_t epoch=time(nullptr);const bool clock=epoch>1700000000;
    bool complete=true,reported=clock;JsonDocument d;
    d["project"]=PROJECT_NAME;d["version"]=FIRMWARE_VERSION;d["ip"]=WiFi.localIP().toString();
    d["uptimeSeconds"]=now/1000;d["freeHeap"]=ESP.getFreeHeap();d["demo"]=s.demo;
    d["message"]=s.message;d["skewSeconds"]=s.skewSeconds;
    for(int i=0;i<energy::Count;++i) {
        const auto& r=s.readings[i];auto out=d["readings"].add<JsonObject>();
        bool live=energy::metricUsable(s,static_cast<energy::Metric>(i),WiFi.isConnected(),now,clock ? uint64_t(epoch)*1000 : 0,c.staleSeconds,c.pollSeconds);
        if (i < energy::SourceCount) out["entity"]=c.entities[i];
        else out["entity"]=nullptr;
        out["derived"]=i==energy::Solar;
        const char* names[]={"Inverter AC","Household","Grid (+import)","Battery (+discharge)","Battery level","Inverter DC","Solar generation"};
        out["name"]=names[i];out["value"]=r.value;out["valid"]=r.valid;out["fresh"]=live;
        out["hasValue"]=r.valid || r.receivedMs!=0;
        out["receivedAgeSeconds"]=r.receivedMs ? uint32_t(now-r.receivedMs)/1000 : 0;
        out["reportedEpochMs"]=r.reportedMs;
        if (clock && r.reportedMs && uint64_t(epoch)*1000>=r.reportedMs) out["reportedAgeSeconds"]=(uint64_t(epoch)*1000-r.reportedMs)/1000;
        out["unit"]=i==energy::Soc ? "%" : "W";
    }
    for(auto metric:energy::displayMetrics) {
        complete &= energy::metricUsable(s,metric,WiFi.isConnected(),now,clock ? uint64_t(epoch)*1000 : 0,c.staleSeconds,c.pollSeconds);
        reported &= s.readings[metric].reportedMs!=0;
    }
    d["complete"]=complete;d["battery"]=c.batteryLabel;d["skewLimitSeconds"]=energy::skewLimitSeconds(c.pollSeconds);d["reportTimeKnown"]=reported;json(d);
}

void uploadChunk() {
    HTTPUpload& upload=server.upload();
    if (upload.status==UPLOAD_FILE_START) {
        uploadAuth=server.authenticate("admin",monitor::accessCode().c_str()) && server.header("X-CYD-Request")=="1";
        uploadOK=false;updateError="";
        if (!uploadAuth) return;
        uploading=true;source::pause(true);
        if (!Update.begin(UPDATE_SIZE_UNKNOWN,U_FLASH)) updateError="Cannot start firmware update";
    } else if (upload.status==UPLOAD_FILE_WRITE && uploadAuth && updateError.isEmpty()) {
        if (Update.write(upload.buf,upload.currentSize)!=upload.currentSize) updateError="Firmware write failed";
    } else if (upload.status==UPLOAD_FILE_END && uploadAuth) {
        uploadOK=updateError.isEmpty() && Update.end(true);
        if (!uploadOK) { Update.abort();updateError="Invalid or incomplete app firmware";source::pause(false);uploading=false; }
    } else if (upload.status==UPLOAD_FILE_ABORTED && uploadAuth) {
        Update.abort();source::pause(false);uploading=false;updateError="Upload interrupted";
    }
}

void begin() {
    const char* headers[]={"X-CYD-Request"};server.collectHeaders(headers,1);
    server.on("/",HTTP_GET,[]{
        if(!auth())return;
        server.sendHeader("Cache-Control","no-store");
        server.sendHeader("X-Content-Type-Options","nosniff");
        server.sendHeader("X-Frame-Options","DENY");
        server.send_P(200,"text/html",WEB_PAGE);
    });
    server.on("/api/config",HTTP_GET,getConfig);server.on("/api/config",HTTP_POST,saveConfig);
    server.on("/api/status",HTTP_GET,getStatus);
    server.on("/api/reboot",HTTP_POST,[]{if(!mutation())return;server.send(200,"application/json","{\"restarting\":true}");requestReboot();});
    server.on("/api/wifi-reset",HTTP_POST,[]{
        if(!mutation())return;
        server.send(200,"application/json","{\"restarting\":true}");
        WiFi.disconnect(false,true);requestReboot();
    });
    server.on("/update",HTTP_POST,[]{
        if(!mutation())return;
        if(!uploadOK) { error(updateError.isEmpty()?"No firmware uploaded":updateError.c_str());return; }
        server.send(200,"application/json","{\"updated\":true}");requestReboot();
    },uploadChunk);
    server.onNotFound([]{server.send(404,"application/json","{\"error\":\"Not found\"}");});
    server.begin();started=true;
}
}  // namespace
void requestReboot() { rebootAt=millis()+1000; }
bool updating() { return uploading; }
void update() {
    if (!started && WiFi.isConnected()) begin();
    if (started) server.handleClient();
    if(rebootAt && static_cast<int32_t>(millis()-rebootAt)>=0) ESP.restart();
}
}  // namespace web
