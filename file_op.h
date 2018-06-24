#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

#define gettid()        syscall(SYS_gettid)

#define MAX_FFI_FILE  256
#define DU_STAMP_LEN  10    /* 时间戳长度 */

#if DEBUG
#define trace(fmt, args...) printf("[TID:%5ld][%8s : %03d]  " fmt "\n", (long)gettid(), __FILE__, __LINE__, ##args)
#else
#define trace(fmt, args...)
#endif

typedef struct ffi_file_info_t {
    int fd;                         // 文件描述符
    char name[41];                  // logger 名, id
    char dir[256];                  // 文件保存目录
    char prefix[128];               // 文件名前缀
    char date_hour[DU_STAMP_LEN+1]; // 当前小时
    time_t file_ctime;              // 文件创建时间
} ffi_file_info;


