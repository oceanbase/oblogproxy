#ifndef SQLPARSER_EXPR_H
#define SQLPARSER_EXPR_H

#include <stdlib.h>
#include <memory>
#include <vector>
#include "ColumnType.h"

namespace hsql {
struct SelectStatement;
// Helper function used by the lexer.
char* substr(const char* source, int from, int to);

char* hex2bin(const char* data, size_t size);

enum ExprType {
  kExprLiteralFloat,
  kExprLiteralString,
  kExprLiteralInt,
  kExprLiteralNull,
  kExprLiteralDate,
  kExprLiteralInterval,
  kExprStar,
  kExprParameter,
  kExprColumnRef,
  kExprFunctionRef,
  kExprOperator,
  kExprSelect,
  kExprHint,
  kExprArray,
  kExprArrayIndex,
  kExprExtract,
  kExprCast,
  kExprVar
};

// Operator types. These are important for expressions of type kExprOperator.
enum OperatorType {
  kOpNone,

  // Ternary operator
  kOpBetween,

  // n-nary special case
  kOpCase,
  kOpCaseListElement,  // `WHEN expr THEN expr`

  // Binary operators.
  kOpPlus,
  kOpMinus,
  kOpAsterisk,
  kOpSlash,
  kOpPercentage,
  kOpCaret,

  kOpEquals,
  kOpNotEquals,
  kOpLess,
  kOpLessEq,
  kOpGreater,
  kOpGreaterEq,
  kOpLike,
  kOpNotLike,
  kOpILike,
  kOpAnd,
  kOpOr,
  kOpIn,
  kOpConcat,

  // Unary operators.
  kOpNot,
  kOpUnaryMinus,
  kOpIsNull,
  kOpExists
};

enum DatetimeField {
  kDatetimeNone,
  kDatetimeSecond,
  kDatetimeMinute,
  kDatetimeHour,
  kDatetimeDay,
  kDatetimeMonth,
  kDatetimeYear,
};

typedef struct Expr Expr;
/*
 *The type of the current variable,
 * whether it is a system variable or a user variable
 */
enum VarType { kSys, kUser };
enum VarLevel { Global, Local, Session };
struct SetClause {
  char* column;
  Expr* value;
  VarLevel type;
  VarType var_type = kSys;
};
// Represents SQL expressions (i.e. literals, operators, column_refs).
// TODO: When destructing a placeholder expression, we might need to alter the
// placeholder_list.
struct Expr {
  Expr(ExprType type);
  virtual ~Expr();

  ExprType type;

  // TODO: Replace expressions by list.
  Expr* expr;
  Expr* expr2;
  std::vector<Expr*>* exprList;
  SelectStatement* select;
  char* name;
  char* table;
  char* alias;
  double fval;
  int64_t ival;
  int64_t ival2;
  DatetimeField datetimeField;
  ColumnType columnType;
  bool isBoolLiteral;

  OperatorType opType;
  bool distinct;

  VarType var_type;
  VarLevel var_level;
  // Convenience accessor methods.

  bool isType(ExprType exprType) const;

  bool isLiteral() const;

  bool hasAlias() const;

  bool hasTable() const;

  const char* getName() const;

  std::string get_value() const;

  // Static constructors.

  static Expr* make(ExprType type);

  static Expr* makeOpUnary(OperatorType op, Expr* expr);

  static Expr* makeOpBinary(Expr* expr1, OperatorType op, Expr* expr2);

  static Expr* makeBetween(Expr* expr, Expr* left, Expr* right);

  static Expr* makeCaseList(Expr* caseListElement);

  static Expr* makeCaseListElement(Expr* when, Expr* then);

  static Expr* caseListAppend(Expr* caseList, Expr* caseListElement);

  static Expr* makeCase(Expr* expr, Expr* when, Expr* elseExpr);

  static Expr* makeLiteral(int64_t val);

  static Expr* makeLiteral(double val);

  static Expr* makeLiteral(char* val);

  static Expr* makeLiteral(bool val);

  static Expr* makeNullLiteral();

  static Expr* makeDateLiteral(char* val);

  static Expr* makeIntervalLiteral(int64_t duration, DatetimeField unit);

  static Expr* makeColumnRef(char* name);

  static Expr* makeColumnRef(char* table, char* name);

  static Expr* makeStar(void);

  static Expr* makeStar(char* table);

  static Expr* makeFunctionRef(char* func_name, std::vector<Expr*>* exprList, bool distinct);

  static Expr* makeArray(std::vector<Expr*>* exprList);

  static Expr* makeArrayIndex(Expr* expr, int64_t index);

  static Expr* makeParameter(int id);

  static Expr* makeSelect(SelectStatement* select);

  static Expr* makeExists(SelectStatement* select);

  static Expr* makeInOperator(Expr* expr, std::vector<Expr*>* exprList);

  static Expr* makeInOperator(Expr* expr, SelectStatement* select);

  static Expr* makeExtract(DatetimeField datetimeField1, Expr* expr);

  static Expr* makeCast(Expr* expr, ColumnType columnType);

  static Expr* makeVar(SetClause* set_clause);
};

// Zero initializes an Expr object and assigns it to a space in the heap
// For Hyrise we still had to put in the explicit NULL constructor
// http://www.ex-parrot.com/~chris/random/initialise.html
// Unused
#define ALLOC_EXPR(var, type)         \
  Expr* var;                          \
  do {                                \
    Expr zero = {type};               \
    var = (Expr*)malloc(sizeof *var); \
    *var = zero;                      \
  } while (0);
#undef ALLOC_EXPR

}  // namespace hsql

#endif
