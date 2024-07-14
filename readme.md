
注：使用 mingw + bash 来编译 与 执行，mingw 要使用 posix 版本， win32 版本不兼容
https://github.com/niXman/mingw-builds-binaries/releases
x86_64-13.2.0-release-posix-seh-ucrt-rt_v11-rev0.7z


1、创建编译文件夹

`
    touch build
`

2、进入编译文件夹

`
    cd build
`

3、生成makefile （重新生成 makefile 文件之前 请删除 build 文件夹内的内容）

`
    cmake -G "MinGW Makefiles" ..
`

4、编译

`
    make
`

5、执行

`
    ./MyProject.exe
`