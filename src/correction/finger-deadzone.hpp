#pragma once
#include <algorithm>
#include <cmath>

namespace frame_correction {
enum class DeadzoneMode { Manual, Dynamic };
struct DeadzoneSettings {
    bool enabled=false;
    DeadzoneMode mode=DeadzoneMode::Manual;
    double manual=0.2;
    double margin=0.02;
    double maximum=0.95;
    double updatePerSecond=0.02;
};
// One instance per side and finger. State is RAM-only and never writes input.
class FingerDeadzone {
    double baseline=0;
    bool acquired=false;
public:
    void reset(){baseline=0;acquired=false;}
    bool ready()const{return acquired;}
    double threshold(const DeadzoneSettings& s)const{
        return s.mode==DeadzoneMode::Manual?s.manual:std::min(s.maximum,baseline+s.margin);
    }
    static bool valid(const DeadzoneSettings& s){
        return std::isfinite(s.manual)&&std::isfinite(s.margin)&&std::isfinite(s.maximum)&&
            std::isfinite(s.updatePerSecond)&&s.manual>=0&&s.manual<=s.maximum&&
            s.margin>=0&&s.margin<=0.2&&s.maximum>=0&&s.maximum<=0.95&&
            s.updatePerSecond>0&&s.updatePerSecond<=0.2;
    }
    // extendedConfirmed must come from deliberate user input, never inferred curl.
    // frozen prevents updates. Reconnection/serial change must call reset().
    bool observe(double input,double dt,bool extendedConfirmed,bool frozen,const DeadzoneSettings& s){
        if(!s.enabled||s.mode!=DeadzoneMode::Dynamic||!valid(s)||!extendedConfirmed||frozen||
           !std::isfinite(input)||input<0||input>s.maximum||
           !std::isfinite(dt)||dt<=0||dt>0.25)return false;
        if(!acquired){baseline=input;acquired=true;return true;}
        baseline+=std::clamp(input-baseline,-s.updatePerSecond*dt,s.updatePerSecond*dt);
        return true;
    }
    double map(double input,const DeadzoneSettings& s)const{
        if(!s.enabled||!valid(s)||!std::isfinite(input)||input<0||input>1||
           (s.mode==DeadzoneMode::Dynamic&&!acquired))return input;
        double d=threshold(s);
        return std::clamp((input-d)/(1-d),0.0,1.0);
    }
};
}
