#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fnmatch.h>
#include <unistd.h>
#include <time.h>

// 用于存储HTTP响应数据
struct MemoryStruct {
    char *memory;
    size_t size;
};

// 错误码定义
typedef enum {
    ERR_OK = 0,
    ERR_MEMORY = -1,
    ERR_CURL_INIT = -2,
    ERR_CURL_PERFORM = -3,
    ERR_HTTP_ERROR = -4,
    ERR_JSON_PARSE = -5,
    ERR_JSON_TYPE = -6,
    ERR_CONFIG = -7,
    ERR_FILE_IO = -8,
    ERR_INVALID_PATH = -9,
    ERR_NOT_FOUND = -10,
    ERR_RETRY_EXHAUSTED = -11
} ErrorCode;

// 日志级别定义
typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO = 1,
    LOG_WARN = 2,
    LOG_ERROR = 3,
    LOG_FATAL = 4
} LogLevel;

typedef struct {
    const char *owner;
    const char *repo;
    const char *token;
    char *release_id;
    const char *tag_name;
    int owner_allocated;  // 标记是否动态分配
    int repo_allocated;   // 标记是否动态分配
    int token_allocated;  // 标记是否动态分配
} Config;

// 全局日志级别，可以通过环境变量 MANAGE_LOG_LEVEL 设置
static LogLevel global_log_level = LOG_INFO;

// 统一的日志函数
static void log_message(LogLevel level, const char *fmt, ...) {
    static const char *level_strs[] = {"DEBUG", "INFO", "WARN", "ERROR", "FATAL"};

    // 检查是否应该输出该级别的日志
    if (level < global_log_level) {
        return;
    }

    va_list args;
    va_start(args, fmt);

    fprintf(stderr, "[%s] ", level_strs[level]);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    va_end(args);

    // 如果是 FATAL 级别，直接退出
    if (level == LOG_FATAL) {
        exit(1);
    }
}

// 便捷日志宏
#define log_debug(...) log_message(LOG_DEBUG, __VA_ARGS__)
#define log_info(...) log_message(LOG_INFO, __VA_ARGS__)
#define log_warn(...) log_message(LOG_WARN, __VA_ARGS__)
#define log_error(...) log_message(LOG_ERROR, __VA_ARGS__)
#define log_fatal(...) log_message(LOG_FATAL, __VA_ARGS__)

// 重试配置
#define MAX_RETRIES 3  // 最大重试次数

// 函数原型声明
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp);
static struct curl_slist* setGithubHeaders(const char *token, const char *content_type);
static ErrorCode getAssets(struct MemoryStruct *chunk, const Config *config);
static ErrorCode getLatestReleaseId(Config *config);
static ErrorCode deleteAsset(const char *assetId, const char *assetName, const Config *config);
static ErrorCode uploadFile(const char *filePath, const Config *config);
static ErrorCode deleteFile(const char *fileName, const Config *config);
static ErrorCode listFiles(const Config *config);
static const char* getFilenameFromPath(const char *path);
static char* readFileToBuffer(const char *filename, long *file_size);
static char* create_url(const char *format, ...);
static ErrorCode validate_config(const Config *config);
static int matchWildcard(const char *pattern, const char *string);
static ErrorCode uploadMultipleFiles(int fileCount, char **filePaths, const Config *config);
static ErrorCode deleteMultipleFiles(int fileCount, char **fileNames, const Config *config);
static ErrorCode updateFile(const char *filePath, const Config *config);
static ErrorCode updateMultipleFiles(int fileCount, char **filePaths, const Config *config);
static int expandWildcards(const char *pattern, char ***result);
static void showUsage(void);
static void showDetailedUsage(void);
static ErrorCode createRelease(const char *tag_name, const char *release_name, const char *description,
                               int is_prerelease, const Config *config, char **out_release_id);

// 重试机制相关
static ErrorCode performWithRetry(ErrorCode (*operation)(const void *), const void *param,
                                  int maxRetries, const char *opName);
static ErrorCode retryableOperationWrapper(const void *param);
static ErrorCode uploadFileWithRetry(const char *filePath, const Config *config, int maxRetries);
static ErrorCode deleteFileWithRetry(const char *fileName, const Config *config, int maxRetries);
static ErrorCode updateFileWithRetry(const char *filePath, const Config *config, int maxRetries);
static int shouldRetryError(ErrorCode error);

// 包装器参数结构体
typedef struct {
    ErrorCode (*operation)(const char *, const Config *);
    const char *param1;
    const Config *config;
    const char *opName;
} RetryWrapperParam;

// 从文件安全地读取token
static char* readTokenFromFile(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        log_error("无法打开token文件: %s", filename);
        return NULL;
    }

    // 检查文件权限，确保不是所有人可读
    struct stat st;
    if (stat(filename, &st) == 0) {
        if (st.st_mode & (S_IRWXG | S_IRWXO)) {
            log_warn("警告：token文件权限过于宽松，建议设置为 600");
        }
    }

    char buffer[512];
    if (fgets(buffer, sizeof(buffer), fp) == NULL) {
        log_error("无法从token文件读取数据: %s", filename);
        fclose(fp);
        return NULL;
    }

    fclose(fp);

    // 移除换行符和空白字符
    buffer[strcspn(buffer, "\r\n")] = '\0';

    // 复制token到堆内存
    char *token = strdup(buffer);
    if (!token) {
        log_error("内存分配失败");
        return NULL;
    }

    // 安全擦除原始buffer
    memset(buffer, 0, sizeof(buffer));

    return token;
}

// 从环境变量获取配置信息
static ErrorCode getConfig(Config *config) {
    if (!config) {
        log_error("配置对象为空");
        return ERR_CONFIG;
    }

    // 初始化日志级别
    const char *log_level_env = getenv("MANAGE_LOG_LEVEL");
    if (log_level_env) {
        int level = atoi(log_level_env);
        if (level >= LOG_DEBUG && level <= LOG_FATAL) {
            global_log_level = level;
            log_debug("日志级别设置为: %d", level);
        }
    }

    // 获取 GitHub token，优先从 GITHUB_TOKEN 环境变量获取
    config->token = getenv("GITHUB_TOKEN");
    config->token_allocated = 0;  // 初始化为环境变量

    // 如果环境变量没设置，尝试从文件读取
    if (!config->token) {
        const char *token_file = getenv("GITHUB_TOKEN_FILE");
        if (token_file) {
            log_debug("从文件读取 token: %s", token_file);
            char *token = readTokenFromFile(token_file);
            if (token) {
                config->token = token;
                config->token_allocated = 1;  // 标记为动态分配
                log_debug("Token 文件读取成功");
            } else {
                log_error("无法从token文件读取");
                fprintf(stderr, "错误：无法从token文件读取: %s\n", token_file);
                return ERR_CONFIG;
            }
        }
    }

    // 如果仍然无法获取token，返回错误
    if (!config->token) {
        log_error("未设置 GitHub token");
        fprintf(stderr, "错误：未设置 GITHUB_TOKEN 环境变量\n");
        fprintf(stderr, "可以使用以下方式之一设置:\n");
        fprintf(stderr, "  1. export GITHUB_TOKEN=your_token_here\n");
        fprintf(stderr, "  2. export GITHUB_TOKEN_FILE=/path/to/token_file\n");
        fprintf(stderr, "  然后将token写入文件并设置权限: chmod 600 token_file\n");
        return ERR_CONFIG;
    }

    // 获取 owner
    config->owner = getenv("GITHUB_OWNER");
    config->owner_allocated = 0;
    if (!config->owner) {
        config->owner = strdup("nostalgia296");
        config->owner_allocated = 1;
        log_debug("使用默认 owner: %s", config->owner);
    }

    // 获取 repo
    config->repo = getenv("GITHUB_REPO");
    config->repo_allocated = 0;
    if (!config->repo) {
        config->repo = strdup("backup");
        config->repo_allocated = 1;
        log_debug("使用默认 repo: %s", config->repo);
    }

    // release_id 将在运行时获取
    config->release_id = NULL;

    // 获取 tag_name（可选）
    config->tag_name = getenv("GITHUB_TAG");
    if (config->tag_name) {
        log_info("使用指定的Tag: %s", config->tag_name);
    }

    log_debug("配置加载成功: owner=%s, repo=%s", config->owner, config->repo);
    return ERR_OK;
}

// 获取Releases列表并获取指定的release id（根据tag_name）或最新的release id
static ErrorCode getLatestReleaseId(Config *config) {
    struct MemoryStruct chunk = {NULL, 0};
    CURL *curl = NULL;
    struct curl_slist *headers = NULL;
    struct json_object *root = NULL;
    char *url = NULL;
    ErrorCode result = ERR_OK;
    char *new_release_id = NULL;

    chunk.memory = malloc(1);
    if (!chunk.memory) {
        fprintf(stderr, "内存分配失败\n");
        return ERR_MEMORY;
    }
    chunk.memory[0] = '\0';

    url = create_url("https://api.github.com/repos/%s/%s/releases",
                     config->owner, config->repo);
    if (!url) {
        fprintf(stderr, "URL 分配失败\n");
        result = ERR_MEMORY;
        goto cleanup;
    }

    curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "初始化 CURL 失败\n");
        result = ERR_CURL_INIT;
        goto cleanup;
    }

    headers = setGithubHeaders(config->token, NULL);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "获取Release列表失败: %s\n", curl_easy_strerror(res));
        result = ERR_CURL_PERFORM;
        goto cleanup;
    }

    // 解析JSON数组
    root = json_tokener_parse(chunk.memory);
    if (!root) {
        fprintf(stderr, "解析JSON失败\n");
        result = ERR_JSON_PARSE;
        goto cleanup;
    }

    if (!json_object_is_type(root, json_type_array)) {
        fprintf(stderr, "返回数据不是JSON数组格式\n");
        result = ERR_JSON_TYPE;
        goto cleanup;
    }

    int arraySize = json_object_array_length(root);
    if (arraySize == 0) {
        fprintf(stderr, "没有找到任何releases\n");
        result = ERR_NOT_FOUND;
        goto cleanup;
    }

    // 查找目标release
    struct json_object *targetRelease = NULL;

    if (config->tag_name) {
        // 按tag_name查找release
        for (int i = 0; i < arraySize; i++) {
            struct json_object *release = json_object_array_get_idx(root, i);
            struct json_object *tag_obj;

            if (json_object_object_get_ex(release, "tag_name", &tag_obj) &&
                json_object_is_type(tag_obj, json_type_string)) {
                const char *tag = json_object_get_string(tag_obj);
                if (strcmp(tag, config->tag_name) == 0) {
                    targetRelease = release;
                    break;
                }
            }
        }

        if (!targetRelease) {
            fprintf(stderr, "未找到tag为 \"%s\" 的release\n", config->tag_name);
            fprintf(stderr, "可用的tag有:\n");
            for (int i = 0; i < arraySize; i++) {
                struct json_object *release = json_object_array_get_idx(root, i);
                struct json_object *tag_obj;
                if (json_object_object_get_ex(release, "tag_name", &tag_obj) &&
                    json_object_is_type(tag_obj, json_type_string)) {
                    fprintf(stderr, "  - %s\n", json_object_get_string(tag_obj));
                }
            }
            result = ERR_NOT_FOUND;
            goto cleanup;
        }
    } else {
        // 未指定tag_name，使用第一个release
        targetRelease = json_object_array_get_idx(root, 0);
    }

    struct json_object *id_obj;
    if (!json_object_object_get_ex(targetRelease, "id", &id_obj) ||
        !json_object_is_type(id_obj, json_type_int)) {
        fprintf(stderr, "无法获取release id\n");
        result = ERR_JSON_TYPE;
        goto cleanup;
    }

    int id_value = json_object_get_int(id_obj);

    // 将id转换为字符串
    char temp_id[32];
    int ret = snprintf(temp_id, sizeof(temp_id), "%d", id_value);
    if (ret < 0 || ret >= (int)sizeof(temp_id)) {
        fprintf(stderr, "格式化ID时出错\n");
        result = ERR_CONFIG;
        goto cleanup;
    }

    // 只在返回 ERR_OK 时更新 release_id
    // 获取并显示tag_name用于确认
    struct json_object *tag_obj;
    if (json_object_object_get_ex(targetRelease, "tag_name", &tag_obj) &&
        json_object_is_type(tag_obj, json_type_string)) {
        printf("使用Release Tag: %s\n", json_object_get_string(tag_obj));
    }

    // 释放旧的 release_id（如果存在）
    if (config->release_id) {
        free(config->release_id);
        config->release_id = NULL;  // 设置为 NULL 避免后续使用已释放的内存
    }

    new_release_id = strdup(temp_id);
    if (!new_release_id) {
        fprintf(stderr, "内存分配失败\n");
        result = ERR_MEMORY;
        goto cleanup;
    }

    config->release_id = new_release_id;
    new_release_id = NULL;
    printf("使用Release ID: %s\n", config->release_id);

cleanup:
    if (root) json_object_put(root);
    if (chunk.memory) free(chunk.memory);
    if (headers) curl_slist_free_all(headers);
    if (curl) curl_easy_cleanup(curl);
    if (url) free(url);
    if (new_release_id) free(new_release_id);

    return result;
}

// 设置常用的GitHub API请求头
static struct curl_slist* setGithubHeaders(const char *token, const char *content_type) {
    struct curl_slist *headers = NULL;
    struct curl_slist *new_headers;

    headers = curl_slist_append(headers, "Accept: application/vnd.github+json");
    if (!headers) return NULL;

    headers = curl_slist_append(headers, "X-GitHub-Api-Version: 2022-11-28");
    if (!headers) return NULL;

    char *auth_header = NULL;
    int auth_len = snprintf(NULL, 0, "Authorization: Bearer %s", token);
    if (auth_len < 0) {
        curl_slist_free_all(headers);
        return NULL;
    }

    auth_header = malloc(auth_len + 1);
    if (!auth_header) {
        curl_slist_free_all(headers);
        return NULL;
    }

    snprintf(auth_header, auth_len + 1, "Authorization: Bearer %s", token);
    new_headers = curl_slist_append(headers, auth_header);
    free(auth_header);

    if (!new_headers) {
        curl_slist_free_all(headers);
        return NULL;
    }
    headers = new_headers;

    if (content_type) {
        char *content_type_header = NULL;
        int ct_len = snprintf(NULL, 0, "Content-Type: %s", content_type);
        if (ct_len < 0) {
            curl_slist_free_all(headers);
            return NULL;
        }

        content_type_header = malloc(ct_len + 1);
        if (!content_type_header) {
            curl_slist_free_all(headers);
            return NULL;
        }

        snprintf(content_type_header, ct_len + 1, "Content-Type: %s", content_type);
        new_headers = curl_slist_append(headers, content_type_header);
        free(content_type_header);

        if (!new_headers) {
            curl_slist_free_all(headers);
            return NULL;
        }
        headers = new_headers;
    }

    return headers;
}

// HTTP响应回调函数
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if(ptr == NULL) {
        printf("Not enough memory (realloc returned NULL)\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

// 验证文件路径是否安全（防止路径遍历）
static int is_safe_path(const char *path) {
    if (!path) return 0;

    // 检查是否包含 "../" 或 "..\"
    if (strstr(path, "../") || strstr(path, "..\\")) {
        return 0;
    }

    // 如果以 "/" 开头，可能是绝对路径，限制访问
    if (path[0] == '/') {
        return 0;
    }

    return 1;
}

// 读取文件到缓冲区
char* readFileToBuffer(const char *filename, long *file_size) {
    if (!is_safe_path(filename)) {
        fprintf(stderr, "无效的文件路径: %s\n", filename);
        return NULL;
    }

    FILE *file = fopen(filename, "rb");
    if (!file) {
        printf("无法打开文件: %s\n", filename);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    *file_size = ftell(file);
    if (*file_size < 0) {
        perror("获取文件大小失败");
        fclose(file);
        return NULL;
    }
    fseek(file, 0, SEEK_SET);

    if (*file_size == 0) {
        printf("文件为空\n");
        fclose(file);
        return NULL;
    }

    // 对于非常大的文件进行检查，防止内存耗尽
    if (*file_size > 1024L * 1024L * 1024L) { // 1GB
        printf("文件过大\n");
        fclose(file);
        return NULL;
    }

    char *buffer = malloc(*file_size);
    if (!buffer) {
        printf("内存分配失败\n");
        fclose(file);
        return NULL;
    }

    size_t result = fread(buffer, 1, *file_size, file);
    if (result != (size_t)*file_size) {
        printf("文件读取失败\n");
        free(buffer);
        fclose(file);
        return NULL;
    }

    fclose(file);
    return buffer;
}

// 获取文件名
const char* getFilenameFromPath(const char *path) {
    const char *filename = strrchr(path, '/');
    if (filename) {
        return filename + 1;
    }
    return path;
}

// 创建URL的辅助函数
static char* create_url(const char *format, ...) {
    va_list args;
    va_list args_copy;
    va_start(args, format);

    // 先计算所需长度
    va_copy(args_copy, args);
    int len = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);

    if (len < 0) {
        va_end(args);
        return NULL;
    }

    char *url = malloc(len + 1);
    if (!url) {
        va_end(args);
        return NULL;
    }

    vsnprintf(url, len + 1, format, args);
    va_end(args);

    return url;
}

// 验证配置
static ErrorCode validate_config(const Config *config) {
    if (!config) {
        fprintf(stderr, "错误：配置为空\n");
        return ERR_CONFIG;
    }

    if (!config->token) {
        fprintf(stderr, "错误：未设置 token\n");
        return ERR_CONFIG;
    }

    if (!config->owner) {
        fprintf(stderr, "错误：未设置 owner\n");
        return ERR_CONFIG;
    }

    if (!config->repo) {
        fprintf(stderr, "错误：未设置 repo\n");
        return ERR_CONFIG;
    }

    if (!config->release_id) {
        fprintf(stderr, "错误：未设置 release_id\n");
        return ERR_CONFIG;
    }

    return ERR_OK;
}

// 通配符匹配（支持 * 和 ?）
static int matchWildcard(const char *pattern, const char *string) {
    return fnmatch(pattern, string, 0) == 0;
}

// 展开通配符模式，返回匹配的文件列表
// 返回: 匹配的文件数量，结果通过 result 参数返回，需要调用者释放内存
static int expandWildcards(const char *pattern, char ***result) {
    *result = NULL;

    // 检查是否包含通配符
    if (strchr(pattern, '*') == NULL && strchr(pattern, '?') == NULL &&
        strchr(pattern, '[') == NULL) {
        // 不包含通配符
        *result = malloc(sizeof(char *));
        if (!*result) return -1;
        (*result)[0] = strdup(pattern);
        if (!(*result)[0]) {
            free(*result);
            *result = NULL;
            return -1;
        }
        return 1;
    }

    // 收集匹配的文件
    char **files = NULL;
    int count = 0;
    int capacity = 10;

    files = malloc(capacity * sizeof(char *));
    if (!files) return -1;

    DIR *dir = opendir(".");
    if (!dir) {
        free(files);
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // 跳过目录和隐藏文件
        if (entry->d_type == DT_DIR) continue;
        if (entry->d_name[0] == '.') continue;

        // 检查是否匹配通配符
        if (matchWildcard(pattern, entry->d_name)) {
            // 扩容
            if (count >= capacity) {
                capacity *= 2;
                char **new_files = realloc(files, capacity * sizeof(char *));
                if (!new_files) {
                    // 清理已分配的文件名
                    for (int i = 0; i < count; i++) {
                        free(files[i]);
                    }
                    free(files);
                    closedir(dir);
                    return -1;
                }
                files = new_files;
            }

            files[count] = strdup(entry->d_name);
            if (!files[count]) {
                // 清理已分配的文件名
                for (int i = 0; i < count; i++) {
                    free(files[i]);
                }
                free(files);
                closedir(dir);
                return -1;
            }
            count++;
        }
    }

    closedir(dir);

    if (count == 0) {
        free(files);
        *result = NULL;
        return 0;
    }

    *result = files;
    return count;
}

// 批量上传文件
static ErrorCode uploadMultipleFiles(int fileCount, char **filePaths, const Config *config) {
    if (fileCount <= 0 || !filePaths || !config) {
        return ERR_CONFIG;
    }

    int success = 0;
    int failed = 0;

    printf("准备批量上传 %d 个文件...\n\n", fileCount);

    for (int i = 0; i < fileCount; i++) {
        printf("[%d/%d] ", i + 1, fileCount);
        fflush(stdout);

        ErrorCode result = uploadFileWithRetry(filePaths[i], config, MAX_RETRIES);
        if (result == ERR_OK) {
            success++;
        } else {
            failed++;
            fprintf(stderr, "文件 \"%s\" 上传失败\n", filePaths[i]);
        }

        // 在上传之间稍作延迟，避免触发API限制
        if (i < fileCount - 1) {
#ifdef _WIN32
            Sleep(100);
#else
            usleep(100000);  // 0.1秒
#endif
        }
    }

    printf("\n===================================\n");
    printf("批量上传完成:\n");
    printf("  成功: %d\n", success);
    printf("  失败: %d\n", failed);
    printf("===================================\n");

    return (failed == 0) ? ERR_OK : ERR_CURL_PERFORM;
}

// 批量删除文件
static ErrorCode deleteMultipleFiles(int fileCount, char **fileNames, const Config *config) {
    if (fileCount <= 0 || !fileNames || !config) {
        return ERR_CONFIG;
    }

    int success = 0;
    int failed = 0;

    printf("准备批量删除 %d 个文件...\n\n", fileCount);

    for (int i = 0; i < fileCount; i++) {
        printf("[%d/%d] ", i + 1, fileCount);
        fflush(stdout);

        ErrorCode result = deleteFileWithRetry(fileNames[i], config, MAX_RETRIES);
        if (result == ERR_OK) {
            success++;
        } else {
            failed++;
        }

        // 在删除之间稍作延迟，避免触发API限制
        if (i < fileCount - 1) {
#ifdef _WIN32
            Sleep(100);
#else
            usleep(100000);  // 0.1秒
#endif
        }
    }

    printf("\n===================================\n");
    printf("批量删除完成:\n");
    printf("  成功: %d\n", success);
    printf("  失败: %d\n", failed);
    printf("===================================\n");

    return (failed == 0) ? ERR_OK : ERR_CURL_PERFORM;
}

// 更新文件（如果文件存在则删除后重新上传）
static ErrorCode updateFile(const char *filePath, const Config *config) {
    if (validate_config(config) != ERR_OK) {
        return ERR_CONFIG;
    }

    if (!filePath) {
        fprintf(stderr, "错误：文件路径不能为空\n");
        return ERR_CONFIG;
    }

    const char *fileName = getFilenameFromPath(filePath);

    printf("准备更新文件 \"%s\"...\n", fileName);

    // 首先尝试删除文件（如果存在的话）
    // 注意：即使删除失败（文件不存在），我们也继续上传
    ErrorCode deleteResult = deleteFile(fileName, config);

    // 如果删除失败且不是因为文件不存在，可能需要关注
    // 但大多数情况下我们应该继续上传
    if (deleteResult == ERR_OK) {
        printf("已删除旧版本文件。\n");
    } else if (deleteResult == ERR_NOT_FOUND) {
        printf("文件不存在于Release中，将直接上传。\n");
    } else {
        fprintf(stderr, "警告：删除现有文件时出现问题，但继续上传...\n");
    }

    // 上传文件
    ErrorCode uploadResult = uploadFile(filePath, config);

    if (uploadResult == ERR_OK) {
        printf("✅ 文件 \"%s\" 更新成功!\n", fileName);
    } else {
        fprintf(stderr, "❌ 文件 \"%s\" 更新失败!\n", fileName);
    }

    return uploadResult;
}

// 批量更新文件
static ErrorCode updateMultipleFiles(int fileCount, char **filePaths, const Config *config) {
    if (fileCount <= 0 || !filePaths || !config) {
        return ERR_CONFIG;
    }

    int success = 0;
    int failed = 0;

    printf("准备批量更新 %d 个文件...\n\n", fileCount);

    for (int i = 0; i < fileCount; i++) {
        printf("[%d/%d] ", i + 1, fileCount);
        fflush(stdout);

        ErrorCode result = updateFileWithRetry(filePaths[i], config, MAX_RETRIES);
        if (result == ERR_OK) {
            success++;
        } else {
            failed++;
            fprintf(stderr, "文件 \"%s\" 更新失败\n", filePaths[i]);
        }

        // 在更新之间稍作延迟，避免触发API限制
        if (i < fileCount - 1) {
#ifdef _WIN32
            Sleep(100);
#else
            usleep(100000);  // 0.1秒
#endif
        }
    }

    printf("\n===================================\n");
    printf("批量更新完成:\n");
    printf("  成功: %d\n", success);
    printf("  失败: %d\n", failed);
    printf("===================================\n");

    return (failed == 0) ? ERR_OK : ERR_CURL_PERFORM;
}

// 获取Release中的所有资产
ErrorCode getAssets(struct MemoryStruct *chunk, const Config *config) {
    if (validate_config(config) != ERR_OK) {
        return ERR_CONFIG;
    }

    CURL *curl = NULL;
    struct curl_slist *headers = NULL;
    ErrorCode result = ERR_OK;
    char *url = NULL;

    url = create_url("https://api.github.com/repos/%s/%s/releases/%s",
                     config->owner, config->repo, config->release_id);
    if (!url) {
        fprintf(stderr, "内存分配失败\n");
        result = ERR_MEMORY;
        goto cleanup;
    }

    curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "初始化 CURL 失败\n");
        result = ERR_CURL_INIT;
        goto cleanup;
    }

    headers = setGithubHeaders(config->token, NULL);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)chunk);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "获取Release信息失败: %s\n", curl_easy_strerror(res));
        result = ERR_CURL_PERFORM;
        goto cleanup;
    }

    // 检查HTTP响应码
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    if (response_code >= 400) {
        fprintf(stderr, "HTTP错误: %ld\n", response_code);
        result = ERR_HTTP_ERROR;
        goto cleanup;
    }

cleanup:
    if (headers) curl_slist_free_all(headers);
    if (curl) curl_easy_cleanup(curl);
    if (url) free(url);

    return result;
}

// 删除指定的资产
ErrorCode deleteAsset(const char *assetId, const char *assetName, const Config *config) {
    if (validate_config(config) != ERR_OK) {
        return ERR_CONFIG;
    }

    if (!assetId || !assetName) {
        fprintf(stderr, "错误：assetId 和 assetName 不能为空\n");
        return ERR_CONFIG;
    }

    CURL *curl = NULL;
    struct curl_slist *headers = NULL;
    ErrorCode result = ERR_OK;
    char *url = NULL;

    url = create_url("https://api.github.com/repos/%s/%s/releases/assets/%s",
                     config->owner, config->repo, assetId);
    if (!url) {
        fprintf(stderr, "内存分配失败\n");
        return ERR_MEMORY;
    }

    curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "初始化 CURL 失败\n");
        free(url);
        return ERR_CURL_INIT;
    }

    headers = setGithubHeaders(config->token, NULL);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "删除资产失败: %s\n", curl_easy_strerror(res));
        result = ERR_CURL_PERFORM;
        goto cleanup;
    }

    // 检查HTTP响应码
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    if (response_code >= 400) {
        fprintf(stderr, "删除资产失败，HTTP错误: %ld\n", response_code);
        result = ERR_HTTP_ERROR;
        goto cleanup;
    }

    printf("\n✅ 文件 \"%s\" 删除成功!\n", assetName);
    result = ERR_OK;

cleanup:
    if (url) free(url);
    if (headers) curl_slist_free_all(headers);
    if (curl) curl_easy_cleanup(curl);

    return result;
}

// 上传文件
ErrorCode uploadFile(const char *filePath, const Config *config) {
    if (validate_config(config) != ERR_OK) {
        return ERR_CONFIG;
    }

    if (!filePath) {
        fprintf(stderr, "错误：文件路径不能为空\n");
        return ERR_CONFIG;
    }

    CURL *curl = NULL;
    struct MemoryStruct chunk = {NULL, 0};
    struct curl_slist *headers = NULL;
    struct json_object *root = NULL;
    struct json_object *uploadResponse = NULL;
    char *fileBuffer = NULL;
    char *uploadUrl = NULL;
    ErrorCode result = ERR_OK;

    // 首先获取Release信息
    chunk.memory = malloc(1);
    if (!chunk.memory) {
        fprintf(stderr, "内存分配失败\n");
        result = ERR_MEMORY;
        goto cleanup;
    }
    chunk.memory[0] = '\0';
    chunk.size = 0;

    if (getAssets(&chunk, config) != ERR_OK) {
        result = ERR_CURL_PERFORM;
        goto cleanup;
    }

    // 解析JSON获取upload_url
    root = json_tokener_parse(chunk.memory);
    if (!root) {
        fprintf(stderr, "解析JSON失败\n");
        result = ERR_JSON_PARSE;
        goto cleanup;
    }

    struct json_object *upload_url_item;
    if (!json_object_object_get_ex(root, "upload_url", &upload_url_item) ||
        !json_object_is_type(upload_url_item, json_type_string)) {
        fprintf(stderr, "获取upload_url失败\n");
        result = ERR_JSON_TYPE;
        goto cleanup;
    }

    const char *uploadUrlTemplate = json_object_get_string(upload_url_item);
    const char *fileName = getFilenameFromPath(filePath);

    // 动态构建上传URL，避免缓冲区溢出
    size_t template_len = strlen(uploadUrlTemplate);
    size_t filename_len = strlen(fileName);
    size_t url_max_len = template_len + filename_len + 64; // +64 用于参数
    uploadUrl = malloc(url_max_len);
    if (!uploadUrl) {
        fprintf(stderr, "内存分配失败\n");
        result = ERR_MEMORY;
        goto cleanup;
    }

    char *template_end = strstr(uploadUrlTemplate, "{?name,label}");
    if (template_end) {
        size_t prefix_len = template_end - uploadUrlTemplate;
        snprintf(uploadUrl, url_max_len, "%.*s?name=%s",
                 (int)prefix_len, uploadUrlTemplate, fileName);
    } else {
        snprintf(uploadUrl, url_max_len, "%s?name=%s", uploadUrlTemplate, fileName);
    }

    // 读取文件
    long fileSize = 0;
    fileBuffer = readFileToBuffer(filePath, &fileSize);
    if (!fileBuffer) {
        result = ERR_FILE_IO;
        goto cleanup;
    }

    printf("准备上传文件 \"%s\" (%ld bytes)...\n", fileName, fileSize);
    printf("上传到: %s\n", uploadUrl);

    // 准备上传
    curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "初始化 CURL 失败\n");
        result = ERR_CURL_INIT;
        goto cleanup;
    }

    headers = setGithubHeaders(config->token, "application/octet-stream");

    // 构建 Content-Length 头
    char content_length[64];
    snprintf(content_length, sizeof(content_length), "Content-Length: %ld", fileSize);
    headers = curl_slist_append(headers, content_length);
    if (!headers) {
        fprintf(stderr, "添加header失败\n");
        result = ERR_MEMORY;
        goto cleanup;
    }

    chunk.size = 0;

    curl_easy_setopt(curl, CURLOPT_URL, uploadUrl);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, fileBuffer);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, fileSize);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "上传文件失败: %s\n", curl_easy_strerror(res));
        result = ERR_CURL_PERFORM;
        goto cleanup;
    }

    // 检查HTTP响应码
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    if (response_code >= 400) {
        fprintf(stderr, "上传文件失败，HTTP错误: %ld\n", response_code);
        result = ERR_HTTP_ERROR;
        goto cleanup;
    }

    // 解析上传响应
    uploadResponse = json_tokener_parse(chunk.memory);
    if (uploadResponse) {
        printf("\n✅ 文件上传成功!\n");

        struct json_object *id;
        if (json_object_object_get_ex(uploadResponse, "id", &id)) {
            printf("   - Asset ID: %d\n", json_object_get_int(id));
        }

        struct json_object *url;
        if (json_object_object_get_ex(uploadResponse, "browser_download_url", &url) &&
            json_object_is_type(url, json_type_string)) {
            printf("   - 下载链接: %s\n", json_object_get_string(url));
        }
    } else {
        printf("上传成功，但无法解析响应\n");
    }

cleanup:
    if (uploadResponse) json_object_put(uploadResponse);
    if (uploadUrl) free(uploadUrl);
    if (fileBuffer) free(fileBuffer);
    if (root) json_object_put(root);
    if (chunk.memory) free(chunk.memory);
    if (headers) curl_slist_free_all(headers);
    if (curl) curl_easy_cleanup(curl);

    return result;
}

// 删除文件
ErrorCode deleteFile(const char *fileName, const Config *config) {
    if (validate_config(config) != ERR_OK) {
        return ERR_CONFIG;
    }

    if (!fileName) {
        fprintf(stderr, "错误：文件名不能为空\n");
        return ERR_CONFIG;
    }

    // 检查文件名是否包含非法字符
    if (strchr(fileName, '/') || strchr(fileName, '\\')) {
        fprintf(stderr, "错误：文件名不能包含路径分隔符\n");
        return ERR_INVALID_PATH;
    }

    struct MemoryStruct chunk = {NULL, 0};
    struct json_object *root = NULL;
    ErrorCode result = ERR_OK;
    int found = 0;

    chunk.memory = malloc(1);
    if (!chunk.memory) {
        fprintf(stderr, "内存分配失败\n");
        result = ERR_MEMORY;
        goto cleanup;
    }
    chunk.memory[0] = '\0';
    chunk.size = 0;

    // 获取资产列表
    if (getAssets(&chunk, config) != ERR_OK) {
        result = ERR_CURL_PERFORM;
        goto cleanup;
    }

    // 解析JSON
    root = json_tokener_parse(chunk.memory);
    if (!root) {
        fprintf(stderr, "解析JSON失败\n");
        result = ERR_JSON_PARSE;
        goto cleanup;
    }

    struct json_object *assets;
    if (!json_object_object_get_ex(root, "assets", &assets) ||
        !json_object_is_type(assets, json_type_array)) {
        fprintf(stderr, "获取资产列表失败\n");
        result = ERR_JSON_TYPE;
        goto cleanup;
    }

    // 查找文件
    int arraySize = json_object_array_length(assets);
    for (int i = 0; i < arraySize; i++) {
        struct json_object *asset = json_object_array_get_idx(assets, i);
        struct json_object *name;

        if (json_object_object_get_ex(asset, "name", &name) &&
            json_object_is_type(name, json_type_string) &&
            strcmp(json_object_get_string(name), fileName) == 0) {
            struct json_object *id;
            if (json_object_object_get_ex(asset, "id", &id)) {
                int id_value = json_object_get_int(id);
                printf("找到文件 \"%s\" (ID: %d)，正在删除...\n", fileName, id_value);

                // 动态分配ID字符串（避免静态缓冲区）
                char *id_str = malloc(32);
                if (!id_str) {
                    fprintf(stderr, "内存分配失败\n");
                    result = ERR_MEMORY;
                    goto cleanup;
                }
                snprintf(id_str, 32, "%d", id_value);

                ErrorCode ret = deleteAsset(id_str, fileName, config);
                free(id_str);

                if (ret == ERR_OK) {
                    result = ERR_OK;
                    found = 1;
                    break;
                } else {
                    result = ret;
                    goto cleanup;
                }
            }
        }
    }

    if (!found) {
        fprintf(stderr, "错误：在Release中未找到名为 \"%s\" 的文件。\n", fileName);
        result = ERR_NOT_FOUND;

        // 显示可用文件
        if (arraySize > 0) {
            printf("可用文件列表:\n");
            for (int i = 0; i < arraySize; i++) {
                struct json_object *asset = json_object_array_get_idx(assets, i);
                struct json_object *name, *id;

                if (json_object_object_get_ex(asset, "name", &name) &&
                    json_object_is_type(name, json_type_string) &&
                    json_object_object_get_ex(asset, "id", &id)) {
                    printf("  - %s (ID: %d)\n",
                           json_object_get_string(name),
                           json_object_get_int(id));
                }
            }
        } else {
            printf("Release中没有文件。\n");
        }
    }

cleanup:
    if (root) json_object_put(root);
    if (chunk.memory) free(chunk.memory);

    return result;
}

// 列出所有文件
ErrorCode listFiles(const Config *config) {
    if (validate_config(config) != ERR_OK) {
        return ERR_CONFIG;
    }

    struct MemoryStruct chunk = {NULL, 0};
    struct json_object *root = NULL;
    ErrorCode result = ERR_OK;

    chunk.memory = malloc(1);
    if (!chunk.memory) {
        fprintf(stderr, "内存分配失败\n");
        result = ERR_MEMORY;
        goto cleanup;
    }
    chunk.memory[0] = '\0';
    chunk.size = 0;

    // 获取资产列表
    if (getAssets(&chunk, config) != ERR_OK) {
        result = ERR_CURL_PERFORM;
        goto cleanup;
    }

    // 解析JSON
    root = json_tokener_parse(chunk.memory);
    if (!root) {
        fprintf(stderr, "解析JSON失败\n");
        result = ERR_JSON_PARSE;
        goto cleanup;
    }

    struct json_object *assets;
    if (!json_object_object_get_ex(root, "assets", &assets) ||
        !json_object_is_type(assets, json_type_array)) {
        fprintf(stderr, "获取资产列表失败\n");
        result = ERR_JSON_TYPE;
        goto cleanup;
    }

    int arraySize = json_object_array_length(assets);
    if (arraySize == 0) {
        printf("Release中没有文件。\n");
    } else {
        printf("Release中的文件列表:\n");
        printf("%-40s %15s %15s\n", "文件名", "大小(bytes)", "下载次数");
        printf("--------------------------------------------------------------------------\n");

        for (int i = 0; i < arraySize; i++) {
            struct json_object *asset = json_object_array_get_idx(assets, i);
            struct json_object *name_obj, *size_obj, *download_count_obj;

            if (json_object_object_get_ex(asset, "name", &name_obj) &&
                json_object_is_type(name_obj, json_type_string)) {

                const char *name = json_object_get_string(name_obj);
                int size = 0, download_count = 0;

                if (json_object_object_get_ex(asset, "size", &size_obj)) {
                    size = json_object_get_int(size_obj);
                }

                if (json_object_object_get_ex(asset, "download_count", &download_count_obj)) {
                    download_count = json_object_get_int(download_count_obj);
                }

                printf("%-40s %15d %15d\n", name, size, download_count);
            }
        }
    }

cleanup:
    if (root) json_object_put(root);
    if (chunk.memory) free(chunk.memory);

    return result;
}

void showUsage() {
    printf("用法:\n");
    printf("  ./manage upload <文件路径> [文件路径2] [文件路径3 ...]\n");
    printf("  ./manage delete <文件名> [文件名2] [文件名3 ...]\n");
    printf("  ./manage list\n");
    printf("  ./manage update <文件路径> [文件路径2] [文件路径3 ...]\n");
    printf("  ./manage create-release <tag_name> [选项] [文件...]\n");
    printf("  ./manage help         # 显示详细说明\n");
    printf("\n批量操作（支持通配符）:\n");
    printf("  ./manage upload *.zip\n");
    printf("  ./manage update *.zip\n");
    printf("  ./manage delete *.tmp\n");
    printf("  ./manage upload file1.zip file2.zip file3.zip\n");
    printf("\n环境变量:\n");
    printf("  GITHUB_TOKEN: GitHub API 令牌（必需）\n");
    printf("  GITHUB_OWNER: GitHub 仓库所有者（默认: nostalgia296）\n");
    printf("  GITHUB_REPO:  GitHub 仓库名（默认: backup）\n");
    printf("  GITHUB_TAG:   指定要操作的Release Tag（可选，未指定时使用最新的Release）\n");
}

void showDetailedUsage() {
    printf("GitHub Release 管理工具 - 详细说明\n");
    printf("=====================================\n\n");

    printf("快速开始:\n");
    printf("  export GITHUB_TOKEN=your_token_here\n");
    printf("  ./manage list                      # 查看当前 Release 中的文件\n");
    printf("  ./manage upload myfile.zip        # 上传文件\n");
    printf("  ./manage update myfile.zip        # 更新文件\n");
    printf("  ./manage create-release v1.0      # 创建新 Release\n\n");

    printf("命令用法:\n");
    printf("-----------\n\n");

    printf("上传文件 (upload):\n");
    printf("  ./manage upload <文件路径> [文件2] [文件3] ...\n");
    printf("  示例:\n");
    printf("    ./manage upload backup.zip\n");
    printf("    ./manage upload *.zip\n");
    printf("    ./manage upload file1.zip file2.zip file3.zip\n\n");

    printf("删除文件 (delete):\n");
    printf("  ./manage delete <文件名> [文件2] [文件3] ...\n");
    printf("  示例:\n");
    printf("    ./manage delete oldfile.zip\n");
    printf("    ./manage delete *.tmp\n");
    printf("    ./manage delete file1.tmp file2.tmp\n\n");

    printf("列出文件 (list):\n");
    printf("  ./manage list\n");
    printf("  显示 Release 中的所有文件，包括大小和下载次数\n\n");

    printf("更新文件 (update):\n");
    printf("  ./manage update <文件路径> [文件2] [文件3] ...\n");
    printf("  先删除旧文件，再上传新文件（用于替换已存在文件）\n");
    printf("  示例:\n");
    printf("    ./manage update newbackup.zip\n");
    printf("    ./manage update *.zip\n\n");

    printf("创建 Release (create-release):\n");
    printf("  ./manage create-release <tag_name> [选项] [文件...]\n");
    printf("  选项:\n");
    printf("    -n, --name <name>        Release 名称（默认使用 tag_name）\n");
    printf("    -d, --description <desc> Release 描述\n");
    printf("    -p, --prerelease         标记为预发布版本\n");
    printf("    [文件...]                创建 release 后要上传的文件（支持通配符）\n");
    printf("  示例:\n");
    printf("    ./manage create-release v1.0                           # 创建普通 release\n");
    printf("    ./manage create-release v1.0 -n \"Version 1.0\"          # 创建指定名称的 release\n");
    printf("    ./manage create-release v1.0 -d \"First stable release\" # 创建带描述的 release\n");
    printf("    ./manage create-release v1.0-beta -p                   # 创建预发布版本\n");
    printf("    ./manage create-release v1.0 *.zip                     # 创建 release 并上传所有 zip 文件\n");
    printf("    ./manage create-release v1.0 file1.zip file2.zip       # 创建 release 并上传指定文件\n\n");

    printf("环境变量配置:\n");
    printf("-------------\n\n");

    printf("必须在运行前设置 GITHUB_TOKEN:\n");
    printf("  export GITHUB_TOKEN=\"ghp_your_personal_access_token\"\n\n");

    printf("可选环境变量:\n");
    printf("  GITHUB_OWNER:  GitHub 用户名或组织名（默认: nostalgia296）\n");
    printf("  GITHUB_REPO:   仓库名称（默认: backup）\n");
    printf("  GITHUB_TAG:    指定要操作的 Release Tag（未指定时使用最新 Release）\n");
    printf("  示例:\n");
    printf("    export GITHUB_OWNER=\"myusername\"\n");
    printf("    export GITHUB_REPO=\"my-backup\"\n");
    printf("    export GITHUB_TAG=\"v1.0\"\n\n");

    printf("获取 GitHub Token:\n");
    printf("  1. 访问 https://github.com/settings/tokens\n");
    printf("  2. 点击 \"Generate new token\" → \"Generate new token (classic)\"\n");
    printf("  3. 选择 'repo' 作用域以访问私有仓库\n");
    printf("  4. 生成并复制 token\n");
    printf("  5. 在运行程序前设置环境变量\n\n");

    printf("注意事项:\n");
    printf("-----------\n");
    printf("  - 上传文件需要在 GitHub Release 中至少有一个 Release\n");
    printf("  - 操作可能需要几秒到几十秒，取决于文件大小和网络状况\n");
    printf("  - API 调用有速率限制，批量操作会自动添加延迟\n");
    printf("  - 如果上传失败，请检查文件大小是否超过 GitHub 限制\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "错误：请提供命令和参数。\n");
        showUsage();
        return 1;
    }

    Config config = {0};
    // 显式初始化标记位
    config.owner_allocated = 0;
    config.repo_allocated = 0;
    config.token_allocated = 0;

    if (getConfig(&config) != ERR_OK) {
        return 1;
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);

    // 初始化随机数生成器（用于重试机制的随机抖动）
    srand(time(NULL));

    const char *command = argv[1];
    ErrorCode result = ERR_OK;

    // 处理不需要获取 release_id 的命令
    if (strcmp(command, "help") == 0) {
        showDetailedUsage();
        goto cleanup;
    }

    // 某些命令不需要预先获取 release_id
    if (strcmp(command, "create-release") != 0) {
        // 获取最新的release id（动态分配）- 只调用一次
        if (getLatestReleaseId(&config) != ERR_OK) {
            result = ERR_CONFIG;
            goto cleanup;
        }
    }

    if (strcmp(command, "upload") == 0) {
        if (argc < 3) {
            fprintf(stderr, "错误：请提供文件路径。\n");
            showUsage();
            return 1;
        }

        // 处理批量上传
        int totalFiles = 0;
        char **allFiles = NULL;

        for (int i = 2; i < argc; i++) {
            char **matchedFiles = NULL;
            int fileCount = expandWildcards(argv[i], &matchedFiles);

            if (fileCount > 0) {
                char **newAllFiles = realloc(allFiles, (totalFiles + fileCount) * sizeof(char *));
                if (!newAllFiles) {
                    fprintf(stderr, "内存分配失败\n");
                    result = ERR_MEMORY;
                    // 释放已分配的文件名
                    if (allFiles) {
                        for (int j = 0; j < totalFiles; j++) {
                            free(allFiles[j]);
                        }
                        free(allFiles);
                    }
                    for (int j = 0; j < fileCount; j++) {
                        free(matchedFiles[j]);
                    }
                    free(matchedFiles);
                    goto cleanup;
                }
                allFiles = newAllFiles;

                for (int j = 0; j < fileCount; j++) {
                    allFiles[totalFiles++] = matchedFiles[j];
                }
                free(matchedFiles);
            }
        }

        if (totalFiles == 0) {
            fprintf(stderr, "错误：找不到匹配的文件\n");
            result = ERR_FILE_IO;
        } else {
            result = uploadMultipleFiles(totalFiles, allFiles, &config);
        }

        // 清理文件列表
        if (allFiles) {
            for (int i = 0; i < totalFiles; i++) {
                free(allFiles[i]);
            }
            free(allFiles);
        }
    } else if (strcmp(command, "delete") == 0) {
        if (argc < 3) {
            fprintf(stderr, "错误：请提供文件名。\n");
            showUsage();
            return 1;
        }

        // 收集所有要删除的文件
        int totalFiles = argc - 2;
        result = deleteMultipleFiles(totalFiles, &argv[2], &config);
    } else if (strcmp(command, "update") == 0) {
        if (argc < 3) {
            fprintf(stderr, "错误：请提供文件路径。\n");
            showUsage();
            return 1;
        }

        // 处理批量更新
        int totalFiles = 0;
        char **allFiles = NULL;

        for (int i = 2; i < argc; i++) {
            char **matchedFiles = NULL;
            int fileCount = expandWildcards(argv[i], &matchedFiles);

            if (fileCount > 0) {
                char **newAllFiles = realloc(allFiles, (totalFiles + fileCount) * sizeof(char *));
                if (!newAllFiles) {
                    fprintf(stderr, "内存分配失败\n");
                    result = ERR_MEMORY;
                    // 释放已分配的文件名
                    if (allFiles) {
                        for (int j = 0; j < totalFiles; j++) {
                            free(allFiles[j]);
                        }
                        free(allFiles);
                    }
                    for (int j = 0; j < fileCount; j++) {
                        free(matchedFiles[j]);
                    }
                    free(matchedFiles);
                    goto cleanup;
                }
                allFiles = newAllFiles;

                for (int j = 0; j < fileCount; j++) {
                    allFiles[totalFiles++] = matchedFiles[j];
                }
                free(matchedFiles);
            }
        }

        if (totalFiles == 0) {
            fprintf(stderr, "错误：找不到匹配的文件\n");
            result = ERR_FILE_IO;
        } else {
            result = updateMultipleFiles(totalFiles, allFiles, &config);
        }

        // 清理文件列表
        if (allFiles) {
            for (int i = 0; i < totalFiles; i++) {
                free(allFiles[i]);
            }
            free(allFiles);
        }
    } else if (strcmp(command, "list") == 0) {
        result = listFiles(&config);
    } else if (strcmp(command, "create-release") == 0) {
        if (argc < 3) {
            fprintf(stderr, "错误：请提供 tag_name。\n");
            showUsage();
            return 1;
        }

        // 创建新的 Release ID 用于接收函数返回值
        char *new_release_id = NULL;

        // 解析命令行参数
        const char *tag_name = argv[2];
        const char *release_name = NULL;
        const char *description = NULL;
        int is_prerelease = 0;

        // 解析可选参数和文件参数
        int file_argv_start = 3;
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--name") == 0) {
                if (i + 1 < argc) {
                    release_name = argv[i + 1];
                    i++; // 跳过下一个参数
                } else {
                    fprintf(stderr, "错误：-n 或 --name 需要一个参数\n");
                    showUsage();
                    return 1;
                }
            } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--description") == 0) {
                if (i + 1 < argc) {
                    description = argv[i + 1];
                    i++; // 跳过下一个参数
                } else {
                    fprintf(stderr, "错误：-d 或 --description 需要一个参数\n");
                    showUsage();
                    return 1;
                }
            } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--prerelease") == 0) {
                is_prerelease = 1;
            } else {
                // 其余参数都是文件路径
                file_argv_start = i;
                break;
            }
        }

        // 创建 Release
        result = createRelease(tag_name, release_name, description, is_prerelease, &config, &new_release_id);

        // 如果创建成功且有文件需要上传
        if (result == ERR_OK && new_release_id && file_argv_start < argc) {
            printf("\n准备上传文件到新创建的 Release...\n");

            // 创建一个临时的配置对象，使用新的 release_id
            Config upload_config = config;
            upload_config.release_id = new_release_id;

            // 处理所有文件模式
            int totalFiles = 0;
            char **allFiles = NULL;

            for (int i = file_argv_start; i < argc; i++) {
                char **matchedFiles = NULL;
                int fileCount = expandWildcards(argv[i], &matchedFiles);

                if (fileCount > 0) {
                    char **newAllFiles = realloc(allFiles, (totalFiles + fileCount) * sizeof(char *));
                    if (!newAllFiles) {
                        fprintf(stderr, "内存分配失败\n");
                        result = ERR_MEMORY;
                        // 清理已分配的文件名
                        if (allFiles) {
                            for (int j = 0; j < totalFiles; j++) {
                                free(allFiles[j]);
                            }
                            free(allFiles);
                        }
                        for (int j = 0; j < fileCount; j++) {
                            free(matchedFiles[j]);
                        }
                        free(matchedFiles);
                        free(new_release_id);
                        goto cleanup;
                    }
                    allFiles = newAllFiles;

                    for (int j = 0; j < fileCount; j++) {
                        allFiles[totalFiles++] = matchedFiles[j];
                    }
                    free(matchedFiles);
                }
            }

            if (totalFiles == 0) {
                fprintf(stderr, "错误：找不到匹配的文件\n");
                result = ERR_FILE_IO;
            } else {
                result = uploadMultipleFiles(totalFiles, allFiles, &upload_config);
            }

            // 清理文件列表
            if (allFiles) {
                for (int i = 0; i < totalFiles; i++) {
                    free(allFiles[i]);
                }
                free(allFiles);
            }

            free(new_release_id);
        } else if (new_release_id) {
            free(new_release_id);
        }
    } else {
        fprintf(stderr, "错误：未知命令 \"%s\"。\n", command);
        showUsage();
        return 1;
    }

cleanup:

    // 清理 release_id
    if (config.release_id) {
        free(config.release_id);
        config.release_id = NULL;
    }

    // 清理字符串配置（使用标记位判断）
    if (config.owner_allocated && config.owner) {
        free((void *)config.owner);
        config.owner = NULL;
    }
    if (config.repo_allocated && config.repo) {
        free((void *)config.repo);
        config.repo = NULL;
    }
    if (config.token_allocated && config.token) {
        free((void *)config.token);
        config.token = NULL;
    }

    curl_global_cleanup();

    // 将 ErrorCode 转换为 main 的返回值
    return (result == ERR_OK) ? 0 : 1;
}

// ==================== 重试机制实现 ====================

// 判断哪些错误需要重试
static int shouldRetryError(ErrorCode error) {
    switch (error) {
        case ERR_CURL_PERFORM:
            // 网络错误，可能临时失败
            return 1;
        case ERR_HTTP_ERROR:
            // HTTP错误可能需要重试（如500, 502, 503, 504）
            return 1;
        case ERR_MEMORY:
            // 内存错误可能由临时资源限制导致
            return 1;
        default:
            // 其他错误（配置错误、文件IO错误等）不需要重试
            return 0;
    }
}

// 核心重试逻辑
static ErrorCode performWithRetry(ErrorCode (*operation)(const void *), const void *param,
                                  int maxRetries, const char *opName) {
    ErrorCode lastError = ERR_OK;
    int retryCount = 0;

    while (retryCount <= maxRetries) {
        log_debug("尝试 %s (尝试 %d/%d)", opName, retryCount + 1, maxRetries + 1);

        lastError = operation(param);

        if (lastError == ERR_OK) {
            // 操作成功
            if (retryCount > 0) {
                log_info("%s 在第 %d 次尝试后成功", opName, retryCount + 1);
            }
            return ERR_OK;
        }

        retryCount++;

        // 检查是否需要重试
        if (retryCount > maxRetries || !shouldRetryError(lastError)) {
            break;
        }

        // 计算指数退避延迟：延迟 = base * 2^retryCount * (1 + 随机值/10)
        int baseDelay = 1; // 基础延迟：1秒
        int delay = baseDelay * (1 << (retryCount - 1)); // 指数增长

        // 添加随机抖动（避免所有客户端同时重试）
        int jitter = rand() % (delay / 10 + 1);
        delay += jitter;

        // 限制最大延迟
        delay = delay > 30 ? 30 : delay;

        log_warn("%s 失败: %d，%d 秒后重试...", opName, lastError, delay);
        sleep(delay);
    }

    log_error("%s 在 %d 次尝试后仍然失败", opName, maxRetries + 1);
    return ERR_RETRY_EXHAUSTED;
}

// 带重试的操作包装器
static ErrorCode retryableOperationWrapper(const void *param) {
    const RetryWrapperParam *wrapperParam = (const RetryWrapperParam *)param;
    return wrapperParam->operation(wrapperParam->param1, wrapperParam->config);
}

// 重试包装函数：上传文件
static ErrorCode uploadFileWithRetry(const char *filePath, const Config *config, int maxRetries) {
    RetryWrapperParam param = {
        .operation = uploadFile,
        .param1 = filePath,
        .config = config,
        .opName = "文件上传"
    };
    const char *fileName = getFilenameFromPath(filePath);
    log_info("开始上传文件: %s (最多重试 %d 次)", fileName, maxRetries);
    return performWithRetry(retryableOperationWrapper, &param, maxRetries, fileName);
}

// 重试包装函数：删除文件
static ErrorCode deleteFileWithRetry(const char *fileName, const Config *config, int maxRetries) {
    RetryWrapperParam param = {
        .operation = deleteFile,
        .param1 = fileName,
        .config = config,
        .opName = "文件删除"
    };
    log_info("开始删除文件: %s (最多重试 %d 次)", fileName, maxRetries);
    return performWithRetry(retryableOperationWrapper, &param, maxRetries, fileName);
}

// 重试包装函数：更新文件
static ErrorCode updateFileWithRetry(const char *filePath, const Config *config, int maxRetries) {
    RetryWrapperParam param = {
        .operation = updateFile,
        .param1 = filePath,
        .config = config,
        .opName = "文件更新"
    };
    const char *fileName = getFilenameFromPath(filePath);
    log_info("开始更新文件: %s (最多重试 %d 次)", fileName, maxRetries);
    return performWithRetry(retryableOperationWrapper, &param, maxRetries, fileName);
}

// 创建新的 GitHub Release，返回新创建的 release_id（动态分配）
static ErrorCode createRelease(const char *tag_name, const char *release_name, const char *description,
                               int is_prerelease, const Config *config, char **out_release_id) {
    // 验证配置（跳过 release_id 检查，因为创建 release 时 release_id 还未生成）
    if (!config) {
        log_error("配置为空");
        return ERR_CONFIG;
    }

    if (!config->token) {
        log_error("未设置 token");
        return ERR_CONFIG;
    }

    if (!config->owner) {
        log_error("未设置 owner");
        return ERR_CONFIG;
    }

    if (!config->repo) {
        log_error("未设置 repo");
        return ERR_CONFIG;
    }

    if (!tag_name) {
        log_error("tag_name 不能为空");
        return ERR_CONFIG;
    }

    CURL *curl = NULL;
    struct curl_slist *headers = NULL;
    struct MemoryStruct chunk = {NULL, 0};
    struct json_object *json_request = NULL;
    char *url = NULL;
    char *post_data = NULL;
    ErrorCode result = ERR_OK;

    // 构建请求体
    json_request = json_object_new_object();
    if (!json_request) {
        log_error("创建 JSON 对象失败");
        result = ERR_MEMORY;
        goto cleanup;
    }

    json_object_object_add(json_request, "tag_name", json_object_new_string(tag_name));

    // 如果提供了 release_name，则使用它，否则使用 tag_name
    if (release_name && strlen(release_name) > 0) {
        json_object_object_add(json_request, "name", json_object_new_string(release_name));
    } else {
        json_object_object_add(json_request, "name", json_object_new_string(tag_name));
    }

    // 添加描述
    if (description && strlen(description) > 0) {
        json_object_object_add(json_request, "body", json_object_new_string(description));
    } else {
        json_object_object_add(json_request, "body", json_object_new_string("Release created by manage tool"));
    }

    // 是否为预发布
    json_object_object_add(json_request, "prerelease", json_object_new_boolean(is_prerelease));

    // 设置为公开或私有（默认公开）
    json_object_object_add(json_request, "draft", json_object_new_boolean(0));

    // 获取 JSON 字符串
    const char *json_str = json_object_to_json_string(json_request);
    if (!json_str) {
        log_error("生成 JSON 字符串失败");
        result = ERR_JSON_PARSE;
        goto cleanup;
    }

    // 复制 JSON 字符串以便 curl 使用
    post_data = strdup(json_str);
    if (!post_data) {
        log_error("内存分配失败");
        result = ERR_MEMORY;
        goto cleanup;
    }

    // 创建 URL
    url = create_url("https://api.github.com/repos/%s/%s/releases", config->owner, config->repo);
    if (!url) {
        log_error("URL 分配失败");
        result = ERR_MEMORY;
        goto cleanup;
    }

    curl = curl_easy_init();
    if (!curl) {
        log_error("初始化 CURL 失败");
        result = ERR_CURL_INIT;
        goto cleanup;
    }

    headers = setGithubHeaders(config->token, "application/json");

    // 设置 POST 数据
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(post_data));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    printf("正在创建新的 Release，标签: %s...\n", tag_name);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "创建 Release 失败: %s\n", curl_easy_strerror(res));
        result = ERR_CURL_PERFORM;
        goto cleanup;
    }

    // 检查 HTTP 响应码
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    if (response_code >= 400) {
        log_error("创建 Release 失败，HTTP错误: %ld", response_code);
        log_error("响应内容: %s", chunk.memory);
        result = ERR_HTTP_ERROR;
        goto cleanup;
    }

    // 解析响应以获取新创建的 Release 信息
    struct json_object *response = json_tokener_parse(chunk.memory);
    if (!response) {
        log_error("解析创建 Release 的响应失败");
        result = ERR_JSON_PARSE;
        goto cleanup;
    }

    struct json_object *id_obj;
    if (!json_object_object_get_ex(response, "id", &id_obj) ||
        !json_object_is_type(id_obj, json_type_int)) {
        log_error("无法从响应中获取 release id");
        result = ERR_JSON_TYPE;
        goto cleanup;
    }

    int id_value = json_object_get_int(id_obj);
    struct json_object *tag_obj;
    const char *created_tag = tag_name;
    if (json_object_object_get_ex(response, "tag_name", &tag_obj) &&
        json_object_is_type(tag_obj, json_type_string)) {
        created_tag = json_object_get_string(tag_obj);
    }

    // 将 release_id 返回给调用者
    if (out_release_id) {
        char temp_id[32];
        snprintf(temp_id, sizeof(temp_id), "%d", id_value);
        *out_release_id = strdup(temp_id);
        if (!*out_release_id) {
            log_error("内存分配失败");
            result = ERR_MEMORY;
            goto cleanup;
        }
    }

    printf("✅ Release 创建成功!\n");
    printf("   - 标签: %s\n", created_tag);
    printf("   - ID: %d\n", id_value);

cleanup:
    if (response) json_object_put(response);
    if (post_data) free(post_data);
    if (json_request) json_object_put(json_request);
    if (chunk.memory) free(chunk.memory);
    if (headers) curl_slist_free_all(headers);
    if (curl) curl_easy_cleanup(curl);
    if (url) free(url);

    return result;
}
