#include "flow_layout.h"
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
        int size=font==6?44:font==4?25:font==2?13:9;
        std::cout<<"<text x='"<<x<<"' y='"<<y+size-2<<"' font-size='"<<size<<"' text-anchor='"<<(align==ui::Left?"start":align==ui::Right?"end":"middle")<<"' fill='"<<color(c)<<"'>"<<escaped(value)<<"</text>\n";
    }
};
int main(int argc,char**argv) {
    SvgCanvas c;
    const char* head="<svg xmlns='http://www.w3.org/2000/svg' width='960' height='720' viewBox='0 0 320 240'><style>text{font-family:DejaVu Sans,sans-serif}</style>\n";
    if(argc>1&&std::string(argv[1])=="boot") {  // boot <layout>: sample start-up screen after Wi-Fi connects.
        ui::BootView b;b.layout=argc>2?static_cast<ui::Layout>(std::atoi(argv[2])):ui::Centre;b.version="v0.6.0-dev";
        b.rows[0]={"Wi-Fi","Connected - HomeNetwork",ui::BootOk};b.rows[1]={"IP address","192.168.1.95",ui::BootInfo};
        b.rows[2]={"Web page","http://solar.local/",ui::BootInfo};b.rows[3]={"Signal","-37 dBm (excellent)",ui::BootInfo};
        b.rows[4]={"Home Asst","192.168.1.254:8123",ui::BootInfo};b.rows[5]={"Clock","17:08 AEDT - synced",ui::BootOk};
        b.progress=.6f;b.footer="Starting display in 3 s - tap to skip";
        std::cout<<head;ui::drawBoot(c,b);std::cout<<"</svg>\n";return 0;
    }
    unsigned scene=argc>1?std::atoi(argv[1]):0;
    ui::FlowView v;v.layout=argc>2?static_cast<ui::Layout>(std::atoi(argv[2])):ui::Cards;v.time="14:32";v.data=energy::demoSnapshot(1000,scene);v.wifi=true;v.phase=1;
    for(bool& b:v.usable)b=true;
    for(bool& b:v.totalLive)b=true;
    v.weatherLive=true;
    if(argc>3)for(bool& b:v.totalLive)b=false;  // No HA totals: clock and weather only.
    v.status="DEMO / tap for setup";v.address="192.168.1.50";v.batteryLabel="LG 9.8 kWh";
    if(scene==4){for(bool& b:v.usable)b=false;for(bool& b:v.totalLive)b=false;v.weatherLive=false;v.data.demo=false;v.wifi=false;v.status="WiFi disconnected";}
    if(scene==5){v.weatherLive=false;v.data=energy::Snapshot();for(bool& b:v.usable)b=false;for(bool& b:v.totalLive)b=false;v.status="Configure Home Assistant";}
    std::cout<<head;
    ui::drawFlow(c,v);std::cout<<"</svg>\n";
}

