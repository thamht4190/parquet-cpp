// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#ifndef PARQUET_COLUMN_PROPERTIES_H
#define PARQUET_COLUMN_PROPERTIES_H

#include <memory>
#include <string>
#include <unordered_map>

#include "parquet/encryption.h"
#include "parquet/exception.h"
#include "parquet/parquet_version.h"
#include "parquet/schema.h"
#include "parquet/types.h"
#include "parquet/util/logging.h"
#include "parquet/util/memory.h"
#include "parquet/util/visibility.h"

namespace parquet {

struct ParquetVersion {
  enum type { PARQUET_1_0, PARQUET_2_0 };
};

static int64_t DEFAULT_BUFFER_SIZE = 0;
static bool DEFAULT_USE_BUFFERED_STREAM = false;

class PARQUET_EXPORT ColumnEncryptionProperties {
 public:
  ColumnEncryptionProperties() = default;
  ColumnEncryptionProperties(bool encrypt, std::string path)
      : encrypt_(encrypt), path_(path), encrypted_with_footer_key_(encrypt) {}

  bool encrypted() const { return encrypt_; }
  bool encrypted_with_footer_key() const { return encrypted_with_footer_key_; }
  const std::string& key() const { return key_; }
  const std::string& key_metadata() const { return key_metadata_; }

  void SetEncryptionKey(const std::string& key, uint32_t key_id = 0) {
    std::string key_metadata =
        key_id == 0 ? "" : std::string(reinterpret_cast<char*>(&key_id), 4);
    SetEncryptionKey(key, key_metadata);
  }

  void SetEncryptionKey(const std::string& key, const std::string& key_metadata) {
    if (!encrypt_) throw ParquetException("Setting key on unencrypted column: " + path_);
    if (key.empty()) throw ParquetException("Null key for " + path_);

    encrypted_with_footer_key_ = false;
    key_ = key;
    key_metadata_ = key_metadata;
  }

  const std::string& path() const { return path_; }

 private:
  bool encrypt_;
  std::string path_;
  bool encrypted_with_footer_key_;
  std::string key_;
  std::string key_metadata_;
};

class PARQUET_EXPORT FileDecryptionProperties {
 public:
  FileDecryptionProperties(const std::string& footer_key) : footer_key_(footer_key) {
    DCHECK(footer_key_.length() == 16 || footer_key_.length() == 24 ||
           footer_key_.length() == 32);
  }

  FileDecryptionProperties(const std::shared_ptr<DecryptionKeyRetriever>& key_retriever)
      : key_retriever_(key_retriever) {}

  void SetAad(const std::string& aad) { aad_ = aad; }

  void SetColumnKey(const std::string& name, const std::string& key) {
    SetColumnKey(std::vector<std::string>({name}), key);
  }

  void SetColumnKey(const std::vector<std::string>& paths, const std::string& key) {
    DCHECK(key.length() == 16 || key.length() == 24 || key.length() == 32);

    schema::ColumnPath columnPath(paths);

    column_keys_[columnPath.ToDotString()] = key;
  }

  const std::string& GetColumnKey(const std::shared_ptr<schema::ColumnPath>& columnPath,
                                  const std::string& key_metadata = "") {
    if (key_metadata.empty()) {
      return column_keys_.at(columnPath->ToDotString());
    }
    if (key_retriever_ == nullptr) {
      throw ParquetException("no key retriever is provided for column key metadata");
    }
    return key_retriever_->GetKey(key_metadata);
  }

  const std::string& GetFooterKey(const std::string& footer_key_metadata = "") {
    if (footer_key_metadata.empty()) {
      return footer_key_;
    }
    if (key_retriever_ == nullptr) {
      throw ParquetException("no key retriever is provided for footer key metadata");
    }
    return key_retriever_->GetKey(footer_key_metadata);
  }
  const std::string& GetAad() { return aad_; }

 private:
  std::string footer_key_;
  std::string aad_;

  std::map<std::string, std::string> column_keys_;

  std::shared_ptr<DecryptionKeyRetriever> key_retriever_;
};

class PARQUET_EXPORT ReaderProperties {
 public:
  explicit ReaderProperties(::arrow::MemoryPool* pool = ::arrow::default_memory_pool())
      : pool_(pool) {
    buffered_stream_enabled_ = DEFAULT_USE_BUFFERED_STREAM;
    buffer_size_ = DEFAULT_BUFFER_SIZE;
  }

  ::arrow::MemoryPool* memory_pool() const { return pool_; }

  std::unique_ptr<InputStream> GetStream(RandomAccessSource* source, int64_t start,
                                         int64_t num_bytes) {
    std::unique_ptr<InputStream> stream;
    if (buffered_stream_enabled_) {
      stream.reset(
          new BufferedInputStream(pool_, buffer_size_, source, start, num_bytes));
    } else {
      stream.reset(new InMemoryInputStream(source, start, num_bytes));
    }
    return stream;
  }

  bool is_buffered_stream_enabled() const { return buffered_stream_enabled_; }

  void enable_buffered_stream() { buffered_stream_enabled_ = true; }

  void disable_buffered_stream() { buffered_stream_enabled_ = false; }

  void set_buffer_size(int64_t buf_size) { buffer_size_ = buf_size; }

  int64_t buffer_size() const { return buffer_size_; }

  void SetFileDecryption(const std::shared_ptr<FileDecryptionProperties>& decryption) {
    file_decryption_ = decryption;
  }

  FileDecryptionProperties* file_decryption() { return file_decryption_.get(); }

 private:
  ::arrow::MemoryPool* pool_;
  int64_t buffer_size_;
  bool buffered_stream_enabled_;
  std::shared_ptr<FileDecryptionProperties> file_decryption_;
};

ReaderProperties PARQUET_EXPORT default_reader_properties();

static constexpr int64_t DEFAULT_PAGE_SIZE = 1024 * 1024;
static constexpr bool DEFAULT_IS_DICTIONARY_ENABLED = true;
static constexpr int64_t DEFAULT_DICTIONARY_PAGE_SIZE_LIMIT = DEFAULT_PAGE_SIZE;
static constexpr int64_t DEFAULT_WRITE_BATCH_SIZE = 1024;
static constexpr int64_t DEFAULT_MAX_ROW_GROUP_LENGTH = 64 * 1024 * 1024;
static constexpr bool DEFAULT_ARE_STATISTICS_ENABLED = true;
static constexpr int64_t DEFAULT_MAX_STATISTICS_SIZE = 4096;
static constexpr Encoding::type DEFAULT_ENCODING = Encoding::PLAIN;
static constexpr ParquetVersion::type DEFAULT_WRITER_VERSION =
    ParquetVersion::PARQUET_1_0;
static const char DEFAULT_CREATED_BY[] = CREATED_BY_VERSION;
static constexpr Compression::type DEFAULT_COMPRESSION_TYPE = Compression::UNCOMPRESSED;
static const EncryptionProperties DEFAULT_ENCRYPTION = EncryptionProperties();

class PARQUET_EXPORT ColumnProperties {
 public:
  ColumnProperties(Encoding::type encoding = DEFAULT_ENCODING,
                   Compression::type codec = DEFAULT_COMPRESSION_TYPE,
                   bool dictionary_enabled = DEFAULT_IS_DICTIONARY_ENABLED,
                   bool statistics_enabled = DEFAULT_ARE_STATISTICS_ENABLED,
                   size_t max_stats_size = DEFAULT_MAX_STATISTICS_SIZE,
                   EncryptionProperties encryption = DEFAULT_ENCRYPTION)
      : encoding_(encoding),
        codec_(codec),
        dictionary_enabled_(dictionary_enabled),
        statistics_enabled_(statistics_enabled),
        max_stats_size_(max_stats_size),
        encryption_(encryption) {}

  void set_encoding(Encoding::type encoding) { encoding_ = encoding; }

  void set_compression(Compression::type codec) { codec_ = codec; }

  void set_dictionary_enabled(bool dictionary_enabled) {
    dictionary_enabled_ = dictionary_enabled;
  }

  void set_statistics_enabled(bool statistics_enabled) {
    statistics_enabled_ = statistics_enabled;
  }

  void set_max_statistics_size(size_t max_stats_size) {
    max_stats_size_ = max_stats_size;
  }

  void set_encryption(EncryptionProperties encryption) { encryption_ = encryption; }

  Encoding::type encoding() const { return encoding_; }

  Compression::type compression() const { return codec_; }

  bool dictionary_enabled() const { return dictionary_enabled_; }

  bool statistics_enabled() const { return statistics_enabled_; }

  size_t max_statistics_size() const { return max_stats_size_; }

  EncryptionProperties encryption() const { return encryption_; }

 private:
  Encoding::type encoding_;
  Compression::type codec_;
  bool dictionary_enabled_;
  bool statistics_enabled_;
  size_t max_stats_size_;
  EncryptionProperties encryption_;
};

class PARQUET_EXPORT FileEncryptionProperties {
 public:
  FileEncryptionProperties() = default;
  FileEncryptionProperties(const FileEncryptionProperties&) = default;

  FileEncryptionProperties(Encryption::type algorithm, const std::string& key,
                           const std::string& key_metadata) {
    DCHECK(key.length() == 16 || key.length() == 24 || key.length() == 32);
    if (!key_metadata.empty()) {
      DCHECK(key_metadata.length() <= 256);
    }

    footer_encryption_.reset(new EncryptionProperties(algorithm, key, key_metadata));
    uniform_encryption_ = !key.empty();
  }

  FileEncryptionProperties(Encryption::type algorithm, const std::string& key, int key_id)
      : FileEncryptionProperties(
            algorithm, key,
            key_id == 0 ? "" : std::string(reinterpret_cast<char*>(&key_id), 4)) {}

  /**
   * encrypt_the_rest will define if other columns (not defined in columns argument)
   * will be encrypted or not
   * if encrypt_the_rest = true, other columns will be encrypted with footer key
   * else, other columns will be unencrypted
   */
  void SetupColumns(const std::vector<ColumnEncryptionProperties>& columns,
                    bool encrypt_the_rest) {
    encrypt_the_rest_ = encrypt_the_rest;
    columns_ = columns;

    if (!footer_encryption_->key().empty()) {
      uniform_encryption_ = true;

      for (const auto& col : columns) {
        if (col.key().compare(footer_encryption_->key()) != 0) {
          uniform_encryption_ = false;
          break;
        }
      }
    } else {
      if (encrypt_the_rest)
        throw ParquetException("Encrypt the rest with null footer key");
      bool all_are_unencrypted = true;
      for (const auto& col : columns) {
        if (col.encrypted()) {
          if (col.key().empty()) {
            throw ParquetException("Encrypt column with null footer key");
          }
          all_are_unencrypted = false;
        }
      }

      if (all_are_unencrypted) {
        throw ParquetException("Footer and all columns unencrypted");
      }
    }
  }

  std::shared_ptr<EncryptionProperties> GetFooterEncryptionProperties() {
    return footer_encryption_;
  }

  std::shared_ptr<ColumnEncryptionProperties> GetColumnCryptoMetaData(
      const std::shared_ptr<schema::ColumnPath>& path) {
    // uniform encryption
    if (uniform_encryption_) {
      return std::make_shared<ColumnEncryptionProperties>(true, path->ToDotString());
    }

    // non-uniform encryption
    std::string path_str = path->ToDotString();
    for (const auto& col : columns_) {
      if (col.path() == path_str) {
        return std::shared_ptr<ColumnEncryptionProperties>(
            const_cast<ColumnEncryptionProperties*>(&col));
      }
    }
    // encrypted with footer key
    if (encrypt_the_rest_) {
      return std::make_shared<ColumnEncryptionProperties>(true, path->ToDotString());
    }

    // unencrypted
    return std::shared_ptr<ColumnEncryptionProperties>(
        new ColumnEncryptionProperties(false, path->ToDotString()));
  }

  std::shared_ptr<EncryptionProperties> GetColumnEncryptionProperties(
      const std::shared_ptr<schema::ColumnPath>& path) {
    // uniform encryption
    if (uniform_encryption_) {
      return footer_encryption_;
    }

    // non-uniform encryption
    std::string path_str = path->ToDotString();
    for (const auto& col : columns_) {
      if (col.path() == path_str) {
        return std::make_shared<EncryptionProperties>(footer_encryption_->algorithm(),
                                                      col.key(), col.key_metadata(),
                                                      footer_encryption_->aad());
      }
    }

    if (encrypt_the_rest_) {
      return footer_encryption_;
    }

    return nullptr;
  }

  void SetupAad(const std::string& aad) { footer_encryption_->aad(aad); }

 private:
  std::shared_ptr<EncryptionProperties> footer_encryption_;

  bool uniform_encryption_;

  std::vector<ColumnEncryptionProperties> columns_;
  bool encrypt_the_rest_;
};

class PARQUET_EXPORT WriterProperties {
 public:
  class Builder {
   public:
    Builder()
        : pool_(::arrow::default_memory_pool()),
          dictionary_pagesize_limit_(DEFAULT_DICTIONARY_PAGE_SIZE_LIMIT),
          write_batch_size_(DEFAULT_WRITE_BATCH_SIZE),
          max_row_group_length_(DEFAULT_MAX_ROW_GROUP_LENGTH),
          pagesize_(DEFAULT_PAGE_SIZE),
          version_(DEFAULT_WRITER_VERSION),
          created_by_(DEFAULT_CREATED_BY) {}
    virtual ~Builder() {}

    Builder* memory_pool(::arrow::MemoryPool* pool) {
      pool_ = pool;
      return this;
    }

    Builder* enable_dictionary() {
      default_column_properties_.set_dictionary_enabled(true);
      return this;
    }

    Builder* disable_dictionary() {
      default_column_properties_.set_dictionary_enabled(false);
      return this;
    }

    Builder* enable_dictionary(const std::string& path) {
      dictionary_enabled_[path] = true;
      return this;
    }

    Builder* enable_dictionary(const std::shared_ptr<schema::ColumnPath>& path) {
      return this->enable_dictionary(path->ToDotString());
    }

    Builder* disable_dictionary(const std::string& path) {
      dictionary_enabled_[path] = false;
      return this;
    }

    Builder* disable_dictionary(const std::shared_ptr<schema::ColumnPath>& path) {
      return this->disable_dictionary(path->ToDotString());
    }

    Builder* dictionary_pagesize_limit(int64_t dictionary_psize_limit) {
      dictionary_pagesize_limit_ = dictionary_psize_limit;
      return this;
    }

    Builder* write_batch_size(int64_t write_batch_size) {
      write_batch_size_ = write_batch_size;
      return this;
    }

    Builder* max_row_group_length(int64_t max_row_group_length) {
      max_row_group_length_ = max_row_group_length;
      return this;
    }

    Builder* data_pagesize(int64_t pg_size) {
      pagesize_ = pg_size;
      return this;
    }

    Builder* version(ParquetVersion::type version) {
      version_ = version;
      return this;
    }

    Builder* created_by(const std::string& created_by) {
      created_by_ = created_by;
      return this;
    }

    /**
     * Define the encoding that is used when we don't utilise dictionary encoding.
     *
     * This either apply if dictionary encoding is disabled or if we fallback
     * as the dictionary grew too large.
     */
    Builder* encoding(Encoding::type encoding_type) {
      if (encoding_type == Encoding::PLAIN_DICTIONARY ||
          encoding_type == Encoding::RLE_DICTIONARY) {
        throw ParquetException("Can't use dictionary encoding as fallback encoding");
      }

      default_column_properties_.set_encoding(encoding_type);
      return this;
    }

    /**
     * Define the encoding that is used when we don't utilise dictionary encoding.
     *
     * This either apply if dictionary encoding is disabled or if we fallback
     * as the dictionary grew too large.
     */
    Builder* encoding(const std::string& path, Encoding::type encoding_type) {
      if (encoding_type == Encoding::PLAIN_DICTIONARY ||
          encoding_type == Encoding::RLE_DICTIONARY) {
        throw ParquetException("Can't use dictionary encoding as fallback encoding");
      }

      encodings_[path] = encoding_type;
      return this;
    }

    /**
     * Define the encoding that is used when we don't utilise dictionary encoding.
     *
     * This either apply if dictionary encoding is disabled or if we fallback
     * as the dictionary grew too large.
     */
    Builder* encoding(const std::shared_ptr<schema::ColumnPath>& path,
                      Encoding::type encoding_type) {
      return this->encoding(path->ToDotString(), encoding_type);
    }

    Builder* compression(Compression::type codec) {
      default_column_properties_.set_compression(codec);
      return this;
    }

    Builder* max_statistics_size(size_t max_stats_sz) {
      default_column_properties_.set_max_statistics_size(max_stats_sz);
      return this;
    }

    Builder* compression(const std::string& path, Compression::type codec) {
      codecs_[path] = codec;
      return this;
    }

    Builder* compression(const std::shared_ptr<schema::ColumnPath>& path,
                         Compression::type codec) {
      return this->compression(path->ToDotString(), codec);
    }

    Builder* encryption(const std::string& key) {
      return encryption(Encryption::AES_GCM_V1, key, 0);
    }

    Builder* encryption(const std::string& key, uint32_t key_id) {
      return encryption(Encryption::AES_GCM_V1, key, key_id);
    }

    Builder* encryption(const std::string& key, std::string key_id) {
      return encryption(Encryption::AES_GCM_V1, key, key_id);
    }

    Builder* encryption(Encryption::type algorithm, const std::string& key,
                        uint32_t key_id) {
      file_encryption_.reset(new FileEncryptionProperties(algorithm, key, key_id));
      return this;
    }

    Builder* encryption(Encryption::type algorithm, const std::string& key,
                        const std::string& key_id) {
      file_encryption_.reset(new FileEncryptionProperties(algorithm, key, key_id));
      return this;
    }

    Builder* column_encryption(const std::vector<ColumnEncryptionProperties>& columns,
                               bool encrypt_the_rest) {
      if (file_encryption_ == nullptr) {
        throw ParquetException("null file encryption");
      }

      file_encryption_->SetupColumns(columns, encrypt_the_rest);
      return this;
    }

    Builder* enable_statistics() {
      default_column_properties_.set_statistics_enabled(true);
      return this;
    }

    Builder* disable_statistics() {
      default_column_properties_.set_statistics_enabled(false);
      return this;
    }

    Builder* enable_statistics(const std::string& path) {
      statistics_enabled_[path] = true;
      return this;
    }

    Builder* enable_statistics(const std::shared_ptr<schema::ColumnPath>& path) {
      return this->enable_statistics(path->ToDotString());
    }

    Builder* disable_statistics(const std::string& path) {
      statistics_enabled_[path] = false;
      return this;
    }

    Builder* disable_statistics(const std::shared_ptr<schema::ColumnPath>& path) {
      return this->disable_statistics(path->ToDotString());
    }

    std::shared_ptr<WriterProperties> build() {
      std::unordered_map<std::string, ColumnProperties> column_properties;
      auto get = [&](const std::string& key) -> ColumnProperties& {
        auto it = column_properties.find(key);
        if (it == column_properties.end())
          return column_properties[key] = default_column_properties_;
        else
          return it->second;
      };

      for (const auto& item : encodings_) get(item.first).set_encoding(item.second);
      for (const auto& item : codecs_) get(item.first).set_compression(item.second);
      for (const auto& item : dictionary_enabled_)
        get(item.first).set_dictionary_enabled(item.second);
      for (const auto& item : statistics_enabled_)
        get(item.first).set_statistics_enabled(item.second);

      return std::shared_ptr<WriterProperties>(new WriterProperties(
          pool_, dictionary_pagesize_limit_, write_batch_size_, max_row_group_length_,
          pagesize_, version_, created_by_, std::move(file_encryption_),
          default_column_properties_, column_properties));
    }

   private:
    ::arrow::MemoryPool* pool_;
    int64_t dictionary_pagesize_limit_;
    int64_t write_batch_size_;
    int64_t max_row_group_length_;
    int64_t pagesize_;
    ParquetVersion::type version_;
    std::string created_by_;
    std::unique_ptr<FileEncryptionProperties> file_encryption_;

    // Settings used for each column unless overridden in any of the maps below
    ColumnProperties default_column_properties_;
    std::unordered_map<std::string, Encoding::type> encodings_;
    std::unordered_map<std::string, Compression::type> codecs_;
    std::unordered_map<std::string, bool> dictionary_enabled_;
    std::unordered_map<std::string, bool> statistics_enabled_;
  };

  inline ::arrow::MemoryPool* memory_pool() const { return pool_; }

  inline int64_t dictionary_pagesize_limit() const { return dictionary_pagesize_limit_; }

  inline int64_t write_batch_size() const { return write_batch_size_; }

  inline int64_t max_row_group_length() const { return max_row_group_length_; }

  inline int64_t data_pagesize() const { return pagesize_; }

  inline ParquetVersion::type version() const { return parquet_version_; }

  inline std::string created_by() const { return parquet_created_by_; }

  inline FileEncryptionProperties* file_encryption() const {
    return parquet_file_encryption_.get();
  }

  inline std::shared_ptr<EncryptionProperties> footer_encryption() const {
    if (parquet_file_encryption_ == nullptr) {
      return nullptr;
    } else {
      return parquet_file_encryption_->GetFooterEncryptionProperties();
    }
  }

  inline Encoding::type dictionary_index_encoding() const {
    if (parquet_version_ == ParquetVersion::PARQUET_1_0) {
      return Encoding::PLAIN_DICTIONARY;
    } else {
      return Encoding::RLE_DICTIONARY;
    }
  }

  inline Encoding::type dictionary_page_encoding() const {
    if (parquet_version_ == ParquetVersion::PARQUET_1_0) {
      return Encoding::PLAIN_DICTIONARY;
    } else {
      return Encoding::PLAIN;
    }
  }

  const ColumnProperties& column_properties(
      const std::shared_ptr<schema::ColumnPath>& path) const {
    auto it = column_properties_.find(path->ToDotString());
    if (it != column_properties_.end()) return it->second;
    return default_column_properties_;
  }

  Encoding::type encoding(const std::shared_ptr<schema::ColumnPath>& path) const {
    return column_properties(path).encoding();
  }

  Compression::type compression(const std::shared_ptr<schema::ColumnPath>& path) const {
    return column_properties(path).compression();
  }

  bool dictionary_enabled(const std::shared_ptr<schema::ColumnPath>& path) const {
    return column_properties(path).dictionary_enabled();
  }

  bool statistics_enabled(const std::shared_ptr<schema::ColumnPath>& path) const {
    return column_properties(path).statistics_enabled();
  }

  size_t max_statistics_size(const std::shared_ptr<schema::ColumnPath>& path) const {
    return column_properties(path).max_statistics_size();
  }

  std::shared_ptr<ColumnEncryptionProperties> column_encryption_props(
      const std::shared_ptr<schema::ColumnPath>& path) const {
    if (parquet_file_encryption_) {
      return parquet_file_encryption_->GetColumnCryptoMetaData(path);
    } else {
      return nullptr;
    }
  }

  std::shared_ptr<EncryptionProperties> encryption(
      const std::shared_ptr<schema::ColumnPath>& path) const {
    if (parquet_file_encryption_) {
      return parquet_file_encryption_->GetColumnEncryptionProperties(path);
    } else {
      return nullptr;
    }
  }

 private:
  explicit WriterProperties(
      ::arrow::MemoryPool* pool, int64_t dictionary_pagesize_limit,
      int64_t write_batch_size, int64_t max_row_group_length, int64_t pagesize,
      ParquetVersion::type version, const std::string& created_by,
      std::shared_ptr<FileEncryptionProperties> file_encryption,
      const ColumnProperties& default_column_properties,
      const std::unordered_map<std::string, ColumnProperties>& column_properties)
      : pool_(pool),
        dictionary_pagesize_limit_(dictionary_pagesize_limit),
        write_batch_size_(write_batch_size),
        max_row_group_length_(max_row_group_length),
        pagesize_(pagesize),
        parquet_version_(version),
        parquet_created_by_(created_by),
        parquet_file_encryption_(file_encryption),
        default_column_properties_(default_column_properties),
        column_properties_(column_properties) {}

  ::arrow::MemoryPool* pool_;
  int64_t dictionary_pagesize_limit_;
  int64_t write_batch_size_;
  int64_t max_row_group_length_;
  int64_t pagesize_;
  ParquetVersion::type parquet_version_;
  std::string parquet_created_by_;
  std::shared_ptr<FileEncryptionProperties> parquet_file_encryption_;
  ColumnProperties default_column_properties_;
  std::unordered_map<std::string, ColumnProperties> column_properties_;
};

std::shared_ptr<WriterProperties> PARQUET_EXPORT default_writer_properties();

}  // namespace parquet

#endif  // PARQUET_COLUMN_PROPERTIES_H
