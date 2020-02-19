#ifndef __FYLEE_UTIL_H__
#define __FYLEE_UTIL_H__

#include <cxxabi.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <string>
#include <iomanip>
#include <jsoncpp/json/json.h>
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <boost/lexical_cast.hpp>


namespace fylee {

pid_t GetThreadId();

uint32_t GetCoroId();

void Backtrace(std::vector<std::string>& bt, int size = 64, int skip = 1);

std::string BacktraceToString(int size = 64, int skip = 2, const std::string& prefix = "");

class FSUtil {
public:
    static void ListAllFile(std::vector<std::string>& files
                            ,const std::string& path
                            ,const std::string& subfix);
    static bool Mkdir(const std::string& dirname);
    static bool IsRunningPidfile(const std::string& pidfile);
    static bool Rm(const std::string& path);
    static bool Mv(const std::string& from, const std::string& to);
    static bool Realpath(const std::string& path, std::string& rpath);
    static bool Symlink(const std::string& frm, const std::string& to);
    static bool Unlink(const std::string& filename, bool exist = false);
    static std::string Dirname(const std::string& filename);
    static std::string Basename(const std::string& filename);
    static bool OpenForRead(std::ifstream& ifs, const std::string& filename
                    ,std::ios_base::openmode mode);
    static bool OpenForWrite(std::ofstream& ofs, const std::string& filename
                    ,std::ios_base::openmode mode);
};

template<class V, class Map, class K>
V GetParamValue(const Map& m, const K& k, const V& def = V()) {
    auto it = m.find(k);
    if(it == m.end()) {
        return def;
    }
    try {
        return boost::lexical_cast<V>(it->second);
    } catch (...) {
    }
    return def;
}

template<class V, class Map, class K>
bool CheckGetParamValue(const Map& m, const K& k, V& v) {
    auto it = m.find(k);
    if(it == m.end()) {
        return false;
    }
    try {
        v = boost::lexical_cast<V>(it->second);
        return true;
    } catch (...) {
    }
    return false;
}

class TypeUtil {
public:
    static int8_t ToChar(const std::string& str);
    static int64_t Atoi(const std::string& str);
    static double Atof(const std::string& str);
    static int8_t ToChar(const char* str);
    static int64_t Atoi(const char* str);
    static double Atof(const char* str);
};

class Atomic {
public:
    template<class T, class S>
    static T addFetch(volatile T& t, S v = 1) {
        return __sync_add_and_fetch(&t, (T)v);
    }

    template<class T, class S>
    static T subFetch(volatile T& t, S v = 1) {
        return __sync_sub_and_fetch(&t, (T)v);
    }

    template<class T, class S>
    static T orFetch(volatile T& t, S v) {
        return __sync_or_and_fetch(&t, (T)v);
    }

    template<class T, class S>
    static T andFetch(volatile T& t, S v) {
        return __sync_and_and_fetch(&t, (T)v);
    }

    template<class T, class S>
    static T xorFetch(volatile T& t, S v) {
        return __sync_xor_and_fetch(&t, (T)v);
    }

    template<class T, class S>
    static T nandFetch(volatile T& t, S v) {
        return __sync_nand_and_fetch(&t, (T)v);
    }

    template<class T, class S>
    static T fetchAdd(volatile T& t, S v = 1) {
        return __sync_fetch_and_add(&t, (T)v);
    }

    template<class T, class S>
    static T fetchSub(volatile T& t, S v = 1) {
        return __sync_fetch_and_sub(&t, (T)v);
    }

    template<class T, class S>
    static T fetchOr(volatile T& t, S v) {
        return __sync_fetch_and_or(&t, (T)v);
    }

    template<class T, class S>
    static T fetchAnd(volatile T& t, S v) {
        return __sync_fetch_and_and(&t, (T)v);
    }

    template<class T, class S>
    static T fetchXor(volatile T& t, S v) {
        return __sync_fetch_and_xor(&t, (T)v);
    }

    template<class T, class S>
    static T fetchNand(volatile T& t, S v) {
        return __sync_fetch_and_nand(&t, (T)v);
    }

    template<class T, class S>
    static T compareAndSwap(volatile T& t, S old_val, S new_val) {
        return __sync_val_compare_and_swap(&t, (T)old_val, (T)new_val);
    }

    template<class T, class S>
    static bool compareAndSwapBool(volatile T& t, S old_val, S new_val) {
        return __sync_bool_compare_and_swap(&t, (T)old_val, (T)new_val);
    }
};

template<class T>
void nop(T*) {}

template<class T>
void delete_array(T* v) {
    if(v) {
        delete[] v;
    }
}

class StringUtil {
public:
    static std::string Format(const char* fmt, ...);
    static std::string Formatv(const char* fmt, va_list ap);

    static std::string UrlEncode(const std::string& str, bool space_as_plus = true);
    static std::string UrlDecode(const std::string& str, bool space_as_plus = true);

    static std::string Trim(const std::string& str, const std::string& delimit = " \t\r\n");
    static std::string TrimLeft(const std::string& str, const std::string& delimit = " \t\r\n");
    static std::string TrimRight(const std::string& str, const std::string& delimit = " \t\r\n");


    static std::string WStringToString(const std::wstring& ws);
    static std::wstring StringToWString(const std::string& s);

};

class JsonUtil {
public:
    static bool NeedEscape(const std::string& v);
    static std::string Escape(const std::string& v);
    static std::string GetString(const Json::Value& json
                          ,const std::string& name
                          ,const std::string& default_value = "");
    static double GetDouble(const Json::Value& json
                     ,const std::string& name
                     ,double default_value = 0);
    static int32_t GetInt32(const Json::Value& json
                     ,const std::string& name
                     ,int32_t default_value = 0);
    static uint32_t GetUint32(const Json::Value& json
                       ,const std::string& name
                       ,uint32_t default_value = 0);
    static int64_t GetInt64(const Json::Value& json
                     ,const std::string& name
                     ,int64_t default_value = 0);
    static uint64_t GetUint64(const Json::Value& json
                       ,const std::string& name
                       ,uint64_t default_value = 0);
    static bool FromString(Json::Value& json, const std::string& v);
    static std::string ToString(const Json::Value& json);
};

uint64_t GetCurrentMS();
uint64_t GetCurrentUS();
std::string Time2Str(time_t ts, const std::string& format);
std::string GetHostName();
std::string GetIPv4();
bool YamlToJson(const YAML::Node& ynode, Json::Value& jnode);
bool JsonToYaml(const Json::Value& jnode, YAML::Node& ynode);
template<class T>
const char* TypeToName() {
    static const char* name = abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, nullptr);
    return name;
}
template<class Iter>
std::string Join(Iter begin, Iter end, const std::string& tag) {
    std::stringstream ss;
    for(Iter it = begin; it != end; ++it) {
        if(it != begin) {
            ss << tag;
        }
        ss << *it;
    }
    return ss.str();
}

}

#endif
