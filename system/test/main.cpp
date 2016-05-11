#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <utils/RefBase.h>
#include <utils/Errors.h>
#include <utils/threads.h>
#include <utils/String8.h>
#include <utils/KeyedVector.h>

#define  status_t int
class Mutex_new {
public:
    enum {
        PRIVATE = 0,
        SHARED = 1
    };

                Mutex_new();
                Mutex_new(const char* name);
                Mutex_new(int type, const char* name = NULL);
                ~Mutex_new();

    // lock or unlock the mutex
    status_t    lock();
    void        unlock();

    // lock if possible; returns 0 on success, error otherwise
    status_t    tryLock();

    // Manages the mutex automatically. It'll be locked when Autolock is
    // constructed and released when Autolock goes out of scope.
    class Autolock {
    public:
        inline Autolock(Mutex_new& mutex) : mLock(mutex)  { mLock.lock(); }
        inline Autolock(Mutex_new* mutex) : mLock(*mutex) { mLock.lock(); }
        inline ~Autolock() { mLock.unlock(); }
    private:
        Mutex_new& mLock;
    };

private:
    friend class Condition;

    // A mutex cannot be copied
                Mutex_new(const Mutex_new&);
    Mutex_new&      operator = (const Mutex_new&);
public:
#if defined(HAVE_PTHREADS)
    pthread_mutex_t mMutex_new;
#else
    void    _init();
    void*   mState;
#endif
};
class Condition_new {
public:
    enum {
        PRIVATE = 0,
        SHARED = 1
    };

    Condition_new();
    Condition_new(int type);
    ~Condition_new();
    // Wait on the condition variable.  Lock the mutex before calling.
    status_t wait(Mutex_new& mutex);
    // same with relative timeout
    status_t waitRelative(Mutex_new& mutex, nsecs_t reltime);
    // Signal the condition variable, allowing one thread to continue.
    void signal();
    // Signal the condition variable, allowing all threads to continue.
    void broadcast();

private:
#if defined(HAVE_PTHREADS)
    //pthread_cond_t *mCond;
    pthread_cond_t mCond;
    //int dummy[16];
#else
    void*   mState;
#endif
};


#if defined(HAVE_PTHREADS)//:mCond(PTHREAD_COND_INITIALIZER)
inline Condition_new::Condition_new(){
     //mCond = new pthread_cond_t;
     for(int i=0 ;i<16;i++) {
         dummy[i] = 64+i;
     }
     pthread_cond_init(&mCond,NULL);
     for(int i=0 ;i<16;i++) {
         printf("---%d %d\n",i,dummy[i]);
     }
}
inline Condition_new::Condition_new(int type) {
    //mCond = new pthread_cond_t;
    printf("******************************\n");
    if (type == SHARED) {
        pthread_condattr_t attr;
        pthread_condattr_init(&attr);
        pthread_condattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        pthread_cond_init(&mCond, &attr);
        pthread_condattr_destroy(&attr);
    } else {
        pthread_cond_init(&mCond,NULL);
    }
}
inline Condition_new::~Condition_new() {

    pthread_cond_destroy(&mCond);
    //delete mCond;
}
inline status_t Condition_new::wait(Mutex_new& mutex) {

    return -pthread_cond_wait(&mCond, &mutex.mMutex_new);
}
inline status_t Condition_new::waitRelative(Mutex_new& mutex, nsecs_t reltime) {
#if defined(HAVE_PTHREAD_COND_TIMEDWAIT_RELATIVE)
    struct timespec ts;
    ts.tv_sec  = reltime/1000000000;
    ts.tv_nsec = reltime%1000000000;
    return -pthread_cond_timedwait_relative_np(&mCond, &mutex.mMutex_new, &ts);
#else // HAVE_PTHREAD_COND_TIMEDWAIT_RELATIVE
    struct timespec ts;
#if defined(HAVE_POSIX_CLOCKS)
    clock_gettime(CLOCK_REALTIME, &ts);
#else // HAVE_POSIX_CLOCKS
    // we don't support the clocks here.
    struct timeval t;
    gettimeofday(&t, NULL);
    ts.tv_sec = t.tv_sec;
    ts.tv_nsec= t.tv_usec*1000;
#endif // HAVE_POSIX_CLOCKS
    ts.tv_sec += reltime/1000000000;
    ts.tv_nsec+= reltime%1000000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_nsec -= 1000000000;
        ts.tv_sec  += 1;
    }
    return -pthread_cond_timedwait(&mCond, &mutex.mMutex_new, &ts);
#endif // HAVE_PTHREAD_COND_TIMEDWAIT_RELATIVE
}
inline void Condition_new::signal() {

    pthread_cond_signal(&mCond);
}
inline void Condition_new::broadcast() {

    pthread_cond_broadcast(&mCond);
}

#endif // HAVE_PTHREADS


// ---------------------------------------------------------------------------



// ---------------------------------------------------------------------------

/*
 * Automatic mutex.  Declare one of these at the top of a function.
 * When the function returns, it will go out of scope, and release the
 * mutex.
 */

typedef Mutex_new::Autolock AutoMutex_new;
#if defined(HAVE_PTHREADS)

inline  Mutex_new::Mutex_new() {
    //mMutex_new = new pthread_mutex_t;

   // printf("%s %d %p %p\n",__FILE__, __LINE__,this, mMutex_new);
    pthread_mutex_init(&mMutex_new, NULL);
}
inline  Mutex_new::Mutex_new(const char* name) {
    //mMutex_new = new pthread_mutex_t;

    pthread_mutex_init(&mMutex_new, NULL);
    //printf("%s %d %s %p %p\n",__FILE__, __LINE__,name, this,mMutex_new);
}
inline  Mutex_new::Mutex_new(int type, const char* name) {
   // mMutex_new = new pthread_mutex_t;

    //printf("%s %d %p\n",__FILE__, __LINE__,mMutex_new);
    if (type == SHARED) {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(&mMutex_new, &attr);
        pthread_mutexattr_destroy(&attr);
    } else {
        pthread_mutex_init(&mMutex_new, NULL);
    }
}
inline  Mutex_new::~Mutex_new() {
    pthread_mutex_destroy(&mMutex_new);
    //printf("%s %d %p\n",__FILE__, __LINE__,mMutex_new);
  //  delete mMutex_new;
}
inline  status_t Mutex_new::lock() {

    return -pthread_mutex_lock(&mMutex_new);
}
inline void Mutex_new::unlock() {
    pthread_mutex_unlock(&mMutex_new);
}
inline status_t Mutex_new::tryLock() {
    return -pthread_mutex_trylock(&mMutex_new);
}
#endif // HAVE_PTHREADS
using namespace android ;
    pthread_cond_t mCond = PTHREAD_COND_INITIALIZER;
    int value[16];
    pthread_mutex_t mMutex_new = PTHREAD_MUTEX_INITIALIZER;

   // Condition_new       ThreadExitedCondition_new;
   // Mutex_new           mLock;


class CThreadTest: public Thread
{
public:
    CThreadTest(){};
    virtual ~CThreadTest(){};
    virtual bool threadLoop();

    int start(){ run(); return 0;};
    //CThreadTest(const CThreadTest&){printf("%s %d\n",__FILE__, __LINE__);};
    //void operator=(const CThreadTest&){printf("%s %d\n",__FILE__, __LINE__);};
};
bool CThreadTest::threadLoop()
{
    //printf("%s %d\n",__FILE__, __LINE__);
    if (exitPending()) {

        return false;
    }
    static int cnt =0;
    if(cnt++ == 1000000)
    {
        printf("%s %d %d\n",__FILE__, __LINE__,cnt);
       pthread_cond_broadcast(&mCond);
   //   ThreadExitedCondition_new.broadcast();
    }


    //printf("%s %d\n",__FILE__, __LINE__);
    return true;
}
class CTest :virtual public RefBase
{
public:
    CTest(){ };
    virtual ~CTest(){};
    void print(){printf("%s %d\n",__FILE__, __LINE__);};
};
class CTestSub : public CTest
{
public:
    CTestSub(){};
    virtual ~CTestSub(){};
    void print(){printf("%s %d\n",__FILE__, __LINE__);};
};





    sp<CThreadTest>  threadtest;
        sp<CTestSub>  test;
int main(int argc ,char *argv[])
{
    printf("%s %d\n",__FILE__, __LINE__);
    #if 1
     test = new  CTestSub();

    test->print();

    printf("%s %d\n",__FILE__, __LINE__);

    CThreadTest *p= new CThreadTest();
   printf("%s %d\n",__FILE__, __LINE__);

    threadtest = p;
    printf("%s %d\n",__FILE__, __LINE__);
    threadtest->start();
    printf("%s %d\n",__FILE__, __LINE__);
    usleep(1000);
    printf("%s %d\n",__FILE__, __LINE__);
  //  pthread_mutex_lock(&mMutex_new);
  printf("%s %d value:%d\n",__FILE__, __LINE__,value);
  #endif
     for(int i=0 ;i<16;i++) {
         value[i] = 64+i;
     }
   //  pthread_condattr_t attr;
   //     pthread_condattr_init(&attr);
  //   pthread_cond_init(&mCond,&attr);
    //mCond= PTHREAD_COND_INITIALIZER;
    // pthread_condattr_destroy(&attr);
     for(int i=0 ;i<16;i++) {
         printf("***%d %d\n",i,value[i]);
     }
  //  printf("%s %d value:%d\n",__FILE__, __LINE__,value);
    pthread_cond_wait(&mCond, &mMutex_new);
   // pthread_mutex_unlock(&mMutex_new);
    //printf("%s %d\n",__FILE__, __LINE__);
     //ThreadExitedCondition_new.wait(mLock);
  //  threadtest->requestExitAndWait();
    printf("%s %d\n",__FILE__, __LINE__);
    return 0;
}
