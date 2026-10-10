#pragma once

#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <cpy/common.hpp>

enum class NodeType
{
  None,
  SyntaxError,
  SourceFile,
  FunctionDef,
  FunctionParam,
  FunctionBody,
  Expression,
  VariableDecl
};

enum class BuiltInType
{
  Unset,
  Int,
  Decimal,
  Bool,
  String,
  Void
};


struct UserType
{
  std::string name; // TODO or std::string_view
};

struct VarType
{
  VarType() : type (BuiltInType::Unset)
  {}

  VarType(const BuiltInType t) : type (t)
  {}

  const auto& value() const
  {
    return type;
  }

  template<typename T>
  const std::optional<T> value_as() const
  {
    if (const auto param_type = std::get_if<T>(&type); param_type)
      return *param_type;
    return std::nullopt;
  }

  template<typename T>
  bool is_type() const requires(std::is_same_v<T, BuiltInType>)
  {
    return std::holds_alternative<T>(type);
  }

  bool is_type(const BuiltInType t) const
  {
    return is_type<BuiltInType>() && *(value_as<BuiltInType>()) == t;
  }

  explicit operator bool() const
  {
    return !is_type(BuiltInType::Unset);
  }

private:
  std::variant<BuiltInType, UserType> type;
};

inline constexpr const auto unresolved_t = BuiltInType::Unset;


inline bool operator==([[maybe_unused]] const UserType& a, [[maybe_unused]] const UserType& b)
{
  throw std::runtime_error{"Comparing unsupported UserType"};
}

inline bool operator==(const VarType& a, const VarType& b)
{
  return a.value() == b.value();
}

// AST nodes //

struct AstNode
{
  AstNode(const NodeType t) : type(t)
  {}

  SourceRegion source;

  NodeType node_type() const { return type; }
  bool is_node_type(const NodeType t) const { return t == node_type(); }

  virtual ~AstNode() = default;
  virtual void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const = 0;

private:
  NodeType type;
};

// Expressions //
enum class BinaryOperator
{
  Add,
  Subtract,
  Multiply,
  Divide,
  Equal,
  NotEqual,
  Less,
  Greater,
  LessEqual,
  GreaterEqual,
};

enum class ExpressionType
{
  Int,
  String,
  Dec,
  Bool,
  Binary,
  FuncCall,
  VariableRef
};


inline std::string_view to_string(const BuiltInType t);
inline std::string_view to_string(const VarType& t);
inline std::string_view to_string(const BinaryOperator op);


struct Expression : public AstNode
{
  static constexpr NodeType Type = NodeType::Expression;

  Expression(const ExpressionType t)
    : AstNode(Type)
    , ex_type(t)
  {}

  virtual ~Expression() = default;

  virtual bool is_expr_type(const ExpressionType t) const { return t == expr_type(); }
  virtual bool is_convertible_to([[maybe_unused]] const BuiltInType t) const { return false; }
  virtual VarType get_var_type() const = 0;

  ExpressionType expr_type() const { return ex_type; }

private:
  ExpressionType ex_type;
};

struct IntegerLiteral : public Expression
{
  static constexpr ExpressionType ExprType = ExpressionType::Int;

  explicit IntegerLiteral(const int64_t val) : Expression(ExprType), v(val)
  {}

  bool is_convertible_to(const BuiltInType t) const override
  {
    return t == BuiltInType::Int;
  }

  VarType get_var_type() const override
  {
    return BuiltInType::Int;
  }

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const override
  {
    os << "int = " << v;
  }

  int64_t v;
};

struct StringLiteral : public Expression
{
  static constexpr ExpressionType ExprType = ExpressionType::String;

  explicit StringLiteral(const std::string_view val) : Expression(ExprType), v(val)
  {}

  bool is_convertible_to(const BuiltInType t) const override
  {
    return t == BuiltInType::String;
  }

  VarType get_var_type() const override
  {
    return BuiltInType::String;
  }

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const override
  {
    os << "str = " << v;
  }

  std::string_view v;
};

struct DecimalLiteral : public Expression
{
  static constexpr ExpressionType ExprType = ExpressionType::Dec;

  explicit DecimalLiteral(const double val) : Expression(ExprType), v(val)
  {}

  bool is_convertible_to(const BuiltInType t) const override
  {
    return t == BuiltInType::Decimal;
  }

  VarType get_var_type() const override
  {
    return BuiltInType::Decimal;
  }

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const override
  {
    os << "dec = " << v;
  }

  double v;
};

struct BooleanLiteral : public Expression
{
  static constexpr ExpressionType ExprType = ExpressionType::Bool;

  explicit BooleanLiteral(const bool val) : Expression(ExprType), v(val)
  {}

  bool is_convertible_to(const BuiltInType t) const override
  {
    return t == BuiltInType::Bool;
  }

  VarType get_var_type() const override
  {
    return BuiltInType::Bool;
  }

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const override
  {
    os << "bool = " << std::boolalpha << v;
  }

  bool v;
};


struct BinaryExpression : public Expression
{
  static constexpr ExpressionType ExprType = ExpressionType::Binary;

  explicit BinaryExpression() : Expression(ExprType)
  {}

  explicit BinaryExpression(std::unique_ptr<Expression>&& lhs, std::unique_ptr<Expression>&& rhs, const BinaryOperator op)
  : Expression(ExprType)
  , lhs(std::move(lhs))
  , op(op)
  , rhs(std::move(rhs))
  {}

  bool is_convertible_to(const BuiltInType t) const override
  {
    return lhs->is_convertible_to(t) && rhs->is_convertible_to(t);
  }

  VarType get_var_type() const override
  {
    if (lhs->is_expr_type(ExpressionType::FuncCall) || rhs->is_expr_type(ExpressionType::FuncCall)) {
      throw std::runtime_error{"get_var_type() called on BinaryExpression with FunctionCall operand(s)"};
    }

    if (lhs->get_var_type() == rhs->get_var_type()) {
      return lhs->get_var_type();
    }
    else {
      throw std::runtime_error{"get_var_type() called on BinaryExpression with incompatible operands"};
    }
  }

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const override
  {
    lhs->dump(os, tab);
    os << ' ' << to_string(op) << ' ';
    rhs->dump(os, tab);
  }

  std::unique_ptr<Expression> lhs;
  BinaryOperator op;
  std::unique_ptr<Expression> rhs;

};


struct FunctionCall : public Expression
{
  static constexpr ExpressionType ExprType = ExpressionType::FuncCall;

  std::string_view module;
  std::string_view name;
  std::vector<std::unique_ptr<Expression>> args;

  FunctionCall(const std::string_view name, std::vector<std::unique_ptr<Expression>> args = {})
    : Expression(ExprType)
    , name(name)
    , args(std::move(args))
  {}

  FunctionCall(const std::string_view name, std::vector<std::unique_ptr<Expression>> args, const std::string_view module)
    : Expression(ExprType)
    , module(module)
    , name(name)
    , args(std::move(args))
  {}

  VarType get_var_type() const override
  {
    // never called because the type is taken from the return type of
    // the function being called
    throw std::runtime_error{"get_var_type() called on FunctionCall"};
  }

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const override
  {
    if (!module.empty()) {
      os << module << "::";
    }
    os << name << '(';
    for (std::size_t i = 0; i < args.size() ; ++i)
    {
      args[i]->dump(os, tab);
      if (i+1 < args.size())
        os << ',';
    }
    os << ')';
  }
};


// Note: not a "ref" like a C++ reference, but "a node which refers to a variable"
struct VariableRef : public Expression
{
  static constexpr ExpressionType ExprType = ExpressionType::VariableRef;

  VariableRef() : Expression(ExprType)
  {}

  VariableRef(const std::string_view name)
    : Expression(ExprType)
    , name(name)
  {}

  VarType get_var_type() const override
  {
    // never called because the type is taken from the type of the variable
    throw std::runtime_error{"get_var_type() called on VariableRef"};
  }

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const override
  {
    os << name;
  }

  std::string_view name;
};

inline std::string_view to_string(const Expression& expr);

template<typename ExprT> requires (std::derived_from<ExprT, Expression>)
const ExprT& get_expression(const std::unique_ptr<Expression>& expr)
{
  if (!expr->is_expr_type(ExprT::ExprType)) {
    throw std::runtime_error{"get_expression() called with expr and ExprT mismatch"};
  }
  return dynamic_cast<ExprT&>(*expr);
}


// Functions Definition //

struct FunctionParam : public AstNode
{
  static constexpr NodeType Type = NodeType::FunctionParam;

  std::string_view param_name;
  std::string_view type_name;


  FunctionParam() : AstNode(NodeType::FunctionParam)
  {}

  FunctionParam(const std::string_view param_name, const std::string_view type)
    : AstNode(NodeType::FunctionParam)
    , param_name(param_name)
    , type_name(type)
  {}

public:

  void dump (std::ostream& os, const uint8_t tab = 0) const override
  {
    os << std::string(tab*2, ' ') << param_name << ":" << type_name << '\n';
  }
};


struct FunctionBody : public AstNode
{
  static constexpr NodeType Type = NodeType::FunctionBody;

  FunctionBody() : AstNode(Type)
  {}

  void add_node(std::unique_ptr<AstNode>&& node)
  {
    if (node) {
      nodes.push_back(std::move(node));
    }
  }

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const override
  {
    for (const auto& n : nodes)
      n->dump(os);
  }

  std::vector<std::unique_ptr<AstNode>> nodes;
};

struct FunctionDef : public AstNode
{
  static constexpr NodeType Type = NodeType::FunctionDef;

  FunctionDef() : AstNode(Type)
  {}

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const override
  {
    os << name << "() -> " << return_type << '\n';

    for(const auto& p : params)
    {
      os << "  > " ;
      p.dump(os, tab);
    }

    body.dump(os, tab);
  }


  std::string name;
  std::vector<FunctionParam> params;
  std::string_view return_type;
  FunctionBody body;
};


// <var_name>: <var_type>;                  // explicit type
// <var_name>: <var_type> = <init_expr>;    // explicit type, initialised
// <var_name> := <init_expr>;               // inferred type, initialised
// foo: int;
// foo: int = 5;
// foo := 5;
struct VariableDecl : public AstNode
{
  static constexpr NodeType Type = NodeType::VariableDecl;

  enum class DeclType
  {
    ExplicitNoInit,
    ExplicitInit,
    InferredType
  };

  VariableDecl() : AstNode(Type)
  {}

  static VariableDecl create_explicit(std::unique_ptr<Expression>&& init)
  {
    VariableDecl decl;
    decl.decl_type = init ? DeclType::ExplicitInit : DeclType::ExplicitNoInit;
    decl.initialiser = std::move(init);
    return decl;
  }

  static VariableDecl create_inferred(std::unique_ptr<Expression>&& init)
  {
    if (!init) {
      throw std::runtime_error{"VariableDecl - InferredType created without init expr"};
    }

    VariableDecl decl;
    decl.decl_type = DeclType::InferredType;
    decl.initialiser = std::move(init);
    return decl;
  }

  DeclType get_decl_type() const
  {
    return decl_type;
  }

  bool has_explicit_type() const
  {
    return decl_type == DeclType::ExplicitNoInit || decl_type == DeclType::ExplicitInit;
  }

  bool has_initialiser() const
  {
    return initialiser != nullptr;
  }

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const override
  {
    os << var_name << ':';

    if (has_explicit_type()) {
      os << var_type;
    }
    else if (initialiser) {
      os << "[inferred " << to_string(*initialiser) << "]";
    }
  }

  std::string_view var_name;
  std::string_view var_type;
  std::unique_ptr<Expression> initialiser;

  private:
    DeclType decl_type;
};



struct SyntaxError : public AstNode
{
  static constexpr NodeType Type = NodeType::SyntaxError;

  SyntaxError() : AstNode(Type)
  {}

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const override
  {
    os << "ERROR\n";
  }
};


struct SourceFile : public AstNode
{
  static constexpr NodeType Type = NodeType::SourceFile;

  SourceFile() : AstNode(Type)
  {}

  std::vector<std::unique_ptr<AstNode>> nodes;
  fs::path src_path;

  void dump (std::ostream& os, [[maybe_unused]] const uint8_t tab = 0) const override
  {
    os << (src_path.empty() ? "" : src_path.string()) << '\n';

    for (const auto& n : nodes)
    {
      n->dump(os) ;
      os << '\n';
    }
  }
};


// to_string
inline std::string_view to_string(const BuiltInType t)
{
  switch (t)
  {
    using enum BuiltInType;

    case Int:
      return "int";
    case Decimal:
      return "dec";
    case Bool:
      return "bool";
    case String:
      return "str";
    case Void:
      return "void";
    default:
      return "Unknown";
  }
}

inline std::string_view to_string(const VarType& t)
{
  if (t.is_type<BuiltInType>()) {
    return to_string(std::get<BuiltInType>(t.value()));
  }
  else {
     throw std::runtime_error{"UserType not implemented"};
  }
}

inline std::string_view to_string(const BinaryOperator op)
{
  switch (op)
  {
    case BinaryOperator::Add:         return "+";
    case BinaryOperator::Subtract:    return "-";
    case BinaryOperator::Multiply:    return "*";
    case BinaryOperator::Divide:      return "/";
    case BinaryOperator::Equal:       return "==";
    case BinaryOperator::NotEqual:    return "!=";
    case BinaryOperator::Less:        return "<";
    case BinaryOperator::Greater:     return ">";
    case BinaryOperator::LessEqual:   return "<=";
    case BinaryOperator::GreaterEqual: return ">=";
  }

  std::unreachable();
}

inline std::string_view to_string(const Expression& expr)
{
  switch (expr.expr_type())
  {
    using enum ExpressionType;

    case Dec:
      return "DecimalLiteral";
    case Int:
      return "IntegerLiteral";
    case Bool:
      return "BooleanLiteral";
    case String:
      return "StringLiteral";
    case Binary:
      return "BinaryExpression";
    case FuncCall:
      return "FunctionCall";
    case VariableRef:
      return "VariableReference";
  }

  std::unreachable();
}
