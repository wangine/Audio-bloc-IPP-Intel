#include "TestSupport.h"
#include "DspStageRuntime.h"
int main(){
 auto s=session(1);StageBusWin32 source,render;std::string error;Planes<10> input;Planes<11> output;
 CHECK(source.create(s,error));DspStageRuntime dsp;CHECK(dsp.attach(s,error));CHECK(render.open(dsp.outputSession(),error));
 StageBusSourceControl control{};std::memcpy(control.session_id,s.session_id,16);control.source_revision=19;control.manual_mute_mask=5;control.music_right=1;
 CHECK(source.trySendSourceControl(control)==StageBusIoResult::Ok);dsp.step(qpc());StageBusSourceControl gotControl{};CHECK(render.tryReceiveSourceControl(gotControl)==StageBusIoResult::Ok);CHECK(!std::memcmp(&control,&gotControl,64));
 // Keep output control occupied while newer revisions are coalesced locally.
 control.source_revision=20;CHECK(source.trySendSourceControl(control)==StageBusIoResult::Ok);dsp.step(qpc());
 control.source_revision=21;CHECK(source.trySendSourceControl(control)==StageBusIoResult::Ok);dsp.step(qpc());
 control.source_revision=22;CHECK(source.trySendSourceControl(control)==StageBusIoResult::Ok);dsp.step(qpc());
 CHECK(render.tryReceiveSourceControl(gotControl)==StageBusIoResult::Ok && gotControl.source_revision==20);
 dsp.step(qpc());CHECK(render.tryReceiveSourceControl(gotControl)==StageBusIoResult::Ok);CHECK(!std::memcmp(&control,&gotControl,64));CHECK(render.tryReceiveSourceControl(gotControl)==StageBusIoResult::Empty);
 StageBusBlockHeader first{};
 for(uint64_t seq=1;seq<=5;seq++){
  auto r=request(s,seq);r.control_revision=19;r.program.program=1;r.program.required_inputs=2;r.program.hardware_armed=1;
  CHECK(render.trySendRequest(r)==StageBusIoResult::Ok);CHECK(dsp.step(qpc())!=DspStepResult::Fault);
  StageBusFrameRequest got{};CHECK(source.tryReceiveRequest(got)==StageBusIoResult::Ok);CHECK(!std::memcmp(&r,&got,56));
  CHECK(got.program.program==0 && got.program.required_inputs==3 && got.program.music_program_active==1 && !got.program.hardware_armed);
  for(unsigned c=0;c<10;c++)for(unsigned i=0;i<1024;i++)input.data[c][i]=seq*100000+c*1024+i;
  auto b=header(got);if(seq==1)first=b;CHECK(source.tryPublish(b,input.in.data())==StageBusIoResult::Ok);
  auto result=dsp.step(qpc());CHECK(result!=DspStepResult::Fault);
  if(seq<5)CHECK(dsp.diagnostics().blocksOut==seq);else CHECK(dsp.diagnostics().blocksOut==4);
 }
 CHECK(dsp.pending());CHECK(dsp.diagnostics().framesIn==5120);CHECK(dsp.diagnostics().framesOut==4096);
 StageBusBlockHeader b{};CHECK(render.tryConsume(b,output.out.data())==StageBusIoResult::Ok);CHECK(!std::memcmp(&first,&b,128));
 CHECK(dsp.step(qpc())==DspStepResult::Published);CHECK(!dsp.pending());
 for(uint64_t seq=2;seq<=5;seq++){CHECK(render.tryConsume(b,output.out.data())==StageBusIoResult::Ok);CHECK(b.block_sequence==seq);for(unsigned c=0;c<10;c++)for(unsigned i=0;i<1024;i++)CHECK(output.data[c<2?c:c+1][i]==seq*100000+c*1024+i);for(double x:output.data[2])CHECK(bits(x)==0);}
 auto duplicate=request(s,5);duplicate.control_revision=19;duplicate.program.program=1;duplicate.program.required_inputs=2;duplicate.program.hardware_armed=1;CHECK(render.trySendRequest(duplicate)==StageBusIoResult::Ok);dsp.step(qpc());StageBusBlockHeader ack{};CHECK(render.tryReceiveResponse(ack)==StageBusIoResult::Ok);CHECK(!std::memcmp(&ack,&b,128));CHECK(source.tryReceiveRequest(duplicate)==StageBusIoResult::Empty);CHECK(render.tryConsume(b,output.out.data())==StageBusIoResult::Empty);
 // Duplicate while processing must not acknowledge an incomplete result.
 auto r=request(s,6);r.control_revision=19;CHECK(render.trySendRequest(r)==StageBusIoResult::Ok);dsp.step(qpc());CHECK(source.tryReceiveRequest(duplicate)==StageBusIoResult::Ok);
 CHECK(render.trySendRequest(r)==StageBusIoResult::Ok);dsp.step(qpc());CHECK(render.tryReceiveResponse(ack)==StageBusIoResult::Empty);
 b=header(r);CHECK(source.tryPublish(b,input.in.data())==StageBusIoResult::Ok);dsp.step(qpc());dsp.step(qpc());CHECK(render.tryReceiveResponse(ack)==StageBusIoResult::Ok);CHECK(ack.block_sequence==6);CHECK(render.tryConsume(b,output.out.data())==StageBusIoResult::Ok);
 r.control_revision++;CHECK(render.trySendRequest(r)==StageBusIoResult::Ok);CHECK(dsp.step(qpc())==DspStepResult::Fault);
 std::cout<<"PASS two-layout same UUID, request/header/control identity, full pending block, no overwrite, duplicate pending/completed and conflicting snapshot fault\n";
}
