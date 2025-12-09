/* 
 * @copyright (c) 2008, Hedspi, Hanoi University of Technology
 * @author Huu-Duc Nguyen
 * @version 1.0
 */
#include <stdio.h>
#include <stdlib.h>

#include "reader.h"
#include "scanner.h"
#include "parser.h"
#include "error.h"
#include "debug.h"

Token *currentToken;
Token *lookAhead;

extern Type* intType;
extern Type* charType;
extern SymTab* symtab;

void scan(void) {
  Token* tmp = currentToken;
  currentToken = lookAhead;
  lookAhead = getValidToken();
  free(tmp);
}

void eat(TokenType tokenType) {
  if (lookAhead->tokenType == tokenType) {
    scan();
  } else missingToken(tokenType, lookAhead->lineNo, lookAhead->colNo);
}

void compileProgram(void) {
  // Create, enter, and exit program block
  Object* programObject;

  eat(KW_PROGRAM);
  eat(TK_IDENT);

  programObject = createProgramObject(currentToken->string);
  
  enterBlock(programObject->progAttrs->scope);

  eat(SB_SEMICOLON);
  compileBlock();
  eat(SB_PERIOD);

  exitBlock();
}

void compileBlock(void) {
  // Create and declare constant objects
  Object* constantObject;
  ConstantValue* value;

  if (lookAhead->tokenType == KW_CONST) {
    eat(KW_CONST);

    do {
      eat(TK_IDENT);
      
      checkFreshIdent(currentToken->string);
      constantObject = createConstantObject(currentToken->string);
      
      eat(SB_EQ);
      value = compileConstant();
      
      constantObject->constAttrs->value = value;
      declareObject(constantObject);
      
      eat(SB_SEMICOLON);
    } while (lookAhead->tokenType == TK_IDENT);

    compileBlock2();
  } 
  else compileBlock2();
}

void compileBlock2(void) {
  // Create and declare type objects
  Object* typeObject;
  Type* typeDefinition;

  if (lookAhead->tokenType == KW_TYPE) {
    eat(KW_TYPE);

    do {
      eat(TK_IDENT);
      
      checkFreshIdent(currentToken->string);
      typeObject = createTypeObject(currentToken->string);
      
      eat(SB_EQ);
      typeDefinition = compileType();
      
      typeObject->typeAttrs->actualType = typeDefinition;
      declareObject(typeObject);
      
      eat(SB_SEMICOLON);
    } while (lookAhead->tokenType == TK_IDENT);

    compileBlock3();
  } 
  else compileBlock3();
}

void compileBlock3(void) {
  // Create and declare variable objects
  Object* variableObject;
  Type* variableType;

  if (lookAhead->tokenType == KW_VAR) {
    eat(KW_VAR);

    do {
      eat(TK_IDENT);
      
      checkFreshIdent(currentToken->string);
      variableObject = createVariableObject(currentToken->string);
      
      eat(SB_COLON);
      variableType = compileType();
      
      variableObject->varAttrs->type = variableType;
      declareObject(variableObject);
      
      eat(SB_SEMICOLON);
    } while (lookAhead->tokenType == TK_IDENT);

    compileBlock4();
  } 
  else compileBlock4();
}

void compileBlock4(void) {
  compileSubDecls();
  compileBlock5();
}

void compileBlock5(void) {
  eat(KW_BEGIN);
  compileStatements();
  eat(KW_END);
}

void compileSubDecls(void) {
  while ((lookAhead->tokenType == KW_FUNCTION) || (lookAhead->tokenType == KW_PROCEDURE)) {
    if (lookAhead->tokenType == KW_FUNCTION)
      compileFuncDecl();
    else compileProcDecl();
  }
}

void compileFuncDecl(void) {
  // Create and declare a function object
  Object* functionObject;
  Type* functionReturnType;

  eat(KW_FUNCTION);
  eat(TK_IDENT);

  checkFreshIdent(currentToken->string);
  functionObject = createFunctionObject(currentToken->string);
  
  declareObject(functionObject);
  enterBlock(functionObject->funcAttrs->scope);
  
  compileParams();
  
  eat(SB_COLON);
  functionReturnType = compileBasicType();
  functionObject->funcAttrs->returnType = functionReturnType;
  
  eat(SB_SEMICOLON);
  compileBlock();
  eat(SB_SEMICOLON);

  exitBlock();
}

void compileProcDecl(void) {
  // Create and declare a procedure object
  Object* procedureObject;

  eat(KW_PROCEDURE);
  eat(TK_IDENT);

  checkFreshIdent(currentToken->string);
  procedureObject = createProcedureObject(currentToken->string);
  
  declareObject(procedureObject);
  enterBlock(procedureObject->procAttrs->scope);
  
  compileParams();
  
  eat(SB_SEMICOLON);
  compileBlock();
  eat(SB_SEMICOLON);

  exitBlock();
}

ConstantValue* compileUnsignedConstant(void) {
  // Create and return an unsigned constant value
  ConstantValue* value;
  Object* object;

  switch (lookAhead->tokenType) {
  case TK_NUMBER:
    eat(TK_NUMBER);
    value = makeIntConstant(currentToken->value);
    break;
  case TK_IDENT:
    eat(TK_IDENT);
    
    object = checkDeclaredConstant(currentToken->string);
    value = duplicateConstantValue(object->constAttrs->value);
    break;
  case TK_CHAR:
    eat(TK_CHAR);
    value = makeCharConstant(currentToken->string[0]);
    break;
  default:
    error(ERR_INVALID_CONSTANT, lookAhead->lineNo, lookAhead->colNo);
    break;
  }
  return value;
}

ConstantValue* compileConstant(void) {
  // Create and return a constant
  ConstantValue* value;

  switch (lookAhead->tokenType) {
  case SB_PLUS:
    eat(SB_PLUS);
    value = compileConstant2();
    break;
  case SB_MINUS:
    eat(SB_MINUS);
    value = compileConstant2();
    
    if (value->type == TP_INT)
      value->intValue = -(value->intValue);
    else
      error(ERR_TYPE_INCONSISTENCY, currentToken->lineNo, currentToken->colNo);
    break;
  case TK_CHAR:
    eat(TK_CHAR);
    value = makeCharConstant(currentToken->string[0]);
    break;
  default:
    value = compileConstant2();
    break;
  }
  return value;
}

ConstantValue* compileConstant2(void) {
  // Create and return a constant value
  ConstantValue* value;
  Object* object;

  switch (lookAhead->tokenType) {
  case TK_NUMBER:
    eat(TK_NUMBER);
    value = makeIntConstant(currentToken->value);
    break;
  case TK_IDENT:
    eat(TK_IDENT);
    
    object = checkDeclaredConstant(currentToken->string);
    value = duplicateConstantValue(object->constAttrs->value);
    break;
  default:
    error(ERR_INVALID_CONSTANT, lookAhead->lineNo, lookAhead->colNo);
    break;
  }
  return value;
}

Type* compileType(void) {
  // Create and return a type
  Type* resultType;
  Type* elemType;
  int size;
  Object* object;

  switch (lookAhead->tokenType) {
  case KW_INTEGER: 
    eat(KW_INTEGER);
    resultType = makeIntType();
    break;
  case KW_CHAR: 
    eat(KW_CHAR);
    resultType = makeCharType();
    break;
  case KW_ARRAY:
    eat(KW_ARRAY);
    eat(SB_LSEL);
    eat(TK_NUMBER);
    
    size = currentToken->value;
    
    eat(SB_RSEL);
    eat(KW_OF);
    elemType = compileType();
    resultType = makeArrayType(size, elemType);
    break;
  case TK_IDENT:
    eat(TK_IDENT);
    
    object = checkDeclaredType(currentToken->string);
    resultType = duplicateType(object->typeAttrs->actualType);
    break;
  default:
    error(ERR_INVALID_TYPE, lookAhead->lineNo, lookAhead->colNo);
    break;
  }
  return resultType;
}

Type* compileBasicType(void) {
  // Create and return a basic type
  Type* basicType;

  switch (lookAhead->tokenType) {
  case KW_INTEGER: 
    eat(KW_INTEGER);
    basicType = makeIntType();
    break;
  case KW_CHAR: 
    eat(KW_CHAR);
    basicType = makeCharType();
    break;
  default:
    error(ERR_INVALID_BASICTYPE, lookAhead->lineNo, lookAhead->colNo);
    break;
  }
  return basicType;
}

void compileParams(void) {
  if (lookAhead->tokenType == SB_LPAR) {
    eat(SB_LPAR);
    compileParam();
    while (lookAhead->tokenType == SB_SEMICOLON) {
      eat(SB_SEMICOLON);
      compileParam();
    }
    eat(SB_RPAR);
  }
}

void compileParam(void) {
  // Create and declare a parameter
  Object* parameterObject;
  Type* parameterType;
  enum ParamKind kind;

  switch (lookAhead->tokenType) {
  case TK_IDENT:
    kind = PARAM_VALUE;
    eat(TK_IDENT);
    
    checkFreshIdent(currentToken->string);
    parameterObject = createParameterObject(currentToken->string, kind, symtab->currentScope->owner);
    
    eat(SB_COLON);
    parameterType = compileBasicType();
    parameterObject->paramAttrs->type = parameterType;
    
    declareObject(parameterObject);
    break;
  case KW_VAR:
    kind = PARAM_REFERENCE;
    eat(KW_VAR);
    eat(TK_IDENT);
    
    checkFreshIdent(currentToken->string);
    parameterObject = createParameterObject(currentToken->string, kind, symtab->currentScope->owner);
    
    eat(SB_COLON);
    parameterType = compileBasicType();
    parameterObject->paramAttrs->type = parameterType;
    
    declareObject(parameterObject);
    break;
  default:
    error(ERR_INVALID_PARAMETER, lookAhead->lineNo, lookAhead->colNo);
    break;
  }
}

void compileStatements(void) {
  compileStatement();
  while (lookAhead->tokenType == SB_SEMICOLON) {
    eat(SB_SEMICOLON);
    compileStatement();
  }
}

void compileStatement(void) {
  switch (lookAhead->tokenType) {
  case TK_IDENT:
    compileAssignSt();
    break;
  case KW_CALL:
    compileCallSt();
    break;
  case KW_BEGIN:
    compileGroupSt();
    break;
  case KW_IF:
    compileIfSt();
    break;
  case KW_WHILE:
    compileWhileSt();
    break;
  case KW_FOR:
    compileForSt();
    break;
    // EmptySt needs to check FOLLOW tokens
  case SB_SEMICOLON:
  case KW_END:
  case KW_ELSE:
    break;
    // Error occurs
  default:
    error(ERR_INVALID_STATEMENT, lookAhead->lineNo, lookAhead->colNo);
    break;
  }
}

void compileLValue(void) {
  Object* var;
  
  eat(TK_IDENT);
  // Check if it's a declared variable or function (for function return value assignment)
  var = checkDeclaredIdent(currentToken->string);
  if (var->kind != OBJ_VARIABLE && var->kind != OBJ_FUNCTION && var->kind != OBJ_PARAMETER)
    error(ERR_INVALID_LVALUE, currentToken->lineNo, currentToken->colNo);
  
  compileIndexes();
}

void compileAssignSt(void) {
  compileLValue();
  eat(SB_ASSIGN);
  compileExpression();
}

void compileCallSt(void) {
  eat(KW_CALL);
  eat(TK_IDENT);
  
  // Check if it's a declared procedure
  checkDeclaredProcedure(currentToken->string);
  
  compileArguments();
}

void compileGroupSt(void) {
  eat(KW_BEGIN);
  compileStatements();
  eat(KW_END);
}

void compileIfSt(void) {
  eat(KW_IF);
  compileCondition();
  eat(KW_THEN);
  compileStatement();
  if (lookAhead->tokenType == KW_ELSE) 
    compileElseSt();
}

void compileElseSt(void) {
  eat(KW_ELSE);
  compileStatement();
}

void compileWhileSt(void) {
  eat(KW_WHILE);
  compileCondition();
  eat(KW_DO);
  compileStatement();
}

void compileForSt(void) {
  eat(KW_FOR);
  eat(TK_IDENT);
  
  // Check if loop variable is declared
  checkDeclaredVariable(currentToken->string);
  
  eat(SB_ASSIGN);
  compileExpression();
  eat(KW_TO);
  compileExpression();
  eat(KW_DO);
  compileStatement();
}

void compileArgument(void) {
  compileExpression();
}

void compileArguments(void) {
  switch (lookAhead->tokenType) {
  case SB_LPAR:
    eat(SB_LPAR);
    compileArgument();

    while (lookAhead->tokenType == SB_COMMA) {
      eat(SB_COMMA);
      compileArgument();
    }

    eat(SB_RPAR);
    break;
    // Check FOLLOW set 
  case SB_TIMES:
  case SB_SLASH:
  case SB_PLUS:
  case SB_MINUS:
  case KW_TO:
  case KW_DO:
  case SB_RPAR:
  case SB_COMMA:
  case SB_EQ:
  case SB_NEQ:
  case SB_LE:
  case SB_LT:
  case SB_GE:
  case SB_GT:
  case SB_RSEL:
  case SB_SEMICOLON:
  case KW_END:
  case KW_ELSE:
  case KW_THEN:
    break;
  default:
    error(ERR_INVALID_ARGUMENTS, lookAhead->lineNo, lookAhead->colNo);
  }
}

void compileCondition(void) {
  compileExpression();
  switch (lookAhead->tokenType) {
  case SB_EQ:
    eat(SB_EQ);
    break;
  case SB_NEQ:
    eat(SB_NEQ);
    break;
  case SB_LE:
    eat(SB_LE);
    break;
  case SB_LT:
    eat(SB_LT);
    break;
  case SB_GE:
    eat(SB_GE);
    break;
  case SB_GT:
    eat(SB_GT);
    break;
  default:
    error(ERR_INVALID_COMPARATOR, lookAhead->lineNo, lookAhead->colNo);
  }

  compileExpression();
}

void compileExpression(void) {
  switch (lookAhead->tokenType) {
  case SB_PLUS:
    eat(SB_PLUS);
    compileExpression2();
    break;
  case SB_MINUS:
    eat(SB_MINUS);
    compileExpression2();
    break;
  default:
    compileExpression2();
  }
}

void compileExpression2(void) {
  compileTerm();
  compileExpression3();
}


void compileExpression3(void) {
  switch (lookAhead->tokenType) {
  case SB_PLUS:
    eat(SB_PLUS);
    compileTerm();
    compileExpression3();
    break;
  case SB_MINUS:
    eat(SB_MINUS);
    compileTerm();
    compileExpression3();
    break;
    // check the FOLLOW set
  case KW_TO:
  case KW_DO:
  case SB_RPAR:
  case SB_COMMA:
  case SB_EQ:
  case SB_NEQ:
  case SB_LE:
  case SB_LT:
  case SB_GE:
  case SB_GT:
  case SB_RSEL:
  case SB_SEMICOLON:
  case KW_END:
  case KW_ELSE:
  case KW_THEN:
    break;
  default:
    error(ERR_INVALID_EXPRESSION, lookAhead->lineNo, lookAhead->colNo);
  }
}

void compileTerm(void) {
  compileFactor();
  compileTerm2();
}

void compileTerm2(void) {
  switch (lookAhead->tokenType) {
  case SB_TIMES:
    eat(SB_TIMES);
    compileFactor();
    compileTerm2();
    break;
  case SB_SLASH:
    eat(SB_SLASH);
    compileFactor();
    compileTerm2();
    break;
    // check the FOLLOW set
  case SB_PLUS:
  case SB_MINUS:
  case KW_TO:
  case KW_DO:
  case SB_RPAR:
  case SB_COMMA:
  case SB_EQ:
  case SB_NEQ:
  case SB_LE:
  case SB_LT:
  case SB_GE:
  case SB_GT:
  case SB_RSEL:
  case SB_SEMICOLON:
  case KW_END:
  case KW_ELSE:
  case KW_THEN:
    break;
  default:
    error(ERR_INVALID_TERM, lookAhead->lineNo, lookAhead->colNo);
  }
}

void compileFactor(void) {
  Object* obj;
  
  switch (lookAhead->tokenType) {
  case TK_NUMBER:
    eat(TK_NUMBER);
    break;
  case TK_CHAR:
    eat(TK_CHAR);
    break;
  case TK_IDENT:
    eat(TK_IDENT);
    
    switch (lookAhead->tokenType) {
    case SB_LPAR:
      // Must be a function - call specific check
      obj = checkDeclaredFunction(currentToken->string);
      compileArguments();
      break;
    case SB_LSEL:
      // Must be a variable (array) - can be variable or parameter
      obj = checkDeclaredIdent(currentToken->string);
      if (obj->kind != OBJ_VARIABLE && obj->kind != OBJ_PARAMETER)
        error(ERR_INVALID_VARIABLE, currentToken->lineNo, currentToken->colNo);
      compileIndexes();
      break;
    default:
      // Can be variable, parameter, constant, or function (for return value)
      obj = checkDeclaredIdent(currentToken->string);
      if (obj->kind != OBJ_VARIABLE && obj->kind != OBJ_PARAMETER && 
          obj->kind != OBJ_CONSTANT && obj->kind != OBJ_FUNCTION)
        error(ERR_INVALID_FACTOR, currentToken->lineNo, currentToken->colNo);
      break;
    }
    break;
  default:
    error(ERR_INVALID_FACTOR, lookAhead->lineNo, lookAhead->colNo);
  }
}

void compileIndexes(void) {
  while (lookAhead->tokenType == SB_LSEL) {
    eat(SB_LSEL);
    compileExpression();
    eat(SB_RSEL);
  }
}

int compile(char *fileName) {
  if (openInputStream(fileName) == IO_ERROR)
    return IO_ERROR;

  currentToken = NULL;
  lookAhead = getValidToken();

  initSymTab();

  compileProgram();

  printObject(symtab->program,0);

  cleanSymTab();

  free(currentToken);
  free(lookAhead);
  closeInputStream();
  return IO_SUCCESS;

}

/******************** Semantic checking functions ***********************/

void checkFreshIdent(char *name) {
  Object* obj = findObject(symtab->currentScope->objList, name);
  if (obj != NULL)
    error(ERR_DUPLICATE_IDENT, currentToken->lineNo, currentToken->colNo);
}

Object* checkDeclaredIdent(char *name) {
  Object* obj = lookupObject(name);
  if (obj == NULL)
    error(ERR_UNDECLARED_IDENT, currentToken->lineNo, currentToken->colNo);
  return obj;
}

Object* checkDeclaredConstant(char *name) {
  Object* obj = lookupObject(name);
  if (obj == NULL)
    error(ERR_UNDECLARED_IDENT, currentToken->lineNo, currentToken->colNo);
  if (obj->kind != OBJ_CONSTANT)
    error(ERR_INVALID_CONSTANT, currentToken->lineNo, currentToken->colNo);
  return obj;
}

Object* checkDeclaredType(char *name) {
  Object* obj = lookupObject(name);
  if (obj == NULL)
    error(ERR_UNDECLARED_IDENT, currentToken->lineNo, currentToken->colNo);
  if (obj->kind != OBJ_TYPE)
    error(ERR_INVALID_TYPE, currentToken->lineNo, currentToken->colNo);
  return obj;
}

Object* checkDeclaredVariable(char *name) {
  Object* obj = lookupObject(name);
  if (obj == NULL)
    error(ERR_UNDECLARED_IDENT, currentToken->lineNo, currentToken->colNo);
  if (obj->kind != OBJ_VARIABLE)
    error(ERR_INVALID_VARIABLE, currentToken->lineNo, currentToken->colNo);
  return obj;
}

Object* checkDeclaredFunction(char *name) {
  Object* obj = lookupObject(name);
  if (obj == NULL)
    error(ERR_UNDECLARED_IDENT, currentToken->lineNo, currentToken->colNo);
  if (obj->kind != OBJ_FUNCTION)
    error(ERR_INVALID_FUNCTION, currentToken->lineNo, currentToken->colNo);
  return obj;
}

Object* checkDeclaredProcedure(char *name) {
  Object* obj = lookupObject(name);
  if (obj == NULL)
    error(ERR_UNDECLARED_IDENT, currentToken->lineNo, currentToken->colNo);
  if (obj->kind != OBJ_PROCEDURE)
    error(ERR_INVALID_PROCEDURE, currentToken->lineNo, currentToken->colNo);
  return obj;
}

