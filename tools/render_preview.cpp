#include "flow_layout.h"
#include <iostream>
#include <sstream>
#include <iomanip>

std::string escaped(const char* s) {
    std::string out;
    for (;*s;++s) {
        switch(*s) { case '&':out+="&amp;";break;case '<':out+="&lt;";break;
            case '>':out+="&gt;";break;case '"':out+="&quot;";break;default:out+=*s; }
    }
    return out;
}
std::string color(unsigned c) { std::ostringstream out;out<<'#'<<std::hex<<std::setw(6)<<std::setfill('0')<<c;return out.str(); }
class SvgCanvas: public ui::Canvas {
public:
    void rect(int x,int y,int w,int h,unsigned c,int r=0) override {
        std::cout<<"<rect x='"<<x<<"' y='"<<y<<"' width='"<<w<<"' height='"<<h<<"' rx='"<<r<<"' fill='"<<color(c)<<"'/>\n";
    }
    void line(int x,int y,int a,int b,unsigned c) override {
        std::cout<<"<path d='M"<<x<<' '<<y<<" L"<<a<<' '<<b<<"' fill='none' stroke='"<<color(c)<<"'/>\n";
    }
    void text(int x,int y,int font,unsigned c,ui::Align align,const char* value) override {
        int size=font==4?25:font==2?13:9;
        std::cout<<"<text x='"<<x<<"' y='"<<y+size-2<<"' font-size='"<<size<<"' text-anchor='"<<(align==ui::Left?"start":align==ui::Right?"end":"middle")<<"' fill='"<<color(c)<<"'>"<<escaped(value)<<"</text>\n";
    }
};
int main(int argc,char**argv) {
    unsigned scene=argc>1?std::atoi(argv[1]):0;
    ui::FlowView v;v.data=energy::demoSnapshot(1000,scene);v.wifi=true;v.phase=1;
    for(bool& b:v.usable)b=true;
    v.status="DEMO / tap for setup";v.address="192.168.1.50";v.batteryLabel="LG 9.8 kWh";
    if(scene==4){for(bool& b:v.usable)b=false;v.data.demo=false;v.wifi=false;v.status="WiFi disconnected";}
    if(scene==5){v.data=energy::Snapshot();for(bool& b:v.usable)b=false;v.status="Configure Home Assistant";}
    SvgCanvas c;
    std::cout<<"<svg xmlns='http://www.w3.org/2000/svg' width='960' height='720' viewBox='0 0 320 240'><style>text{font-family:DejaVu Sans,sans-serif}</style>\n";
    ui::drawFlow(c,v);std::cout<<"</svg>\n";
}

