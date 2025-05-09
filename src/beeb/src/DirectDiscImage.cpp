#include <shared/system.h>
#include <string>
#include <beeb/DirectDiscImage.h>
#include <limits.h>
#include <shared/file_io.h>
#include <shared/path.h>
#include <shared/log.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::string DirectDiscImage::LOAD_METHOD_DIRECT = "direct";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

DirectDiscImage::~DirectDiscImage() {
    this->Close();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<DirectDiscImage> DirectDiscImage::CreateForFile(std::string path,
                                                                const LogSet &logs) {
    uint64_t size;
    bool can_write;
    if (!PathIsFileOnDisk(path, &size, &can_write)) {
        logs.e.f("Couldn't get details for file: %s\n", path.c_str());
        return nullptr;
    }

    DiscGeometry geometry;
    if (!FindDiscGeometryFromFileDetails(&geometry, path.c_str(), size, &logs)) {
        return nullptr;
    }

    return std::shared_ptr<DirectDiscImage>(new DirectDiscImage(std::move(path), geometry, !can_write));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool DirectDiscImage::CanClone() const {
    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool DirectDiscImage::CanSave() const {
    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<DiscImage> DirectDiscImage::Clone() const {
    return nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string DirectDiscImage::GetHash() const {
    return std::string();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string DirectDiscImage::GetName() const {
    return m_path;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string DirectDiscImage::GetLoadMethod() const {
    return LOAD_METHOD_DIRECT;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// TODO - basically the same as MemoryDiscImage.
std::string DirectDiscImage::GetDescription() const {
    char description[100];
    snprintf(description, sizeof description, "%s %s %zuT x %zuS",
             m_geometry.double_sided ? "DS" : "SS",
             m_geometry.double_density ? "DD" : "SD",
             m_geometry.num_tracks,
             m_geometry.sectors_per_track);

    return description;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// TODO - basically the same as MemoryDiscImage.
std::vector<FileDialogFilter> DirectDiscImage::GetFileDialogFilters() const {
    if (const char *ext = GetExtensionFromDiscGeometry(m_geometry)) {
        return {{"BBC disc image", {ext}}};
    }

    return {};
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool DirectDiscImage::SaveToFile(const std::string &file_name, const LogSet &logs) const {
    this->Close();

    std::vector<uint8_t> data;
    if (!LoadFile(&data, m_path, &logs)) {
        return false;
    }

    if (!SaveFile(data, file_name, &logs)) {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool DirectDiscImage::Read(uint8_t *value,
                           uint8_t side,
                           uint8_t track,
                           uint8_t sector,
                           size_t offset) const {
    if (!this->fopenAndSeek(false, side, track, sector, offset)) {
        return false;
    }

    int c = fgetc(m_fp);
    if (c == EOF) {
        // This case is OK - the disc image is logically its full size, even
        // when truncated.
        //
        // Returns 0s rather than a fill byte - writing past end supposedly
        // fills the gap with zeroes (https://en.cppreference.com/w/c/io/fseek)
        *value = 0;
    } else {
        *value = (uint8_t)c;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool DirectDiscImage::Write(uint8_t side,
                            uint8_t track,
                            uint8_t sector,
                            size_t offset,
                            uint8_t value) {
    if (!this->fopenAndSeek(true, side, track, sector, offset)) {
        return false;
    }

    bool good = false;
    if (fputc(value, m_fp) != EOF) {
        good = true;
    }

    return good;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void DirectDiscImage::Flush() {
    this->Close();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// TDOO - same as MemoryDiscImage.
bool DirectDiscImage::GetDiscSectorSize(size_t *size,
                                        uint8_t side,
                                        uint8_t track,
                                        uint8_t sector,
                                        bool double_density) const {
    if (double_density != m_geometry.double_density) {
        return false;
    }

    size_t index;
    if (!m_geometry.GetIndex(&index, side, track, sector, 0)) {
        return false;
    }

    *size = m_geometry.bytes_per_sector;
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool DirectDiscImage::IsWriteProtected() const {
    return m_write_protected;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

DirectDiscImage::DirectDiscImage(std::string path,
                                 const DiscGeometry &geometry,
                                 bool write_protected)
    : m_path(std::move(path))
    , m_geometry(geometry)
    , m_write_protected(write_protected) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool DirectDiscImage::fopenAndSeek(bool write,
                                   uint8_t side,
                                   uint8_t track,
                                   uint8_t sector,
                                   size_t offset) const {
    size_t index;
    if (!m_geometry.GetIndex(&index, side, track, sector, offset)) {
        return false;
    }

    if (index > LONG_MAX) {
        return false;
    }

    if (m_fp && write && !m_fp_write) {
        this->Close();
    }

    if (!m_fp) {
        const char *mode;
        if (write) {
            mode = "r+b";
        } else {
            mode = "rb";
        }

        m_fp = fopenUTF8(m_path.c_str(), mode);
        if (!m_fp) {
            return false;
        }

        m_fp_write = write;
    }

    if (fseek(m_fp, (long)index, SEEK_SET) != 0) {
        this->Close();
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void DirectDiscImage::Close() const {
    if (m_fp) {
        fclose(m_fp);
        m_fp = nullptr;
    }
}
