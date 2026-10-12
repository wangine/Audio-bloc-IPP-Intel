#pragma once
#include <audioclient.h>
// Eligibility of a packet flagged SILENT, never an amplitude-based inference.
inline bool validStereoSilence(bool stereoLocked,bool encodedLocked,bool encodedActive,DWORD flags) noexcept {
 return stereoLocked && !encodedLocked && !encodedActive &&
        (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0 &&
        (flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) == 0;
}
inline bool stereoTransportDiscontinuity(bool encodedLocked,DWORD flags) noexcept {
 return !encodedLocked && (flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) != 0;
}
