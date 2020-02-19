#include "buffer.h"
#include <fstream>
#include <sstream>
#include <string.h>
#include <iomanip>

#include "endian.h"
#include "log.h"

namespace fylee {

static fylee::Logger::ptr g_logger = LOG_NAME("system");

Buffer::Node::Node(size_t s)
    :ptr(new char[s]), 
     next(nullptr), 
     size(s) {
}

Buffer::Node::Node()
    :ptr(nullptr)
    ,next(nullptr)
    ,size(0) {
}

Buffer::Node::~Node() {
    if(ptr) {
        delete[] ptr;
    }
}

Buffer::Buffer(size_t base_size)
    :baseSize_(base_size)
    ,position_(0)
    ,capacity_(base_size)
    ,size_(0)
    ,endian_(FYLEE_LITTLE_ENDIAN)
    ,root_(new Node(base_size))
    ,curr_(root_) {
}

Buffer::~Buffer() {
    Node* tmp = root_;
    while(tmp) {
        curr_ = tmp;
        tmp = tmp->next;
        delete curr_;
    }
}

bool Buffer::isLittleEndian() const {
    return endian_ == FYLEE_LITTLE_ENDIAN;
}

void Buffer::setIsLittleEndian(bool val) {
    if(val) {
        endian_ = FYLEE_LITTLE_ENDIAN;
    } else {
        endian_ = FYLEE_BIG_ENDIAN;
    }
}

void Buffer::writeFint8  (int8_t value) {
    write(&value, sizeof(value));
}

void Buffer::writeFuint8 (uint8_t value) {
    write(&value, sizeof(value));
}

#define WRITE(value) \
    if(endian_ != FYLEE_BYTE_ORDER) { \
        value = byteswap(value); \
    } \
    write(&value, sizeof(value)); 

void Buffer::writeFint16 (int16_t value) {
    WRITE(value);
}

void Buffer::writeFuint16(uint16_t value) {
    WRITE(value);
}

void Buffer::writeFint32 (int32_t value) {
    WRITE(value);
}

void Buffer::writeFuint32(uint32_t value) {
    WRITE(value);
}

void Buffer::writeFint64 (int64_t value) {
    WRITE(value);
}

void Buffer::writeFuint64(uint64_t value) {
    WRITE(value);
}

#undef WRITE
static uint32_t EncodeZigzag32(const int32_t& v) {
    if(v < 0) {
        return ((uint32_t)(-v)) * 2 - 1;
    } else {
        return v * 2;
    }
}

static uint64_t EncodeZigzag64(const int64_t& v) {
    if(v < 0) {
        return ((uint64_t)(-v)) * 2 - 1;
    } else {
        return v * 2;
    }
}

static int32_t DecodeZigzag32(const uint32_t& v) {
    return (v >> 1) ^ -(v & 1);
}

static int64_t DecodeZigzag64(const uint64_t& v) {
    return (v >> 1) ^ -(v & 1);
}


void Buffer::writeInt32  (int32_t value) {
    writeUint32(EncodeZigzag32(value));
}

void Buffer::writeUint32 (uint32_t value) {
    uint8_t tmp[5];
    uint8_t i = 0;
    while(value >= 0x80) {
        tmp[i++] = (value & 0x7F) | 0x80;
        value >>= 7;
    }
    tmp[i++] = value;
    write(tmp, i);
}

void Buffer::writeInt64(int64_t value) {
    writeUint64(EncodeZigzag64(value));
}

void Buffer::writeUint64(uint64_t value) {
    uint8_t tmp[10];
    uint8_t i = 0;
    while(value >= 0x80) {
        tmp[i++] = (value & 0x7F) | 0x80;
        value >>= 7;
    }
    tmp[i++] = value;
    write(tmp, i);
}

void Buffer::writeFloat(float value) {
    uint32_t v;
    memcpy(&v, &value, sizeof(value));
    writeFuint32(v);
}

void Buffer::writeDouble(double value) {
    uint64_t v;
    memcpy(&v, &value, sizeof(value));
    writeFuint64(v);
}

void Buffer::writeStringF16(const std::string& value) {
    writeFuint16(value.size());
    write(value.c_str(), value.size());
}

void Buffer::writeStringF32(const std::string& value) {
    writeFuint32(value.size());
    write(value.c_str(), value.size());
}

void Buffer::writeStringF64(const std::string& value) {
    writeFuint64(value.size());
    write(value.c_str(), value.size());
}

void Buffer::writeStringVint(const std::string& value) {
    writeUint64(value.size());
    write(value.c_str(), value.size());
}

void Buffer::writeStringWithoutLength(const std::string& value) {
    write(value.c_str(), value.size());
}

int8_t   Buffer::readFint8() {
    int8_t v;
    read(&v, sizeof(v));
    return v;
}

uint8_t  Buffer::readFuint8() {
    uint8_t v;
    read(&v, sizeof(v));
    return v;
}

#define READ(type) \
    type v; \
    read(&v, sizeof(v)); \
    if(endian_ == FYLEE_BYTE_ORDER) { \
        return v; \
    } else { \
        return byteswap(v); \
    }

int16_t  Buffer::readFint16() {
    READ(int16_t);
}
uint16_t Buffer::readFuint16() {
    READ(uint16_t);
}

int32_t  Buffer::readFint32() {
    READ(int32_t);
}

uint32_t Buffer::readFuint32() {
    READ(uint32_t);
}

int64_t  Buffer::readFint64() {
    READ(int64_t);
}

uint64_t Buffer::readFuint64() {
    READ(uint64_t);
}

#undef READ

int32_t  Buffer::readInt32() {
    return DecodeZigzag32(readUint32());
}

uint32_t Buffer::readUint32() {
    uint32_t result = 0;
    for(int i = 0; i < 32; i += 7) {
        uint8_t b = readFuint8();
        if(b < 0x80) {
            result |= ((uint32_t)b) << i;
            break;
        } else {
            result |= (((uint32_t)(b & 0x7f)) << i);
        }
    }
    return result;
}

int64_t Buffer::readInt64() {
    return DecodeZigzag64(readUint64());
}

uint64_t Buffer::readUint64() {
    uint64_t result = 0;
    for(int i = 0; i < 64; i += 7) {
        uint8_t b = readFuint8();
        if(b < 0x80) {
            result |= ((uint64_t)b) << i;
            break;
        } else {
            result |= (((uint64_t)(b & 0x7f)) << i);
        }
    }
    return result;
}

float Buffer::readFloat() {
    uint32_t v = readFuint32();
    float value;
    memcpy(&value, &v, sizeof(v));
    return value;
}

double Buffer::readDouble() {
    uint64_t v = readFuint64();
    double value;
    memcpy(&value, &v, sizeof(v));
    return value;
}

std::string Buffer::readStringF16() {
    uint16_t len = readFuint16();
    std::string buff;
    buff.resize(len);
    read(&buff[0], len);
    return buff;
}

std::string Buffer::readStringF32() {
    uint32_t len = readFuint32();
    std::string buff;
    buff.resize(len);
    read(&buff[0], len);
    return buff;
}

std::string Buffer::readStringF64() {
    uint64_t len = readFuint64();
    std::string buff;
    buff.resize(len);
    read(&buff[0], len);
    return buff;
}

std::string Buffer::readStringVint() {
    uint64_t len = readUint64();
    std::string buff;
    buff.resize(len);
    read(&buff[0], len);
    return buff;
}

void Buffer::clear() {
    position_ = size_ = 0;
    capacity_ = baseSize_;
    Node* tmp = root_->next;
    while(tmp) {
        curr_ = tmp;
        tmp = tmp->next;
        delete curr_;
    }
    curr_ = root_;
    root_->next = NULL;
}

void Buffer::write(const void* buf, size_t size) {
    if(size == 0) {
        return;
    }
    addCapacity(size);

    size_t npos = position_ % baseSize_;
    size_t ncap = curr_->size - npos;
    size_t bpos = 0;

    while(size > 0) {
        if(ncap >= size) { // 当前内存块够用，一次写完
            memcpy(curr_->ptr + npos, (const char*)buf + bpos, size);
            if(curr_->size == (npos + size)) { // 写满则指向下一块
                curr_ = curr_->next;
            }
            position_ += size; // 偏移量增加
            bpos += size;
            size = 0;
        } else { // 当前内存块不够用，分段写
            memcpy(curr_->ptr + npos, (const char*)buf + bpos, ncap); // 先写满当前内存块
            position_ += ncap;
            bpos += ncap;
            size -= ncap;
            curr_ = curr_->next; // 指向下一块
            ncap = curr_->size;
            npos = 0;
        }
    }

    if(position_ > size_) {
        size_ = position_;
    }
}

void Buffer::read(void* buf, size_t size) {
    if(size > getReadSize()) {
        throw std::out_of_range("not enough len");
    }

    size_t npos = position_ % baseSize_;
    size_t ncap = curr_->size - npos;
    size_t bpos = 0;
    while(size > 0) {
        if(ncap >= size) { // 当前内存块足够读，一次读完
            memcpy((char*)buf + bpos, curr_->ptr + npos, size);
            if(curr_->size == (npos + size)) {
                curr_ = curr_->next;
            }
            position_ += size;
            bpos += size;
            size = 0;
        } else { // 当前内存块不够读，分段读
            memcpy((char*)buf + bpos, curr_->ptr + npos, ncap); // 先读当前剩余部分
            position_ += ncap;
            bpos += ncap;
            size -= ncap;
            curr_ = curr_->next; // 指向下一块继续读
            ncap = curr_->size;
            npos = 0;
        }
    }
}

void Buffer::read(void* buf, size_t size, size_t position) const { // 从指定位置开始读
    if(size > (size_ - position)) {
        throw std::out_of_range("not enough len");
    }

    size_t npos = position % baseSize_;
    size_t ncap = curr_->size - npos;
    size_t bpos = 0;
    Node* cur = curr_;
    while(size > 0) {
        if(ncap >= size) {
            memcpy((char*)buf + bpos, cur->ptr + npos, size);
            if(cur->size == (npos + size)) {
                cur = cur->next;
            }
            position += size;
            bpos += size;
            size = 0;
        } else {
            memcpy((char*)buf + bpos, cur->ptr + npos, ncap);
            position += ncap;
            bpos += ncap;
            size -= ncap;
            cur = cur->next;
            ncap = cur->size;
            npos = 0;
        }
    }
}

void Buffer::setPosition(size_t v) {
    if(v > capacity_) {
        throw std::out_of_range("set_position out of range");
    }
    position_ = v;
    if(position_ > size_) {
        size_ = position_;
    }
    curr_ = root_;
    while(v > curr_->size) {
        v -= curr_->size;
        curr_ = curr_->next;
    }
    if(v == curr_->size) {
        curr_ = curr_->next;
    }
}

bool Buffer::writeToFile(const std::string& name) const {
    std::ofstream ofs;
    ofs.open(name, std::ios::trunc | std::ios::binary);
    if(!ofs) {
        LOG_ERROR(g_logger) << "writeToFile name=" << name
            << " error , errno=" << errno << " errstr=" << strerror(errno);
        return false;
    }

    int64_t read_size = getReadSize();
    int64_t pos = position_;
    Node* cur = curr_;

    while(read_size > 0) {
        int diff = pos % baseSize_;
        int64_t len = (read_size > (int64_t)baseSize_ ? baseSize_ : read_size) - diff;
        ofs.write(cur->ptr + diff, len);
        cur = cur->next;
        pos += len;
        read_size -= len;
    }

    return true;
}

bool Buffer::readFromFile(const std::string& name) {
    std::ifstream ifs;
    ifs.open(name, std::ios::binary);
    if(!ifs) {
        LOG_ERROR(g_logger) << "readFromFile name=" << name
            << " error, errno=" << errno << " errstr=" << strerror(errno);
        return false;
    }

    std::shared_ptr<char> buff(new char[baseSize_], [](char* ptr) { delete[] ptr;});
    while(!ifs.eof()) {
        ifs.read(buff.get(), baseSize_);
        write(buff.get(), ifs.gcount());
    }
    return true;
}

void Buffer::addCapacity(size_t size) {
    if(size == 0) {
        return;
    }
    size_t old_cap = getCapacity();
    if(old_cap >= size) {
        return;
    }

    size = size - old_cap;
    size_t count = ceil(1.0 * size / baseSize_); // 需要增加的内存块数量
    Node* tmp = root_;
    while(tmp->next) {
        tmp = tmp->next;
    }

    Node* first = NULL;
    for(size_t i = 0; i < count; ++i) {
        tmp->next = new Node(baseSize_);
        if(first == NULL) {
            first = tmp->next;
        }
        tmp = tmp->next;
        capacity_ += baseSize_;
    }

    if(old_cap == 0) {
        curr_ = first;
    }
}

std::string Buffer::toString() const {
    std::string str;
    str.resize(getReadSize());
    if(str.empty()) {
        return str;
    }
    read(&str[0], str.size(), position_);
    return str;
}

std::string Buffer::toHexString() const {
    std::string str = toString();
    std::stringstream ss;

    for(size_t i = 0; i < str.size(); ++i) {
        if(i > 0 && i % 32 == 0) {
            ss << std::endl;
        }
        ss << std::setw(2) << std::setfill('0') << std::hex
           << (int)(uint8_t)str[i] << " ";
    }

    return ss.str();
}


uint64_t Buffer::getReadBuffers(std::vector<iovec>& buffers, uint64_t len) const {
    len = len > getReadSize() ? getReadSize() : len;
    if(len == 0) {
        return 0;
    }

    uint64_t size = len;

    size_t npos = position_ % baseSize_;
    size_t ncap = curr_->size - npos;
    struct iovec iov;
    Node* cur = curr_;

    while(len > 0) {
        if(ncap >= len) {
            iov.iov_base = cur->ptr + npos;
            iov.iov_len = len;
            len = 0;
        } else {
            iov.iov_base = cur->ptr + npos;
            iov.iov_len = ncap;
            len -= ncap;
            cur = cur->next;
            ncap = cur->size;
            npos = 0;
        }
        buffers.push_back(iov);
    }
    return size;
}

uint64_t Buffer::getReadBuffers(std::vector<iovec>& buffers, 
                                   uint64_t len, uint64_t position) const {
    len = len > getReadSize() ? getReadSize() : len;
    if(len == 0) {
        return 0;
    }

    uint64_t size = len;

    size_t npos = position % baseSize_;
    size_t count = position / baseSize_;
    Node* cur = root_;
    while(count > 0) {
        cur = cur->next;
        --count;
    }

    size_t ncap = cur->size - npos;
    struct iovec iov;
    while(len > 0) {
        if(ncap >= len) {
            iov.iov_base = cur->ptr + npos;
            iov.iov_len = len;
            len = 0;
        } else {
            iov.iov_base = cur->ptr + npos;
            iov.iov_len = ncap;
            len -= ncap;
            cur = cur->next;
            ncap = cur->size;
            npos = 0;
        }
        buffers.push_back(iov);
    }
    return size;
}

uint64_t Buffer::getWriteBuffers(std::vector<iovec>& buffers, uint64_t len) {
    if(len == 0) {
        return 0;
    }
    addCapacity(len);
    uint64_t size = len;

    size_t npos = position_ % baseSize_;
    size_t ncap = curr_->size - npos;
    struct iovec iov;
    Node* cur = curr_;
    while(len > 0) {
        if(ncap >= len) {
            iov.iov_base = cur->ptr + npos;
            iov.iov_len = len;
            len = 0;
        } else {
            iov.iov_base = cur->ptr + npos;
            iov.iov_len = ncap;
            len -= ncap;
            cur = cur->next;
            ncap = cur->size;
            npos = 0;
        }
        buffers.push_back(iov);
    }
    return size;
}

}
