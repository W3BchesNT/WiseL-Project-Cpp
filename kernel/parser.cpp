#include "header/parser.h"
#include <iostream>

using namespace std;

bool needs_space_before(const string& val) {
    if (val.empty()) return false;
    char c = val[0];
    return c != ',' && c != ']' && c != ')' && c != ':';
}

bool needs_space_after(const string& val) {
    if (val.empty()) return false;
    char c = val.back();
    return c != '[' && c != '(';
}

ASTNode parse_asm_block(const vector<Token>& tokens, size_t& pos) {
    ASTNode node;
    node.type = NodeType::ASM_BLOCK;

    pos++;
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

ASTNode parse_block(const vector<Token>& tokens, size_t& pos) {
    ASTNode block;
    block.type = NodeType::ASM_BLOCK;

    if (pos < tokens.size() && tokens[pos].type == TokenType::LBRACE) {
        pos++;

        while (pos < tokens.size() && tokens[pos].type != TokenType::RBRACE) {
            if (tokens[pos].type == TokenType::ASM) {
                ASTNode asm_node = parse_asm_block(tokens, pos);
                block.body.push_back(asm_node);
            }
            else if (tokens[pos].type == TokenType::LET) {
                ASTNode let_node;
                let_node.type = NodeType::LET_STMT;
                let_node.is_func_local = true;
                pos++;

                if (pos < tokens.size() && tokens[pos].type == TokenType::MUT) {
                    let_node.is_mut = true;
                    pos++;
                }
                if (pos < tokens.size() && tokens[pos].type == TokenType::IDENT) {
                    let_node.var_name = tokens[pos].value;
                    pos++;
                }
                if (pos < tokens.size() && tokens[pos].type == TokenType::COLON) {
                    pos++;
                }
                if (pos < tokens.size() && tokens[pos].type == TokenType::IDENT) {
                    let_node.var_type = tokens[pos].value;
                    pos++;
                }
                if (pos < tokens.size() && tokens[pos].type == TokenType::ASSIGN) {
                    pos++;
                    if (pos < tokens.size()) {
                        let_node.var_value = tokens[pos].value;
                        pos++;
                    }
                }
                block.body.push_back(let_node);
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
                            // args[str].value or args[int].value — collect as one arg
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
                                call_node.args.push_back(tokens[pos].value);
                                pos++;
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

ASTNode parse_data_block(const vector<Token>& tokens, size_t& pos) {
    ASTNode node;
    node.type = NodeType::DATA_BLOCK;
    pos++;

    if (pos < tokens.size() && tokens[pos].type == TokenType::LBRACE) {
        pos++;

        while (pos < tokens.size() && tokens[pos].type != TokenType::RBRACE) {
            if (tokens[pos].type == TokenType::ASM) {
                ASTNode asm_node = parse_asm_block(tokens, pos);
                node.body.push_back(asm_node);
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
                if (pos < tokens.size() && tokens[pos].type == TokenType::DOTDOTDOT) {
                    pos++;
                }
                continue;
            }
            if (tokens[pos].type == TokenType::IDENT) {
                string first = tokens[pos].value;
                pos++;
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
            ASTNode func_node = parse_function(tokens, pos);
            nodes.push_back(func_node);
        }
        else if (tokens[pos].type == TokenType::DIRECTIVE && tokens[pos].value == "data") {
            ASTNode data_node = parse_data_block(tokens, pos);
            nodes.push_back(data_node);
        }
        else if (tokens[pos].type == TokenType::DIRECTIVE && tokens[pos].value == "library") {
            ASTNode dll_node = parse_dll_block(tokens, pos);
            nodes.push_back(dll_node);
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
            ASTNode node;
            node.type = NodeType::LET_STMT;
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
            if (pos < tokens.size() && tokens[pos].type == TokenType::ASSIGN) {
                pos++;
                if (pos < tokens.size()) {
                    node.var_value = tokens[pos].value;
                    pos++;
                }
            }

            nodes.push_back(node);
        }
        else {
            pos++;
        }
    }
    return nodes;
}