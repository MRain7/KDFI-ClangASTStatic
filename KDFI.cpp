#include <fstream>
#include <set>
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Sema/Sema.h"
#include "llvm/Support/raw_ostream.h"
using namespace clang;
using namespace std;

#include "KDFI_analysis.h"

class ClangPluginASTConsumer : public ASTConsumer {
	//对AST树进行处理
	CompilerInstance &Instance;
	set<string> ParsedTemplates;
	ClangPluginASTVisitor visitor = ClangPluginASTVisitor(Instance);

public:
	ClangPluginASTConsumer(CompilerInstance &Instance,
		set<string> ParsedTemplates)
	: Instance(Instance), ParsedTemplates(ParsedTemplates) {}

	void HandleTranslationUnit(ASTContext& context) override {
		//调用ClangPluginASTVisitor类函数，得到想要的功能
		visitor.setContext(context);
		visitor.TraverseDecl(context.getTranslationUnitDecl());
        //visitor.ModWriteToFile();
	}
};

class ClangPluginASTAction : public PluginASTAction {
	//注册clang插件
	set<string> ParsedTemplates;
protected:
	unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
		llvm::StringRef) override {
		return llvm::make_unique<ClangPluginASTConsumer>(CI, ParsedTemplates); //设置插件的处理类为ClangPluginASTConsumer
	}

	bool ParseArgs(const CompilerInstance &CI,
		const vector<string> &args) override { //插件使用的参数
		return true;
	}
};


static FrontendPluginRegistry::Add<ClangPluginASTAction>
X("KDFI", "KDFI"); //显式注册插件KDFIs

