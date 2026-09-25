#include <iostream>
#include <sstream>
#include <algorithm>
#include "header/codegen.h"

using namespace std;

// Type registry
static bool is_int_type(const string& t) { return !t.empty() && (t[0] == 'i' || t[0] == 'u'); }
static bool is_signed_type(const string& t) { return !t.empty() && t[0] == 'i'; }
static bool is_unsigned_type(const string& t) { return !t.empty() && t[0] == 'u'; }
static bool is_pointer_type(const string& t) { return t.size() >= 2 && t[0] == '*'; }
static bool is_string_type(const string& t) { return t == "str" || t == "*u8" || t == "*i8" || t == "*char"; }
static bool is_float_type(const string& t) { return t == "f32" || t == "f64"; }
static bool is_char_type(const string& t) { return t == "char"; }

// Counters & registries
static int data_counter = 0;
static string current_func_name;
static map<string, string> func_return_types;
static map<string, vector<string>> func_param_types;

// Standard emit helpers
static void emit(std::stringstream& ss, const std::string& instr) {
    ss << "    " << instr << "\n";
}

static void emit_label(std::stringstream& ss, const std::string& label) {
    ss << label << ":\n";
}

static void emit_comment(std::stringstream& ss, const std::string& comment) {
    ss << "    ; " << comment << "\n";
}

// Helper trim function
static string trim(string s) {
    size_t first = s.find_first_not_of(" \t\r\n");
    return (first == string::npos) ? "" : s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
}

// Forward declaration
string resolve_var(const string& name, const map<string, string>& locals, 
                   const map<string, string>& params,
                   const map<string, string>& var_types);

// Escape string for FASM db directive
string escape_for_fasm(const string& raw) {
    string result;
    for (size_t i = 0; i < raw.size(); i++) {
        if (!result.empty()) result += ",";
        
        if (raw[i] == '\\' && i + 1 < raw.size()) {
            switch (raw[++i]) {
                case 'r': result += "13"; break;
                case 'n': result += "10"; break;
                case 't': result += "9";  break;
                case '"': result += "34"; break;
                case '\'': result += "39"; break;
                case '\\': result += "'\\'"; break;
                default: result += "'" + string(1, raw[i]) + "'"; break;
            }
        } else if (raw[i] == '\'') {
            result += "39";
        } else if (raw[i] == '"') {
            result += "34";
        } else {
            result += "'" + string(1, raw[i]) + "'";
        }
    }
    return result;
}

// Generate inline assembly blocks
void generate_asm_block(const ASTNode& node, std::stringstream& ss,
                        const map<string, string>& locals,
                        const map<string, string>& params,
                        const map<string, string>& var_types) {
    for (const auto& line : node.asm_lines) {
        string result;
        size_t pos = 0;

        while (pos < line.size()) {
            size_t start = line.find('{', pos);
            if (start == string::npos) { result += line.substr(pos); break; }
            
            size_t close = line.find('}', start);
            if (close == string::npos) { result += line.substr(pos); break; }
            result += line.substr(pos, start - pos);
        
            string name = line.substr(start + 1, close - start - 1), clean;
            for (char c : name) if (c != ' ') clean += c;

            auto lit = locals.find(clean);
            auto pit = params.find(clean);
            result += (lit != locals.end()) ? lit->second : (pit != params.end() ? pit->second : clean);
            pos = close + 1;
        }
        emit(ss, trim(result));
    }
}

// Generate function call with arguments in rcx, rdx, r8, r9 (Windows x64 ABI)
void generate_func_call(const ASTNode& stmt, std::stringstream& ss, std::stringstream& extra_ss, 
                        const map<string, string>& var_types, 
                        const map<string, string>& locals, 
                        const map<string, string>& params,
                        const map<string, bool>& defined_functions) {
    if (stmt.args.empty()) {
        if (defined_functions.find(stmt.value) != defined_functions.end()) {
            emit(ss, "call func_" + stmt.value);
        } else {
            emit(ss, "call [" + stmt.value + "]");
        }
        return;
    }

    vector<string> arg_regs = {"rcx", "rdx", "r8", "r9"};

    for (size_t i = 0; i < stmt.args.size() && i < 4; i++) {
        const string& arg = stmt.args[i];
        string reg = arg_regs[i];

        if (arg.size() >= 2 && arg.front() == '"') {
            string label = "str_arg_" + to_string(data_counter++);
            string raw_arg = arg.substr(1, arg.size() - 2);
            emit(extra_ss, label + " db " + escape_for_fasm(raw_arg) + ",0");
            emit(ss, "lea " + reg + ", [" + label + "]");
        }
        else if (!arg.empty() && (isdigit(arg[0]) || (arg.size() > 1 && arg[0] == '-' && isdigit(arg[1])))) {
            emit(ss, "mov " + reg + ", " + arg);
        }
        else {
            if (arg.size() >= 2 && arg[0] == '&') {
                string varname = arg.substr(1);
                auto lit = locals.find(varname);
                auto pit = params.find(varname);
                auto tit = var_types.find(varname);
                
                if (lit != locals.end()) {
                    emit(ss, "sub rsp, 8");
                    emit(ss, "mov qword [rsp], " + lit->second);
                    emit(ss, "lea " + reg + ", [rsp]");
                } else if (pit != params.end()) {
                    emit(ss, "lea " + reg + ", [" + pit->second + "]");
                } else if (tit != var_types.end()) {
                    emit(ss, "lea " + reg + ", [" + varname + "]");
                } else {
                    emit(ss, "lea " + reg + ", [" + varname + "]");
                }
            }
            else {
                auto lit = locals.find(arg);
                auto pit = params.find(arg);
                auto tit = var_types.find(arg);

                if (lit != locals.end()) {
                    emit(ss, "mov " + reg + ", " + lit->second);
                } else if (pit != params.end()) {
                    emit(ss, "mov " + reg + ", " + pit->second);
                } else if (tit != var_types.end() && (is_string_type(tit->second) || tit->second == "i8" || tit->second == "u8")) {
                    emit(ss, "lea " + reg + ", [" + arg + "]");
                } else if (arg.find('[') != string::npos) {
                    string resolved = resolve_var(arg, locals, params, var_types);
                    emit(ss, "movzx " + reg + ", " + resolved);
                } else {
                    emit(ss, "mov " + reg + ", [" + arg + "]");
                }
            }
        }
    }

    if (stmt.args.size() > 4) {
        int stack_args = stmt.args.size() - 4;
        emit(ss, "sub rsp, " + to_string(stack_args * 8));
        for (size_t i = 4; i < stmt.args.size(); i++) {
            const string& arg = stmt.args[i];
            int offset = (i - 4) * 8;
            if (!arg.empty() && isdigit(arg[0])) {
                emit(ss, "mov qword [rsp + " + to_string(offset) + "], " + arg);
            } else if (arg == "NULL" || arg == "null" || arg == "0") {
                emit(ss, "mov qword [rsp + " + to_string(offset) + "], 0");
            } else {
                auto lit = locals.find(arg);
                auto pit = params.find(arg);
                auto tit = var_types.find(arg);
                if (lit != locals.end()) {
                    emit(ss, "mov qword [rsp + " + to_string(offset) + "], " + lit->second);
                } else if (pit != params.end()) {
                    emit(ss, "mov qword [rsp + " + to_string(offset) + "], " + pit->second);
                } else if (tit != var_types.end()) {
                    emit(ss, "mov qword [rsp + " + to_string(offset) + "], [" + arg + "]");
                } else {
                    emit(ss, "mov qword [rsp + " + to_string(offset) + "], [" + arg + "]");
                }
            }
        }
    }

    if (defined_functions.find(stmt.value) != defined_functions.end()) {
        emit(ss, "call func_" + stmt.value);
    } else {
        emit(ss, "call [" + stmt.value + "]");
    }

    if (stmt.args.size() > 4) {
        int stack_args = stmt.args.size() - 4;
        emit(ss, "add rsp, " + to_string(stack_args * 8));
    }
}

// Resolving the variables
string resolve_var(const string& name, const map<string, string>& locals, 
                   const map<string, string>& params,
                   const map<string, string>& var_types) {
    size_t bracket = name.find('[');
    if (bracket != string::npos) {
        string base = name.substr(0, bracket);
        size_t close = name.find(']');
        string index = name.substr(bracket + 1, close - bracket - 1);
        
        string base_resolved = resolve_var(base, locals, params, var_types);
        
        auto idx_lit = locals.find(index);
        auto idx_pit = params.find(index);
        string idx_resolved;
        if (idx_lit != locals.end()) idx_resolved = idx_lit->second;
        else if (idx_pit != params.end()) idx_resolved = idx_pit->second;
        else idx_resolved = index;
        
        return "byte [" + base + " + " + idx_resolved + "]";
    }
    auto lit = locals.find(name);
    if (lit != locals.end()) return lit->second;
    auto pit = params.find(name);
    if (pit != params.end()) return pit->second;
    auto vit = var_types.find(name);
    if (vit != var_types.end()) {
        if (is_string_type(vit->second)) return name;
        string type = vit->second;
        if (type == "i8" || type == "u8" || type == "char") return "byte [" + name + "]";
        if (type == "i16" || type == "u16") return "word [" + name + "]";
        if (type == "i32" || type == "u32") return "dword [" + name + "]";
        if (type == "i64" || type == "u64") return "qword [" + name + "]";
        return "[" + name + "]";
    }
    return name;
}

// Generate comparison and jump for if/while conditions
static void emit_condition(const string& cond, std::stringstream& ss,
                           const map<string, string>& locals,
                           const map<string, string>& params,
                           const map<string, string>& var_types,
                           const string& jump_if_false_label,
                           std::stringstream& extra_ss,
                           const map<string, string>& local_types) {
    if (cond == "true") return;
    size_t pos;

    static const vector<pair<string, string>> ops = {
        {"<=", "jg"}, {">=", "jl"},
        {"!=", "je"}, {"==", "jne"},
        {"<",  "jge"}, {">",  "jle"},
    };

    for (const auto& op : ops) {
        pos = cond.find(op.first);
        if (pos != string::npos) {
            string left = trim(cond.substr(0, pos));
            string right = trim(cond.substr(pos + op.first.size()));

            size_t bracket_pos = left.find('[');
            if (bracket_pos != string::npos) {
                string base = trim(left.substr(0, bracket_pos));
                size_t close_pos = left.find(']');
                string index = trim(left.substr(bracket_pos + 1, close_pos - bracket_pos - 1));

                string base_reg = resolve_var(base, locals, params, var_types);
                string idx_reg = resolve_var(index, locals, params, var_types);

                if (right.size() == 3 && right[0] == '\'' && right[2] == '\'') {
                    int char_val = (unsigned char)right[1];
                    emit(ss, "cmp byte [" + base_reg + " + " + idx_reg + "], " + to_string(char_val));
                } else {
                    emit(ss, "cmp byte [" + base_reg + " + " + idx_reg + "], " + right);
                }
            }
            else if ((op.first == "==" || op.first == "!=") && right.size() >= 2 && right[0] == '"') {
                string label = "cmp_str_" + to_string(data_counter++);
                string cmp_label = "cmp_" + to_string(data_counter++);
                string raw = right.substr(1, right.size() - 2);
                
                emit(extra_ss, label + " db " + escape_for_fasm(raw) + ",0");
                
                string left_reg = resolve_var(left, locals, params, var_types);
                emit_comment(ss, "String comparison");
                emit(ss, "lea rsi, [" + label + "]");
                emit(ss, "mov rdi, " + left_reg);
                emit_label(ss, "." + cmp_label + "_loop");
                emit(ss, "mov al, [rsi]");
                emit(ss, "mov cl, [rdi]");
                emit(ss, "cmp al, cl");
                emit(ss, "jne ." + cmp_label + "_diff");
                emit(ss, "test al, al");
                emit(ss, "jz ." + cmp_label + "_equal");
                emit(ss, "inc rsi");
                emit(ss, "inc rdi");
                emit(ss, "jmp ." + cmp_label + "_loop");
                emit_label(ss, "." + cmp_label + "_diff");
                if (op.first == "==") {
                    emit(ss, "jmp " + jump_if_false_label);
                } else {
                    emit_comment(ss, "strings different, != is true, continue");
                }
                emit(ss, "jmp ." + cmp_label + "_end");
                emit_label(ss, "." + cmp_label + "_equal");
                if (op.first == "!=") {
                    emit(ss, "jmp " + jump_if_false_label);
                } else {
                    emit_comment(ss, "strings equal, == is true, continue");
                }
                emit_label(ss, "." + cmp_label + "_end");
                return;
            }
            else if (right.size() == 3 && right[0] == '\'' && right[2] == '\'') {
                int char_val = (unsigned char)right[1];
                string left_reg = resolve_var(left, locals, params, var_types);
                
                bool is_string = false;
                auto vit = var_types.find(left);
                if (vit != var_types.end() && (is_pointer_type(vit->second) || is_string_type(vit->second))) {
                    is_string = true;
                }
                auto lit = local_types.find(left);
                if (lit != local_types.end() && (is_pointer_type(lit->second) || is_string_type(lit->second))) {
                    is_string = true;
                }
                
                if (is_string) {
                    emit(ss, "cmp byte [" + left_reg + "], " + to_string(char_val));
                } else {
                    emit(ss, "cmp " + left_reg + ", " + to_string(char_val));
                }
                emit(ss, op.second + " " + jump_if_false_label);
                return;
            }
            else {
                string left_reg = resolve_var(left, locals, params, var_types);
                emit(ss, "cmp " + left_reg + ", " + right);
                emit(ss, op.second + " " + jump_if_false_label);
                return;
            }
        }
    }
}

// Determine argument type for variadic function dispatch
string get_arg_type(const string& arg, const map<string, string>& locals, const map<string, string>& params, const map<string, string>& var_types, const map<string, string>& local_types) {
    if (arg.size() >= 2 && arg[0] == '"') return "str";
    if (!arg.empty() && (isdigit(arg[0]) || (arg.size() > 1 && arg[0] == '-'))) return "int";

    auto vit = var_types.find(arg);
    if (vit != var_types.end() && is_string_type(vit->second)) return "str";

    auto lit2 = local_types.find(arg);
    if (lit2 != local_types.end() && is_string_type(lit2->second)) return "str";

    return "int";
}

// Count local variables for register allocation
static int count_locals(const vector<ASTNode>& stmts) {
    int count = 0;
    for (const auto& stmt : stmts) {
        if (stmt.type == NodeType::LET_STMT) {
            if (!stmt.is_static) count++;
        }
        else if (stmt.type == NodeType::WHILE_STMT || stmt.type == NodeType::IF_STMT) {
            count += count_locals(stmt.body);
            count += count_locals(stmt.else_body);            
        }
    }
    return count;
}

// Check if block contains function calls for stack frame allocation
static bool has_func_calls(const vector<ASTNode>& stmts) {
    for (const auto& stmt : stmts) {
        if (stmt.type == NodeType::FUNC_CALL) return true;
        if (stmt.type == NodeType::WHILE_STMT || stmt.type == NodeType::IF_STMT) {
            if (has_func_calls(stmt.body)) return true;
            if (has_func_calls(stmt.else_body)) return true;
        }   
    }
    return false;
}

// Check if any inline assembly block contains a 'call' or 'invoke' instruction
static bool has_asm_calls(const vector<ASTNode>& stmts) {
    for (const auto& stmt : stmts) {
        if (stmt.type == NodeType::ASM_BLOCK) {
            bool has_sub_rsp = false;
            for (const auto& line : stmt.asm_lines) {
                if (line.find("sub rsp") != string::npos) has_sub_rsp = true;
            }
            if (has_sub_rsp) continue;
            for (const auto& line : stmt.asm_lines) {
                if (line.find("call") != string::npos || line.find("invoke") != string::npos) {
                    return true;
                }
            }
        }
        else if (stmt.type == NodeType::LET_STMT) {
            if (!stmt.var_value.empty() && stmt.var_value.find("(") != string::npos) {
                return true;
            }
        }
        else if (stmt.type == NodeType::WHILE_STMT || stmt.type == NodeType::IF_STMT) {
            if (has_asm_calls(stmt.body) || has_asm_calls(stmt.else_body)) {
                return true;
            }
        }
    }
    return false;
}

// Generate code for variadic function arguments (println, etc)
void generate_variadic_call(const ASTNode& stmt, const ASTNode& func_def,
                            std::stringstream& ss, std::stringstream& extra_ss,
                            const map<string, string>& var_types,
                            const map<string, string>& locals,
                            const map<string, string>& local_types,
                            const map<string, string>& params,
                            const map<string, bool>& defined_functions) {
    for (const auto& arg : stmt.args) {
        string arg_type = get_arg_type(arg, locals, params, var_types, local_types);

        for (const auto& var_stmt : func_def.variadic_body) {
            if (var_stmt.type == NodeType::IF_STMT) {
                string cond = var_stmt.condition;
                bool match = false;

                if (cond.find("str") != string::npos && arg_type == "str") match = true;
                if (cond.find("int") != string::npos && (arg_type == "i32" || arg_type == "i64" || arg_type == "int")) match = true;

                if (match) {
                    for (const auto& body_stmt : var_stmt.body) {
                        if (body_stmt.type == NodeType::FUNC_CALL) {
                            ASTNode new_call = body_stmt;
                            for (size_t j = 0; j < new_call.args.size(); j++) {
                                if (new_call.args[j].find("args[") != string::npos) {
                                    new_call.args[j] = arg;
                                }
                            }
                            generate_func_call(new_call, ss, extra_ss, var_types, locals, params, defined_functions);
                        }
                    }
                }
            }
        }
    }
}

// Validate type compatibility
static void validate_type(const string& var_name, const string& var_type, 
                          const string& value, const map<string, string>& func_return_types_map) {
    if (value.size() >= 2 && value[0] == '"' && value.back() == '"') {
        if (is_int_type(var_type) || is_float_type(var_type)) {
            cerr << "[ERROR] Cannot assign string to numeric type '" << var_type 
                 << "' for variable '" << var_name << "'" << endl;
            exit(1);
        }
    }
    
    if (!value.empty() && (isdigit(value[0]) || (value.size() > 1 && value[0] == '-' && isdigit(value[1])))) {
        if (is_string_type(var_type)) {
            cerr << "[ERROR] Cannot assign number to string type '" << var_type 
                 << "' for variable '" << var_name << "'" << endl;
            exit(1);
        }
    }
    
    if (value.find("(") != string::npos && value.find(")") != string::npos) {
        string func_name = trim(value.substr(0, value.find("(")));
        
        auto frt = func_return_types_map.find(func_name);
        if (frt != func_return_types_map.end()) {
            string ret_type = frt->second;
            bool ret_is_int = is_int_type(ret_type);
            bool var_is_str = is_string_type(var_type) || is_pointer_type(var_type);
            
            if (ret_is_int && var_is_str) {
                cerr << "[ERROR] Function '" << func_name << "' returns " << ret_type 
                     << " but variable '" << var_name << "' is " << var_type << endl;
                exit(1);
            }
        }
    }
}

// Generate code for block statements
void generate_block(const vector<ASTNode>& stmts, std::stringstream& ss, std::stringstream& extra_ss, 
                    const map<string, string>& var_types,
                    const map<string, string>& params,
                    map<string, string>& locals,
                    map<string, string>& local_types,
                    vector<string>& local_regs, int& local_index,
                    const string& loop_end_label,
                    const map<string, ASTNode>& variadic_funcs,
                    const map<string, bool>& defined_functions) {
    
    for (const auto& stmt : stmts) {
        switch (stmt.type) {

            case NodeType::ASM_BLOCK: {
                generate_asm_block(stmt, ss, locals, params, var_types);
                break;
            }

            case NodeType::LET_STMT: {
                if (stmt.is_static) break;
                
                validate_type(stmt.var_name, stmt.var_type, stmt.var_value, func_return_types);
                
                string comment = "let " + stmt.var_name;
                if (!stmt.var_type.empty()) comment += ": " + stmt.var_type;
                if (!stmt.var_value.empty()) comment += " = " + stmt.var_value;
                emit_comment(ss, comment);

                if (local_index >= (int)local_regs.size()) {
                    cerr << "[ERROR] Too many local variables (max " << local_regs.size() << "). Variable: " << stmt.var_name << endl;
                    exit(1);
                }
                string reg = local_regs[local_index++];
                locals[stmt.var_name] = reg;
                if (!stmt.var_type.empty()) {
                    local_types[stmt.var_name] = stmt.var_type;
                }

                // 1. String literal
                if ((is_string_type(stmt.var_type) || is_pointer_type(stmt.var_type)) && 
                    stmt.var_value.size() >= 2 && stmt.var_value[0] == '"') {
                    string label = "str_arg_" + to_string(data_counter++);
                    string clean = stmt.var_value.substr(1, stmt.var_value.size() - 2);
                    emit(extra_ss, label + " db " + escape_for_fasm(clean) + ",0");
                    emit(ss, "lea " + reg + ", [" + label + "]");
                }
                // 2. Binary arithmetic expression (+, -, *, /, %)
                else if (stmt.expr && stmt.expr->type == NodeType::BINARY_OP) {
                    string left_reg  = resolve_var(stmt.expr->left->value, locals, params, var_types);
                    string right_val = resolve_var(stmt.expr->right->value, locals, params, var_types);
                    string op = stmt.expr->op;

                    if (op == "+" || op == "-" || op == "*") {
                        string asm_op = (op == "+") ? "add" : (op == "-") ? "sub" : "imul";
                        emit(ss, "mov " + reg + ", " + left_reg);
                        emit(ss, asm_op + " " + reg + ", " + right_val);
                    } 
                    else if (op == "/" || op == "%") {
                        emit(ss, "mov rax, " + left_reg);
                        emit(ss, "xor rdx, rdx");
                        if (!right_val.empty() && isdigit(right_val[0])) {
                            emit(ss, "mov r10, " + right_val);
                            emit(ss, "div r10");
                        } else {
                            emit(ss, "div " + right_val);
                        }
                        emit(ss, "mov " + reg + ", " + (op == "%" ? "rdx" : "rax"));
                    }
                }
                // 3. Uninitialized variable (zero initialization)
                else if (stmt.var_value.empty()) {
                    emit(ss, "xor " + reg + ", " + reg);
                }
                // 4. Function call result assignment
                else if (stmt.var_value.find("(") != string::npos && stmt.var_value.find(")") != string::npos) {
                    string func_name = trim(stmt.var_value.substr(0, stmt.var_value.find("(")));

                    size_t open = stmt.var_value.find("(");
                    size_t close = stmt.var_value.rfind(")");
                    string args_str = stmt.var_value.substr(open + 1, close - open - 1);

                    vector<string> call_args;
                    string current;
                    int depth = 0;
                    for (size_t i = 0; i < args_str.size(); i++) {
                        char c = args_str[i];
                        if (c == '(' || c == '[') depth++;
                        else if (c == ')' || c == ']') depth--;
                        else if (c == ',' && depth == 0) {
                            string trimmed = trim(current);
                            if (!trimmed.empty()) call_args.push_back(trimmed);
                            current.clear();
                            continue;
                        }
                        current += c;
                    }
                    string trimmed = trim(current);
                    if (!trimmed.empty()) call_args.push_back(trimmed);

                    vector<string> arg_regs = {"rcx", "rdx", "r8", "r9"};
                    for (size_t i = 0; i < call_args.size() && i < 4; i++) {
                        const string& arg = call_args[i];
                        string arg_reg = arg_regs[i];

                        if (arg.size() >= 2 && arg.front() == '"') {
                            string label = "str_arg_" + to_string(data_counter++);
                            string raw_arg = arg.substr(1, arg.size() - 2);
                            emit(extra_ss, label + " db " + escape_for_fasm(raw_arg) + ",0");
                            emit(ss, "lea " + arg_reg + ", [" + label + "]");
                        }
                        else if (!arg.empty() && (isdigit(arg[0]) || (arg.size() > 1 && arg[0] == '-' && isdigit(arg[1])))) {
                            emit(ss, "mov " + arg_reg + ", " + arg);
                        }
                        else {
                            auto lit = locals.find(arg);
                            auto pit = params.find(arg);
                            auto tit = var_types.find(arg);
                            if (lit != locals.end()) {
                                emit(ss, "mov " + arg_reg + ", " + lit->second);
                            } else if (pit != params.end()) {
                                emit(ss, "mov " + arg_reg + ", " + pit->second);
                            } else if (tit != var_types.end() && is_string_type(tit->second)) {
                                emit(ss, "lea " + arg_reg + ", [" + arg + "]");
                            } else {
                                emit(ss, "lea " + arg_reg + ", [" + arg + "]");
                            }
                        }
                    }

                    if (defined_functions.find(func_name) != defined_functions.end()) {
                        emit(ss, "call func_" + func_name);
                    } else {
                        emit(ss, "call [" + func_name + "]");
                    }

                    auto frt = func_return_types.find(func_name);
                    if (frt != func_return_types.end() && !stmt.var_type.empty()) {
                        string return_type = frt->second;
                        bool func_returns_str = is_string_type(return_type) || is_pointer_type(return_type);
                        bool var_is_int = is_int_type(stmt.var_type);
                        
                        if (func_returns_str && var_is_int) {
                            emit(ss, "mov rcx, rax");
                            emit(ss, "call func_atoi");
                        }
                    }

                    emit(ss, "mov " + reg + ", rax");
                }
                // 5. Numeric constant
                else if (!stmt.var_value.empty() && (isdigit(stmt.var_value[0]) || (stmt.var_value.size() > 1 && stmt.var_value[0] == '-'))) {
                    if (stmt.var_value == "0") emit(ss, "xor " + reg + ", " + reg);
                    else emit(ss, "mov " + reg + ", " + stmt.var_value);
                }
                // 6. Copy from another variable
                else {
                    if (is_pointer_type(stmt.var_type) || is_string_type(stmt.var_type)) {
                        auto vit = var_types.find(stmt.var_value);
                        if (vit != var_types.end()) {
                            emit(ss, "lea " + reg + ", [" + stmt.var_value + "]");
                        } else {
                            string resolved = resolve_var(stmt.var_value, locals, params, var_types);
                            emit(ss, "mov " + reg + ", " + resolved);
                        }
                    }
                    else {
                        string resolved = resolve_var(stmt.var_value, locals, params, var_types);
                        auto vit = var_types.find(stmt.var_value);
                        if (vit != var_types.end()) {
                            string type = vit->second;
                            if (type == "i32" || type == "u32") emit(ss, "mov " + reg + ", dword [" + stmt.var_value + "]");
                            else if (type == "i16" || type == "u16") emit(ss, "mov " + reg + ", word [" + stmt.var_value + "]");
                            else if (type == "i8" || type == "u8" || type == "char") emit(ss, "mov " + reg + ", byte [" + stmt.var_value + "]");
                            else emit(ss, "mov " + reg + ", qword [" + stmt.var_value + "]");
                        } else {
                            emit(ss, "mov " + reg + ", " + resolved);
                        }
                    }
                }
                break;
            }

            case NodeType::RETURN_STMT: {
                string val = stmt.var_value;
                if (!val.empty()) {
                    if (val.size() >= 2 && val[0] == '"') {
                        string label = "str_arg_" + to_string(data_counter++);
                        string clean = val.substr(1, val.size() - 2);
                        emit(extra_ss, label + " db " + escape_for_fasm(clean) + ",0");
                        emit(ss, "lea rax, [" + label + "]");
                    }
                    else if (!val.empty() && (isdigit(val[0]) || (val.size() > 1 && val[0] == '-'))) {
                        emit(ss, "mov rax, " + val);
                    }
                    else {
                        auto type_it = var_types.find(val);
                        if (type_it != var_types.end() && is_string_type(type_it->second)) {
                            emit(ss, "lea rax, [" + val + "]");
                        }
                        else {
                            emit(ss, "mov rax, " + resolve_var(val, locals, params, var_types));
                        }
                    }
                }
                emit(ss, "jmp .func_end_" + current_func_name);
                break;
            }

            case NodeType::INC_STMT: {
                auto it = locals.find(stmt.var_name);
                if (it != locals.end()) {
                    emit_comment(ss, "increment " + stmt.var_name);
                    emit(ss, "inc " + it->second);
                }
                break;
            }

            case NodeType::BREAK_STMT: {
                if (!loop_end_label.empty()) {
                    emit_comment(ss, "break loop");
                    emit(ss, "jmp " + loop_end_label);
                }
                break;
            }

            case NodeType::WHILE_STMT: {
                static int while_counter = 0;
                string start_label = ".while_start_" + to_string(while_counter);
                string end_label = ".while_end_" + to_string(while_counter);
                while_counter++;

                emit_label(ss, start_label);
                emit_condition(stmt.condition, ss, locals, params, var_types, end_label, extra_ss, local_types);
                generate_block(stmt.body, ss, extra_ss, var_types, params, locals, local_types, local_regs, local_index, end_label, variadic_funcs, defined_functions);
                emit(ss, "jmp " + start_label);
                emit_label(ss, end_label);
                break;
            }

            case NodeType::IF_STMT: {
                static int if_counter = 0;
                string else_label = ".if_else_" + to_string(if_counter);
                string end_label = ".if_end_" + to_string(if_counter);
                if_counter++;

                emit_condition(stmt.condition, ss, locals, params, var_types, else_label, extra_ss, local_types);
                generate_block(stmt.body, ss, extra_ss, var_types, params, locals, local_types, local_regs, local_index, loop_end_label, variadic_funcs, defined_functions);

                if (!stmt.else_body.empty()) {
                    emit(ss, "jmp " + end_label);
                    emit_label(ss, else_label);
                    generate_block(stmt.else_body, ss, extra_ss, var_types, params, locals, local_types, local_regs, local_index, loop_end_label, variadic_funcs, defined_functions);
                }
                else {
                    emit_label(ss, else_label);
                }
                emit_label(ss, end_label);
                break;
            }

            case NodeType::FUNC_CALL: {
                string args_str;
                for (size_t i = 0; i < stmt.args.size(); i++) {
                    if (i > 0) args_str += ", ";
                    args_str += stmt.args[i];
                }
                emit_comment(ss, "call function " + stmt.value + "(" + args_str + ")");
                auto vit = variadic_funcs.find(stmt.value);
                if (vit != variadic_funcs.end()) {
                    generate_variadic_call(stmt, vit->second, ss, extra_ss, var_types, locals, local_types, params, defined_functions);
                    for (const auto& body_stmt : vit->second.body) {
                        if (body_stmt.type == NodeType::FUNC_CALL) {
                            generate_func_call(body_stmt, ss, extra_ss, var_types, locals, params, defined_functions);
                        }
                        else if (body_stmt.type == NodeType::ASM_BLOCK) {
                            generate_asm_block(body_stmt, ss, locals, params, var_types);
                        }
                    }
                }
                else {
                    generate_func_call(stmt, ss, extra_ss, var_types, locals, params, defined_functions);
                }
                break;
            }

            default:
                break;
        }
    }
}

// Generate main function entry point
void generate_function(const ASTNode& node, std::stringstream& ss, std::stringstream& extra_ss, const map<string, string>& var_types, const map<string, ASTNode>& variadic_funcs, const map<string, bool>& defined_functions) {
    emit_label(ss, "start");

    bool has_locals = count_locals(node.body) > 0;
    bool calls_funcs = has_func_calls(node.body) || has_asm_calls(node.body);

    vector<string> param_regs = {"rcx", "rdx", "r8", "r9"};
    map<string, string> params;
    for (size_t i = 0; i < node.params.size() && i < 4; i++) {
        params[node.params[i]] = param_regs[i];
    }

    int local_count = count_locals(node.body);
    vector<string> local_regs = {"rbx", "r12", "r13", "r14", "r15", "rdi", "rsi", "rbp", "r9", "r10", "r11", "r8"};
    int regs_needed = min(local_count, (int)local_regs.size());
    int local_index = 0;
    map<string, string> locals;
    map<string, string> local_types;

    if (has_locals || calls_funcs) {
        emit(ss, "sub rsp, 8");
        emit(ss, "and rsp, -16");
        for (int i = 0; i < regs_needed; i++) {
            emit(ss, "push " + local_regs[i]);
        }
        int alignment_padding = (regs_needed % 2 == 0) ? 8 : 0;
        int rsp_sub_size = 32 + alignment_padding;
        emit(ss, "sub rsp, " + to_string(rsp_sub_size));
    }

    current_func_name = "main";
    generate_block(node.body, ss, extra_ss, var_types, params, locals, local_types, local_regs, local_index, "", variadic_funcs, defined_functions);

    if (has_locals || calls_funcs) {
        int alignment_padding = (regs_needed % 2 == 0) ? 8 : 0;
        int rsp_sub_size = 32 + alignment_padding;
        emit(ss, "add rsp, " + to_string(rsp_sub_size));
        for (int i = regs_needed - 1; i >= 0; i--) {
            emit(ss, "pop " + local_regs[i]);
        }
    }
}

// Generate named function definition
void generate_func_def(const ASTNode& node, std::stringstream& ss, std::stringstream& extra_ss, const map<string, string>& var_types, const map<string, ASTNode>& variadic_funcs, const map<string, bool>& defined_functions) {
    ss << "\n";
    emit_label(ss, "func_" + node.value);
    
    vector<string> param_regs = {"rcx", "rdx", "r8", "r9"};
    map<string, string> params;
    for (size_t i = 0; i < node.params.size() && i < 4; i++) {
        params[node.params[i]] = param_regs[i];
    }

    int local_count = count_locals(node.body);
    bool calls_funcs = has_func_calls(node.body) || has_asm_calls(node.body);

    vector<string> local_regs = {"rbx", "r12", "r13", "r14", "r15", "rdi", "rsi", "rbp", "r9", "r10", "r11", "r8"};
    int regs_needed = min(local_count, (int)local_regs.size());
    
    for (int i = 0; i < regs_needed; i++) {
        emit(ss, "push " + local_regs[i]);
    }

    int param_space = 0;
    if (calls_funcs && node.params.size() > 0) {
        param_space = node.params.size() * 8;
        emit(ss, "sub rsp, " + to_string(param_space));
        for (size_t i = 0; i < node.params.size() && i < 4; i++) {
            int offset = i * 8;
            emit(ss, "mov qword [rsp + " + to_string(offset) + "], " + param_regs[i]);
            params[node.params[i]] = "qword [rsp + " + to_string(offset) + "]";
        }
    }

    int local_index = 0;
    map<string, string> locals;
    map<string, string> local_types;

    current_func_name = node.value;
    generate_block(node.body, ss, extra_ss, var_types, params, locals, local_types, local_regs, local_index, "", variadic_funcs, defined_functions);

    emit_label(ss, ".func_end_" + node.value);

    if (calls_funcs && node.params.size() > 0) {
        int param_space = node.params.size() * 8;
        emit(ss, "add rsp, " + to_string(param_space));
    }

    for (int i = regs_needed - 1; i >= 0; i--) {
        emit(ss, "pop " + local_regs[i]);
    }

    emit(ss, "ret");
}

// Convert WiseL type to FASM directive
string type_to_fasm(const string& type) {
    if (type == "i8" || type == "u8" || type == "char") return "db";
    if (type == "i16" || type == "u16") return "dw";
    if (type == "i32" || type == "u32") return "dd";
    if (type == "i64" || type == "u64" || type == "i128" || type == "u128") return "dq";
    return "dq";
}

// Generate .data section
void generate_data_section(const vector<ASTNode>& nodes, std::stringstream& ss, const string& extra_data) {
    ss << "section '.data' data readable writeable\n";
    emit(ss, "wisel_v1 dd ?");
    for (const auto& node : nodes) {
        if (node.type == NodeType::DATA_BLOCK) {
            for (const auto& stmt : node.body) {
                if (stmt.type == NodeType::ASM_BLOCK) {
                    map<string, string> empty;
                    generate_asm_block(stmt, ss, empty, empty, empty);
                }
            }
        }
        else if (node.type == NodeType::LET_STMT && (node.is_static || !node.is_func_local)) {
            string name = node.var_name;
            string type = node.var_type;
            string value = node.var_value;

            if (is_string_type(type)) {
                string raw = value;
                if (raw.size() >= 2 && raw[0] == '"' && raw.back() == '"') {
                    raw = raw.substr(1, raw.size() - 2);
                }
                size_t display_len = 0;
                for (size_t i = 0; i < raw.size(); i++) {
                    if (raw[i] == '\\' && i + 1 < raw.size()) {
                        i++;
                    }
                    display_len++;
                }
                emit(ss, name + " db " + escape_for_fasm(raw) + ",0");
                emit(ss, name + "_len dd " + to_string(display_len));
            }
            else if (value.size() >= 2 && value.front() == '[' && value.back() == ']') {
                string size_str = value.substr(1, value.size() - 2);
                string clean_size;
                for (char c : size_str) {
                    if (c != ' ') clean_size += c;
                }
                emit(ss, name + " " + type_to_fasm(type) + " " + clean_size + " dup(?)");
            }
            else {
                string fasm_value = value.empty() ? "?" : value;
                emit(ss, name + " " + type_to_fasm(type) + " " + fasm_value);
            }
        }
    }
    ss << extra_data;
    ss << "\n";
}

// Generate .idata section
void generate_import_section(const vector<ASTNode>& nodes, std::stringstream& ss) {
    map<string, vector<string>> dll_imports;
    vector<string> dll_order;

    for (const auto& node : nodes) {
        if (node.type == NodeType::DLL_BLOCK) {
            string dll_name = node.value;
            if (dll_imports.find(dll_name) == dll_imports.end()) {
                dll_order.push_back(dll_name);
            }
            for (const auto& func : node.imports) {
                auto& imports = dll_imports[dll_name];
                bool found = false;
                for (const auto& existing : imports) {
                    if (existing == func) { found = true; break; }
                }
                if (!found) imports.push_back(func);
            }
        }
    }

    if (dll_order.empty()) return;

    string library_line = "    library ";
    string import_blocks = "";

    for (size_t d = 0; d < dll_order.size(); d++) {
        if (d > 0) library_line += ",\\\n            ";

        string dll_name = dll_order[d];
        string dll_upper = dll_name;
        for (auto& c : dll_upper) c = toupper(c);
        library_line += dll_name + ",'" + dll_upper + ".DLL'";
    }

    for (const auto& dll_name : dll_order) {
        import_blocks += "    import " + dll_name + ",\\\n";
        const auto& imports = dll_imports[dll_name];
        for (size_t i = 0; i < imports.size(); i++) {
            import_blocks += "        " + imports[i] + ",'" + imports[i] + "'";
            if (i < imports.size() - 1) import_blocks += ",\\";
            import_blocks += "\n";
        }
    }

    ss << "section '.idata' import data readable writeable\n";
    ss << library_line << "\n" << import_blocks;
}

// Main code generation entry point
string generate(const vector<ASTNode>& nodes) {
    string format_str = "PE64 CONSOLE";
    bool has_main = false;
    std::stringstream text_ss;
    std::stringstream extra_ss;
    
    data_counter = 0;
    func_return_types.clear();

    map<string, string> var_types;
    map<string, ASTNode> variadic_funcs;
    map<string, bool> defined_functions;

    for (const auto& node : nodes) {
        if (node.type == NodeType::FUNC_DEF) {
            defined_functions[node.value] = true;
        }
    }

    for (const auto& node : nodes) {
        if (node.type == NodeType::FORMAT) {
            format_str = node.value;
        }
        else if (node.type == NodeType::FUNC_DEF) {
            if (node.is_variadic) variadic_funcs[node.value] = node;
            if (!node.return_type.empty()) func_return_types[node.value] = node.return_type;
        }
        else if (node.type == NodeType::LET_STMT && (node.is_static || !node.is_func_local)) {
            var_types[node.var_name] = node.var_type;
        }
    }

    string clean_format = format_str;
    if (clean_format.size() >= 2 && clean_format.front() == '"' && clean_format.back() == '"') {
        clean_format = clean_format.substr(1, clean_format.size() - 2);
    }

    for (const auto& node : nodes) {
        if (node.type == NodeType::FUNC_DEF) {
            if (node.value == "main") {
                has_main = true;
                generate_function(node, text_ss, extra_ss, var_types, variadic_funcs, defined_functions);
            }
            else if (!node.is_variadic) {
                generate_func_def(node, text_ss, extra_ss, var_types, variadic_funcs, defined_functions);
            }
        }
    }

    if (!has_main) {
        emit_label(text_ss, "start");
        emit(text_ss, "invoke ExitProcess, 0");
    }

    std::stringstream ss;
    ss << "format " << clean_format << "\n";
    ss << "entry start\n\n";
    
    for (const auto& node : nodes) {
        if (node.type == NodeType::INCLUDE_BLOCK) {
            for (const auto& inc : node.imports) {
                ss << "include '" << (inc.size() >= 2 && inc.front() == '"' && inc.back() == '"' ? inc.substr(1, inc.size() - 2) : inc) << "'\n";
            }
        }
    }
    ss << "\n"; 

    generate_data_section(nodes, ss, extra_ss.str());

    ss << "section '.text' code readable executable\n";
    ss << text_ss.str();
    ss << "\n";

    generate_import_section(nodes, ss);

    return ss.str();
}