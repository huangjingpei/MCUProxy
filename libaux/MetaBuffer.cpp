
#include "TokenPool.h"
#include "MetaBuffer.h"

#include "sh_print.h"

#define LOG_TAG "MetaBuffer"
#include "utility.h"


// TODO: 2nd level hashTable may be engaged
#if 0
// static
static uint32_t atom_hash(const char *s) //AAtomizer::Hash
{
    uint32_t sum = 0;
    while (*s != '\0') {
        sum = (sum * 31) + *s;
        ++s;
    }

    return sum;
}

static uint32_t djb2_hash(const char *str, int count=7)
{
    uint32_t hash = 5381;
    int c;

    if (count < 0 || count >= 7)
        count = 7;      // common hash for 7 char

    while ((c = *(str++)) && count--)
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}
#endif

MetaBuffer* MetaBuffer::create(size_t length, void *data, freeFunc ff)
{
    MetaBuffer* self = NULL;
    status_t err = UNKNOWN_ERROR;

    do
    {
        if (data != NULL) {
            self = new MetaBuffer(data, length, ff);
        } else {
            self = new MetaBuffer(length);
        }

        BREAK_IF(self == NULL);
        BREAK_IF(self->data() == NULL);
        err = NO_ERROR;
    }while(0);

    if (err != NO_ERROR && self != NULL) {
        delete self;
        self = NULL;
    }

    return self;
}

MetaBuffer::MetaBuffer(size_t capacity)
    : mData(malloc(capacity)),
      mCapacity(capacity),
      mRangeOffset(0),
      mRangeLength(capacity),
      mQuickData(0),
      mOwnsData(true),
      mDealloc(NULL),
      mNumItems(0)
{
    ///memset(table, 0, sizeof(table));
}

MetaBuffer::MetaBuffer(void *data, size_t capacity, freeFunc ff)
    : mData(data),
      mCapacity(capacity),
      mRangeOffset(0),
      mRangeLength(capacity),
      mQuickData(0),
      mOwnsData(false),
      mDealloc(ff),
      mNumItems(0)
{
    ///memset(table, 0, sizeof(table));
}

MetaBuffer::~MetaBuffer()
{
    if (mOwnsData) {
        if (mData != NULL) {
            free(mData);
            mData = NULL;
        }
    } else if (mDealloc != NULL) {
        mDealloc(mData);
    }

    clear();
}


void MetaBuffer::setRange(size_t offset, size_t size)
{
    //ASSERT(offset <= mCapacity);
    //ASSERT((offset + size) <= mCapacity);

    mRangeOffset = offset;
    mRangeLength = size;
}

#if 1

void MetaBuffer::clear()
{
    for (size_t i = 0; i < mNumItems; ++i)
    {
        Item *item = &mItems[i];
        freeItem(item);
    }
    mNumItems = 0;
}

void MetaBuffer::freeItem(Item *item)
{
/*    switch (item->mType) {
        case kTypeString:
        {
            delete item->u.stringValue;
            break;
        }

        case kTypeObject:
        case kTypeMessage:
        case kTypeBuffer:
        {
            if (item->u.refValue != NULL) {
                item->u.refValue->decStrong(this);
            }
            break;
        }

        default:
            break;
    }*/
}

MetaBuffer::Item *MetaBuffer::allocateItem(const char *name)
{
    name = TokenPool::atomize(name);

    size_t i = 0;
    while (i < mNumItems && mItems[i].mName != name) {
        ++i;
    }

    Item *item;

    if (i < mNumItems) {
        item = &mItems[i];
        freeItem(item);
    } else {
        CHECK(mNumItems < kMaxNumItems);
        i = mNumItems++;
        ///LOGV("increased %d", mNumItems);
        item = &mItems[i];

        item->mName = name;
    }

    return item;
}

const MetaBuffer::Item *MetaBuffer::findItem(
        const char *name, Type type) const
{
    name = TokenPool::atomize(name);

    for (size_t i = 0; i < mNumItems; ++i) {
        const Item *item = &mItems[i];

        if (item->mName == name) {
            return item->mType == type ? item : NULL;
        }
    }

    return NULL;
}

#define BASIC_TYPE(NAME,FIELDNAME,TYPENAME)                             \
void MetaBuffer::set##NAME(const char *name, TYPENAME value) {         \
    Item *item = allocateItem(name);                                    \
                                                                        \
    item->mType = kType##NAME;                                          \
    item->u.FIELDNAME = value;                                          \
}                                                                       \
                                                                        \
bool MetaBuffer::find##NAME(const char *name, TYPENAME *value) {       \
    const Item *item = findItem(name, kType##NAME);                     \
    if (item) {                                                         \
        *value = item->u.FIELDNAME;                                     \
        return true;                                                    \
    }                                                                   \
    return false;                                                       \
}

BASIC_TYPE(Int32,int32Value,int32_t)
BASIC_TYPE(Int64,int64Value,int64_t)
BASIC_TYPE(Size,sizeValue,size_t)
BASIC_TYPE(Float,floatValue,float)
BASIC_TYPE(Double,doubleValue,double)
BASIC_TYPE(Pointer,ptrValue,void *)

#undef BASIC_TYPE
/***********
void MetaBuffer::setString(
        const char *name, const char *s, ssize_t len) {
    Item *item = allocateItem(name);
    item->mType = kTypeString;
    item->u.stringValue = new AString(s, len < 0 ? strlen(s) : len);
}

void MetaBuffer::setObjectInternal(
        const char *name, const sp<RefBase> &obj, Type type) {
    Item *item = allocateItem(name);
    item->mType = type;

    if (obj != NULL) { obj->incStrong(this); }
    item->u.refValue = obj.get();
}

void MetaBuffer::setObject(const char *name, const sp<RefBase> &obj) {
    setObjectInternal(name, obj, kTypeObject);
}

void MetaBuffer::setBuffer(const char *name, const sp<MetaBuffer> &buffer) {
    setObjectInternal(name, sp<RefBase>(buffer), kTypeBuffer);
}

void MetaBuffer::setMessage(const char *name, const sp<AMessage> &obj) {
    Item *item = allocateItem(name);
    item->mType = kTypeMessage;

    if (obj != NULL) { obj->incStrong(this); }
    item->u.refValue = obj.get();
}

void MetaBuffer::setRect(
        const char *name,
        int32_t left, int32_t top, int32_t right, int32_t bottom) {
    Item *item = allocateItem(name);
    item->mType = kTypeRect;

    item->u.rectValue.mLeft = left;
    item->u.rectValue.mTop = top;
    item->u.rectValue.mRight = right;
    item->u.rectValue.mBottom = bottom;
}

bool MetaBuffer::findString(const char *name, AString *value) const {
    const Item *item = findItem(name, kTypeString);
    if (item) {
        *value = *item->u.stringValue;
        return true;
    }
    return false;
}

bool MetaBuffer::findObject(const char *name, sp<RefBase> *obj) const {
    const Item *item = findItem(name, kTypeObject);
    if (item) {
        *obj = item->u.refValue;
        return true;
    }
    return false;
}

bool MetaBuffer::findBuffer(const char *name, sp<MetaBuffer> *buf) const {
    const Item *item = findItem(name, kTypeBuffer);
    if (item) {
        *buf = (MetaBuffer *)(item->u.refValue);
        return true;
    }
    return false;
}

bool MetaBuffer::findMessage(const char *name, sp<AMessage> *obj) const {
    const Item *item = findItem(name, kTypeMessage);
    if (item) {
        *obj = static_cast<AMessage *>(item->u.refValue);
        return true;
    }
    return false;
}

bool MetaBuffer::findRect(
        const char *name,
        int32_t *left, int32_t *top, int32_t *right, int32_t *bottom) const {
    const Item *item = findItem(name, kTypeRect);
    if (item == NULL) {
        return false;
    }

    *left = item->u.rectValue.mLeft;
    *top = item->u.rectValue.mTop;
    *right = item->u.rectValue.mRight;
    *bottom = item->u.rectValue.mBottom;

    return true;
}
****************/
#else
void MetaBuffer::setInt32(const char *name, int32_t value)
{
    metabSetInt32(&mContent, name, value);
}

void MetaBuffer::setInt64(const char *name, int64_t value)
{   // discard 'name' by now
    mInt64Data = value;
}

///void MetaBuffer::setSize(const char *name, size_t value);
///void MetaBuffer::setFloat(const char *name, float value);
///void MetaBuffer::setDouble(const char *name, double value);
void MetaBuffer::setPointer(const char *name, void *value)
{
    metabSetInt32(&mContent, name, (int32_t)value);
}
///void MetaBuffer::setString(const char *name, const char *s, ssize_t len = -1);

bool MetaBuffer::findInt32(const char *name, int32_t *value)
{
    int ret = metabGetInt32(&mContent, name, value);
    return (ret == 0)? true : false;
}

bool MetaBuffer::findInt64(const char *name, int64_t *value)
{
    if ((name != NULL) && (value != NULL)) {
        *value = mInt64Data;
        return true;
    } else {
        LOGE("%s %p error", name, value);
        return false;
    }
}

///bool MetaBuffer::findSize(const char *name, size_t *value) const;
///bool MetaBuffer::findFloat(const char *name, float *value) const;
///bool MetaBuffer::findDouble(const char *name, double *value) const;
bool MetaBuffer::findPointer(const char *name, void **value)
{
    int ret = metabGetInt32(&mContent, name, (int32_t *)value);
    return (ret == 0)? true : false;
}
///bool MetaBuffer::findString(const char *name, AString *value) const;
#endif

void MetaBuffer::dump()
{
}
// debug: could list hash table status and check efficientcy, if exists
void MetaBuffer::onLastStrongRef(const void* id)
{
}

