
注：使用 mingw + bash 来编译 与 执行，mingw 要使用 posix 版本， win32 版本不兼容

*MinGW下载地址：
```
    https://github.com/niXman/mingw-builds-binaries/releases
    x86_64-13.2.0-release-posix-seh-ucrt-rt_v11-rev0.7z
```

1、使用cmake + 脚本 生产工程配置

```
    . ./configuer.sh
```

2、编译 & 执行

```
    cd build
    . ../build.sh && ./MyProject.exe
```
