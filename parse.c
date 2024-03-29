#include "cc0.h"

// 現在見ているトークン
Token *token;
char *user_input;
// ローカル変数
LVar *locals[100];
Node *code[100];
int cur_func = 0;

// エラー報告のための関数
// printfと同じ引数をとる
void error(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

// 具体的な位置を示すエラー報告のための関数
void error_at(char *loc, char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  int pos = loc - user_input;
  fprintf(stderr, "%s\n", user_input);
  fprintf(stderr, "%*s", pos, " "); // pos個のwhitespace
  fprintf(stderr, "^ ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

// 次のトークンが期待している記号の時には，トークンを1つ読み進めて
// trueを返す。それ以外はfalse
bool consume(char *op) {
  if (token->kind != TK_RESERVED || strlen(op) != token->len ||
      memcmp(token->str, op, token->len))
    return false;
  // トークンを次に進める
  token = token->next;
  return true;
}

Token *consume_kind(TokenKind kind) {
  if (token->kind != kind) {
    return NULL;
  }
  Token *tok = token;
  token = token->next;
  return tok;
};

LVar *find_lvar(Token *tok) {
  for (LVar *var = locals[cur_func]; var; var = var->next) {
    if (var->len == tok->len && !memcmp(tok->str, var->name, var->len))
      return var;
  }
  return NULL;
}

// 次のトークンが期待している記号の時には，トークンを1つ読み進める
// それ以外はエラー
void expect(char *op) {
  if (token->kind != TK_RESERVED || strlen(op) != token->len ||
      memcmp(token->str, op, token->len))
    error_at(token->str, "'%s'ではありません", op);
  token = token->next;
}

// 次のトークンが期待している記号の時には，トークンを1つ読み進めてその値を返す
// それ以外はエラー
int expect_number() {
  if (token->kind != TK_NUM)
    error_at(token->str, "数字ではありません");
  int val = token->val;
  token = token->next;
  return val;
}

bool at_eof() { return token->kind == TK_EOF; }

// 新しいトークンを作成してcurにつなげる
Token *new_token(TokenKind kind, Token *cur, char *str, int len) {
  Token *tok = calloc(1, sizeof(Token));
  tok->kind = kind;
  tok->str = str;
  tok->len = len;
  cur->next = tok;
  return tok;
}

typedef struct ReservedWord ReservedWord;
struct ReservedWord {
  char *word;
  TokenKind kind;
};

ReservedWord reservedWords[] = {
    {"return", TK_RETURN}, {"if", TK_IF},   {"else", TK_ELSE},
    {"while", TK_WHILE},   {"for", TK_FOR}, {"int", TK_TYPE},
    {"", TK_EOF},
};

bool startswith(char *p, char *q) { return memcmp(p, q, strlen(q)) == 0; }

int is_alnum(char c) {
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') ||
         ('0' <= c && c <= '9') || (c == '_');
}

// 入力文字列pをトークナイズしてそれを返す
Token *tokenize() {
  char *p = user_input;
  Token head;
  head.next = NULL;
  Token *cur = &head;

  while (*p) {
    // 空白をスキップ
    if (isspace(*p)) {
      p++;
      continue;
    }

    // 複数文字の比較演算子
    if (startswith(p, "==") || startswith(p, "!=") || startswith(p, "<=") ||
        startswith(p, ">=")) {
      cur = new_token(TK_RESERVED, cur, p, 2);
      p += 2;
      continue;
    }

    bool found = false;
    for (int i = 0; reservedWords[i].kind != TK_EOF; i++) {
      char *w = reservedWords[i].word;
      int len = strlen(w);
      if (startswith(p, w) && !is_alnum(p[len])) {
        cur = new_token(reservedWords[i].kind, cur, p, len);
        p += len;
        found = true;
        break;
      }
    }
    if (found) {
      continue;
    }

    // アルファベット小文字1文字ならTK_IDENT
    if ('a' <= *p && *p <= 'z') {
      char *c = p;
      while (is_alnum(*c)) {
        c++;
      }
      int len = c - p;
      cur = new_token(TK_IDENT, cur, p, len);
      p = c;
      continue;
    }

    // 1文字の演算子
    if (strchr("+-*/()<>=;{},&", *p)) {
      cur = new_token(TK_RESERVED, cur, p, 1);
      p++;
      continue;
    }

    if (isdigit(*p)) {
      cur = new_token(TK_NUM, cur, p, 0);
      char *q = p;
      cur->val = strtol(p, &p, 10);
      cur->len = p - q;
      continue;
    }

    error_at(p, "トークナイズできません。");
  }

  new_token(TK_EOF, cur, p, 0);
  return head.next;
}

// 2項演算子用のNode
Node *new_node(NodeKind kind) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  return node;
}

Node *new_binary(NodeKind kind, Node *lhs, Node *rhs) {
  Node *node = new_node(kind);
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

// 数値を受け取る用のNode
Node *new_num(int val) {
  Node *node = new_node(ND_NUM);
  node->val = val;
  return node;
}

Node *assign() {
  Node *node = equality();
  if (consume("="))
    node = new_binary(ND_ASSIGN, node, assign());
  return node;
}

Node *expr() { return assign(); }

// stmt = expr ";"
//        | "{" stmt "}"
//        |  "if" "(" expr ")" stmt ("else" stmt)?
//        |  "while" "(" expr ")" stmt
//        |  "for" "(" expr? ";" expr? ";" expr? ")" stmt
//        | "int" "*"*? ident ";"
//        |  ...
Node *stmt() {
  Node *node;

  if (consume("{")) {
    node = calloc(1, sizeof(Node));
    node->kind = ND_BLOCK;
    // TODO: とりあえず100行Nodeが入る
    node->block = calloc(100, sizeof(Node));
    for (int i = 0; !consume("}"); i++) {
      node->block[i] = stmt();
    }
    return node;
  }

  if (consume_kind(TK_FOR)) {
    expect("(");
    node = calloc(1, sizeof(Node));
    node->kind = ND_FOR;
    // for文を２つのNodeに分ける
    // for (A; B; C) D;を(A, B) (C, D)に分ける
    Node *left = calloc(1, sizeof(Node));
    left->kind = ND_FOR_LEFT;
    Node *right = calloc(1, sizeof(Node));
    right->kind = ND_FOR_RIGHT;

    // for (A; B; C) D のA,B,Cは省略可能なため
    if (!consume(";")) {
      left->lhs = expr();
      expect(";");
    }
    if (!consume(";")) {
      left->rhs = expr();
      expect(";");
    }
    if (!consume(")")) {
      right->lhs = expr();
      expect(")");
    }
    right->rhs = stmt();
    node->lhs = left;
    node->rhs = right;
    return node;
  }

  if (consume_kind(TK_WHILE)) {
    expect("(");
    node = calloc(1, sizeof(Node));
    node->kind = ND_WHILE;
    node->lhs = expr();
    expect(")");
    node->rhs = stmt();
    return node;
  }

  if (consume_kind(TK_IF)) {
    expect("(");
    node = calloc(1, sizeof(Node));
    node->kind = ND_IF;
    node->lhs = expr();
    expect(")");
    node->rhs = stmt();
    if (consume_kind(TK_ELSE)) {
      Node *els = calloc(1, sizeof(Node));
      els->kind = ND_ELSE;
      els->lhs = node->rhs;
      els->rhs = stmt();
      node->rhs = els;
    }
    return node;
  }

  if (consume_kind(TK_RETURN)) {
    node = calloc(1, sizeof(Node));
    node->kind = ND_RETURN;
    node->lhs = expr();
    expect(";");
    return node;
  }

  if (consume_kind(TK_TYPE)) {
    node = define_variable();
    expect(";");
    return node;
  }

  node = expr();
  expect(";");
  return node;
}

// program = func*
void program() {
  int i = 0;
  while (!at_eof()) {
    code[i++] = func();
  }
  code[i] = NULL;
}

// func = "int" ident "(" ("int" ident ("," "int" "ident")*)?")" stmt
Node *func() {
  // locals[0]は使われないけど、まあいいか
  cur_func++;
  Node *node;
  if (!consume_kind(TK_TYPE)) {
    error("function return type is not defined.");
  }

  Token *tok = (consume_kind(TK_IDENT));
  if (tok == NULL) {
    error("not function!");
  }
  node = calloc(1, sizeof(Node));
  node->kind = ND_FUNC_DEF;
  node->funcname = calloc(100, sizeof(char));
  // 引数の配列の長さ
  node->args = calloc(10, sizeof(char *));
  memcpy(node->funcname, tok->str, tok->len);
  expect("(");

  for (int i = 0; !consume(")"); i++) {
    if (!consume_kind(TK_TYPE)) {
      error("function args type not found.");
    }

    // Token *tok = consume_kind(TK_IDENT);
    // if (tok == NULL) {
    //   error("invalid function args");
    // }
    // if (tok != NULL) {
    // }
    node->args[i] = define_variable();

    if (consume(")")) {
      break;
    }
    expect(",");
  }
  node->lhs = stmt();
  return node;
}

Node *equality() {
  Node *node = relational();

  for (;;) {
    if (consume("=="))
      node = new_binary(ND_EQ, node, relational());
    else if (consume("!="))
      node = new_binary(ND_NE, node, relational());
    else
      return node;
  }
}

Node *relational() {
  Node *node = add();
  for (;;) {
    if (consume("<"))
      node = new_binary(ND_LT, node, add());
    else if (consume("<="))
      node = new_binary(ND_LE, node, add());
    else if (consume(">"))
      node = new_binary(ND_LT, add(), node);
    else if (consume(">="))
      node = new_binary(ND_LE, add(), node);
    else
      return node;
  }
}

Node *add() {
  Node *node = mul();
  for (;;) {
    if (consume("+")) {
      Node *r = mul();
      if (node->type && node->type->ty == PTR) {
        int n = node->type->ptr_to->ty == INT ? 4 : 8;
        r = new_binary(ND_MUL, r, new_num(n));
      }
      node = new_binary(ND_ADD, node, r);
    } else if (consume("-")) {
      Node *r = mul();
      if (node->type && node->type->ty == PTR) {
        int n = node->type->ptr_to->ty == INT ? 4 : 8;
        r = new_binary(ND_MUL, r, new_num(n));
      }
      node = new_binary(ND_SUB, node, r);
    } else {
      return node;
    }
  }
}

Node *mul() {
  Node *node = unary();
  for (;;) {
    if (consume("*"))
      node = new_binary(ND_MUL, node, unary());
    else if (consume("/"))
      node = new_binary(ND_DIV, node, unary());
    else
      return node;
  }
}

// unary = "+"? primary
//         "-"? primary
//         "&"? unary
//         "*"? unary
Node *unary() {
  if (consume("+"))
    return unary();
  if (consume("-"))
    return new_binary(ND_SUB, new_num(0), unary());
  if (consume("&"))
    return new_binary(ND_ADDR, unary(), NULL);
  if (consume("*"))
    return new_binary(ND_DEREF, unary(), NULL);
  return primary();
}

// primary = num
//        |  ident ( "(" expr* ")")?
//        |  "(" expr ")"
Node *primary() {

  // 次のトークンが(なら( expr ) という形になっているはず
  if (consume("(")) {
    Node *node = expr();
    expect(")");
    return node;
  }
  Token *tok = consume_kind(TK_IDENT);

  if (tok) {
    if (consume("(")) {
      // 関数呼び出し
      Node *node = calloc(1, sizeof(Node));
      node->kind = ND_FUNC_CALL;
      node->funcname = calloc(100, sizeof(char));
      memcpy(node->funcname, tok->str, tok->len);
      // TODO: とりあえず引数10個まで
      node->block = calloc(10, sizeof(Node));
      for (int i = 0; !consume(")"); i++) {
        node->block[i] = expr();
        if (consume(")")) {
          break;
        }
        // 引数の間には','がある
        expect(",");
      }
      return node;
    }
    // 関数呼び出しではない場合、変数
    return variable(tok);
  }
  return new_num(expect_number());
}

Node *define_variable() {
  Type *type;
  type = calloc(1, sizeof(Type));
  type->ty = INT;
  type->ptr_to = NULL;

  while (consume("*")) {
    // ポインタを処理する
    Type *t;
    t = calloc(1, sizeof(Type));
    t->ty = PTR;
    t->ptr_to = type;
    type = t;
  }
  Token *tok = consume_kind(TK_IDENT);
  if (tok == NULL) {
    error("invalid define variable");
  }

  Node *node = calloc(1, sizeof(Node));
  node->kind = ND_LVAR;

  LVar *lvar = find_lvar(tok);
  // 変数としてなければErorr
  if (lvar != NULL) {
    // デバッグ用
    char name[100] = {0};
    memcpy(name, tok->str, tok->len);
    error("redefined variable %s\n", name);
  }
  lvar = calloc(1, sizeof(LVar));
  lvar->next = locals[cur_func];
  lvar->name = tok->str;
  lvar->len = tok->len;
  if (locals[cur_func] == NULL) {
    lvar->offset = 8;
  } else {
    lvar->offset = locals[cur_func]->offset + 8;
  }
  lvar->type = type;
  node->offset = lvar->offset;
  node->type = lvar->type;
  locals[cur_func] = lvar;

  return node;
}

Node *variable(Token *tok) {

  Node *node = calloc(1, sizeof(Node));
  node->kind = ND_LVAR;

  LVar *lvar = find_lvar(tok);
  if (lvar == NULL) {
    char name[100] = {0};
    memcpy(name, tok->str, tok->len);
    error("undefined variable %s\n", name);
  }
  node->offset = lvar->offset;
  node->type = lvar->type;

  return node;
}
// そうでなければ数値
