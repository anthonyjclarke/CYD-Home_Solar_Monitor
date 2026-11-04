#include "history_layout.h"
#include <cstdio>
#include <ctime>

namespace ui {
namespace {
constexpr unsigned paper=0xffffff, ink=0x242455, soft=0x6d6d55, rule=0xb6b6aa;
constexpr unsigned dark=0x000000, white=0xffffff, muted=0x9292aa, darkRule=0x242455;
constexpr unsigned colors[] = {0x6d6daa,0x6d6daa,0x2449aa,0x249255,0x249255,0x6d6daa,0x926d00};
constexpr bool level(unsigned v, unsigned steps) { return v == ((v*steps+127)/255*255+steps/2)/steps; }
constexpr bool rgb332(unsigned v) { return level(v>>16,7) && level((v>>8)&255,7) && level(v&255,3); }
static_assert(rgb332(paper) && rgb332(ink) && rgb332(soft) && rgb332(rule) && rgb332(darkRule) &&
              rgb332(muted) && rgb332(colors[0]) && rgb332(colors[2]) && rgb332(colors[3]) && rgb332(colors[6]),
              "History palette must use exact RGB332 levels");
void value(char* out, size_t size, float v, unsigned field) {
    if (field < energy::Count && field != history::Soc) std::snprintf(out,size,"%.2f kW",v/1000);
    else std::snprintf(out,size,"%.1f %s",v,history::unit(field));
}
void localClock(char* out, size_t size, uint32_t epoch, const char* format) {
    time_t t=epoch; tm local{};
    if (epoch && localtime_r(&t,&local)) strftime(out,size,format,&local);
    else std::snprintf(out,size,"--:--");
}
}
void drawHistory(Canvas& c, const HistoryView& v) {
    const bool light=v.layout==Centre;
    const unsigned bg=light?paper:dark, fg=light?ink:white, label=light?soft:muted, grid=light?rule:darkRule;
    const unsigned color=light && v.field<history::Today?colors[v.field]:light?ink:0xffb655;
    c.rect(0,0,320,240,bg);
    c.text(8,6,2,fg,Left,"24H HISTORY");
    char text[64], clock[24];
    localClock(clock,sizeof(clock),v.epoch,"%H:%M");
    c.text(247,7,2,label,Right,clock);
    c.rect(260,0,60,28,grid,4); c.text(290,6,2,fg,Center,"Home");
    const char* tabNames[]={"Solar","Home","Grid","Batt","Level"};
    for (unsigned i=0;i<5;++i) {
        const bool selected=v.field==history::tabs[i];
        c.rect(i*64+2,31,60,25,selected?grid:bg,3);
        c.text(i*64+32,35,2,selected?fg:label,Center,tabNames[i]);
    }
    c.text(8,61,2,color,Left,history::name(v.field));
    const auto s=history::stats(v.store,v.field,v.epoch);
    if (s.count) value(text,sizeof(text),s.latest,v.field);
    else std::snprintf(text,sizeof(text),"-- %s",v.field<history::Today && v.field!=history::Soc?"kW":history::unit(v.field));
    c.text(312,60,2,fg,Right,text);
    if (s.count) {
        localClock(clock,sizeof(clock),s.latestAt,"%d %b %H:%M");
        std::snprintf(text,sizeof(text),"Last %s | %u/288 samples",clock,s.count);
    } else std::snprintf(text,sizeof(text),"%s",v.epoch?"No actual samples yet":"Waiting for clock sync");
    c.text(8,81,1,label,Left,text);
    const bool code=v.field==history::WeatherCode || v.field==history::TomorrowCode;
    if (s.count && !code) {
        // Compact signed statistics; each column stays inside its 104 px box.
        const float scale=v.field<history::Today && v.field!=history::Soc?1000:1;
        const char* u=scale==1000?"kW":history::unit(v.field);
        const float vals[]={s.minimum,s.mean,s.maximum};
        const char* labels[]={"Min","Avg","Max"};
        for (unsigned i=0;i<3;++i) {
            std::snprintf(text,sizeof(text),"%s %.1f%s",labels[i],vals[i]/scale,u);
            c.text(8+i*104,97,1,label,Left,text);
        }
    }
    constexpr int left=42, right=310, top=118, bottom=189;
    float lo=s.count?std::min(0.0f,s.minimum):0, hi=s.count?std::max(0.0f,s.maximum):(v.field<history::Today && v.field!=history::Soc?1000:1);
    if (v.field==history::Soc) { lo=0; hi=100; }
    if (hi-lo<0.01f) hi=lo+1;
    auto y=[&](float val) { return bottom-int((val-lo)/(hi-lo)*(bottom-top)); };
    const bool kw=v.field<history::Today && v.field!=history::Soc;
    c.text(4,110,1,label,Left,kw?"kW":history::unit(v.field));
    for (unsigned i=0;i<3;++i) {
        float val=lo+(hi-lo)*i/2;
        std::snprintf(text,sizeof(text),"%.1f",val/(kw?1000:1));
        c.text(left-5,y(val)-3,1,label,Right,text);
        c.line(left,y(val),right,y(val),grid);
    }
    if (lo<0 && hi>0) c.line(left,y(0),right,y(0),label);
    if (v.epoch) {
        const uint32_t first=history::bucket(v.epoch)-(history::capacity-1)*history::interval;
        auto x=[&](uint32_t t) { return left+int(double(int64_t(t)-v.epoch+history::duration)/history::duration*(right-left)); };
        bool pen=false; int px=0, py=0;
        for (uint32_t slot=first;slot<=history::bucket(v.epoch);slot+=history::interval) {
            const auto* r=v.store.at(slot);
            if (!r || r->capturedAt>v.epoch || !r->has(v.field)) { pen=false; continue; }
            int xx=x(r->capturedAt), yy=y(r->values[v.field]);
            if (pen && !code) c.line(px,py,xx,yy,color);
            c.rect(xx-1,yy-1,2,2,color); px=xx; py=yy; pen=true;
        }
        for (unsigned i=0;i<5;++i) {
            const uint32_t t=v.epoch-history::duration+i*history::duration/4;
            localClock(clock,sizeof(clock),t,"%H:%M");
            c.text(left+i*(right-left)/4,195,1,label,i==0?Left:i==4?Right:Center,clock);
        }
    }
    c.rect(2,211,48,27,grid,3); c.text(26,217,2,fg,Center,"Prev");
    c.rect(270,211,48,27,grid,3); c.text(294,217,2,fg,Center,"Next");
    bool unknown=false;
    if (v.epoch && v.field<history::Today) {
        const uint32_t first=history::bucket(v.epoch)-(history::capacity-1)*history::interval;
        for (const auto& r:v.store.records)
            if (r.slot>=first && r.capturedAt<=v.epoch && r.has(v.field) && !(r.sourceTimeKnown&(1U<<v.field))) unknown=true;
    }
    const char* note=v.demo?"Actual history | demo excluded":!v.storageOK?"RAM only / storage fault":unknown?"5 min | source age unknown":
        v.field==history::Grid?"+import / -export":v.field==history::Battery?"+supply / -charge":"5 min samples | gaps = no data";
    c.text(160,213,1,label,Center,note);
    c.text(160,227,1,label,Center,"Hold >2s: Settings");
}
}  // namespace ui
