#pragma once
#include <string>
#include <vector>

// AST NODE TYPES
enum class NodeType {
    FORMAT,
    FUNC_DEF,
    ASM_BLOCK,
    DATA_BLOCK, 
    DLL_BLOCK,
    FUNC_CALL,
    LET_STMT,

    WHILE_STMT,
    IF_STMT,
    BREAK_STMT,
    INC_STMT,
    
    USELIB,
    INCLUDE_BLOCK,
    VARIADIC_BODY
};

// AST Node

struct ASTNode {
    NodeType type;
    std::string value;  // FORMAT: "PE64 CONSOLE"

    // FUNC DEF:
    std::vector<ASTNode> body;

    // FUNC DEF RARAMS:
    std::vector<std::string> params;

    // ASM_BLOCK:
    std::vector<std::string> asm_lines;

    // dll_block:
    std::vector<std::string> imports;

    // LET_STMT:
    std::string var_name;
    std::string var_type;
    std::string var_value;
    bool is_mut = false;
    bool is_func_local = false;

    // FUNC_CALL:
    std::vector<std::string> args;

    // WHILE_STMT / IF_STMT
    std::string condition;

    // IF_STMT:
    std::vector<ASTNode> else_body;
    
    // VARIADIC:
    bool is_variadic = false;
    std::vector<ASTNode> variadic_body;
};