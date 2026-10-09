#include "AuditHooks.h"
void (*auditBoundaryHook)(unsigned,unsigned,const double*const*,unsigned)=nullptr;
void (*auditProgramHook)(const double*const*,uint64_t,unsigned)=nullptr;

void (*auditFifoOperationHook)(const void*)=nullptr;
void (*auditResetRefusedHook)(const void*)=nullptr;
