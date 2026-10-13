#pragma once

#include <entropy/module_abi.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ENTROPY_DEVICE_MANAGER_EXTENSION_ID "audio.entropy.device-manager.v1"
#define ENTROPY_DEVICE_MANAGER_EXTENSION_VERSION 0x00010000u
#define ENTROPY_DEVICE_BINDING_SCHEMA_VERSION 0x00010000u
#define ENTROPY_DEVICE_MAX_EVIDENCE 20u
#define ENTROPY_DEVICE_MAX_FORMATS 32u
#define ENTROPY_DEVICE_EVIDENCE_VALUE_BYTES 768u
#define ENTROPY_DEVICE_ENDPOINT_ID_BYTES 768u
#define ENTROPY_DEVICE_DISPLAY_NAME_BYTES 256u
#define ENTROPY_DEVICE_LOGICAL_ID_BYTES 64u
#define ENTROPY_DEVICE_FINGERPRINT_BYTES 64u
#define ENTROPY_DEVICE_MAX_MATCHES 8u

typedef enum EntropyDeviceBindingState {
    ENTROPY_DEVICE_UNVERIFIED = 0,
    ENTROPY_DEVICE_VERIFIED = 1,
    ENTROPY_DEVICE_OFFLINE = 2,
    ENTROPY_DEVICE_AMBIGUOUS = 3,
    ENTROPY_DEVICE_CHANGED = 4
} EntropyDeviceBindingState;

typedef enum EntropyDeviceEvidenceKind {
    ENTROPY_DEVICE_EVIDENCE_UNKNOWN = 0,
    ENTROPY_DEVICE_EVIDENCE_ENDPOINT_ID = 1,
    ENTROPY_DEVICE_EVIDENCE_INTERFACE_PATH = 2,
    ENTROPY_DEVICE_EVIDENCE_CONTAINER_ID = 3,
    ENTROPY_DEVICE_EVIDENCE_INSTANCE_ID = 4,
    ENTROPY_DEVICE_EVIDENCE_VID_PID = 5,
    ENTROPY_DEVICE_EVIDENCE_SERIAL_NUMBER = 6,
    ENTROPY_DEVICE_EVIDENCE_HARDWARE_IDS = 7,
    ENTROPY_DEVICE_EVIDENCE_FRIENDLY_NAME = 8,
    ENTROPY_DEVICE_EVIDENCE_CHANNEL_CAPABILITY = 9,
    ENTROPY_DEVICE_EVIDENCE_EXACT_FORMATS = 10,
    ENTROPY_DEVICE_EVIDENCE_DRIVER_IDENTITY = 11
} EntropyDeviceEvidenceKind;

/* Increasing values mean increasing expected persistence.  They are metadata,
   never a license for the host to guess a logical DAC identity. */
typedef enum EntropyDeviceEvidenceStability {
    ENTROPY_DEVICE_EVIDENCE_DESCRIPTIVE = 0,
    ENTROPY_DEVICE_EVIDENCE_VOLATILE = 1,
    ENTROPY_DEVICE_EVIDENCE_PORT_BOUND = 2,
    ENTROPY_DEVICE_EVIDENCE_INSTALLATION_STABLE = 3,
    ENTROPY_DEVICE_EVIDENCE_DEVICE_STABLE = 4,
    ENTROPY_DEVICE_EVIDENCE_HARDWARE_IMMUTABLE = 5
} EntropyDeviceEvidenceStability;

typedef enum EntropyDeviceEvidenceFlags {
    ENTROPY_DEVICE_EVIDENCE_FLAG_NONE = 0,
    ENTROPY_DEVICE_EVIDENCE_FLAG_IDENTITY_ANCHOR = 1u << 0,
    ENTROPY_DEVICE_EVIDENCE_FLAG_OPEN_ADDRESS = 1u << 1,
    ENTROPY_DEVICE_EVIDENCE_FLAG_MATERIAL = 1u << 2,
    ENTROPY_DEVICE_EVIDENCE_FLAG_DIAGNOSTIC = 1u << 3
} EntropyDeviceEvidenceFlags;

typedef enum EntropyDevicePcmEncoding {
    ENTROPY_DEVICE_PCM_INTEGER = 1,
    ENTROPY_DEVICE_PCM_FLOAT = 2
} EntropyDevicePcmEncoding;

typedef struct EntropyDeviceEvidence {
    uint32_t struct_size;
    uint32_t kind;
    uint32_t stability;
    uint32_t flags;
    char value_utf8[ENTROPY_DEVICE_EVIDENCE_VALUE_BYTES];
    uint64_t reserved[2];
} EntropyDeviceEvidence;

typedef struct EntropyDeviceExactFormat {
    uint32_t struct_size;
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t container_bits;
    uint32_t valid_bits;
    uint32_t encoding;
    uint32_t reserved_u32[2];
} EntropyDeviceExactFormat;

typedef struct EntropyDeviceInfo {
    uint32_t struct_size;
    uint32_t active;
    uint32_t is_default;
    uint32_t evidence_count;
    uint32_t format_count;
    uint32_t maximum_channels;
    uint32_t reserved_u32[2];
    char endpoint_id_utf8[ENTROPY_DEVICE_ENDPOINT_ID_BYTES];
    char display_name_utf8[ENTROPY_DEVICE_DISPLAY_NAME_BYTES];
    char composite_fingerprint[ENTROPY_DEVICE_FINGERPRINT_BYTES];
    EntropyDeviceEvidence evidence[ENTROPY_DEVICE_MAX_EVIDENCE];
    EntropyDeviceExactFormat formats[ENTROPY_DEVICE_MAX_FORMATS];
    uint64_t reserved[4];
} EntropyDeviceInfo;

/* This record is created only after an explicit Host/User approval event.
   The DLL never persists it and never arms an output. */
typedef struct EntropyDeviceBinding {
    uint32_t struct_size;
    uint32_t schema_version;
    uint32_t evidence_count;
    uint32_t format_count;
    uint64_t approval_revision;
    char logical_device_id[ENTROPY_DEVICE_LOGICAL_ID_BYTES];
    char approved_endpoint_id_utf8[ENTROPY_DEVICE_ENDPOINT_ID_BYTES];
    char approved_display_name_utf8[ENTROPY_DEVICE_DISPLAY_NAME_BYTES];
    char composite_fingerprint[ENTROPY_DEVICE_FINGERPRINT_BYTES];
    EntropyDeviceEvidence evidence[ENTROPY_DEVICE_MAX_EVIDENCE];
    EntropyDeviceExactFormat formats[ENTROPY_DEVICE_MAX_FORMATS];
    uint64_t reserved[4];
} EntropyDeviceBinding;

typedef struct EntropyDeviceBindingResolution {
    uint32_t struct_size;
    uint32_t state;
    uint32_t candidate_count;
    uint32_t matched_device_index;
    uint32_t exact_endpoint_matches;
    uint32_t strong_identity_matches;
    uint32_t material_mismatches;
    uint32_t matching_device_indices[ENTROPY_DEVICE_MAX_MATCHES];
    char resolved_endpoint_id_utf8[ENTROPY_DEVICE_ENDPOINT_ID_BYTES];
    char detail_utf8[256];
    uint64_t reserved[4];
} EntropyDeviceBindingResolution;

typedef struct EntropyDeviceManagerApiV1 {
    uint32_t struct_size;
    uint32_t extension_version;
    EntropyResult (ENTROPY_ABI_CALL *refresh_devices)(EntropyModuleHandle);
    EntropyResult (ENTROPY_ABI_CALL *device_count)(EntropyModuleHandle, uint32_t*);
    EntropyResult (ENTROPY_ABI_CALL *get_device)(EntropyModuleHandle, uint32_t, EntropyDeviceInfo*);
    EntropyResult (ENTROPY_ABI_CALL *capture_approved_binding)(
        EntropyModuleHandle, uint32_t, const char*, uint64_t, EntropyDeviceBinding*);
    EntropyResult (ENTROPY_ABI_CALL *resolve_binding)(
        EntropyModuleHandle, const EntropyDeviceBinding*, EntropyDeviceBindingResolution*);
    uint64_t reserved[8];
} EntropyDeviceManagerApiV1;

#ifdef __cplusplus
}
#endif
