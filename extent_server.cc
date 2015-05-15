// the extent server implementation

#include "extent_server.h"
#include "rpc/marshall.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctime>

extent_server::extent_server(): rand_(std::random_device()()){
    extents_[1] = std::make_shared<extent_entry>(1, 0, "", 0);
}


int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &ret) {
    // You fill this in for Lab 2.
    bool is_file = isfile(id);
    std::unique_lock<std::mutex> m_(mtx_);
    auto iter = extents_.find(id);
    if (iter == extents_.end()){
        // New entry
        return extent_protocol::NOENT;
//        extents_[id] = std::make_shared<extent_entry>(id, 0, buf, 0);
    } else {
//         Old entry, update name and mtime, atime
//         No, for old entry we just return EXIST
        iter->second->name = std::move(buf);
        (iter->second->attr).mtime = (iter->second->attr).atime = std::time(nullptr);
    }
    return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf) {
    // You fill this in for Lab 2.
    std::unique_lock<std::mutex> m_(mtx_);
    auto iter = extents_.find(id);
    if (iter != extents_.end()){
        buf = iter->second->name;
        (iter->second->attr).atime = std::time(nullptr);
        return extent_protocol::OK;
    }
    return extent_protocol::NOENT;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a) {
    // You fill this in for Lab 2.
    // You replace this with a real implementation. We send a phony response
    // for now because it's difficult to get FUSE to do anything (including
    // unmount) if getattr fails.
    std::unique_lock<std::mutex> m_(mtx_);
    auto it = extents_.find(id);
    if (it != extents_.end()){
        a = it->second->attr;
        return extent_protocol::OK;
    }
    return extent_protocol::NOENT;
}

int extent_server::remove(extent_protocol::extentid_t id, int &) {
    // You fill this in for Lab 2.
    std::unique_lock<std::mutex> m_(mtx_);
    auto it = extents_.find(id);
    if (it != extents_.end()){
        auto piter = extents_.find(it->second->parent_id);
        for (auto lstiter = piter->second->chd.begin(); lstiter != piter->second->chd.end(); ++lstiter){
            if (lstiter->eid == id){
                piter->second->chd.erase(lstiter);
//                VERIFY(piter->second->no_chd >= 0);
                break;
            }
        }
        extents_.erase(it);
        return extent_protocol::OK;
    }
    return extent_protocol::NOENT;
}

int extent_server::create( extent_protocol::extentid_t pid, std::string name, extent_protocol::extentid_t &ret) {
    printf("create for %llu/%s\n", pid, name.data());

    std::unique_lock<std::mutex> m_(mtx_);
    auto piter = extents_.find(pid);
    if (piter == extents_.end() || isfile(pid)){
        return extent_protocol::IOERR;
    }

    std::list<dir_ent> &chdlst = piter->second->chd;
    auto lstiter = chdlst.cbegin();
    for (; lstiter != chdlst.cend(); ++lstiter){
        if (lstiter->name == name){
            return extent_protocol::EXIST;
        } else if (lstiter->name > name){
            break;
        }
    }

    for (int i = 0; i < 10; ++i){
        auto id = rand_();
        id |= 0x80000000;
        if (extents_.find(id) == extents_.end()){
            extents_[id] = std::make_shared<extent_entry>(id, 0, name, 0);
            ret = id;
            chdlst.insert(lstiter, dir_ent(name, id));
            printf("create ok");
            return extent_protocol::OK;
        }
    }
    return extent_protocol::IOERR;
}

int extent_server::lookup( extent_protocol::extentid_t pid, std::string name, extent_protocol::extentid_t &ret) {

    printf("lookup for %llu/%s\n", pid, name.data());
    std::unique_lock<std::mutex> m_(mtx_);
    auto piter = extents_.find(pid);
    if (piter == extents_.end()){
        return 0;
    }

    std::list<dir_ent> &chdlst = piter->second->chd;
    auto lstiter = chdlst.cbegin();
    for (; lstiter != chdlst.cend(); ++lstiter){
        if (lstiter->name == name){
            ret = lstiter->eid;
            printf("lookup for %llu , \n");
            return 1;
        } else if (lstiter->name > name){
            return 0;
        }
    }

    return false;
}

int extent_server::readdir(extent_protocol::extentid_t pid, std::map<std::string, extent_protocol::extentid_t> &ents){
    std::unique_lock<std::mutex> m_(mtx_);
    auto piter = extents_.find(pid);
    if (piter == extents_.end() || isfile(pid)){
        return extent_protocol::NOENT;
    }

    std::list<dir_ent> &chdlst = piter->second->chd;
    for (auto &item : chdlst){
        ents[item.name] = item.eid;
    }

    return extent_protocol::OK;
}
//