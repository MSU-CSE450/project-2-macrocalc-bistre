#include <cassert>
#include <fstream>
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
    out.AddChildren(ASTNode(ASTNode::IDENTIFIER, var_id, &ident), expr);

    return out;
  }

  ASTNode ParseAssign() {
    Token new_id = ExpectToken(Lexer::ID_ID);
    ExpectToken(Lexer::ID_ASSIGN);
    ASTNode node = ASTNode{ASTNode::ASSIGN};
    size_t var_id = table.FindVar(new_id.lexeme, new_id.line_id);
    node.AddChildren(ASTNode(ASTNode::IDENTIFIER, var_id, &new_id),
                     ParseExpr());
    ExpectToken(Lexer::ID_ENDLINE);
    return node;
  }

  ASTNode ParseExpr() {
    // stub expression handler for now, only works for literals and idents
    if (auto token = IfToken(Lexer::ID_NUMBER)) {
      return ASTNode(ASTNode::NUMBER, std::stod(token->lexeme));
    }

    if (auto token = IfToken(Lexer::ID_ID)) {
      return ASTNode(ASTNode::IDENTIFIER,
                     table.FindVar(token->lexeme, token->line_id), token);
    }

    ErrorUnexpected(CurToken(), Lexer::ID_ID, Lexer::ID_NUMBER);
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
      node.AddChild(ParseExpr());
    }
    ExpectToken(Lexer::ID_CLOSE_PARENTHESIS);
    ExpectToken(Lexer::ID_ENDLINE);
    return node;
  }

  ASTNode ParseWhile() {
    ExpectToken(Lexer::ID_WHILE);
    ExpectToken(Lexer::ID_OPEN_PARENTHESIS);
    ASTNode node = ASTNode(ASTNode::WHILE);
    // hack to get around dealing with expressions
    // but still be able to do some basic testing
    if (CurToken() == Lexer::ID_ID) {
      Token id = ConsumeToken();
      node.AddChild(ASTNode(ASTNode::IDENTIFIER,
                            table.FindVar(id.lexeme, id.line_id), &id));
    } else {
      node.AddChild(ParseExpr());
    }
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
    case Lexer::ID_ID:
      return ParseAssign();
    case Lexer::ID_PRINT:
      return ParsePrint();
    case Lexer::ID_WHILE:
      return ParseWhile();
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
