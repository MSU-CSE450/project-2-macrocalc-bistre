#pragma once

#include <cmath>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "SymbolTable.hpp"
class ASTNode {

private:
  std::vector<ASTNode> children{};

public:
  enum Type {
    EMPTY = 0,
    SCOPE,
    PRINT,
    ASSIGN,
    IDENTIFIER,
    CONDITIONAL,
    OPERATION,
    NUMBER,
    WHILE,
    STRING
  };
  const Type type;
  double value{};
  size_t var_id{};
  // can also serve as an operation name if of type OPERATION. Might also
  // change things so we have another enum of operator types.
  std::string literal{};
  Token const *token = nullptr; // for error reporting

  ASTNode(Type type = EMPTY) : type(type) {};
  ASTNode(Type type, std::string literal) : type(type), literal(literal) {};
  ASTNode(Type type, double value) : type(type), value(value) {};
  ASTNode(Type type, size_t var_id, Token const *token)
      : type(type), var_id(var_id), token(token) {};

  operator int() const { return type; }

  void AddChild(ASTNode node) {
    if (node) {
      children.push_back(node);
    }
  }

  template <typename T, typename... Rest>
  void AddChildren(T node, Rest... rest) {
    AddChild(node);
    AddChildren(rest...);
  }

  template <typename T> void AddChildren(T node) { AddChild(node); }

  std::optional<double> Run(SymbolTable &symbols) {
    switch (type) {
    case EMPTY:
      return std::nullopt;
    case SCOPE:
      RunScope(symbols);
      return std::nullopt;
    case PRINT:
      RunPrint(symbols);
      return std::nullopt;
    case ASSIGN:
      RunAssign(symbols);
      return std::nullopt;
    case IDENTIFIER:
      return RunIdentifier(symbols);
    case CONDITIONAL:
      RunConditional(symbols);
      return std::nullopt;
    case OPERATION:
      return RunOperation(symbols);
    case NUMBER:
      return value;
    case WHILE:
      RunWhile(symbols);
      return std::nullopt;
    default:
      assert(false);
      return std::nullopt; // rose: thank you gcc very cool
    };
  }

  double RunExpect(SymbolTable &symbols) {
    if (auto result = Run(symbols)) {
      return result.value();
    }
    throw std::runtime_error("Child did not return value!");
  }

  // note: I would have made some of these SymbolTable references constant, but
  // they'll be making recursive calls to Run, some of which will need to make
  // changes to the symbol table, so none of them are constant except the one in
  // RunIdentifier
  void RunScope(SymbolTable &symbols) {
    // push a new scope
    // run each child node in order
    // pop scope
    for (ASTNode child : children) {
      child.Run(symbols);
    }
  }
  void RunPrint(SymbolTable &symbols) {
    // iterate over children
    // if child is an expression or number, run it and print the value it
    // returns if it's a string literal, print it need to do something about
    // identifiers in curly braces
    for (ASTNode child : children) {
      if (child.type == ASTNode::STRING) {
        std::cout << child.literal;
      } else {
        std::cout << child.RunExpect(symbols);
      }
    }
    std::cout << std::endl;
  }
  void RunAssign(SymbolTable &symbols) {
    assert(children.size() == 2);
    symbols.SetValue(children.at(0).var_id, children.at(1).RunExpect(symbols));
  }
  double RunIdentifier(SymbolTable &symbols) {
    assert(value == double{});
    assert(literal == std::string{});

    return symbols.GetValue(var_id, token);
  }
  void RunConditional([[maybe_unused]] SymbolTable &symbols) {
    // conditional statement is of the form "if (expression1) statment1 else
    // statement2" so a conditional node should have 2 or 3 children: an
    // expression, a statement, and possibly another statement run the first
    // one; if it gives a nonzero value, run the second; otherwise, run the
    // third, if it exists
  }
  double RunOperation([[maybe_unused]] SymbolTable &symbols) {
    // node will have an operator (e.g. +, *, etc.) specified somewhere (maybe
    // in the "literal"?) and one or two children run the child or children,
    // apply the operator to the returned value(s), then return the result
    if (literal == "!" || literal == "-"){
      assert(children.size() == 1);
      double left = children[0].RunExpect(symbols);
      if (literal == "!"){
        return left == 0 ? 1 : 0;
      } else {
        return -1 * left;
      }
    } else {
      assert(children.size() == 2);
      double left = children[0].RunExpect(symbols);
      if (literal == "&&"){
        if (!left) return 0; //short-circuit when left is false
        return children[1].RunExpect(symbols) != 0;
      } else if (literal == "||"){
        if (left) return 1; //short-circuit when left is true
        return children[1].RunExpect(symbols) != 0;
      }
      //don't evaluate the right until you know you won't have to short-circuit
      double right = children[1].RunExpect(symbols);
      if (literal == "**"){
        return pow(left, right);
      } else if (literal == "*") {
        return left * right;
      } else if (literal == "/"){
        if (right == 0){
          throw std::runtime_error("Division by zero");
        }
        return left / right;
      } else if (literal == "%"){
        if (right == 0){
          throw std::runtime_error("Modulus by zero");
        }
        return int(left) % int(right);
      } else if (literal == "+"){
        return left + right;
      } else if (literal == "-"){
        return left - right;
      } else if (literal == "<"){
        return left < right;
      } else if (literal == ">"){
        return left > right;
      } else if (literal == "<="){
        return left <= right;
      } else if (literal == ">=") {
        return left >= right;
      } else if (literal == "=="){
        return left == right;
      } else if (literal == "!="){
        return left != right;
      } else {
        throw std::runtime_error("Tried to run unknown operator");
      }
    }
  }
  void RunWhile(SymbolTable & symbols){
    assert(children.size() == 2);
    assert(value == double{});
    assert(literal == std::string{});

    ASTNode condition = children[0];
    ASTNode body = children[1];
    while (condition.RunExpect(symbols)){
      body.Run(symbols);
    }
  }
};
