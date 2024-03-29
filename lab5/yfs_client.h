#ifndef yfs_client_h
#define yfs_client_h

#include <string>
#include "lock_protocol.h"
#include "lock_client.h"
//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>

using namespace std;

class yfs_client {
  extent_client *ec;
  lock_client *lc;
 public:

  typedef unsigned long long inum;
  enum xxstatus { OK, RPCERR, NOENT, IOERR, EXIST };
  typedef int status;

  struct fileinfo {
    unsigned long long size;
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirinfo {
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  typedef struct fileinfo linkinfo;
  struct dirent {
    std::string name;
    yfs_client::inum inum;
  };

 private:

  static std::string filename(inum);
  static inum n2i(std::string);
  int make(inum, const char *, inum &, int);
  bool judge(inum, int);
  int twrite(inum, size_t, off_t, const char *, size_t &);
  int treaddir(inum, std::list<dirent> &);
 
 public:
  yfs_client(std::string, std::string);

  bool isfile(inum);
  bool isdir(inum);
  bool issym(inum);

  int getfile(inum, fileinfo &);
  int getdir(inum, dirinfo &);
  int getlink(inum, linkinfo &);

  int setattr(inum, size_t);
  int lookup(inum, const char *, bool &, inum &);
  int create(inum, const char *, mode_t, inum &);
  int readdir(inum, std::list<dirent> &);
  int write(inum, size_t, off_t, const char *, size_t &);
  int read(inum, size_t, off_t, std::string &);
  int unlink(inum,const char *);
  int mkdir(inum , const char *, mode_t , inum &);
  int symlink(inum, const char *, const char *, inum &);
  int readlink(inum, string &);
};

#endif 
