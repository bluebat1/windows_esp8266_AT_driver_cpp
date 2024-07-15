#ifndef _TextUtil_h_
#define _TextUtil_h_

#include <string.h>

class TextUtil
{
private:
    /* data */
public:
    TextUtil(/* args */);
    ~TextUtil();

    // 切割字符串
    static int Split(char *str, const char * delimit, int limit){
        int strSize = strlen(str);
        int delimitSize = strlen(delimit);
        if(strSize < delimitSize) {
            return -1;
        }
        int count = 0;
        // 查找次数限制
        // -1 为最大次数
        if(limit < 0) {
            limit = 999;
        }
        char *_pos = str;
        while (limit--)
        {
            // 查找切割符位置
            char *pos = strstr(_pos, delimit);
            // 未找到
            if(pos == NULL) {
                // 如果最后一次切割位置不在 字符串结尾 则子串数量 +1
                if(_pos != str + strSize) {
                    count += 1;
                }
                break;
            }
            // 找到
            // 将切割符位置填充为0
            for (size_t i = 0; i < delimitSize; i++)
            {
                pos[i] = 0;
            }
            count++;
            _pos = pos + delimitSize;
        }
        return count;
    }
    // 跳转到下一个子串
    // 内存不安全 需要在外部保证内存安全
    static char * ToNextSubStr(char * str){
        while(*str != 0) {
            str ++;
        }
        while (*str == 0)
        {
            str ++;
        }
        return str;
    }

    // prefix check
    static bool WithStart(const char * str, const char * prefix){
        int prefixSize = strlen(prefix);
        int strSize = strlen(str);
        if(strSize < prefixSize) {
            return false;
        }
        for (size_t i = 0; i < prefixSize; i++)
        {
            if(str[i] != prefix[i]) {
                return false;
            }
        }
        return true;
    }
};

TextUtil::TextUtil(/* args */)
{
}

TextUtil::~TextUtil()
{
}


#endif // !_TextUtil_h_
