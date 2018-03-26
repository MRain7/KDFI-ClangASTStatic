CXX := g++
CXXFLAGS := -fno-rtti -O0 -g
PLUGIN_CXXFLAGS := -fpic

LLVM_CXXFLAGS := `llvm-config --cxxflags`
LLVM_LDFLAGS := `llvm-config --ldflags`

LLVM_LDFLAGS_NOLIBS := `llvm-config --ldflags`
PLUGIN_LDFLAGS := -shared

#LLVM_INCLUDES := -I/usr/local/include
#LLVM_LIB := -L/usr/local/lib

# Because llvm installed path is /usr
LLVM_INCLUDES := -I/usr/include
LLVM_LIB := -L/usr/lib

CLANG_LIBS := \
	-Wl,--start-group \
	-lclangAST \
	-lclangASTMatchers \
	-lclangAnalysis \
	-lclangBasic \
	-lclangDriver \
	-lclangEdit \
	-lclangFrontend \
	-lclangFrontendTool \
	-lclangLex \
	-lclangParse \
	-lclangSema \
	-lclangEdit \
	-lclangRewrite \
	-lclangRewriteFrontend \
	-lclangStaticAnalyzerFrontend \
	-lclangStaticAnalyzerCheckers \
	-lclangStaticAnalyzerCore \
	-lclangSerialization \
	-lclangToolingCore \
	-lclangTooling \
	-lclangFormat \
	-Wl,--end-group

KDFI.so: KDFI.cpp
	$(CXX) $(PLUGIN_CXXFLAGS) $(CXXFLAGS) $(LLVM_CXXFLAGS) $(LLVM_INCLUDES) $^ \
		$(PLUGIN_LDFLAGS) $(LLVM_LDFLAGS_NOLIBS) -o $@

