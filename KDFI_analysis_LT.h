#pragma once
/////////aaaaa
#include <queue>
#include <string>
#include <iostream>
#include <fstream>
#include <set>
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Sema/Sema.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/AST/Expr.h"
using namespace clang;
using namespace std;


class ClangPluginASTVisitor : public RecursiveASTVisitor<ClangPluginASTVisitor>{
	/*遍历AST树，对每个结点进行分析和记录*/
public:
	ClangPluginASTVisitor(CompilerInstance &Instance):Instance(Instance){
	}

	void setContext(ASTContext &context){
		//设置ASTContext，得到更详细的AST树信息
		this->context = &context;
	}
	bool VisitDecl(Decl *D){
		//对每个Decl结点分析
        HandleDefine(D);
		return true;
	}
	bool VisitStmt(Stmt *S){
        //S->dump();
		//对每个Stmt结点分析
        if(isa<BinaryOperator>(S) || isa<UnaryOperator>(S)){ //操作结点
            HandleOp(S);

            //llvm::errs()<< safeornot;
        }
        else if(isa<CallExpr>(S)){ //函数调用结点
            HandleCall(S);
        }
        else if(isa<ReturnStmt>(S)){ //函数返回结点
            HandleRet(S);
        }
		else if(isa<IfStmt>(S) || isa<ForStmt>(S) || isa<WhileStmt>(S) || isa<SwitchStmt>(S)){ //条件判断及循环结点
            HandleCond(S);
        }else{//其他情况
            HandleOthers(S);
        }
		return true;
	}

private:
	CompilerInstance &Instance;
    ASTContext *context;
    string localFunctionNameCollect="";
    void HandleDefine(Decl *D);
    void HandleOp(Stmt *S);
    void HandleCall(Stmt *S);
    void HandleRet(Stmt *S);
    void HandleCond(Stmt *S);
    void HandleOthers(Stmt *S);

    //自用函数声明：
    string HelpHandleDeclRefExpr(DeclRefExpr *);
    void print(string thing);
    string HelpGetDst(Expr*);
    string HelpGetSrc(Expr*,Expr*,string);
    string HelpGetFunctionName(DeclRefExpr*);
    string HelpVisitRightTree(Expr *);
    string HelpPointerDereference(DeclRefExpr *, string);
    string HelpHandleIncrementDecrementOp(UnaryOperator *,bool,string);
    string HelpHandleCompoundAssignOperator(CompoundAssignOperator *,bool,string);
    string HelpFindPointerInLeftTree(Expr *);
    string HelpHandleFunctionCall(Expr *,bool);
    string HelpAnalyzeLeftTree(Expr *,string);
    string HelpHandleArraySubsriptExpr(Expr *);
    string HelpGetType(Expr *);
    string HelpGetStars(Expr *);
    string HelpHandleVarDecl(VarDecl *);
    void HelpHandleRecordDecl(Decl*);
    string HelpCutTheLastComma(string);
    string HelpHandleMemberExpr(MemberExpr *);
    ///////////////////////////
    string concatStr(queue<Stmt*> q);
    string getCondStr(Stmt* root);
    queue<Stmt*> getAllChilds(Stmt* root);
    queue<Stmt*> extractChilds(queue<Stmt*> q);
    unsigned ModGetDeclLine(Decl *D);
    unsigned ModGetDeclLine(Stmt *S);
    set<IfStmt*> ifStmtSet;
    ///////////////////////////
    string HandleSafeType(Stmt *);
    string HelpGetDstSTUO(Expr *);
    string HelpGetSrcST(Expr *);
};


//////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                               TODO                                                   //
//    1. 暂时将要求的信息以要求的格式输出到llvm::errs()，对于所有的指针解引用操作，需要判断访存操作是否安全              //
//    2. 伪代码.txt作为参考，由于KDFI设计文档.pdf后面有些修改，因此可能有些不匹配的情况，一切依KDFI设计文档.pdf为准。    //
/////////////////////////////////////////////////////////////////////////////////////////////////////////

void ClangPluginASTVisitor::HandleDefine(Decl *D){
    //D->dump();
    if(isa<VarDecl>(D)){
        //D->dump();
        VarDecl* varDecl = (VarDecl*)D;
        QualType qualType = (QualType)varDecl -> getType();
        string nameStr = varDecl -> getNameAsString();  // 变量名
        string typeStr = qualType.getAsString();  // 类型名
        string varname = HelpHandleVarDecl(varDecl);
        unsigned int lineNum = ModGetDeclLine(varDecl);  // 行号
        if(isa<ParmVarDecl>(D)){//属于对函数参数的声明
            llvm::errs() << lineNum << "," << "Define" << ",Parm," << varname << "," << typeStr << "\n";
        }else
        llvm::errs() << lineNum << "," << "Define" << "," << varname << "," << typeStr << "\n";
        if(Expr *init = (Expr *)varDecl->getInit()){//获取等号右边的初始化信息

            if(isa<BinaryOperator>(init) || isa<UnaryOperator>(init)){ //操作结点
                HandleOp(init);
            }
            else if(isa<CallExpr>(init)){ //函数调用结点
                HandleCall(init);
            }
            else if(isa<IntegerLiteral>(init)){//在这个判断条件中表示int i = 1
                string varname = HelpHandleVarDecl(varDecl);
                llvm::errs()<< lineNum << ",op {" << varname  << "}<-{imm}\n";
            }
            else if(isa<ImplicitCastExpr>(init)){
                //llvm::errs()<<"\nbug\n";
                //init->dump();
                string sourcename = HelpVisitRightTree(init);
                if(sourcename[sourcename.size()-1]==',')
                    sourcename = sourcename.substr(0,sourcename.size()-1);
                llvm::errs()<< lineNum << ",op {" << varname << "}<-{" << sourcename << "}\n";
            }
            else if(isa<InitListExpr>(init)){
                //init->dump();
                string varname = HelpHandleVarDecl(varDecl);
                llvm::errs()<< lineNum << ",op {" << varname  << "[]}<-{imm}\n";
            }
        }
    }
    else if(isa<FunctionDecl>(D)){//这里的是一个函数的声明，需要获取这个函数的名称，返回值的类型

        VarDecl* varDecl = (VarDecl*)D;
        unsigned int lineNum = ModGetDeclLine(varDecl);
        FunctionDecl *FD = dyn_cast<FunctionDecl>(D);
        string fname =  FD->getNameAsString();
        if(FD->doesThisDeclarationHaveABody()){//如果这个函数声明带有函数体，则初步认为这是个本地的函数
            localFunctionNameCollect += fname;
            //D->dump();
        }//else D->dump();

        QualType returnType = (QualType)FD->getReturnType();
        string typeStr = returnType.getAsString();
        llvm::errs()<<  lineNum << ",Define,Function,"<< fname << "," << typeStr << "\n";
    }else if(isa<RecordDecl>(D)){
        //这个是结构体或是联合的处理
        HelpHandleRecordDecl(D);
    }
}

void ClangPluginASTVisitor::HandleOp(Stmt *S){
    //处理op,获取变量之间的数据传递关系
    //格式:line,op,{func@var}<-{func@var1,func@var2,func@var3,......}
    //说明:行号，本行是赋值操作，被修改的变量是var，修改来源是var1，var2，var3等
    ////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////////
    //S->dump();
    string safeOrNot = HandleSafeType(S);
    if(CompoundAssignOperator *CO = dyn_cast<CompoundAssignOperator>(S)){
        HelpHandleCompoundAssignOperator(CO,true,safeOrNot);
    }

    else if(BinaryOperator *BO = dyn_cast<BinaryOperator>(S)){
        if(BO->isAssignmentOp()){ //只处理赋值的表达式，即会影响变量值的表达式
		    //llvm::errs() << "This is an assign\n";
            FullSourceLoc fsl = context->getFullLoc(BO->getLocStart());
		    if (fsl.isValid()){
			    unsigned linenumber = fsl.getSpellingLineNumber(); //获取行数
			    Expr *exprl = BO->getLHS(); //获取左子树，即赋值的结点
			    string dst;
                dst = HelpGetDst(exprl);//获取左边的变量名
			    llvm::errs() <<  linenumber << ",op,{" << dst << "}<-{";
			    string src;
                Expr *exprr = BO->getRHS();//获取右子树
                src = HelpGetSrc(exprl,exprr,dst);//获取右侧的数据来源
                //如果src以逗号结束，则去掉这个逗号。
                if(src[src.size()-1]==',')
                    src = src.substr(0,src.size()-1);
                //string safeOrNot = HandleSafeType(S);
                llvm::errs() << src << "} "<< safeOrNot << "\n";
 		    }
	    }
    }
	else if(UnaryOperator *UO = dyn_cast<UnaryOperator>(S)){
        if(UO->isIncrementDecrementOp()){//一元操作结点,处理++类似的操作
            HelpHandleIncrementDecrementOp(UO,true,safeOrNot);
        }
    }
}

void ClangPluginASTVisitor::HandleCall(Stmt *S){//当前用于处理函数调用的指令
    //处理函数调用call
    //具体要求见KDFI设计文档
    FullSourceLoc fsl = context->getFullLoc(S->getLocStart());
    unsigned linenumber = fsl.getSpellingLineNumber();
    CallExpr *CE = dyn_cast<CallExpr>(S);
    //S->dump();
    string args="";
    for(CallExpr::arg_iterator it = CE -> arg_begin(); it != CE -> arg_end(); ++it) {
        Expr* arg = *it;
        args += HelpVisitRightTree(arg);//每一个参数都有可能是一个运算结果，需要进行树状分析
    }
    string directCallOrNot="";
    if(FunctionDecl *FD = CE->getDirectCallee()){
        //FD->dump();
        directCallOrNot += "Direct,";
        string functionName = FD->getNameAsString();
        directCallOrNot +=  functionName;
        std::size_t found = localFunctionNameCollect.find(functionName);//在本地函数集合中查找函数名，如果没有找到则判定为库函数
        if(found==std::string::npos){//判断是否为外部函数，但存在问题，如果对于多个源文件的情况是否使用还需验证
            //llvm::errs()<<"\n not found\n";
            directCallOrNot += ",Libc,Key";
        }
    }
    else{
        //S->dump();
        directCallOrNot += "Indirect,Key";
    }
    if(args[args.size()-1]==',')
        args = args.substr(0,args.size()-1);
    llvm::errs()<<linenumber<<",Call,"<< directCallOrNot << ",Para{"<<args<<"}\n";
}

void ClangPluginASTVisitor::HandleRet(Stmt *S){
    //处理函数返回ret
    //具体要求见KDFI设计文档
    //S->dump();
    FullSourceLoc fsl = context->getFullLoc(S->getLocStart());
    unsigned linenumber = fsl.getSpellingLineNumber();
    ReturnStmt *RS = dyn_cast<ReturnStmt>(S);
    string retStr = "";
    if(Expr *child = RS->getRetValue()){
        retStr += HelpVisitRightTree(child);
    }else
        retStr += "void";
    if(retStr[retStr.size()-1]==',')
        retStr = retStr.substr(0,retStr.size()-1);
    llvm::errs()<< linenumber << ",Ret,{}<-{"<< retStr << "}\n";
}

void ClangPluginASTVisitor::HandleCond(Stmt *S){
    unsigned int lineNum = ModGetDeclLine(S);
    //处理cond，条件分支的参数都是输入，也都是关键数据
    //格式:line,cond,type ,{}<-{func@para1,func@para2}
    //说明:行号，是条件分支，条件分支的具体类型(if,else,for,while,switch等) ，参数是para1和 para2等
	if(isa<IfStmt>(S)) {
        IfStmt* ifStmt = (IfStmt*)S;
        if(ifStmtSet.count(ifStmt) == 0) {  // 还未访问过这个 IfStmt
            Expr* expr = ifStmt -> getCond();
            string str = getCondStr(expr);
            llvm::errs() << lineNum  << "," << "cond" << "," << "if" << "," << "{}<-{" << str << "}" << "\n";
            ifStmtSet.insert(ifStmt);  // 标记已访问
            Stmt* elseIfStmtOrElseStmt = ifStmt -> getElse();
            int lineNumElseIfOrElse = 0;
            while(elseIfStmtOrElseStmt != NULL && isa<IfStmt>(elseIfStmtOrElseStmt)) {
                lineNumElseIfOrElse = ModGetDeclLine(elseIfStmtOrElseStmt);
                IfStmt* elseIfStmt = (IfStmt*)elseIfStmtOrElseStmt;
                ifStmtSet.insert(elseIfStmt);  // 标记已经访问过了
                Expr* expr = elseIfStmt -> getCond();
                string str = getCondStr(expr);
                llvm::errs() << lineNumElseIfOrElse  << "," << "cond" << "," << "else if" << "," << "{}<-{" << str << "}" << "\n";
                elseIfStmtOrElseStmt = elseIfStmt -> getElse();
            }
            //if(dyn_cast<Stmt>(elseIfStmtOrElseStmt)) {
            if(elseIfStmtOrElseStmt != NULL) {
                lineNumElseIfOrElse = ModGetDeclLine(elseIfStmtOrElseStmt);
                llvm::errs() << lineNumElseIfOrElse << "," << "cond" << "," << "else" << "\n";
            }
        }
    } else if(isa<ForStmt>(S)) {
        ForStmt* forStmt = (ForStmt*)S;
        //forStmt -> dump();
        Expr* expr = forStmt -> getCond();
        //expr -> dump();
        string str = getCondStr(expr);
        llvm::errs() << lineNum << "," << "cond" << "," << "for" << "," << "{}<-{" << str << "}" << "\n";
    } else if(isa<WhileStmt>(S)) {
        WhileStmt* whileStmt = (WhileStmt*)S;
        //whileStmt -> dump();
        Expr* expr = whileStmt -> getCond();
        //expr -> dump();
        string str = getCondStr(expr);
        llvm::errs() << lineNum << "," << "cond" << "," << "while" << "," << "{}<-{" << str << "}" << "\n";
    } else if(isa<SwitchStmt>(S)) {
        SwitchStmt* switchStmt = (SwitchStmt*)S;
        //switchStmt -> dump();
        Expr* expr = switchStmt -> getCond();
        //expr -> dump();
        string str = getCondStr(expr);
        llvm::errs() << lineNum << "," << "cond" << "," << "switch" << "," << "{}<-{" << str << "}" << "\n";
    }

}

void ClangPluginASTVisitor::HandleOthers(Stmt *S){
    //将该行代码原样输出，供人工检查
    //S->dump();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//                                       辅助函数区                                                  //
//   1. 为实现上述函数功能，可能需要自己定义一些辅助函数，但为了后续代码整合统一，建议遵循以下命名规则                //
//   2. 协助处理某个类的函数：以Help开头，如HelpFunctionDecl()即是协助HandleFunctionDecl()的函数，具体功能可在 //
//      命名中说明，如HelpGetFunctionNameFromFunctionDecl()                                           //
//   3. 实现某个子功能的函数：以Mod开头，如ModWriteToFile()即是独立的输出信息到文件的函数，具体输出信息可以在命名中  //
//      说明，如ModWriteDataFlowToFile()                                                             //
/////////////////////////////////////////////////////////////////////////////////////////////////////


string ClangPluginASTVisitor::HelpGetFunctionName(DeclRefExpr *e){//获取变量所在的函数名，若是全局变量则为global
    string funname;
    ValueDecl *VED = e->getDecl();
    NamedDecl *ND = dyn_cast<NamedDecl>(VED);
    Decl *D = dyn_cast<Decl>(ND);
    if(isa<FunctionDecl>(D)){
        if(FunctionDecl *FD = dyn_cast<FunctionDecl>(D)){
            string fname =  FD->getNameAsString();
            return fname;
        }
    }
    if(D->isDefinedOutsideFunctionOrMethod()){
        funname = "_Global_@";
    }
    else{
        DeclContext *DC = D->getParentFunctionOrMethod(); //获取作用范围的函数定义
        if(FunctionDecl *FD = dyn_cast<FunctionDecl>(DC)){
             //D->dump();
             funname = FD->getNameAsString()+ "@" ;
        }//else return "notfound";
    }
    return funname;
}
string ClangPluginASTVisitor::HelpHandleDeclRefExpr(DeclRefExpr *e){
    string valname;
    string funname;
    ValueDecl *VED = e->getDecl();
    funname = HelpGetFunctionName(e);
    if(VarDecl *VD = dyn_cast<VarDecl>(VED)){
		if(VD->hasGlobalStorage() && !VD->isStaticLocal()){
			valname = funname + VD->getNameAsString();
		}else{
			valname = funname + VD->getNameAsString();
	    }
	}
    else if(isa<FunctionDecl>(VED)){
        valname = "_FUN_@"+ funname;
    }
    //else valname = "papa";
    return valname;
}
string ClangPluginASTVisitor::HelpGetDst(Expr* expr){
    //这个函数用于获取赋值操作等号左边的标的数据

	string leftop;
	if(DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(expr)) //若赋值的结点的是变量
		leftop = HelpHandleDeclRefExpr(DRE);
    if(UnaryOperator *UO = dyn_cast<UnaryOperator>(expr)){
        if(UO->getOpcode() == UO_Deref){
            //leftop = "hi!";
            if(isa<ParenExpr>(*(expr->child_begin()))){
                //如果进入这个判断条件，说名出现左值是*()的情况，需要进行分析。
                ParenExpr *PE = dyn_cast<ParenExpr>(*(expr->child_begin()));
                if(isa<BinaryOperator>(*(PE->child_begin()))){
                    //如果进入这个判断条件，则说明左值是*(+),在内部出现运算，可以直接判断是非安全的操作。
                    //下面的操作负责将其中的指针字面量找出例如：*(a+i),则输出指针变量a.
                    BinaryOperator *BO = dyn_cast<BinaryOperator>(*(PE->child_begin()));
                    leftop += HelpFindPointerInLeftTree(BO);
                }
                if(isa<UnaryOperator>(*(PE->child_begin()))){
                    UnaryOperator *UO = dyn_cast<UnaryOperator>(*(PE->child_begin()));
                    if(UO->isIncrementDecrementOp()){//一元操作结点,处理++类似的操作,说名左边是*(p++)
                        leftop +="*";
                        leftop += HelpHandleIncrementDecrementOp(UO,false,"");
                        llvm::errs()<<"wrong\n";
                        return leftop;
                    }
                }
                else if(isa<CompoundAssignOperator>(*(PE->child_begin()))){
                    llvm::errs()<< "+=\n\n";
                }
            }
            string star = "";
            while(isa<UnaryOperator>(expr)){//在这个循环中，说名是多层指针的情况
				star += "*";
				expr =(Expr *)(*(expr->child_begin()));
				expr =(Expr *)(*(expr->child_begin()));
		    }
            if(DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(expr)){
                leftop = HelpPointerDereference(DRE, star);
            }
        }
    }
	else if(ArraySubscriptExpr *ASE = dyn_cast<ArraySubscriptExpr>(expr)){ //若赋值的结点是数组
        leftop = HelpHandleArraySubsriptExpr(ASE);
	}
    else if(MemberExpr *ME = dyn_cast<MemberExpr>(expr)){
        leftop = HelpHandleMemberExpr(ME);
    }
	return leftop;
}
string ClangPluginASTVisitor::HelpGetSrc(Expr* expl,Expr* expr,string leftop){
    //这个函数用于获取赋值操作中等号右边的源数据,但是等号左边的操作也会影响数据的来源分析，因此需要分析等号左边
    string src;
    string funname;
    //expr->dump();
    src = HelpVisitRightTree(expr);
    if(UnaryOperator *LUO = dyn_cast<UnaryOperator>(expl)){
        if(isa<ParenExpr>(*(LUO->child_begin()))){//等号左边出现*(p+i)的情况，说明需要继续分析
            src += HelpAnalyzeLeftTree(LUO,leftop);
        }
    }
    else if(ArraySubscriptExpr *ASE  = dyn_cast<ArraySubscriptExpr>(expl)){
        //expl->dump();
        if(Expr *subScrip = ASE->getIdx()){
            //if(ImplicitCastExpr *ICE = dyn_cast<ImplicitCastExpr>(subScrip)){
            //    if(DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(*(ICE->child_begin()))){
            //        src += HelpHandleDeclRefExpr(DRE);
            //    }
            //}
            src += HelpVisitRightTree(subScrip);
        }
    }
    return src;
}
string ClangPluginASTVisitor::HelpVisitRightTree(Expr * root){
    //这个函数用于访问等号右边出现的四则运算，可以处理多次运算，使用二叉树的后序遍历方法。 对于指针运算，最多处理两层
    string src = "";
    if(isa<BinaryOperator>(root)){
        BinaryOperator *par = dyn_cast<BinaryOperator>(root);
        Expr *lchild = par -> getLHS();
        Expr *rchild = par -> getRHS();
        src += HelpVisitRightTree(lchild);
        src += HelpVisitRightTree(rchild);
    }
    else if(isa<ImplicitCastExpr>(root)){
        if(DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(*(root->child_begin()))){
            src += HelpHandleDeclRefExpr(DRE) + ",";
        }
        if(UnaryOperator *UO = dyn_cast<UnaryOperator>(*(root->child_begin()))){
            if(ParenExpr *PE = dyn_cast<ParenExpr>(*(UO->child_begin()))){
                if (BinaryOperator *BO1 = dyn_cast<BinaryOperator>(*(PE->child_begin()))){
                    BinaryOperator *par = dyn_cast<BinaryOperator>(BO1);
                    Expr *lchild = par -> getLHS();
                    Expr *rchild = par -> getRHS();
                    if(isa<PointerType>(lchild->getType())){//如果进入这个条件，说明出现了*(..*p...)的表示
                        src += "*"+HelpVisitRightTree(lchild);
                    }else{
                        src += HelpVisitRightTree(lchild);
                    }
                    if(isa<PointerType>(rchild->getType())){
                        src += "*"+HelpVisitRightTree(rchild);
                    }else{
                        src += HelpVisitRightTree(rchild);
                    }
                }
            }
            if(UO->getOpcode() == UO_Deref){
                if(ImplicitCastExpr *ICE = dyn_cast<ImplicitCastExpr>(*(UO->child_begin()))){
                    if(isa<PointerType>(ICE->getType())){
                        src += "*" + HelpVisitRightTree(ICE);
                    }else{
                        src += HelpVisitRightTree(ICE);
                    }
                }
            }
            else if(UO->getOpcode()==UO_AddrOf){//关于这些判断条件，来源于Expr.cpp,在本判断条件之下，说名是取地址操作&,应当不存在二重取地址操作
                if(DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(*(UO->child_begin()))){
                    src += "&" + HelpHandleDeclRefExpr(DRE);
                }
            }
        }
        if(ArraySubscriptExpr *ASE = dyn_cast<ArraySubscriptExpr>(*(root->child_begin()))){
            src += HelpHandleArraySubsriptExpr(ASE)+",";
            if(Expr* Idx = ASE->getIdx()){
                //获取到了这个数组的索引信息，也就是方括号里面的内容，方括号里面的也可能是一个操作式，需要树状分析
                src += HelpVisitRightTree(Idx);
            }
        }
        else if(ImplicitCastExpr *NICE = dyn_cast<ImplicitCastExpr>(*(root->child_begin()))){
            src += HelpVisitRightTree(NICE);
        }
        else if(Expr * CE = dyn_cast<CallExpr>(*(root->child_begin()))){
            //CE->dump();
            src += HelpHandleFunctionCall(CE,false);
        }
        else if(isa<CharacterLiteral>(*(root->child_begin()))){
            src += "ImmChar";
        }
        else if(BinaryOperator *par = dyn_cast<BinaryOperator>(*(root->child_begin()))){
            //par->dump();
            Expr *lchild = par -> getLHS();
            Expr *rchild = par -> getRHS();
            src += HelpVisitRightTree(lchild);
            src += HelpVisitRightTree(rchild);
        }
        else if(isa<IntegerLiteral>(*(root->child_begin()))){
            src += "Imm,";
        }
        else if(isa<MemberExpr>(*(root->child_begin()))){//进到这个判断条件中说名右边出现结构体成员
            MemberExpr *ME = dyn_cast<MemberExpr>(*(root->child_begin()));
            src += HelpHandleMemberExpr(ME)+",";
        }

    }
    else if(isa<CallExpr>(root)){
        //llvm::errs()<<"Found a Function";
        src +=  HelpHandleFunctionCall(root,false);
        //root->dump();
    }
    else if(isa<IntegerLiteral>(root)){
        src += "Imm,";
    }
    else if(isa<UnaryOperator>(root)){
        UnaryOperator *UO = dyn_cast<UnaryOperator>(root);
        if(UO->isIncrementDecrementOp()){//一元操作结点,处理++类似的操作
            src += HelpHandleIncrementDecrementOp(UO,false,"") + ",";
        }
        if(UO->getOpcode()==UO_AddrOf){//在普通的单操作中发现＆操作
            if(DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(*(UO->child_begin()))){
                src += "&" + HelpHandleDeclRefExpr(DRE);
            }
        }
    }
    else if(isa<ParenExpr>(root)){//碰到了括号，则将括号中的内容取出，继续递归
        Expr *CONTENT = dyn_cast<Expr>(*root->child_begin());
        src += HelpVisitRightTree(CONTENT);
        if(isa<CompoundAssignOperator>(CONTENT)){
            CompoundAssignOperator *CO = dyn_cast<CompoundAssignOperator>(CONTENT);
            Expr *left = CO->getLHS();
            if(DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(left)){
                src += HelpHandleDeclRefExpr(DRE) + ",";
            }
        }
    }
    else if(isa<UnaryExprOrTypeTraitExpr>(root)){
        UnaryExprOrTypeTraitExpr *UEOTTE = dyn_cast<UnaryExprOrTypeTraitExpr>(root);
        if(UEOTTE->isArgumentType()){
            src+="Imm,";
        }
        else{
            Expr *arg = UEOTTE -> getArgumentExpr();
            src += HelpVisitRightTree(arg);
        }
    }

    //else root->dump();
    return src;
}
string ClangPluginASTVisitor::HelpPointerDereference(DeclRefExpr *DRE, string star){
    ValueDecl *VED = DRE->getDecl();
	string valname = star;
	if(VarDecl *VD = dyn_cast<VarDecl>(VED)){
		if(VD->hasGlobalStorage() && !VD->isStaticLocal()){
			valname += "_Global_@" + VD->getNameAsString();
		}else{
            string funcname = HelpGetFunctionName(DRE);
			valname += funcname  + VD->getNameAsString();
		}
	}
    return valname;
}
string ClangPluginASTVisitor::HelpHandleIncrementDecrementOp(UnaryOperator * UO,bool printout,string safeOrNot){
    FullSourceLoc fsl = context -> getFullLoc(UO -> getLocStart());
    string left;
    if(fsl.isValid()){
        unsigned linenumber = fsl.getSpellingLineNumber(); //记录行号
        Expr *expr = UO -> getSubExpr();
        DeclRefExpr *e;
        while((e = dyn_cast<DeclRefExpr>(expr))==NULL){ //得到变量名
			expr =(Expr *)(*(expr->child_begin()));
            if(isa<UnaryOperator>(expr)){
                left += "*";
            }
        }
        left += HelpHandleDeclRefExpr(e);
        if(printout){
            llvm::errs() << linenumber << ",op,{" << left << "}<-{" << left <<"} "<< safeOrNot <<"\n";
        }
    }
    return left;
}
string ClangPluginASTVisitor::HelpHandleCompoundAssignOperator(CompoundAssignOperator *CO,bool printout,string safeOrNot){
    FullSourceLoc fsl = context -> getFullLoc(CO -> getLocStart());
    string left = "";
    if(fsl.isValid()){
        unsigned linenumber =  fsl.getSpellingLineNumber();
        //DeclRefExpr *DRE = *(CO->child_begin());
        if(DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(*(CO->child_begin()))){
            left += HelpHandleDeclRefExpr(DRE);
        }
        if(printout){
            llvm::errs() << linenumber << ",op,{" << left << "}<-{" << left <<"，Imm} "<< safeOrNot <<"\n";
        }
    }
    return left;
}
string ClangPluginASTVisitor::HelpFindPointerInLeftTree(Expr *root){
    //这个函数帮助找到在左树中参与运算的指针变量，并返回这个指针的格式,最多能找到二重指针
    string thepointer="";
    if(isa<BinaryOperator>(root)){
        //thepointer +="111";
        BinaryOperator *par = dyn_cast<BinaryOperator>(root);
        Expr *lchild = par -> getLHS();
        Expr *rchild = par -> getRHS();
        thepointer += HelpFindPointerInLeftTree(lchild);
        thepointer += HelpFindPointerInLeftTree(rchild);
    }
    else if(isa<ImplicitCastExpr>(root)){
        string star = "*";
        if(isa<PointerType>(root->getType())){

            Expr *expr =(Expr *)(*(root->child_begin()));
            if(DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(expr)){
                thepointer = HelpPointerDereference(DRE, star);
            }
            if(isa<UnaryOperator>(expr)){
			    star += "*";
			    expr =(Expr *)(*(expr->child_begin()));
                if(isa<ParenExpr>(expr)){
                    expr =(Expr *)(*(expr->child_begin()));
                    if(BinaryOperator *BO = dyn_cast<BinaryOperator>(expr)){
                        //如果进到这个条件中，说明出现了*(*(p+i)+j)的情况
                        thepointer += "*";
                        thepointer += HelpFindPointerInLeftTree(BO);
                    }
                }
                expr = (Expr *)(*(expr->child_begin()));
			}
            if(DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(expr)){
            //string valname;
                thepointer = HelpPointerDereference(DRE, star);
            //llvm::errs() << valname << ",";
            }
        }
    }
    return thepointer;
}
string ClangPluginASTVisitor::HelpHandleFunctionCall(Expr *caller,bool printout){
    //这个函数专门应用与在等号左边出现对函数调用的情况
    string funname="";
    if(ImplicitCastExpr *ICE = dyn_cast<ImplicitCastExpr>(*(caller->child_begin()))){
        if(DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(*(ICE->child_begin()))){//进入到这个判断说名是直接调用
            ValueDecl *VED = DRE->getDecl();
            NamedDecl *ND = dyn_cast<NamedDecl>(VED);
            Decl *D = dyn_cast<Decl>(ND);
            //string funname="ret ";
            //Decl *D = dyn_cast<Decl>(DRE);
            if(FunctionDecl *FD = dyn_cast<FunctionDecl>(D)){
                funname += FD->getNameAsString();
            }
        }
        else if(ParenExpr *PE = dyn_cast<ParenExpr>(*(ICE->child_begin()))){
            Expr *E = dyn_cast<Expr>(*(PE->child_begin()));
            while(!isa<DeclRefExpr>(E)){
                E = dyn_cast<Expr>(*(E->child_begin()));
            }
            DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(E);
            funname +=   HelpHandleDeclRefExpr(DRE);
        }
    }
    CallExpr *CE = dyn_cast<CallExpr>(caller);
    if(CE->getDirectCallee()){

    }
    else if(CE->getCalleeDecl()){
        funname = "*" + funname;
    }
    else
        funname = "**" + funname;

    if(printout){
        llvm::errs()<< funname + ",";
    }
    return "Retfrom "+funname + ",";
}
string ClangPluginASTVisitor::HelpAnalyzeLeftTree(Expr *root,string leftop){
    //这个函数用于分析在等号左边的参与指针运算的普通变量，将这些普通变量加入src中。
    //目前只处理常规变量的情况
    //本函数规定，等号左边必须是一个*(p+i)类似的操作,目前只支持*(p+i+...)的操作，括号内必须只能有一个指针字面量，而后将其他变量输出
    //经过尝试，两个指针字面量是不允许做运算的，只允许指针字面量与普通变量做运算
    string src="";
    string tmp = "";
    //root->dump();
    if(isa<BinaryOperator>(root)){
        BinaryOperator *par = dyn_cast<BinaryOperator>(root);
        Expr *lchild = par -> getLHS();
        Expr *rchild = par -> getRHS();
        src += HelpAnalyzeLeftTree(lchild,leftop);
        src += HelpAnalyzeLeftTree(rchild,leftop);
    }
    else if(isa<UnaryOperator>(root)){
        UnaryOperator *UO = dyn_cast<UnaryOperator>(root);
        Expr *UON = dyn_cast<Expr>(*(UO->child_begin()));
        src += HelpAnalyzeLeftTree(UON,leftop);
    }
    else if(isa<ParenExpr>(root)){
        ParenExpr *PE = dyn_cast<ParenExpr>(root);
        Expr *PEN = dyn_cast<Expr>(*(PE->child_begin()));
        src += HelpAnalyzeLeftTree(PEN,leftop);
    }
    else if(isa<ImplicitCastExpr>(root)){
        ImplicitCastExpr *ICE = dyn_cast<ImplicitCastExpr>(root);
        Expr *ICEN = dyn_cast<Expr>(*(ICE->child_begin()));
        src += HelpAnalyzeLeftTree(ICEN,leftop);
    }
    else if(isa<DeclRefExpr>(root)){
        DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(root);
        tmp = HelpHandleDeclRefExpr(DRE);
        //if(isa<PointerType>(DRE->getType()))
        string stars = HelpGetStars(DRE);
        if( "*" + tmp == leftop || "**" + tmp  == leftop)
            src += "";
        else src += stars + tmp + ",";
    }
    //else root->dump();
    return src;
}
string ClangPluginASTVisitor::HelpHandleArraySubsriptExpr(Expr *ASE){//这个函数仅用来获取数组的名称
    string arrayname;
    //ASE->dump();
    if(ImplicitCastExpr *ICE = dyn_cast<ImplicitCastExpr>(*(ASE->child_begin()))){
        if(DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(*(ICE->child_begin()))){
            arrayname = HelpHandleDeclRefExpr(DRE);
        }
        else if(MemberExpr *ME = dyn_cast<MemberExpr>(*(ICE->child_begin()))){//进入到这个条件说明这个数组是一个结构体成员
            arrayname = HelpHandleMemberExpr(ME);
        }
    }
    return arrayname + "[]";
}
string ClangPluginASTVisitor::HelpGetType(Expr *exp){
    QualType T = exp->getType();
    SplitQualType T_split = T.split();
    string type = QualType::getAsString(T_split);
    return type;
}
string ClangPluginASTVisitor::HelpGetStars(Expr *exp){
    string type = HelpGetType(exp);
    string stars = "";
    if(type == "int *") return "*";
    if(type == "int **") return "**";
    if(type == "char **") return "**";
    if(type == "char *") return "*";
    return stars;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 添加或修改函数：衣龙浩
string ClangPluginASTVisitor::getCondStr(Stmt* root) {
    queue<Stmt*> q1 = getAllChilds(root);
    queue<Stmt*> q2 = extractChilds(q1);
    return concatStr(q2);
}
string ClangPluginASTVisitor::concatStr(queue<Stmt*> q) {
    string res = "";
    string nameStr = "";
    string funcStr = "";
    string funcAndNameStr = "";  // func@name
    vector<string> strsVec;
    while(!q.empty()) {
        Stmt* temp = q.front();
        q.pop();  // 出队列
        if(isa<IntegerLiteral>(temp) || isa<FloatingLiteral>(temp)) {
            strsVec.push_back("imm");
        } else if(isa<DeclRefExpr>(temp)) {  // DeclRefExpr
            // 获取变量名
            DeclRefExpr* declRefExpr = (DeclRefExpr*)temp;
            ValueDecl* valueDecl = declRefExpr -> getDecl();
            nameStr = valueDecl -> getNameAsString();
            // 判断是全局变量还是局部变量
            if(valueDecl -> isDefinedOutsideFunctionOrMethod()) {
                // 全局变量对应的函数定义设定为 _global_
                funcStr = "_global_";
            } else {
                // 获取作用域范围对应的函数定义
                DeclContext* declContext = valueDecl -> getParentFunctionOrMethod();
                // 获取函数名
                if(FunctionDecl* functionDecl = dyn_cast<FunctionDecl>(declContext)){
                    funcStr = functionDecl -> getNameAsString();
                }
            }
            funcAndNameStr = funcStr + "@" + nameStr;
            strsVec.push_back(funcAndNameStr);
        }
    }
    // 拼接字符串
    for(unsigned int i = 0; i < strsVec.size(); i++) {
        res += strsVec[i];
        res += ",";
    }
    res = res.substr(0, res.length() - 1); // 去掉最后一个多余的逗号
    return res;
}
queue<Stmt*> ClangPluginASTVisitor::getAllChilds(Stmt* root) {
    queue<Stmt*> stmtQueue; // 最终要返回的队列
    queue<Stmt*> stmtTempQueue;  // 临时队列
    stmtTempQueue.push(root);  // 根节点入队
    while(!stmtTempQueue.empty()) {
        Stmt* temp = stmtTempQueue.front();
        stmtTempQueue.pop();  // 出队列
        stmtQueue.push(temp);  // 出了临时队列，则进入另一个队列永久保存
        for(Stmt::child_iterator it = temp -> child_begin(); it != temp -> child_end(); ++it) {
            Stmt* child = *it;
            stmtTempQueue.push(child);
        }
    }
    return stmtQueue;
}
queue<Stmt*> ClangPluginASTVisitor::extractChilds(queue<Stmt*> q) {
    queue<Stmt*> stmtTempQueue;
    queue<Stmt*> declRefExprQueue;
    queue<Stmt*> returnQueue;
    while(!q.empty()) {
        Stmt* temp = q.front();
        stmtTempQueue.push(temp);
        q.pop(); // 出队列
        if(isa<DeclRefExpr>(temp)) {
            declRefExprQueue.push(temp);
        }
        //if(isa<ImplicitCastExpr>(temp) || isa<DeclRefExpr>(temp) || isa<IntegerLiteral>(temp) || isa<CallExpr>(temp)) {  // VarDecl
        //    stmtExtractQueue.push(temp);
        //}
    }
    int skipFlag = 0;
    while(!stmtTempQueue.empty()) {
        Stmt* temp = stmtTempQueue.front();
        stmtTempQueue.pop();
        if(isa<IntegerLiteral>(temp) || isa<FloatingLiteral>(temp)) {
            returnQueue.push(temp);
            if(isa<FloatingLiteral>(temp)) {
                skipFlag = 1;
            }
        } else if(isa<ImplicitCastExpr>(temp)) {
            if(skipFlag == 1) {
                skipFlag = 0;
                continue;
            } else {
                Stmt* declRefExpr = declRefExprQueue.front();
                declRefExprQueue.pop();
                returnQueue.push(declRefExpr);
            }
        }
    }
    return returnQueue;
}
unsigned ClangPluginASTVisitor::ModGetDeclLine(Decl *D){
    //获取定义在源代码中的行号
    FullSourceLoc fsl = context->getFullLoc(D->getLocStart());
    if(fsl.isValid())
        return fsl.getSpellingLineNumber();
    else
        return 0;
}
unsigned ClangPluginASTVisitor::ModGetDeclLine(Stmt *S){
    //获取定义在源代码中的行号
    FullSourceLoc fsl = context->getFullLoc(S->getLocStart());
    if(fsl.isValid())
        return fsl.getSpellingLineNumber();
    else
        return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
string ClangPluginASTVisitor::HelpHandleVarDecl(VarDecl * VD){
    //这个函数帮助获取变量定义的作用域和名称，只返回　作用域@变量名的格式。
    string nameStr = VD -> getNameAsString();
    if(VD -> isDefinedOutsideFunctionOrMethod()){  // 全局变量
            //llvm::errs() << lineNum << "," << "def" << "," << "_global_" << "@" << nameStr << "," << typeStr << "," << keyOrNormal <<"\n";
            return "_Global_@" + nameStr;
    } else {  // 局部变量
        DeclContext* declContext = VD -> getParentFunctionOrMethod(); // 获取作用域范围对应的函数定义
        if(FunctionDecl* functionDecl = dyn_cast<FunctionDecl>(declContext)){
            string funcStr = functionDecl -> getNameAsString();  // 函数名
            //llvm::errs() << lineNum << "," << "def" << "," << funcStr << "@" << nameStr << "," << typeStr << "\n";
            return funcStr+"@"+nameStr;
        }
    }
    return "";
}

///////////////////////////////////////
//GYH
///////////////////////////////////////
string  ClangPluginASTVisitor::HandleSafeType(Stmt *S){
    //S->dump();
    string Stype= "";
    string Stypel = "";
    string Styper = "";
    string result = "";
    if(CompoundAssignOperator *CO = dyn_cast<CompoundAssignOperator>(S)){//+=操作
        FullSourceLoc fsl = context -> getFullLoc(CO -> getLocStart());
        string left = "";
        if(fsl.isValid()){
            if(DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(*(CO->child_begin()))){
                if(isa<PointerType>(DRE->getType())){
                    Stype="unsafePointer  ";
                }
            }
            //llvm::errs() << Stype ;
            result+= Stype;
        }
    }

    else if(BinaryOperator *BO = dyn_cast<BinaryOperator>(S)){
        if(BO->isAssignmentOp()){ //只处理赋值的表达式，即会影响变量值的表达式
            FullSourceLoc fsl = context->getFullLoc(BO->getLocStart());
            if (fsl.isValid()){
                Expr *exprl = BO->getLHS(); //获取左子树，即赋值的结点
                string dst;
                if(DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(exprl)){
                    if(isa<PointerType>(DRE->getType())){ //若左边是指针变量，需要对右边进行判断
                        Expr *exprr = BO->getRHS();
                        if((isa<BinaryOperator>(exprr))||(isa<IntegerLiteral>(exprr))){//若右边为立即数或者运算则判断为非安全指针
                            Stype = "unsafePointer  ";
                        }
                    }
                }
                else if(isa<UnaryOperator>(exprl)){//对于左边是一元操作的情况，判断是否为可变的写值
                    Stypel = HelpGetDstSTUO(exprl);
                }
                string src;
                Expr *exprr = BO->getRHS();//获取右子树
                Styper = HelpGetSrcST(exprr);
                if(Styper!="")
                	Styper="unsafeRead";
                //llvm::errs() << Stype<<Stypel<<Styper;
                result+= Stype +Stypel+Styper;
            }
        }
    }
    else if(UnaryOperator *UO = dyn_cast<UnaryOperator>(S)){
        if(UO->isIncrementDecrementOp()){//一元操作结点,处理++类似的操作
            FullSourceLoc fsl = context -> getFullLoc(UO -> getLocStart());
            if(fsl.isValid()){
                Expr *expr = UO -> getSubExpr();
                DeclRefExpr *e;
                while((e = dyn_cast<DeclRefExpr>(expr))==NULL){ //得到变量名
                    expr =(Expr *)(*(expr->child_begin()));
                }
                if(isa<PointerType>(e->getType())){
                    Stype = "unsafePointer  ";
                }
                //llvm::errs() << Stype;
                result+= Stype;
            }

        }
    }
    //if((Stype!="") || (Stypel!="") || (Styper!=""))
    //    llvm::errs()<<"\n";
    return result;
}
string ClangPluginASTVisitor::HelpGetDstSTUO(Expr* expr){
    //这个函数用于获取赋值操作等号左边的标的数据
    string Stypel;
    if(UnaryOperator *UO = dyn_cast<UnaryOperator>(expr)){
        if(UO->getOpcode() == UO_Deref){
            if(isa<ParenExpr>(*(expr->child_begin()))){
                //如果进入这个判断条件，说名出现左值是*()的情况，需要进行分析。
                ParenExpr *PE = dyn_cast<ParenExpr>(*(expr->child_begin()));
                if(isa<BinaryOperator>(*(PE->child_begin()))){
                    //如果进入这个判断条件，则说明左值是*(+)
                    //下面的操作负责将其中的指针字面量找出例如：*(a+i),则输出指针变量a.
                    BinaryOperator *BO = dyn_cast<BinaryOperator>(*(PE->child_begin()));
                    Expr *exprr = BO->getRHS();
                    if(isa<IntegerLiteral>(exprr)){
                        Stypel = "";
                    }
                    else{
                            QualType T = exprr->getType();
                            SplitQualType T_split = T.split();
                            string type = QualType::getAsString(T_split);
                            if(type=="int")
                                Stypel = "unsafeWrite  ";
                    }
                }
            }
        }
    }
    //else if(ArraySubscriptExpr *ASE = dyn_cast<ArraySubscriptExpr>(expr)){ //若赋值的结点是数组
        //leftop = HelpHandleArraySubsriptExpr(ASE);
    //}
    return Stypel;
}
string ClangPluginASTVisitor::HelpGetSrcST(Expr * root){
    //这个函数用于访问等号右边出现的四则运算
    string Styper = "";
    //string src;
    int i=0;
    if(isa<BinaryOperator>(root)){
        BinaryOperator *par = dyn_cast<BinaryOperator>(root);
        Expr *lchild = par -> getLHS();
        Expr *rchild = par -> getRHS();
        Styper += HelpGetSrcST(lchild);
        Styper += HelpGetSrcST(rchild);
    }
    else if(isa<ImplicitCastExpr>(root)){
        if(UnaryOperator *UO = dyn_cast<UnaryOperator>(*(root->child_begin()))){
            if(ParenExpr *PE = dyn_cast<ParenExpr>(*(UO->child_begin()))){
                if (BinaryOperator *BO1 = dyn_cast<BinaryOperator>(*(PE->child_begin()))){
                    BinaryOperator *par = dyn_cast<BinaryOperator>(BO1);
                    //Expr *lchild = par -> getLHS();
                    Expr *rchild = par -> getRHS();
                    if(isa<IntegerLiteral>(rchild)){
                       i++;
                    }
                    else{
                            QualType T = rchild->getType();
                            SplitQualType T_split = T.split();
                            string type = QualType::getAsString(T_split);
                            if(type=="int"){
                                Styper = "unsafeRead  ";
                            }
                    }
                }
            }
        }
        //if(ArraySubscriptExpr *ASE = dyn_cast<ArraySubscriptExpr>(*(root->child_begin()))){
            //src += HelpHandleArraySubsriptExpr(ASE)+",";
        //}
        if(ImplicitCastExpr *NICE = dyn_cast<ImplicitCastExpr>(*(root->child_begin()))){
            Styper += HelpGetSrcST(NICE);
        }
    }
    else if(isa<ParenExpr>(root)){//碰到了括号，则将括号中的内容取出，继续递归
        Expr *CONTENT = dyn_cast<Expr>(*root->child_begin());
        Styper += HelpGetSrcST(CONTENT);
    }
    return Styper;
}
//===================================================================================================
void ClangPluginASTVisitor::HelpHandleRecordDecl(Decl *D){
    unsigned int lineNum = ModGetDeclLine(D);
    //D -> dump();
    RecordDecl *RD = dyn_cast<RecordDecl>(D);
    string name = RD -> getNameAsString();
    string type = "";
    string fields = "";
    if( RD -> getTagKind() == TTK_Struct ){
        type = "Struct";
    }
    if( RD-> getTagKind() == TTK_Union ){
        type = "Union";
    }
    for(RecordDecl::field_iterator it = RD -> field_begin(); it != RD -> field_end(); ++it) {
        //it->dump();
        FieldDecl* field = *it;
        string fieldName = field -> getNameAsString();
        QualType qualType = field->getType();
        string typeStr = qualType.getAsString();
        fields += fieldName + "#"+ typeStr + ",";
    }
    fields = HelpCutTheLastComma(fields);
    llvm::errs()<< lineNum <<",Define,"<< type <<","<< name <<","<<"Fields:"<< fields <<"\n";
}
string ClangPluginASTVisitor::HelpCutTheLastComma(string src){
    if(src[src.size()-1]==',')
        src = src.substr(0,src.size()-1);
    return src;
}
string ClangPluginASTVisitor::HelpHandleMemberExpr(MemberExpr *ME){//这个函数用于处理对结构体成语的赋值，获取格式函数＠结构体名称＃成员名称
    string baseName = "";
    ValueDecl *VD = ME->getMemberDecl();
    string memberName = VD->getNameAsString();
    if(DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(*(ME->child_begin()))){
        //DRE->dump();
        baseName += HelpHandleDeclRefExpr(DRE);
    }
    return baseName+"#"+memberName;
}
