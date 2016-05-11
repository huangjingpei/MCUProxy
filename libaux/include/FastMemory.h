#ifndef FAST_MEMORY_H
#define FAST_MEMORY_H

#include <unistd.h>
#include <utils/RefBase.h>
#include <utils/threads.h>
#include <utils/List.h>

#include "MetaBuffer.h"
#include "myAlloc.h"

#define CONST_MAXNUM_ALLOC      (16384)

using namespace android;

class FastMemory : public RefBase
{
public:
    static inline FastMemory* Instance()
    {
        if (instance == NULL) {
            Mutex::Autolock _l(cLock);
            do {
                if (instance != NULL)
                    break;

                sp<FastMemory> ptr = new FastMemory();
                if (ptr == NULL)
                    break;

                if (NO_ERROR != ptr->init())
                    break;

                instance = ptr;
            }while(0);
        }
        return instance.get();
    };

    void *malloc(size_t _size);
    void free(void *_ptr);

    uint32_t toPhysical(void *_ptr);
    status_t allocate(sp<MetaBuffer> &buf, size_t _size);

    void dumpInfo(void *stream);

protected:
    status_t init();
    void reset();

    uint32_t mBase, mEnd;
    uint32_t mPhyBase;

    struct _mslot
    {
        void *mem;
        size_t size;
    }mem_slot[CONST_MAXNUM_ALLOC];
    slot_alloc_t mAllocSlot;

    struct _mslot *mSlotNow;

    Mutex mLockQ;
    List<struct _mslot *> mFreeSlots;
    List<struct _mslot *> mUsedSlots;

    bool validAddress(void *_addr) {
        return (((uint8_t *)_addr >= (uint8_t *)mBase) &&
                ((uint8_t *)_addr < (uint8_t *)mEnd));
    };

    struct _mslot *allocSlot() {
        return (struct _mslot *)slot_malloc(&mAllocSlot, sizeof(struct _mslot));
    };
    void freeSlot(struct _mslot *_slot) {
        slot_free(&mAllocSlot, _slot);
    }

    void enqueSlot(struct _mslot* _slot);
    void dequeSlot();

    status_t malloc(struct _mslot* _slot);

    FastMemory();
    virtual ~FastMemory();

private:
    static Mutex cLock;
    static sp<FastMemory> instance;
};

#endif

