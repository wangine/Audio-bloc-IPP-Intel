#pragma once
void* stage02ReferenceCreate(unsigned maximumFrames);
void stage02ReferenceDestroy(void*);
bool stage02ReferenceSet(void*, const char*, double);
bool stage02ReferenceApply(void*, bool reset);
int stage02ReferenceProcess(void*, const double* const*, double* const*, unsigned);
unsigned stage02ReferenceLatency(void*);
double stage02ReferenceGet(void*, const char*);
