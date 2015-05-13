// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server():
  nacquire (0)
{
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
	// Your lab4 code goes here
  if (mutexes.find(lid) == mutexes.end()) {
      pthread_mutex_t mu;
      pthread_mutex_init(&mu, NULL);
      pthread_cond_t co;
      pthread_cond_init(&co, NULL);
      mutexes[lid] = std::make_pair(mu, co);
  }
  pthread_mutex_lock(&mutexes[lid].first);
  if (lock_states.find(lid) == lock_states.end()) {
      lock_states[lid] = true;
      nacquire++;
  } else {
      while (lock_states[lid])
          pthread_cond_wait(&mutexes[lid].second, &mutexes[lid].first);

      lock_states[lid] = true;
      nacquire++;
  }
  pthread_mutex_unlock(&mutexes[lid].first);
  return lock_protocol::OK;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
  if (mutexes.find(lid) == mutexes.end() || lock_states.find(lid) == lock_states.end())
      return lock_protocol::RETRY;
  pthread_mutex_lock(&mutexes[lid].first);
  lock_states[lid] = false;
  nacquire--;
  pthread_cond_signal(&mutexes[lid].second);
  pthread_mutex_unlock(&mutexes[lid].first);
 // Your lab4 code goes here
  return lock_protocol::OK;
}
