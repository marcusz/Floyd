//
//  main.cpp
//  FloydSpeak
//
//  Created by Marcus Zetterquist on 27/03/16.
//  Copyright © 2016 Marcus Zetterquist. All rights reserved.
//

#define QUARK_ASSERT_ON true
#define QUARK_TRACE_ON true
#define QUARK_UNIT_TESTS_ON true

#include "quark.h"
#include "steady_vector.h"
#include <string>
#include <map>
#include <iostream>

using std::vector;
using std::string;
using std::pair;


const string kProgram1 =
"int main(string args){\n"
"	log(args);\n"
"	return 3;\n"
"}\n";

typedef pair<string, string> seq;



const char* basic_types[] = {
	"bool",
	"char???",
	"code_point",
	"double",
	"float",
	"float32",
	"float80",
	"hash",
	"int",
	"int16",
	"int32",
	"int64",
	"int8",
	"path",
	"string",
	"text"
};

const char* _advanced_types[] = {
	"clock",
	"defect_exception",
	"dyn",
	"dyn**<>",
	"enum",
	"exception",
	"map",
	"protocol",
	"rights",
	"runtime_exception",
	"seq",
	"struct",
	"typedef",
	"vector"
};

const char* _keywords[] = {
	"assert",
	"catch",
	"deserialize()",
	"diff()",
	"else",
	"ensure",
	"false",
	"foreach",
	"hash()",
	"if",
	"invariant",
	"log",
	"mutable",
	"namespace???",
	"null",
	"private",
	"property",
	"prove",
	"require",
	"return",
	"serialize()",
	"swap",
	"switch",
	"tag",
	"test",
	"this",
	"true",
	"try",
	"typecast",
	"typeof",
	"while"
};

const string whitespace_chars = " \n\t";
const string identifier_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";
const string brackets = "(){}[]<>";
const string open_brackets = "({[<";








bool is_whitespace(char c){
	return c == ' ' || c == '\t' || c == '\n';
}

/*
	example
		input " \t\tint blob ()"
		result "int", " blob ()"
	example
		input "test()"
		result "test", "()"
*/

string skip_whitespace(const string& s){
	size_t pos = 0;
	while(pos < s.size() && is_whitespace(s[pos])){
		pos++;
	}
	return s.substr(pos);
}


seq get_next_token(const string& s, string first_skip, string then_use){
	size_t pos = 0;
	while(pos < s.size() && first_skip.find(s[pos]) != string::npos){
		pos++;
	}

	size_t pos2 = pos;
	while(pos2 < s.size() && then_use.find(s[pos2]) != string::npos){
		pos2++;
	}

	return seq(
		s.substr(pos, pos2 - pos),
		s.substr(pos2)
	);
}

bool is_start_char(char c){
	return c == '(' || c == '[' || c == '{' || c == '<';
}

QUARK_UNIT_TEST("", "is_start_char()", "", ""){
	QUARK_TEST_VERIFY(is_start_char('('));
	QUARK_TEST_VERIFY(is_start_char('['));
	QUARK_TEST_VERIFY(is_start_char('{'));
	QUARK_TEST_VERIFY(is_start_char('<'));

	QUARK_TEST_VERIFY(!is_start_char(')'));
	QUARK_TEST_VERIFY(!is_start_char(']'));
	QUARK_TEST_VERIFY(!is_start_char('}'));
	QUARK_TEST_VERIFY(!is_start_char('>'));

	QUARK_TEST_VERIFY(!is_start_char(' '));
}

bool is_end_char(char c){
	return c == ')' || c == ']' || c == '}' || c == '>';
}

QUARK_UNIT_TEST("", "is_end_char()", "", ""){
	QUARK_TEST_VERIFY(!is_end_char('('));
	QUARK_TEST_VERIFY(!is_end_char('['));
	QUARK_TEST_VERIFY(!is_end_char('{'));
	QUARK_TEST_VERIFY(!is_end_char('<'));

	QUARK_TEST_VERIFY(is_end_char(')'));
	QUARK_TEST_VERIFY(is_end_char(']'));
	QUARK_TEST_VERIFY(is_end_char('}'));
	QUARK_TEST_VERIFY(is_end_char('>'));

	QUARK_TEST_VERIFY(!is_end_char(' '));
}


char start_char_to_end_char(char start_char){
	QUARK_ASSERT(is_start_char(start_char));

	if(start_char == '('){
		return ')';
	}
	else if(start_char == '['){
		return ']';
	}
	else if(start_char == '{'){
		return '}';
	}
	else if(start_char == '<'){
		return '>';
	}
	else {
		QUARK_ASSERT(false);
	}
}

QUARK_UNIT_TEST("", "start_char_to_end_char()", "", ""){
	QUARK_TEST_VERIFY(start_char_to_end_char('(') == ')');
	QUARK_TEST_VERIFY(start_char_to_end_char('[') == ']');
	QUARK_TEST_VERIFY(start_char_to_end_char('{') == '}');
	QUARK_TEST_VERIFY(start_char_to_end_char('<') == '>');
}

/*
	First char is the start char, like '(' or '{'.
*/

seq get_balanced(const string& s){
	QUARK_ASSERT(s.size() > 0);

	const char start_char = s[0];
	QUARK_ASSERT(is_start_char(start_char));

	const char end_char = start_char_to_end_char(start_char);

	int depth = 0;
	size_t pos = 0;
	while(pos < s.size() && !(depth == 1 && s[pos] == end_char)){
		const char c = s[pos];
		if(is_start_char(c)) {
			depth++;
		}
		else if(is_end_char(c)){
			if(depth == 0){
				throw std::runtime_error("unbalanced ([{< >}])");
			}
			depth--;
		}
		pos++;
	}

	return seq(
		s.substr(0, pos + 1),
		s.substr(pos + 1)
	);
}

QUARK_UNIT_TEST("", "get_balanced()", "", ""){
//	QUARK_TEST_VERIFY(get_balanced("") == seq("", ""));
	QUARK_TEST_VERIFY(get_balanced("()") == seq("()", ""));
	QUARK_TEST_VERIFY(get_balanced("(abc)") == seq("(abc)", ""));
	QUARK_TEST_VERIFY(get_balanced("(abc)def") == seq("(abc)", "def"));
	QUARK_TEST_VERIFY(get_balanced("((abc))def") == seq("((abc))", "def"));
	QUARK_TEST_VERIFY(get_balanced("((abc)[])def") == seq("((abc)[])", "def"));
}





struct expression {
};

/*
*/
enum StatementType {
	return_expression,
	if_else_node__bodies,
	for_expression__body,
	foreach_expression__body,
	while_expression__body,
	let_expression,
	mut_expression,
	assert_expression,
};

struct Statement {
	StatementType _type;
	
};

/*
	Functions always return a value.
*/
struct function_body {
	vector<Statement> _root_statements;
};


struct function_def {
	string return_type;
	vector<pair<string, string>> _args;
	function_body _body;
};




struct pass1 {
	std::map<string, function_def> _functions;
};


pass1 compiler_pass1(string program){
	const auto start = seq("", program);

	const seq t1 = get_next_token(program, whitespace_chars, identifier_chars);

	string type1;
	if(t1.first == "int" || t1.first == "bool" || t1.first == "string"){
		type1 = t1.first;
	}
	else {
		throw std::runtime_error("expected type");
	}

	const seq t2 = get_next_token(t1.second, whitespace_chars, identifier_chars);
	const string identifier = t2.first;

	const seq t3 = get_next_token(t2.second, whitespace_chars, "");

	//	Is this a function definition?
	if(t3.second.size() > 0 && t3.second[0] == '('){
		const auto t4 = get_balanced(t3.second);

		const auto function_return = type1;
		const auto function_args = std::string("(") + t4.first;
		QUARK_TRACE("");
	}
	else{
		throw std::runtime_error("expected (");
	}

	return pass1();
}

string emit_c_code(){
	return
		"int main(int argc, char *argv[]){\n"
		"	return 0;\n"
		"}\n";
}

QUARK_UNIT_TEST("", "compiler_pass1()", "", ""){
	QUARK_TEST_VERIFY(compiler_pass1(kProgram1)._functions.size() == 0);
}



int main(int argc, const char * argv[]) {
	try {
#if QUARK_UNIT_TESTS_ON
		quark::run_tests();
#endif
	}
	catch(...){
		QUARK_TRACE("Error");
		return -1;
	}

  return 0;
}
