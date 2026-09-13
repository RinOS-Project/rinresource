/* SPDX-License-Identifier: MIT */
/* Bounded in-process catalog for immutable RinOS resources. */

#ifndef RINRESOURCE_CATALOG_H
#define RINRESOURCE_CATALOG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RIN_RESOURCE_CATALOG_VERSION_1 UINT16_C(1)
#define RIN_RESOURCE_CATALOG_MAX_ENTRIES UINT32_C(256)
#define RIN_RESOURCE_CATALOG_MAX_PATH_BYTES UINT32_C(256)
#define RIN_RESOURCE_CATALOG_MAX_BLOB_BYTES UINT64_C(67108864)

typedef enum RinResourceCatalogStatus {
    RIN_RESOURCE_CATALOG_OK = 0,
    RIN_RESOURCE_CATALOG_INVALID_ARGUMENT = 1,
    RIN_RESOURCE_CATALOG_INVALID_LAYOUT = 2,
    RIN_RESOURCE_CATALOG_UNSUPPORTED_VERSION = 3,
    RIN_RESOURCE_CATALOG_NOT_FOUND = 4,
    RIN_RESOURCE_CATALOG_WRONG_SOURCE = 5,
    RIN_RESOURCE_CATALOG_BUFFER_TOO_SMALL = 6,
    RIN_RESOURCE_CATALOG_IO_ERROR = 7
} RinResourceCatalogStatus;

typedef enum RinResourceCatalogType {
    RIN_RESOURCE_CATALOG_TYPE_ICON = 1,
    RIN_RESOURCE_CATALOG_TYPE_CURSOR = 2,
    RIN_RESOURCE_CATALOG_TYPE_IMAGE = 3,
    RIN_RESOURCE_CATALOG_TYPE_FONT = 4,
    RIN_RESOURCE_CATALOG_TYPE_LOCALIZATION = 5,
    RIN_RESOURCE_CATALOG_TYPE_THEME = 6,
    RIN_RESOURCE_CATALOG_TYPE_SHADER = 7,
    RIN_RESOURCE_CATALOG_TYPE_APPLICATION = 8
} RinResourceCatalogType;

#define RIN_RESOURCE_CATALOG_SOURCE_PATH UINT32_C(0x00000001)
#define RIN_RESOURCE_CATALOG_SOURCE_BLOB UINT32_C(0x00000002)
#define RIN_RESOURCE_CATALOG_FLAG_IMMUTABLE UINT32_C(0x00000004)
#define RIN_RESOURCE_CATALOG_KNOWN_FLAGS \
    (RIN_RESOURCE_CATALOG_SOURCE_PATH | RIN_RESOURCE_CATALOG_SOURCE_BLOB | \
     RIN_RESOURCE_CATALOG_FLAG_IMMUTABLE)

typedef struct RinResourceCatalogEntryV1 {
    uint32_t struct_size;
    uint16_t version;
    uint16_t type;
    uint32_t resource_id;
    uint32_t flags;
    const char* path;
    uint32_t path_size;
    const uint8_t* data;
    uint64_t data_size;
    uint32_t reserved[2];
} RinResourceCatalogEntryV1;

typedef struct RinResourceCatalogV1 {
    uint32_t struct_size;
    uint16_t version;
    uint16_t reserved;
    const RinResourceCatalogEntryV1* entries;
    uint32_t entry_count;
    uint64_t generation;
    uint32_t reserved_tail[2];
} RinResourceCatalogV1;

static inline int rin_resource_catalog_type_valid(uint16_t type)
{
    return type >= RIN_RESOURCE_CATALOG_TYPE_ICON &&
           type <= RIN_RESOURCE_CATALOG_TYPE_APPLICATION;
}

static inline int rin_resource_catalog_path_valid(const char* path,
                                                  uint32_t path_size)
{
    uint32_t index;
    uint32_t component_start = 1u;
    if (path == 0 || path_size < 2u ||
        path_size >= RIN_RESOURCE_CATALOG_MAX_PATH_BYTES ||
        path[0] != '/' || path[path_size] != '\0') return 0;
    for (index = 1u; index < path_size; ++index) {
        unsigned char byte = (unsigned char)path[index];
        if (byte < 0x20u || byte == '\\' || byte == '\0') return 0;
        if (byte == '/') {
            uint32_t component_size = index - component_start;
            if (component_size == 0u ||
                (component_size == 1u && path[component_start] == '.') ||
                (component_size == 2u && path[component_start] == '.' &&
                 path[component_start + 1u] == '.')) return 0;
            component_start = index + 1u;
        }
    }
    {
        uint32_t component_size = path_size - component_start;
        if (component_size == 0u ||
            (component_size == 1u && path[component_start] == '.') ||
            (component_size == 2u && path[component_start] == '.' &&
             path[component_start + 1u] == '.')) return 0;
    }
    return 1;
}

static inline RinResourceCatalogStatus
rin_resource_catalog_entry_validate(const RinResourceCatalogEntryV1* entry)
{
    uint32_t source_flags;
    if (entry == 0) return RIN_RESOURCE_CATALOG_INVALID_ARGUMENT;
    if (entry->struct_size < sizeof(*entry))
        return RIN_RESOURCE_CATALOG_INVALID_LAYOUT;
    if (entry->version != RIN_RESOURCE_CATALOG_VERSION_1)
        return RIN_RESOURCE_CATALOG_UNSUPPORTED_VERSION;
    if (!rin_resource_catalog_type_valid(entry->type) ||
        entry->resource_id == 0u || entry->reserved[0] != 0u ||
        entry->reserved[1] != 0u ||
        (entry->flags & ~RIN_RESOURCE_CATALOG_KNOWN_FLAGS) != 0u ||
        (entry->flags & RIN_RESOURCE_CATALOG_FLAG_IMMUTABLE) == 0u)
        return RIN_RESOURCE_CATALOG_INVALID_LAYOUT;
    source_flags = entry->flags &
        (RIN_RESOURCE_CATALOG_SOURCE_PATH | RIN_RESOURCE_CATALOG_SOURCE_BLOB);
    if (source_flags == RIN_RESOURCE_CATALOG_SOURCE_PATH) {
        if (entry->data != 0 || entry->data_size != 0u ||
            !rin_resource_catalog_path_valid(entry->path, entry->path_size))
            return RIN_RESOURCE_CATALOG_INVALID_LAYOUT;
    } else if (source_flags == RIN_RESOURCE_CATALOG_SOURCE_BLOB) {
        if (entry->path != 0 || entry->path_size != 0u || entry->data == 0 ||
            entry->data_size == 0u ||
            entry->data_size > RIN_RESOURCE_CATALOG_MAX_BLOB_BYTES)
            return RIN_RESOURCE_CATALOG_INVALID_LAYOUT;
    } else {
        return RIN_RESOURCE_CATALOG_INVALID_LAYOUT;
    }
    return RIN_RESOURCE_CATALOG_OK;
}

static inline RinResourceCatalogStatus
rin_resource_catalog_validate(const RinResourceCatalogV1* catalog)
{
    uint32_t index;
    uint32_t prior;
    RinResourceCatalogStatus status;
    if (catalog == 0) return RIN_RESOURCE_CATALOG_INVALID_ARGUMENT;
    if (catalog->struct_size < sizeof(*catalog) || catalog->reserved != 0u ||
        catalog->reserved_tail[0] != 0u || catalog->reserved_tail[1] != 0u ||
        catalog->generation == 0u ||
        catalog->entry_count > RIN_RESOURCE_CATALOG_MAX_ENTRIES ||
        (catalog->entry_count != 0u && catalog->entries == 0))
        return RIN_RESOURCE_CATALOG_INVALID_LAYOUT;
    if (catalog->version != RIN_RESOURCE_CATALOG_VERSION_1)
        return RIN_RESOURCE_CATALOG_UNSUPPORTED_VERSION;
    for (index = 0u; index < catalog->entry_count; ++index) {
        status = rin_resource_catalog_entry_validate(&catalog->entries[index]);
        if (status != RIN_RESOURCE_CATALOG_OK) return status;
        for (prior = 0u; prior < index; ++prior) {
            if (catalog->entries[prior].resource_id ==
                catalog->entries[index].resource_id)
                return RIN_RESOURCE_CATALOG_INVALID_LAYOUT;
            if ((catalog->entries[prior].flags &
                     RIN_RESOURCE_CATALOG_SOURCE_PATH) != 0u &&
                (catalog->entries[index].flags &
                     RIN_RESOURCE_CATALOG_SOURCE_PATH) != 0u &&
                catalog->entries[prior].path_size ==
                    catalog->entries[index].path_size) {
                uint32_t path_index;
                int same_path = 1;
                for (path_index = 0u;
                     path_index < catalog->entries[index].path_size;
                     ++path_index) {
                    if (catalog->entries[prior].path[path_index] !=
                        catalog->entries[index].path[path_index]) {
                        same_path = 0;
                        break;
                    }
                }
                if (same_path)
                    return RIN_RESOURCE_CATALOG_INVALID_LAYOUT;
            }
        }
    }
    return RIN_RESOURCE_CATALOG_OK;
}

static inline RinResourceCatalogStatus
rin_resource_catalog_find(const RinResourceCatalogV1* catalog, uint16_t type,
                          uint32_t resource_id,
                          const RinResourceCatalogEntryV1** entry_out)
{
    uint32_t index;
    RinResourceCatalogStatus status;
    if (entry_out == 0 || !rin_resource_catalog_type_valid(type) ||
        resource_id == 0u) return RIN_RESOURCE_CATALOG_INVALID_ARGUMENT;
    status = rin_resource_catalog_validate(catalog);
    if (status != RIN_RESOURCE_CATALOG_OK) return status;
    for (index = 0u; index < catalog->entry_count; ++index) {
        const RinResourceCatalogEntryV1* entry = &catalog->entries[index];
        if (entry->type == type && entry->resource_id == resource_id) {
            *entry_out = entry;
            return RIN_RESOURCE_CATALOG_OK;
        }
    }
    *entry_out = 0;
    return RIN_RESOURCE_CATALOG_NOT_FOUND;
}

static inline RinResourceCatalogStatus
rin_resource_catalog_resolve_path(const RinResourceCatalogV1* catalog,
                                  uint16_t type, uint32_t resource_id,
                                  const char** path_out,
                                  uint32_t* path_size_out)
{
    const RinResourceCatalogEntryV1* entry = 0;
    RinResourceCatalogStatus status;
    if (path_out == 0) return RIN_RESOURCE_CATALOG_INVALID_ARGUMENT;
    status = rin_resource_catalog_find(catalog, type, resource_id, &entry);
    if (status != RIN_RESOURCE_CATALOG_OK) return status;
    if ((entry->flags & RIN_RESOURCE_CATALOG_SOURCE_PATH) == 0u)
        return RIN_RESOURCE_CATALOG_WRONG_SOURCE;
    *path_out = entry->path;
    if (path_size_out != 0) *path_size_out = entry->path_size;
    return RIN_RESOURCE_CATALOG_OK;
}

static inline RinResourceCatalogStatus
rin_resource_catalog_resolve_blob(const RinResourceCatalogV1* catalog,
                                  uint16_t type, uint32_t resource_id,
                                  const uint8_t** data_out,
                                  uint64_t* data_size_out)
{
    const RinResourceCatalogEntryV1* entry = 0;
    RinResourceCatalogStatus status;
    if (data_out == 0 || data_size_out == 0)
        return RIN_RESOURCE_CATALOG_INVALID_ARGUMENT;
    status = rin_resource_catalog_find(catalog, type, resource_id, &entry);
    if (status != RIN_RESOURCE_CATALOG_OK) return status;
    if ((entry->flags & RIN_RESOURCE_CATALOG_SOURCE_BLOB) == 0u)
        return RIN_RESOURCE_CATALOG_WRONG_SOURCE;
    *data_out = entry->data;
    *data_size_out = entry->data_size;
    return RIN_RESOURCE_CATALOG_OK;
}

#ifdef __cplusplus
}
#endif

#endif /* RINRESOURCE_CATALOG_H */
