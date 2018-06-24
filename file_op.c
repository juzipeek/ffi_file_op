#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/syscall.h>

#include "file_op.h"

/*******************************************************************************
 * 编译命令
 * gcc -g -o libluacallc.so -fPIC -shared fast_sort.c
 ******************************************************************************/

static __thread int g_init_flag = 0;    // 线程局部变量初始化标志
static __thread ffi_file_info g_all_files[MAX_FFI_FILE];
static char g_logger_config[1024];  // 日志配置字符串
static int  g_logger_config_len;     // 日志配置字符串长度

/*******************************************************************************
 * 辅助函数，保证目录存在
 *
 ******************************************************************************/
static int
keep_dir_exist(char *dir_path)
{
    char cmd[512] = { 0 };

    if (access(dir_path, F_OK) < 0) {
        snprintf(cmd, sizeof(cmd) - 1, "mkdir -p %s", dir_path);
        return system(cmd);
    }

    return 0;
}

/*******************************************************************************
 * export
 *
 ******************************************************************************/
int
file_op_init(char *cfg)
{
    memset(g_logger_config, 0x00, sizeof(g_logger_config));
    strncpy(g_logger_config, cfg, sizeof(g_logger_config) - 1);

    g_logger_config_len = strlen(g_logger_config);

    char cfg_str[1024] = {0};
    strcpy(cfg_str, g_logger_config);
    trace("logger_config[%s][%s]", g_logger_config, cfg_str);

    char *save_ptr1 = NULL;
    char *save_ptr2 = NULL;
    char token1[] = ";";
    char token2[] = ":";
    char *find_record = NULL;
    char *find_item = NULL;

    int ret = 0;

    for (find_record = strtok_r(cfg_str, token1, &save_ptr1);;find_record = strtok_r(NULL, token1, &save_ptr1)) {
        if (!find_record) {
            break;
        }
        // recorder name
        find_item = strtok_r(find_record, token2, &save_ptr2);
        if (!find_item)
            continue;

        // data file save dir
        find_item = strtok_r(NULL, token2, &save_ptr2);
        if (!find_item)
            continue;

        ret = keep_dir_exist(find_item);
        if (0 != ret) {
            trace("keep dir[%s] exist error[%d]!", find_item, ret);
            return -1;
        }
    }

    return 0;
}

/*******************************************************************************
 * 根据配置初始化每线程的 recorder
 * 日志以 `log_name_1:dir:prefix;log_name_2:dir:prefix` 形式配置，使用时将其分割
 ******************************************************************************/
static void
thread_recorder_init(int cfg_str_len)
{
    char *save_ptr1 = NULL;
    char *save_ptr2 = NULL;
    char token1[] = ";";
    char token2[] = ":";
    char *find_record = NULL;
    char *find_item = NULL;

    char cfg_str[cfg_str_len+1];

    strncpy(cfg_str, g_logger_config, cfg_str_len);
    cfg_str[cfg_str_len] = 0x00;

    memset(g_all_files, 0x00, sizeof(ffi_file_info) * MAX_FFI_FILE);
    trace("call thread_recorder_init,logger_config[%s][%s]", g_logger_config, cfg_str);

    int i = 0;
    for (find_record = strtok_r(cfg_str, token1, &save_ptr1);;find_record = strtok_r(NULL, token1, &save_ptr1)) {
        if (!find_record) {
            break;
        }

        // recorder name
        find_item = strtok_r(find_record, token2, &save_ptr2);
        if (!find_item)
            continue;
        memset(g_all_files[i].name, 0x00, sizeof(g_all_files[i].name));
        strncpy(g_all_files[i].name, find_item, sizeof(g_all_files[i].name) - 1);

        // data file save dir
        find_item = strtok_r(NULL, token2, &save_ptr2);
        if (!find_item)
            continue;
        memset(g_all_files[i].dir, 0x00, sizeof(g_all_files[i].dir));
        strncpy(g_all_files[i].dir, find_item, sizeof(g_all_files[i].dir) - 1);
        if ('/' != g_all_files[i].dir[strlen(g_all_files[i].dir) - 1])
            strcat(g_all_files[i].dir, "/");

        // data file prefix
        find_item = strtok_r(NULL, token2, &save_ptr2);
        if (!find_item)
            continue;
        memset(g_all_files[i].prefix, 0x00, sizeof(g_all_files[i].prefix));
        strncpy(g_all_files[i].prefix, find_item, sizeof(g_all_files[i].prefix) - 1);

        g_all_files[i].fd = -1;
        trace("logger[%s]dir[%s]prefix[%s]", g_all_files[i].name, g_all_files[i].dir, g_all_files[i].prefix);
        i++;
    }
}

/*******************************************************************************
 * 根据日志名获取日志文件信息
 ******************************************************************************/
static ffi_file_info *
ffi_get_file_info(char *name)
{
    int i;

    // 未初始化线程配置，需要进行初始化
    if (0 == g_init_flag) {
        thread_recorder_init(g_logger_config_len);
        g_init_flag = 1;
    }
    // find
    for (i = 0; i < MAX_FFI_FILE; i++) {
        if (!strcmp(name, g_all_files[i].name)) {
            return &g_all_files[i];
        }
    }

    return NULL;
}

/*******************************************************************************
 * 检查是否需要打开新日志文件
 * 根据当前时间与之前时间进行比较,如果不同则更新 file_info
 ******************************************************************************/
static int
ffi_file_check(ffi_file_info *file_info)
{
    // char now[DU_STAMP_LEN + 1] = { 0 };

    if (!file_info) {
        trace("file_info is NULL!");
        return -1;
    }

    time_t now;
    struct tm tm_now, tm_last;
    time(&now);
    localtime_r(&now, &tm_now);
    localtime_r(&file_info->file_ctime, &tm_last);

    if (tm_now.tm_hour != tm_last.tm_hour) {
        char file_path[512] = { 0 };
        trace("need update logger[%s]", file_info->name);

        // 关闭文件
        if (0 > file_info->fd) {
            close(file_info->fd);
        }

        memset(file_info->date_hour, 0x00, DU_STAMP_LEN);
        // YYYYMMDDHH:2017072014
        sprintf(file_info->date_hour, "%04d%02d%02d%02d", tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday, tm_now.tm_hour);

        // 文件名格式:"文件名前缀.日期时间.log"
        snprintf(file_path, sizeof(file_path) - 1, "%s%s.%s.data", file_info->dir, file_info->prefix, file_info->date_hour);

        trace("open file[%s]", file_path);

        file_info->fd = open(file_path, O_CREAT|O_WRONLY|O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
        if (file_info->fd < 0) {
            trace("open file [%s] error!", file_path);
            return -2;
        }

        // update
        file_info->file_ctime = now;
        trace("update logger[%s] success!", file_info->name);
    }

    return 0;
}

/*******************************************************************************
 * 写数据
 ******************************************************************************/
int
file_op_write(char *name, char *data, int data_len)
{
    int ret = 0;
    ffi_file_info *file_info = NULL;

    if (!name || !data) {
        trace("param error!");
        return -1;
    }

    file_info = ffi_get_file_info(name);
    if (!file_info) {
        trace("can not find logger[%s]", name);
        return -1;
    }
    // du_log(NGX_LOG_DEBUG, NULL, 0, "find logger[%s][%s-%s-%s]", name_str, file_info->dir, file_info->prefix, file_info->date_hour);

    /* 检查是否需要重新打开文件描述符 */
    ret = ffi_file_check(file_info);
    if (ret < 0) {
        trace("file check error!ret=%d", ret);
        return ret;
    }
    // du_log(NGX_LOG_DEBUG, NULL, 0, "file check ret=%d", ret);

    /* 写记录处理 */
    ret = write(file_info->fd, data, data_len);
    trace("write data success.ret=%d", ret);

    return ret;
}

