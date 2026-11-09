#include "frame-observation-status.hpp"
#include <cassert>
using frame_observation::parseDiagnosticStatus;
int main() {
    assert(parseDiagnosticStatus("FRAME_COMPAT_STATUS_1 10 waiting none",12).verdict=="waiting");
    assert(parseDiagnosticStatus("FRAME_COMPAT_STATUS_1 10 waiting bad",12).verdict.empty());
    assert(parseDiagnosticStatus("FRAME_COMPAT_STATUS_1 10 waiting none",14).verdict.empty());
    assert(parseDiagnosticStatus("FRAME_COMPAT_STATUS_1 10 structurally_compatible none",12).verdict=="structurally_compatible");
    assert(parseDiagnosticStatus("FRAME_COMPAT_STATUS_1 10 rejected finger_missing",13).reason=="finger_missing");
    assert(parseDiagnosticStatus("FRAME_COMPAT_STATUS_1 10 rejected finger_missing",14).verdict.empty());
    assert(parseDiagnosticStatus("FRAME_COMPAT_STATUS_1 10 rejected finger_missing",9).verdict.empty());
    assert(parseDiagnosticStatus("FRAME_COMPAT_STATUS_1 10 active none",10).verdict.empty());
    assert(parseDiagnosticStatus("FRAME_COMPAT_STATUS_1 10 observing 37",10).witnessed==37);
    assert(parseDiagnosticStatus("FRAME_COMPAT_STATUS_1 10 verified 254",10).witnessed==254);
    assert(parseDiagnosticStatus("FRAME_COMPAT_STATUS_1 10 verified 1",10).witnessed==1);
    assert(parseDiagnosticStatus("FRAME_COMPAT_STATUS_1 10 active 1",10).verdict=="active");
    assert(parseDiagnosticStatus("FRAME_COMPAT_STATUS_1 10 verified 0",10).verdict.empty());
    assert(parseDiagnosticStatus("FRAME_COMPAT_STATUS_1 10 active 0",10).verdict.empty());
    assert(parseDiagnosticStatus("FRAME_COMPAT_STATUS_1 10 active 256",10).verdict.empty());
    assert(parseDiagnosticStatus("FRAME_COMPAT_STATUS_1 10 active 255",10).verdict=="active");
    assert(parseDiagnosticStatus("FRAME_COMPAT_STATUS_1 10 restore_failed restart_required",10).verdict=="restore_failed");
    assert(parseDiagnosticStatus("FRAME_COMPAT_STATUS_1 10 rejected bad!value",10).verdict.empty());
    assert(parseDiagnosticStatus("FRAME_COMPAT_STATUS_1 10 rejected bad_value trailing",10).verdict.empty());
    assert(parseDiagnosticStatus(std::string(161,'x'),10).verdict.empty());
}
