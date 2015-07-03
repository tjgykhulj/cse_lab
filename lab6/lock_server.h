// this is the lock server
// the lock client has a similar interface

#ifndef lock_server_h
#define lock_server_h

#include <string>
#include "lock_protocol.h"
#include "lock_client.h"
#include "rpc.h"
#include <sys/time.h>

class lock_server {

 private:
	timeval* gettime() {
        timeval* s = new timeval;
        gettimeofday(s,0);
        return s;
    }
 protected:
  int nacquire;
  std::map<lock_protocol::lockid_t, std::pair<pthread_mutex_t, pthread_cond_t> > mutexes;
  //true for locked, false for free
  std::map<lock_protocol::lockid_t, timeval*> lock_states;


 public:
  lock_server();
  ~lock_server() {};
  lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status acquire(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status release(int clt, lock_protocol::lockid_t lid, int &);
};

#endif 







