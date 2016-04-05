#include "clang/Driver/Options.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/AST/Expr.h"
#include "clang/Lex/Lexer.h"

using namespace std;
using namespace llvm;
using namespace clang;

clang::Rewriter rewriter;
unsigned int __function_index;

class FunctionSplitter : public clang::RecursiveASTVisitor<FunctionSplitter> {
public:
    FunctionSplitter(clang::CompilerInstance *ci)
        : ast_context_(&(ci->getASTContext())) {
        stmtParser.localVarDecls = &localVarDecls;
        stmtParser.constantDecls = &constantDecls;
        stmtParser.varDeclsInsideSplitRange = &varDeclsInsideSplitRange;
        stmtParser.varDeclStringsInsideSplitRange = &varDeclStringsInsideSplitRange;
        rewriter.setSourceMgr(ast_context_->getSourceManager(),
                ast_context_->getLangOpts());
        functionIndex = 0;
        stmtParser.ast_context_ = ast_context_;
        splitMethod = STRING;
        //nStmts = 0;
    }

    enum SPLIT_METHOD { STRING, LOOP_ONLY };

    virtual bool VisitFunctionDecl(FunctionDecl *fd);

    std::set<string *> *rewrittenFunctions;
    std::set<string *> *rewrittenFunctionDecls;
    SourceLocation *mainFunctionDecl;
    SourceLocation *firstFunctionDecl;
    clang::ASTContext* ast_context_; // used for getting additional AST info
    SPLIT_METHOD splitMethod;
    QualType currentFunctionReturnType;

    class StmtParser : public clang::RecursiveASTVisitor<StmtParser> {
    public:
        SourceLocation splitStartLocation;
        bool isLocalVariable(DeclRefExpr *st);
        virtual bool VisitDeclRefExpr(DeclRefExpr *st);
        virtual bool VisitVarDecl(VarDecl *st);
        set<VarDecl *> *localVarDecls;
        set<VarDecl *> *constantDecls;
        set<VarDecl *> *varDeclsInsideSplitRange;
        set<SourceLocation> rewrittenLocations;
        list<string> *varDeclStringsInsideSplitRange;
        ASTContext* ast_context_;
    };

    StmtParser stmtParser;



private:
    set<VarDecl *> localVarDecls;
    set<VarDecl *> constantDecls;
    set<VarDecl *> varDeclsInsideSplitRange;
    list<string> varDeclStringsInsideSplitRange;
    //list<Stmt *> splittedStmts;
    //list<Stmt *> possiblySplittedStmts;
    unsigned int functionIndex;
    //unsigned int nStmts;

    bool isSplitStartPoint(Stmt *stmt);
    bool isSplitEndPoint(Stmt *stmt);
    //void splitFunction(SourceLocation start, SourceLocation end);
    void splitFunction(list<Stmt *> *splittedStmts);
    bool reallySplit(list<Stmt *> *splittedStmts);
    void selectSplitRegion(Stmt *compoundStmt, int rootFunctionSize=-1);
    string getNewFunctionName();
};


class FunctionSplitASTConsumer : public clang::ASTConsumer {
public:
    explicit FunctionSplitASTConsumer(clang::CompilerInstance* ci)
        : visitor_(new FunctionSplitter(ci)) // initialize the visitor_
    {
        visitor_->rewrittenFunctions = &rewrittenFunctions;
        visitor_->rewrittenFunctionDecls = &rewrittenFunctionDecls;
        visitor_->mainFunctionDecl = &mainFunctionDecl;
        visitor_->firstFunctionDecl = &firstFunctionDecl;
    }

    void rewriteFunctions();
    void rewriteFunctionDecls();

    virtual void HandleTranslationUnit(clang::ASTContext& context)
    {
        visitor_->TraverseDecl(context.getTranslationUnitDecl());
        rewriteFunctions();
        rewriteFunctionDecls();
    }

    std::set<string *> rewrittenFunctions;
    std::set<string *> rewrittenFunctionDecls;
    SourceLocation mainFunctionDecl;
    SourceLocation firstFunctionDecl;

private:
    FunctionSplitter* visitor_; // doesn't have to be private
};


class FunctionSplitFrontendAction : public clang::ASTFrontendAction {
public:
    virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& ci,
                                                  llvm::StringRef file)
    {
        std::unique_ptr<clang::ASTConsumer> p(new FunctionSplitASTConsumer(&ci)); // pass CI pointer to ASTConsumer
        return p;
    }
};
