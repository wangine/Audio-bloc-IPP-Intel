#include "DspLocalDemand.h"
#include "DspAutoLink.h"
#include "TestSupport.h"
int main(){
 std::string error;auto s=session(2);StageBusWin32 producer;CHECK(producer.create(s,error));DspLocalDemand local;
 StageBusProgramSnapshot program{};program.required_inputs=3;program.music_program_active=1;program.manual_mute_mask=2;program.effective_mute_mask=2;
 CHECK(local.attach(s,program,5,error));CHECK(local.step(qpc())!=DspStepResult::Fault);CHECK(local.pending());
 StageBusFrameRequest r{};CHECK(producer.tryReceiveRequest(r)==StageBusIoResult::Ok);CHECK(r.block_sequence==1 && r.first_frame==0 && r.frames==1024 && r.control_revision==5);CHECK(!std::memcmp(&r.program,&program,64));
 CHECK(local.step(qpc())!=DspStepResult::Fault);StageBusFrameRequest other{};CHECK(producer.tryReceiveRequest(other)==StageBusIoResult::Empty);
 Planes<11> planes;auto h=header(r);CHECK(producer.tryPublish(h,planes.in.data())==StageBusIoResult::Ok);CHECK(local.step(qpc())==DspStepResult::Published);CHECK(local.blocks()==1 && !local.pending());
 StageBusSourceControl control{};std::memcpy(control.session_id,s.session_id,16);control.source_revision=9;control.music_right=1;control.manual_mute_mask=5;CHECK(producer.trySendSourceControl(control)==StageBusIoResult::Ok);
 CHECK(local.step(qpc()+frequency()/100)!=DspStepResult::Fault);CHECK(producer.tryReceiveRequest(r)==StageBusIoResult::Ok);CHECK(r.block_sequence==2 && r.first_frame==1024 && r.control_revision==9 && r.program.manual_mute_mask==5 && r.program.effective_mute_mask==5 && !r.program.hardware_armed);
 local.beginQuiesce();CHECK(local.pending());h=header(r);CHECK(producer.tryPublish(h,planes.in.data())==StageBusIoResult::Ok);CHECK(local.step(qpc())==DspStepResult::Published);CHECK(!local.pending());CHECK(local.step(qpc()+frequency())==DspStepResult::Idle);CHECK(producer.tryReceiveRequest(r)==StageBusIoResult::Empty);local.close();
 producer.close();s=session(2);CHECK(producer.create(s,error));CHECK(local.attach(s,program,5,error));auto epoch=qpc();
 for(uint64_t seq=1;seq<=10;seq++){auto due=epoch+(seq-1)*frequency()*1024/192000+(seq%2?10:20);CHECK(local.step(due)!=DspStepResult::Fault);CHECK(producer.tryReceiveRequest(r)==StageBusIoResult::Ok);CHECK(r.block_sequence==seq);h=header(r);CHECK(producer.tryPublish(h,planes.in.data())==StageBusIoResult::Ok);CHECK(local.step(due+100)==DspStepResult::Published);}local.beginQuiesce();local.close();producer.close();
 DspAutoLink link(entropy::link::endpointName()+L".absent-test");DspStageRuntime runtime;CHECK(link.poll(qpc(),runtime,local,error)==DspAutoLinkState::WaitingSource);CHECK(!runtime.pending());CHECK(link.finish(runtime,local,error));
 std::cout<<"PASS bounded local demand, one pending request, sequence/revision/control identity, quiesce and missing-source wait\n";
}
