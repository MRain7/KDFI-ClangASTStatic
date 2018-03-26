# KDFI-ClangASTStatic
The subproject of Key Data Flow Integrity, Analyzing the AST information by clang.


## File description

* KDFI\_analysis.h: 需要写代码的文件
* KDFI.cpp: KDFI主文件，不需要修改
* KDFI.sh: shell文件，可能需要chmod +x KDFI.sh给予执行权限，直接运行即可对test.c进行静态分析
* KDFI.so: KDFI.cpp和KDFI\_analysis.h编译后生成的静态库文件
* makefile: 使用make -B编译KDFI.cpp和KDFI\_analysis.h成为KDFI.so，若llvm没有使用make install进系统文件夹，则需要更改其中的LLVM\_INCLUDES和LLVM\_LIB为对应文件夹
* test.ast: test.c的ast树
* test.c: 测试文件，可以根据需要自行修改.

PS: 修改完KDFI\_analysis.h后使用make -B更新修改，在运行KDFI.sh查看分析结果。只需要修改KDFI\_analysis.h。

## Notes

* [Install LLVM & Clang](https://www.jianshu.com/p/e8d3c38ebe77)
* [Source Code about LLVM](https://code.woboq.org/llvm/)
* Google the Clang API: `Clang Expr`...
