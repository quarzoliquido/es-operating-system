/*
 * Copyright (c) 2006
 * Nintendo Co., Ltd.
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Nintendo makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 */

#include <stdlib.h>
#include <string.h>
#include <es.h>
#include <es/ref.h>
#include <es/clsid.h>
#include <es/interlocked.h>
#include <es/base/ICache.h>
#include "core.h"

#define VERBOSE
#include "memoryStream.h"

#define TEST(exp)                           \
    (void) ((exp) ||                        \
            (esPanic(__FILE__, __LINE__, "\nFailed test " #exp), 0))

const u8 Pattern[16] =
{
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    'A', 'B', 'C', 'D', 'E', 'F'
};

int main()
{
    long long size;
    long len;

    IInterface* root = 0;
    esInit(&root);

    ICacheFactory* cacheFactory = 0;
    esCreateInstance(CLSID_CacheFactory,
                     IID_ICacheFactory,
                     reinterpret_cast<void**>(&cacheFactory));

    MemoryStream* backingStore = new MemoryStream(0);
    ICache* cache = cacheFactory->create(backingStore);

    IStream* stream = cache->getStream();

    stream->write(Pattern, 8);
    size = stream->getSize();
    TEST(size == 8);

    // Wait for 3 seconds to see the update thread cleans up modified
    // page.
    esSleep(30000000);

    stream->write(Pattern + 8, 8);
    size = stream->getSize();
    TEST(size == 16);

    stream->flush();
    stream->release();
    cache->release();

    size = backingStore->getSize();
    TEST(size == 16);

    cache = cacheFactory->create(backingStore);
    stream = cache->getStream();
    size = stream->getSize();
    TEST(size == 16);

    u8 buffer[16];
    memset(buffer, 0, 16);
    stream->read(buffer, 16, 0);
    esReport("%.16s\n", buffer);
    TEST(memcmp(buffer, Pattern, 16) == 0);

    stream->setSize(0);
    size = stream->getSize();
    TEST(size == 0);
    size = stream->read(buffer, 16, 0);
    TEST(size == 0);
    size = backingStore->getSize();
    TEST(size == 0);

    stream->release();
    cache->release();

    // Test input/output streams
    cache = cacheFactory->create(backingStore);

    stream = cache->getInputStream();
    len = 0;
    try
    {
        len = stream->write(Pattern, 8);
    }
    catch (SystemException<EACCES>&)
    {
        esReport("write() inhibitted.\n");
    }
    TEST(len == 0);
    stream->release();

    stream = cache->getOutputStream();
    len = 0;
    try
    {
        len = stream->read(buffer, 8);
    }
    catch (SystemException<EACCES>&)
    {
        esReport("read() inhibitted.\n");
    }
    TEST(len == 0);
    stream->release();

    cache->release();

    // Test sector size
    cache = cacheFactory->create(backingStore);
    int sectorSize = 512;
    cache->setSectorSize(sectorSize);
    sectorSize = 0;
    cache->getSectorSize(sectorSize);
    TEST(sectorSize == 512);
    stream = cache->getStream();
    stream->setSize(4096);
    stream->write(Pattern, 16, 0);
    stream->flush();
    stream->write(Pattern, 16, 0);
    stream->write(Pattern, 16, 512);
    stream->flush();
    stream->write(Pattern, 16, 513);
    stream->write(Pattern, 16, 2048 + 512 - 1);
    stream->write(Pattern, 16, 2049 + 512);
    stream->write(Pattern, 1, 2048 + 1024 - 1);
    stream->flush();

    cache->release();

    cacheFactory->release();

    // Wait for 3 seconds to see the update thread exits.
    esSleep(30000000);

    esReport("done.\n");
}
