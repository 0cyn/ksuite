//
// Created by kat on 7/1/23.
//

#include <string>

#ifndef KSUITE_HEX_H
#define KSUITE_HEX_H

std::string formatAddress(uint64_t address, uint8_t padSize = 8)
{
    // put this in apiiiiii :(
    static const char hex[513] =
            "000102030405060708090a0b0c0d0e0f"
            "101112131415161718191a1b1c1d1e1f"
            "202122232425262728292a2b2c2d2e2f"
            "303132333435363738393a3b3c3d3e3f"
            "404142434445464748494a4b4c4d4e4f"
            "505152535455565758595a5b5c5d5e5f"
            "606162636465666768696a6b6c6d6e6f"
            "707172737475767778797a7b7c7d7e7f"
            "808182838485868788898a8b8c8d8e8f"
            "909192939495969798999a9b9c9d9e9f"
            "a0a1a2a3a4a5a6a7a8a9aaabacadaeaf"
            "b0b1b2b3b4b5b6b7b8b9babbbcbdbebf"
            "c0c1c2c3c4c5c6c7c8c9cacbcccdcecf"
            "d0d1d2d3d4d5d6d7d8d9dadbdcdddedf"
            "e0e1e2e3e4e5e6e7e8e9eaebecedeeef"
            "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff";
    char temp[16];
    char fin[17];
    int i = 0;
    for (; i < 16; i += 2)
    {
        temp[i] = hex[(address & 0xff) * 2];
        temp[i + 1] = hex[((address & 0xff) * 2) + 1];
        address = address >> 8;
        if (!address && i >= (padSize - 1))
            break;
    }
    int j = 0;
    int len = i + 1;
    for (; j < len; j += 2)
    {
        fin[j] = temp[i];
        fin[j + 1] = temp[i + 1];
        i -= 2;
    }
    fin[j] = '\0';
    // chop leading zero
    if (j > padSize && fin[0] == '0')
        return {&(fin[1])};
    return {fin};
};

#endif //KSUITE_HEX_H
