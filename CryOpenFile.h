#pragma once
#ifndef CRYFS_LIB_CRYOPENFILE_H_
#define CRYFS_LIB_CRYOPENFILE_H_

#include "messmer/fspp/fs_interface/OpenFile.h"
#include "messmer/cpp-utils/macros.h"

namespace cryfs {
class CryDevice;
class FileBlob;

class CryOpenFile: public fspp::OpenFile {
public:
  CryOpenFile(std::unique_ptr<FileBlob> fileBlob);
  virtual ~CryOpenFile();

  void stat(struct ::stat *result) const override;
  void truncate(off_t size) const override;
  int read(void *buf, size_t count, off_t offset) override;
  void write(const void *buf, size_t count, off_t offset) override;
  void flush() override;
  void fsync() override;
  void fdatasync() override;

private:
  std::unique_ptr<FileBlob> _fileBlob;

  DISALLOW_COPY_AND_ASSIGN(CryOpenFile);
};

}

#endif