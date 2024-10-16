#include "ASTNode.hpp"
#include "SymbolTable.hpp"
#include <iostream>

int main() {
  SymbolTable table{};
  // 1 + 2 * 3
  // should print 7
  ASTNode top = ASTNode(ASTNode::OPERATION, "+");
  ASTNode left = ASTNode(ASTNode::NUMBER, 1);
  ASTNode right = ASTNode(ASTNode::OPERATION, "*");
  right.AddChildren(ASTNode(ASTNode::NUMBER, 2), ASTNode(ASTNode::NUMBER, 3));
  top.AddChildren(left, right);
  std::cout << top.RunExpect(table) << std::endl;

  //(1 + 2 * 3) == 7
  // should print 1
  ASTNode top2 = ASTNode(ASTNode::OPERATION, "==");
  ASTNode right2 = ASTNode(ASTNode::NUMBER, 7);
  top2.AddChildren(top, right2);
  std::cout << top2.RunExpect(table) << std::endl;

  // 1 / 0
  // should throw error

  ASTNode right3 = ASTNode(ASTNode::OPERATION, "/");
  right3.AddChildren(ASTNode(ASTNode::NUMBER, 1), ASTNode(ASTNode::NUMBER, 0));
  // std::cout << right3.RunExpect(table) << std::endl;

  //((1 + 2 * 3) == 7) || (1 / 0)
  // should return 1 and short-circuit
  ASTNode top3 = ASTNode(ASTNode::OPERATION, "||");
  top3.AddChildren(top2, right3);
  std::cout << top3.RunExpect(table) << std::endl;

  //!(1 + 2 * 3 == 7)
  // should return 0
  ASTNode top4 = ASTNode(ASTNode::OPERATION, "!");
  top4.AddChild(top2);
  std::cout << top4.RunExpect(table) << std::endl;
}
