#include "header/lexer.h"
#include <iostream>
#include <cctype>

using namespace std;

void skip_whitespace(const string& src, size_t& pos) {
    while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\r')) {
        pos++;
    }
}

string read_string(const string& src, size_t& pos) {
    string result = "\"";
    pos++;
    while (pos < src.size() && src[pos] != '"') {
        result += src[pos];
        pos++;
    }
    result += "\"";
    if (pos < src.size()) pos++;
    return result;
}

string read_identifier(const string& src, size_t& pos) {
    string result;
    while (pos < src.size() && (isalnum(src[pos]) || src[pos] == '_')) {
        result += src[pos];
        pos++;
    }
    return result;
}

string read_number(const string& src, size_t& pos) {
    string result;
    while (pos < src.size() && isdigit(src[pos])) {
        result += src[pos];
        pos++;
    }
    return result;
}

vector<Token> tokenize(const string& source) {
    vector<Token> tokens;
    size_t pos = 0;

    while (pos < source.size()) {
        skip_whitespace(source, pos);
        if (pos >= source.size()) break;

        char ch = source[pos];

        if (ch == '\n') {
            tokens.push_back({TokenType::NEWLINE, "\n"});
            pos++;
        }
        else if (ch == '"') {
            string str = read_string(source, pos);
            tokens.push_back({TokenType::STRING, str});
        }
        else if (ch == '\'') {
            string result = "'";
            pos++;
            while (pos < source.size() && source[pos] != '\'') {
                result += source[pos];
                pos++;
            }
            result += "'";
            if (pos < source.size()) pos++;
            tokens.push_back({TokenType::STRING, result});
        }
        else if (ch == '@') {
            pos++;
            if (pos < source.size() && isalpha(source[pos])) {
                string word = read_identifier(source, pos);
                if (word == "args") {
                    if (pos < source.size() && source[pos] == '[') {
                        pos++;
                        if (pos < source.size() && source[pos] == ']') {
                            pos++;
                            tokens.push_back({TokenType::AT_ARGS, "@args[]"});
                            continue;
                        }
                    }
                }
                while (pos < source.size() && source[pos] != ' ' && source[pos] != '\t' &&
                    source[pos] != '{' && source[pos] != '\n' && source[pos] != '\r') {
                    word += source[pos];
                    pos++;
                }
                tokens.push_back({TokenType::DIRECTIVE, word});
            }
            else {
                tokens.push_back({TokenType::IDENT, "@"});
            }
        }

        else if (ch == '(') {
            tokens.push_back({TokenType::LPAREN, "("});
            pos++;
        }
        else if (ch == ')') {
            tokens.push_back({TokenType::RPAREN, ")"});
            pos++;
        }
        else if (ch == '{') {
            tokens.push_back({TokenType::LBRACE, "{"});
            pos++;
        }
        else if (ch == '}') {
            tokens.push_back({TokenType::RBRACE, "}"});
            pos++;
        }
        else if (ch == '[') {
            tokens.push_back({TokenType::LBRACKET, "["});
            pos++;
        }
        else if (ch == ']') {
            tokens.push_back({TokenType::RBRACKET, "]"});
            pos++;
        }
        else if (ch == ',') {
            tokens.push_back({TokenType::COMMA, ","});
            pos++;
        }
        else if (ch == ':' && pos + 1 < source.size() && (isalpha(source[pos+1]) || source[pos+1] == '_')) {
            string label = ":";
            pos++;
            label += read_identifier(source, pos);
            tokens.push_back({TokenType::IDENT, label});
        }
        else if (ch == '.') {
            if (pos + 1 < source.size() && isdigit(source[pos+1])) {
                pos++;
                string num = "." + read_number(source, pos);
                tokens.push_back({TokenType::NUMBER, num});
            }
            else if (pos + 2 < source.size() && source[pos+1] == '.' && source[pos+2] == '.') {
                tokens.push_back({TokenType::DOTDOTDOT, "..."});
                pos += 3;
            }
            else if (pos + 1 < source.size() && (isalpha(source[pos+1]) || source[pos+1] == '_')) {
                string label = ".";
                pos++;
                label += read_identifier(source, pos);
                tokens.push_back({TokenType::IDENT, label});
            }
            else {
                tokens.push_back({TokenType::DOT, "."});
                pos++;
            }
        }
        else if (ch == ':') {
            tokens.push_back({TokenType::COLON, ":"});
            pos++;
        }
        else if (ch == '=') {
            if (pos + 1 < source.size() && source[pos + 1] == '=') {
                tokens.push_back({TokenType::EQ, "=="});
                pos += 2;
            }
            else {
                tokens.push_back({TokenType::ASSIGN, "="});
                pos++;
            }
        }
        else if (ch == '-' && pos + 1 < source.size() && isdigit(source[pos + 1])) {
            pos++;
            string num = "-" + read_number(source, pos);
            tokens.push_back({TokenType::NUMBER, num});
        }
        else if (ch == '+') {
            if (pos + 1 < source.size() && source[pos + 1] == '+') {
                tokens.push_back({TokenType::PLUSPLUS, "++"});
                pos += 2;
            }
            else {
                tokens.push_back({TokenType::IDENT, "+"});
                pos++;
            }
        }
        else if (isdigit(ch)) {
            string num = read_number(source, pos);
            tokens.push_back({TokenType::NUMBER, num});
        }
        else if (isalpha(ch) || ch == '_') {
            string word = read_identifier(source, pos);

            if (word == "Format") {
                tokens.push_back({TokenType::FORMAT, word});
            }
            else if (word == "func") {
                tokens.push_back({TokenType::FUNC, word});
            }
            else if (word == "asm") {
                tokens.push_back({TokenType::ASM, word});
            }
            else if (word == "let") {
                tokens.push_back({TokenType::LET, word});
            }
            else if (word == "mut") {
                tokens.push_back({TokenType::MUT, word});
            }
            else if (word == "while") {
                tokens.push_back({TokenType::WHILE, word});
            }
            else if (word == "if") {
                tokens.push_back({TokenType::IF, word});
            }
            else if (word == "else") {
                tokens.push_back({TokenType::ELSE, word});
            }
            else if (word == "break") {
                tokens.push_back({TokenType::BREAK, word});
            }
            else if (word == "UseLib") {
                tokens.push_back({TokenType::USELIB, word});
            }
            else if (word == "args") {
                if (pos + 1 < source.size() && source[pos] == '[' && source[pos + 1] == ']') {
                    pos += 2;
                    tokens.push_back({TokenType::ARGS, "args[]"});
                    continue;
                }
                tokens.push_back({TokenType::IDENT, word});
            }
            else {
                tokens.push_back({TokenType::IDENT, word});
            }
        }
        else {
            tokens.push_back({TokenType::IDENT, string(1, ch)});
            pos++;
        }
    }

    tokens.push_back({TokenType::END, ""});
    return tokens;
}