#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// トークンの種類
typedef enum {
  TK_RESERVED, // 記号
  TK_IDENT,    // 識別子
  TK_NUM,      // 数値
  TK_EOF,      // 入力の終わりを表すトークン
  TK_RETURN,   // return文
  TK_IF,       // if 文
  TK_ELSE,     // else
  TK_WHILE,    // while
  TK_FOR,      // for
  TK_BLOCK,    // block
  TK_TYPE,     // type
} TokenKind;

typedef struct Token Token;

// トークン型
struct Token {
  TokenKind kind; // トークンの型
  Token *next;    // 次の入力トークン
  int val;        // kindがTK＿NUMの時，その数値
  char *str;      // トークン文字列
  int len;        // トークンの長さ
};

typedef struct Type Type;
// 型がintかptrか
struct Type {
  enum { INT, PTR } ty;
  struct Type *ptr_to;
};

typedef struct LVar LVar;
struct LVar {
  LVar *next; // 次の変数かNULL
  char *name; // 変数名
  int len;    // 変数名長さ
  int offset; // RBPからのオフセット
  Type *type; // タイプ
};

LVar *find_lvar(Token *tok);

// 現在見ているトークン
extern Token *token;
extern char *user_input;
// ローカル変数
extern LVar *locals[];
extern int cur_func;

// エラー報告のための関数
// printfと同じ引数をとる
void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);

// 次のトークンが期待している記号の時には，トークンを1つ読み進めて
// trueを返す。それ以外はfalse
bool consume(char *op);
Token *consume_kind(TokenKind kind);

// 次のトークンが期待している記号の時には，トークンを1つ読み進める
// それ以外はエラー
void expect(char *op);

// 次のトークンが期待している記号の時には，トークンを1つ読み進めてその値を返す
// それ以外はエラー
int expect_number();

bool at_eof();

// 新しいトークンを作成してcurにつなげる
Token *new_token(TokenKind kind, Token *cur, char *str, int len);

bool startswith(char *p, char *q);

int is_alnum(char c);
// 入力文字列pをトークナイズしてそれを返す
Token *tokenize();

// Paeser
// 抽象構文木のノードの種類　
typedef enum {
  ND_ADD,       // +
  ND_SUB,       // -
  ND_MUL,       // *
  ND_DIV,       // /
  ND_EQ,        // ==
  ND_NE,        // !=
  ND_LT,        // <
  ND_LE,        // <=
  ND_NUM,       // 整数
  ND_ASSIGN,    // =
  ND_LVAR,      // ローカル変数
  ND_RETURN,    // return
  ND_IF,        // if
  ND_ELSE,      // else
  ND_WHILE,     // while
  ND_FOR,       // for
  ND_FOR_LEFT,  // for left
  ND_FOR_RIGHT, // for right
  ND_BLOCK,     // block
  ND_FUNC_CALL, // call func
  ND_FUNC_DEF,  // define func
  ND_ADDR,      // &
  ND_DEREF,     // *
} NodeKind;

typedef struct Node Node;

struct Node {
  NodeKind kind;  // ノードの型
  Node *lhs;      // 左辺 left hand side
  Node *rhs;      // 右辺 right hand side
  Node **block;   // only kind == ND_FUNC*
  Node **args;    // args only kind == ND_FUNC_DEF
  char *funcname; // 関数名
  int len;        //
  int val;        // kindがND_NUMの時だけ使う
  int offset;     // kindがND_LVARの時だけ使う
  Type *type;     // only kind == ND_LVAR
};

// 2項演算子用のNode
Node *new_node(NodeKind kind);

Node *new_binary(NodeKind kind, Node *lhs, Node *rhs);

// 数値を受け取る用のNode
Node *new_num(int val);

// Parser  TODO: 100はハードコード
extern Node *code[];
// program = stmt*
void program();
Node *func();
// stmt = expr ";"
Node *stmt();
// expr = assign
Node *expr();
// assign = equality ("=" assign)?
Node *assign();
// equality = relational ("==" relational | "!=" relational)*
Node *equality();
// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
Node *relational();
// add = mul ("+" mul | "-" mul)*
Node *add();
// mul = unary ("*" unary | "/" unary)*
Node *mul();
// unary = ("+" | "-")? primary
Node *unary();
// primary = num | indent | "(" expr ")"
Node *primary();
Node *define_variable();
Node *variable(Token *tok);

// Code Generator
void gen_lval(Node *node);
void gen(Node *node);
