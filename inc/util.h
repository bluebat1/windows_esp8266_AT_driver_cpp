#ifndef _util_h_
#define _util_h_

#define logd(fmt, ...) printf("%s:%d >>  " fmt "\r\n", __FILE__, __LINE__, ##__VA_ARGS__)

#endif // !_util_h_
