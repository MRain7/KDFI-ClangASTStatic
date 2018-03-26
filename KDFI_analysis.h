#pragma once

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
        if(FunctionDecl *FD = dyn_cast<FunctionDecl>(D)){  //decl the function
            funcName = FD->getNameAsString(); //set function name
        }
		return true;
	}
	bool VisitStmt(Stmt *S){
		//对每个Stmt结点分析
        if(isa<BinaryOperator>(S) || isa<UnaryOperator>(S)){ //操作结点
            HandleOp(S);
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

    string funcName;
    SourceManager &SM = Instance.getSourceManager();

    void HandleDefine(Decl *D);
    void HandleOp(Stmt *S);
    void HandleCall(Stmt *S);
    void HandleRet(Stmt *S);
    void HandleCond(Stmt *S);
    void HandleOthers(Stmt *S);

    unsigned HelpGetLineNumber(Stmt *S);
    string HelpGetCallerArgument(string callerName, CallExpr *CE);
    string HelpGetMaybeChangeCallerArgument(string callerName, CallExpr *CE);
    string HelpGetCalleeName(Decl *D, Stmt *S);
    string HelpGetCallType(CallExpr *CE);
    string HelpGetFunctionReturnValue(string callerName, Expr *E);
    string HelpGetDeclName(DeclRefExpr *DRE);
    string HelpGetVarType(string type);

};


void ClangPluginASTVisitor::HandleDefine(Decl *D){

}

void ClangPluginASTVisitor::HandleOp(Stmt *S){
}

void ClangPluginASTVisitor::HandleCall(Stmt *S){
    //get the callExpr root
    CallExpr *CE = dyn_cast<CallExpr>(S);

    //caller
    string callerName = funcName;
    unsigned lineNum = 0;
    lineNum= HelpGetLineNumber(S);
    string callerArgument;
    callerArgument = HelpGetCallerArgument(callerName,CE);

    string maybeChangeCallerArgument;
    maybeChangeCallerArgument = HelpGetMaybeChangeCallerArgument(callerName,CE);

    //callee
    Decl *calleeDecl = CE->getCalleeDecl();
    string calleeName;
    calleeName = HelpGetCalleeName(calleeDecl,S);

    //judge function call type: direct or Indirect;
    string callType = HelpGetCallType(CE);

    //output
    llvm::errs() << lineNum << ", call, " << callType << ", " << calleeName << ", " << "{" << callerName << "#" << calleeName << "}<-{" << callerArgument << "}, " << "{Op}<-{" << callerName << "#" << calleeName << "}, {" << maybeChangeCallerArgument << "}<-{" << callerName << "#" << calleeName << "}\n";
    llvm::errs() << lineNum << ", call, " << "Flag: CallStackAddress" << ", key" << "\n";
}

void ClangPluginASTVisitor::HandleRet(Stmt *S){
    string callerName = funcName;
    unsigned lineNum = HelpGetLineNumber(S);

    ReturnStmt *RS = dyn_cast<ReturnStmt>(S);
    Expr *E = RS->getRetValue();

    string returnValue;
    returnValue = HelpGetFunctionReturnValue(callerName, E);

    //output
    llvm::errs() << lineNum << ", ret, " << "{}<-{" << returnValue << "}\n";
    llvm::errs() << lineNum << ", ret, " << "Flag: RetStackAddress" << ", key" << "\n";
}

void ClangPluginASTVisitor::HandleCond(Stmt *S){
}

void ClangPluginASTVisitor::HandleOthers(Stmt *S){

}

//============================================================================================
//
//============================================================================================

unsigned ClangPluginASTVisitor::HelpGetLineNumber(Stmt *S){
    unsigned lineNum = 0;
    FullSourceLoc fsl = context->getFullLoc(S->getLocStart());
    if(fsl.isValid()){
        lineNum = fsl.getSpellingLineNumber();   //get the location line number;
    }
    return lineNum;
}

string ClangPluginASTVisitor ::HelpGetCallerArgument(string callerName, CallExpr *CE){
    string callerArgument;
    string callerArgumentType;
    string star;
    string callerArgumentName;
    for(CallExpr::arg_iterator i = CE->arg_begin(), e = CE->arg_end(); i != e; i++){
        callerArgumentType = QualType::getAsString((*i)->getType().split());
        star = HelpGetVarType(callerArgumentType);
        if(DeclRefExpr * DRE = dyn_cast<DeclRefExpr>(*(*i)->child_begin())){
            callerArgumentName = HelpGetDeclName(DRE);
        }
        callerArgument += star + callerName + "@" + callerArgumentName + ", ";
    }
    return callerArgument;
}

string ClangPluginASTVisitor::HelpGetMaybeChangeCallerArgument(string callerName, CallExpr *CE){

    string maybeChangeCallerArgument;
    string callerArgumentType;
    string star;
    string callerArgumentName;
    for(CallExpr::arg_iterator i = CE->arg_begin(), e = CE->arg_end(); i != e; i++){
        callerArgumentType = QualType::getAsString((*i)->getType().split());
        star = HelpGetVarType(callerArgumentType);
        if(DeclRefExpr * DRE = dyn_cast<DeclRefExpr>(*(*i)->child_begin())){
            callerArgumentName = HelpGetDeclName(DRE);
        }
        if(star != ""){
            maybeChangeCallerArgument += star + callerName + "@" + callerArgumentName + ", ";
        }
    }
    return maybeChangeCallerArgument;
}

string ClangPluginASTVisitor::HelpGetCalleeName(Decl *D, Stmt *S){
    string indirectType;  //Indirect callee type
    string indirectTypeFlag; //flag about Indirect call type
    string calleeName;

    //Direct call type, eg. a = fun();
    if(FunctionDecl *FD = dyn_cast<FunctionDecl>(D)){
        calleeName = FD->getNameAsString();
        //for(FunctionDecl::param_iterator i = FD->param_begin(), e = FD->param_end(); i != e; i++){
            //string star;
            //string calleeArgumentType;
            //calleeArgumentType = QualType::getAsString((*i)->getType().split());
            //if(calleeArgumentType.find("**") != string::npos){
                //star = "**";
            //}
            //else if(calleeArgumentType.find("*") != string::npos){
                //star = "*";
            //}
            //else{
                //star = "";
            //}
            //calleeArgument += star + calleeName + "@" + (*i)->getNameAsString() + ", ";
        //}
    }
    else{   //handel Indirect call, eg. *func = swap(int **, int **);
        Expr *e = dyn_cast<Expr>(S);
        if(ImplicitCastExpr *ICE = dyn_cast<ImplicitCastExpr>(*(e->child_begin()))){
            if(ParenExpr *PE = dyn_cast<ParenExpr>(*(ICE->child_begin()))){
                Expr *E = dyn_cast<Expr>(*(PE->child_begin()));
                while(!isa<DeclRefExpr>(E)){
                    E = dyn_cast<Expr>(*(E->child_begin()));
                }
                DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(E);
                indirectType = QualType::getAsString(DRE->getType().split());
                indirectTypeFlag = HelpGetVarType(indirectType);
                //get the callee funcaname
                ValueDecl *VED = DRE->getDecl();
                VarDecl *VD = dyn_cast<VarDecl>(VED);
                calleeName = indirectTypeFlag + VD->getNameAsString();
            }
        }
        else{
            calleeName = "";
        }
    }
    return calleeName;

}

string ClangPluginASTVisitor::HelpGetCallType(CallExpr *CE){
    string callType;
    if(CE->getDirectCallee()){
        callType = "Direct";
    }
    else{
        callType = "Indirect";
    }
    return callType;
}

string ClangPluginASTVisitor::HelpGetFunctionReturnValue(string callerName, Expr *E){
    string returnValue;
    string returnValueType;
    string star;
    string returnValueName;
    returnValueType = QualType::getAsString((E)->getType().split());
    star = HelpGetVarType(returnValueType);
    if(DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(*(E->child_begin()))){  //ret a;
        returnValueName = HelpGetDeclName(DRE);
    }
    //else if()
    returnValue += star + callerName + "@" + returnValueName + ", ";
    return returnValue;
}

string ClangPluginASTVisitor::HelpGetDeclName(DeclRefExpr *DRE){
    ValueDecl *VED = DRE->getDecl();
    if(VarDecl *VD = dyn_cast<VarDecl>(VED)){
        return VD->getNameAsString();
    }
    else if(FunctionDecl *FD = dyn_cast<FunctionDecl>(VED)){
        return FD->getNameAsString();
    }
    else{
        return "";
    }
}

string ClangPluginASTVisitor::HelpGetVarType(string type){
    if(type.find("**") != string::npos){
        return "**";
    }
    else if(type.find("*") != string::npos){
        return "*";
    }
    else{
        return "";
    }
}
