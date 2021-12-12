%filenames = "scanner"

 /*
  * Please don't modify the lines above.
  */

 /* You can add lex definitions here. */


%x COMMENT STR IGNORE

%%

 /*
  * Below is examples, which you can wipe out
  * and write regular expressions and actions of your own.
  *
  * All the tokens:
  *   Parser::ID
  *   Parser::STRING
  *   Parser::INT
  *   Parser::COMMA
  *   Parser::COLON
  *   Parser::SEMICOLON
  *   Parser::LPAREN
  *   Parser::RPAREN
  *   Parser::LBRACK
  *   Parser::RBRACK
  *   Parser::LBRACE
  *   Parser::RBRACE
  *   Parser::DOT
  *   Parser::PLUS
  *   Parser::MINUS
  *   Parser::TIMES
  *   Parser::DIVIDE
  *   Parser::EQ
  *   Parser::NEQ
  *   Parser::LT
  *   Parser::LE
  *   Parser::GT
  *   Parser::GE
  *   Parser::AND
  *   Parser::OR
  *   Parser::ASSIGN
  *   Parser::ARRAY
  *   Parser::IF
  *   Parser::THEN
  *   Parser::ELSE
  *   Parser::WHILE
  *   Parser::FOR
  *   Parser::TO
  *   Parser::DO
  *   Parser::LET
  *   Parser::IN
  *   Parser::END
  *   Parser::OF
  *   Parser::BREAK
  *   Parser::NIL
  *   Parser::FUNCTION
  *   Parser::VAR
  *   Parser::TYPE
  */

 /* symbols */
"," {adjust(); return Parser::COMMA;}
":" {adjust(); return Parser::COLON;}
";" {adjust(); return Parser::SEMICOLON;}
"(" {adjust(); return Parser::LPAREN;}
")" {adjust(); return Parser::RPAREN;}
"[" {adjust(); return Parser::LBRACK;}
"]" {adjust(); return Parser::RBRACK;}
"{" {adjust(); return Parser::LBRACE;}
"}" {adjust(); return Parser::RBRACE;}
"." {adjust(); return Parser::DOT;}
"+" {adjust(); return Parser::PLUS;}
"-" {adjust(); return Parser::MINUS;}
"*" {adjust(); return Parser::TIMES;}
"/" {adjust(); return Parser::DIVIDE;}
"=" {adjust(); return Parser::EQ;}
"<>" {adjust(); return Parser::NEQ;}
"<" {adjust(); return Parser::LT;}
"<=" {adjust(); return Parser::LE;}
">" {adjust(); return Parser::GT;}
">=" {adjust(); return Parser::GE;}
"&" {adjust(); return Parser::AND;}
"|" {adjust(); return Parser::OR;}
":=" {adjust(); return Parser::ASSIGN;}


 /* reserved words */
"array" {adjust(); return Parser::ARRAY;}
"if" {adjust(); return Parser::IF;}
"then" {adjust(); return Parser::THEN;}
"else" {adjust(); return Parser::ELSE;}
"while" {adjust(); return Parser::WHILE;}
"for" {adjust(); return Parser::FOR;}
"to" {adjust(); return Parser::TO;}
"do" {adjust(); return Parser::DO;}
"let" {adjust(); return Parser::LET;}
"in" {adjust(); return Parser::IN;}
"end" {adjust(); return Parser::END;}
"of" {adjust(); return Parser::OF;}
"break" {adjust(); return Parser::BREAK;}
"nil" {adjust(); return Parser::NIL;}
"function" {adjust(); return Parser::FUNCTION;}
"var" {adjust(); return Parser::VAR;}
"type" {adjust(); return Parser::TYPE;}

  /* ID and INT */
[[:alpha:]][[:alnum:]_]* {adjust(); return Parser::ID;}
[[:digit:]]+ {adjust(); return Parser::INT;}

 /* string: carried in string_buf_ */
\" {adjust(); string_buf_.clear(); pushCond(StartCondition__::STR);}

<STR> {
  \\n {adjustStr(); string_buf_ += '\n';}
  \\t {adjustStr(); string_buf_ += '\t';}
  \\[[:digit:]]{3} {adjustStr(); string_buf_ += (char)std::stoi(matched().substr(1));}
  \\\" {adjustStr(); string_buf_ += '\"';}
  \\\\ {adjustStr(); string_buf_ += '\\';}
  \\ {adjustStr(); pushCond(StartCondition__::IGNORE);}

  \\\^\@ {adjustStr(); string_buf_ += '\0';}
  \\\^[[:upper:]] {adjustStr(); string_buf_ += (char)(matched()[2] - 'A' + 1);}
  \\\^\[ {adjustStr(); string_buf_ += '\e';}
  
  \" {adjustStr(); popCond(); setMatched(string_buf_); return Parser::STRING;}
  . {adjustStr(); string_buf_ += matched();}
}

 /* ignore some sentence in STR */
<IGNORE> {
  \\ {adjustStr(); popCond();}
  .|\n {adjustStr();}
}

 /* comment */
"/*" {adjust(); pushCond(StartCondition__::COMMENT);}

<COMMENT> {
  "/*" {adjustStr(); pushCond(StartCondition__::COMMENT);}
  "*/" {adjustStr(); popCond();}
  .|\n {adjustStr();}
}

  /* unclosed String or Comment*/
<STR><<EOF>> {adjust(); errormsg_->Error(errormsg_->tok_pos_, "unclosed string");}
<COMMENT><<EOF>> {adjust(); errormsg_->Error(errormsg_->tok_pos_, "unclosed comment");}
<IGNORE><<EOF>> {adjust(); errormsg_->Error(errormsg_->tok_pos_, "unclosed ignore segment");}

 /*
  * skip white space chars.
  * space, tabs and LF
  */
[ \t]+ {adjust();}
\n {adjust(); errormsg_->Newline();}

 /* illegal input */
. {adjust(); errormsg_->Error(errormsg_->tok_pos_, "illegal token");}
