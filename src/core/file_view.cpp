#include <amethyst/core/file_view.hpp>

#if defined(_WIN32)
    #include <Windows.h>
#else
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <unistd.h>
    #include <fcntl.h>
#endif

namespace am {
    namespace fs = std::filesystem;

    CFileView::CFileView() noexcept = default;

    CFileView::~CFileView() noexcept {
        AM_PROFILE_SCOPED();
        if (_data) {
#if defined(_WIN32)
            AM_ASSERT(UnmapViewOfFile(_data), "internal filesystem error");
            AM_ASSERT(CloseHandle(static_cast<HANDLE>(_mapping)), "internal filesystem error");
            AM_ASSERT(CloseHandle(static_cast<HANDLE>(_handle)), "internal filesystem error");
#else
            AM_ASSERT(!munmap(_mapping, _size), "internal filesystem error");
            close(static_cast<int32>(reinterpret_cast<int64>(_handle)));
#endif
        }
    }

    AM_NODISCARD CExpected<CRcPtr<CFileView>> CFileView::make(const fs::path& path) noexcept {
        AM_PROFILE_SCOPED();
        auto result = std::unique_ptr<Self>(new Self());
        AM_UNLIKELY_IF(!fs::exists(path)) {
            return EErrorType::FileNotFound;
        }

#if defined(_WIN32)
        auto handle = CreateFileW(path.native().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        auto mapping = CreateFileMappingW(handle, nullptr, PAGE_READONLY, 0, 0, nullptr);
        auto size = GetFileSize(handle, nullptr);
        auto data = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, size);
        AM_UNLIKELY_IF(!data) {
            return EErrorType::InternalError;
        }
#else
        auto handle = open(path.native().c_str(), O_RDONLY);
        struct stat64 stat = {};
        fstat64(handle, &stat);
        auto mapping = mmap64(nullptr, stat.st_size, PROT_READ, MAP_SHARED, handle, 0);
        auto size = stat.st_size;
        auto data = mapping;
#endif
        result->_handle = reinterpret_cast<void*>(handle);
        result->_mapping = static_cast<void*>(mapping);
        result->_size = size;
        result->_data = static_cast<const void*>(data);
        return CRcPtr<Self>::make(result.release());
    }

    AM_NODISCARD const void* CFileView::data() const noexcept {
        AM_PROFILE_SCOPED();
        return _data;
    }

    AM_NODISCARD uint64 CFileView::size() const noexcept {
        AM_PROFILE_SCOPED();
        return _size;
    }
} // namespace am
