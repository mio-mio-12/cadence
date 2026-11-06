#pragma once
#include <cstdint>
namespace scene::imported {
// Reconstruction uses the recording's import convention, never a global
// preference. Thread-local scope also isolates background replay preparation.
inline thread_local std::uint32_t activeImportRevision=1;
struct ScopedImportRevision {
    std::uint32_t previous;
    explicit ScopedImportRevision(std::uint32_t value):previous(activeImportRevision){activeImportRevision=value;}
    ~ScopedImportRevision(){activeImportRevision=previous;}
    ScopedImportRevision(const ScopedImportRevision&)=delete;
    ScopedImportRevision& operator=(const ScopedImportRevision&)=delete;
};
}
