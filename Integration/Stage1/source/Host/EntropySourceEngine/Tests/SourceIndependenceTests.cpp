#include "SourceFrameDemand.h"
#include "TestSupport.h"
int sourceApplicationProof(int,char**);
int sourceLinkProof(int,char**);
int sourceSelectionProof();
int sourceBoundaryProof();
int main(int argc,char**argv){
 auto s=session(MIX_CINEMA10_V1); auto r=request(s,1); r.program={};
 CHECK(validateSourceDemandRequest(s,r)==StageBusValidationResult::Ok);
 CHECK(validateRequest(s,r)!=StageBusValidationResult::Ok);
 SourceFrameDemand demand(s); CHECK(demand.accept(r)==DemandResult::Accepted);
 auto same=r; std::memset(&same.program,0xa5,sizeof(same.program));
 CHECK(validateSourceDemandRequest(s,same)==StageBusValidationResult::Ok);
 CHECK(demand.accept(same)==DemandResult::Duplicate);
 auto next=request(s,2); CHECK(demand.accept(next)==DemandResult::Fault);
 CHECK(demand.complete(1)); CHECK(demand.accept(same)==DemandResult::Duplicate);
 CHECK(demand.accept(next)==DemandResult::Accepted); CHECK(demand.complete(2));
 auto bad=next; bad.frames=512; CHECK(validateSourceDemandRequest(s,bad)!=StageBusValidationResult::Ok);
 bad=next;bad.reserved=1;CHECK(validateSourceDemandRequest(s,bad)!=StageBusValidationResult::Ok);
 bad=next;bad.session_id[0]^=1;CHECK(validateSourceDemandRequest(s,bad)!=StageBusValidationResult::Ok);
 bad=next;bad.first_frame++;CHECK(validateSourceDemandRequest(s,bad)!=StageBusValidationResult::Ok);
 bad=next;bad.control_revision=0;CHECK(validateSourceDemandRequest(s,bad)!=StageBusValidationResult::Ok);
 bad=next;bad.stream_epoch=2;CHECK(validateSourceDemandRequest(s,bad)!=StageBusValidationResult::Ok);
 auto invalid=s;invalid.capacity=5;CHECK(validateSourceDemandRequest(invalid,next)!=StageBusValidationResult::Ok);
 for(unsigned armed: {0u,1u}){same.program.hardware_armed=armed;CHECK(validateSourceDemandRequest(s,same)==StageBusValidationResult::Ok);}
 std::string error;StageBusWin32 producer,consumer;CHECK(producer.create(s,error));CHECK(consumer.open(s,error));
 CHECK(consumer.trySendSourceRequest(same)==StageBusIoResult::Ok);
 StageBusFrameRequest got{};CHECK(producer.tryReceiveSourceRequest(got)==StageBusIoResult::Ok);
 CHECK(SourceFrameEnvelope(got)==SourceFrameEnvelope(same));
 auto legacy=request(s,2);CHECK(consumer.trySendRequest(legacy)==StageBusIoResult::Ok);
 CHECK(producer.tryReceiveSourceRequest(got)==StageBusIoResult::Ok);
 CHECK(got.block_sequence==2);
 CHECK(sourceSelectionProof()==0);
 CHECK(sourceBoundaryProof()==0);
 CHECK(sourceApplicationProof(argc,argv)==0);
 CHECK(sourceLinkProof(argc,argv)==0);
 std::cout<<"PASS Source validation, hardware bit/program independence, duplicate/order, generic and legacy wire requests\n";
}
