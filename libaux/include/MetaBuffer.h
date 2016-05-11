#ifndef AMETABUFFER_H
#define AMETABUFFER_H

#include <stdint.h>

#include <utils/RefBase.h>
#include <utils/Errors.h>
#include <utils/threads.h>

using namespace android;

class MetaBuffer : public RefBase
{
public:
    typedef void (*freeFunc)(void* data);

    static MetaBuffer *create(size_t length,
                              void *data=NULL, freeFunc ff=NULL);

    uint8_t *data() { return (uint8_t *)mData + mRangeOffset; }
    size_t capacity() const { return mCapacity; }
    size_t size() const { return mRangeLength; }
    size_t offset() const { return mRangeOffset; }

    void setRange(size_t offset, size_t size);
    void setRange(size_t range) {
        ///ASSERT(range < mCapacity)
        mRangeLength = range;
    }

    // meta data part
    enum Type {
        kTypeInt32,
        kTypeInt64,
        kTypeSize,
        kTypeFloat,
        kTypeDouble,
        kTypePointer,
        kTypeString,
#if 0
        kTypeObject,
        kTypeMessage,
        kTypeRect,
        kTypeBuffer,
#endif
    };

    void clear();

    void setQuickData(uint32_t data) { mQuickData = data; }
    uint32_t quickData() const { return mQuickData; }

    void setInt32(const char *name, int32_t value);
    void setInt64(const char *name, int64_t value);
    void setSize(const char *name, size_t value);
    void setFloat(const char *name, float value);
    void setDouble(const char *name, double value);
    void setPointer(const char *name, void *value);
    ///void setString(const char *name, const char *s, ssize_t len = -1);

    bool findInt32(const char *name, int32_t *value);
    bool findInt64(const char *name, int64_t *value);
    bool findSize(const char *name, size_t *value);
    bool findFloat(const char *name, float *value);
    bool findDouble(const char *name, double *value);
    bool findPointer(const char *name, void **value);
    ///bool findString(const char *name, AString *value);


    virtual ~MetaBuffer();

    static void dump();

protected:
    virtual void onLastStrongRef(const void* id);

private:
    MetaBuffer(size_t capacity);
    MetaBuffer(void *data, size_t length, freeFunc ff);

    void *mData;
    size_t mCapacity;
    size_t mRangeOffset;
    size_t mRangeLength;

    uint32_t mQuickData;

    bool mOwnsData;
    // used while not own data memory, but user require metaBuffer to free it
    freeFunc mDealloc;

    // TODO: 2nd level hashTable may be engaged
    struct Item {
        union {
            int32_t int32Value;
            int64_t int64Value;
            size_t sizeValue;
            float floatValue;
            double doubleValue;
            void *ptrValue;
#if 0
            RefBase *refValue;
            AString *stringValue;
            Rect rectValue;
#endif
        } u;
        const char *mName;
        Type mType;
        ///struct Item *next;
    };

    ///Item *table[16];

    enum {
        kMaxNumItems = 12
    };
    Item mItems[kMaxNumItems];
    size_t mNumItems;

    Item *allocateItem(const char *name);
    void freeItem(Item *item);
    const Item *findItem(const char *name, Type type) const;
};

#endif

