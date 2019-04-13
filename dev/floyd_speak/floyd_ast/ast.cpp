//
//  parser_ast.cpp
//  FloydSpeak
//
//  Created by Marcus Zetterquist on 10/08/16.
//  Copyright © 2016 Marcus Zetterquist. All rights reserved.
//

#include "ast.h"


#include "ast_json.h"
#include "statement.h"



#include "statement.h"
#include "utils.h"
#include "json_support.h"
#include "ast.h"
#include "ast_typeid_helpers.h"
#include "parser_primitives.h"


namespace floyd {

using namespace std;

const std::vector<statement_t > astjson_to_statements(const json_t& p);
json_t statement_to_json(const statement_t& e);




typeid_t resolve_type_name(const json_t& t){
	const auto t2 = typeid_from_ast_json(t);
	return t2;
}

std::shared_ptr<typeid_t> get_optional_typeid(const json_t& json_array, int optional_index){
	if(optional_index < json_array.get_array_size()){
		const auto e = json_array.get_array_n(optional_index);
		return std::make_shared<typeid_t>(typeid_from_ast_json(e));
	}
	else{
		return nullptr;
	}
}


//??? loses output-type for some expressions. Make it nonlossy!
expression_t astjson_to_expression(const json_t& e){
	QUARK_ASSERT(e.check_invariant());

	const auto op = e.get_array_n(0).get_string();
	QUARK_ASSERT(op != "");
	if(op.empty()){
		quark::throw_exception();
	}

	if(op == expression_opcode_t::k_literal){
		QUARK_ASSERT(e.get_array_size() == 3);

		const auto value = e.get_array_n(1);
		const auto type = e.get_array_n(2);
		const auto type2 = resolve_type_name(type);

		if(type2.is_undefined()){
			return expression_t::make_literal_undefined();
		}
		else if(type2.get_base_type() == base_type::k_bool){
			return expression_t::make_literal_bool(value.is_false() ? false : true);
		}
		else if(type2.get_base_type() == base_type::k_int){
			return expression_t::make_literal_int((int)value.get_number());
		}
		else if(type2.get_base_type() == base_type::k_double){
			return expression_t::make_literal_double((double)value.get_number());
		}
		else if(type2.get_base_type() == base_type::k_string){
			return expression_t::make_literal_string(value.get_string());
		}
		else{
			QUARK_ASSERT(false);
			quark::throw_exception();
		}
	}
	else if(op == expression_opcode_t::k_unary_minus){
		QUARK_ASSERT(e.get_array_size() == 2 || e.get_array_size() == 3);

		const auto expr = astjson_to_expression(e.get_array_n(1));
		const auto annotated_type = get_optional_typeid(e, 2);
		return expression_t::make_unary_minus(expr, annotated_type);
	}
	else if(is_simple_expression__2(op)){
		QUARK_ASSERT(e.get_array_size() == 3 || e.get_array_size() == 4);

		const auto op2 = token_to_expression_type(op);
		const auto lhs_expr = astjson_to_expression(e.get_array_n(1));
		const auto rhs_expr = astjson_to_expression(e.get_array_n(2));
		const auto annotated_type = get_optional_typeid(e, 3);
		return expression_t::make_simple_expression__2(op2, lhs_expr, rhs_expr, annotated_type);
	}
	else if(op == expression_opcode_t::k_conditional_operator){
		QUARK_ASSERT(e.get_array_size() == 4 || e.get_array_size() == 5);

		const auto condition_expr = astjson_to_expression(e.get_array_n(1));
		const auto a_expr = astjson_to_expression(e.get_array_n(2));
		const auto b_expr = astjson_to_expression(e.get_array_n(3));
		const auto annotated_type = get_optional_typeid(e, 4);
		return expression_t::make_conditional_operator(condition_expr, a_expr, b_expr, annotated_type);
	}
	else if(op == expression_opcode_t::k_call){
		QUARK_ASSERT(e.get_array_size() == 3 || e.get_array_size() == 4);

		const auto function_expr = astjson_to_expression(e.get_array_n(1));
		const auto args = e.get_array_n(2);
		vector<expression_t> args2;
		for(const auto& arg: args.get_array()){
			args2.push_back(astjson_to_expression(arg));
		}

		const auto annotated_type = get_optional_typeid(e, 3);

		return expression_t::make_call(function_expr, args2, annotated_type);
	}
	else if(op == expression_opcode_t::k_resolve_member){
		QUARK_ASSERT(e.get_array_size() == 3 || e.get_array_size() == 4);

		const auto base_expr = astjson_to_expression(e.get_array_n(1));
		const auto member = e.get_array_n(2).get_string();
		const auto annotated_type = get_optional_typeid(e, 3);
		return expression_t::make_resolve_member(base_expr, member, annotated_type);
	}
	else if(op == expression_opcode_t::k_load){
		QUARK_ASSERT(e.get_array_size() == 2 || e.get_array_size() == 3);

		const auto variable_symbol = e.get_array_n(1).get_string();
		const auto annotated_type = get_optional_typeid(e, 2);
		return expression_t::make_load(variable_symbol, annotated_type);
	}
	else if(op == expression_opcode_t::k_load2){
		QUARK_ASSERT(e.get_array_size() == 3 || e.get_array_size() == 4);

		const auto parent_step = (int)e.get_array_n(1).get_number();
		const auto index = (int)e.get_array_n(2).get_number();
		const auto annotated_type = get_optional_typeid(e, 3);
		return expression_t::make_load2(variable_address_t::make_variable_address(parent_step, index), annotated_type);
	}
	else if(op == expression_opcode_t::k_lookup_element){
		QUARK_ASSERT(e.get_array_size() == 3 || e.get_array_size() == 4);

		const auto parent_address_expr = astjson_to_expression(e.get_array_n(1));
		const auto lookup_key_expr = astjson_to_expression(e.get_array_n(2));
		const auto annotated_type = get_optional_typeid(e, 3);
		return expression_t::make_lookup(parent_address_expr, lookup_key_expr, annotated_type);
	}
	else if(op == expression_opcode_t::k_value_constructor){
		QUARK_ASSERT(e.get_array_size() == 3);

		const auto value_type = resolve_type_name(e.get_array_n(1));
		const auto args = e.get_array_n(2).get_array();

		std::vector<expression_t> args2;
		for(const auto& m: args){
			args2.push_back(astjson_to_expression(m));
		}

		return expression_t::make_construct_value_expr(value_type, args2);
	}
	else{
		quark::throw_exception();
	}
}

//	INPUT: [2, "bind", "^double", "cmath_pi", ["k", 3.14159, "^double"]]
std::pair<json_t, location_t> unpack_loc(const json_t& s){
	QUARK_ASSERT(s.is_array());

	const bool has_location = s.get_array_n(0).is_number();
	if(has_location){
		const location_t source_offset = has_location ? location_t(static_cast<std::size_t>(s.get_array_n(0).get_number())) : k_no_location;

		const auto elements = s.get_array();
		const std::vector<json_t> elements2 = { elements.begin() + 1, elements.end() };
		const auto statement = json_t::make_array(elements2);

		return { statement, source_offset };
	}
	else{
		return { s, k_no_location };
	}
}

statement_t astjson_to_statement__nonlossy(const json_t& statement0){
	QUARK_ASSERT(statement0.check_invariant());
	QUARK_ASSERT(statement0.is_array());

	const auto statement1 = unpack_loc(statement0);
	const auto loc = statement1.second;
	const auto statement = statement1.first;
	const std::string type = statement.get_array_n(0).get_string();

	//	[ "return", [ "k", 3, "int" ] ]
	if(type == statement_opcode_t::k_return){
		QUARK_ASSERT(statement.get_array_size() == 2);
		const auto expr = astjson_to_expression(statement.get_array_n(1));
		return statement_t::make__return_statement(loc, expr);
	}

	//	[ "bind", "double", "x", EXPRESSION, {} ]
	//	Last element is a list of meta info, like "mutable" etc.
	else if(type == statement_opcode_t::k_bind){
		QUARK_ASSERT(statement.get_array_size() == 4 || statement.get_array_size() == 5);
		const auto bind_type = statement.get_array_n(1);
		const auto name = statement.get_array_n(2);
		const auto expr = statement.get_array_n(3);
		const auto meta = statement.get_array_size() >= 5 ? statement.get_array_n(4) : json_t();

		const auto bind_type2 = resolve_type_name(bind_type);
		const auto name2 = name.get_string();
		const auto expr2 = astjson_to_expression(expr);
		bool mutable_flag = !meta.is_null() && meta.does_object_element_exist("mutable");
		const auto mutable_mode = mutable_flag ? statement_t::bind_local_t::k_mutable : statement_t::bind_local_t::k_immutable;
		return statement_t::make__bind_local(loc, name2, bind_type2, expr2, mutable_mode);
	}

	//	[ "store", "x", EXPRESSION ]
	else if(type == statement_opcode_t::k_store){
		QUARK_ASSERT(statement.get_array_size() == 3);
		const auto name = statement.get_array_n(1);
		const auto expr = statement.get_array_n(2);

		const auto name2 = name.get_string();
		const auto expr2 = astjson_to_expression(expr);
		return statement_t::make__store(loc, name2, expr2);
	}

	//	[ "store2", parent_index, variable_index, EXPRESSION ]
	else if(type == statement_opcode_t::k_store2){
		QUARK_ASSERT(statement.get_array_size() == 4);
		const auto parent_index = (int)statement.get_array_n(1).get_number();
		const auto variable_index = (int)statement.get_array_n(2).get_number();
		const auto expr = statement.get_array_n(3);

		const auto expr2 = astjson_to_expression(expr);
		return statement_t::make__store2(loc, variable_address_t::make_variable_address(parent_index, variable_index), expr2);
	}

	//	[ "block", [ STATEMENTS ] ]
	else if(type == statement_opcode_t::k_block){
		QUARK_ASSERT(statement.get_array_size() == 2);

		const auto statements = statement.get_array_n(1);
		const auto r = astjson_to_statements(statements);

		const auto body = body_t(r);
		return statement_t::make__block_statement(loc, body);
	}

	else if(type == statement_opcode_t::k_def_struct){
		QUARK_ASSERT(statement.get_array_size() == 2);
		const auto struct_def = statement.get_array_n(1);
		const auto name = struct_def.get_object_element("name").get_string();
		const auto members = struct_def.get_object_element("members").get_array();

		const auto members2 = members_from_json(members);
		const auto struct_def2 = struct_definition_t(members2);

		const auto s = statement_t::define_struct_statement_t{ name, std::make_shared<struct_definition_t>(struct_def2) };
		return statement_t::make__define_struct_statement(loc, s);
	}
	else if(type == statement_opcode_t::k_def_protocol){
		QUARK_ASSERT(statement.get_array_size() == 2);
		const auto protocol_def = statement.get_array_n(1);
		const auto name = protocol_def.get_object_element("name").get_string();
		const auto members = protocol_def.get_object_element("members").get_array();

		const auto members2 = members_from_json(members);
		const auto protocol_def2 = protocol_definition_t(members2);

		const auto s = statement_t::define_protocol_statement_t{ name, std::make_shared<protocol_definition_t>(protocol_def2) };
		return statement_t::make__define_protocol_statement(loc, s);
	}

	else if(type == statement_opcode_t::k_def_func){
		QUARK_ASSERT(statement.get_array_size() == 2);
		const auto def = statement.get_array_n(1);
		const auto name = def.get_object_element("name");
		const auto args = def.get_object_element("args");
		const auto fstatements = def.get_object_element("statements");
		const auto return_type = def.get_object_element("return_type");
		const auto impure = def.get_object_element("impure");

		const auto name2 = name.get_string();
		const auto args2 = members_from_json(args);
		const auto fstatements2 = astjson_to_statements(fstatements);
		const auto return_type2 = resolve_type_name(return_type);

		if(impure.is_true() == false && impure.is_false() == false){
			quark::throw_exception();
		}
		const auto pure = impure.is_false();
		const auto function_typeid = typeid_t::make_function(return_type2, get_member_types(args2), pure ? epure::pure : epure::impure);
		const auto body = body_t{fstatements2};
		const auto function_def = function_definition_t{k_no_location, name2, function_typeid, args2, make_shared<body_t>(body), 0};

		const auto s = statement_t::define_function_statement_t{ name2, std::make_shared<function_definition_t>(function_def) };
		return statement_t::make__define_function_statement(loc, s);
	}

	//	[ "if", CONDITION_EXPR, THEN_STATEMENTS, ELSE_STATEMENTS ]
	//	Else is optional.
	else if(type == statement_opcode_t::k_if){
		QUARK_ASSERT(statement.get_array_size() == 3 || statement.get_array_size() == 4);
		const auto condition_expression = statement.get_array_n(1);
		const auto then_statements = statement.get_array_n(2);
		const auto else_statements = statement.get_array_size() == 4 ? statement.get_array_n(3) : json_t::make_array();

		const auto condition_expression2 = astjson_to_expression(condition_expression);
		const auto then_statements2 = astjson_to_statements(then_statements);
		const auto else_statements2 = astjson_to_statements(else_statements);

		return statement_t::make__ifelse_statement(
			loc,
			condition_expression2,
			body_t{ then_statements2 },
			body_t{ else_statements2 }
		);
	}
	else if(type == statement_opcode_t::k_for){
		QUARK_ASSERT(statement.get_array_size() == 6);
		const auto for_mode = statement.get_array_n(1);
		const auto iterator_name = statement.get_array_n(2);
		const auto start_expression = statement.get_array_n(3);
		const auto end_expression = statement.get_array_n(4);
		const auto body_statements = statement.get_array_n(5);

		const auto start_expression2 = astjson_to_expression(start_expression);
		const auto end_expression2 = astjson_to_expression(end_expression);
		const auto body_statements2 = astjson_to_statements(body_statements);

		const auto range_type = for_mode.get_string() == "open-range" ? statement_t::for_statement_t::k_open_range : statement_t::for_statement_t::k_closed_range;
		return statement_t::make__for_statement(
			loc,
			iterator_name.get_string(),
			start_expression2,
			end_expression2,
			body_t{ body_statements2 },
			range_type
		);
	}
	else if(type == statement_opcode_t::k_while){
		QUARK_ASSERT(statement.get_array_size() == 3);
		const auto expression = statement.get_array_n(1);
		const auto body_statements = statement.get_array_n(2);

		const auto expression2 = astjson_to_expression(expression);
		const auto body_statements2 = astjson_to_statements(body_statements);

		return statement_t::make__while_statement(loc, expression2, body_t{body_statements2});
	}

	//	[ "expression-statement", EXPRESSION ]
	else if(type == statement_opcode_t::k_expression_statement){
		QUARK_ASSERT(statement.get_array_size() == 2);
		const auto expr = statement.get_array_n(1);
		const auto expr2 = astjson_to_expression(expr);
		return statement_t::make__expression_statement(loc, expr2);
	}

	else if(type == statement_opcode_t::k_software_system){
		QUARK_ASSERT(statement.get_array_size() == 2);
		const auto json_data = statement.get_array_n(1);

		return statement_t::make__software_system_statement(loc, json_data);
	}
	else if(type == statement_opcode_t::k_container_def){
		QUARK_ASSERT(statement.get_array_size() == 2);
		const auto json_data = statement.get_array_n(1);

		return statement_t::make__container_def_statement(loc, json_data);
	}

	else{
		quark::throw_runtime_error("Illegal statement.");
	}
}

const std::vector<statement_t> astjson_to_statements(const json_t& p){
	QUARK_ASSERT(p.check_invariant());
	QUARK_ASSERT(p.is_array());

	vector<statement_t> statements2;
	for(const auto& statement: p.get_array()){
		const auto s2 = astjson_to_statement__nonlossy(statement);
		statements2.push_back(s2);
	}
	return statements2;
}



json_t symbol_to_json(const symbol_t& symbol){
	const auto symbol_type_str = symbol._symbol_type == symbol_t::immutable_local ? "immutable_local" : "mutable_local";
	const auto value_type = typeid_to_ast_json(symbol._value_type, json_tags::k_tag_resolve_state);

	const auto e2 = json_t::make_object({
		{ "symbol_type", symbol_type_str },
		{ "value_type", value_type },
		{ "init", value_to_ast_json(symbol._init, json_tags::k_tag_resolve_state) }
	});
	return e2;
}

symbol_t json_to_symbol(const json_t& e){
	const auto symbol_type = e.get_object_element("symbol_type").get_string();
	const auto value_type = e.get_object_element("value_type");

	if(symbol_type == "immutable_local" || symbol_type == "mutable_local"){
	}
	else{
		throw std::exception();
	}
	const auto symbol_type1 = symbol_type == "immutable_local" ? symbol_t::immutable_local : symbol_t::mutable_local;
	const auto value_type1 = typeid_from_ast_json(value_type);
	const auto init = e.get_object_element("init");
	const auto init_value1 = init.is_null() ? value_t::make_undefined() : ast_json_to_value(value_type1, init);
	const auto result = symbol_t(symbol_type1, value_type1, init_value1);
	return result;
}


std::vector<json_t> symbols_to_json(const symbol_table_t& symbol_table){
	std::vector<json_t> r;
	int symbol_index = 0;
	for(const auto& e: symbol_table._symbols){
		const auto symbol1 = symbol_to_json(e.second);
		const auto e2 = json_t::make_array({
			symbol_index,
			e.first,
			symbol1
		});
		r.push_back(e2);
		symbol_index++;
	}
	return r;
}

symbol_table_t astjson_to_symbols(const json_t& p){
	std::vector<std::pair<std::string, floyd::symbol_t>> result;
	const auto json_array = p.get_array();
	for(const auto& e: json_array){
		const auto symbol_array_json = e;
		const auto symbol_index = e.get_array_n(0).get_number();
		const auto symbol_name = e.get_array_n(1).get_string();
		const auto symbol = json_to_symbol(e.get_array_n(2));

		while(result.size() < symbol_index){
			result.push_back({"dummy_symbol #" + std::to_string(result.size()), symbol});
		}

		QUARK_ASSERT(result.size() == symbol_index);
		result.push_back({ symbol_name, symbol } );
	}
	return symbol_table_t{ result };
}





json_t body_to_json(const body_t& e){
	std::vector<json_t> statements;
	for(const auto& i: e._statements){
		statements.push_back(statement_to_json(i));
	}

	const auto symbols = symbols_to_json(e._symbol_table);

	return json_t::make_object({
		std::pair<std::string, json_t>{ "statements", json_t::make_array(statements) },
		std::pair<std::string, json_t>{ "symbols", json_t::make_array(symbols) }
	});
}

body_t json_to_body(const json_t& json){
	const auto statements = json.does_object_element_exist("statements") ? json.get_object_element("statements") : json_t();
	const auto statements1 = statements.is_null() ? std::vector<statement_t>() : astjson_to_statements(statements);

	const auto symbols = json.does_object_element_exist("symbols") ? json.get_object_element("symbols") : json_t();
	const auto symbols1 = symbols.is_null() ? symbol_table_t{} : astjson_to_symbols(symbols);

	return body_t(statements1, symbols1);
}




json_t statement_to_json(const statement_t& e){
	QUARK_ASSERT(e.check_invariant());

	struct visitor_t {
		const statement_t& statement;

		json_t operator()(const statement_t::return_statement_t& s) const{
			return make_statement1(k_no_location, statement_opcode_t::k_return, expression_to_json(s._expression));
		}
		json_t operator()(const statement_t::define_struct_statement_t& s) const{
			return make_statement2(k_no_location, statement_opcode_t::k_def_struct, json_t(s._name), struct_definition_to_ast_json(*s._def));
		}
		json_t operator()(const statement_t::define_protocol_statement_t& s) const{
			return make_statement2(k_no_location, statement_opcode_t::k_def_protocol, json_t(s._name), protocol_definition_to_ast_json(*s._def));
		}
		json_t operator()(const statement_t::define_function_statement_t& s) const{
			return make_statement3(
				statement.location,
				statement_opcode_t::k_def_func,
				json_t(s._name),
				function_def_expression_to_ast_json(*s._def),
				s._def->_function_type.get_function_pure() == epure::impure ? true : false
			);
		}

		json_t operator()(const statement_t::bind_local_t& s) const{
			bool mutable_flag = s._locals_mutable_mode == statement_t::bind_local_t::k_mutable;
			const auto meta = mutable_flag
				? json_t::make_object({
					std::pair<std::string, json_t>{"mutable", mutable_flag}
				})
				: json_t();

			return make_statement4(
				statement.location,
				statement_opcode_t::k_bind,
				s._new_local_name,
				typeid_to_ast_json(s._bindtype, json_tags::k_tag_resolve_state),
				expression_to_json(s._expression),
				meta
			);
		}
		json_t operator()(const statement_t::store_t& s) const{
			return make_statement2(
				statement.location,
				statement_opcode_t::k_store,
				s._local_name,
				expression_to_json(s._expression)
			);
		}
		json_t operator()(const statement_t::store2_t& s) const{
			return make_statement3(
				statement.location,
				statement_opcode_t::k_store2,
				s._dest_variable._parent_steps,
				s._dest_variable._index,
				expression_to_json(s._expression)
			);
		}
		json_t operator()(const statement_t::block_statement_t& s) const{
			return make_statement1(k_no_location, statement_opcode_t::k_block, body_to_json(s._body));
		}

		json_t operator()(const statement_t::ifelse_statement_t& s) const{
			return make_statement3(
				statement.location,
				statement_opcode_t::k_if,
				expression_to_json(s._condition),
				body_to_json(s._then_body),
				body_to_json(s._else_body)
			);
		}
		json_t operator()(const statement_t::for_statement_t& s) const{
			return make_statement4(
				statement.location,
				statement_opcode_t::k_for,
				//??? open_range?
				json_t("closed_range"),
				expression_to_json(s._start_expression),
				expression_to_json(s._end_expression),
				body_to_json(s._body)
			);
		}
		json_t operator()(const statement_t::while_statement_t& s) const{
			return make_statement2(
				statement.location,
				statement_opcode_t::k_while,
				expression_to_json(s._condition),
				body_to_json(s._body)
			);
		}


		json_t operator()(const statement_t::expression_statement_t& s) const{
			return make_statement1(
				statement.location,
				statement_opcode_t::k_expression_statement,
				expression_to_json(s._expression)
			);
		}
		json_t operator()(const statement_t::software_system_statement_t& s) const{
			return make_statement1(
				statement.location,
				statement_opcode_t::k_software_system,
				s._json_data
			);
		}
		json_t operator()(const statement_t::container_def_statement_t& s) const{
			return make_statement1(
				statement.location,
				statement_opcode_t::k_container_def,
				s._json_data
			);
		}
	};

	return std::visit(visitor_t{ e }, e._contents);
}






json_t gp_ast_to_json(const general_purpose_ast_t& ast){
	QUARK_ASSERT(ast.check_invariant());

	std::vector<json_t> fds;
	for(const auto& e: ast._function_defs){
		const auto fd = function_def_to_ast_json(*e);
		fds.push_back(fd);
	}

	const auto function_defs_json = json_t::make_array(fds);
	return json_t::make_object(
		{
			{ "globals", body_to_json(ast._globals) },
			{ "function_defs", function_defs_json }
		}
	);

	//??? add software-system and containerdef.
}

general_purpose_ast_t json_to_gp_ast(const json_t& json){

	//	This is a pass2 AST: it contains an array of statements, with hierachical functions and types.
	if(json.is_array()){
		const auto program_body = astjson_to_statements(json);
		return general_purpose_ast_t{
			body_t{ program_body },
			{},
			{},
			{}
		};
	}

	//	Pass 3 AST aka SAST: contains globals + function-defs.
	else{
		const auto globals0 = json.get_object_element("globals");
		const auto function_defs = json.get_object_element("function_defs");

		body_t globals1 = json_to_body(globals0);

		std::vector<std::shared_ptr<const floyd::function_definition_t>> function_defs1;
		for(const auto& f: function_defs.get_array()){
			const auto f1 = json_to_function_def(f);
			const auto f2 = std::make_shared<const floyd::function_definition_t>(f1);
			function_defs1.push_back(f2);
		}

		return general_purpose_ast_t {
			globals1,
			function_defs1,
			software_system_t{},
			container_t{}
		};
	}
}




} //	floyd
