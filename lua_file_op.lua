local ffi = require"ffi"

--lua 数组映射到C层
ffi.cdef[[
/*
 * 配置文件初始化
 * @param cfg       日志配置;
 *      格式为： "日志名:日志目录:日志文件前缀"。
 *      例如配置“logger1:/tmp/:logger1;”将在 /tmp 目录下生成日志文件 logger1.2018062222.data。
 */
int file_op_init(char *cfg);

/*
 * 写入日志
 *
 * @param name      日志名
 * @param data      写入日志的数据
 * @param data_len  写入数据的长度
 */
int file_op_write(char *name, char *data, int data_len);
]]

local file_op = ffi.load('file_op')

local cfg = ffi.new("char[?]", 256)
ffi.copy(cfg, "logger1:/tmp/:logger1;logger2:/tmp:logger2")
local logger = ffi.new("char[?]", 128)
ffi.copy(logger, "logger1")

local msg_r = "hello world!\n"
local msg = ffi.new("char[?]", 1024)
ffi.copy(msg, msg_r)

file_op.file_op_init(cfg)
local ret = file_op.file_op_write(logger, msg, string.len(msg_r))
print(ret)

