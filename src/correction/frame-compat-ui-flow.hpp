#pragma once
#include <string_view>
namespace frame_compat_ui {
enum class Command { None, Observe, Activate };
// UI intent only: the native controller independently revalidates everything.
// A saved ON preference is the same intent as clicking ON. OFF never attaches.
class Flow {
    bool observationSent_=false, activationSent_=false, failed_=false;
public:
    Command next(std::string_view verdict, bool requested) {
        if(failed_||!requested)return Command::None;
        if(verdict=="structurally_compatible"&&!observationSent_){
            observationSent_=true;return Command::Observe;
        }
        if(verdict=="verified"&&observationSent_&&!activationSent_){
            activationSent_=true;return Command::Activate;
        }
        return Command::None;
    }
    void failed(){failed_=true;}
};
}
