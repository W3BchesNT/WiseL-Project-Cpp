#include "header/parser.h"
#include <iostream>
#include <memory>

using namespace std;

// Parse conditional expression into BINARY_OP node
static shared_ptr<ASTNode> parse_condition_expr(const vector<Token>& expr_tokens) {
    int op_idx = -1;
    for (size_t i = 0; i < expr_tokens.size(); i++) {
        string v = expr_tokens[i].value;
        if (v == "==" || v == "!=" || v == "<=" || v == ">=" || v == "<" || v == ">") {
            op_idx = i;
            break;
        }
    }
    if (op_idx == -1) return nullptr;

    auto bin_node = make_shared<ASTNode>();
    bin_node->type = NodeType::BINARY_OP;
    bin_node->op = expr_tokens[op_idx].value;

    string left_val;
    for (int i = 0; i < op_idx; i++) {
        if (!left_val.empty()) left_val += " ";
        left_val += expr_tokens[i].value;
    }
    bin_node->left = make_shared<ASTNode>();
    bin_node->left->value = left_val;

    string right_val;
    for (size_t i = op_idx + 1; i < (int)expr_tokens.size(); i++) {
        if (!right_val.empty()) right_val += " ";
        right_val += expr_tokens[i].value;
    }
    bin_node->right = make_shared<ASTNode>();
    bin_node->right->value = right_val;

    return bin_node;
}


// Check if character requires leading whitespace in reconstructed output
bool needs_space_before(const string& val) {
    if (val.empty()) return false;
    char c = val[0];
    return c != ',' && c != ']' && c != ')' && c != ':';
}

// Check if character requires trailing whitespace in reconstructed output
bool needs_space_after(const string& val) {
    if (val.empty()) return false;
    char c = val.back();
    return c != '[' && c != '(';
}

// Parse inline assembly block asm { ... }
ASTNode parse_asm_block(const vector<Token>& tokens, size_t& pos) {
    ASTNode node;
    node.type = NodeType::ASM_BLOCK;

    pos++; // Skip 'asm' keyword

    if (pos < tokens.size() && tokens[pos].type == TokenType::LBRACE) {
        pos++;
        string current_line;
        int brace_depth = 0;

        while (pos < tokens.size()) {
            if (tokens[pos].type == TokenType::RBRACE && brace_depth == 0) {
                break;
            }

            if (tokens[pos].type == TokenType::LBRACE) {
                brace_depth++;
                if (!current_line.empty()) current_line += " ";
                current_line += "{";
                pos++;
                continue;
            }

            if (tokens[pos].type == TokenType::RBRACE && brace_depth > 0) {
                brace_depth--;
                current_line += "}";
                pos++;
                continue;
            }

            if (tokens[pos].type == TokenType::NEWLINE) {
                if (!current_line.empty()) {
                    node.asm_lines.push_back(current_line);
                    current_line.clear();
                }
                pos++;
                continue;
            }

            if (!tokens[pos].value.empty()) {
                if (!current_line.empty() && needs_space_before(tokens[pos].value) && needs_space_after(current_line)) {
                    current_line += " ";
                }
                current_line += tokens[pos].value;
            }
            pos++;
        }

        if (!current_line.empty()) {
            node.asm_lines.push_back(current_line);
        }

        if (pos < tokens.size() && tokens[pos].type == TokenType::RBRACE) {
            pos++;
        }
    }
    return node;
}

// Parse variable declarations (let / static)
ASTNode parse_let_stmt(const vector<Token>& tokens, size_t& pos, bool is_static, bool is_func_local) {
    ASTNode node;
    node.type = NodeType::LET_STMT;
    node.is_static = is_static;
    node.is_func_local = is_func_local;
    pos++;

    if (pos < tokens.size() && tokens[pos].type == TokenType::MUT) {
        node.is_mut = true;
        pos++;
    }
    if (pos < tokens.size() && tokens[pos].type == TokenType::IDENT) {
        node.var_name = tokens[pos].value;
        pos++;
    }
    if (pos < tokens.size() && tokens[pos].type == TokenType::COLON) {
        pos++;
    }
    if (pos < tokens.size() && tokens[pos].type == TokenType::IDENT) {
        node.var_type = tokens[pos].value;
        pos++;
    }
    // Parse pointer type definitions (*u8, *i32, etc.)
    if (node.var_type == "*" && pos < tokens.size() && tokens[pos].type == TokenType::IDENT) {
        node.var_type += tokens[pos].value;
        pos++;
    }

    if (pos < tokens.size() && tokens[pos].type == TokenType::ASSIGN) {
        pos++;
        vector<Token> expr_tokens;
        while (pos < tokens.size() && tokens[pos].type != TokenType::NEWLINE) {
            expr_tokens.push_back(tokens[pos]);
            pos++;
        }

        // Search for binary arithmetic operators (+, -, *, /, %)
        int op_idx = -1;
        for (size_t i = 0; i < expr_tokens.size(); i++) {
            string v = expr_tokens[i].value;
            if (v == "+" || v == "-" || v == "*" || v == "/" || v == "%") {
                // Ignore leading unary minus (e.g. -5)
                if (v == "-" && i == 0) continue; 
                op_idx = i;
                break;
            }
        }

        // Construct BINARY_OP node if operator is found
        if (op_idx != -1) {
            auto bin_node = make_shared<ASTNode>();
            bin_node->type = NodeType::BINARY_OP;
            bin_node->op = expr_tokens[op_idx].value;

            // Assemble left-hand side operand
            string left_val;
            for (int i = 0; i < op_idx; i++) {
                if (!left_val.empty()) left_val += " ";
                left_val += expr_tokens[i].value;
            }
            bin_node->left = make_shared<ASTNode>();
            bin_node->left->value = left_val;

            // Assemble right-hand side operand
            string right_val;
            for (size_t i = op_idx + 1; i < expr_tokens.size(); i++) {
                if (!right_val.empty()) right_val += " ";
                right_val += expr_tokens[i].value;
            }
            bin_node->right = make_shared<ASTNode>();
            bin_node->right->value = right_val;

            node.expr = bin_node; 
        } else {
            // Standard scalar or literal assignment
            string val;
            for (const auto& t : expr_tokens) {
                if (!val.empty()) val += " ";
                val += t.value;
            }
            node.var_value = val;
        }
    }
    return node;
}

// Parse function/block bodies and control flow statements
ASTNode parse_block(const vector<Token>& tokens, size_t& pos) {
    ASTNode block;
    block.type = NodeType::ASM_BLOCK;

    if (pos < tokens.size() && tokens[pos].type == TokenType::LBRACE) {
        pos++;

        while (pos < tokens.size() && tokens[pos].type != TokenType::RBRACE) {
            if (tokens[pos].type == TokenType::ASM) {
                block.body.push_back(parse_asm_block(tokens, pos));
            }
            else if (tokens[pos].type == TokenType::LET) {
                block.body.push_back(parse_let_stmt(tokens, pos, false, true));
            }
            else if (tokens[pos].type == TokenType::STATIC) {
                block.body.push_back(parse_let_stmt(tokens, pos, true, true));
            }
            else if (tokens[pos].type == TokenType::RETURN) {
                ASTNode ret_node;
                ret_node.type = NodeType::RETURN_STMT;
                pos++;
                
                if (pos < tokens.size() && tokens[pos].type != TokenType::NEWLINE && tokens[pos].type != TokenType::RBRACE) {
                    string val;
                    while (pos < tokens.size() && tokens[pos].type != TokenType::NEWLINE && tokens[pos].type != TokenType::RBRACE) {
                        if (!val.empty()) val += " ";
                        val += tokens[pos].value;
                        pos++;
                    }
                    ret_node.var_value = val;
                }
                block.body.push_back(ret_node);
            }
            else if (tokens[pos].type == TokenType::WHILE) {
                ASTNode while_node;
                while_node.type = NodeType::WHILE_STMT;
                pos++;

                if (pos < tokens.size() && tokens[pos].type == TokenType::LPAREN) {
                    pos++;
                    string cond;
                    while (pos < tokens.size() && tokens[pos].type != TokenType::RPAREN) {
                        if (!cond.empty()) cond += " ";
                        cond += tokens[pos].value;
                        pos++;
                    }
                    if (pos < tokens.size() && tokens[pos].type == TokenType::RPAREN) {
                        pos++;
                    }
                    while_node.condition = cond;
                }

                if (pos < tokens.size() && tokens[pos].type == TokenType::LBRACE) {
                    ASTNode body = parse_block(tokens, pos);
                    while_node.body = body.body;
                }
                block.body.push_back(while_node);
            }
            else if (tokens[pos].type == TokenType::IF) {
                ASTNode if_node;
                if_node.type = NodeType::IF_STMT;
                pos++;

                if (pos < tokens.size() && tokens[pos].type == TokenType::LPAREN) {
                    pos++;
                    string cond;
                    while (pos < tokens.size() && tokens[pos].type != TokenType::RPAREN) {
                        if (!cond.empty()) cond += " ";
                        cond += tokens[pos].value;
                        pos++;
                    }
                    if (pos < tokens.size() && tokens[pos].type == TokenType::RPAREN) {
                        pos++;
                    }
                    if_node.condition = cond;
                }

                if (pos < tokens.size() && tokens[pos].type == TokenType::LBRACE) {
                    ASTNode body = parse_block(tokens, pos);
                    if_node.body = body.body;
                }

                if (pos < tokens.size() && tokens[pos].type == TokenType::ELSE) {
                    pos++;
                    if (pos < tokens.size() && tokens[pos].type == TokenType::LBRACE) {
                        ASTNode else_body = parse_block(tokens, pos);
                        if_node.else_body = else_body.body;
                    }
                }
                block.body.push_back(if_node);
            }
            else if (tokens[pos].type == TokenType::BREAK) {
                ASTNode break_node;
                break_node.type = NodeType::BREAK_STMT;
                pos++;
                block.body.push_back(break_node);
            }
            else if (tokens[pos].type == TokenType::AT_ARGS) {
                ASTNode var_body;
                var_body.type = NodeType::VARIADIC_BODY;
                pos++;

                if (pos < tokens.size() && tokens[pos].type == TokenType::LBRACE) {
                    ASTNode body = parse_block(tokens, pos);
                    var_body.body = body.body;
                }
                block.body.push_back(var_body);
            }
            else if (tokens[pos].type == TokenType::IDENT) {
                string name = tokens[pos].value;
                pos++;

                if (pos < tokens.size() && tokens[pos].type == TokenType::PLUSPLUS) {
                    ASTNode inc_node;
                    inc_node.type = NodeType::INC_STMT;
                    inc_node.var_name = name;
                    pos++;
                    block.body.push_back(inc_node);
                }
                else {
                    ASTNode call_node;
                    call_node.type = NodeType::FUNC_CALL;
                    call_node.value = name;

                    if (pos < tokens.size() && tokens[pos].type == TokenType::LPAREN) {
                        pos++;
                        while (pos < tokens.size() && tokens[pos].type != TokenType::RPAREN) {
                            if (tokens[pos].type == TokenType::COMMA || tokens[pos].type == TokenType::NEWLINE) {
                                pos++;
                                continue;
                            }
                            if (tokens[pos].type == TokenType::IDENT && tokens[pos].value == "args" &&
                                pos + 1 < tokens.size() && tokens[pos + 1].type == TokenType::LBRACKET) {
                                string arg_str = "args";
                                pos++;
                                int depth = 0;
                                while (pos < tokens.size() && tokens[pos].type != TokenType::RPAREN) {
                                    if (tokens[pos].type == TokenType::COMMA && depth == 0) break;
                                    if (tokens[pos].type == TokenType::LBRACKET || tokens[pos].type == TokenType::LPAREN) depth++;
                                    if (tokens[pos].type == TokenType::RBRACKET && depth > 0) depth--;
                                    if (!tokens[pos].value.empty()) {
                                        arg_str += tokens[pos].value;
                                    }
                                    pos++;
                                }
                                call_node.args.push_back(arg_str);
                            }
                            else if (tokens[pos].type == TokenType::ARGS) {
                                string arg_str = "args[]";
                                pos++;
                                if (pos < tokens.size() && tokens[pos].type == TokenType::IDENT) {
                                    arg_str += tokens[pos].value;
                                    pos++;
                                }
                                call_node.args.push_back(arg_str);
                            }
                            else if (!tokens[pos].value.empty()) {
                                string arg_val = tokens[pos].value;
                                pos++;
                                if (arg_val == "&" && pos < tokens.size() && 
                                    (tokens[pos].type == TokenType::IDENT || tokens[pos].type == TokenType::NUMBER)) {
                                    arg_val += tokens[pos].value;
                                    pos++;
                                }
                                call_node.args.push_back(arg_val);
                            }
                            else {
                                pos++;
                            }
                        }
                        if (pos < tokens.size() && tokens[pos].type == TokenType::RPAREN) {
                            pos++;
                        }
                    }
                    block.body.push_back(call_node);
                }
            }
            else if (tokens[pos].type == TokenType::NEWLINE) {
                pos++;
            }
            else {
                pos++;
            }
        }
        if (pos < tokens.size() && tokens[pos].type == TokenType::RBRACE) {
            pos++;
        }
    }
    return block;
}

// Parse @data directives for assembly global data blocks
ASTNode parse_data_block(const vector<Token>& tokens, size_t& pos) {
    ASTNode node;
    node.type = NodeType::DATA_BLOCK;
    pos++;

    if (pos < tokens.size() && tokens[pos].type == TokenType::LBRACE) {
        pos++;

        while (pos < tokens.size() && tokens[pos].type != TokenType::RBRACE) {
            if (tokens[pos].type == TokenType::ASM) {
                node.body.push_back(parse_asm_block(tokens, pos));
            }
            else {
                pos++;
            }
        }
        if (pos < tokens.size() && tokens[pos].type == TokenType::RBRACE) {
            pos++;
        }
    }
    return node;
}

// Parse @library directives for Windows DLL imports
ASTNode parse_dll_block(const vector<Token>& tokens, size_t& pos) {
    ASTNode node;
    node.type = NodeType::DLL_BLOCK;
    pos++;

    if (pos < tokens.size() && tokens[pos].type == TokenType::IDENT) {
        node.value = tokens[pos].value;
        pos++;
    }

    if (pos < tokens.size() && tokens[pos].type == TokenType::LBRACE) {
        pos++;

        while (pos < tokens.size() && tokens[pos].type != TokenType::RBRACE) {
            if (tokens[pos].type == TokenType::NEWLINE) {
                pos++;
                continue;
            }

            if (!tokens[pos].value.empty()) {
                node.imports.push_back(tokens[pos].value);
            }
            pos++;
        }

        if (pos < tokens.size() && tokens[pos].type == TokenType::RBRACE) {
            pos++;
        }
    }
    return node;
}

// Parse function definitions
ASTNode parse_function(const vector<Token>& tokens, size_t& pos) {
    ASTNode node;
    node.type = NodeType::FUNC_DEF;
    pos++;

    if (pos < tokens.size() && tokens[pos].type == TokenType::IDENT) {
        node.value = tokens[pos].value;
        pos++;
    }

    if (pos < tokens.size() && tokens[pos].type == TokenType::LPAREN) {
        pos++;
        while (pos < tokens.size() && tokens[pos].type != TokenType::RPAREN) {
            if (tokens[pos].type == TokenType::COMMA) {
                pos++;
                continue;
            }
            if (tokens[pos].type == TokenType::ARGS) {
                node.is_variadic = true;
                pos++;
                continue;
            }
            if (tokens[pos].type == TokenType::IDENT) {
                string first = tokens[pos].value;
                pos++;
                if (first == "*" && pos < tokens.size() && tokens[pos].type == TokenType::IDENT) {
                    first += tokens[pos].value;
                    pos++;
                }
                if (pos < tokens.size() && tokens[pos].type == TokenType::IDENT) {
                    node.params.push_back(tokens[pos].value);
                    pos++;
                }
                else {
                    node.params.push_back(first);
                }
            }
            else {
                pos++;
            }
        }
        if (pos < tokens.size() && tokens[pos].type == TokenType::RPAREN) {
            pos++;
        }
    }

    if (pos < tokens.size() && tokens[pos].type == TokenType::ARROW) {
        pos++;
        if (pos < tokens.size() && tokens[pos].type == TokenType::IDENT) {
            node.return_type = tokens[pos].value;
            pos++;
        }
        if (node.return_type == "*" && pos < tokens.size() && tokens[pos].type == TokenType::IDENT) {
            node.return_type += tokens[pos].value;
            pos++;
        }
    }

    if (pos < tokens.size() && tokens[pos].type == TokenType::LBRACE) {
        ASTNode body = parse_block(tokens, pos);

        for (const auto& stmt : body.body) {
            if (stmt.type == NodeType::VARIADIC_BODY) {
                node.variadic_body = stmt.body;
            }
            else {
                node.body.push_back(stmt);
            }
        }
    }
    return node;
}

// Global AST Parser entry point
vector<ASTNode> parse(const vector<Token>& tokens) {
    vector<ASTNode> nodes;
    size_t pos = 0;

    while (pos < tokens.size() && tokens[pos].type != TokenType::END){
        if (tokens[pos].type == TokenType::FORMAT) {
            pos++;
            if (pos < tokens.size() && tokens[pos].type == TokenType::STRING) {
                ASTNode node;
                node.type = NodeType::FORMAT;
                node.value = tokens[pos].value;
                nodes.push_back(node);
                pos++;
            }
        }
        else if (tokens[pos].type == TokenType::FUNC) {
            nodes.push_back(parse_function(tokens, pos));
        }
        else if (tokens[pos].type == TokenType::DIRECTIVE && tokens[pos].value == "data") {
            nodes.push_back(parse_data_block(tokens, pos));
        }
        else if (tokens[pos].type == TokenType::DIRECTIVE && tokens[pos].value == "library") {
            nodes.push_back(parse_dll_block(tokens, pos));
        }
        else if (tokens[pos].type == TokenType::DIRECTIVE && tokens[pos].value == "include.inc") {
            ASTNode node;
            node.type = NodeType::INCLUDE_BLOCK;
            pos++;
            if (pos < tokens.size() && tokens[pos].type == TokenType::LBRACE) {
                pos++;
                while (pos < tokens.size() && tokens[pos].type != TokenType::RBRACE) {
                    if (tokens[pos].type == TokenType::STRING) {
                        node.imports.push_back(tokens[pos].value);
                    }
                    pos++;
                }
                if (pos < tokens.size() && tokens[pos].type == TokenType::RBRACE) {
                    pos++;
                }
            }
            nodes.push_back(node);
        }
        else if (tokens[pos].type == TokenType::USELIB) {
            pos++;
            if (pos < tokens.size() && tokens[pos].type == TokenType::STRING) {
                ASTNode node;
                node.type = NodeType::USELIB;
                node.value = tokens[pos].value;
                nodes.push_back(node);
                pos++;
            }
        }
        else if (tokens[pos].type == TokenType::LET) {
            nodes.push_back(parse_let_stmt(tokens, pos, false, false));
        }
        else if (tokens[pos].type == TokenType::STATIC) {
            nodes.push_back(parse_let_stmt(tokens, pos, true, false));
        }
        else {
            pos++;
        }
    }
    return nodes;
}