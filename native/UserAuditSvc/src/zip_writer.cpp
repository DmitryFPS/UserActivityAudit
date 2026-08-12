#include "useraudit/zip_writer.hpp"

#include <fstream>
#include <vector>

namespace useraudit {

namespace {

#pragma pack(push, 1)
struct LocalFileHeader {
    std::uint32_t signature = 0x04034b50;
    std::uint16_t version = 20;
    std::uint16_t flags = 0;
    std::uint16_t method = 0;
    std::uint16_t mod_time = 0;
    std::uint16_t mod_date = 0;
    std::uint32_t crc32 = 0;
    std::uint32_t compressed_size = 0;
    std::uint32_t uncompressed_size = 0;
    std::uint16_t name_length = 0;
    std::uint16_t extra_length = 0;
};

struct CentralDirectoryHeader {
    std::uint32_t signature = 0x02014b50;
    std::uint16_t version_made = 20;
    std::uint16_t version_needed = 20;
    std::uint16_t flags = 0;
    std::uint16_t method = 0;
    std::uint16_t mod_time = 0;
    std::uint16_t mod_date = 0;
    std::uint32_t crc32 = 0;
    std::uint32_t compressed_size = 0;
    std::uint32_t uncompressed_size = 0;
    std::uint16_t name_length = 0;
    std::uint16_t extra_length = 0;
    std::uint16_t comment_length = 0;
    std::uint16_t disk_start = 0;
    std::uint16_t internal_attrs = 0;
    std::uint32_t external_attrs = 0;
    std::uint32_t local_header_offset = 0;
};

struct EndOfCentralDirectory {
    std::uint32_t signature = 0x06054b50;
    std::uint16_t disk_number = 0;
    std::uint16_t central_dir_disk = 0;
    std::uint16_t entries_on_disk = 0;
    std::uint16_t total_entries = 0;
    std::uint32_t central_dir_size = 0;
    std::uint32_t central_dir_offset = 0;
    std::uint16_t comment_length = 0;
};
#pragma pack(pop)

std::uint32_t crc32_update(std::uint32_t crc, const std::uint8_t* data, std::size_t size) {
    crc = ~crc;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask = -(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

bool read_file_bytes(const std::filesystem::path& path, std::vector<std::uint8_t>& out,
                     std::string& error) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        error = "cannot open file for zip: " + path.string();
        return false;
    }
    input.seekg(0, std::ios::end);
    const auto size = input.tellg();
    if (size < 0) {
        error = "cannot stat file for zip: " + path.string();
        return false;
    }
    out.resize(static_cast<std::size_t>(size));
    input.seekg(0, std::ios::beg);
    input.read(reinterpret_cast<char*>(out.data()), size);
    if (!input) {
        error = "cannot read file for zip: " + path.string();
        return false;
    }
    return true;
}

}  // namespace

bool create_zip_store(const std::filesystem::path& zip_path, const std::vector<ZipEntry>& entries,
                      std::string& error) {
    if (entries.empty()) {
        error = "zip entries list is empty";
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(zip_path.parent_path(), ec);

    std::ofstream output(zip_path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        error = "cannot create zip: " + zip_path.string();
        return false;
    }

    struct CentralRecord {
        CentralDirectoryHeader header{};
        std::string name;
    };

    std::vector<CentralRecord> central_records;
    central_records.reserve(entries.size());

    for (const auto& entry : entries) {
        if (entry.archive_name.empty()) {
            error = "zip entry has empty archive name";
            return false;
        }

        std::vector<std::uint8_t> bytes;
        if (!read_file_bytes(entry.source_path, bytes, error)) {
            return false;
        }

        const std::uint32_t offset = static_cast<std::uint32_t>(output.tellp());
        const std::uint32_t crc = crc32_update(0, bytes.data(), bytes.size());
        const auto size = static_cast<std::uint32_t>(bytes.size());

        LocalFileHeader local{};
        local.crc32 = crc;
        local.compressed_size = size;
        local.uncompressed_size = size;
        local.name_length = static_cast<std::uint16_t>(entry.archive_name.size());

        output.write(reinterpret_cast<const char*>(&local), sizeof(local));
        output.write(entry.archive_name.data(), static_cast<std::streamsize>(entry.archive_name.size()));
        output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));

        CentralRecord record{};
        record.header.crc32 = crc;
        record.header.compressed_size = size;
        record.header.uncompressed_size = size;
        record.header.name_length = local.name_length;
        record.header.local_header_offset = offset;
        record.name = entry.archive_name;
        central_records.push_back(std::move(record));
    }

    const std::uint32_t central_offset = static_cast<std::uint32_t>(output.tellp());
    std::uint32_t central_size = 0;

    for (const auto& record : central_records) {
        output.write(reinterpret_cast<const char*>(&record.header), sizeof(record.header));
        output.write(record.name.data(), static_cast<std::streamsize>(record.name.size()));
        central_size += static_cast<std::uint32_t>(sizeof(record.header) + record.name.size());
    }

    EndOfCentralDirectory eocd{};
    eocd.entries_on_disk = static_cast<std::uint16_t>(central_records.size());
    eocd.total_entries = eocd.entries_on_disk;
    eocd.central_dir_size = central_size;
    eocd.central_dir_offset = central_offset;
    output.write(reinterpret_cast<const char*>(&eocd), sizeof(eocd));

    if (!output.good()) {
        error = "failed writing zip: " + zip_path.string();
        return false;
    }

    error.clear();
    return true;
}

}  // namespace useraudit
