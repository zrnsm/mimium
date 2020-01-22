#pragma once

#include "compiler/alphaconvert_visitor.hpp"
#include "basic/ast.hpp"
#include "compiler/closure_convert.hpp"
#include "compiler/driver.hpp"
#include "basic/helper_functions.hpp"
#include "compiler/knormalize_visitor.hpp"
#include "compiler/llvmgenerator.hpp"
#include "basic/mir.hpp"
#include "compiler/recursive_checker.hpp"
#include "basic/type.hpp"
#include "compiler/type_infer_visitor.hpp"

namespace mimium {
// compiler class  that load source code,analyze, and emit llvm IR.
class Compiler {
public: 
    Compiler();
    Compiler(llvm::LLVMContext& ctx);
    ~Compiler();
    AST_Ptr loadSource(std::string source);
    AST_Ptr loadSourceFile(std::string filename);
    void setFilePath(std::string path);
    void setDataLayout(const llvm::DataLayout& dl);
    void setDataLayout();

    AST_Ptr alphaConvert(AST_Ptr ast);
    TypeEnv& typeInfer(AST_Ptr ast);
    std::shared_ptr<MIRblock> generateMir(AST_Ptr ast);
    std::shared_ptr<MIRblock> closureConvert(std::shared_ptr<MIRblock> mir);
    llvm::Module& generateLLVMIr(std::shared_ptr<MIRblock> mir);
    auto moveLLVMModule(){return llvmgenerator.moveModule();}
 private:
    void recursiveCheck(AST_Ptr ast);
  mmmpsr::MimiumDriver driver;
  AlphaConvertVisitor alphavisitor;
  TypeInferVisitor typevisitor;
  RecursiveChecker recursivechecker;
  KNormalizeVisitor knormvisitor;
  std::shared_ptr<ClosureConverter> closureconverter;
  LLVMGenerator llvmgenerator;
  std::string path;
};

}  // namespace mimium