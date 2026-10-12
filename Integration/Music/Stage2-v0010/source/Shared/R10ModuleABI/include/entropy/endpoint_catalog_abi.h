#pragma once
#include <entropy/module_abi.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Optional, control-plane extension. Does not replace Endpoint read V1. */
#define ENTROPY_ENDPOINT_CATALOG_ID "audio.entropy.endpoint-catalog.v1"
#define ENTROPY_ENDPOINT_CATALOG_VERSION 0x00010000u

#define ENTROPY_CATALOG_DIRECTION_UNKNOWN 0u
#define ENTROPY_CATALOG_DIRECTION_RESERVED_1 1u
#define ENTROPY_CATALOG_DIRECTION_RESERVED_2 2u
#define ENTROPY_CATALOG_DIRECTION_PRIVATE_TAP 3u
#define ENTROPY_CATALOG_DIRECTION_RESERVED_4 4u
#define ENTROPY_CATALOG_DIRECTION_RESERVED_5 5u
#define ENTROPY_CATALOG_DIRECTION_MUSIC_TRANSPORT 6u
#define ENTROPY_CATALOG_PRESENCE_UNKNOWN 0u
#define ENTROPY_CATALOG_PRESENT 1u
#define ENTROPY_CATALOG_ABSENT 2u
#define ENTROPY_CATALOG_DISABLED 3u
#define ENTROPY_CATALOG_UNVALIDATED 0u
#define ENTROPY_CATALOG_ABI_ONLY 1u
#define ENTROPY_CATALOG_BINDING_VERIFIED 2u
#define ENTROPY_CATALOG_PCM_VERIFIED 3u
#define ENTROPY_ACCESS_NONE 0u
#define ENTROPY_ACCESS_RESERVED_1 1u
#define ENTROPY_ACCESS_RESERVED_2 2u
#define ENTROPY_ACCESS_RESERVED_3 3u
#define ENTROPY_ACCESS_PRIVATE_TAP 4u
#define ENTROPY_ACCESS_RESERVED_5 5u
#define ENTROPY_ACCESS_RESERVED_6 6u
#define ENTROPY_ACCESS_RESERVED_7 7u
#define ENTROPY_ACCESS_MUSIC_TRANSPORT 8u
/* Check an externally supplied mode is < 64 BEFORE shifting. */
#define ENTROPY_CATALOG_CAP_RATE_FOLLOW (UINT64_C(1) << 0)
#define ENTROPY_CATALOG_CAP_BOUNDED_READ (UINT64_C(1) << 1)
#define ENTROPY_CATALOG_CAP_SINGLE_READER (UINT64_C(1) << 2)
#define ENTROPY_CATALOG_TEXT_ENDPOINT_ID 1u
#define ENTROPY_CATALOG_TEXT_FRIENDLY_NAME 2u
#define ENTROPY_CATALOG_TEXT_RENDER_ID 3u
#define ENTROPY_CATALOG_TEXT_RESOURCE_KEY 4u
#define ENTROPY_CATALOG_TEXT_REASON 5u

typedef struct EntropyEndpointCatalogInfoV1 {
    uint32_t struct_size;
    uint32_t reserved_u32;
    uint64_t snapshot;
    uint64_t count;
} EntropyEndpointCatalogInfoV1;

typedef struct EntropyEndpointDeviceInfoV1 {
    uint32_t struct_size;
    uint32_t direction;
    uint32_t presence;
    uint32_t validation;
    uint64_t access_mask;
    uint64_t capabilities;
} EntropyEndpointDeviceInfoV1;

typedef struct EntropyEndpointBindRequestV1 {
    uint32_t struct_size;
    uint32_t access_mode;
    uint64_t revision;
    const char* endpoint_id_utf8;
    uint32_t endpoint_id_bytes;
    uint32_t left_channel;
    uint32_t right_channel;
    uint32_t reserved_u32;
} EntropyEndpointBindRequestV1;

typedef struct EntropyEndpointCatalogApiV1 {
    uint32_t struct_size;
    uint32_t extension_version;
    EntropyResult (ENTROPY_ABI_CALL *begin_catalog)(
        EntropyModuleHandle, EntropyEndpointCatalogInfoV1*);
    EntropyResult (ENTROPY_ABI_CALL *get_device)(
        EntropyModuleHandle, uint64_t snapshot, uint64_t index,
        EntropyEndpointDeviceInfoV1*);
    EntropyResult (ENTROPY_ABI_CALL *get_text)(
        EntropyModuleHandle, uint64_t snapshot, uint64_t index,
        uint32_t field, char* destination, uint32_t capacity,
        uint32_t* required_bytes);
    void (ENTROPY_ABI_CALL *end_catalog)(EntropyModuleHandle, uint64_t snapshot);
    EntropyResult (ENTROPY_ABI_CALL *bind_stopped)(
        EntropyModuleHandle, const EntropyEndpointBindRequestV1*);
    void (ENTROPY_ABI_CALL *cancel_read)(EntropyModuleHandle);
    uint64_t reserved[4];
} EntropyEndpointCatalogApiV1;

/*
 * All fields use natural x64 alignment, not pack(1). Caller zero-initializes
 * structures and sets struct_size. Version must match exactly. Query buffers
 * must fit the known prefix AND the caller's declared size; successful providers
 * write only the known prefix, report its size, and zero reserved fields.
 *
 * begin_catalog freezes metadata only; no bind/connect/read. Snapshot tokens are
 * per instance and expire at end_catalog. The provider owns the snapshot until
 * then; the caller owns text buffers. Index is not a persistent identity.
 *
 * get_text(NULL,0,&needed) returns OK; needed includes the final NUL. Too small
 * returns INVALID_ARGUMENT with needed set and destination UNMODIFIED (no partial
 * string). Invalid snapshot/index returns INVALID_STATE. Optional absent text is
 * an empty NUL string; missing identity is not replaced by a friendly name.
 *
 * bind_stopped copies endpoint_id_bytes (including NUL) before return. No active
 * reader is permitted. Accepted binding is not live format or ACTIVE stream.
 * revision belongs to the Host session, not the driver's stream generation.
 *
 * cancel_read is idempotent and may race ONLY with read; it must release a
 * blocked read without destroying resources in use. Caller joins the reader
 * before disconnect/destroy/unload. A timeout does not permit unloading code.
 * Provider/library must outlive instances, snapshots and in-flight calls.
 * Nothing in this extension permits SRC, gain, downmix or default fallback.
 */
#ifdef __cplusplus
}
#endif
