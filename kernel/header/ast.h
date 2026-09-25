#pragma once
#include <string>
#include <vector>
#include <memory>

// AST Node Types
enum class NodeType {
    FORMAT,
    FUNC_DEF,
    ASM_BLOCK,
    DATA_BLOCK, 
    DLL_BLOCK,
    FUNC_CALL,
    LET_STMT,
    RETURN_STMT,
    WHILE_STMT,
    IF_STMT,
    BREAK_STMT,
    INC_STMT,
    USELIB,
    INCLUDE_BLOCK,
    VARIADIC_BODY,
    BINARY_OP
};

// AST Node Structure
struct ASTNode {
    NodeType type;
    std::string value;

    // Function definition body and parameters
    std::vector<ASTNode> body;
    std::vector<std::string> params;

    // Inline Assembly and Imports
    std::vector<std::string> asm_lines;
    std::vector<std::string> imports;

    // Variable declarations (LET_STMT / STATIC)
    std::string var_name;
    std::string var_type;
    std::string var_value;
    bool is_mut = false;
    bool is_func_local = false;
    bool is_static = false;

    // Function Calls and Control Flow
    std::vector<std::string> args;
    std::string condition;
    std::vector<ASTNode> else_body;

    // Variadic functions
    bool is_variadic = false;
    std::vector<ASTNode> variadic_body;

    // Function Return Type
    std::string return_type;

    // Expression Nodes (BINARY_OP)
    std::string op;
    std::shared_ptr<ASTNode> left;
    std::shared_ptr<ASTNode> right;
    std::shared_ptr<ASTNode> expr;
};