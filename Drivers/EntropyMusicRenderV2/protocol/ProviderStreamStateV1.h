#pragma once
// Optional, capability-negotiated extension of endpoint_provider_v2.h.
// Structure sizes and the frozen ABI header remain unchanged.
// Only read()==0 may carry STREAM_STATE in flags and reserved[0]. No PCM
// payload is present. read()==1 still has zero reserved fields and raw bytes.
// Clients must accept this capability explicitly. Frozen v0002 rejects it;
// keep its original Provider bundle. Providers without this capability keep
// their existing NoData behavior. UNKNOWN never suppresses RUN starvation.
#define ENTROPY_CAP_STREAM_STATE_V1 4u
#define ENTROPY_PACKET_STREAM_STATE_V1 16u
#define ENTROPY_STREAM_UNKNOWN_V1 0u
#define ENTROPY_STREAM_IDLE_V1 1u
#define ENTROPY_STREAM_RUNNING_V1 2u
