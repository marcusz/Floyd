//
//  compiler_basics.cpp
//  Floyd
//
//  Created by Marcus Zetterquist on 2019-02-05.
//  Copyright © 2019 Marcus Zetterquist. All rights reserved.
//

#include "types.h"

#include "parser_primitives.h"
#include "json_support.h"
#include "utils.h"
#include "ast_helpers.h"
#include "json_support.h"
#include "format_table.h"
#include "compiler_basics.h"
#include "parser_primitives.h"

#include <limits.h>
#include <set>




namespace floyd {

static const type_node_t& lookup_typeinfo_from_type(const types_t& types, const type_t& type);
static type_node_t& lookup_typeinfo_from_type(types_t& types, const type_t& type);
static type_t lookup_type_from_index_it(const types_t& types, size_t type_index);




////////////////////////////////	base_type


std::string base_type_to_opcode(const base_type t){
	if(t == base_type::k_undefined){
		return "undef";
	}
	else if(t == base_type::k_any){
		return "any";
	}

	else if(t == base_type::k_void){
		return "void";
	}
	else if(t == base_type::k_bool){
		return "bool";
	}
	else if(t == base_type::k_int){
		return "int";
	}
	else if(t == base_type::k_double){
		return "double";
	}
	else if(t == base_type::k_string){
		return "string";
	}
	else if(t == base_type::k_json){
		return "json";
	}

	else if(t == base_type::k_typeid){
		return "typeid";
	}

	else if(t == base_type::k_struct){
		return "struct";
	}
	else if(t == base_type::k_vector){
		return "vector";
	}
	else if(t == base_type::k_dict){
		return "dict";
	}
	else if(t == base_type::k_function){
		return "func";
	}
	else if(t == base_type::k_symbol_ref){
		return "symbol-ref";
	}
	else if(t == base_type::k_named_type){
		return "named-type";
	}
	else{
		QUARK_ASSERT(false);
		quark::throw_exception();
	}
	return "**impossible**";
}

QUARK_TEST("", "base_type_to_opcode(base_type)", "", ""){
	QUARK_VERIFY(base_type_to_opcode(base_type::k_undefined) == "undef");
}
QUARK_TEST("", "base_type_to_opcode(base_type)", "", ""){
	QUARK_VERIFY(base_type_to_opcode(base_type::k_any) == "any");
}
QUARK_TEST("", "base_type_to_opcode(base_type)", "", ""){
	QUARK_VERIFY(base_type_to_opcode(base_type::k_void) == "void");
}

QUARK_TEST("", "base_type_to_opcode(base_type)", "", ""){
	QUARK_VERIFY(base_type_to_opcode(base_type::k_bool) == "bool");
}
QUARK_TEST("", "base_type_to_opcode(base_type)", "", ""){
	QUARK_VERIFY(base_type_to_opcode(base_type::k_int) == "int");
}
QUARK_TEST("", "base_type_to_opcode(base_type)", "", ""){
	QUARK_VERIFY(base_type_to_opcode(base_type::k_double) == "double");
}
QUARK_TEST("", "base_type_to_opcode(base_type)", "", ""){
	QUARK_VERIFY(base_type_to_opcode(base_type::k_string) == "string");
}
QUARK_TEST("", "base_type_to_opcode(base_type)", "", ""){
	QUARK_VERIFY(base_type_to_opcode(base_type::k_json) == "json");
}

QUARK_TEST("", "base_type_to_opcode(base_type)", "", ""){
	QUARK_VERIFY(base_type_to_opcode(base_type::k_typeid) == "typeid");
}

QUARK_TEST("", "base_type_to_opcode(base_type)", "", ""){
	QUARK_VERIFY(base_type_to_opcode(base_type::k_struct) == "struct");
}
QUARK_TEST("", "base_type_to_opcode(base_type)", "", ""){
	QUARK_VERIFY(base_type_to_opcode(base_type::k_vector) == "vector");
}
QUARK_TEST("", "base_type_to_opcode(base_type)", "", ""){
	QUARK_VERIFY(base_type_to_opcode(base_type::k_dict) == "dict");
}
QUARK_TEST("", "base_type_to_opcode(base_type)", "", ""){
	QUARK_VERIFY(base_type_to_opcode(base_type::k_function) == "func");
}
QUARK_TEST("", "base_type_to_opcode(base_type)", "", ""){
	QUARK_VERIFY(base_type_to_opcode(base_type::k_symbol_ref) == "symbol-ref");
}
QUARK_TEST("", "base_type_to_opcode(base_type)", "", ""){
	QUARK_VERIFY(base_type_to_opcode(base_type::k_named_type) == "named-type");
}


base_type opcode_to_base_type(const std::string& s){
	QUARK_ASSERT(s != "");

	if(s == "undef"){
		return base_type::k_undefined;
	}
	else if(s == "any"){
		return base_type::k_any;
	}
	else if(s == "void"){
		return base_type::k_void;
	}
	else if(s == "bool"){
		return base_type::k_bool;
	}
	else if(s == "int"){
		return base_type::k_int;
	}
	else if(s == "double"){
		return base_type::k_double;
	}
	else if(s == "string"){
		return base_type::k_string;
	}
	else if(s == "typeid"){
		return base_type::k_typeid;
	}
	else if(s == "json"){
		return base_type::k_json;
	}

	else if(s == "struct"){
		return base_type::k_struct;
	}
	else if(s == "vector"){
		return base_type::k_vector;
	}
	else if(s == "dict"){
		return base_type::k_dict;
	}
	else if(s == "func"){
		return base_type::k_function;
	}
	else if(s == "symbol"){
		return base_type::k_symbol_ref;
	}
	else if(s == "named-type"){
		return base_type::k_named_type;
	}

	else{
		QUARK_ASSERT(false);
	}
	return base_type::k_undefined;
}


void ut_verify_basetype(const quark::call_context_t& context, const base_type& result, const base_type& expected){
	ut_verify_string(
		context,
		base_type_to_opcode(result),
		base_type_to_opcode(expected)
	);
}


////////////////////////////////	get_json_type()


int get_json_type(const json_t& value){
	QUARK_ASSERT(value.check_invariant());

	if(value.is_object()){
		return 1;
	}
	else if(value.is_array()){
		return 2;
	}
	else if(value.is_string()){
		return 3;
	}
	else if(value.is_number()){
		return 4;
	}
	else if(value.is_true()){
		return 5;
	}
	else if(value.is_false()){
		return 6;
	}
	else if(value.is_null()){
		return 7;
	}
	else{
		QUARK_ASSERT(false);
		quark::throw_exception();
	}
	return 0;
}


////////////////////////////////	function type



////////////////////////////////	type_name_t


std::string pack_type_name(const type_name_t& tag){
	if(tag.lexical_path.empty()){
		return "/";
	}
	else{
		std::string acc;
		for(const auto& e: tag.lexical_path){
			acc = acc + "/" + e;
		}
		return acc;
	}
}

QUARK_TEST("", "pack_type_name()", "", ""){
	QUARK_VERIFY(pack_type_name(type_name_t{{ "hello" }} ) == "/hello");
}
QUARK_TEST("", "pack_type_name()", "", ""){
	QUARK_VERIFY(pack_type_name(type_name_t{{ "hello", "goodbye" }} ) == "/hello/goodbye");
}
QUARK_TEST("", "pack_type_name()", "", ""){
	QUARK_VERIFY(pack_type_name(type_name_t{{ "" }} ) == "/");
}


bool is_type_name(const std::string& s){
	return s.size() > 0 && s[0] == '/';
}

QUARK_TEST("", "is_type_name()", "", ""){
	QUARK_VERIFY(is_type_name("hello") == false);
}
QUARK_TEST("", "is_type_name()", "", ""){
	QUARK_VERIFY(is_type_name("/hello") == true);
}
QUARK_TEST("", "is_type_name()", "", ""){
	QUARK_VERIFY(is_type_name("/hello/goodbye") == true);
}


type_name_t unpack_type_name(const std::string& tag){
	QUARK_ASSERT(is_type_name(tag));

	if(tag == "/"){
		return type_name_t { } ;
	}
	else{
		std::vector<std::string> path;
		size_t pos = 0;
		while(pos != tag.size()){
			const auto next_pos = tag.find("/", pos + 1);
			const auto next_pos2 = next_pos == std::string::npos ? tag.size() : next_pos;
			const auto s = tag.substr(pos + 1, next_pos2 - 1);
			path.push_back(s);
			pos = next_pos2;
		}
		return type_name_t { path } ;
	}
}

QUARK_TEST("", "pack_type_name()", "", ""){
	QUARK_VERIFY(unpack_type_name("/hello") == type_name_t{{ "hello" }} );
}
QUARK_TEST("", "pack_type_name()", "", ""){
	QUARK_VERIFY(unpack_type_name("/hello/goodbye") == ( type_name_t{{ "hello", "goodbye" }} ) );
}
QUARK_TEST("", "pack_type_name()", "", ""){
	QUARK_VERIFY(unpack_type_name("/") == type_name_t{{ }} );
}


type_name_t make_empty_type_name(){
	return type_name_t { };
}



//////////////////////////////////////////////////		type_t


type_t::type_t(const type_desc_t& desc) :
	type_t(desc.non_name_type)
{
}

#if 0
QUARK_TESTQ("type_t", "make_undefined()"){
	ut_verify_type(QUARK_POS, type_t::make_undefined().get_base_type(), base_type::k_undefined);
}
QUARK_TESTQ("type_t", "is_undefined()"){
	ut_verify_auto(QUARK_POS, type_t::make_undefined().is_undefined(), true);
}
QUARK_TESTQ("type_t", "is_undefined()"){
	ut_verify_auto(QUARK_POS, type_t::make_bool().is_undefined(), false);
}


QUARK_TESTQ("type_t", "make_any()"){
	ut_verify_type(QUARK_POS, type_t::make_any().get_base_type(), base_type::k_any);
}
QUARK_TESTQ("type_t", "is_any()"){
	ut_verify_auto(QUARK_POS, type_t::make_any().is_any(), true);
}
QUARK_TESTQ("type_t", "is_any()"){
	ut_verify_auto(QUARK_POS, type_t::make_bool().is_any(), false);
}


QUARK_TESTQ("type_t", "make_void()"){
	ut_verify_type(QUARK_POS, type_t::make_void().get_base_type(), base_type::k_void);
}
QUARK_TESTQ("type_t", "is_void()"){
	ut_verify_auto(QUARK_POS, type_t::make_void().is_void(), true);
}
QUARK_TESTQ("type_t", "is_void()"){
	ut_verify_auto(QUARK_POS, type_t::make_bool().is_void(), false);
}


QUARK_TESTQ("type_t", "make_bool()"){
	ut_verify_type(QUARK_POS, type_t::make_bool().get_base_type(), base_type::k_bool);
}
QUARK_TESTQ("type_t", "is_bool()"){
	ut_verify_auto(QUARK_POS, type_t::make_bool().is_bool(), true);
}
QUARK_TESTQ("type_t", "is_bool()"){
	ut_verify_auto(QUARK_POS, type_t::make_void().is_bool(), false);
}


QUARK_TESTQ("type_t", "make_int()"){
	ut_verify_type(QUARK_POS, type_t::make_int().get_base_type(), base_type::k_int);
}
QUARK_TESTQ("type_t", "is_int()"){
	QUARK_VERIFY(type_t::make_int().is_int() == true);
}
QUARK_TESTQ("type_t", "is_int()"){
	QUARK_VERIFY(type_t::make_bool().is_int() == false);
}


QUARK_TESTQ("type_t", "make_double()"){
	QUARK_VERIFY(type_t::make_double().is_double());
}
QUARK_TESTQ("type_t", "is_double()"){
	QUARK_VERIFY(type_t::make_double().is_double() == true);
}
QUARK_TESTQ("type_t", "is_double()"){
	QUARK_VERIFY(type_t::make_bool().is_double() == false);
}


QUARK_TESTQ("type_t", "make_string()"){
	QUARK_VERIFY(type_t::make_string().get_base_type() == base_type::k_string);
}
QUARK_TESTQ("type_t", "is_string()"){
	QUARK_VERIFY(type_t::make_string().is_string() == true);
}
QUARK_TESTQ("type_t", "is_string()"){
	QUARK_VERIFY(type_t::make_bool().is_string() == false);
}


QUARK_TESTQ("type_t", "make_json()"){
	QUARK_VERIFY(type_t::make_json().get_base_type() == base_type::k_json);
}
QUARK_TESTQ("type_t", "is_json()"){
	QUARK_VERIFY(type_t::make_json().is_json() == true);
}
QUARK_TESTQ("type_t", "is_json()"){
	QUARK_VERIFY(type_t::make_bool().is_json() == false);
}


QUARK_TESTQ("type_t", "make_typeid()"){
	QUARK_VERIFY(type_t::make_typeid().get_base_type() == base_type::k_typeid);
}
QUARK_TESTQ("type_t", "is_typeid()"){
	QUARK_VERIFY(type_t::make_typeid().is_typeid() == true);
}
QUARK_TESTQ("type_t", "is_typeid()"){
	QUARK_VERIFY(type_t::make_bool().is_typeid() == false);
}


type_t make_empty_struct(){
	return make_struct({});
}

const auto k_struct_test_members_b = std::vector<member_t>({
	{ type_t::make_int(), "x" },
	{ type_t::make_string(), "y" },
	{ type_t::make_bool(), "z" }
});

type_t make_test_struct_a(){
	return type_t::make_struct2(k_struct_test_members_b);
}

QUARK_TESTQ("type_t", "make_struct2()"){
	QUARK_VERIFY(type_t::make_struct2({}).get_base_type() == base_type::k_struct);
}
QUARK_TESTQ("type_t", "make_struct2()"){
	QUARK_VERIFY(make_empty_struct().get_base_type() == base_type::k_struct);
}
QUARK_TESTQ("type_t", "is_struct()"){
	QUARK_VERIFY(make_empty_struct().is_struct() == true);
}
QUARK_TESTQ("type_t", "is_struct()"){
	QUARK_VERIFY(type_t::make_bool().is_struct() == false);
}

QUARK_TESTQ("type_t", "make_struct2()"){
	const auto t = make_test_struct_a();
	QUARK_VERIFY(t.get_struct() == k_struct_test_members_b);
}
QUARK_TESTQ("type_t", "get_struct()"){
	const auto t = make_test_struct_a();
	QUARK_VERIFY(t.get_struct() == k_struct_test_members_b);
}
QUARK_TESTQ("type_t", "get_struct_ref()"){
	const auto t = make_test_struct_a();
	QUARK_VERIFY(t.get_struct_ref()->_members == k_struct_test_members_b);
}



QUARK_TESTQ("type_t", "make_vector()"){
	QUARK_VERIFY(type_t::make_vector(type_t::make_int()).get_base_type() == base_type::k_vector);
}
QUARK_TESTQ("type_t", "is_vector()"){
	QUARK_VERIFY(type_t::make_vector(type_t::make_int()).is_vector() == true);
}
QUARK_TESTQ("type_t", "is_vector()"){
	QUARK_VERIFY(type_t::make_bool().is_vector() == false);
}
QUARK_TESTQ("type_t", "get_vector_element_type()"){
	QUARK_VERIFY(type_t::make_vector(type_t::make_int()).get_vector_element_type().is_int());
}
QUARK_TESTQ("type_t", "get_vector_element_type()"){
	QUARK_VERIFY(type_t::make_vector(type_t::make_string()).get_vector_element_type().is_string());
}


QUARK_TESTQ("type_t", "make_dict()"){
	QUARK_VERIFY(type_t::make_dict(type_t::make_int()).get_base_type() == base_type::k_dict);
}
QUARK_TESTQ("type_t", "is_dict()"){
	QUARK_VERIFY(type_t::make_dict(type_t::make_int()).is_dict() == true);
}
QUARK_TESTQ("type_t", "is_dict()"){
	QUARK_VERIFY(type_t::make_bool().is_dict() == false);
}
QUARK_TESTQ("type_t", "get_dict_value_type()"){
	QUARK_VERIFY(type_t::make_dict(type_t::make_int()).get_dict_value_type().is_int());
}
QUARK_TESTQ("type_t", "get_dict_value_type()"){
	QUARK_VERIFY(type_t::make_dict(type_t::make_string()).get_dict_value_type().is_string());
}



const auto k_test_function_args_a = std::vector<type_t>({
	{ type_t::make_int() },
	{ type_t::make_string() },
	{ type_t::make_bool() }
});

QUARK_TESTQ("type_t", "make_function()"){
	const auto t = make_function(type_t::make_void(), {}, epure::pure);
	QUARK_VERIFY(t.get_base_type() == base_type::k_function);
}
QUARK_TESTQ("type_t", "is_function()"){
	const auto t = make_function(type_t::make_void(), {}, epure::pure);
	QUARK_VERIFY(t.is_function() == true);
}
QUARK_TESTQ("type_t", "is_function()"){
	QUARK_VERIFY(type_t::make_bool().is_function() == false);
}
QUARK_TESTQ("type_t", "get_function_return()"){
	const auto t = make_function(type_t::make_void(), {}, epure::pure);
	QUARK_VERIFY(t.get_function_return().is_void());
}
QUARK_TESTQ("type_t", "get_function_return()"){
	const auto t = make_function(type_t::make_string(), {}, epure::pure);
	QUARK_VERIFY(t.get_function_return().is_string());
}
QUARK_TESTQ("type_t", "get_function_args()"){
	const auto t = make_function(type_t::make_void(), k_test_function_args_a, epure::pure);
	QUARK_VERIFY(t.get_function_args() == k_test_function_args_a);
}


QUARK_TESTQ("type_t", "is_identifier()"){
	const auto t = type_t::make_identifier("xyz");
	QUARK_VERIFY(t.is_identifier() == true);
}
QUARK_TESTQ("type_t", "is_identifier()"){
	QUARK_VERIFY(type_t::make_bool().is_identifier() == false);
}

QUARK_TESTQ("type_t", "get_unresolved()"){
	const auto t = type_t::make_identifier("xyz");
	QUARK_VERIFY(t.get_identifier() == "xyz");
}
QUARK_TESTQ("type_t", "get_unresolved()"){
	const auto t = type_t::make_identifier("123");
	QUARK_VERIFY(t.get_identifier() == "123");
}


QUARK_TESTQ("type_t", "operator==()"){
	const auto a = type_t::make_vector(type_t::make_int());
	const auto b = type_t::make_vector(type_t::make_int());
	QUARK_VERIFY(a == b);
}
QUARK_TESTQ("type_t", "operator==()"){
	const auto a = type_t::make_vector(type_t::make_int());
	const auto b = type_t::make_dict(type_t::make_int());
	QUARK_VERIFY((a == b) == false);
}
QUARK_TESTQ("type_t", "operator==()"){
	const auto a = type_t::make_vector(type_t::make_int());
	const auto b = type_t::make_vector(type_t::make_string());
	QUARK_VERIFY((a == b) == false);
}


QUARK_TESTQ("type_t", "operator=()"){
	const auto a = type_t::make_bool();
	const auto b = a;
	QUARK_VERIFY(a == b);
}
QUARK_TESTQ("type_t", "operator=()"){
	const auto a = type_t::make_vector(type_t::make_int());
	const auto b = a;
	QUARK_VERIFY(a == b);
}
QUARK_TESTQ("type_t", "operator=()"){
	const auto a = type_t::make_dict(type_t::make_int());
	const auto b = a;
	QUARK_VERIFY(a == b);
}
QUARK_TESTQ("type_t", "operator=()"){
	const auto a = make_function(type_t::make_string(), { type_t::make_int(), type_t::make_double() }, epure::pure);
	const auto b = a;
	QUARK_VERIFY(a == b);
}

#endif


std::string type_t::get_symbol_ref(const types_t& types) const {
	QUARK_ASSERT(check_invariant());
	QUARK_ASSERT(is_symbol_ref());

	const auto& info = lookup_typeinfo_from_type(types, *this);
	return info.def.symbol_identifier;
}


type_name_t type_t::get_named_type(const types_t& types) const {
	QUARK_ASSERT(check_invariant());
	QUARK_ASSERT(is_named_type());

	const auto& info = lookup_typeinfo_from_type(types, *this);
	return info.optional_name;
}



/////////////////////////////////////////////////		type_desc_t



static struct_type_desc_t make_struct_desc(const type_node_t& node){
	QUARK_ASSERT(node.check_invariant());
	QUARK_ASSERT(node.def.bt == base_type::k_struct);

	QUARK_ASSERT(node.def.child_types.size() == node.def.names.size());
	std::vector<member_t> members;
	for(int i = 0 ; i < node.def.child_types.size() ; i++){
		const auto m = member_t(node.def.child_types[i], node.def.names[i]);
		members.push_back(m);
	}

	return struct_type_desc_t { members };
}


struct_type_desc_t type_desc_t::get_struct(const types_t& types) const{
	QUARK_ASSERT(check_invariant());
	QUARK_ASSERT(types.check_invariant());
	QUARK_ASSERT(is_struct());

	const auto& info = lookup_typeinfo_from_type(types, *this);
	return make_struct_desc(info);
//	return info.struct_desc;
}

type_t type_desc_t::get_vector_element_type(const types_t& types) const{
	QUARK_ASSERT(check_invariant());
	QUARK_ASSERT(types.check_invariant());
	QUARK_ASSERT(is_vector());

	const auto& info = lookup_typeinfo_from_type(types, non_name_type);
	return info.def.child_types[0];
}

type_t type_desc_t::get_dict_value_type(const types_t& types) const{
	QUARK_ASSERT(check_invariant());
	QUARK_ASSERT(types.check_invariant());
	QUARK_ASSERT(is_dict());

	const auto& info = lookup_typeinfo_from_type(types, *this);
	return info.def.child_types[0];
}


type_t type_desc_t::get_function_return(const types_t& types) const{
	QUARK_ASSERT(check_invariant());
	QUARK_ASSERT(is_function());
	QUARK_ASSERT(types.check_invariant());

	const auto& info = lookup_typeinfo_from_type(types, *this);
	return info.def.child_types[0];
}

std::vector<type_t> type_desc_t::get_function_args(const types_t& types) const{
	QUARK_ASSERT(check_invariant());
	QUARK_ASSERT(is_function());
	QUARK_ASSERT(types.check_invariant());

	const auto& info = lookup_typeinfo_from_type(types, *this);
	return std::vector<type_t>(info.def.child_types.begin() + 1, info.def.child_types.end());
}

return_dyn_type type_desc_t::get_function_dyn_return_type(const types_t& types) const{
	QUARK_ASSERT(check_invariant());
	QUARK_ASSERT(is_function());
	QUARK_ASSERT(types.check_invariant());

	const auto& info = lookup_typeinfo_from_type(types, *this);
	return info.def.func_return_dyn_type;
}

epure type_desc_t::get_function_pure(const types_t& types) const{
	QUARK_ASSERT(check_invariant());
	QUARK_ASSERT(is_function());
	QUARK_ASSERT(types.check_invariant());

	const auto& info = lookup_typeinfo_from_type(types, *this);
	return info.def.func_pure;
}




/////////////////////////////////////////////////		FREE FUNCTIONS




std::vector<type_t> get_member_types(const std::vector<member_t>& m){
	std::vector<type_t> r;
	for(const auto& a: m){
		r.push_back(a._type);
	}
	return r;
}


int find_struct_member_index(const struct_type_desc_t& desc, const std::string& name){
	int index = 0;
	while(index < desc._members.size() && desc._members[index]._name != name){
		index++;
	}
	if(index == desc._members.size()){
		return -1;
	}
	else{
		return index;
	}
}





type_variant_t get_type_variant(const types_t& types, const type_t& type){
	QUARK_ASSERT(types.check_invariant());
	QUARK_ASSERT(type.check_invariant());

	if(type.is_named_type()){
		const auto& info = lookup_typeinfo_from_type(types, type);
		return named_type_t { info.def.child_types[0] };
	}
	else{
		const auto& desc = peek2(types, type);

		if(type.is_undefined()){
			return undefined_t {};
		}
		else if(desc.is_any()){
			return any_t {};
		}
		else if(desc.is_void()){
			return void_t {};
		}


		else if(desc.is_bool()){
			return bool_t {};
		}
		else if(desc.is_int()){
			return int_t {};
		}
		else if(desc.is_double()){
			return double_t {};
		}
		else if(desc.is_string()){
			return string_t {};
		}
		else if(desc.is_json()){
			return json_type_t {};
		}
		else if(desc.is_typeid()){
			return typeid_type_t {};
		}

		else if(desc.is_struct()){
			const auto& info = lookup_typeinfo_from_type(types, desc.non_name_type);
			return struct_t { make_struct_desc(info) };
		}
		else if(desc.is_vector()){
			const auto& info = lookup_typeinfo_from_type(types, type);
			return vector_t { info.def.child_types };
		}
		else if(desc.is_dict()){
			const auto& info = lookup_typeinfo_from_type(types, type);
			return dict_t { info.def.child_types };
		}
		else if(desc.is_function()){
			const auto& info = lookup_typeinfo_from_type(types, desc.non_name_type);
			return function_t { info.def.child_types };
		}
		else if(type.is_symbol_ref()){
			const auto& info = lookup_typeinfo_from_type(types, type);
			return symbol_ref_t { info.def.symbol_identifier };
		}
		else if(type.is_named_type()){
			QUARK_ASSERT(false);
			throw std::exception();
		}
		else{
			QUARK_ASSERT(false);
			throw std::exception();
		}
	}
}


/////////////////////////////////////////////////		types_t


static physical_type_t calc_physical_type(const types_t& types, const type_t& type){
	const auto index = type.get_lookup_index();
	const auto& node = types.nodes[index];
	return physical_type_t { type_t::make_void() };
}





static type_t intern_node__mutate(types_t& types, const type_node_t& node){
	QUARK_ASSERT(types.check_invariant());

//??? We find by-value = not OK to modify node right after.

	const auto it = std::find_if(
		types.nodes.begin(),
		types.nodes.end(),
		[&](const auto& e){ return e == node; }
	);

	if(it != types.nodes.end()){
		return lookup_type_from_index_it(types, it - types.nodes.begin());
	}

	//	New type, store it.
	else{

		//	All child type are guaranteed to have types already since those are specified using types_t:s.
		types.nodes.push_back(node);
		return lookup_type_from_index_it(types, types.nodes.size() - 1);
	}
}

static type_t make_named_type_internal__mutate(types_t& types, const type_name_t& n, const type_t& destination_type){
	QUARK_ASSERT(types.check_invariant());
	QUARK_ASSERT(n.check_invariant());
	QUARK_ASSERT(destination_type.check_invariant());

	if(false) trace_types(types);

	const auto it = std::find_if(
		types.nodes.begin(),
		types.nodes.end(),
		[&](const auto& e){ return e.optional_name == n; }
	);
	if(it != types.nodes.end()){
		throw std::exception();
	}

	const auto dest_type_node = lookup_typeinfo_from_type(types, destination_type);

	const auto node = type_node_t{
		n,
		type_def_t {
			base_type::k_named_type,
			{ destination_type },
			{},
			epure::pure,
			return_dyn_type::none,
			""
		}
	};

	QUARK_ASSERT(node.def.child_types.size() == 1);

	//	Can't use intern_node__mutate() since we have a tag.
	types.nodes.push_back(node);
	return lookup_type_from_index_it(types, types.nodes.size() - 1);
}

static type_t update_named_type_internal__mutate(types_t& types, const type_t& named, const type_t& destination_type){
	QUARK_ASSERT(types.check_invariant());
	QUARK_ASSERT(named.check_invariant());
	QUARK_ASSERT(destination_type.check_invariant());

	auto& node = lookup_typeinfo_from_type(types, named);
	QUARK_ASSERT(node.def.bt == base_type::k_named_type);
	QUARK_ASSERT(node.def.child_types.size() == 1);

	node.def.child_types = { destination_type };

//??? needs to update the physical type

	//	Returns a new type for the named tag, so it contains the updated byte_type info.
	return lookup_type_from_index(types, named.get_lookup_index());
}

static type_node_t make_entry(const base_type& bt){
	auto result = type_node_t{
		make_empty_type_name(),
		bt,
		std::vector<type_t>{},
		{},
		epure::pure,
		return_dyn_type::none,
		""
	};
	return result;
}

types_t::types_t(){
	//	Order is designed to match up the nodes[] with base_type indexes.
	nodes.push_back(make_entry(base_type::k_undefined));
	nodes.push_back(make_entry(base_type::k_any));
	nodes.push_back(make_entry(base_type::k_void));


	nodes.push_back(make_entry(base_type::k_bool));
	nodes.push_back(make_entry(base_type::k_int));
	nodes.push_back(make_entry(base_type::k_double));
	nodes.push_back(make_entry(base_type::k_string));
	nodes.push_back(make_entry(base_type::k_json));

	nodes.push_back(make_entry(base_type::k_typeid));

	//	These are complex types and are undefined. We need them to take up space in the nodes-vector.
	nodes.push_back(make_entry(base_type::k_undefined));
	nodes.push_back(make_entry(base_type::k_undefined));
	nodes.push_back(make_entry(base_type::k_undefined));
	nodes.push_back(make_entry(base_type::k_undefined));

	nodes.push_back(make_entry(base_type::k_symbol_ref));
	nodes.push_back(make_entry(base_type::k_undefined));

	for(int i = 0 ; i < nodes.size() ; i++){
		const auto& type = lookup_type_from_index(*this, i);
		const auto p = calc_physical_type(*this, type);
		physical_types.push_back(p);
	}

	QUARK_ASSERT(check_invariant());
}

bool types_t::check_invariant() const {
	QUARK_ASSERT(nodes.size() < INT_MAX);

	QUARK_ASSERT(nodes[(type_lookup_index_t)base_type::k_undefined] == make_entry(base_type::k_undefined));
	QUARK_ASSERT(nodes[(type_lookup_index_t)base_type::k_any] == make_entry(base_type::k_any));
	QUARK_ASSERT(nodes[(type_lookup_index_t)base_type::k_void] == make_entry(base_type::k_void));

	QUARK_ASSERT(nodes[(type_lookup_index_t)base_type::k_bool] == make_entry(base_type::k_bool));
	QUARK_ASSERT(nodes[(type_lookup_index_t)base_type::k_int] == make_entry(base_type::k_int));
	QUARK_ASSERT(nodes[(type_lookup_index_t)base_type::k_double] == make_entry(base_type::k_double));
	QUARK_ASSERT(nodes[(type_lookup_index_t)base_type::k_string] == make_entry(base_type::k_string));
	QUARK_ASSERT(nodes[(type_lookup_index_t)base_type::k_json] == make_entry(base_type::k_json));

	QUARK_ASSERT(nodes[(type_lookup_index_t)base_type::k_typeid] == make_entry(base_type::k_typeid));

	QUARK_ASSERT(nodes[(type_lookup_index_t)base_type::k_struct] == make_entry(base_type::k_undefined));
	QUARK_ASSERT(nodes[(type_lookup_index_t)base_type::k_vector] == make_entry(base_type::k_undefined));
	QUARK_ASSERT(nodes[(type_lookup_index_t)base_type::k_dict] == make_entry(base_type::k_undefined));
	QUARK_ASSERT(nodes[(type_lookup_index_t)base_type::k_function] == make_entry(base_type::k_undefined));

	QUARK_ASSERT(nodes[(type_lookup_index_t)base_type::k_symbol_ref] == make_entry(base_type::k_symbol_ref));
	QUARK_ASSERT(nodes[(type_lookup_index_t)base_type::k_named_type] == make_entry(base_type::k_undefined));

//	QUARK_ASSERT(nodes.size() == physical_types.size());

	for(int i = 0 ; i < nodes.size() ; i++){
	}

	//??? Add other common combinations, like vectors with each atomic type, dict with each atomic
	//	type. This allows us to hardcoded their type indexes.
	return true;
}



/////////////////////////////////////////////////		FREE FUNCTIONS



static type_t lookup_node(const types_t& types, const type_node_t& node){
	QUARK_ASSERT(types.check_invariant());

	const auto it = std::find_if(
		types.nodes.begin(),
		types.nodes.end(),
		[&](const auto& e){ return e == node; }
	);

	if(it != types.nodes.end()){
		return lookup_type_from_index_it(types, it - types.nodes.begin());
	}
	else{
		throw std::exception();
//		return type_t::make_undefined();
	}
}

type_t make_named_type(types_t& types, const type_name_t& n, const type_t& destination_type){
	QUARK_ASSERT(types.check_invariant());
	QUARK_ASSERT(n.check_invariant());
	QUARK_ASSERT(destination_type.check_invariant());

	return make_named_type_internal__mutate(types, n, destination_type);
}

type_t update_named_type(types_t& types, const type_t& named, const type_t& destination_type){
	QUARK_ASSERT(types.check_invariant());
	QUARK_ASSERT(named.check_invariant());
	QUARK_ASSERT(destination_type.check_invariant());

	return update_named_type_internal__mutate(types, named, destination_type);
}

type_t lookup_type_from_name(const types_t& types, const type_name_t& tag){
	QUARK_ASSERT(types.check_invariant());

	if(tag.lexical_path.empty()){
		throw std::exception();
	}
	else{
		const auto it = std::find_if(
			types.nodes.begin(),
			types.nodes.end(),
			[&](const auto& e){ return e.optional_name == tag; }
		);
		if(it == types.nodes.end()){
			throw std::exception();
		}

		return lookup_type_from_index_it(types, it - types.nodes.begin());
	}
}

static type_t peek0(const types_t& types, const type_t& type){
	QUARK_ASSERT(types.check_invariant());
	QUARK_ASSERT(type.check_invariant());

	if(false) trace_types(types);

	const auto& info = lookup_typeinfo_from_type(types, type);
	if(info.def.bt == base_type::k_named_type){
		QUARK_ASSERT(info.def.child_types.size() == 1);
		const auto dest = info.def.child_types[0];

		//	Support many linked tags using recursion.
		return peek0(types, dest);
	}
	else{
		return type;
	}
}


type_desc_t peek2(const types_t& types, const type_t& type){
	const auto type2 = peek0(types, type);
	return type_desc_t::wrap_non_named(type2);
}




type_t refresh_type(const types_t& types, const type_t& type){
	const auto lookup_index = type.get_lookup_index();
	QUARK_ASSERT(lookup_index >= 0);
	QUARK_ASSERT(lookup_index < types.nodes.size());

	return lookup_type_from_index(types, type.get_lookup_index());
}


//??? remove bt1 and bt2 from index!? Makes things complicated.

type_t lookup_type_from_index(const types_t& types, type_lookup_index_t type_index){
	QUARK_ASSERT(types.check_invariant());
	QUARK_ASSERT(type_index >= 0 && type_index < types.nodes.size());

	const auto& node = types.nodes[type_index];

	if(node.def.bt == base_type::k_struct){
		return type_t(type_t::assemble(type_index, base_type::k_struct, base_type::k_undefined));
	}
	else if(node.def.bt == base_type::k_vector){
		const auto element_bt = node.def.child_types[0].get_base_type();
		return type_t(type_t::assemble(type_index, base_type::k_vector, element_bt));
	}
	else if(node.def.bt == base_type::k_dict){
		const auto value_bt = node.def.child_types[0].get_base_type();
		return type_t(type_t::assemble(type_index, base_type::k_dict, value_bt));
	}
	else if(node.def.bt == base_type::k_function){
		//??? We could keep the return type inside the type
		return type_t(type_t::assemble(type_index, base_type::k_function, base_type::k_undefined));
	}
	else if(node.def.bt == base_type::k_symbol_ref){
		return type_t::assemble2(type_index, base_type::k_symbol_ref, base_type::k_undefined);
	}
	else if(node.def.bt == base_type::k_named_type){
		return type_t::assemble2(type_index, base_type::k_named_type, base_type::k_undefined);
	}
	else{
		const auto bt = node.def.bt;
		return type_t::assemble2((type_lookup_index_t)type_index, bt, base_type::k_undefined);
	}
}

static type_t lookup_type_from_index_it(const types_t& types, size_t type_index){
	QUARK_ASSERT(types.check_invariant());
	QUARK_ASSERT(type_index >= 0 && type_index < types.nodes.size());

	return lookup_type_from_index(types, static_cast<type_lookup_index_t>(type_index));
}



void trace_types(const types_t& types){
	QUARK_ASSERT(types.check_invariant());

	{
		QUARK_SCOPED_TRACE("TYPES");
		std::vector<std::vector<std::string>> matrix;
		for(auto i = 0 ; i < types.nodes.size() ; i++){
			const auto& type = lookup_type_from_index(types, i);

			const auto& e = types.nodes[i];

			const type_node_t& node = lookup_typeinfo_from_type(types, type);

			const auto physical = type_to_compact_string(
				types,
				type_t::make_double(), //types.physical_types[i].physical,
				enamed_type_mode::full_names
			);

			if(e.def.bt == base_type::k_named_type){
				const auto contents = std::to_string(e.def.child_types[0].get_lookup_index());
				const auto line = std::vector<std::string>{
					std::to_string(i),
					pack_type_name(e.optional_name),
					base_type_to_opcode(e.def.bt),
					contents,
					physical,
				};
				matrix.push_back(line);
			}
			else{
				const auto contents = type_to_compact_string(types, type, enamed_type_mode::full_names);
				const auto line = std::vector<std::string>{
					std::to_string(i),
					"",
					base_type_to_opcode(e.def.bt),
					contents,
					physical
				};
				matrix.push_back(line);
			}
		}

		const auto result = generate_table_type1({ "type_t", "name-tag", "base_type", "contents", "physical" }, matrix);
		QUARK_TRACE(result);
	}
}










//////////////////////////////////////////////////		member_t



//////////////////////////////////////////////////		struct_type_desc_t




std::string type_to_debug_string(const type_t& type){
	std::stringstream s;
	s << "{ " << type.get_lookup_index() << ": " << base_type_to_opcode(type.get_base_type()) << " }";
	return s.str();
}

std::string type_to_compact_string(const types_t& types, const type_t& type, enamed_type_mode named_type_mode){
	QUARK_ASSERT(types.check_invariant());
	QUARK_ASSERT(type.check_invariant());

	struct visitor_t {
		const types_t& types;
		const type_t& type;
		const enamed_type_mode named_type_mode;


		std::string operator()(const undefined_t& e) const{
			return "undef";
		}
		std::string operator()(const any_t& e) const{
			return "any";
		}

		std::string operator()(const void_t& e) const{
			return "void";
		}
		std::string operator()(const bool_t& e) const{
			return "bool";
		}
		std::string operator()(const int_t& e) const{
			return "int";
		}
		std::string operator()(const double_t& e) const{
			return "double";
		}
		std::string operator()(const string_t& e) const{
			return "string";
		}

		std::string operator()(const json_type_t& e) const{
			return "json";
		}
		std::string operator()(const typeid_type_t& e) const{
			return "typeid";
		}

		std::string operator()(const struct_t& e) const{
			std::string members_acc;
			for(const auto& m: e.desc._members){
				members_acc = members_acc + type_to_compact_string(types, m._type, named_type_mode) + " " + m._name + ";";
			}
			return "struct {" + members_acc + "}";
		}
		std::string operator()(const vector_t& e) const{
			QUARK_ASSERT(e._parts.size() == 1);

			return "[" + type_to_compact_string(types, e._parts[0], named_type_mode) + "]";
		}
		std::string operator()(const dict_t& e) const{
			QUARK_ASSERT(e._parts.size() == 1);

			return "[string:" + type_to_compact_string(types, e._parts[0], named_type_mode) + "]";
		}
		std::string operator()(const function_t& e) const{
			const auto desc = peek2(types, type);
			const auto ret = desc.get_function_return(types);
			const auto args = desc.get_function_args(types);
			const auto pure = desc.get_function_pure(types);

			std::vector<std::string> args_str;
			for(const auto& a: args){
				args_str.push_back(type_to_compact_string(types, a, named_type_mode));
			}

			return std::string() + "func " + type_to_compact_string(types, ret, named_type_mode) + "(" + concat_strings_with_divider(args_str, ",") + ") " + (pure == epure::pure ? "pure" : "impure");
		}
		std::string operator()(const symbol_ref_t& e) const {
			return std::string("%") + e.s;
		}
		std::string operator()(const named_type_t& e) const {
			if(named_type_mode == enamed_type_mode::full_names){
				const auto& info = lookup_typeinfo_from_type(types, type);
				return pack_type_name(info.optional_name);
			}
			else if(named_type_mode == enamed_type_mode::short_names){
				const auto& info = lookup_typeinfo_from_type(types, type);
				return info.optional_name.lexical_path.back();
			}
			else{
				QUARK_ASSERT(false);
				throw std::exception();
			}
		}
	};

	const auto result = std::visit(visitor_t{ types, type, named_type_mode }, get_type_variant(types, type));
	return result;
}


type_t make_struct(types_t& types, const struct_type_desc_t& desc){
	const auto member_names = mapf<std::string>(desc._members, [types](const auto& m){
		return m._name;
	});
	const auto logical_member_types = mapf<type_t>(desc._members, [types](const auto& m){
		return m._type;
	});

	const auto member_physical_types = mapf<type_t>(desc._members, [types](const auto& m){
		const auto index = m._type.get_lookup_index();
		return types.physical_types[index].physical;
	});

/*
	const auto physical_node = type_node_t{
		make_empty_type_name(),
		base_type::k_struct,
		member_physical_types,
		member_names,
		epure::pure,
		return_dyn_type::none,
		"",
		type_t::make_undefined()
	};
	const auto physical_type = intern_node__mutate(types, physical_node);
*/

	const auto node = type_node_t{
		make_empty_type_name(),
		type_def_t {
			base_type::k_struct,
			logical_member_types,
			member_names,
			epure::pure,
			return_dyn_type::none,
			""
		}
	};
	return intern_node__mutate(types, node);
}

type_t make_struct(const types_t& types, const struct_type_desc_t& desc){
	const auto member_names = mapf<std::string>(desc._members, [types](const auto& m){
		return m._name;
	});
	const auto logical_member_types = mapf<type_t>(desc._members, [types](const auto& m){
		return m._type;
	});

	const auto node = type_node_t{
		make_empty_type_name(),
		type_def_t {
			base_type::k_struct,
			logical_member_types,
			member_names,
			epure::pure,
			return_dyn_type::none,
			""
		}
	};
	return lookup_node(types, node);
}


type_t make_vector(types_t& types, const type_t& element_type){
	const auto node = type_node_t{
		make_empty_type_name(),
		type_def_t {
			base_type::k_vector,
			{ element_type },
			{},
			epure::pure,
			return_dyn_type::none,
			""
		}
	};
	const auto result = intern_node__mutate(types, node);

	//??? Warning: this should be implemented on each aggregated type (vector, dict, function,
	//	struct etc.). LLVM codegen flattens all named types. Alt A: make sure all flattended types always exists, B: Make llvm use named types, C: Let llvm add flattened types itself.
	//	Also make a type for a variant of the vector where named type is flattened.
	const auto element2 = peek0(types, element_type);
	if(element2 != element_type){
		make_vector(types, element2);
	}
	return result;
}
type_t make_vector(const types_t& types, const type_t& element_type){
	const auto node = type_node_t{
		make_empty_type_name(),
		type_def_t {
			base_type::k_vector,
			{ element_type },
			{},
			epure::pure,
			return_dyn_type::none,
			""
		}
	};
	return lookup_node(types, node);
}

type_t make_dict(types_t& types, const type_t& value_type){
	const auto node = type_node_t{
		make_empty_type_name(),
		type_def_t {
			base_type::k_dict,
			{ value_type },
			{},
			epure::pure,
			return_dyn_type::none,
			""
		}
	};
	return intern_node__mutate(types, node);
}

type_t make_dict(const types_t& types, const type_t& value_type){
	const auto node = type_node_t{
		make_empty_type_name(),
		type_def_t {
			base_type::k_dict,
			{ value_type },
			{},
			epure::pure,
			return_dyn_type::none,
			""
		}
	};
	return lookup_node(types, node);
}

type_t make_function3(types_t& types, const type_t& ret, const std::vector<type_t>& args, epure pure, return_dyn_type dyn_return){
	const auto node = type_node_t{
		make_empty_type_name(),
		type_def_t {
			base_type::k_function,
			concat(
				std::vector<type_t>{ ret },
				args
			),
			{},
			pure,
			dyn_return,
			""
		}
	};
	return intern_node__mutate(types, node);
}


type_t make_function3(const types_t& types, const type_t& ret, const std::vector<type_t>& args, epure pure, return_dyn_type dyn_return){
	const auto node = type_node_t{
		make_empty_type_name(),
		type_def_t {
			base_type::k_function,
			concat(
				std::vector<type_t>{ ret },
				args
			),
			{},
			pure,
			dyn_return,
			""
		}
	};
	return lookup_node(types, node);
}

type_t make_function_dyn_return(types_t& types, const std::vector<type_t>& args, epure pure, return_dyn_type dyn_return){
	return make_function3(types, type_t::make_any(), args, pure, dyn_return);
}

type_t make_function(types_t& types, const type_t& ret, const std::vector<type_t>& args, epure pure){
	QUARK_ASSERT(peek2(types, ret).is_any() == false);

	return make_function3(types, ret, args, pure, return_dyn_type::none);
}

type_t make_function(const types_t& types, const type_t& ret, const std::vector<type_t>& args, epure pure){
	QUARK_ASSERT(peek2(types, ret).is_any() == false);

	return make_function3(types, ret, args, pure, return_dyn_type::none);
}


type_t make_symbol_ref(types_t& types, const std::string& s){
	const auto node = type_node_t{
		make_empty_type_name(),
		type_def_t {
			base_type::k_symbol_ref,
			std::vector<type_t>{},
			{},
			epure::pure,
			return_dyn_type::none,
			s
		}
	};
	return intern_node__mutate(types, node);
}

static const type_node_t& lookup_typeinfo_from_type(const types_t& types, const type_t& type){
	QUARK_ASSERT(types.check_invariant());
	QUARK_ASSERT(type.check_invariant());

	const auto lookup_index = type.get_lookup_index();
	QUARK_ASSERT(lookup_index >= 0);
	QUARK_ASSERT(lookup_index < types.nodes.size());

	const auto& result = types.nodes[lookup_index];
	return result;
}

static type_node_t& lookup_typeinfo_from_type(types_t& types, const type_t& type){
	QUARK_ASSERT(types.check_invariant());
	QUARK_ASSERT(type.check_invariant());

	const auto lookup_index = type.get_lookup_index();
	QUARK_ASSERT(lookup_index >= 0);
	QUARK_ASSERT(lookup_index < types.nodes.size());

	auto& result = types.nodes[lookup_index];
	return result;
}


bool is_atomic_type(type_t type){
	const auto bt = type.get_base_type();
	return is_atomic_type(bt);
}







static bool is_fully_defined_internal(const types_t& types, const std::set<type_lookup_index_t>& done_types, const type_t& t);


bool is_fully_defined_internal(const types_t& types, const std::set<type_lookup_index_t>& done_types, const struct_type_desc_t& s){
	QUARK_ASSERT(s.check_invariant());

	for(const auto& e: s._members){
		bool result = is_fully_defined_internal(types, done_types, e._type);
		if(result == false){
			return false;
		}
	}
	return true;
}


bool is_fully_defined__type_vector(const types_t& types, const std::set<type_lookup_index_t>& done_types, const std::vector<type_t>& elements){
	QUARK_ASSERT(types.check_invariant());

	for(const auto& e: elements){
		if(is_fully_defined_internal(types, done_types, e) == false){
			return false;
		}
	}
	return true;
}

static bool is_fully_defined_internal(const types_t& types, const std::set<type_lookup_index_t>& done_types, const type_t& t){
	QUARK_ASSERT(types.check_invariant());
	QUARK_ASSERT(t.check_invariant());

	if(done_types.find(t.get_lookup_index()) != done_types.end()){
		return true;
	}
	else{
		auto done2 = done_types;
		done2.insert(t.get_lookup_index());

		struct visitor_t {
			const types_t& types;
			const std::set<type_lookup_index_t>& done_types;


			bool operator()(const undefined_t& e) const{
				return false;
			}
			bool operator()(const any_t& e) const{
				return true;
			}

			bool operator()(const void_t& e) const{
				return true;
			}
			bool operator()(const bool_t& e) const{
				return true;
			}
			bool operator()(const int_t& e) const{
				return true;
			}
			bool operator()(const double_t& e) const{
				return true;
			}
			bool operator()(const string_t& e) const{
				return true;
			}

			bool operator()(const json_type_t& e) const{
				return true;
			}
			bool operator()(const typeid_type_t& e) const{
				return true;
			}

			bool operator()(const struct_t& e) const{
				return is_fully_defined_internal(types, done_types, e.desc);
			}
			bool operator()(const vector_t& e) const{
				return is_fully_defined__type_vector(types, done_types, e._parts);
			}
			bool operator()(const dict_t& e) const{
				return is_fully_defined__type_vector(types, done_types, e._parts);
			}
			bool operator()(const function_t& e) const{
				return is_fully_defined__type_vector(types, done_types, e._parts);
			}
			bool operator()(const symbol_ref_t& e) const {
				return false;
			}
			bool operator()(const named_type_t& e) const {
				return is_fully_defined_internal(types, done_types, e.destination_type);
			}
		};
		return std::visit(visitor_t { types, done2 }, get_type_variant(types, t));
	}
}


bool is_fully_defined(const types_t& types, const type_t& t){
	QUARK_ASSERT(types.check_invariant());
	QUARK_ASSERT(t.check_invariant());

	std::set<type_lookup_index_t> done_types;
	return is_fully_defined_internal(types, done_types, t);
}

QUARK_TEST("Types", "is_fully_defined()", "", ""){
	types_t types;
	QUARK_VERIFY(is_fully_defined(types, type_t::make_int()))
}
QUARK_TEST("Types", "is_fully_defined()", "", ""){
	types_t types;
	const type_t t = make_named_type(types, type_name_t { { "global", "f" } }, type_t::make_int());
	QUARK_VERIFY(is_fully_defined(types, t))
}



physical_type_t get_physical_type(const types_t& types, const type_t& type){
	QUARK_ASSERT(types.check_invariant());
	QUARK_ASSERT(type.check_invariant());

	const auto lookup_index = type.get_lookup_index();
	return types.physical_types[lookup_index];
}



//////////////////////////////////////////////////		get_type_variant()



//////////////////////////////////////////////////		JSON


json_t type_to_json_shallow(const type_t& type){
	const auto s = std::string("type:") + std::to_string(type.get_lookup_index());
	return json_t(s);
}

static json_t struct_definition_to_json(const types_t& types, const struct_type_desc_t& desc){
	QUARK_ASSERT(desc.check_invariant());

	return json_t::make_array({
		members_to_json(types, desc._members)
	});
}

static std::vector<json_t> typeids_to_json_array(const types_t& types, const std::vector<type_t>& m){
	std::vector<json_t> r;
	for(const auto& a: m){
		r.push_back(type_to_json(types, a));
	}
	return r;
}
static std::vector<type_t> typeids_from_json_array(types_t& types, const std::vector<json_t>& m){
	std::vector<type_t> r;
	for(const auto& a: m){
		r.push_back(type_from_json(types, a));
	}
	return r;
}

json_t type_to_json(const types_t& types, const type_t& type){
	QUARK_ASSERT(types.check_invariant());
	QUARK_ASSERT(type.check_invariant());

	const auto bt = type.get_base_type();
	const auto basetype_str = base_type_to_opcode(bt);

	struct visitor_t {
		const types_t& types;
		const type_t& type;
		const std::string basetype_str;


		json_t operator()(const undefined_t& e) const{
			return basetype_str;
		}
		json_t operator()(const any_t& e) const{
			return basetype_str;
		}

		json_t operator()(const void_t& e) const{
			return basetype_str;
		}
		json_t operator()(const bool_t& e) const{
			return basetype_str;
		}
		json_t operator()(const int_t& e) const{
			return basetype_str;
		}
		json_t operator()(const double_t& e) const{
			return basetype_str;
		}
		json_t operator()(const string_t& e) const{
			return basetype_str;
		}

		json_t operator()(const json_type_t& e) const{
			return basetype_str;
		}
		json_t operator()(const typeid_type_t& e) const{
			return basetype_str;
		}

		json_t operator()(const struct_t& e) const{
			return json_t::make_array(
				{
					json_t(basetype_str),
					struct_definition_to_json(types, e.desc)
				}
			);
		}
		json_t operator()(const vector_t& e) const{
			const auto desc = peek2(types, type);
			const auto d = desc.get_vector_element_type(types);
			return json_t::make_array( { json_t(basetype_str), type_to_json(types, d) });
		}
		json_t operator()(const dict_t& e) const{
			const auto desc = peek2(types, type);
			const auto d = desc.get_dict_value_type(types);
			return json_t::make_array( { json_t(basetype_str), type_to_json(types, d) });
		}
		json_t operator()(const function_t& e) const{
			const auto desc = peek2(types, type);
			const auto ret = desc.get_function_return(types);
			const auto args = desc.get_function_args(types);
			const auto pure = desc.get_function_pure(types);
			const auto dyn = desc.get_function_dyn_return_type(types);

			//	Only include dyn-type it it's different than return_dyn_type::none.
			const auto dyn_type = dyn != return_dyn_type::none ? json_t(static_cast<int>(dyn)) : json_t();
			return make_array_skip_nulls({
				basetype_str,
				type_to_json(types, ret),
				typeids_to_json_array(types, args),
				pure == epure::pure ? true : false,
				dyn_type
			});

			std::vector<std::string> args_str;
			for(const auto& a: args){
				args_str.push_back(type_to_compact_string(types, a));
			}

			return std::string() + "func " + type_to_compact_string(types, ret) + "(" + concat_strings_with_divider(args_str, ",") + ") " + (pure == epure::pure ? "pure" : "impure");
		}
		json_t operator()(const symbol_ref_t& e) const {
			const auto identifier = e.s;
			return json_t(std::string("%") + identifier);
		}
		json_t operator()(const named_type_t& e) const {
			const auto& tag = type.get_named_type(types);
			return pack_type_name(tag);
			//??? also store child_types[0]
//			return type_to_json_shallow(e.destination_type);
		}
	};

	const auto result = std::visit(visitor_t{ types, type, basetype_str }, get_type_variant(types, type));
	return result;
}

type_t type_from_json(types_t& types, const json_t& t){
	QUARK_ASSERT(types.check_invariant());
	QUARK_ASSERT(t.check_invariant());

	if(t.is_string()){
		const auto s = t.get_string();

		//	Identifier.
		if(s.front() == '%'){
			return make_symbol_ref(types, s.substr(1));
		}

		//	Tagged type.
		else if(is_type_name(s)){
			//??? also store child_types[0]
			return make_named_type(types, unpack_type_name(s), make_undefined());
		}

		//	Other types.
		else {
			if(s == ""){
				return make_undefined();
			}
			const auto b = opcode_to_base_type(s);

			if(b == base_type::k_undefined){
				return make_undefined();
			}
			else if(b == base_type::k_any){
				return type_t::make_any();
			}
			else if(b == base_type::k_void){
				return type_t::make_void();
			}
			else if(b == base_type::k_bool){
				return type_t::make_bool();
			}
			else if(b == base_type::k_int){
				return type_t::make_int();
			}
			else if(b == base_type::k_double){
				return type_t::make_double();
			}
			else if(b == base_type::k_string){
				return type_t::make_string();
			}
			else if(b == base_type::k_typeid){
				return type_desc_t::make_typeid();
			}
			else if(b == base_type::k_json){
				return type_t::make_json();
			}
			else{
				quark::throw_exception();
			}
		}
	}
	else if(t.is_array()){
		const auto a = t.get_array();
		const auto s = a[0].get_string();
		QUARK_ASSERT(s.front() != '#');
		QUARK_ASSERT(s.front() != '^');
		QUARK_ASSERT(s.front() != '%');
		QUARK_ASSERT(s.front() != '/');

		if(s == "struct"){
			const auto struct_def_array = a[1].get_array();
			const auto member_array = struct_def_array[0].get_array();

			const std::vector<member_t> struct_members = members_from_json(types, member_array);
			return make_struct(types, struct_members);
		}
		else if(s == "vector"){
			const auto element_type = type_from_json(types, a[1]);
			return make_vector(types, element_type);
		}
		else if(s == "dict"){
			const auto value_type = type_from_json(types, a[1]);
			return make_dict(types, value_type);
		}
		else if(s == "func"){
			const auto ret_type = type_from_json(types, a[1]);
			const auto arg_types_array = a[2].get_array();
			const std::vector<type_t> arg_types = typeids_from_json_array(types, arg_types_array);

			if(a[3].is_true() == false && a[3].is_false() == false){
				quark::throw_exception();
			}
			const bool pure = a[3].is_true();

			if(a.size() > 4){
				const auto dyn = static_cast<return_dyn_type>(a[4].get_number());
				if(dyn == return_dyn_type::none){
					return make_function(types, ret_type, arg_types, pure ? epure::pure : epure::impure);
				}
				else{
					return make_function_dyn_return(types, arg_types, pure ? epure::pure : epure::impure, dyn);
				}
			}
			else{
				return make_function(types, ret_type, arg_types, pure ? epure::pure : epure::impure);
			}
		}
		else if(s == "unknown-identifier"){
			QUARK_ASSERT(false);
			quark::throw_exception();
		}
		else {
			QUARK_ASSERT(false);
			quark::throw_exception();
		}
	}
	else{
		quark::throw_runtime_error("Invalid typeid-json.");
	}
	return make_undefined();
}

type_t type_from_json_shallow(const json_t& j){
	QUARK_ASSERT(j.check_invariant());

	if(j.is_string() && j.get_string().substr(0, 6) == "type:"){
		const auto n_str = j.get_string().substr(6);
		const auto i = std::stoi(n_str);
		const auto type = type_t(i);
		return type;
	}
	else{
		throw std::exception();
	}
}

json_t members_to_json(const types_t& types, const std::vector<member_t>& members){
	std::vector<json_t> r;
	for(const auto& i: members){
		const auto member = make_object({
			{ "type", type_to_json(types, i._type) },
			{ "name", json_t(i._name) }
		});
		r.push_back(json_t(member));
	}
	return r;
}

std::vector<member_t> members_from_json(types_t& types, const json_t& members){
	QUARK_ASSERT(members.check_invariant());

	std::vector<member_t> r;
	for(const auto& i: members.get_array()){
		const auto m = member_t {
			type_from_json(types, i.get_object_element("type")),
			i.get_object_element("name").get_string()
		};

		r.push_back(m);
	}
	return r;
}

//??? Doesnt work. Needs to use type_t-style code that generates contents of type
json_t types_to_json(const types_t& types){
	std::vector<json_t> result;
	for(auto i = 0 ; i < types.nodes.size() ; i++){
		const auto& type = lookup_type_from_index(types, i);

		const auto& e = types.nodes[i];
		const auto tag = pack_type_name(e.optional_name);
		const auto desc = type_to_json(types, type);
		const auto x = json_t::make_object({
			{ "tag", tag },
			{ "desc", desc }
		});
		result.push_back(x);
	}

	//??? support tags!
	return result;
}

//??? Doesnt work. Needs to use type_t-style code that generates contents of type
//??? support tags!
types_t types_from_json(const json_t& j){
	types_t types;
	for(const auto& t: j.get_array()){
		const auto tag = t.get_object_element("tag").get_string();
		const auto desc = t.get_object_element("desc");

		const auto tag2 = unpack_type_name(tag);
		const auto type2 = type_from_json(types, desc);
		(void)type2;
//		types.push_back(type_node_t { tag2, type2 });
	}
	return types;
}




////////////////////////////////		TESTS


QUARK_TEST("Types", "update_named_type()", "", ""){
	types_t types;
	const auto name = unpack_type_name("/a/b");
	const auto a = make_named_type(types, name, make_undefined());
	const auto s = make_struct(types, struct_type_desc_t( { member_t(a, "f") } ));
	const auto b = update_named_type(types, a, s);

	if(false) trace_types(types);
	QUARK_ASSERT(is_fully_defined(types, b));
}




QUARK_TEST("Types", ")", "", ""){
	types_t types;
	const auto a = get_physical_type(types, type_t::make_bool());
//	QUARK_VERIFY(a.physical.is_bool());
}


}	// floyd

