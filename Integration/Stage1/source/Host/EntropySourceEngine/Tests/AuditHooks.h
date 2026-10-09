#pragma once
#include <cstdint>
extern void (*auditBoundaryHook)(unsigned,unsigned,const double*const*,unsigned);
extern void (*auditProgramHook)(const double*const*,uint64_t,unsigned);
inline void auditBoundary(unsigned kind,unsigned role,const double*const*p,unsigned n){if(auditBoundaryHook)auditBoundaryHook(kind,role,p,n);}
inline void auditProgram(const double*const*p,uint64_t seq,unsigned validity){if(auditProgramHook)auditProgramHook(p,seq,validity);}

extern void (*auditFifoOperationHook)(const void*);
extern void (*auditResetRefusedHook)(const void*);
