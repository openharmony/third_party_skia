//
// Copyright (c) 2025 Huawei Device Co., Ltd.
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// *****************
// *** IMPORTANT ***
// *****************
//
// 1. This file is only used when gn arg sksl_lex is set to true. It is used to regenerate the
//    SkSLLexer.h and SkSLLexer.cpp files.
// 2. Since token IDs are used to identify operators and baked into the .dehydrated.sksl files,
//    after modifying this file it is likely everything will break until you update the dehydrated
//    binaries. If things break after updating the lexer, set REHYDRATE in SkSLCompiler.cpp to 0,
//    rebuild, and then set it back to 1.

FLOAT_LITERAL  = [0-9]*\.[0-9]+([eE][+-]?[0-9]+)?|[0-9]+\.[0-9]*([eE][+-]?[0-9]+)?|[0-9]+([eE][+-]?[0-9]+)
INT_LITERAL    = [0-9]+|0[xX][0-9a-fA-F]+
TRUE_LITERAL   = "true"
FALSE_LITERAL  = "false"
IF             = "if"
STATIC_IF      = "@if"
ELSE           = "else"
FOR            = "for"
WHILE          = "while"
DO             = "do"
SWITCH         = "switch"
STATIC_SWITCH  = "@switch"
CASE           = "case"
DEFAULT        = "default"
BREAK          = "break"
CONTINUE       = "continue"
DISCARD        = "discard"
RETURN         = "return"
IN             = "in"
OUT            = "out"
INOUT          = "inout"
UNIFORM        = "uniform"
CONST          = "const"
FLAT           = "flat"
NOPERSPECTIVE  = "noperspective"
INLINE         = "inline"
NOINLINE       = "noinline"
HASSIDEEFFECTS = "sk_has_side_effects"
READONLY       = "readonly"
WRITEONLY      = "writeonly"
BUFFER         = "buffer"
STRUCT         = "struct"
LAYOUT         = "layout"
HIGHP          = "highp"
MEDIUMP        = "mediump"
LOWP           = "lowp"
ES3            = "$es3"
RESERVED       = attribute|varying|precision|invariant|asm|class|union|enum|typedef|template|this|packed|goto|volatile|public|static|extern|external|interface|long|double|fixed|unsigned|superp|input|output|hvec[234]|dvec[234]|fvec[234]|sampler[12]DShadow|sampler3DRect|sampler2DRectShadow|samplerCube|sizeof|cast|namespace|using|gl_[0-9a-zA-Z_$]*
IDENTIFIER     = [a-zA-Z_$][0-9a-zA-Z_$]*
DIRECTIVE      = #[a-zA-Z_$][0-9a-zA-Z_$]*
LPAREN         = "("
RPAREN         = ")"
LBRACE         = "{"
RBRACE         = "}"
LBRACKET       = "["
RBRACKET       = "]"
DOT            = "."
COMMA          = ","
PLUSPLUS       = "++"
MINUSMINUS     = "--"
PLUS           = "+"
MINUS          = "-"
STAR           = "*"
SLASH          = "/"
PERCENT        = "%"
SHL            = "<<"
SHR            = ">>"
BITWISEOR      = "|"
BITWISEXOR     = "^"
BITWISEAND     = "&"
BITWISENOT     = "~"
LOGICALOR      = "||"
LOGICALXOR     = "^^"
LOGICALAND     = "&&"
LOGICALNOT     = "!"
QUESTION       = "?"
COLON          = ":"
EQ             = "="
EQEQ           = "=="
NEQ            = "!="
GT             = ">"
LT             = "<"
GTEQ           = ">="
LTEQ           = "<="
PLUSEQ         = "+="
MINUSEQ        = "-="
STAREQ         = "*="
SLASHEQ        = "/="
PERCENTEQ      = "%="
SHLEQ          = "<<="
SHREQ          = ">>="
BITWISEOREQ    = "|="
BITWISEXOREQ   = "^="
BITWISEANDEQ   = "&="
SEMICOLON      = ";"
ARROW          = "->"
WHITESPACE     = \s+
LINE_COMMENT   = //.*
BLOCK_COMMENT  = /\*([^*]|\*[^/])*\*/
INVALID        = .
