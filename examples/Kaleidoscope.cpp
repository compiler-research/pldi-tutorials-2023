/* See the LICENSE file in the project root for license terms. */

#include "Kaleidoscope.h"

#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Target/TargetMachine.h"
#include <atomic>
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace llvm;
using namespace llvm::orc;

//===----------------------------------------------------------------------===//
// Lexer
//===----------------------------------------------------------------------===//

// The lexer returns tokens [0-255] if it is an unknown character, otherwise one
// of these for known things.
enum Token {
  tok_eof = -1,

  // commands
  tok_def = -2,
  tok_extern = -3,

  // primary
  tok_identifier = -4,
  tok_number = -5,

  // control
  tok_if = -6,
  tok_then = -7,
  tok_else = -8,
  tok_for = -9,
  tok_in = -10,

  // operators
  tok_binary = -11,
  tok_unary = -12,

  // var definition
  tok_var = -13
};

static std::string IdentifierStr; // Filled in if tok_identifier
static double NumVal;             // Filled in if tok_number

static int LastChar = ' ';
static StringRef InputLine;

void resetInputLine(StringRef NewInputLine) {
  LastChar = ' ';
  InputLine = NewInputLine;
}

char getInputLineChar() {
  if (InputLine.empty())
    return EOF;

  char C = InputLine.front();
  InputLine = InputLine.drop_front();
  return C;
}

/// gettok - Return the next token from standard input.
static int gettok() {

  // Skip any whitespace.
  while (isspace(LastChar))
    LastChar = getInputLineChar();

  if (isalpha(LastChar)) { // identifier: [a-zA-Z][a-zA-Z0-9]*
    IdentifierStr = LastChar;
    while (isalnum((LastChar = getInputLineChar())))
      IdentifierStr += LastChar;

    if (IdentifierStr == "def")
      return tok_def;
    if (IdentifierStr == "extern")
      return tok_extern;
    if (IdentifierStr == "if")
      return tok_if;
    if (IdentifierStr == "then")
      return tok_then;
    if (IdentifierStr == "else")
      return tok_else;
    if (IdentifierStr == "for")
      return tok_for;
    if (IdentifierStr == "in")
      return tok_in;
    if (IdentifierStr == "binary")
      return tok_binary;
    if (IdentifierStr == "unary")
      return tok_unary;
    if (IdentifierStr == "var")
      return tok_var;
    return tok_identifier;
  }

  if (isdigit(LastChar) || LastChar == '.') { // Number: [0-9.]+
    std::string NumStr;
    do {
      NumStr += LastChar;
      LastChar = getInputLineChar();
    } while (isdigit(LastChar) || LastChar == '.');

    NumVal = strtod(NumStr.c_str(), nullptr);
    return tok_number;
  }

  if (LastChar == '#') {
    // Comment until end of line.
    do
      LastChar = getInputLineChar();
    while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

    if (LastChar != EOF)
      return gettok();
  }

  // Check for end of "file".
  if (LastChar == EOF)
    return tok_eof;

  // Otherwise, just return the character as its ascii value.
  int ThisChar = LastChar;
  LastChar = getInputLineChar();
  return ThisChar;
}

//===----------------------------------------------------------------------===//
// Abstract Syntax Tree (aka Parse Tree)
//===----------------------------------------------------------------------===//

/// ExprAST - Base class for all expression nodes.
class ExprAST {
public:
  virtual ~ExprAST() = default;

  virtual Value *codegen(KaleidoscopeParser &P, CodeGenContext &CGCtx) = 0;
};

/// NumberExprAST - Expression class for numeric literals like "1.0".
class NumberExprAST : public ExprAST {
  double Val;

public:
  NumberExprAST(double Val) : Val(Val) {}

  Value *codegen(KaleidoscopeParser &P, CodeGenContext &CGCtx) override;
};

/// VariableExprAST - Expression class for referencing a variable, like "a".
class VariableExprAST : public ExprAST {
  std::string Name;

public:
  VariableExprAST(const std::string &Name) : Name(Name) {}

  Value *codegen(KaleidoscopeParser &P, CodeGenContext &CGCtx) override;
  const std::string &getName() const { return Name; }
};

/// UnaryExprAST - Expression class for a unary operator.
class UnaryExprAST : public ExprAST {
  char Opcode;
  std::unique_ptr<ExprAST> Operand;

public:
  UnaryExprAST(char Opcode, std::unique_ptr<ExprAST> Operand)
      : Opcode(Opcode), Operand(std::move(Operand)) {}

  Value *codegen(KaleidoscopeParser &P, CodeGenContext &CGCtx) override;
};

/// BinaryExprAST - Expression class for a binary operator.
class BinaryExprAST : public ExprAST {
  char Op;
  std::unique_ptr<ExprAST> LHS, RHS;

public:
  BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS,
                std::unique_ptr<ExprAST> RHS)
      : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}

  Value *codegen(KaleidoscopeParser &P, CodeGenContext &CGCtx) override;
};

/// CallExprAST - Expression class for function calls.
class CallExprAST : public ExprAST {
  std::string Callee;
  std::vector<std::unique_ptr<ExprAST>> Args;

public:
  CallExprAST(const std::string &Callee,
              std::vector<std::unique_ptr<ExprAST>> Args)
      : Callee(Callee), Args(std::move(Args)) {}

  Value *codegen(KaleidoscopeParser &P, CodeGenContext &CGCtx) override;
};

/// IfExprAST - Expression class for if/then/else.
class IfExprAST : public ExprAST {
  std::unique_ptr<ExprAST> Cond, Then, Else;

public:
  IfExprAST(std::unique_ptr<ExprAST> Cond, std::unique_ptr<ExprAST> Then,
            std::unique_ptr<ExprAST> Else)
      : Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else)) {}

  Value *codegen(KaleidoscopeParser &P, CodeGenContext &CGCtx) override;
};

/// ForExprAST - Expression class for for/in.
class ForExprAST : public ExprAST {
  std::string VarName;
  std::unique_ptr<ExprAST> Start, End, Step, Body;

public:
  ForExprAST(const std::string &VarName, std::unique_ptr<ExprAST> Start,
             std::unique_ptr<ExprAST> End, std::unique_ptr<ExprAST> Step,
             std::unique_ptr<ExprAST> Body)
      : VarName(VarName), Start(std::move(Start)), End(std::move(End)),
        Step(std::move(Step)), Body(std::move(Body)) {}

  Value *codegen(KaleidoscopeParser &P, CodeGenContext &CGCtx) override;
};

/// VarExprAST - Expression class for var/in
class VarExprAST : public ExprAST {
  std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames;
  std::unique_ptr<ExprAST> Body;

public:
  VarExprAST(
      std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames,
      std::unique_ptr<ExprAST> Body)
      : VarNames(std::move(VarNames)), Body(std::move(Body)) {}

  Value *codegen(KaleidoscopeParser &P, CodeGenContext &CGCtx) override;
};

/// PrototypeAST - This class represents the "prototype" for a function,
/// which captures its name, and its argument names (thus implicitly the number
/// of arguments the function takes), as well as if it is an operator.
class PrototypeAST {
  std::string Name;
  std::vector<std::string> Args;
  bool IsOperator;
  unsigned Precedence; // Precedence if a binary op.

public:
  PrototypeAST(const std::string &Name, std::vector<std::string> Args,
               bool IsOperator = false, unsigned Prec = 0)
      : Name(Name), Args(std::move(Args)), IsOperator(IsOperator),
        Precedence(Prec) {}

  Function *codegen(KaleidoscopeParser &P, CodeGenContext &CGCtx);
  const std::string &getName() const { return Name; }
  void setName(std::string NewName) { Name = std::move(NewName); }

  bool isUnaryOp() const { return IsOperator && Args.size() == 1; }
  bool isBinaryOp() const { return IsOperator && Args.size() == 2; }

  char getOperatorName() const {
    assert(isUnaryOp() || isBinaryOp());
    return Name[Name.size() - 1];
  }

  unsigned getBinaryPrecedence() const { return Precedence; }
};

//===----------------------------------------------------------------------===//
// Parser
//===----------------------------------------------------------------------===//

/// CurTok/getNextToken - Provide a simple token buffer.  CurTok is the current
/// token the parser is looking at.  getNextToken reads another token from the
/// lexer and updates CurTok with its results.
static int CurTok;
static int getNextToken() { return CurTok = gettok(); }

/// BinopPrecedence - This holds the precedence for each binary operator that is
/// defined.
static std::map<char, int> BinopPrecedence({
    {'=', 2},
    {'<', 10},
    {'+', 20},
    {'-', 20},
    {'*', 40}
  });

/// GetTokPrecedence - Get the precedence of the pending binary operator token.
static int GetTokPrecedence() {
  if (!isascii(CurTok))
    return -1;

  // Make sure it's a declared binop.
  int TokPrec = BinopPrecedence[CurTok];
  if (TokPrec <= 0)
    return -1;
  return TokPrec;
}

/// LogError* - These are little helper functions for error handling.
std::unique_ptr<ExprAST> LogError(const std::string &Str) {
  fprintf(stderr, "Error: %s\n", Str.c_str());
  return nullptr;
}

std::unique_ptr<PrototypeAST> LogErrorP(const std::string &Str) {
  LogError(Str);
  return nullptr;
}

static std::unique_ptr<ExprAST> ParseExpression();

/// numberexpr ::= number
static std::unique_ptr<ExprAST> ParseNumberExpr() {
  auto Result = std::make_unique<NumberExprAST>(NumVal);
  getNextToken(); // consume the number
  return std::move(Result);
}

/// parenexpr ::= '(' expression ')'
static std::unique_ptr<ExprAST> ParseParenExpr() {
  getNextToken(); // eat (.
  auto V = ParseExpression();
  if (!V)
    return nullptr;

  if (CurTok != ')')
    return LogError("expected ')'");
  getNextToken(); // eat ).
  return V;
}

/// identifierexpr
///   ::= identifier
///   ::= identifier '(' expression* ')'
static std::unique_ptr<ExprAST> ParseIdentifierExpr() {
  std::string IdName = IdentifierStr;

  getNextToken(); // eat identifier.

  if (CurTok != '(') // Simple variable ref.
    return std::make_unique<VariableExprAST>(IdName);

  // Call.
  getNextToken(); // eat (
  std::vector<std::unique_ptr<ExprAST>> Args;
  if (CurTok != ')') {
    while (true) {
      if (auto Arg = ParseExpression())
        Args.push_back(std::move(Arg));
      else
        return nullptr;

      if (CurTok == ')')
        break;

      if (CurTok != ',')
        return LogError("Expected ')' or ',' in argument list");
      getNextToken();
    }
  }

  // Eat the ')'.
  getNextToken();

  return std::make_unique<CallExprAST>(IdName, std::move(Args));
}

/// ifexpr ::= 'if' expression 'then' expression 'else' expression
static std::unique_ptr<ExprAST> ParseIfExpr() {
  getNextToken(); // eat the if.

  // condition.
  auto Cond = ParseExpression();
  if (!Cond)
    return nullptr;

  if (CurTok != tok_then)
    return LogError("expected then");
  getNextToken(); // eat the then

  auto Then = ParseExpression();
  if (!Then)
    return nullptr;

  if (CurTok != tok_else)
    return LogError("expected else");

  getNextToken();

  auto Else = ParseExpression();
  if (!Else)
    return nullptr;

  return std::make_unique<IfExprAST>(std::move(Cond), std::move(Then),
                                      std::move(Else));
}

/// forexpr ::= 'for' identifier '=' expr ',' expr (',' expr)? 'in' expression
static std::unique_ptr<ExprAST> ParseForExpr() {
  getNextToken(); // eat the for.

  if (CurTok != tok_identifier)
    return LogError("expected identifier after for");

  std::string IdName = IdentifierStr;
  getNextToken(); // eat identifier.

  if (CurTok != '=')
    return LogError("expected '=' after for");
  getNextToken(); // eat '='.

  auto Start = ParseExpression();
  if (!Start)
    return nullptr;
  if (CurTok != ',')
    return LogError("expected ',' after for start value");
  getNextToken();

  auto End = ParseExpression();
  if (!End)
    return nullptr;

  // The step value is optional.
  std::unique_ptr<ExprAST> Step;
  if (CurTok == ',') {
    getNextToken();
    Step = ParseExpression();
    if (!Step)
      return nullptr;
  }

  if (CurTok != tok_in)
    return LogError("expected 'in' after for");
  getNextToken(); // eat 'in'.

  auto Body = ParseExpression();
  if (!Body)
    return nullptr;

  return std::make_unique<ForExprAST>(IdName, std::move(Start), std::move(End),
                                       std::move(Step), std::move(Body));
}

/// varexpr ::= 'var' identifier ('=' expression)?
//                    (',' identifier ('=' expression)?)* 'in' expression
static std::unique_ptr<ExprAST> ParseVarExpr() {
  getNextToken(); // eat the var.

  std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames;

  // At least one variable name is required.
  if (CurTok != tok_identifier)
    return LogError("expected identifier after var");

  while (true) {
    std::string Name = IdentifierStr;
    getNextToken(); // eat identifier.

    // Read the optional initializer.
    std::unique_ptr<ExprAST> Init = nullptr;
    if (CurTok == '=') {
      getNextToken(); // eat the '='.

      Init = ParseExpression();
      if (!Init)
        return nullptr;
    }

    VarNames.push_back(std::make_pair(Name, std::move(Init)));

    // End of var list, exit loop.
    if (CurTok != ',')
      break;
    getNextToken(); // eat the ','.

    if (CurTok != tok_identifier)
      return LogError("expected identifier list after var");
  }

  // At this point, we have to have 'in'.
  if (CurTok != tok_in)
    return LogError("expected 'in' keyword after 'var'");
  getNextToken(); // eat 'in'.

  auto Body = ParseExpression();
  if (!Body)
    return nullptr;

  return std::make_unique<VarExprAST>(std::move(VarNames), std::move(Body));
}

/// primary
///   ::= identifierexpr
///   ::= numberexpr
///   ::= parenexpr
///   ::= ifexpr
///   ::= forexpr
///   ::= varexpr
static std::unique_ptr<ExprAST> ParsePrimary() {
  switch (CurTok) {
  default:
    return LogError("unknown token when expecting an expression");
  case tok_identifier:
    return ParseIdentifierExpr();
  case tok_number:
    return ParseNumberExpr();
  case '(':
    return ParseParenExpr();
  case tok_if:
    return ParseIfExpr();
  case tok_for:
    return ParseForExpr();
  case tok_var:
    return ParseVarExpr();
  }
}

/// unary
///   ::= primary
///   ::= '!' unary
static std::unique_ptr<ExprAST> ParseUnary() {
  // If the current token is not an operator, it must be a primary expr.
  if (!isascii(CurTok) || CurTok == '(' || CurTok == ',')
    return ParsePrimary();

  // If this is a unary operator, read it.
  int Opc = CurTok;
  getNextToken();
  if (auto Operand = ParseUnary())
    return std::make_unique<UnaryExprAST>(Opc, std::move(Operand));
  return nullptr;
}

/// binoprhs
///   ::= ('+' unary)*
static std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec,
                                              std::unique_ptr<ExprAST> LHS) {
  // If this is a binop, find its precedence.
  while (true) {
    int TokPrec = GetTokPrecedence();

    // If this is a binop that binds at least as tightly as the current binop,
    // consume it, otherwise we are done.
    if (TokPrec < ExprPrec)
      return LHS;

    // Okay, we know this is a binop.
    int BinOp = CurTok;
    getNextToken(); // eat binop

    // Parse the unary expression after the binary operator.
    auto RHS = ParseUnary();
    if (!RHS)
      return nullptr;

    // If BinOp binds less tightly with RHS than the operator after RHS, let
    // the pending operator take RHS as its LHS.
    int NextPrec = GetTokPrecedence();
    if (TokPrec < NextPrec) {
      RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS));
      if (!RHS)
        return nullptr;
    }

    // Merge LHS/RHS.
    LHS =
        std::make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS));
  }
}

/// expression
///   ::= unary binoprhs
///
static std::unique_ptr<ExprAST> ParseExpression() {
  auto LHS = ParseUnary();
  if (!LHS)
    return nullptr;

  return ParseBinOpRHS(0, std::move(LHS));
}

/// prototype
///   ::= id '(' id* ')'
///   ::= binary LETTER number? (id, id)
///   ::= unary LETTER (id)
static std::unique_ptr<PrototypeAST> ParsePrototype() {
  std::string FnName;

  unsigned Kind = 0; // 0 = identifier, 1 = unary, 2 = binary.
  unsigned BinaryPrecedence = 30;

  switch (CurTok) {
  default:
    return LogErrorP("Expected function name in prototype");
  case tok_identifier:
    FnName = IdentifierStr;
    Kind = 0;
    getNextToken();
    break;
  case tok_unary:
    getNextToken();
    if (!isascii(CurTok))
      return LogErrorP("Expected unary operator");
    FnName = "unary";
    FnName += (char)CurTok;
    Kind = 1;
    getNextToken();
    break;
  case tok_binary:
    getNextToken();
    if (!isascii(CurTok))
      return LogErrorP("Expected binary operator");
    FnName = "binary";
    FnName += (char)CurTok;
    Kind = 2;
    getNextToken();

    // Read the precedence if present.
    if (CurTok == tok_number) {
      if (NumVal < 1 || NumVal > 100)
        return LogErrorP("Invalid precedecnce: must be 1..100");
      BinaryPrecedence = (unsigned)NumVal;
      getNextToken();
    }
    break;
  }

  if (CurTok != '(')
    return LogErrorP("Expected '(' in prototype");

  std::vector<std::string> ArgNames;
  while (getNextToken() == tok_identifier)
    ArgNames.push_back(IdentifierStr);
  if (CurTok != ')')
    return LogErrorP("Expected ')' in prototype");

  // success.
  getNextToken(); // eat ')'.

  // Verify right number of names for operator.
  if (Kind && ArgNames.size() != Kind)
    return LogErrorP("Invalid number of operands for operator");

  return std::make_unique<PrototypeAST>(FnName, ArgNames, Kind != 0,
                                         BinaryPrecedence);
}

/// definition ::= 'def' prototype expression
static std::unique_ptr<FunctionAST> ParseDefinition(KaleidoscopeParser &P) {
  getNextToken(); // eat def.
  auto Proto = ParsePrototype();
  if (!Proto)
    return nullptr;

  if (auto E = ParseExpression()) {
    P.FunctionProtos[Proto->getName()] = std::make_unique<PrototypeAST>(*Proto);
    return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
  }
  return nullptr;
}

/// toplevelexpr ::= expression
static std::unique_ptr<FunctionAST> ParseTopLevelExpr(KaleidoscopeParser &P) {
  static std::atomic_uint64_t Counter = 0;

  if (auto E = ParseExpression()) {
    // Make an anonymous proto.
    auto Proto = std::make_unique<PrototypeAST>(
        ("expr." + Twine(Counter++)).str(), std::vector<std::string>());

    P.FunctionProtos[Proto->getName()] = std::make_unique<PrototypeAST>(*Proto);
    return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
  }
  return nullptr;
}

/// external ::= 'extern' prototype
static std::unique_ptr<PrototypeAST> ParseExtern(KaleidoscopeParser &P) {
  getNextToken(); // eat extern.
  auto Proto = ParsePrototype();
  if (Proto)
    P.FunctionProtos[Proto->getName()] = std::make_unique<PrototypeAST>(*Proto);
  return Proto;
}

//===----------------------------------------------------------------------===//
// Code Generation
//===----------------------------------------------------------------------===//

struct CodeGenContext {
  static std::atomic_uint64_t Counter;

  CodeGenContext(const DataLayout &DL) {
    TheModule->setDataLayout(DL);
  }

  std::unique_ptr<LLVMContext> TheContext = std::make_unique<LLVMContext>();
  std::unique_ptr<Module> TheModule =
    std::make_unique<Module>(("m." + Twine(Counter++)).str(), *TheContext);
  std::unique_ptr<IRBuilder<>> Builder = std::make_unique<IRBuilder<>>(*TheContext);
  std::map<std::string, llvm::AllocaInst *> NamedValues;
};

std::atomic_uint64_t CodeGenContext::Counter = 0;
static ExitOnError ExitOnErr;

Value *LogErrorV(const std::string &Str) {
  LogError(Str);
  return nullptr;
}

Function *getFunction(KaleidoscopeParser &P, CodeGenContext &CGCtx, std::string Name) {
  // First, see if the function has already been added to the current module.
  if (auto *F = CGCtx.TheModule->getFunction(Name))
    return F;

  // If not, check whether we can codegen the declaration from some existing
  // prototype.
  auto FI = P.FunctionProtos.find(Name);
  if (FI != P.FunctionProtos.end())
    return FI->second->codegen(P, CGCtx);

  // If no existing prototype exists, return null.
  return nullptr;
}

/// CreateEntryBlockAlloca - Create an alloca instruction in the entry block of
/// the function.  This is used for mutable variables etc.
static AllocaInst *CreateEntryBlockAlloca(Function *TheFunction,
                                          StringRef VarName) {
  IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
                   TheFunction->getEntryBlock().begin());
  return TmpB.CreateAlloca(Type::getDoubleTy(TheFunction->getContext()),
                           nullptr, VarName);
}

Value *NumberExprAST::codegen(KaleidoscopeParser &P, CodeGenContext &CGCtx) {
  return ConstantFP::get(*CGCtx.TheContext, APFloat(Val));
}

Value *VariableExprAST::codegen(KaleidoscopeParser &P, CodeGenContext &CGCtx) {
  // Look this variable up in the function.
  Value *V = CGCtx.NamedValues[Name];
  if (!V)
    return LogErrorV("Unknown variable name");

  // Load the value.
  return CGCtx.Builder->CreateLoad(Type::getDoubleTy(*CGCtx.TheContext), V,
                                   Name.c_str());
}

Value *UnaryExprAST::codegen(KaleidoscopeParser &P, CodeGenContext &CGCtx) {
  Value *OperandV = Operand->codegen(P, CGCtx);
  if (!OperandV)
    return nullptr;

  Function *F = getFunction(P, CGCtx, std::string("unary") + Opcode);
  if (!F)
    return LogErrorV("Unknown unary operator");

  return CGCtx.Builder->CreateCall(F, OperandV, "unop");
}

Value *BinaryExprAST::codegen(KaleidoscopeParser &P, CodeGenContext &CGCtx) {
  // Special case '=' because we don't want to emit the LHS as an expression.
  if (Op == '=') {
    // Assignment requires the LHS to be an identifier.
    // This assume we're building without RTTI because LLVM builds that way by
    // default.  If you build LLVM with RTTI this can be changed to a
    // dynamic_cast for automatic error checking.
    VariableExprAST *LHSE = static_cast<VariableExprAST *>(LHS.get());
    if (!LHSE)
      return LogErrorV("destination of '=' must be a variable");
    // Codegen the RHS.
    Value *Val = RHS->codegen(P, CGCtx);
    if (!Val)
      return nullptr;

    // Look up the name.
    Value *Variable = CGCtx.NamedValues[LHSE->getName()];
    if (!Variable)
      return LogErrorV("Unknown variable name");

    CGCtx.Builder->CreateStore(Val, Variable);
    return Val;
  }

  Value *L = LHS->codegen(P, CGCtx);
  Value *R = RHS->codegen(P, CGCtx);
  if (!L || !R)
    return nullptr;

  switch (Op) {
  case '+':
    return CGCtx.Builder->CreateFAdd(L, R, "addtmp");
  case '-':
    return CGCtx.Builder->CreateFSub(L, R, "subtmp");
  case '*':
    return CGCtx.Builder->CreateFMul(L, R, "multmp");
  case '<':
    L = CGCtx.Builder->CreateFCmpULT(L, R, "cmptmp");
    // Convert bool 0/1 to double 0.0 or 1.0
    return CGCtx.Builder->CreateUIToFP(L, Type::getDoubleTy(*CGCtx.TheContext),
                                       "booltmp");
  default:
    break;
  }

  // If it wasn't a builtin binary operator, it must be a user defined one. Emit
  // a call to it.
  Function *F = getFunction(P, CGCtx, std::string("binary") + Op);
  assert(F && "binary operator not found!");

  Value *Ops[] = {L, R};
  return CGCtx.Builder->CreateCall(F, Ops, "binop");
}

Value *CallExprAST::codegen(KaleidoscopeParser &P, CodeGenContext &CGCtx) {
  // Look up the name in the global module table.
  Function *CalleeF = getFunction(P, CGCtx, Callee);
  if (!CalleeF)
    return LogErrorV("Unknown function " + Callee + " referenced");

  // If argument mismatch error.
  if (CalleeF->arg_size() != Args.size())
    return LogErrorV("Incorrect # arguments passed");

  std::vector<Value *> ArgsV;
  for (unsigned i = 0, e = Args.size(); i != e; ++i) {
    ArgsV.push_back(Args[i]->codegen(P, CGCtx));
    if (!ArgsV.back())
      return nullptr;
  }

  return CGCtx.Builder->CreateCall(CalleeF, ArgsV, "calltmp");
}

Value *IfExprAST::codegen(KaleidoscopeParser &P, CodeGenContext &CGCtx) {
  Value *CondV = Cond->codegen(P, CGCtx);
  if (!CondV)
    return nullptr;

  // Convert condition to a bool by comparing equal to 0.0.
  CondV = CGCtx.Builder->CreateFCmpONE(
      CondV, ConstantFP::get(*CGCtx.TheContext, APFloat(0.0)), "ifcond");

  Function *TheFunction = CGCtx.Builder->GetInsertBlock()->getParent();

  // Create blocks for the then and else cases.  Insert the 'then' block at the
  // end of the function.
  BasicBlock *ThenBB = BasicBlock::Create(*CGCtx.TheContext, "then", TheFunction);
  BasicBlock *ElseBB = BasicBlock::Create(*CGCtx.TheContext, "else");
  BasicBlock *MergeBB = BasicBlock::Create(*CGCtx.TheContext, "ifcont");

  CGCtx.Builder->CreateCondBr(CondV, ThenBB, ElseBB);

  // Emit then value.
  CGCtx.Builder->SetInsertPoint(ThenBB);

  Value *ThenV = Then->codegen(P, CGCtx);
  if (!ThenV)
    return nullptr;

  CGCtx.Builder->CreateBr(MergeBB);
  // Codegen of 'Then' can change the current block, update ThenBB for the PHI.
  ThenBB = CGCtx.Builder->GetInsertBlock();

  // Emit else block.
  TheFunction->insert(TheFunction->end(), ElseBB);
  CGCtx.Builder->SetInsertPoint(ElseBB);

  Value *ElseV = Else->codegen(P, CGCtx);
  if (!ElseV)
    return nullptr;

  CGCtx.Builder->CreateBr(MergeBB);
  // Codegen of 'Else' can change the current block, update ElseBB for the PHI.
  ElseBB = CGCtx.Builder->GetInsertBlock();

  // Emit merge block.
  TheFunction->insert(TheFunction->end(), MergeBB);
  CGCtx.Builder->SetInsertPoint(MergeBB);
  PHINode *PN = CGCtx.Builder->CreatePHI(Type::getDoubleTy(*CGCtx.TheContext), 2, "iftmp");

  PN->addIncoming(ThenV, ThenBB);
  PN->addIncoming(ElseV, ElseBB);
  return PN;
}

// Output for-loop as:
//   var = alloca double
//   ...
//   start = startexpr
//   store start -> var
//   goto loop
// loop:
//   ...
//   bodyexpr
//   ...
// loopend:
//   step = stepexpr
//   endcond = endexpr
//
//   curvar = load var
//   nextvar = curvar + step
//   store nextvar -> var
//   br endcond, loop, endloop
// outloop:
Value *ForExprAST::codegen(KaleidoscopeParser &P, CodeGenContext &CGCtx) {
  Function *TheFunction = CGCtx.Builder->GetInsertBlock()->getParent();

  // Create an alloca for the variable in the entry block.
  AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);

  // Emit the start code first, without 'variable' in scope.
  Value *StartVal = Start->codegen(P, CGCtx);
  if (!StartVal)
    return nullptr;

  // Store the value into the alloca.
  CGCtx.Builder->CreateStore(StartVal, Alloca);

  // Make the new basic block for the loop header, inserting after current
  // block.
  BasicBlock *LoopBB = BasicBlock::Create(*CGCtx.TheContext, "loop",
                                          TheFunction);

  // Insert an explicit fall through from the current block to the LoopBB.
  CGCtx.Builder->CreateBr(LoopBB);

  // Start insertion in LoopBB.
  CGCtx.Builder->SetInsertPoint(LoopBB);

  // Within the loop, the variable is defined equal to the PHI node.  If it
  // shadows an existing variable, we have to restore it, so save it now.
  AllocaInst *OldVal = CGCtx.NamedValues[VarName];
  CGCtx.NamedValues[VarName] = Alloca;

  // Emit the body of the loop.  This, like any other expr, can change the
  // current BB.  Note that we ignore the value computed by the body, but don't
  // allow an error.
  if (!Body->codegen(P, CGCtx))
    return nullptr;

  // Emit the step value.
  Value *StepVal = nullptr;
  if (Step) {
    StepVal = Step->codegen(P, CGCtx);
    if (!StepVal)
      return nullptr;
  } else {
    // If not specified, use 1.0.
    StepVal = ConstantFP::get(*CGCtx.TheContext, APFloat(1.0));
  }

  // Compute the end condition.
  Value *EndCond = End->codegen(P, CGCtx);
  if (!EndCond)
    return nullptr;

  // Reload, increment, and restore the alloca.  This handles the case where
  // the body of the loop mutates the variable.
  Value *CurVar =
    CGCtx.Builder->CreateLoad(Type::getDoubleTy(*CGCtx.TheContext), Alloca,
                              VarName.c_str());
  Value *NextVar = CGCtx.Builder->CreateFAdd(CurVar, StepVal, "nextvar");
  CGCtx.Builder->CreateStore(NextVar, Alloca);

  // Convert condition to a bool by comparing equal to 0.0.
  EndCond = CGCtx.Builder->CreateFCmpONE(
      EndCond, ConstantFP::get(*CGCtx.TheContext, APFloat(0.0)), "loopcond");

  // Create the "after loop" block and insert it.
  BasicBlock *AfterBB =
      BasicBlock::Create(*CGCtx.TheContext, "afterloop", TheFunction);

  // Insert the conditional branch into the end of LoopEndBB.
  CGCtx.Builder->CreateCondBr(EndCond, LoopBB, AfterBB);

  // Any new code will be inserted in AfterBB.
  CGCtx.Builder->SetInsertPoint(AfterBB);

  // Restore the unshadowed variable.
  if (OldVal)
    CGCtx.NamedValues[VarName] = OldVal;
  else
    CGCtx.NamedValues.erase(VarName);

  // for expr always returns 0.0.
  return Constant::getNullValue(Type::getDoubleTy(*CGCtx.TheContext));
}

Value *VarExprAST::codegen(KaleidoscopeParser &P, CodeGenContext &CGCtx) {
  std::vector<AllocaInst *> OldBindings;

  Function *TheFunction = CGCtx.Builder->GetInsertBlock()->getParent();

  // Register all variables and emit their initializer.
  for (unsigned i = 0, e = VarNames.size(); i != e; ++i) {
    const std::string &VarName = VarNames[i].first;
    ExprAST *Init = VarNames[i].second.get();

    // Emit the initializer before adding the variable to scope, this prevents
    // the initializer from referencing the variable itself, and permits stuff
    // like this:
    //  var a = 1 in
    //    var a = a in ...   # refers to outer 'a'.
    Value *InitVal;
    if (Init) {
      InitVal = Init->codegen(P, CGCtx);
      if (!InitVal)
        return nullptr;
    } else { // If not specified, use 0.0.
      InitVal = ConstantFP::get(*CGCtx.TheContext, APFloat(0.0));
    }

    AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);
    CGCtx.Builder->CreateStore(InitVal, Alloca);

    // Remember the old variable binding so that we can restore the binding when
    // we unrecurse.
    OldBindings.push_back(CGCtx.NamedValues[VarName]);

    // Remember this binding.
    CGCtx.NamedValues[VarName] = Alloca;
  }

  // Codegen the body, now that all vars are in scope.
  Value *BodyVal = Body->codegen(P, CGCtx);
  if (!BodyVal)
    return nullptr;

  // Pop all our variables from scope.
  for (unsigned i = 0, e = VarNames.size(); i != e; ++i)
    CGCtx.NamedValues[VarNames[i].first] = OldBindings[i];

  // Return the body computation.
  return BodyVal;
}

Function *PrototypeAST::codegen(KaleidoscopeParser &P, CodeGenContext &CGCtx) {
  // Make the function type:  double(double,double) etc.
  std::vector<Type *> Doubles(Args.size(), Type::getDoubleTy(*CGCtx.TheContext));
  FunctionType *FT =
      FunctionType::get(Type::getDoubleTy(*CGCtx.TheContext), Doubles, false);

  Function *F =
      Function::Create(FT, Function::ExternalLinkage, Name,
                       CGCtx.TheModule.get());

  // Set names for all arguments.
  unsigned Idx = 0;
  for (auto &Arg : F->args())
    Arg.setName(Args[Idx++]);

  return F;
}

FunctionAST::FunctionAST(std::unique_ptr<PrototypeAST> Proto,
                         std::unique_ptr<ExprAST> Body)
    : Proto(std::move(Proto)), Body(std::move(Body)) {}
FunctionAST::~FunctionAST() = default;

const std::string &FunctionAST::getName() const {
  return Proto->getName();
}

void FunctionAST::setName(std::string NewName) {
  Proto->setName(std::move(NewName));
}

Function *FunctionAST::codegen(KaleidoscopeParser &P, CodeGenContext &CGCtx) {
  Function *TheFunction = getFunction(P, CGCtx, Proto->getName());
  if (!TheFunction)
    TheFunction = Proto->codegen(P, CGCtx);
  if (!TheFunction)
    return nullptr;

  // If this is an operator, install it.
  if (Proto->isBinaryOp())
    BinopPrecedence[Proto->getOperatorName()] = Proto->getBinaryPrecedence();

  // Create a new basic block to start insertion into.
  BasicBlock *BB = BasicBlock::Create(*CGCtx.TheContext, "entry", TheFunction);
  CGCtx.Builder->SetInsertPoint(BB);

  // Record the function arguments in the CGCtx.NamedValues map.
  CGCtx.NamedValues.clear();
  for (auto &Arg : TheFunction->args()) {
    // Create an alloca for this variable.
    AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, Arg.getName());

    // Store the initial value into the alloca.
    CGCtx.Builder->CreateStore(&Arg, Alloca);

    // Add arguments to variable symbol table.
    CGCtx.NamedValues[std::string(Arg.getName())] = Alloca;
  }

  if (Value *RetVal = Body->codegen(P, CGCtx)) {
    // Finish off the function.
    CGCtx.Builder->CreateRet(RetVal);

    // Validate the generated code, checking for consistency.
    verifyFunction(*TheFunction);

    // Add Prototype to FunctionProtos map.
    P.FunctionProtos[Proto->getName()] = std::make_unique<PrototypeAST>(*Proto);

    return TheFunction;
  }

  // Error reading body, remove function.
  TheFunction->eraseFromParent();

  if (Proto->isBinaryOp())
    BinopPrecedence.erase(Proto->getOperatorName());
  return nullptr;
}

//===----------------------------------------------------------------------===//
// Top-Level parsing and JIT Driver
//===----------------------------------------------------------------------===//

KaleidoscopeParser::KaleidoscopeParser() = default;
KaleidoscopeParser::~KaleidoscopeParser() = default;

std::optional<KaleidoscopeParser::ParseResult>
KaleidoscopeParser::parse(llvm::StringRef Code) {
  resetInputLine(Code);
  getNextToken();

  while (!InputLine.empty()) {
    switch (CurTok) {
    case tok_eof:
      return std::nullopt;
      break;
    case ';': // ignore top-level semicolons.
      if (getNextToken() != tok_eof) {
        fprintf(stderr, "Error: Unexpected input after top-level semicolon\n");
        return std::nullopt;
      }
      break;
    case tok_def:
      if (auto FnAST = ParseDefinition(*this)) {
        ParseResult PR;
        PR.FnAST = std::move(FnAST);
        return PR;
      } else {
        fprintf(stderr, "Error: Could not parse function definition\n");
        return std::nullopt;
      }
      break;
    case tok_extern: {
      auto ProtoAST = ParseExtern(*this);
      if (ProtoAST) {
        FunctionProtos[ProtoAST->getName()] = std::move(ProtoAST);
        return std::nullopt;
      }
      else {
        fprintf(stderr, "Error: Could not parse extern function declaration\n");
        return std::nullopt;
      }
      break;
    }
    default:
      if (auto FnAST = ParseTopLevelExpr(*this)) {
        ParseResult PR;
        PR.FnAST = std::move(FnAST);
        PR.TopLevelExpr = PR.FnAST->getName();
        return PR;
      } else {
        fprintf(stderr, "Could not parse top-level expression\n");
        return std::nullopt;
      }
      break;
    }
  }

  return std::nullopt;
}

std::optional<ThreadSafeModule>
KaleidoscopeParser::codegen(std::unique_ptr<FunctionAST> FnAST,
                            const DataLayout &DL) {
  CodeGenContext CGCtx(DL);
  if (!FnAST->codegen(*this, CGCtx))
    return std::nullopt;
  return ThreadSafeModule(std::move(CGCtx.TheModule),
                          std::move(CGCtx.TheContext));
}

//===----------------------------------------------------------------------===//
// "Library" functions that can be "extern'd" from user code.
//===----------------------------------------------------------------------===//

/// putchard - putchar that takes a double and returns 0.
extern "C" double putchard(double X) {
  fputc((char)X, stderr);
  return 0;
}

/// printd - printf that takes a double prints it as "%f\n", returning 0.
extern "C" double printd(double X) {
  fprintf(stderr, "%f\n", X);
  return 0;
}

