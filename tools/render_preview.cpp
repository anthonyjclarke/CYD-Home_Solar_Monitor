#include "config.h"
#include "flow_layout.h"
#include "history_layout.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>

std::string escaped(const char* s) {
    std::string out;
    for (;*s;++s) {
        switch(*s) { case '&':out+="&amp;";break;case '<':out+="&lt;";break;
            case '>':out+="&gt;";break;case '"':out+="&quot;";break;case '`':out+="&#176;";break;default:out+=*s; }
    }
    return out;
}
std::string color(unsigned c) { std::ostringstream out;out<<'#'<<std::hex<<std::setw(6)<<std::setfill('0')<<c;return out.str(); }
class SvgCanvas: public ui::Canvas {
public:
    // TFT_eSPI glyph advance widths for fonts 2, 4 and 6 (ASCII 32-127), copied
    // from its Font16.c, Font32rle.c and Font64rle.c so previews place text as
    // the panel does. Font 2 uses the grave-as-degree variant.
    int textWidth(int font,const char* s) override {
        static const unsigned char f2[96]={6,3,4,9,8,9,9,3,7,7,8,6,3,6,5,7,8,8,8,8,8,8,8,8,8,8,3,3,6,6,6,8,9,8,8,8,8,8,8,8,8,4,8,8,7,10,8,8,8,8,8,8,8,8,8,10,8,8,8,4,7,4,7,9,5,7,7,7,7,7,6,7,7,4,5,6,4,8,7,8,7,8,6,6,5,7,8,8,6,7,7,5,3,5,8,6};
        static const unsigned char f4[96]={5,8,8,19,14,21,17,6,8,8,12,10,7,8,7,8,14,14,14,14,14,14,14,14,14,14,7,7,14,9,14,13,25,16,17,18,18,16,15,19,18,6,13,17,13,21,18,19,16,19,17,16,14,18,15,23,15,16,16,9,13,9,12,13,9,14,15,13,15,14,8,15,15,6,6,12,6,22,15,15,15,15,8,12,7,14,12,18,13,13,12,13,13,13,15,1};
        static const unsigned char f6[96]={12,12,12,12,12,12,12,12,12,12,12,12,12,17,15,12,27,27,27,27,27,27,27,27,27,27,15,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,27,12,12,12,12,12,12,12,12,12,12,12,42,12,12,29,12,12,12,12,12,12,12,12,12,12,12,7,12,12,12};
        const unsigned char* t=font==6?f6:font==4?f4:font==2?f2:nullptr;
        int w=0;
        for(;*s;++s) w+=t?(*s>=32?t[*s-32]:0):6;  // char is signed: >= 32 excludes 128+.
        return w;
    }
    void rect(int x,int y,int w,int h,unsigned c,int r=0) override {
        std::cout<<"<rect x='"<<x<<"' y='"<<y<<"' width='"<<w<<"' height='"<<h<<"' rx='"<<r<<"' fill='"<<color(c)<<"'/>\n";
    }
    void line(int x,int y,int a,int b,unsigned c) override {
        std::cout<<"<path d='M"<<x<<' '<<y<<" L"<<a<<' '<<b<<"' fill='none' stroke='"<<color(c)<<"'/>\n";
    }
    void text(int x,int y,int font,unsigned c,ui::Align align,const char* value) override {
        // Baselines as in TFT_eSPI: font 2 is 13 px and font 4 19 px below the text top.
        int size=font==6?44:font==4?25:font==2?13:9, base=font==6?size-2:font==4?19:font==2?13:7;
        std::cout<<"<text x='"<<x<<"' y='"<<y+base<<"' font-size='"<<size<<"' text-anchor='"<<(align==ui::Left?"start":align==ui::Right?"end":"middle")<<"' fill='"<<color(c)<<"'>"<<escaped(value)<<"</text>\n";
    }
};
int main(int argc,char**argv) {
    SvgCanvas c;
    // A leading "wide" renders the 4.0-inch's native 480 x 320 layouts.
    ui::Panel panel=ui::Panel320;
    if (argc>1 && std::string(argv[1])=="wide") { panel=ui::Panel480; --argc; ++argv; }
    const char* head=panel==ui::Panel480?
        "<svg xmlns='http://www.w3.org/2000/svg' width='960' height='640' viewBox='0 0 480 320'><style>text{font-family:DejaVu Sans,sans-serif}</style>\n":
        "<svg xmlns='http://www.w3.org/2000/svg' width='960' height='720' viewBox='0 0 320 240'><style>text{font-family:DejaVu Sans,sans-serif}</style>\n";
    if (argc>1 && std::string(argv[1])=="settings") {  // settings [portal]: device screen, sample address and code.
        ui::SettingsView s; s.panel=panel; s.connected=argc<=2;
        s.webUrl="http://solar.local/"; s.ip="192.0.2.95"; s.apName="CYD-Solar-1A2B";
        s.accessCode="4821-7730"; s.haUrl="http://homeassistant.local:8123"; s.version=FIRMWARE_VERSION;
        std::cout<<head;ui::drawSettings(c,s);std::cout<<"</svg>\n";return 0;
    }
    if (argc>1 && std::string(argv[1])=="history") {
        // Explicitly synthetic history for layout QA, never persisted on a CYD.
        history::Store store;
        const uint32_t epoch=history::bucket(1793836800);
        const std::string scene=argc>2?argv[2]:"solar";
        if (scene!="empty") for (unsigned i=0;i<history::capacity;++i) {
            if ((i>=100 && i<125) || i==230) continue; // Visible outage gaps.
            auto s=energy::demoSnapshot(1000,i/72%4); s.demo=false;
            const uint32_t capturedAt=epoch-(287-i)*history::interval;
            for (auto& r:s.readings) r.reportedMs=uint64_t(capturedAt)*1000;
            s.heartbeatMs=uint64_t(capturedAt)*1000;
            s.readings[energy::Solar].value=std::max(0.0f,4500*std::sin(float(i)/288*6.28f));
            store.add(history::capture(s,true,1000,capturedAt,180,5));
        }
        ui::HistoryView v{store}; v.epoch=epoch; v.storageOK=true; v.panel=panel;
        if (scene=="battery") v.field=history::Battery;
        if (scene=="weather") v.field=history::TomorrowTemperature;
        if (scene=="dark") v.layout=ui::Cards;
        std::cout<<head;ui::drawHistory(c,v);std::cout<<"</svg>\n";return 0;
    }
    if(argc>1&&std::string(argv[1])=="boot") {  // boot <layout>: sample start-up screen after Wi-Fi connects.
        ui::BootView b;b.panel=panel;b.layout=argc>2?static_cast<ui::Layout>(std::atoi(argv[2])):ui::Centre;b.version="v" FIRMWARE_VERSION;
        boot::Inputs in;
        in.connected=in.clock=in.token=in.solar=in.sourceTimeKnown=in.historyHealthy=true;
        in.sources=6; in.restore=history::RestoreState::Restored; in.historySamples=144;
        in.ssid="HomeNetwork"; in.ip="192.0.2.95"; in.rssi=-37; in.localTime="17:08 AEDT";
        if (argc>3) {
            in.connected=in.clock=in.token=false; in.sources=0; in.solar=false;
            in.portal=std::string(argv[3])=="warning";
            in.historyHealthy=false;
            in.restore=in.portal?history::RestoreState::Unavailable:history::RestoreState::Starting;
            if (!in.portal) in.token=true;
        }
        const auto status=boot::build(in);
        for (unsigned i=0;i<boot::RowCount;++i) b.rows[i]={boot::label(i),status.rows[i].value,status.rows[i].state};
        b.progress=.6f;b.footer=in.connected?"Starting display in 3 s - tap to skip":"Waiting for Wi-Fi - tap to skip";
        std::cout<<head;ui::drawBoot(c,b);std::cout<<"</svg>\n";return 0;
    }
    unsigned scene=argc>1?std::atoi(argv[1]):0;
    ui::FlowView v;v.layout=argc>2?static_cast<ui::Layout>(std::atoi(argv[2])):ui::Cards;v.time="14:32";v.data=energy::demoSnapshot(1000,scene);v.wifi=true;v.phase=1;v.panel=panel;
    for(bool& b:v.usable)b=true;
    for(bool& b:v.totalLive)b=true;
    v.weatherLive=true;
    if(argc>3)for(bool& b:v.totalLive)b=false;  // No HA totals: clock and weather only.
    // Sample values come from the demo scenes, but are drawn as live readings.
    v.data.demo=false;v.status="Live / hold for settings";v.address="192.168.1.50";v.batteryLabel="LG 9.8 kWh";
    if(scene==4){for(bool& b:v.usable)b=false;for(bool& b:v.totalLive)b=false;v.weatherLive=false;v.data.demo=false;v.wifi=false;v.status="WiFi disconnected";}
    if(scene==5){v.weatherLive=false;v.data=energy::Snapshot();for(bool& b:v.usable)b=false;for(bool& b:v.totalLive)b=false;v.status="Waiting / stale or unavailable";}
    std::cout<<head;
    ui::drawFlow(c,v);std::cout<<"</svg>\n";
}

