#include <entropy/stage_bus_v1.h>
#include <fstream>
#include <iostream>
int main(int argc, char **argv)
{
    if (argc != 2)
        return 2;
    StageBusSessionHeader s{};
    s.magic = STAGE_BUS_MAGIC;
    s.abi_version = STAGE_BUS_ABI;
    s.header_bytes = sizeof(s);
    s.session_id[0] = 0x12;
    s.slot_stride = 0x123456789abcdef0ull;
    StageBusBlockHeader b{};
    b.stream_epoch = 0x1122334455667788ull;
    b.frames = 1024;
    b.music_session_revision = 17;
    StageBusFrameRequest r{};
    r.program.required_inputs = 3;
    r.program.program_revision = 0x1234567890ull;
    StageBusSourceControl c{};
    c.source_revision = 0x1020304050607080ull;
    c.manual_mute_mask = 5;
    c.music_right = 1;
    c.music_binding_revision = 0xabcdef123456ull;
    std::ofstream out(argv[1], std::ios::binary);
    out.write(reinterpret_cast<const char *>(&s), sizeof(s));
    out.write(reinterpret_cast<const char *>(&b), sizeof(b));
    out.write(reinterpret_cast<const char *>(&r), sizeof(r));
    out.write(reinterpret_cast<const char *>(&c), sizeof(c));
    return out.good() ? 0 : 1;
}
