/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "compiler/type_infer_visitor.hpp"

namespace mimium {
TypeInferVisitor::TypeInferVisitor() : has_return(false) {
  for (const auto& [key, val] : LLVMBuiltin::ftable) {
    typeenv.emplace(key, val.mmmtype);
  }
  typeenv.emplace("mimium_getnow", types::Function(types::Float(), {}));
}

void TypeInferVisitor::init() { has_return = false; }

bool TypeInferVisitor::typeCheck(types::Value& lt, types::Value& rt) {
  bool res = lt.index() == rt.index();
  if (!res) {
    throw std::logic_error("type not matched");
  }
  return res;
}

bool TypeInferVisitor::unify(types::Value& lt, types::Value& rt) {
  bool istvl = types::isTypeVar(lt);
  bool istvr = types::isTypeVar(rt);
  bool res = true;
  if (istvl && !istvr) {
    auto& lhs = rv::get<types::TypeVar>(lt);
    unifyTypeVar(lhs, rt);
  } else if (!istvl && istvr) {
    auto& rhs = rv::get<types::TypeVar>(rt);
    unifyTypeVar(rhs, lt);
  } else if (istvl && istvr) {
    auto& lhs = rv::get<types::TypeVar>(lt);
    unifyTypeVar(lhs, rt);
  } else {
    res = typeCheck(lt, rt);
  }
  return res;
}

bool TypeInferVisitor::unify(std::string lname, types::Value& rt) {
  return unify(typeenv.find(lname), rt);
}
bool TypeInferVisitor::unify(std::string lname, std::string rname) {
  return unify(typeenv.find(lname), typeenv.find(rname));
}

void TypeInferVisitor::unifyTypeVar(types::TypeVar& tv, types::Value& v) {
  auto it = tvreplacevisitor.tvmap.find(tv.index);
  if (it != tvreplacevisitor.tvmap.end()) {
    bool issametype = it->second.index() == v.index();
    if (!issametype) {
      std::runtime_error("typevars assigned to different type");
    }
  } else {
    tvreplacevisitor.tvmap.emplace(tv.index, v);
  }
}

// bool TypeInferVisitor::unifyArg(types::Value& target, types::Value& realarg)
// {
//   if (auto* timeptr = std::get_if<Rec_Wrap<types::Time>>(&realarg)) {
//     types::Time time = *timeptr;
//     return unify(time.val, target);
//   } else {
//     return unify(realarg, target);
//   }
// }

void TypeInferVisitor::visit(AssignAST& ast) {
  auto lvar = ast.getName();
  auto lname = lvar->getVal();
  lvar->accept(*this);
  auto ltype = stackPop();
  ;
  typeenv.emplace(lname, ltype);
  auto body = ast.getBody();
  if (body->getid() == LAMBDA) {
    tmpfname = lname;
  }
  body->accept(*this);
  auto r = stackPop();
  if (!unify(lvar->getVal(), r)) {
    throw std::logic_error("type " + lvar->getVal() +
                           " did not matched to expression " +
                           ast.getBody()->toString());
  }
}

void TypeInferVisitor::visit(LvarAST& ast) {
  auto& ltype = ast.getType();
  ;
  bool is_specified = !std::holds_alternative<types::None>(ltype);
  bool is_exist = typeenv.exist(ast.getVal());
  if (is_exist && is_specified) {
    if (typeenv.find(ast.getVal()) != ltype) {
      throw std::logic_error("assignment to different type is not allowed");
    }
    res_stack.push(ltype);

  } else if (!is_exist && !is_specified) {
    res_stack.push(typeenv.createNewTypeVar());
  } else if (!is_exist && is_specified) {
    res_stack.push(ltype);
  } else {  // case of already declared variable that have not specified in
            // source code

    res_stack.push(typeenv.find(ast.getVal()));
  }
}

void TypeInferVisitor::visit(RvarAST& ast) {
  res_stack.push(typeenv.find(ast.getVal()));
}
void TypeInferVisitor::visit(SelfAST& ast) {
  // self is the same type as return type of its function.
  if (!current_return_type) {
    res_stack.push(typeenv.createNewTypeVar());
  } else {
    if (std::holds_alternative<types::Void>(current_return_type.value())) {
      throw std::logic_error("Void Function cannot have a keyword \"self\"!");
    }
    ast.type = current_return_type.value();
    res_stack.push(current_return_type.value());
  }
}

void TypeInferVisitor::visit(OpAST& ast) {
  ast.lhs->accept(*this);
  types::Value lhstype = stackPop();
  ;
  ast.rhs->accept(*this);
  types::Value rhstype = stackPop();
  ;
  if (unify(lhstype, rhstype)) {
    std::logic_error("type of lhs and rhs is not matched");
  }
  // anyway return type is float for now
  res_stack.push(rhstype);
}
void TypeInferVisitor::visit(ListAST& ast) {
  for (auto& line : ast.getElements()) {
    line->accept(*this);
  }
}
void TypeInferVisitor::visit(NumberAST& /*ast*/) {
  res_stack.push(*std::make_unique<types::Float>());
}

void TypeInferVisitor::visit(StringAST& /*ast*/) {
  res_stack.push(*std::make_unique<types::String>());
}

void TypeInferVisitor::visit(ArgumentsAST& ast) {
  //
}
void TypeInferVisitor::visit(FcallArgsAST& ast) {
  //
}
void TypeInferVisitor::visit(ArrayAST& ast) {
  auto& elms = ast.getElements();
  auto tmpres = types::Value();
  int c = 0;
  bool res = true;
  for (const auto& v : elms) {
    v->accept(*this);
    auto mr = stackPop();
    res &= (c) ? true : (tmpres.index() == mr.index());
    if (!res) {
      throw std::logic_error("array contains different types.");
    }
    tmpres = mr;
    ++c;
  }
  // res_stack.push(types::Array(tmpres, elms.size()));
}
void TypeInferVisitor::visit(ArrayAccessAST& ast) {
  auto type = typeenv.find(ast.getName()->getVal());
  types::Value res;
  // fixme
  types::Value arr = types::Array(types::Float());
  unify(type, arr);

  res_stack.push(types::Float());
}
// bool TypeInferVisitor::checkArg(types::Value& fnarg, types::Value& givenarg)
// {
//   bool res;
//   if (std::holds_alternative<Rec_Wrap<types::Time>>(givenarg)) {
//     auto v = rv::get<types::Time>(givenarg);
//     res = fnarg.index() == v.val.index();  // currently
//   } else {
//     res = fnarg.index() == givenarg.index();
//   }
//   return res;
// }

void TypeInferVisitor::visit(FcallAST& ast) {
  ast.getFname()->accept(*this);
  auto ftype = stackPop();
  auto& fn = rv::get<types::Function>(ftype);
  auto fnargtypes = fn.getArgTypes();
  types::Value res;
  auto args = ast.getArgs()->getElements();
  std::vector<types::Value> argtypes;
  bool checkflag = true;
  for (int i = 0; i < args.size(); i++) {
    args[i]->accept(*this);
    auto r = stackPop();
    checkflag &= unify(fnargtypes[i], r);
  }
  if (!checkflag) {
    throw std::invalid_argument("argument types were invalid");
  }
  if (ast.time != nullptr) {
    ast.time->accept(*this);
    auto timetype = stackPop();
    types::Value f = types::Float();
    unify(timetype, f);
  }
  res_stack.push(fn.getReturnType());
}
void TypeInferVisitor::visit(LambdaAST& ast) {
  // type registoration for each arguments
  auto args = ast.getArgs();
  std::vector<types::Value> argtypes;
  for (const auto& a : args->getElements()) {
    a->accept(*this);
    auto r = stackPop();
    typeenv.emplace(a->getVal(), r);
    argtypes.push_back(r);
  }
  bool isspecified =
      std::holds_alternative<Rec_Wrap<types::Function>>(ast.type);
  // case of no type specifier

  if (isspecified) {
    auto fntype = rv::get<types::Function>(ast.type);
    current_return_type = fntype.ret_type;
  } else {
    if (ast.isrecursive) {
      throw std::logic_error("recursive function need to specify return type.");
    }
    current_return_type = typeenv.createNewTypeVar();
  }
  types::Value res_type =
      types::Function(current_return_type.value(), argtypes);
  typeenv.emplace(tmpfname, res_type);
  ast.getBody()->accept(*this);
  auto& ref = rv::get<types::Function>(typeenv.find(tmpfname));
  types::Value ret_type = (has_return) ? stackPop() : types::Void();
  unify(ret_type, ref.ret_type);

  tmpfname = "";
  res_stack.push(ref);
  has_return = false;  // switch back flag
  current_return_type = std::nullopt;
}
void TypeInferVisitor::visit(IfAST& ast) {
  ast.getCond()->accept(*this);
  auto condval = stackPop();
  types::Value ref = types::Float();
  unify(condval, ref);
  if (ast.isexpr) {
    // auto newcond = stack_pop_ptr();
    ast.getThen()->accept(*this);
    auto thenval = stackPop();
    ast.getElse()->accept(*this);
    auto elseval = stackPop();
    unify(thenval, elseval);
    res_stack.push(thenval);
  }
  // auto newelse = stack_pop_ptr();
  // auto newast =
  // std::make_unique<IfAST>(std::move(newcond),std::move(newthen),std::move(newelse));
  // res_stack.push(std::move(newast));
};

void TypeInferVisitor::visit(ReturnAST& ast) {
  ast.getExpr()->accept(*this);
  has_return = true;
}
void TypeInferVisitor::visit(ForAST& ast) {
  // env = env->createNewChild("forloop"+std::to_string(envcount));
  // envcount++;
  // ast.getVar()->accept(*this);
  // auto newvar = stack_pop_ptr();
  // ast.getIterator()->accept(*this);
  // auto newiter = stack_pop_ptr();
  // ast.getExpression()->accept(*this);
  // auto newexpr = stack_pop_ptr();
  // auto newast =
  // std::make_unique<IfAST>(std::move(newvar),std::move(newiter),std::move(newexpr));
  // res_stack.push(std::move(newast));
  // env = env->getParent();
}
void TypeInferVisitor::visit(DeclarationAST& ast) {
  // will not be called
}
// void TypeInferVisitor::visit(TimeAST& ast) {
//   types::Time t;
//   ast.getExpr()->accept(*this);
//   auto res = stackPop();
//   if (std::holds_alternative<Rec_Wrap<types::Time>>(res)) {
//     throw std::logic_error("Time type can not be nested.");
//   }

//   t.val = res;
//   ast.getTime()->accept(*this);
//   auto r = stackPop();
//   types::Value tmpf = types::Float();
//   unify(r, tmpf);
//   t.val=r;
//   t.time = types::Float();
//   res_stack.push(t);
// }

void TypeInferVisitor::visit(StructAST& ast) {}
void TypeInferVisitor::visit(StructAccessAST& ast) {}
TypeEnv& TypeInferVisitor::infer(AST_Ptr toplevel) {
  toplevel->accept(*this);
  replaceTypeVars();
  return typeenv;
}

void TypeInferVisitor::replaceTypeVars() {
  for (auto&& [name, type] : typeenv) {
    auto newv = std::visit(tvreplacevisitor, type);
    typeenv.emplace(name, std::move(newv));
  }
}

// new TypeInferer
using ExprTypeVisitor = TypeInferer::ExprTypeVisitor;
using StatementTypeVisitor = TypeInferer::StatementTypeVisitor;
using TypeUnifyVisitor = TypeInferer::TypeUnifyVisitor;

types::Value ExprTypeVisitor::operator()(newast::Op& ast) {
  if (!ast.lhs.has_value()) {
    return std::visit(*this, *ast.rhs);
  }
  return inferer.unify(std::visit(*this, *ast.lhs.value()),
                       std::visit(*this, *ast.rhs));
}
types::Value ExprTypeVisitor::operator()(newast::Number& ast) {
  return types::Float();
}
types::Value ExprTypeVisitor::operator()(newast::String& ast) {
  return types::String();
}
types::Value ExprTypeVisitor::operator()(newast::Rvar& ast) {
  return inferer.typeenv.find(ast.value);
}
types::Value ExprTypeVisitor::operator()(newast::Self& ast) {
  if(inferer.selftype_stack.empty()){
    throw std::runtime_error("keyword \"self\" cannot be used out of function");
  }
  return inferer.selftype_stack.top();
}
types::Value ExprTypeVisitor::operator()(newast::Lambda& ast) {
  std::vector<types::Value> argtypes;
  for (auto&& a : ast.args.args) {
    argtypes.emplace_back(inferer.addLvar(a));
  }
  auto rettype_tv = ast.ret_type.value_or(inferer.typeenv.createNewTypeVar());
  inferer.selftype_stack.push(rettype_tv);
  auto rettype = inferer.inferStatements(ast.body);
  inferer.selftype_stack.pop();
  return types::Function(inferer.unify(rettype_tv, rettype),
                         std::move(argtypes));
}
types::Value ExprTypeVisitor::operator()(newast::Fcall& ast) {
  std::vector<types::Value> argtypes;
  for (auto&& a : ast.args.args) {
    argtypes.emplace_back(std::visit(*this, *a));
  }
  auto frettype = inferer.typeenv.createNewTypeVar();
  auto ftype = types::Function(std::move(frettype), std::move(argtypes));
  return inferer.unify(std::visit(*this, *ast.fn), std::move(ftype));
}
types::Value ExprTypeVisitor::operator()(newast::Time& ast) {
  inferer.unify(std::visit(*this, *ast.when), types::Float());
  // anyway, this rvalue will not be used(time specification is used as void
  return (*this)(ast.fcall);
}
types::Value ExprTypeVisitor::operator()(newast::Struct& ast) {
  std::vector<types::Struct::Keytype> argtypes;
  types::Value tmptype;
  for (auto&& a : ast.args) {
    // todo(tomoya): need to fix parser(add key)
    argtypes.push_back({"struct_key", std::visit(*this, *a)});
  }
  return types::Struct(std::move(argtypes));
}
types::Value ExprTypeVisitor::operator()(newast::StructAccess& ast) {
  auto stru_type = std::visit(*this, *ast.stru);
  if (!rv::holds_alternative<types::Struct>(stru_type)) {
    throw std::runtime_error(
        "you cannot access to variables other than Struct type with dot "
        "operator.");
  }
  auto& stru = rv::get<types::Struct>(stru_type);
  // todo(tomoya): need to restrict field ast to rvar, not an term
  return stru.arg_types.at(0).val;
}
types::Value ExprTypeVisitor::operator()(newast::ArrayInit& ast) {
  std::vector<types::Value> argtypes;
  types::Value tmptype;
  for (auto a = ast.args.begin(); a != (--ast.args.end()); ++a) {
    tmptype = inferer.unify(std::visit(*this, **a), std::visit(*this, **(++a)));
  }
  return types::Array(tmptype);
}
types::Value ExprTypeVisitor::operator()(newast::ArrayAccess& ast) {
  auto indextype = inferer.unify(std::visit(*this, *ast.index), types::Float());
  auto arraytype = std::visit(*this, *ast.array);
  if (!rv::holds_alternative<types::Array>(arraytype)) {
    throw std::runtime_error(
        "you cannot access to variables other than Array type with [] "
        "operator.");
  }
  return rv::get<types::Array>(arraytype).elem_type;
}
types::Value ExprTypeVisitor::operator()(newast::Tuple& ast) {
  // Todo(tomoya)
  return types::Tuple({types::Float()});
}

types::Value StatementTypeVisitor::operator()(newast::Assign& ast) {
  auto lvartype = inferer.addLvar(ast.lvar);
  auto rvartype = std::visit(inferer.exprvisitor, *ast.expr);
  inferer.unify(lvartype, rvartype);
  return types::Void();
}
types::Value StatementTypeVisitor::operator()(newast::Return& ast) {
  return std::visit(inferer.exprvisitor, *ast.value);
}
// types::Value StatementTypeVisitor::operator()(newast::Declaration& ast){}
types::Value StatementTypeVisitor::operator()(newast::For& ast) {
  // TODO(tomoya): types for iterator
  inferer.infer(ast.statements);
  return types::Void();  // for loop becomes void anyway
}
types::Value StatementTypeVisitor::operator()(newast::If& ast) {
  if(!ast.else_stmts.has_value()){
    return inferer.inferStatements(ast.then_stmts);
  }
  auto then_r_type =  inferer.inferStatements(ast.then_stmts);
  auto else_r_type = inferer.inferStatements(ast.else_stmts.value());
  return inferer.unify(then_r_type, else_r_type);
}
types::Value StatementTypeVisitor::operator()(newast::ExprPtr& ast) {
  return std::visit(inferer.exprvisitor, *ast);
}

types::Value TypeInferer::addLvar(newast::Lvar& lvar) {
  auto res = lvar.type.value_or(typeenv.createNewTypeVar());
  auto [iter, was_newvar] = typeenv.emplace(lvar.value, res);
  return res;
}
types::Value TypeUnifyVisitor::operator()(types::Pointer& p1,
                                          types::Pointer& p2) {
  auto target = inferer.unify(p1.val, p2.val);
  return types::Pointer(target);
}
types::Value TypeUnifyVisitor::operator()(types::Ref& p1, types::Ref& p2) {
  auto target = inferer.unify(p1.val, p2.val);
  return types::Pointer(target);
}
types::Value TypeUnifyVisitor::operator()(types::Function& f1,
                                          types::Function& f2) {
  auto argtype = unifyArgs(f1.arg_types, f2.arg_types);
  auto rettype = std::visit(*this, f1.ret_type, f2.ret_type);
  return types::Function(std::move(rettype), std::move(argtype));
}

types::Value TypeUnifyVisitor::operator()(types::Array& a1, types::Array& a2) {
  auto elemtype = inferer.unify(a1.elem_type, a2.elem_type);
  // size check?
  return types::Array(elemtype, a1.size);
}
types::Value TypeUnifyVisitor::operator()(types::Struct& s1,
                                          types::Struct& s2) {
  // TODO(tomoya)
  return s1;
}
types::Value TypeUnifyVisitor::operator()(types::Tuple& f1, types::Tuple& f2) {
  // TODO(tomoya)
  return f1;
}
std::vector<types::Value> TypeUnifyVisitor::unifyArgs(
    std::vector<types::Value>& v1, std::vector<types::Value>& v2) {
  std::vector<types::Value> res;
  if (v1.size() != v2.size()) {
    throw std::runtime_error("type mismatch: argument size are different");
  }
  for (int i = 0; i < v1.size(); i++) {
    res.emplace_back(std::visit(*this, v1.at(i), v2.at(i)));  // discard return values
  }
  return res;
}

types::Value TypeInferer::unify(types::Value const& lhs,
                                types::Value const& rhs) {
  return std::visit(unifyvisitor, lhs, rhs);
}
types::Value TypeInferer::inferStatements(newast::Statements& statements) {
  types::Value stmttype;
  for (auto&& statement : statements) {
    stmttype = std::visit(statementvisitor, *statement);
  }
  return stmttype;
}
TypeEnv& TypeInferer::infer(newast::Statements& topast) {
  inferStatements(topast);
  substituteTypeVars();
  return typeenv;
}
void TypeInferer::substituteTypeVars(){
  for(auto&& [key,t]: typeenv.env){
    auto[iter,replaced] = typeenv.env.insert_or_assign(key,std::visit(substitutevisitor,t));
  }
}


}  // namespace mimium
