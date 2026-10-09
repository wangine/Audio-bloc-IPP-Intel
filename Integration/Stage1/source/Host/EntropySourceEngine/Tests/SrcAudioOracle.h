#pragma once
#include "reference/BaselineSourceBlockExecutor.h"
#include <entropy/src_abi.h>
#include <cstring>

// Independent native SRC replay and frozen matrix/mixer executor. Test only.
// Reference queues and buffers are allocated before starting the live workers.
class SrcAudioOracle {
    entropy::host::ModuleInstance src_;
    entropy::host::SourceGraphRuntime graph_;
    entropy::host::BaselineSourceBlockExecutor executor_;
    EntropySrcApiV1 api_{};
    EntropySrcConfig config_{};
    std::array<std::array<double,256>,2> input_{};
    std::array<std::vector<double>,2> output_,queue_;
    std::array<std::array<double,1024>,2> converted_{};
    std::array<std::array<double,1024>,10> expected_{};
    uint64_t inputIndex_=0;
    size_t head_=0,fill_=0,capacity_=0;
public:
    explicit SrcAudioOracle(std::shared_ptr<entropy::host::ModuleLibrary> lib):src_(lib) {}
    bool prepare(unsigned rate,const char* moduleRoot,std::string& error,unsigned engine=0) {
        if(!src_.create(error)||!src_.prepare(192000,1024,2,2,error)) return false;
        api_.struct_size=sizeof(api_);
        if(!src_.queryExtension(ENTROPY_SRC_EXTENSION_ID,ENTROPY_SRC_EXTENSION_VERSION,&api_,sizeof(api_),error)) return false;
        config_.struct_size=sizeof(config_); config_.source_rate=rate; config_.destination_rate=192000;
        config_.channels=2; config_.engine=engine; config_.maximum_input_frames=256;
        if(api_.configure(src_.handle(),&config_)!=ENTROPY_OK) return false;
        capacity_=size_t(config_.required_output_capacity)+2048;
        for(unsigned c=0;c<2;++c) { output_[c].resize(config_.required_output_capacity); queue_[c].resize(capacity_); }
        return graph_.prepare(moduleRoot,error)&&executor_.prepare(graph_,error);
    }
    bool next() noexcept {
        // The existing startup probe establishes output appears within 1024 packets.
        for(unsigned attempts=0;fill_<1024&&attempts<2048;++attempts) {
            for(unsigned i=0;i<256;++i) { auto n=inputIndex_++; double x=n==config_.source_rate/4?0.5:(int(n%127)-63)/131072.0; input_[0][i]=x; input_[1][i]=-x; }
            std::array<const double*,2> ip{input_[0].data(),input_[1].data()};
            std::array<double*,2> op{output_[0].data(),output_[1].data()};
            EntropySrcBlockF64 b{}; b.struct_size=sizeof(b); b.channels=2; b.input_frames=256;
            b.output_capacity=config_.required_output_capacity; b.inputs=ip.data(); b.outputs=op.data();
            if(api_.process(src_.handle(),&b)!=ENTROPY_OK||b.consumed_frames!=256||fill_+b.produced_frames>capacity_) return false;
            for(unsigned c=0;c<2;++c) for(unsigned i=0;i<b.produced_frames;++i) queue_[c][(head_+fill_+i)%capacity_]=output_[c][i];
            fill_+=b.produced_frames;
        }
        if(fill_<1024) return false;
        for(unsigned c=0;c<2;++c) for(unsigned i=0;i<1024;++i) converted_[c][i]=queue_[c][(head_+i)%capacity_];
        head_=(head_+1024)%capacity_; fill_-=1024; return true;
    }
    bool sourceIdentical(double* const* actual) const noexcept {
        return !std::memcmp(actual[0],converted_[0].data(),8192)&&!std::memcmp(actual[1],converted_[1].data(),8192);
    }
    bool outputIdentical(const StageBusFrameRequest& request,
                         entropy::host::AudioBlockReport music,
                         const entropy::host::AudioBlockReport& empty,double* const* actual) noexcept {
        music.convertedAudioPointers[0]=converted_[0].data(); music.convertedAudioPointers[1]=converted_[1].data();
        auto baselineRequest=request; baselineRequest.program.effective_mute_mask=0;
        std::array<double*,10> op{}; for(unsigned c=0;c<10;++c) op[c]=expected_[c].data();
        if(!executor_.process(baselineRequest,music,empty,empty,op.data()).passed) return false;
        for(unsigned c=0;c<10;++c) if(std::memcmp(actual[c],op[c],8192)) return false;
        return true;
    }
};
