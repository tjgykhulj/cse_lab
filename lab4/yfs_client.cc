// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
using namespace std;

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  ec = new extent_client(extent_dst);
  lc = new lock_client(lock_dst);
  if (ec->put(1, "") != extent_protocol::OK)
      printf("error init root dir\n"); // XYB: init root dir
}

yfs_client::inum
yfs_client::n2i(std::string n)
{
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string
yfs_client::filename(inum inum)
{
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

bool yfs_client::judge(inum inum, int types)
{
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK)
	{
        printf("error getting attr\n");
		return false;
	}
    if ((int)a.type == types) {
        printf("isfile: %lld is a type[%d]\n", inum, types);
        return true;
    }
	printf("isfile: %lld is not a type[%d]\n", inum, types);
	return false;
}

bool yfs_client::isfile(inum inum)
{
	lc->acquire(inum);
	bool ret = judge(inum, extent_protocol::T_FILE);
	lc->release(inum);
	return ret;
}
/** Your code here for Lab...
 * You may need to add routines such as
 * readlink, issymlink here to implement symbolic link.
 * 
 * */

bool yfs_client::isdir(inum inum)
{
	lc->acquire(inum);
	bool ret = judge(inum, extent_protocol::T_DIR);
	lc->release(inum);
	return ret;
}


bool yfs_client::issym(inum inum)
{
	lc->acquire(inum);
	bool ret = judge(inum, extent_protocol::T_SYMLINK);
	lc->release(inum);
	return ret;
}

int yfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;

	lc->acquire(inum);

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
	lc->release(inum);
    return r;
}

int yfs_client::getdir(inum inum, dirinfo &din)
{
    int ret = OK;

	lc->acquire(inum);

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        ret = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
	lc->release(inum);
    return ret;
}

int yfs_client::getlink(inum inum, linkinfo &lin)
{
    int r = OK;

	lc->acquire(inum);

    printf("getlink %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    lin.atime = a.atime;
    lin.mtime = a.mtime;
    lin.ctime = a.ctime;
	lin.size = a.size;
release:
	lc->release(inum);
    return r;
}


#define EXT_RPC(xx) do { \
    if ((xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        r = IOERR; \
        goto release; \
    } \
} while (0)

// Only support set size of attr
int yfs_client::setattr(inum ino, size_t size)
{
    /*
     * your lab2 code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */
	int ret = OK;
	lc->acquire(ino);

	string buf;
	if (ec->get(ino, buf) != OK) 
	{
		ret = IOERR;
		goto release;
	}
	buf.resize(size, 0);
	ec->put(ino, buf);

release:
	lc->release(ino);
    return ret;
}

int yfs_client::make(inum parent, const char *name, inum &ino_out, int types)
{
	bool found;
	lookup(parent, name, found, ino_out);
	if (found) return EXIST;

	ec->create(types, ino_out);
	string buf;
	ec->get(parent, buf);
	buf += string(name) + '/' + filename(ino_out) + '/';
	ec->put(parent, buf);
	return OK;
}

int yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    // your lab2 code goes here.
	lc->acquire(parent);
	int ret = make(parent, name, ino_out, extent_protocol::T_FILE);
	lc->release(parent);
	return ret;
}

int yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    // your lab2 code goes here.
	lc->acquire(parent);
	int ret = make(parent, name, ino_out, extent_protocol::T_DIR);
	lc->release(parent);
	return ret;
}

int yfs_client::symlink(inum parent, const char *name, const char *link, inum &ino_out)
{
    // your lab2 code goes here.
	lc->acquire(parent);
	int ret = make(parent, name, ino_out, extent_protocol::T_SYMLINK);
	lc->release(parent);
	if (ret == OK) {
		lc->acquire(ino_out);
		size_t size;
		twrite(ino_out, strlen(link), 0, link, size);
		lc->release(ino_out);
	}
	return ret;
}

int yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    // your lab2 code goes here.
	int ret = OK;
	found = false;
	list <dirent> dirents;
	dirents.clear();
	if (treaddir(parent, dirents) != OK) 
	{
		ret = IOERR;
		goto release;
	}

	for (list<dirent>::iterator it=dirents.begin(); 
			it!=dirents.end(); it++)
	if (it->name == name)
	{
		found = true;
		ino_out = it->inum;
		break;
	}
release:
    return ret;
}

int yfs_client::treaddir(inum dir, std::list<dirent> &list)
{
    /*
     * your lab2 code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */
	string buf, name, inum;
	dirent dr;

	ec->get(dir, buf);
	for (unsigned int i=0; i<buf.size();)
	{
		unsigned int j = i;
		while (i<buf.size()) if (buf[i++] == '/') break;
		dr.name = buf.substr(j, i-j-1);
		j = i;
		while (i<buf.size()) if (buf[i++] == '/') break;
		dr.inum = n2i( buf.substr(j, i-j-1) );
		list.push_back(dr);
	}

    return OK;
}
int yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    /*
     * your lab2 code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */
	lc->acquire(dir);
	int ret = treaddir(dir, list);
	lc->release(dir);
	return ret;
}

int
yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    /*
     * your lab2 code goes here.
     * note: read using ec->get().
     */
	lc->acquire(ino);

	if (ec->get(ino, data) != OK) {
		lc->release(ino);
		return IOERR;
	}
	if ((size_t)off>data.size()) data = "";
	else
		data = data.substr(off, size);

	lc->release(ino);
    return OK;
}

int
yfs_client::twrite(inum ino, size_t size, off_t off, const char *data,
        size_t &bytes_written)
{
    /*
     * your lab2 code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */

	string buf;
	ec->get(ino, buf);

	bytes_written = size;
	if ((size_t)off > buf.size())
	{
		bytes_written += off - buf.size();
	}
	if (buf.size() < off + size)
		buf.resize(off + size, 0);

	buf.replace(off, size, data, size);
//	for (size_t i=0; i<size; i++) buf[off+i] = data[i];
	ec->put(ino, buf);
    return OK;
}
	
int
yfs_client::write(inum ino, size_t size, off_t off, const char *data,
        size_t &bytes_written)
{
    /*
     * your lab2 code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */

	lc->acquire(ino);
	int ret = twrite(ino, size, off, data, bytes_written);
	lc->release(ino);
	return ret;
}

int yfs_client::unlink(inum parent,const char *name)
{
	/*
     * your lab2 code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */
	lc->acquire(parent);

	int ret = OK;
	inum ino_out;
	string buf, bname;

	bool found;
	lookup(parent, name, found, ino_out);
	if (!found) {
		ret = NOENT;
		goto release;
	}

	lc->acquire(ino_out);
	ec->remove(ino_out);
	lc->release(ino_out);

	ec->get(parent, buf);
	for (unsigned int i=0; i<buf.size();)
	{
		unsigned int j = i;
		for (; i<buf.size(); i++) if (buf[i] == '/') break;
		bname = buf.substr(j, i-j);
		i++;
		for (; i<buf.size(); i++) if (buf[i] == '/') break;
		i++;
		if (bname == string(name)) {
			buf.erase(j, i-j);
			break;
		}
	}
    ec->put(parent, buf);

release:
	lc->release(parent);
    return ret;
}

int yfs_client::readlink(inum ino, string &data)
{
	lc->acquire(ino);
    ec->get(ino, data);
	lc->release(ino);
    return OK;
}
