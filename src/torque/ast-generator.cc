// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "src/base/template-utils.h"
#include "src/torque/ast-generator.h"

namespace v8 {
namespace internal {
namespace torque {

namespace {

std::string GetOptionalType(TorqueParser::OptionalTypeContext* context) {
  if (!context->type()) return "void";
  return context->type()->IDENTIFIER()->getSymbol()->getText();
}

LabelAndTypesVector GetOptionalLabelAndTypeList(
    TorqueParser::OptionalLabelListContext* context) {
  LabelAndTypesVector labels;
  for (auto label : context->labelParameter()) {
    LabelAndTypes new_label;
    new_label.name = label->IDENTIFIER()->getSymbol()->getText();
    if (label->typeList() != nullptr) {
      for (auto& type : label->typeList()->type()) {
        new_label.types.push_back(type->IDENTIFIER()->getSymbol()->getText());
      }
    }
    labels.push_back(new_label);
  }
  return labels;
}

std::string StringLiteralUnquote(const std::string& s) {
  assert('"' == s.front() || '\'' == s.front());
  assert('"' == s.back() || '\'' == s.back());
  std::stringstream result;
  for (size_t i = 1; i < s.length() - 1; ++i) {
    if (s[i] == '\\') {
      switch (s[++i]) {
        case 'n':
          result << '\n';
          break;
        case 'r':
          result << '\r';
          break;
        case 't':
          result << '\t';
          break;
        case '\'':
        case '"':
        case '\\':
          result << s[i];
          break;
        default:
          UNREACHABLE();
      }
    } else {
      result << s[i];
    }
  }
  return result.str();
}

}  // namespace

antlrcpp::Any AstGenerator::visitParameterList(
    TorqueParser::ParameterListContext* context) {
  ParameterList result{{}, {}, context->VARARGS(), {}};
  if (context->VARARGS()) {
    result.arguments_variable = context->IDENTIFIER()->getSymbol()->getText();
  }
  for (auto* parameter : context->parameter()) {
    parameter->accept(this);
    result.names.push_back(parameter->IDENTIFIER()->getSymbol()->getText());
    result.types.push_back(
        parameter->type()->IDENTIFIER()->getSymbol()->getText());
  }
  return std::move(result);
}

antlrcpp::Any AstGenerator::visitTypeList(
    TorqueParser::TypeListContext* context) {
  ParameterList result{{}, {}, false, {}};
  result.types.reserve(context->type().size());
  for (auto* type : context->type()) {
    result.types.push_back(type->IDENTIFIER()->getSymbol()->getText());
  }
  return std::move(result);
}

antlrcpp::Any AstGenerator::visitTypeListMaybeVarArgs(
    TorqueParser::TypeListMaybeVarArgsContext* context) {
  ParameterList result{{}, {}, context->VARARGS(), {}};
  result.types.reserve(context->type().size());
  for (auto* type : context->type()) {
    result.types.push_back(type->IDENTIFIER()->getSymbol()->getText());
  }
  return std::move(result);
}

antlrcpp::Any AstGenerator::visitModuleDeclaration(
    TorqueParser::ModuleDeclarationContext* context) {
  ModuleDeclaration* result = new ExplicitModuleDeclaration{
      Pos(context), context->IDENTIFIER()->getSymbol()->getText(), {}};
  for (auto* declaration : context->declaration()) {
    result->declarations.push_back(
        declaration->accept(this).as<Declaration*>());
  }
  return base::implicit_cast<Declaration*>(result);
}

antlrcpp::Any AstGenerator::visitMacroDeclaration(
    TorqueParser::MacroDeclarationContext* context) {
  return base::implicit_cast<Declaration*>(new MacroDeclaration{
      Pos(context), context->IDENTIFIER()->getSymbol()->getText(),
      std::move(context->parameterList()->accept(this).as<ParameterList>()),
      GetOptionalType(context->optionalType()),
      GetOptionalLabelAndTypeList(context->optionalLabelList()),
      context->helperBody()->accept(this).as<Statement*>()});
}

antlrcpp::Any AstGenerator::visitBuiltinDeclaration(
    TorqueParser::BuiltinDeclarationContext* context) {
  return base::implicit_cast<Declaration*>(new BuiltinDeclaration{
      Pos(context), context->JAVASCRIPT() != nullptr,
      context->IDENTIFIER()->getSymbol()->getText(),
      std::move(context->parameterList()->accept(this).as<ParameterList>()),
      GetOptionalType(context->optionalType()),
      context->helperBody()->accept(this).as<Statement*>()});
}

antlrcpp::Any AstGenerator::visitExternalMacro(
    TorqueParser::ExternalMacroContext* context) {
  ExternalMacroDeclaration* result = new ExternalMacroDeclaration{
      Pos(context),
      context->IDENTIFIER()->getSymbol()->getText(),
      context->IMPLICIT() != nullptr,
      {},
      std::move(
          context->typeListMaybeVarArgs()->accept(this).as<ParameterList>()),
      GetOptionalType(context->optionalType()),
      GetOptionalLabelAndTypeList(context->optionalLabelList())};
  if (auto* op = context->STRING_LITERAL())
    result->op = StringLiteralUnquote(op->getSymbol()->getText());
  return base::implicit_cast<Declaration*>(result);
}

antlrcpp::Any AstGenerator::visitExternalBuiltin(
    TorqueParser::ExternalBuiltinContext* context) {
  return base::implicit_cast<Declaration*>(new ExternalBuiltinDeclaration{
      Pos(context), context->JAVASCRIPT() != nullptr,
      context->IDENTIFIER()->getSymbol()->getText(),
      std::move(context->typeList()->accept(this).as<ParameterList>()),
      GetOptionalType(context->optionalType())});
}

antlrcpp::Any AstGenerator::visitExternalRuntime(
    TorqueParser::ExternalRuntimeContext* context) {
  return base::implicit_cast<Declaration*>(new ExternalRuntimeDeclaration{
      Pos(context), context->IDENTIFIER()->getSymbol()->getText(),
      std::move(
          context->typeListMaybeVarArgs()->accept(this).as<ParameterList>()),
      GetOptionalType(context->optionalType())});
}

antlrcpp::Any AstGenerator::visitConstDeclaration(
    TorqueParser::ConstDeclarationContext* context) {
  return base::implicit_cast<Declaration*>(new ConstDeclaration{
      Pos(context), context->IDENTIFIER()->getSymbol()->getText(),
      context->type()->IDENTIFIER()->getSymbol()->getText(),
      StringLiteralUnquote(context->STRING_LITERAL()->getSymbol()->getText())});
}

antlrcpp::Any AstGenerator::visitTypeDeclaration(
    TorqueParser::TypeDeclarationContext* context) {
  TypeDeclaration* result = new TypeDeclaration{
      Pos(context), context->IDENTIFIER()->getSymbol()->getText(), {}, {}};
  if (context->extendsDeclaration())
    result->extends =
        context->extendsDeclaration()->IDENTIFIER()->getSymbol()->getText();
  if (context->generatesDeclaration()) {
    result->generates = StringLiteralUnquote(context->generatesDeclaration()
                                                 ->STRING_LITERAL()
                                                 ->getSymbol()
                                                 ->getText());
  }
  return base::implicit_cast<Declaration*>(result);
}

antlrcpp::Any AstGenerator::visitVariableDeclaration(
    TorqueParser::VariableDeclarationContext* context) {
  return new VarDeclarationStatement{
      Pos(context),
      context->IDENTIFIER()->getSymbol()->getText(),
      context->type()->IDENTIFIER()->getSymbol()->getText(),
      {}};
}

antlrcpp::Any AstGenerator::visitVariableDeclarationWithInitialization(
    TorqueParser::VariableDeclarationWithInitializationContext* context) {
  VarDeclarationStatement* result =
      VarDeclarationStatement::cast(context->variableDeclaration()
                                        ->accept(this)
                                        .as<VarDeclarationStatement*>());
  result->pos = Pos(context);
  if (context->expression())
    result->initializer = context->expression()->accept(this).as<Expression*>();
  return base::implicit_cast<Statement*>(result);
}

antlrcpp::Any AstGenerator::visitHelperCall(
    TorqueParser::HelperCallContext* context) {
  antlr4::tree::TerminalNode* callee;
  bool is_operator = context->MIN() || context->MAX();
  if (context->MIN()) callee = context->MIN();
  if (context->MAX()) callee = context->MAX();
  if (context->IDENTIFIER()) callee = context->IDENTIFIER();
  std::vector<std::string> labels;
  for (auto label : context->optionalOtherwise()->IDENTIFIER()) {
    labels.push_back(label->getSymbol()->getText());
  }
  CallExpression* result = new CallExpression{
      Pos(context), callee->getSymbol()->getText(), is_operator, {}, labels};
  for (auto* arg : context->argumentList()->argument()) {
    result->arguments.push_back(arg->accept(this).as<Expression*>());
  }
  return base::implicit_cast<Expression*>(result);
}

antlrcpp::Any AstGenerator::visitHelperCallStatement(
    TorqueParser::HelperCallStatementContext* context) {
  Statement* result;
  if (context->TAIL()) {
    result = new TailCallStatement{
        Pos(context),
        CallExpression::cast(
            context->helperCall()->accept(this).as<Expression*>())};
  } else {
    result = new ExpressionStatement{
        Pos(context), context->helperCall()->accept(this).as<Expression*>()};
  }
  return result;
}

antlrcpp::Any AstGenerator::visitStatementScope(
    TorqueParser::StatementScopeContext* context) {
  BlockStatement* result =
      new BlockStatement{Pos(context), context->DEFERRED() != nullptr, {}};
  for (auto* child : context->statementList()->statement()) {
    result->statements.push_back(child->accept(this).as<Statement*>());
  }
  return base::implicit_cast<Statement*>(result);
}

antlrcpp::Any AstGenerator::visitExpressionStatement(
    TorqueParser::ExpressionStatementContext* context) {
  return base::implicit_cast<Statement*>(new ExpressionStatement{
      Pos(context), context->assignment()->accept(this).as<Expression*>()});
}

antlrcpp::Any AstGenerator::visitReturnStatement(
    TorqueParser::ReturnStatementContext* context) {
  return base::implicit_cast<Statement*>(new ReturnStatement{
      Pos(context), context->expression()->accept(this).as<Expression*>()});
}

antlrcpp::Any AstGenerator::visitBreakStatement(
    TorqueParser::BreakStatementContext* context) {
  return base::implicit_cast<Statement*>(new BreakStatement{Pos(context)});
}

antlrcpp::Any AstGenerator::visitContinueStatement(
    TorqueParser::ContinueStatementContext* context) {
  return base::implicit_cast<Statement*>(new ContinueStatement{Pos(context)});
}

antlrcpp::Any AstGenerator::visitGotoStatement(
    TorqueParser::GotoStatementContext* context) {
  GotoStatement* result = new GotoStatement{Pos(context), {}, {}};
  if (context->labelReference())
    result->label =
        context->labelReference()->IDENTIFIER()->getSymbol()->getText();
  if (context->argumentList() != nullptr) {
    for (auto a : context->argumentList()->argument()) {
      result->arguments.push_back(a->accept(this).as<Expression*>());
    }
  }
  return base::implicit_cast<Statement*>(result);
}

antlrcpp::Any AstGenerator::visitIfStatement(
    TorqueParser::IfStatementContext* context) {
  IfStatement* result = new IfStatement{
      Pos(context),
      std::move(context->expression()->accept(this).as<Expression*>()),
      std::move(context->statementBlock(0)->accept(this).as<Statement*>()),
      {}};
  if (context->statementBlock(1))
    result->if_false =
        std::move(context->statementBlock(1)->accept(this).as<Statement*>());
  return base::implicit_cast<Statement*>(result);
}

antlrcpp::Any AstGenerator::visitWhileLoop(
    TorqueParser::WhileLoopContext* context) {
  return base::implicit_cast<Statement*>(new WhileStatement{
      Pos(context), context->expression()->accept(this).as<Expression*>(),
      context->statementBlock()->accept(this).as<Statement*>()});
}

antlrcpp::Any AstGenerator::visitForLoop(
    TorqueParser::ForLoopContext* context) {
  ForLoopStatement* result = new ForLoopStatement{
      Pos(context),
      {},
      context->expression()->accept(this).as<Expression*>(),
      context->assignment()->accept(this).as<Expression*>(),
      context->statementBlock()->accept(this).as<Statement*>()};
  if (auto* init = context->forInitialization()
                       ->variableDeclarationWithInitialization()) {
    result->var_declaration =
        VarDeclarationStatement::cast(init->accept(this).as<Statement*>());
  }
  return base::implicit_cast<Statement*>(result);
}

antlrcpp::Any AstGenerator::visitForOfLoop(
    TorqueParser::ForOfLoopContext* context) {
  ForOfLoopStatement* result = new ForOfLoopStatement{
      Pos(context),
      context->variableDeclaration()
          ->accept(this)
          .as<VarDeclarationStatement*>(),
      context->expression()->accept(this).as<Expression*>(),
      {},
      {},
      context->statementBlock()->accept(this).as<Statement*>()};
  if (auto* range = context->forOfRange()->rangeSpecifier()) {
    if (auto* begin = range->begin) {
      result->begin = begin->accept(this).as<Expression*>();
    }
    if (auto* end = range->end) {
      result->end = end->accept(this).as<Expression*>();
    }
  }
  return base::implicit_cast<Statement*>(result);
}

antlrcpp::Any AstGenerator::visitTryCatch(
    TorqueParser::TryCatchContext* context) {
  TryCatchStatement* result = new TryCatchStatement{
      Pos(context),
      context->statementBlock()->accept(this).as<Statement*>(),
      {}};
  for (auto* handler : context->handlerWithStatement()) {
    if (handler->CATCH() != nullptr) {
      CatchBlock* catch_block = new CatchBlock{
          Pos(handler->statementBlock()),
          {},
          handler->statementBlock()->accept(this).as<Statement*>()};
      catch_block->caught = handler->IDENTIFIER()->getSymbol()->getText();
      result->catch_blocks.push_back(catch_block);
    } else {
      handler->labelDeclaration()->accept(this);
      auto parameter_list = handler->labelDeclaration()->parameterList();
      ParameterList label_parameters = parameter_list == nullptr
                                           ? ParameterList()
                                           : handler->labelDeclaration()
                                                 ->parameterList()
                                                 ->accept(this)
                                                 .as<ParameterList>();
      LabelBlock* label_block = new LabelBlock{
          Pos(handler->statementBlock()),
          handler->labelDeclaration()->IDENTIFIER()->getSymbol()->getText(),
          label_parameters,
          handler->statementBlock()->accept(this).as<Statement*>()};
      result->label_blocks.push_back(label_block);
    }
  }
  return base::implicit_cast<Statement*>(result);
}

antlrcpp::Any AstGenerator::visitPrimaryExpression(
    TorqueParser::PrimaryExpressionContext* context) {
  if (auto* e = context->helperCall()) return e->accept(this);
  if (auto* e = context->DECIMAL_LITERAL())
    return base::implicit_cast<Expression*>(
        new NumberLiteralExpression{Pos(context), e->getSymbol()->getText()});
  if (auto* e = context->STRING_LITERAL())
    return base::implicit_cast<Expression*>(
        new StringLiteralExpression{Pos(context), e->getSymbol()->getText()});
  if (context->CONVERT_KEYWORD())
    return base::implicit_cast<Expression*>(new ConvertExpression{
        Pos(context), context->type()->IDENTIFIER()->getSymbol()->getText(),
        context->expression()->accept(this).as<Expression*>()});
  if (context->CAST_KEYWORD())
    return base::implicit_cast<Expression*>(new CastExpression{
        Pos(context), context->type()->IDENTIFIER()->getSymbol()->getText(),
        context->IDENTIFIER()->getSymbol()->getText(),
        context->expression()->accept(this).as<Expression*>()});
  return context->expression()->accept(this);
}

antlrcpp::Any AstGenerator::visitAssignment(
    TorqueParser::AssignmentContext* context) {
  if (auto* e = context->incrementDecrement()) return e->accept(this);
  LocationExpression* location = LocationExpression::cast(
      context->locationExpression()->accept(this).as<Expression*>());
  if (auto* e = context->expression()) {
    AssignmentExpression* result = new AssignmentExpression{
        Pos(context), location, {}, e->accept(this).as<Expression*>()};
    if (auto* op_node = context->ASSIGNMENT_OPERATOR()) {
      std::string op = op_node->getSymbol()->getText();
      result->op = op.substr(0, op.length() - 1);
    }
    return base::implicit_cast<Expression*>(result);
  }
  return base::implicit_cast<Expression*>(location);
}

antlrcpp::Any AstGenerator::visitIncrementDecrement(
    TorqueParser::IncrementDecrementContext* context) {
  bool postfix = context->op;
  return base::implicit_cast<Expression*>(new IncrementDecrementExpression{
      Pos(context),
      LocationExpression::cast(
          context->locationExpression()->accept(this).as<Expression*>()),
      context->INCREMENT() ? IncrementDecrementOperator::kIncrement
                           : IncrementDecrementOperator::kDecrement,
      postfix});
}

antlrcpp::Any AstGenerator::visitLocationExpression(
    TorqueParser::LocationExpressionContext* context) {
  if (auto* l = context->locationExpression()) {
    Expression* location = l->accept(this).as<Expression*>();
    if (auto* e = context->expression()) {
      return base::implicit_cast<Expression*>(new ElementAccessExpression{
          Pos(context), location, e->accept(this).as<Expression*>()});
    }
    return base::implicit_cast<Expression*>(new FieldAccessExpression{
        Pos(context), location, context->IDENTIFIER()->getSymbol()->getText()});
  }
  return base::implicit_cast<Expression*>(new IdentifierExpression{
      Pos(context), context->IDENTIFIER()->getSymbol()->getText()});
}

antlrcpp::Any AstGenerator::visitUnaryExpression(
    TorqueParser::UnaryExpressionContext* context) {
  if (auto* e = context->assignmentExpression()) return e->accept(this);
  std::vector<Expression*> args;
  args.push_back(context->unaryExpression()->accept(this).as<Expression*>());
  return base::implicit_cast<Expression*>(new CallExpression{
      Pos(context), context->op->getText(), true, std::move(args), {}});
}

antlrcpp::Any AstGenerator::visitMultiplicativeExpression(
    TorqueParser::MultiplicativeExpressionContext* context) {
  auto* right = context->unaryExpression();
  if (auto* left = context->multiplicativeExpression()) {
    return base::implicit_cast<Expression*>(
        new CallExpression{Pos(context),
                           context->op->getText(),
                           true,
                           {left->accept(this).as<Expression*>(),
                            right->accept(this).as<Expression*>()},
                           {}});
  }
  return right->accept(this);
}

antlrcpp::Any AstGenerator::visitAdditiveExpression(
    TorqueParser::AdditiveExpressionContext* context) {
  auto* right = context->multiplicativeExpression();
  if (auto* left = context->additiveExpression()) {
    return base::implicit_cast<Expression*>(
        new CallExpression{Pos(context),
                           context->op->getText(),
                           true,
                           {left->accept(this).as<Expression*>(),
                            right->accept(this).as<Expression*>()},
                           {}});
  }
  return right->accept(this);
}

antlrcpp::Any AstGenerator::visitShiftExpression(
    TorqueParser::ShiftExpressionContext* context) {
  auto* right = context->additiveExpression();
  if (auto* left = context->shiftExpression()) {
    return base::implicit_cast<Expression*>(
        new CallExpression{Pos(context),
                           context->op->getText(),
                           true,
                           {left->accept(this).as<Expression*>(),
                            right->accept(this).as<Expression*>()},
                           {}});
  }
  return right->accept(this);
}

antlrcpp::Any AstGenerator::visitRelationalExpression(
    TorqueParser::RelationalExpressionContext* context) {
  auto* right = context->shiftExpression();
  if (auto* left = context->relationalExpression()) {
    return base::implicit_cast<Expression*>(
        new CallExpression{Pos(context),
                           context->op->getText(),
                           true,
                           {left->accept(this).as<Expression*>(),
                            right->accept(this).as<Expression*>()},
                           {}});
  }
  return right->accept(this);
}

antlrcpp::Any AstGenerator::visitEqualityExpression(
    TorqueParser::EqualityExpressionContext* context) {
  auto* right = context->relationalExpression();
  if (auto* left = context->equalityExpression()) {
    return base::implicit_cast<Expression*>(
        new CallExpression{Pos(context),
                           context->op->getText(),
                           true,
                           {left->accept(this).as<Expression*>(),
                            right->accept(this).as<Expression*>()},
                           {}});
  }
  return right->accept(this);
}

antlrcpp::Any AstGenerator::visitBitwiseExpression(
    TorqueParser::BitwiseExpressionContext* context) {
  auto* right = context->equalityExpression();
  if (auto* left = context->bitwiseExpression()) {
    return base::implicit_cast<Expression*>(
        new CallExpression{Pos(context),
                           context->op->getText(),
                           true,
                           {left->accept(this).as<Expression*>(),
                            right->accept(this).as<Expression*>()},
                           {}});
  }
  return right->accept(this);
}

antlrcpp::Any AstGenerator::visitLogicalANDExpression(
    TorqueParser::LogicalANDExpressionContext* context) {
  auto* right = context->bitwiseExpression();
  if (auto* left = context->logicalANDExpression()) {
    return base::implicit_cast<Expression*>(new LogicalAndExpression{
        Pos(context), left->accept(this).as<Expression*>(),
        right->accept(this).as<Expression*>()});
  }
  return right->accept(this);
}

antlrcpp::Any AstGenerator::visitLogicalORExpression(
    TorqueParser::LogicalORExpressionContext* context) {
  auto* right = context->logicalANDExpression();
  if (auto* left = context->logicalORExpression()) {
    return base::implicit_cast<Expression*>(new LogicalOrExpression{
        Pos(context), left->accept(this).as<Expression*>(),
        right->accept(this).as<Expression*>()});
  }
  return right->accept(this);
}

antlrcpp::Any AstGenerator::visitConditionalExpression(
    TorqueParser::ConditionalExpressionContext* context) {
  if (auto* condition = context->conditionalExpression()) {
    return base::implicit_cast<Expression*>(new ConditionalExpression{
        Pos(context), condition->accept(this).as<Expression*>(),

        context->logicalORExpression(0)->accept(this).as<Expression*>(),

        context->logicalORExpression(1)->accept(this).as<Expression*>()});
  }
  return context->logicalORExpression(0)->accept(this);
}

void AstGenerator::visitSourceFile(SourceFileContext* context) {
  source_file_context_ = context;
  current_source_file_ = ast_.AddSource(context->name);
  for (auto* declaration : context->file->children) {
    ast_.declarations().push_back(declaration->accept(this).as<Declaration*>());
  }
  source_file_context_ = nullptr;
}

SourcePosition AstGenerator::Pos(antlr4::ParserRuleContext* context) {
  antlr4::misc::Interval i = context->getSourceInterval();
  auto token = source_file_context_->tokens->get(i.a);
  int line = static_cast<int>(token->getLine());
  int column = static_cast<int>(token->getCharPositionInLine());
  return SourcePosition{current_source_file_, line, column};
}

}  // namespace torque
}  // namespace internal
}  // namespace v8
