#include <cassert>
#include <fstream>
#include <memory>
#include <string>

#include <vector>

#include "ASTNode.hpp"
#include "Error.hpp"
#include "SymbolTable.hpp"
#include "lexer.hpp"
#include "string_lexer.hpp"

using namespace emplex;

class MacroCalc {
private:
  std::vector<Token> tokens{};
  emplex::Lexer lexer{};
  SymbolTable table{};
  size_t token_idx{0};
  ASTNode root{ASTNode::SCOPE};

  emplex2::StringLexer string_lexer{};

  Token const &CurToken() const {
    if (token_idx >= tokens.size())
      ErrorNoLine("Unexpected EOF");
    return tokens.at(token_idx);
  }

  Token const &ConsumeToken() {
    if (token_idx >= tokens.size())
      ErrorNoLine("Unexpected EOF");
    return tokens.at(token_idx++);
  }

  Token const &ExpectToken(int token) {
    if (CurToken() == token) {
      return ConsumeToken();
    }
    ErrorUnexpected(CurToken(), token);
  }

  // rose: C++ optionals can't hold references, grumble grumble
  Token const *IfToken(int token) {
    if (CurToken() == token) {
      return &ConsumeToken();
    }
    return nullptr;
  }

  ASTNode ParseScope() {
    ExpectToken(Lexer::ID_SCOPE_Start);
    ASTNode scope{ASTNode::SCOPE};
    table.PushScope();

    while (CurToken() != Lexer::ID_SCOPE_END) {
      scope.AddChild(ParseStatement());
    }
    ConsumeToken();
    table.PopScope();
    return scope;
  }

  ASTNode ParseDecl() {
    ExpectToken(Lexer::ID_VAR);
    Token const &ident = ExpectToken(Lexer::ID_ID);
    if (IfToken(Lexer::ID_ENDLINE)) {
      table.AddVar(ident.lexeme, ident.line_id);
      return ASTNode{};
    }
    ExpectToken(Lexer::ID_ASSIGN);

    ASTNode expr = ParseExpr();
    ExpectToken(Lexer::ID_ENDLINE);

    // don't add until _after_ we possibly resolve idents in expression
    // ex. var foo = foo should error if foo is undefined
    size_t var_id = table.AddVar(ident.lexeme, ident.line_id);

    ASTNode out = ASTNode{ASTNode::ASSIGN};
    out.AddChildren(ASTNode(ASTNode::IDENTIFIER, var_id, &ident),
                    std::move(expr));

    return out;
  }

  ASTNode ParseExpr() { return ParseAssign(); }

  ASTNode ParseAssign() {
    ASTNode lhs = ParseOr();
    if (CurToken().lexeme == "=") {
      // can only have variable names as the LHS of an assignment
      if (lhs.type != ASTNode::IDENTIFIER) {
        ErrorUnexpected(CurToken(), Lexer::ID_ID);
      }
      ExpectToken(Lexer::ID_ASSIGN);
      ASTNode rhs = ParseAssign();
      return ASTNode(ASTNode::ASSIGN, "=", std::move(lhs), std::move(rhs));
    }
    return lhs;
  }

  ASTNode ParseOr() {
    auto lhs = std::make_unique<ASTNode>(ParseAnd());
    while (CurToken().lexeme == "||") {
      ConsumeToken();
      ASTNode rhs = ParseAnd();
      lhs = std::make_unique<ASTNode>(
          ASTNode(ASTNode::OPERATION, "||", std::move(*lhs), std::move(rhs)));
    }
    return ASTNode{std::move(*lhs)};
  }

  ASTNode ParseAnd() {
    auto lhs = std::make_unique<ASTNode>(ParseEquals());
    while (CurToken().lexeme == "&&") {
      ConsumeToken();
      ASTNode rhs = ParseEquals();
      lhs = std::make_unique<ASTNode>(
          ASTNode(ASTNode::OPERATION, "&&", std::move(*lhs), std::move(rhs)));
    }
    return ASTNode{std::move(*lhs)};
  }

  ASTNode ParseEquals() {
    auto lhs = std::make_unique<ASTNode>(ParseCompare());
    if (CurToken() == Lexer::ID_EQUALS) {
      std::string operation = ExpectToken(Lexer::ID_EQUALS).lexeme;
      ASTNode rhs = ParseCompare();
      return ASTNode(ASTNode::OPERATION, operation, std::move(*lhs),
                     std::move(rhs));
    }
    return ASTNode{std::move(*lhs)};
  }

  ASTNode ParseCompare() {
    auto lhs = std::make_unique<ASTNode>(ParseAddSub());
    if (CurToken() == Lexer::ID_COMPARE) {
      std::string operation = ExpectToken(Lexer::ID_COMPARE).lexeme;
      ASTNode rhs = ParseAddSub();
      return ASTNode(ASTNode::OPERATION, operation, std::move(*lhs),
                     std::move(rhs));
    }
    return ASTNode{std::move(*lhs)};
  }

  ASTNode ParseAddSub() {
    auto lhs = std::make_unique<ASTNode>(ParseMulDivMod());
    while (CurToken().lexeme == "+" || CurToken().lexeme == "-") {
      std::string operation = ConsumeToken().lexeme;
      ASTNode rhs = ParseMulDivMod();
      lhs = std::make_unique<ASTNode>(ASTNode(ASTNode::OPERATION, operation,
                                              std::move(*lhs), std::move(rhs)));
    }
    return ASTNode{std::move(*lhs)};
  }

  ASTNode ParseMulDivMod() {
    auto lhs = std::make_unique<ASTNode>(ParseExponentiation());
    while (CurToken().lexeme == "*" || CurToken().lexeme == "/" ||
           CurToken().lexeme == "%") {
      std::string operation = ConsumeToken().lexeme;
      ASTNode rhs = ParseExponentiation();
      lhs = std::make_unique<ASTNode>(ASTNode(ASTNode::OPERATION, operation,
                                              std::move(*lhs), std::move(rhs)));
    }
    return ASTNode{std::move(*lhs)};
  }

  ASTNode ParseExponentiation() {
    ASTNode lhs = ParseTerm();
    if (CurToken().lexeme == "**") {
      ConsumeToken();
      ASTNode rhs = ParseExponentiation();
      return ASTNode(ASTNode::OPERATION, "**", std::move(lhs), std::move(rhs));
    }
    return lhs;
  }

  ASTNode ParseNegate(){
    auto lhs = std::make_unique<ASTNode>(ASTNode::NUMBER, -1);
    auto rhs = ParseTerm();
    return ASTNode(ASTNode::OPERATION, "*", std::move(*lhs), std::move(rhs));
  }

  ASTNode ParseNOT(){
    auto rhs = ParseTerm();
    return ASTNode(ASTNode::OPERATION, "!", std::move(rhs));
  }

  ASTNode ParseTerm() {
    Token const &current = CurToken();
    switch (current) {
    case Lexer::ID_NUMBER:
      return ASTNode(ASTNode::NUMBER, std::stod(ConsumeToken().lexeme));
    case Lexer::ID_ID:
      return ASTNode(ASTNode::IDENTIFIER,
                     table.FindVar(ConsumeToken().lexeme, current.line_id),
                     &current);
    case Lexer::ID_OPEN_PARENTHESIS: {
      ExpectToken(Lexer::ID_OPEN_PARENTHESIS);
      ASTNode subexpression = ParseExpr();
      ExpectToken(Lexer::ID_CLOSE_PARENTHESIS);
      return subexpression;
    }
    case Lexer::ID_MATH:
      if (current.lexeme == "-") {
        ConsumeToken();
        return ParseNegate();
      }
      ErrorUnexpected(current);
    case Lexer::ID_NOT:
      ConsumeToken();
      return ParseNOT();
    default:
      ErrorUnexpected(current);
    }
    return ASTNode{};
  }

  ASTNode ParsePrint() {
    ExpectToken(Lexer::ID_PRINT);
    ExpectToken(Lexer::ID_OPEN_PARENTHESIS);
    ASTNode node{ASTNode::PRINT};
    if (auto current = IfToken(Lexer::ID_STRING)) {
      // strip quotes
      std::string to_print =
          current->lexeme.substr(1, current->lexeme.length() - 2);
      std::vector<emplex2::Token> string_pieces =
          string_lexer.Tokenize(to_print);
      for (auto token : string_pieces) {
        switch (token.id) {
        case emplex2::StringLexer::ID_LITERAL:
          node.AddChild(ASTNode(ASTNode::STRING, token.lexeme));
          break;
        case emplex2::StringLexer::ID_ESCAPE_CHAR:
          node.AddChild(ASTNode(ASTNode::STRING, token.lexeme));
          break;
        case emplex2::StringLexer::ID_IDENTIFIER: {
          std::string ident = token.lexeme.substr(1, token.lexeme.length() - 2);
          node.AddChild(ASTNode(ASTNode::IDENTIFIER,
                                table.FindVar(ident, current->line_id),
                                nullptr));
          break;
        }
        default:
          // Since ID_LITERAL is a catchall for everything else, I don't think
          // there should be any unexpected tokens in strings, but I'll think
          // about it some more and maybe  add some better error handling.
          assert(false);
        }
      }
    } else {
      node.AddChild(ParseExpr()); // NOTE TO SELF - CHANGE THIS BACK LATER
    }
    ExpectToken(Lexer::ID_CLOSE_PARENTHESIS);
    ExpectToken(Lexer::ID_ENDLINE);
    return node;
  }

  ASTNode ParseIF() {
    ExpectToken(Lexer::ID_IF);
    ExpectToken(Lexer::ID_OPEN_PARENTHESIS);

    if (CurToken() == Lexer::ID_CLOSE_PARENTHESIS) {
      Error(CurToken(), "Expected condition body, found empty condition");
    }

    ASTNode node{ASTNode::CONDITIONAL};

    node.AddChild(ParseExpr());

    ExpectToken(Lexer::ID_CLOSE_PARENTHESIS);

    node.AddChild(ParseStatement());

    if (IfToken(Lexer::ID_ELSE)) {
      node.AddChild(ParseStatement());
    }

    return node;
  }

  ASTNode ParseWhile() {
    ExpectToken(Lexer::ID_WHILE);
    ExpectToken(Lexer::ID_OPEN_PARENTHESIS);
    ASTNode node = ASTNode(ASTNode::WHILE);
    node.AddChild(ParseExpr());

    ExpectToken(Lexer::ID_CLOSE_PARENTHESIS);
    node.AddChild(ParseStatement());
    return node;
  }

  ASTNode ParseStatement() {
    Token const &current = CurToken();
    switch (current) {
    case Lexer::ID_SCOPE_Start:
      return ParseScope();
    case Lexer::ID_VAR:
      return ParseDecl();
    case Lexer::ID_ID: {
      ASTNode node = ParseExpr();
      ExpectToken(Lexer::ID_ENDLINE);
      return node;
    }
    case Lexer::ID_PRINT:
      return ParsePrint();
    case Lexer::ID_IF:
      return ParseIF();
    case Lexer::ID_WHILE:
      return ParseWhile();
    // stub code, will remove later
    case Lexer::ID_NUMBER: {
      ASTNode node = ParseExpr();
      ExpectToken(Lexer::ID_ENDLINE);
      return node;
    }
    default:
      ErrorUnexpected(current);
    }
  }

public:
  MacroCalc(std::ifstream &input) {
    tokens = lexer.Tokenize(input);
    Parse();
  };

  void Parse() {
    while (token_idx < tokens.size()) {
      root.AddChild(ParseStatement());
    }
  }

  void Execute() { root.Run(table); }
};

int main(int argc, char *argv[]) {
  if (argc != 2) {
    ErrorNoLine("Format: ", argv[0], " [filename]");
  }

  std::string filename = argv[1];

  std::ifstream in_file(filename);
  if (in_file.fail()) {
    ErrorNoLine("Unable to open file '", filename, "'.");
  }

  MacroCalc calc{in_file};
  calc.Parse();
  calc.Execute();
}
