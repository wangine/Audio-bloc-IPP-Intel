#pragma once
namespace entropy::music {
// Return 1 for a packet, 0 for no packet, -1 for error/cancellation.
template<class Read, class Wait> int readCapturePacket(Read&& read, Wait&& wait) {
    const int packet=read();
    if(packet!=0)return packet;
    const int event=wait();
    return event==1?read():event;
}
}
