#include "functionSplit.h"

#include <fstream>

using namespace std;
using namespace llvm;
using namespace clang;


int main(int argc, const char **argv) {
    cl::OptionCategory functionSplitterCategory("function splitter options");
    clang::tooling::CommonOptionsParser op(argc, argv, functionSplitterCategory);

    clang::tooling::ClangTool tool(op.getCompilations(), op.getSourcePathList());

    for(int i=0; i<argc; i++) {
        errs() << argv[i] << "\n";
    }

    tool.run(
            clang::tooling::newFrontendActionFactory<FunctionSplitFrontendAction>().get()
            );

    FileID mainFileID = rewriter.getSourceMgr().getMainFileID();
    /*
    const FileEntry *fileEntry = rewriter.getSourceMgr().getFileEntryForID(mainFileID);
    string fileName = fileEntry->getName();
    */
    string fileName = "";
    ofstream outFile;
    outFile.open(fileName+"_modified.c");
    const RewriteBuffer *rewriteBuf = rewriter.getRewriteBufferFor(mainFileID);
    outFile << string(rewriteBuf->begin(), rewriteBuf->end());
    outFile.close();

    //rewriter.overwriteChangedFiles();

    return __function_index;
}


bool FunctionSplitter::VisitFunctionDecl(FunctionDecl *fd) {
    if(fd->isMain()) *mainFunctionDecl = fd->getLocStart();

    if(rewriter.getSourceMgr().isInMainFile(fd->getLocStart())) {
        if(!firstFunctionDecl->isValid()) {
            *firstFunctionDecl = rewriter.getSourceMgr().getFileLoc(
                    fd->getLocStart());
        }
        else {
            FullSourceLoc fdLoc(rewriter.getSourceMgr().getFileLoc(fd->getLocStart()),
                    rewriter.getSourceMgr());
            if(fdLoc.isBeforeInTranslationUnitThan(*firstFunctionDecl)) {
                *firstFunctionDecl = rewriter.getSourceMgr().getFileLoc(
                        fd->getLocStart());
            }
        }
    }

    if(fd->isThisDeclarationADefinition()) {
        currentFunctionReturnType = fd->getReturnType();
        Stmt *body = fd->getBody();
        bool started = false, ended = false;
        SourceLocation start, end;
        Stmt *prevSt = NULL;
        unsigned int nStmts = 0;
        list<Stmt *> splittedStmts;
        list<Stmt *> possiblySplittedStmts;
        for(auto st : body->children()) {
            //save this point so if the function is too long, cut it
            if(!started && !isa<DeclStmt>(st)) {
                if(ForStmt *forStmt = dyn_cast<ForStmt>(st)) {
                    if(CompoundStmt *body = dyn_cast<CompoundStmt>(forStmt->getBody())) {
                        nStmts += body->size();
                    }
                } else if(WhileStmt * whileStmt = dyn_cast<WhileStmt>(st)) {
                    if(CompoundStmt *body = dyn_cast<CompoundStmt>(whileStmt->getBody())) {
                        nStmts += body->size();
                    }
                } else {
                    nStmts++;
                }
                possiblySplittedStmts.push_back(st);
                if(nStmts >= 10) {
                    errs() << "split function since nStmts >= 10\n";
                    splittedStmts.assign(possiblySplittedStmts.begin(),
                            possiblySplittedStmts.end());

                    start = rewriter.getSourceMgr().getFileLoc(
                            possiblySplittedStmts.front()->getLocStart());

                    errs() << start.printToString(rewriter.getSourceMgr()) << "\n";

                    stmtParser.splitStartLocation = start;
                    started = true;
                    ended = true;
                }
            }

            //Check start & end point
            if(!started && !ended && isSplitStartPoint(st)) {
                nStmts = 0;
                splittedStmts.clear();
                possiblySplittedStmts.clear();
                if(splitMethod == LOOP_ONLY) {
                    start = rewriter.getSourceMgr().getFileLoc(
                            st->getLocStart());
                    stmtParser.splitStartLocation = start;
                    splittedStmts.push_back(st);
                    started = true;
                    ended= true;
                } else if(splitMethod == STRING) {
                stmtParser.splitStartLocation = start;
                started = true;
                }
            }
            if(started && !ended && isSplitEndPoint(st)) {
                ended = true;
            }

            if(started && !ended) {
                splittedStmts.push_back(st);
            }

            //start and end location are found
            if(ended) {
                for(auto stmt : splittedStmts) {
                    stmtParser.TraverseStmt(stmt);
                }

                errs() << "start spliting from:" << start.printToString(rewriter.getSourceMgr()) << "\n";
                errs() << " to:" << end.printToString(rewriter.getSourceMgr()) << "\n";

                splitFunction(&splittedStmts);

                started = false;
                ended = false;
                stmtParser.rewrittenLocations.clear();
                SourceLocation temp;
                stmtParser.splitStartLocation = temp;
                nStmts = 0;
                splittedStmts.clear();
                possiblySplittedStmts.clear();
            }

            prevSt = st;
        }
    }
    return true;
}


string FunctionSplitter::getNewFunctionName() {
    //need to check if there is same name already existing
    string name = "__generated__" + to_string(functionIndex++);
    __function_index = functionIndex;
    return name;
}


//void FunctionSplitter::splitFunction(SourceLocation start, SourceLocation end) {
void FunctionSplitter::splitFunction(list<Stmt *> *splittedStmts) {
    string splitFunctionCall = "";
    LangOptions langOpts;
    for(auto st : varDeclsInsideSplitRange) {
        SourceLocation end = Lexer::getLocForEndOfToken(st->getLocEnd(), 0, rewriter.getSourceMgr(), 
                langOpts);
        SourceRange range(st->getLocStart(), end);
        if(!st->hasInit() || isa<InitListExpr>(st->getInit())) {
            rewriter.RemoveText(range);
        } else {
            QualType type = st->getType();
            string typeString = type.getAsString();
            if(typeString.find("[") != string::npos) {
                typeString = typeString.substr(0, typeString.find("[")-1).substr(0, typeString.rfind(" "));
            }
            SourceLocation nameStart = st->getLocStart().getLocWithOffset(typeString.length()+1);
            rewriter.InsertTextBefore(nameStart, "(*");
            rewriter.InsertTextAfter(nameStart.getLocWithOffset(string(st->getName()).length()), ")");
            rewriter.RemoveText(SourceRange(
                        st->getLocStart(), st->getLocStart().getLocWithOffset(typeString.length())));
        }
    }

    varDeclStringsInsideSplitRange.unique();
    for(auto st : varDeclStringsInsideSplitRange) {
        splitFunctionCall += st;
    }

    string functionName = getNewFunctionName();
    
    string returnTypeString = currentFunctionReturnType.getAsString();
    /*
    if(returnTypeString != "void") {
        splitFunctionCall += "return ";
    }
    */
    splitFunctionCall += functionName + "(";
    string newFunctionDecl = returnTypeString + " " + functionName +"(";

    for(set<VarDecl *>::iterator i = localVarDecls.begin();
            i != localVarDecls.end(); ) {
        VarDecl *decl = *i;
        QualType type = decl->getType();
        if(const TypedefType *t = dyn_cast<TypedefType>(decl->getType())) {
            type = t->desugar();
        }
        string typeString = type.getAsString();

        //x[] should be called as *x[], or **x
        size_t s;
        while((s = typeString.find("[")) != string::npos) {
            size_t e = typeString.find("]");
            typeString.replace(s, e, "*");
        }

        newFunctionDecl += typeString + "* ";
        newFunctionDecl += decl->getName();

        splitFunctionCall += "&";
        splitFunctionCall += decl->getName();

        if(++i != localVarDecls.end()) {
            newFunctionDecl += ", ";
            splitFunctionCall += ", ";
        }
    }

    if(!constantDecls.empty()) {
        newFunctionDecl += ", ";
        splitFunctionCall += ", ";
    }

    for(set<VarDecl *>::iterator i = constantDecls.begin();
            i != constantDecls.end(); ) {
        VarDecl *decl = *i;
        QualType type = decl->getType();
        if(const TypedefType *t = dyn_cast<TypedefType>(decl->getType())) {
            type = t->desugar();
        }
        string typeString = type.getAsString();

        //x[] should be called as *x[], or **x
        size_t s;
        while((s = typeString.find("[")) != string::npos) {
            size_t e = typeString.find("]");
            typeString.replace(s, e, "*");
        }

        newFunctionDecl += typeString + " ";
        newFunctionDecl += decl->getName();

        splitFunctionCall += decl->getName();

        if(++i != constantDecls.end()) {
            newFunctionDecl += ", ";
            splitFunctionCall += ", ";
        }
        //
    }
    newFunctionDecl += ")";
    splitFunctionCall += ");\n";

    rewrittenFunctionDecls->insert(new string(newFunctionDecl+";"));

    SourceLocation start = rewriter.getSourceMgr().getFileLoc(
            splittedStmts->front()->getLocStart());
    SourceLocation end = rewriter.getSourceMgr().getFileLoc(
            splittedStmts->back()->getLocEnd());
    /*
    pair<FileID, unsigned> decomposedLoc = rewriter.getSourceMgr().getDecomposedSpellingLoc(start);
    start = Lexer::GetBeginningOfToken(start, rewriter.getSourceMgr(), langOpts);
    int rangeSize = rewriter.getRangeSize(SourceRange(start, end));
    */

    if(rewriter.getRewrittenText(SourceRange(end, end)) == "}") {
        //end = end.getLocWithOffset(1);
    } else {
        LangOptions langOpts;
        end = Lexer::getLocForEndOfToken(end, 0, rewriter.getSourceMgr(), langOpts);
        while(rewriter.getRewrittenText(SourceRange(end, end)) != ";") {
            end = end.getLocWithOffset(1);
        }
        //end = end.getLocWithOffset(1);
    }

    SourceLocation fileEnd = rewriter.getSourceMgr().getLocForEndOfFile(
            rewriter.getSourceMgr().getMainFileID());

    SourceRange range(start, end);
    newFunctionDecl += " {\n";

    for(set<VarDecl *>::iterator i = localVarDecls.begin();
            i != localVarDecls.end(); i++) {
        VarDecl *decl = *i;
        QualType type = decl->getType();
        if(const TypedefType *t = dyn_cast<TypedefType>(decl->getType())) {
            type = t->desugar();
        }
        string typeString = type.getAsString();

        //x[] should be called as *x[], or **x
        size_t s;
        while((s = typeString.find("[")) != string::npos) {
            size_t e = typeString.find("]");
            typeString.replace(s, e, "*");
        }

        string newDeclName(string("__generated__"));
        newDeclName += decl->getName();

        //newFunctionDecl += "//";
        newFunctionDecl += typeString + " ";
        newFunctionDecl += newDeclName;
        newFunctionDecl += " = *";
        newFunctionDecl += decl->getName();
        newFunctionDecl += ";\n";

    }

    newFunctionDecl += rewriter.getRewrittenText(range);
    newFunctionDecl += "\n";

    for(set<VarDecl *>::iterator i = localVarDecls.begin();
            i != localVarDecls.end(); i++) {
        VarDecl *decl = *i;

        //newFunctionDecl += "//";
        newFunctionDecl += "*";
        newFunctionDecl += decl->getName();
        newFunctionDecl += " = __generated__";
        newFunctionDecl += decl->getName();
        newFunctionDecl += ";\n";

    }
    newFunctionDecl += "\n}\n";

    rewrittenFunctions->insert(new string(newFunctionDecl));
    errs() << "replace this to function call: " << 
        rewriter.getRewrittenText(range) << "\n";

    string startString = rewriter.getRewrittenText(
            SourceRange(start, start.getLocWithOffset(12)));
    if(startString.substr(0, 13) == "__generated__") {
        /*
        string preceedingChar = rewriter.getRewrittenText(SourceRange(start.getLocWithOffset(-1), start.getLocWithOffset(-1)));
        preceedingChar += splitFunctionCall;
        rewriter.getEditBuffer(rewriter.getSourceMgr().getMainFileID()).ReplaceText(decomposedLoc.second-1, rangeSize+1, preceedingChar.c_str());
        */
        start = start.getLocWithOffset(-1);
        string preceedingChar = rewriter.getRewrittenText(
                SourceRange(start, start));
        splitFunctionCall = preceedingChar + splitFunctionCall;
    }

    rewriter.ReplaceText(
            SourceRange(start,end), splitFunctionCall);

    localVarDecls.clear();
    constantDecls.clear();
}


bool FunctionSplitter::isSplitStartPoint(Stmt *stmt) {
    if(splitMethod == STRING) {
        string SPLIT_START_STRING = "__SPLIT_START";
        if(ImplicitCastExpr *expr = dyn_cast<ImplicitCastExpr>(stmt)) {
            Expr *subExpr = expr->getSubExpr();
            if(StringLiteral *str = dyn_cast<StringLiteral>(subExpr)) {
                if(str->getString() == SPLIT_START_STRING) {
                    /*
                    rewriter.RemoveText(SourceRange(
                                stmt->getLocStart(), stmt->getLocEnd()));
                                */
                    return true;
                }
            }
        }
    }
    else if(splitMethod == LOOP_ONLY) {
        /*
        if(ForStmt *forStmt = dyn_cast<ForStmt>(stmt)) {
            if(CompoundStmt *body = dyn_cast<CompoundStmt>(forStmt->getBody())) {
                int nStmtsInBody = body->size();
            }
        }
        */
        return isa<ForStmt>(stmt) || isa<WhileStmt>(stmt);
    }
    return false;
}


bool FunctionSplitter::isSplitEndPoint(Stmt *stmt) {
    string SPLIT_END_STRING = "__SPLIT_END";
    if(ImplicitCastExpr *expr = dyn_cast<ImplicitCastExpr>(stmt)) {
        Expr *subExpr = expr->getSubExpr();
        if(StringLiteral *str = dyn_cast<StringLiteral>(subExpr)) {
            if(str->getString() == SPLIT_END_STRING) {
                /*
                rewriter.RemoveText(SourceRange(
                            stmt->getLocStart(), stmt->getLocEnd()));
                            */
                return true;
            }
        }
    }
    if(isa<ReturnStmt>(stmt)) return true;
    return false;
}


void FunctionSplitASTConsumer::rewriteFunctions() {
    SourceLocation end = rewriter.getSourceMgr().getLocForEndOfFile(
            rewriter.getSourceMgr().getMainFileID());
    rewriter.InsertTextAfter(end, "\n\n//Generated function definitions\n");
    for(set<string *>::iterator i = rewrittenFunctions.begin();
            i != rewrittenFunctions.end(); i++) {
        string *fd = *i;
        rewriter.InsertTextAfter(end, *fd+"\n\n");
    }
}


void FunctionSplitASTConsumer::rewriteFunctionDecls() {
    SourceLocation declStart = firstFunctionDecl;
    /*
    if(mainFunctionDecl.isValid()) {
        declStart = mainFunctionDecl;
    } else {
        declStart = firstFunctionDecl;
    }
    */
    rewriter.InsertTextAfter(declStart, "//Generated function declarations\n");
    for(set<string *>::iterator i = rewrittenFunctionDecls.begin();
            i != rewrittenFunctionDecls.end(); i++) {
        string *fd = *i;
        rewriter.InsertTextAfter(declStart, *fd+"\n");
    }
    rewriter.InsertTextAfter(declStart, "//--------------\n\n");
}


bool FunctionSplitter::StmtParser::isLocalVariable(DeclRefExpr *st) {
    if(!splitStartLocation.isValid()) return false;
    ValueDecl *decl = st->getDecl();
    //QualType type = decl->getType();
    /*
    if(type.isConstant(*ast_context_)) {
        return false;
    }
    if(ConstantArrayType::classof(type.getTypePtr())) {
        return false;
    }
    */
    if(VarDecl *varDecl = dyn_cast<VarDecl>(decl)) {
        if(localVarDecls->find(varDecl) != localVarDecls->end()) return true;
        if(!varDecl->isLocalVarDecl() && varDecl->getKind() != Decl::ParmVar) { 
            return false;
        }
        FullSourceLoc declLoc(decl->getLocStart(), rewriter.getSourceMgr());
        if(declLoc.isBeforeInTranslationUnitThan(splitStartLocation)) {
            return true;
        }
    }
    return false;
}


bool FunctionSplitter::StmtParser::VisitDeclRefExpr(DeclRefExpr *st) {
    if(isLocalVariable(st)) {
        VarDecl *decl = dyn_cast<VarDecl>(st->getDecl());
        if(constantDecls->find(decl) != constantDecls->end()) return true;
        QualType type = decl->getType();
        if(type.isConstant(*ast_context_) || ConstantArrayType::classof(type.getTypePtr())) { 
            constantDecls->insert(decl);
            return true; 
        }

        SourceLocation start = rewriter.getSourceMgr().getFileLoc(st->getLocStart());
        if(rewrittenLocations.find(start) != rewrittenLocations.end()) return true;
        /*
        rewriter.InsertTextBefore(start, "(*");
        int varLength = string(decl->getName()).length();
        rewriter.InsertTextAfter(start.getLocWithOffset(varLength), ")");
        */

        rewriter.InsertTextBefore(start, "__generated__");
        rewrittenLocations.insert(start);
        //int varLength = string(decl->getName()).length();
        //rewriter.InsertTextAfter(start.getLocWithOffset(varLength), ")");

        /*
        bool alreadyInLocalVarDecls = false;
        for(auto localVarDecl : *localVarDecls) {
            if(string(decl->getName()) == string(localVarDecl->getName())) {
                alreadyInLocalVarDecls = true;
                break;
            }
        }
        if(!alreadyInLocalVarDecls) localVarDecls->insert(decl);
        */
        localVarDecls->insert(decl);
        if(start == splitStartLocation) {
            //splitStartLocation = splitStartLocation.getLocWithOffset(-13);
            /*
            splitStartLocation = splitStartLocation.getLocWithOffset(
                    -string("__generated__").length());
            */
        }
    }
    return true;
}


bool FunctionSplitter::StmtParser::VisitVarDecl(VarDecl *st) {
    /*
    SourceRange range(st->getLocStart(), st->getLocEnd().getLocWithOffset(1));
    string declString = rewriter.getRewrittenText(range);
    declString += "\n";
    rewriter.InsertTextBefore(splitStartLocation, declString);
    rewriter.RemoveText(range);
    */

    /*
    if(ConstantArrayType::classof(st->getType().getTypePtr())) {
        constantDecls->insert(st);
    } else {
        localVarDecls->insert(st);
    }
    varDeclsInsideSplitRange->insert(st);
    SourceLocation start = st->getLocStart();
    SourceLocation end = st->getLocEnd();
    LangOptions langOpts;
    end = Lexer::getLocForEndOfToken(end, 0, rewriter.getSourceMgr(), langOpts);
    while(rewriter.getRewrittenText(SourceRange(end, end)) != ";") {
        end = end.getLocWithOffset(1);
    }
    end = end.getLocWithOffset(2);
    string declString = rewriter.getRewrittenText(SourceRange(start, end));
    varDeclStringsInsideSplitRange->push_back(declString);
    //rewriter.InsertTextBefore(splitStartLocation, st->getNameAsString());
    */
    return true;
}
