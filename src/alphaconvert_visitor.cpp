#include "alphaconvert_visitor.hpp"
namespace mimium {
AlphaConvertVisitor::AlphaConvertVisitor():namecount(0),envcount(0) { init(); }
void AlphaConvertVisitor::init() {
  namecount = 0;
  envcount = 0;
  env.reset();
  env = std::make_shared<Environment>("root", nullptr);
}

AlphaConvertVisitor::~AlphaConvertVisitor()=default;
auto AlphaConvertVisitor::getResult() -> std::shared_ptr<ListAST> {
  return std::static_pointer_cast<ListAST>(std::get<AST_Ptr>(res_stack.top()));
};

void AlphaConvertVisitor::visit(LvarAST& ast) {
  std::string newname;
  if (env->isVariableSet(ast.getVal())) {
    newname = std::get<std::string>(env->findVariable(ast.getVal()));
  } else {
    namecount++;
    newname = ast.getVal() + std::to_string(namecount);
    env->setVariableRaw(ast.getVal(), newname);  // register to map
  }
  auto newast = std::make_unique<LvarAST>(newname, ast.type);

  res_stack.push(std::move(newast));
}

void AlphaConvertVisitor::visit(RvarAST& ast) {
  auto newname = std::get<std::string>(env->findVariable(ast.getVal()));
  auto newast = std::make_unique<RvarAST>(newname);
  res_stack.push(std::move(newast));
}
void AlphaConvertVisitor::visit(OpAST& ast) {
  ast.rhs->accept(*this);
  ast.lhs->accept(*this);
  auto newast =
      std::make_unique<OpAST>(ast.op, stackPopPtr(), stackPopPtr());
  res_stack.push(std::move(newast));
}
void AlphaConvertVisitor::visit(ListAST& ast) { listastvisit(ast); }
void AlphaConvertVisitor::visit(NumberAST& ast) { defaultvisit(ast); }
void AlphaConvertVisitor::visit(AssignAST& ast) {
  ast.getName()->accept(*this);
  auto newname = stackPopPtr();
  ast.getBody()->accept(*this);
  auto newast =
      std::make_unique<AssignAST>(std::move(newname), stackPopPtr());
  res_stack.push(std::move(newast));
}
void AlphaConvertVisitor::visit(ArgumentsAST& ast) { listastvisit(ast); }
void AlphaConvertVisitor::visit(ArrayAST& ast) { listastvisit(ast); }
void AlphaConvertVisitor::visit(ArrayAccessAST& ast) {
  ast.getName()->accept(*this);
  auto newname = stackPopPtr();
  ast.getIndex()->accept(*this);
  auto newast =
      std::make_unique<ArrayAccessAST>(std::move(newname), stackPopPtr());
  res_stack.push(std::move(newast));
}
void AlphaConvertVisitor::visit(FcallAST& ast) {
  ast.getFname()->accept(*this);
  auto newname = stackPopPtr();
  ast.getArgs()->accept(*this);
  auto newargs = stackPopPtr();
  auto newast =
      std::make_unique<FcallAST>(std::move(newname), std::move(newargs));
  res_stack.push(std::move(newast));
}
void AlphaConvertVisitor::visit(LambdaAST& ast) {
  env = env->createNewChild("lambda" + std::to_string(envcount));
  envcount++;
  ast.getArgs()->accept(*this);  // register argument as unique name
  auto newargs = stackPopPtr();
  ast.getBody()->accept(*this);
  auto newbody = stackPopPtr();
  auto newast =
      std::make_unique<LambdaAST>(std::move(newargs), std::move(newbody));
  res_stack.push(std::move(newast));
  env = env->getParent();
}
void AlphaConvertVisitor::visit(IfAST& ast) {
  ast.getCond()->accept(*this);
  auto newcond = stackPopPtr();
  ast.getThen()->accept(*this);
  auto newthen = stackPopPtr();
  ast.getElse()->accept(*this);
  auto newelse = stackPopPtr();
  auto newast = std::make_unique<IfAST>(std::move(newcond), std::move(newthen),
                                        std::move(newelse));
  res_stack.push(std::move(newast));
};

void AlphaConvertVisitor::visit(ReturnAST& ast) {
  ast.getExpr()->accept(*this);
  res_stack.push(std::make_unique<ReturnAST>(stackPopPtr()));
}
void AlphaConvertVisitor::visit(ForAST& ast) {
  env = env->createNewChild("forloop" + std::to_string(envcount));
  envcount++;
  ast.getVar()->accept(*this);
  auto newvar = stackPopPtr();
  ast.getIterator()->accept(*this);
  auto newiter = stackPopPtr();
  ast.getExpression()->accept(*this);
  auto newexpr = stackPopPtr();
  auto newast = std::make_unique<IfAST>(std::move(newvar), std::move(newiter),
                                        std::move(newexpr));
  res_stack.push(std::move(newast));
  env = env->getParent();
}
void AlphaConvertVisitor::visit(DeclarationAST& ast) {
  // will not be called
}
void AlphaConvertVisitor::visit(TimeAST& ast) {
  ast.getExpr()->accept(*this);
  auto newexpr = stackPopPtr();
  ast.getTime()->accept(*this);
  auto newtime = stackPopPtr();
  auto newast =
      std::make_unique<TimeAST>(std::move(newexpr), std::move(newtime));
  res_stack.push(std::move(newast));
}

void AlphaConvertVisitor::visit(StructAST& ast) {
  auto newast = std::make_unique<StructAST>();  // make empty
  for (auto& [key, val] : ast.map) {
    val->accept(*this);
    key->accept(*this);
    newast->addPair(stackPopPtr(), stackPopPtr());
  }
  res_stack.push(std::move(newast));
}
void AlphaConvertVisitor::visit(StructAccessAST& ast) {
  ast.getVal()->accept(*this);
  ast.getKey()->accept(*this);
  auto newast =
      std::make_unique<StructAccessAST>(stackPopPtr(), stackPopPtr());
  res_stack.push(std::move(newast));
}

}  // namespace mimium