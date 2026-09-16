#include "header/codegen.h"

using namespace std;

// counter for unique labels: str_arg_0, str_arg_1
static int data_counter = 0;

// Generate assembly for blocks asm {}
void generate_asm_block(const ASTNode& node, string& code,
                        const map<string, string>& locals,
                        const map<string, string>& params,
                        const map<string, string>& var_types) {
    for (const auto& line : node.asm_lines) {
        string result = "    ";
        size_t i = 0;
        while (i < line.size()) {
            if (line[i] == '{') {
                size_t close = line.find('}', i);
                if (close != string::npos) {
                    string name = line.substr(i + 1, close - i - 1);
                    string clean;
                    for (char c : name) {
                        if (c != ' ') clean += c;
                    }

                    auto lit = locals.find(clean);
                    if (lit != locals.end()) {
                        result += lit->second;
                    }
                    else {
                        auto pit = params.find(clean);
                        if (pit != params.end()) {
                            result += pit->second;
                        }
                        else {
                            result += clean;
                        }
                    }
                    i = close + 1;
                }
                else {
                    result += line[i];
                    i++;
                }
            }
            else {
                result += line[i];
                i++;
            }
        }
        code += result + "\n";
    }
}

// Generate call function with arguments. Every arguments goint to ABI (rcx)
void generate_func_call(const ASTNode& stmt, string& code, string& extra_data, 
                        const map<string, string>& var_types, 
                        const map<string, string>& locals, 
                        const map<string, string>& params) {
    if (stmt.args.empty()) {
        code += "    call func_" + stmt.value + "\n";
    }
    else {
        for (size_t i = 0; i < stmt.args.size(); i++) {
            string arg = stmt.args[i];

            if (arg.size() >= 2 && arg[0] == '"') {
                string label = "str_arg_" + to_string(data_counter++);
                string clean = arg.substr(1, arg.size() - 2);
                extra_data += "    " + label + " db '" + clean + "',0\n";
                code += "    lea rcx, [" + label + "]\n";
            }
            else if (!arg.empty() && (isdigit(arg[0]) || (arg.size() > 1 && arg[0] == '-'))) {
                code += "    mov rcx, " + arg + "\n";
            }
            else {
                auto lit = locals.find(arg);
                if (lit != locals.end()) {
                    code += "    mov rcx, " + lit->second + "\n";
                }
                else {
                    auto pit = params.find(arg);
                    if (pit != params.end()) {
                        code += "    mov rcx, " + pit->second + "\n";
                    }
                    else {
                        auto it = var_types.find(arg);
                        if (it != var_types.end() && it->second == "str") {
                            code += "    lea rcx, [" + arg + "]\n";
                        }
                        else {
                            code += "    mov rcx, [" + arg + "]\n";
                        }
                    }
                }
            }
            code += "    call func_" + stmt.value + "\n";
        }
    }
}

// resolving the variables
string resolve_var(const string& name, const map<string, string>& locals, const map<string, string>& params) {
    auto lit = locals.find(name);
    if (lit != locals.end()) return lit->second;
    auto pit = params.find(name);
    if (pit != params.end()) return pit->second;
    return name;
}

static void emit_condition(const string& cond, string& code,
                           const map<string, string>& locals,
                           const map<string, string>& params,
                           const string& jump_if_false_label) {
    if (cond == "true") return;

    string normalized = cond;
    size_t pos;
    while ((pos = normalized.find("< =")) != string::npos) normalized.replace(pos, 3, "<=");
    while ((pos = normalized.find("> =")) != string::npos) normalized.replace(pos, 3, ">=");
    while ((pos = normalized.find("! =")) != string::npos) normalized.replace(pos, 3, "!=");
    while ((pos = normalized.find("= =")) != string::npos) normalized.replace(pos, 3, "==");

    static const vector<pair<string, string>> ops = {
        {"<=", "jg"},
        {">=", "jl"},
        {"!=", "je"},
        {"==", "jne"},
        {"<",  "jge"},
        {">",  "jle"},
    };

    for (const auto& op : ops) {
        pos = normalized.find(op.first);
        if (pos != string::npos) {
            string left = normalized.substr(0, pos);
            string right = normalized.substr(pos + op.first.size());

            while (!left.empty() && left.back() == ' ') left.pop_back();
            while (!right.empty() && right[0] == ' ') right = right.substr(1);

            size_t bracket_pos = left.find('[');
            if (bracket_pos != string::npos) {
                string base = left.substr(0, bracket_pos);
                while (!base.empty() && base.back() == ' ') base.pop_back();

                size_t close_pos = left.find(']');
                string index = left.substr(bracket_pos + 1, close_pos - bracket_pos - 1);
                while (!index.empty() && index[0] == ' ') index = index.substr(1);
                while (!index.empty() && index.back() == ' ') index.pop_back();

                string base_reg = resolve_var(base, locals, params);
                string idx_reg = resolve_var(index, locals, params);

                code += "    cmp byte [" + base_reg + " + " + idx_reg + "], " + right + "\n";
            } else {
                string left_reg = resolve_var(left, locals, params);
                code += "    cmp " + left_reg + ", " + right + "\n";
            }

            code += "    " + op.second + " " + jump_if_false_label + "\n";
            return;
        }
    }
}

//(args[] ... )
string get_arg_type(const string& arg, const map<string, string>& locals, const map<string, string>& params, const map<string, string>& var_types, const map<string, string>& local_types) {
    if (arg.size() >= 2 && arg[0] == '"') {
        return "str";
    }
    if (!arg.empty() && (isdigit(arg[0]) || (arg.size() > 1 && arg[0] == '-'))) {
        return "int";
    }
    auto vit = var_types.find(arg);
    if (vit != var_types.end()) {
        if (vit->second == "str") return "str";
        return "int";
    }
    auto lit2 = local_types.find(arg);
    if (lit2 != local_types.end()) {
        if (lit2->second == "str") return "str";
        return "int";
    }
    return "int";
}

static int count_locals(const vector<ASTNode>& stmts) {
    int count = 0;
    for (const auto& stmt : stmts) {
        if (stmt.type == NodeType::LET_STMT) count++;
        else if (stmt.type == NodeType::WHILE_STMT || stmt.type == NodeType::IF_STMT) {
            count += count_locals(stmt.body);
            count += count_locals(stmt.else_body);            
        }
    }
    return count;
}

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
// println("Hello", 42, "World")
void generate_variadic_call(const ASTNode& stmt, const ASTNode& func_def,
                            string& code, string& extra_data,
                            const map<string, string>& var_types,
                            const map<string, string>& locals,
                            const map<string, string>& local_types,
                            const map<string, string>& params) {
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
                            generate_func_call(new_call, code, extra_data, var_types, locals, params);
                        }
                    }
                }
            }
        }
    }
}

// Main function -> Generate the body code
void generate_block(const vector<ASTNode>& stmts, string& code, string& extra_data, 
                    const map<string, string>& var_types,
                    const map<string, string>& params,
                    map<string, string>& locals,
                    map<string, string>& local_types,
                    vector<string>& local_regs, int& local_index,
                    const string& loop_end_label,
                    const map<string, ASTNode>& variadic_funcs) {
    for (const auto& stmt : stmts) {
        if (stmt.type == NodeType::ASM_BLOCK) {
            generate_asm_block(stmt, code, locals, params, var_types);
        }
        else if (stmt.type == NodeType::LET_STMT) {
            string reg = local_regs[local_index++];
            locals[stmt.var_name] = reg;
            if (!stmt.var_type.empty()) {
                local_types[stmt.var_name] = stmt.var_type;
            }

            if (stmt.var_type == "str" && stmt.var_value.size() >= 2 && stmt.var_value[0] == '"') {
                string label = "str_arg_" + to_string(data_counter++);
                string clean = stmt.var_value.substr(1, stmt.var_value.size() - 2);
                extra_data += "    " + label + " db '" + clean + "',0\n";
                code += "    lea " + reg + ", [" + label + "]\n";
            }
            else if (stmt.var_value.empty()) {
                code += "    xor " + reg + ", " + reg + "\n";
            }
            else {
                auto pit = params.find(stmt.var_value);
                if (pit != params.end()) {
                    code += "    mov " + reg + ", " + pit->second + "\n";
                }
                else if (!stmt.var_value.empty() && (isdigit(stmt.var_value[0]) || (stmt.var_value.size() > 1 && stmt.var_value[0] == '-'))) {
                    if (stmt.var_value == "0") {
                        code += "    xor " + reg + ", " + reg + "\n";
                    }
                    else {
                        code += "    mov " + reg + ", " + stmt.var_value + "\n";
                    }
                }
                else {
                    auto lit = locals.find(stmt.var_value);
                    if (lit != locals.end()) {
                        code += "    mov " + reg + ", " + lit->second + "\n";
                    }
                    else {
                        code += "    mov " + reg + ", [" + stmt.var_value + "]\n";
                    }
                }
            }
        }
        else if (stmt.type == NodeType::INC_STMT) {
            auto it = locals.find(stmt.var_name);
            if (it != locals.end()) {
                code += "    inc " + it->second + "\n";
            }
        }
        else if (stmt.type == NodeType::BREAK_STMT) {
            if (!loop_end_label.empty()) {
                code += "    jmp " + loop_end_label + "\n";
            }
        }
        else if (stmt.type == NodeType::WHILE_STMT) {
            static int while_counter = 0;
            string start_label = ".while_start_" + to_string(while_counter);
            string end_label = ".while_end_" + to_string(while_counter);
            while_counter++;

            code += "  " + start_label + ":\n";

            emit_condition(stmt.condition, code, locals, params, end_label);

            generate_block(stmt.body, code, extra_data, var_types, params, locals, local_types, local_regs, local_index, end_label, variadic_funcs);

            code += "    jmp " + start_label + "\n";
            code += "  " + end_label + ":\n";
        }
        else if (stmt.type == NodeType::IF_STMT) {
            static int if_counter = 0;
            string else_label = ".if_else_" + to_string(if_counter);
            string end_label = ".if_end_" + to_string(if_counter);
            if_counter++;

            emit_condition(stmt.condition, code, locals, params, else_label);

            generate_block(stmt.body, code, extra_data, var_types, params, locals, local_types, local_regs, local_index, loop_end_label, variadic_funcs);

            if (!stmt.else_body.empty()) {
                code += "    jmp " + end_label + "\n";
                code += "  " + else_label + ":\n";
                generate_block(stmt.else_body, code, extra_data, var_types, params, locals, local_types, local_regs, local_index, loop_end_label, variadic_funcs);
            }
            else {
                code += "  " + else_label + ":\n";
            }
            code += "  " + end_label + ":\n";
        }
        else if (stmt.type == NodeType::FUNC_CALL) {
            auto vit = variadic_funcs.find(stmt.value);
            if (vit != variadic_funcs.end()) {
                generate_variadic_call(stmt, vit->second, code, extra_data, var_types, locals, local_types, params);
                for (const auto& body_stmt : vit->second.body) {
                    if (body_stmt.type == NodeType::FUNC_CALL) {
                        generate_func_call(body_stmt, code, extra_data, var_types, locals, params);
                    }
                    else if (body_stmt.type == NodeType::ASM_BLOCK) {
                        generate_asm_block(body_stmt, code, locals, params, var_types);
                    }
                }
            }
            else {
                generate_func_call(stmt, code, extra_data, var_types, locals, params);
            }
        }
    }
}

void generate_function(const ASTNode& node, string& code, string& extra_data, const map<string, string>& var_types, const map<string, ASTNode>& variadic_funcs) {
    code += "start:\n";

    bool has_locals = false;
    bool has_func_calls = false;

    for (const auto& stmt : node.body) {
        if (stmt.type == NodeType::LET_STMT) {
            has_locals = true;
        }
        else if (stmt.type == NodeType::FUNC_CALL) {
            has_func_calls = true;
        }
        else if (stmt.type == NodeType::WHILE_STMT || stmt.type == NodeType::IF_STMT) {
            for (const auto& s : stmt.body) {
                if (s.type == NodeType::LET_STMT) has_locals = true;
                if (s.type == NodeType::FUNC_CALL) has_func_calls = true;
            }
            for (const auto& s : stmt.else_body) {
                if (s.type == NodeType::LET_STMT) has_locals = true;
                if (s.type == NodeType::FUNC_CALL) has_func_calls = true;
            }
        }
    }

    if (has_locals || has_func_calls) {
        code += "    sub rsp, 8\n";
        code += "    and rsp, -16\n";

        vector<string> param_regs = {"rcx", "rdx", "r8", "r9"};
        map<string, string> params;
        for (size_t i = 0; i < node.params.size() && i < 4; i++) {
            params[node.params[i]] = param_regs[i];
        }

        code += "    push rbx\n";
        code += "    push r12\n";
        code += "    push r13\n";
        code += "    push r14\n";
        code += "    push r15\n";
        code += "    push rdi\n";
        code += "    sub rsp, 48\n";

        vector<string> local_regs = {"rbx", "r12", "r13", "r14", "r15", "rdi"};
        int local_index = 0;
        map<string, string> locals;
        map<string, string> local_types;

        generate_block(node.body, code, extra_data, var_types, params, locals, local_types, local_regs, local_index, "", variadic_funcs);

        code += "    add rsp, 48\n";
        code += "    pop rdi\n";
        code += "    pop r15\n";
        code += "    pop r14\n";
        code += "    pop r13\n";
        code += "    pop r12\n";
        code += "    pop rbx\n";
    }
    else {
        vector<string> param_regs = {"rcx", "rdx", "r8", "r9"};
        map<string, string> params;
        for (size_t i = 0; i < node.params.size() && i < 4; i++) {
            params[node.params[i]] = param_regs[i];
        }

        vector<string> local_regs = {"rbx", "r12", "r13", "r14", "r15", "rdi"};
        int local_index = 0;
        map<string, string> locals;
        map<string, string> local_types;

        generate_block(node.body, code, extra_data, var_types, params, locals, local_types, local_regs, local_index, "", variadic_funcs);
    }
}

void generate_func_def(const ASTNode& node, string& code, string& extra_data, const map<string, string>& var_types, const map<string, ASTNode>& variadic_funcs) {
    code += "\nfunc_" + node.value + ":\n";
    code += "    push rbp\n";
    code += "    mov rbp, rsp\n";

    vector<string> param_regs = {"rcx", "rdx", "r8", "r9"};
    map<string, string> params;
    for (size_t i = 0; i < node.params.size() && i < 4; i++) {
        params[node.params[i]] = param_regs[i];
    }

    int local_count = count_locals(node.body);
    bool calls_funcs = has_func_calls(node.body);

    vector<string> local_regs = {"rbx", "r12", "r13", "r14", "r15", "rdi"};
    int regs_needed = min(local_count, (int)local_regs.size());
    
    for (int i = 0; i < regs_needed; i++) {
        code += "    push " + local_regs[i] + "\n";
    }

    if (calls_funcs) {
        code += "    sub rsp, 48\n";
    }

    int local_index = 0;
    map<string, string> locals;
    map<string, string> local_types;

    generate_block(node.body, code, extra_data, var_types, params, locals, local_types, local_regs, local_index, "", variadic_funcs);

    if (calls_funcs) {
        code += "    add rsp, 48\n";
    }

    code += "    mov rsp, rbp\n";
    code += "    pop rbp\n";
    code += "    ret\n";
}

string type_to_fasm(const string& type) {
    if (type == "i8" || type == "u8" || type == "char") return "db";
    if (type == "i16" || type == "u16") return "dw";
    if (type == "i32" || type == "u32") return "dd";
    if (type == "i64" || type == "u64" || type == "i128" || type == "u128") return "dq";
    return "dq";
}

void generate_data_section(const vector<ASTNode>& nodes, string& code, const string& extra_data) {
    code += "section '.data' data readable writeable\n";
    code += "    wisel_v1 dd ?\n";
    for (const auto& node : nodes) {
        if (node.type == NodeType::DATA_BLOCK) {
            for (const auto& stmt : node.body) {
                if (stmt.type == NodeType::ASM_BLOCK) {
                    map<string, string> empty;
                    generate_asm_block(stmt, code, empty, empty, empty);
                }
            }
        }
        else if (node.type == NodeType::LET_STMT && !node.is_func_local) {
            string name = node.var_name;
            string type = node.var_type;
            string value = node.var_value;

            if (type == "str") {
                string raw = value;
                if (raw.size() >= 2 && raw[0] == '"' && raw.back() == '"') {
                    raw = raw.substr(1, raw.size() - 2);
                }
                string fasm_bytes;
                size_t display_len = 0;
                for (size_t i = 0; i < raw.size(); i++) {
                    if (raw[i] == '\\' && i + 1 < raw.size()) {
                        char next = raw[i + 1];
                        if (next == 'r') {
                            if (!fasm_bytes.empty()) fasm_bytes += ",";
                            fasm_bytes += "13";
                            i++;
                            display_len++;
                        }
                        else if (next == 'n') {
                            if (!fasm_bytes.empty()) fasm_bytes += ",";
                            fasm_bytes += "10";
                            i++;
                            display_len++;
                        }
                        else if (next == 't') {
                            if (!fasm_bytes.empty()) fasm_bytes += ",";
                            fasm_bytes += "9";
                            i++;
                            display_len++;
                        }
                        else if (next == '\\') {
                            if (!fasm_bytes.empty()) fasm_bytes += ",";
                            fasm_bytes += "'\\'";
                            i++;
                            display_len++;
                        }
                        else if (next == '"') {
                            if (!fasm_bytes.empty()) fasm_bytes += ",";
                            fasm_bytes += "34";
                            i++;
                            display_len++;
                        }
                    }
                    else {
                        if (!fasm_bytes.empty()) fasm_bytes += ",";
                        fasm_bytes += "'" + string(1, raw[i]) + "'";
                        display_len++;
                    }
                }
                code += "    " + name + " db " + fasm_bytes + "\n";
                code += "    " + name + "_len dd " + to_string(display_len) + "\n";
            }
            else {
                string fasm_value = value.empty() ? "?" : value;
                code += "    " + name + " " + type_to_fasm(type) + " " + fasm_value + "\n";
            }
        }
    }
    code += extra_data;
    code += "\n";
}

void generate_import_section(const vector<ASTNode>& nodes, string& code) {
    code += "section '.idata' import data readable writeable\n";

    string library_line = "    library ";
    bool first_dll = true;

    for (const auto& node : nodes) {
        if (node.type == NodeType::DLL_BLOCK) {
            if (!first_dll) {
                library_line += ",";
            }
            string dll_name = node.value;
            string dll_upper = dll_name;
            for (auto& c : dll_upper) c = toupper(c);
            library_line += dll_name + ",'" + dll_upper + ".DLL'";
            first_dll = false;
        }
    }
    code += library_line + "\n";

    for (const auto& node : nodes) {
        if (node.type == NodeType::DLL_BLOCK) {
            string dll_name = node.value;
            code += "    import " + dll_name + ",\\\n";
            
            for (size_t i = 0; i < node.imports.size(); i++) {
                string func = node.imports[i];
                code += "        " + func + ",'" + func + "'";
                if (i < node.imports.size() - 1) {
                    code += ",\\";
                }
                code += "\n";
            }
        }
    }
}

string generate(const vector<ASTNode>& nodes) {
    string format_str = "PE64 CONSOLE";
    bool has_main = false;
    string text_code;
    string extra_data;
    data_counter = 0;

    map<string, string> var_types;
    map<string, ASTNode> variadic_funcs;

    for (const auto& node : nodes) {
        if (node.type == NodeType::FORMAT) {
            format_str = node.value;
        }
        else if (node.type == NodeType::FUNC_DEF) {
            if (node.value == "main") has_main = true;
            if (node.is_variadic) variadic_funcs[node.value] = node;
        }
        else if (node.type == NodeType::LET_STMT && !node.is_func_local) {
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
                generate_function(node, text_code, extra_data, var_types, variadic_funcs);
            }
            else if (!node.is_variadic) {
                generate_func_def(node, text_code, extra_data, var_types, variadic_funcs);
            }
        }
    }

    if (!has_main) {
        text_code += "start:\n";
        text_code += "    invoke ExitProcess, 0\n";
    }

    string code;
    code += "format " + clean_format + "\n";
    code += "entry start\n\n";
    for (const auto& node : nodes) {
        if (node.type == NodeType::INCLUDE_BLOCK) {
            for (const auto& inc : node.imports) {
                string clean = inc;
                if (clean.size() >= 2 && clean.front() == '"' && clean.back() == '"') {
                    clean = clean.substr(1, clean.size() - 2);
                }
                code += "include '" + clean + "'\n";
            }
        }
    }
    code += "\n"; 

    generate_data_section(nodes, code, extra_data);

    code += "section '.text' code readable executable\n";
    code += text_code;
    code += "\n";

    generate_import_section(nodes, code);

    return code;
}