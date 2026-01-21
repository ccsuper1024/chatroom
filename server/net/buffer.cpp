#include "net/buffer.h"
#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

const char Buffer::kCRLF[] = "\r\n";

ssize_t Buffer::readFd(int fd, int* savedErrno) {
    // 节省一次 ioctl/FIONREAD 系统调用
    char extrabuf[65536];
    struct iovec vec[2];
    const size_t writable = writableBytes();
    
    // 第一块缓冲区：Buffer 内部的可写空间
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;
    
    // 第二块缓冲区：栈上的临时空间 (64KB)
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;
    
    // 如果 Buffer 剩余空间足够大，就不需要第二块了
    // 这里的逻辑是：如果 writable < 64KB，我们启用第二块；否则只用第一块
    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
    
    const ssize_t n = ::readv(fd, vec, iovcnt);
    if (n < 0) {
        *savedErrno = errno;
    } else if (static_cast<size_t>(n) <= writable) {
        writerIndex_ += n;
    } else {
        // 第一块写满了，剩余的在 extrabuf 里
        writerIndex_ = buffer_.size();
        append(extrabuf, n - writable);
    }
    
    return n;
}
