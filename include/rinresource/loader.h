/* SPDX-License-Identifier: MIT */
/* Bounded, caller-owned resource loading on top of the immutable catalog. */

#ifndef RINRESOURCE_LOADER_H
#define RINRESOURCE_LOADER_H

#include "catalog.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The callback owns the filesystem/service boundary for path-backed entries.
 * It must not retain path or output after returning.  A short read is valid;
 * a failed callback must return a non-OK status and the loader will keep
 * output_size at zero. */
typedef RinResourceCatalogStatus (*RinResourceCatalogReadPathFunction)(
    void* context, const char* path, uint32_t path_size, uint8_t* output,
    uint64_t output_capacity, uint64_t* output_size);

/* Resolve one catalog entry and copy/read it into a caller-owned buffer.
 * No allocation or filesystem operation is performed here.  The operation is
 * failure-atomic with respect to output_size; callers must treat the output
 * bytes as unpublished unless this returns OK. */
static inline RinResourceCatalogStatus rin_resource_catalog_load(
    const RinResourceCatalogV1* catalog, uint16_t type, uint32_t resource_id,
    RinResourceCatalogReadPathFunction read_path, void* context,
    uint8_t* output, uint64_t output_capacity, uint64_t* output_size)
{
    const RinResourceCatalogEntryV1* entry = 0;
    RinResourceCatalogStatus status;
    uint64_t index;

    if (output_size == 0 ||
        output_capacity > (uint64_t)SIZE_MAX ||
        (output_capacity != 0u && output == 0)) {
        if (output_size != 0) *output_size = 0u;
        return RIN_RESOURCE_CATALOG_INVALID_ARGUMENT;
    }
    *output_size = 0u;
    status = rin_resource_catalog_find(catalog, type, resource_id, &entry);
    if (status != RIN_RESOURCE_CATALOG_OK) return status;
    if ((entry->flags & RIN_RESOURCE_CATALOG_SOURCE_BLOB) != 0u) {
        if (entry->data_size > output_capacity)
            return RIN_RESOURCE_CATALOG_BUFFER_TOO_SMALL;
        if (entry->data_size != 0u && output == 0)
            return RIN_RESOURCE_CATALOG_INVALID_ARGUMENT;
        for (index = 0u; index < entry->data_size; ++index)
            output[index] = entry->data[index];
        *output_size = entry->data_size;
        return RIN_RESOURCE_CATALOG_OK;
    }
    if ((entry->flags & RIN_RESOURCE_CATALOG_SOURCE_PATH) == 0u)
        return RIN_RESOURCE_CATALOG_WRONG_SOURCE;
    if (read_path == 0)
        return RIN_RESOURCE_CATALOG_INVALID_ARGUMENT;
    status = read_path(context, entry->path, entry->path_size, output,
                       output_capacity, output_size);
    if (status != RIN_RESOURCE_CATALOG_OK || *output_size > output_capacity) {
        *output_size = 0u;
        return status == RIN_RESOURCE_CATALOG_OK
            ? RIN_RESOURCE_CATALOG_BUFFER_TOO_SMALL : status;
    }
    return RIN_RESOURCE_CATALOG_OK;
}

#ifdef __cplusplus
}
#endif

#endif /* RINRESOURCE_LOADER_H */
