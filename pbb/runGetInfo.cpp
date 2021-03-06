#include "stdafx.h"
#include "runGetInfo.h"
#include "doDec.h"
#include "doBuild.h"
#include "Utils.h"

// 解密并获取key
pvKey getKey(const unsigned char *mem, int size)
{
    pvTable vt = doBuild(buildKey(), false);
    doDec((void *)mem, size, vt);

    //utils::output(mem, size);

    char buf[17] = { 0 };
    for ( int i = 0; i < 16; i++ ) {
        buf[i] = mem[i * 16 + 8];
    }
    return buildKey(buf);
}

pvKey getKey(fInfo file)
{
	char buf[0x10c + 0x20];
	memcpy(buf, file.addr + file.size - 0x10c, 0x10c);
	memcpy(&buf[0x10c], file.addr + file.size - 0x10C - 0x2A8 - 0x20, 0x20);
	return getKey((const unsigned char*)buf, 0x10c);
}

void runGetInfo(fInfo file)
{
    char buf[0x10c + 0x20];
    memcpy(buf, file.addr + file.size - 0x10c, 0x10c);
    memcpy(&buf[0x10c], file.addr + file.size - 0x10C - 0x2A8 - 0x20, 0x20);
    pvKey oldKey = getKey((const unsigned char*)buf, 0x10c);
    srcInfo info = getSrcInfo((const unsigned char*)&buf[0x10c], 0x20);

    // 打印
    printf("key: ");
    for ( int i = 0; i < 16; i++ ) {
        printf("%.2x", oldKey->v[i]);
    }
    printf("\r\nsize: %u %u\r\n", info.old_size, info.size);
}

srcInfo getSrcInfo(const unsigned char *mem, std::size_t size)
{
    pvTable vt = doBuild(buildKey(), false);
    doDec((void *)mem, size, vt);

    //utils::output(mem, size);

    return{
        *(unsigned int*)&mem[8],    // 对齐后大小
        *(unsigned int*)&mem[16] }; // 原本大小
}