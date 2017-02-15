//
// Created by Harunobu Daikoku on 2017/02/13.
//

#ifndef RNETLIB_LOCAL_MEMORY_REGION_H
#define RNETLIB_LOCAL_MEMORY_REGION_H

enum MRType {
  MR_LOCAL_WRITE = (1 << 0),
  MR_REMOTE_WRITE = (1 << 1),
  MR_REMOTE_READ = (1 << 2)
};

namespace rnetlib {
class LocalMemoryRegion {
 public:

  virtual void *GetAddr() const = 0;

  virtual size_t GetLength() const = 0;

  virtual uint32_t GetLKey() const = 0;

  virtual uint32_t GetRKey() const = 0;

};
}

#endif //RNETLIB_LOCAL_MEMORY_REGION_H
