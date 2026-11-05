#pragma once
#include "scalar-component-identity.hpp"
#ifdef FRAME_SCALAR_ALL_FINGERS_BETA
#include "frame-beta-paths.hpp"
#endif
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

namespace frame_scalar {
struct ComponentDescription {
    uint64_t handle=0;
    std::string path;
    uint32_t units=0;
    uint64_t nodeAddress=0;
    bool operator==(const ComponentDescription& other)const{
        return handle==other.handle&&path==other.path&&units==other.units&&nodeAddress==other.nodeAddress;
    }
};
struct Snapshot {bool valid=false;std::vector<ComponentDescription> components;};
// Version-specific read-only map decoder. Reader must return false on unmapped,
// partial or out-of-range reads. Never dereference the runtime addresses directly.
template<class Reader>
Snapshot snapshotOnce(Reader& reader,uint64_t backend,uint32_t device){
    Snapshot out;
    if(device>=64||!backend||backend>std::numeric_limits<uint64_t>::max()-512)return out;
    auto pointer=[](uint64_t p){return p>=4096&&p%8==0&&p<std::numeric_limits<uint64_t>::max()-512;};
    auto read64=[&](uint64_t address,uint64_t& value){return reader.read(address,&value,8);};
    uint64_t root=0;
    if(!read64(backend+280,root))return out;
    const uint64_t container=(uint64_t{1}<<32)|device;
    uint64_t node=root,table=0;
    std::vector<uint64_t> seen;
    while(node){
        if(!pointer(node)||seen.size()>=64||std::find(seen.begin(),seen.end(),node)!=seen.end())return out;
        seen.push_back(node);
        uint64_t left=0,right=0,key=0;
        if(!read64(node+16,left)||!read64(node+24,right)||!read64(node+32,key))return out;
        if(key==container){if(!read64(node+40,table))return out;break;}
        node=container<key?left:right;
    }
    if(!node||!pointer(table))return out;
    uint64_t componentRoot=0;
    if(!read64(table+16,componentRoot))return out;
    struct Branch {uint64_t node,low,high;};
    std::vector<Branch> pending{{componentRoot,0,std::numeric_limits<uint64_t>::max()}};
    seen.clear();
    while(!pending.empty()){
        auto branch=pending.back();pending.pop_back();node=branch.node;
        if(!node)continue;
        if(!pointer(node)||seen.size()>=256||std::find(seen.begin(),seen.end(),node)!=seen.end())return {};
        seen.push_back(node);
        uint64_t left=0,right=0,key=0;uint32_t type=0;
        if(!read64(node+16,left)||!read64(node+24,right)||!read64(node+32,key)||!reader.read(node+72,&type,4))return {};
        auto owner=deviceFromComponent(key);
        if(!owner||*owner!=device||key<=branch.low||key>=branch.high)return {};
        pending.push_back({right,key,branch.high});pending.push_back({left,branch.low,key});
        if(type!=2)continue;
        uint64_t text=0,length=0;
        if(!read64(node+40,text)||!read64(node+48,length)||length>128||!text)return {};
        char path[129]{};
        if(!reader.read(text,path,static_cast<size_t>(length)+1)||path[length]||
           std::memchr(path,0,static_cast<size_t>(length)+1)!=path+length)return {};
#ifdef FRAME_SCALAR_ALL_FINGERS_BETA
        if(frame_beta::fingerForPath(path)<0)continue;
#else
        if(std::strcmp(path,"/input/capsense/ring")&&std::strcmp(path,"/input/capsense/pinky"))continue;
#endif
        uint32_t units=0;
        if(!reader.read(node+84,&units,4)||units>1)return {};
        out.components.push_back({key,path,units,node});
    }
    uint64_t endRoot=0,endComponentRoot=0;
    if(!read64(backend+280,endRoot)||endRoot!=root||
       !read64(table+16,endComponentRoot)||endComponentRoot!=componentRoot)return {};
    std::sort(out.components.begin(),out.components.end(),[](const auto& a,const auto& b){return a.handle<b.handle;});
    out.valid=true;return out;
}
template<class Reader>
Snapshot stableSnapshot(Reader& reader,uint64_t backend,uint32_t device){
    auto first=snapshotOnce(reader,backend,device);
    if(!first.valid)return {};
    auto second=snapshotOnce(reader,backend,device);
    if(!second.valid||first.components!=second.components)return {};
    return second;
}
// Last stored value, not an UpdateScalar event or a sensor timestamp. This
// runtime stores its post-normalization scalar at component node+116.
template<class Reader>
bool storedScalar(Reader& reader,const ComponentDescription& description,float& value){
    auto node=description.nodeAddress;
    if(!node||node>UINT64_MAX-256||description.path.size()>128)return false;
    uint64_t handle=0,text=0,length=0;uint32_t type=0,units=0,bits=0,again=0;
    char path[129]{};
    if(!reader.read(node+32,&handle,8)||handle!=description.handle||
       !reader.read(node+40,&text,8)||!reader.read(node+48,&length,8)||length!=description.path.size()||
       !reader.read(text,path,static_cast<size_t>(length)+1)||path[length]||
       std::memcmp(path,description.path.c_str(),static_cast<size_t>(length)+1)||
       !reader.read(node+72,&type,4)||type!=2||!reader.read(node+84,&units,4)||units!=description.units||units>1||
       !reader.read(node+116,&bits,4)||!reader.read(node+116,&again,4)||bits!=again||
       !reader.read(node+32,&handle,8)||handle!=description.handle)return false;
    std::memcpy(&value,&bits,4);
    return std::isfinite(value)&&value>=(units==0?0.f:-1.f)&&value<=1.f;
}
}
