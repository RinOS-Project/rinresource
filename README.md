# RinResource

RinResource provides a small bounded catalog and loader interface for locating packaged RinOS resources.

## Public API contract

| Requirement | Contract |
| --- | --- |
| Purpose | RinResource provides a small bounded catalog and loader interface for locating packaged RinOS resources. |
| Supported API | Public C interfaces are `rinresource/catalog.h` and `rinresource/loader.h`. A caller may provide catalog entries and a loader callback for retrieving their bytes. |
| Unsupported API | The catalog does not authorize access, canonicalize paths, validate filesystem containment, or implement a filesystem backend. Those decisions belong to the callback owner. |
| ownership | The catalog and entry strings are supplied by the caller and must outlive uses that reference them. Loaded blob ownership and release follow the loader callback contract. |
| thread-safety | Read-only use of an immutable catalog can be shared if the supplied loader is thread-safe. Catalog mutation, callback state, and blob lifetime synchronization are the caller's responsibility. |
| limits | The inline catalog accepts at most 256 entries; paths are limited to 256 bytes and each resource blob to 64 MiB. |
| errors | Missing entries, invalid catalog data, callback failures, and over-limit resources are reported as errors. Callers must check callback results and release successful blobs as documented. |
| ABI stability | The declarations in `include/rinresource` are the public C ABI. No cross-version ABI guarantee is published; rebuild consumers with library updates. |
| security | Treat catalog paths and resource contents as untrusted. The loader callback must enforce authorization and safe path handling; catalog membership alone is not authority. |
| build | Use the public headers through the RinOS build. No standalone build/install process is documented. |
| test | No standalone test command is documented. Validate catalog and loader behavior in the consuming RinOS resource pipeline. |
