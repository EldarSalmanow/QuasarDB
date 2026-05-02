# Query Language Grammar — QuasarDB

## 1. Notation

This grammar uses Extended Backus–Naur Form (EBNF):

| Notation     | Meaning                       |
|--------------|-------------------------------|
| `::=`        | is defined as                 |
| `\|`         | or (alternation)              |
| `[ A ]`      | A is optional (0 or 1 times)  |
| `{ A }`      | A repeats zero or more times  |
| `( A \| B )` | grouping                      |
| `"text"`     | literal terminal token        |

---

## 2. Lexical Rules

```ebnf
<letter>      ::= "A" | "B" | ... | "Z" | "a" | "b" | ... | "z"
<digit>       ::= "0" | "1" | ... | "9"

<identifier>  ::= ( <letter> | "_" ) { <letter> | <digit> | "_" }

<table_ref>   ::= <identifier> [ "." <identifier> ]
(* Unqualified name resolves against the active database set by USE. *)
(* Qualified form: database_name.table_name                          *)

<string_lit>  ::= '"' { <any_char_except_quote> } '"'
<integer_lit> ::= <digit> { <digit> }
<literal>     ::= <string_lit> | <integer_lit> | "NULL"

<value>       ::= <identifier> | <literal>
(* <value> covers both column references and constants *)

<col_type>    ::= "INT" | "STRING"
```

> **Case sensitivity:** keywords are case-insensitive but must be uniform within a single token — `SELECT`, `select`, and `Select` are all valid; `SeLeCt` is not.

---

## 3. Top-Level Structure

```ebnf
<program>   ::= { <statement> }

<statement> ::= ( <db_stmt> | <ddl_stmt> | <dml_stmt> ) ";"
```

---

## 4. Database Statements

```ebnf
<db_stmt>   ::= <create_db> | <drop_db> | <use_db>

<create_db> ::= "CREATE" "DATABASE" <identifier>
<drop_db>   ::= "DROP"   "DATABASE" <identifier>
<use_db>    ::= "USE"               <identifier>
```

---

## 5. DDL Statements

```ebnf
<ddl_stmt>     ::= <create_table> | <drop_table>

<create_table> ::= "CREATE" "TABLE" <table_ref>
                   "(" <col_def> { "," <col_def> } ")"

<col_def>      ::= <identifier> <col_type>
                   [ "NOT_NULL" ]
                   [ "INDEXED" ]
                   [ "DEFAULT" <literal> ]
(* Modifiers must appear in this exact order: NOT_NULL → INDEXED → DEFAULT *)
(* Examples:                                                                *)
(*   id     INT    NOT_NULL INDEXED DEFAULT 0                               *)
(*   name   STRING NOT_NULL                                                 *)
(*   price  INT    DEFAULT 0                                                *)

<drop_table>   ::= "DROP" "TABLE" <table_ref>
```

---

## 6. DML Statements

```ebnf
<dml_stmt> ::= <insert_stmt>
             | <update_stmt>
             | <delete_stmt>
             | <select_stmt>
```

### 6.1 INSERT

```ebnf
<insert_stmt> ::= "INSERT" "INTO" <table_ref>
                  "(" <identifier> { "," <identifier> } ")"
                  "VALUE" <row_values> { "," <row_values> }

<row_values>  ::= "(" <literal> { "," <literal> } ")"
```

> Columns omitted from the column list receive their `DEFAULT` value if defined, or `NULL` if the column allows it. Omitting a `NOT_NULL` column with no `DEFAULT` is a semantic error (`SEMANTIC_ERROR`).

### 6.2 UPDATE

```ebnf
<update_stmt> ::= "UPDATE" <table_ref>
                  "SET" <assignment> { "," <assignment> }
                  "WHERE" <condition>

<assignment>  ::= <identifier> "=" <value>
```

### 6.3 DELETE

```ebnf
<delete_stmt> ::= "DELETE" "FROM" <table_ref>
                  "WHERE" <condition>
```

### 6.4 SELECT

```ebnf
<select_stmt> ::= "SELECT" <select_list>
                  "FROM" <table_ref>
                  [ "WHERE" <condition> ]

<select_list> ::= "*"
                | <select_item> { "," <select_item> }

<select_item> ::= <aggregate>  [ "AS" <identifier> ]
                | <identifier> [ "AS" <identifier> ]

<aggregate>   ::= ( "SUM" | "COUNT" | "AVG" ) "(" <identifier> ")"
```

---

## 7. Conditions (WHERE clause)

Operator precedence (lowest → highest):

| Level | Operator                        | Associativity |
|-------|---------------------------------|---------------|
| 1     | `OR`                            | left          |
| 2     | `AND`                           | left          |
| 3     | comparison / `BETWEEN` / `LIKE` | —             |

```ebnf
<condition>    ::= <or_expr>

<or_expr>      ::= <and_expr>  { "OR"  <and_expr>  }
<and_expr>     ::= <predicate> { "AND" <predicate> }

<predicate>    ::= "(" <condition> ")"
                 | <between_pred>
                 | <like_pred>
                 | <comparison>

<comparison>   ::= <value> <cmp_op> <value>
<cmp_op>       ::= "==" | "!=" | "<" | ">" | "<=" | ">="

<between_pred> ::= <value> "BETWEEN" <value> "AND" <value>
(* Checks that value is in the closed interval [val2, val3] *)

<like_pred>    ::= <value> "LIKE" <string_lit>
(* string_lit is a regular expression in ECMAScript syntax (std::regex default) *)
(* Applies to STRING columns only                                                *)
```

---

## 8. Examples

```sql
-- Select active database
USE shop;

-- Create a table (modifiers in order: NOT_NULL → INDEXED → DEFAULT)
CREATE TABLE products (
    id    INT    NOT_NULL INDEXED DEFAULT 0,
    name  STRING NOT_NULL,
    price INT    DEFAULT 0
);

-- Qualified reference (no USE required)
CREATE TABLE shop.logs (
    entry STRING NOT_NULL
);

-- Insert multiple rows; price omitted → uses DEFAULT 0
INSERT INTO products (id, name)
VALUE (1, "Apple"),
      (2, "Banana");

-- Update with compound condition and grouping
UPDATE products
SET price = 99
WHERE id == 1 AND (name == "Apple" OR name == "Mango");

-- Delete with BETWEEN
DELETE FROM products
WHERE price BETWEEN 0 AND 10;

-- Select with aliases and aggregates
SELECT name AS product_name,
       AVG(price) AS avg_price,
       COUNT(id)  AS total
FROM products
WHERE price >= 1 AND name LIKE "^A.*";

-- Select all with multi-line condition
SELECT *
FROM shop.products
WHERE (price >= 1 AND price <= 50)
   OR id == 99;
```