#pragma once
#include <cstddef>
#include <cstdint>
#include <sys/uio.h>
#include <unistd.h>

// This process only. Invalid/partial reads fail without dereferencing a pointer.
// A fresh instance bounds each snapshot's total work. No write API is provided.
struct SelfMemoryReader {
    size_t remaining=262144;
    bool read(uint64_t address,void* output,size_t size){
        if(!address||!output||!size||size>256||size>remaining||address>UINT64_MAX-size)return false;
        remaining-=size;
        iovec local{output,size},remote{reinterpret_cast<void*>(address),size};
        return process_vm_readv(getpid(),&local,1,&remote,1,0)==static_cast<ssize_t>(size);
    }
};
