#ifndef __FYLEE_BUFFER_H__
#define __FYLEE_BUFFER_H__

#include <memory>
#include <string>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <vector>

namespace fylee {

class Buffer {
public:
    typedef std::shared_ptr<Buffer> ptr;

    struct Node {
        Node(size_t s);
        Node();
        ~Node();
        char* ptr;
        Node* next;
        size_t size;
    };

    Buffer(size_t base_size = 4096);

    ~Buffer();

    void clear();

    void write(const void* buf, size_t size);

    void read(void* buf, size_t size);

    void read(void* buf, size_t size, size_t position) const;

    size_t getPosition() const { return position_;}

    void setPosition(size_t v);

    bool writeToFile(const std::string& name) const;

  
    bool readFromFile(const std::string& name);

  
    size_t getBaseSize() const { return baseSize_;}

    size_t getReadSize() const { return size_ - position_;}

    bool isLittleEndian() const;

    void setIsLittleEndian(bool val);
  
    std::string toString() const;
    
    std::string toHexString() const;

    uint64_t getReadBuffers(std::vector<iovec>& buffers, uint64_t len = ~0ull) const;

    uint64_t getReadBuffers(std::vector<iovec>& buffers, uint64_t len, uint64_t position) const;

    uint64_t getWriteBuffers(std::vector<iovec>& buffers, uint64_t len);

    size_t getSize() const { return size_;}
private:

    void addCapacity(size_t size);

    size_t getCapacity() const { return capacity_ - position_;}
private:
    /// 内存块的大小
    size_t baseSize_;
    /// 当前操作位置
    size_t position_;
    /// 当前的总容量
    size_t capacity_;
    /// 当前数据的大小
    size_t size_;
    /// 第一个内存块指针(头节点)
    Node* root_;
    /// 当前操作的内存块指针
    Node* curr_;
};

}

#endif
